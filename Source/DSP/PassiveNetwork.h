#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <vector>
#include <algorithm>

namespace sa670
{
/**
    Linear passive R/L/C network simulated with Modified Nodal Analysis.

    Reactive elements use trapezoidal (bilinear) companion models, so the
    digital network is the exact bilinear transform of the analog circuit.
    The conductance matrix is only re-inverted when a component value
    changes (pot turned / switch moved); per sample the cost is one N x N
    matrix-vector product.

    Node 0 is ground. The input is an ideal voltage source with series
    resistance Rs driving node `inNode`. Output = voltage at `outNode`.
*/
class PassiveNetwork
{
public:
    static constexpr int maxNodes = 16, maxElems = 40, maxCh = 2;
    enum class Type { R, C, L, RL };   // RL = inductor with series (DC) resistance

    void reset (int numNodes, int inNode, int outNode, double sourceR)
    {
        N = numNodes; in = inNode; out = outNode; Rs = sourceR;
        elems.clear();
        dirty = true;
        clearState();
    }

    int add (Type t, int a, int b, double v, double v2 = 0.0)
    {
        elems.push_back ({ t, a, b, v, v2 });
        elems.back().nv = v; elems.back().nv2 = v2;
        dirty = true;
        return (int) elems.size() - 1;
    }

    void set (int idx, double v, double v2 = 0.0)
    {
        auto& e = elems[(size_t) idx];
        e.nv = v; e.nv2 = v2;
        v *= e.tol; v2 *= e.tol2;
        if (e.v != v || e.v2 != v2) { e.v = v; e.v2 = v2; dirty = true; }
    }

    /** Component tolerances: every part gets a fixed deviation derived from `seed`
        (resistors +/-1 %, capacitors +/-5 %, inductors +/-3 %), like two
        hardware units never being identical. seed < 0 = ideal parts. */
    void setTolerance (int seed)
    {
        unsigned s = (unsigned) (seed * 2654435761u + 12345u);
        auto rnd = [&s] { s = s * 1664525u + 1013904223u; return ((s >> 8) & 0xffff) / 32767.5 - 1.0; };
        for (auto& e : elems)
        {
            const double r1 = rnd(), r2 = rnd();
            if (seed < 0) { e.tol = e.tol2 = 1.0; }
            else switch (e.t)
            {
                case Type::R:  e.tol = 1.0 + 0.01 * r1; e.tol2 = 1.0; break;
                case Type::C:  e.tol = 1.0 + 0.05 * r1; e.tol2 = 1.0; break;
                case Type::L:  e.tol = 1.0 + 0.03 * r1; e.tol2 = 1.0; break;
                default:       e.tol = 1.0 + 0.03 * r1; e.tol2 = 1.0 + 0.03 * r2; break;
            }
            e.v = e.nv * e.tol; e.v2 = e.nv2 * e.tol2;
        }
        dirty = true;
    }

    void setSampleRate (double fs) { T = 1.0 / fs; dirty = true; }
    void setOutputGain (double g) { outGain = g; }

    void clearState()
    {
        for (auto& ch : state)
            for (auto& s : ch) s = {};
    }

    void rebuildIfNeeded()
    {
        if (! dirty) return;
        dirty = false;

        std::array<double, maxNodes * maxNodes> G {};
        auto stamp = [&G, this] (int a, int b, double g)
        {
            if (a > 0) G[(size_t) ((a - 1) * N + (a - 1))] += g;
            if (b > 0) G[(size_t) ((b - 1) * N + (b - 1))] += g;
            if (a > 0 && b > 0)
            {
                G[(size_t) ((a - 1) * N + (b - 1))] -= g;
                G[(size_t) ((b - 1) * N + (a - 1))] -= g;
            }
        };

        stamp (in, 0, 1.0 / Rs);
        for (auto& e : elems)
        {
            e.g = conductance (e);
            stamp (e.a, e.b, e.g);
        }
        invert (G);
    }

    inline float process (int ch, float xIn) noexcept
    {
        auto& st = state[(size_t) ch];
        std::array<double, maxNodes> rhs {};
        rhs[(size_t) (in - 1)] = (double) xIn / Rs;

        // history current sources of reactive elements
        for (size_t k = 0; k < elems.size(); ++k)
        {
            const auto& e = elems[k];
            if (e.t == Type::R) continue;
            const auto& s = st[k];
            double K;
            switch (e.t)
            {
                case Type::C:  K = -(e.g * s.v + s.i); break;
                case Type::L:  K = s.i + e.g * s.v; break;
                default:       K = e.g * (s.v - (e.v - 2.0 * e.v2 / T) * s.i); break; // RL
            }
            st[k].K = K;
            if (e.a > 0) rhs[(size_t) (e.a - 1)] -= K;
            if (e.b > 0) rhs[(size_t) (e.b - 1)] += K;
        }

        std::array<double, maxNodes> v {};
        for (int r = 0; r < N; ++r)
        {
            double acc = 0.0;
            const double* row = &Ginv[(size_t) (r * N)];
            for (int c = 0; c < N; ++c) acc += row[c] * rhs[(size_t) c];
            v[(size_t) r] = acc;
        }

        for (size_t k = 0; k < elems.size(); ++k)
        {
            const auto& e = elems[k];
            if (e.t == Type::R) continue;
            const double vv = (e.a > 0 ? v[(size_t) (e.a - 1)] : 0.0) - (e.b > 0 ? v[(size_t) (e.b - 1)] : 0.0);
            auto& s = st[k];
            s.i = e.g * vv + s.K;
            s.v = vv;
        }

        return (float) (v[(size_t) (out - 1)] * outGain);
    }

    /** Small-signal AC analysis of the analog circuit: Vout/Vsource at f (Hz). */
    std::complex<double> analogResponse (double f) const
    {
        using cd = std::complex<double>;
        const cd jw (0.0, 2.0 * 3.14159265358979323846 * f);
        std::vector<cd> Y ((size_t) (N * (N + 1)), cd (0));
        auto at = [&Y, this] (int r, int c) -> cd& { return Y[(size_t) (r * (N + 1) + c)]; };
        auto stamp = [&] (int a, int b, cd y)
        {
            if (a > 0) at (a - 1, a - 1) += y;
            if (b > 0) at (b - 1, b - 1) += y;
            if (a > 0 && b > 0) { at (a - 1, b - 1) -= y; at (b - 1, a - 1) -= y; }
        };
        stamp (in, 0, 1.0 / Rs);
        for (auto& e : elems)
        {
            cd y;
            switch (e.t)
            {
                case Type::R:  y = 1.0 / e.v; break;
                case Type::C:  y = jw * e.v; break;
                case Type::L:  y = 1.0 / (jw * e.v); break;
                default:       y = 1.0 / (e.v + jw * e.v2); break;
            }
            stamp (e.a, e.b, y);
        }
        at (in - 1, N) = 1.0 / Rs; // rhs: Vs = 1

        // Gaussian elimination with partial pivoting
        for (int c = 0; c < N; ++c)
        {
            int p = c;
            for (int r = c + 1; r < N; ++r)
                if (std::abs (at (r, c)) > std::abs (at (p, c))) p = r;
            if (p != c) for (int k = 0; k <= N; ++k) std::swap (at (c, k), at (p, k));
            for (int r = 0; r < N; ++r)
            {
                if (r == c) continue;
                const cd f2 = at (r, c) / at (c, c);
                if (f2 == cd (0)) continue;
                for (int k = c; k <= N; ++k) at (r, k) -= f2 * at (c, k);
            }
        }
        return at (out - 1, N) / at (out - 1, out - 1) * outGain;
    }

private:
    struct Elem { Type t; int a, b; double v, v2; double g = 0; double nv = 0, nv2 = 0, tol = 1, tol2 = 1; };
    struct State { double v = 0, i = 0, K = 0; };

    double conductance (const Elem& e) const
    {
        switch (e.t)
        {
            case Type::R:  return 1.0 / e.v;
            case Type::C:  return 2.0 * e.v / T;
            case Type::L:  return T / (2.0 * e.v);
            default:       return 1.0 / (e.v + 2.0 * e.v2 / T);
        }
    }

    void invert (std::array<double, maxNodes * maxNodes>& A)
    {
        // Gauss-Jordan with partial pivoting
        std::array<double, maxNodes * maxNodes> I {};
        for (int i = 0; i < N; ++i) I[(size_t) (i * N + i)] = 1.0;
        auto a = [&A, this] (int r, int c) -> double& { return A[(size_t) (r * N + c)]; };
        auto b = [&I, this] (int r, int c) -> double& { return I[(size_t) (r * N + c)]; };

        for (int c = 0; c < N; ++c)
        {
            int p = c;
            for (int r = c + 1; r < N; ++r)
                if (std::abs (a (r, c)) > std::abs (a (p, c))) p = r;
            if (p != c)
                for (int k = 0; k < N; ++k) { std::swap (a (c, k), a (p, k)); std::swap (b (c, k), b (p, k)); }

            const double inv = 1.0 / a (c, c);
            for (int k = 0; k < N; ++k) { a (c, k) *= inv; b (c, k) *= inv; }
            for (int r = 0; r < N; ++r)
            {
                if (r == c) continue;
                const double f = a (r, c);
                if (f == 0.0) continue;
                for (int k = 0; k < N; ++k) { a (r, k) -= f * a (c, k); b (r, k) -= f * b (c, k); }
            }
        }
        Ginv = I;
    }

    int N = 1, in = 1, out = 1;
    double Rs = 600.0, T = 1.0 / 44100.0, outGain = 1.0;
    bool dirty = true;
    std::vector<Elem> elems;
    std::array<double, maxNodes * maxNodes> Ginv {};
    std::array<std::array<State, maxElems>, maxCh> state {};
};
} // namespace sa670
