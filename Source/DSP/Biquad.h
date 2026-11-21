#pragma once
#include <cmath>
#include <algorithm>

namespace sa670
{
// RBJ "Audio EQ Cookbook" biquad, Transposed Direct Form II, double precision.
struct BiquadCoeffs
{
    double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;

    static constexpr double pi = 3.14159265358979323846;

    static BiquadCoeffs identity() { return {}; }

    static BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        BiquadCoeffs c;
        c.b0 = b0 / a0; c.b1 = b1 / a0; c.b2 = b2 / a0;
        c.a1 = a1 / a0; c.a2 = a2 / a0;
        return c;
    }

    static double clampFreq (double fs, double f) { return std::clamp (f, 5.0, fs * 0.45); }

    static BiquadCoeffs peak (double fs, double f, double q, double gainDb)
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise (1 + alpha * A, -2 * cw, 1 - alpha * A, 1 + alpha / A, -2 * cw, 1 - alpha / A);
    }

    static BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb)
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) - (A - 1) * cw + sA),
                          2 * A * ((A - 1) - (A + 1) * cw),
                          A * ((A + 1) - (A - 1) * cw - sA),
                          (A + 1) + (A - 1) * cw + sA,
                          -2 * ((A - 1) + (A + 1) * cw),
                          (A + 1) + (A - 1) * cw - sA);
    }

    static BiquadCoeffs highShelf (double fs, double f, double q, double gainDb)
    {
        f = clampFreq (fs, f);
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        const double sA = 2.0 * std::sqrt (A) * alpha;
        return normalise (A * ((A + 1) + (A - 1) * cw + sA),
                          -2 * A * ((A - 1) + (A + 1) * cw),
                          A * ((A + 1) + (A - 1) * cw - sA),
                          (A + 1) - (A - 1) * cw + sA,
                          2 * ((A - 1) - (A + 1) * cw),
                          (A + 1) - (A - 1) * cw - sA);
    }

    static BiquadCoeffs highPass (double fs, double f, double q = 0.7071)
    {
        f = clampFreq (fs, f);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1 + cw) / 2, -(1 + cw), (1 + cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }

    // First-order (6 dB/oct) low-pass, bilinear transform with prewarping.
    static BiquadCoeffs lowPass1 (double fs, double f)
    {
        f = clampFreq (fs, f);
        const double k = std::tan (pi * f / fs);
        return normalise (k, k, 0.0, 1.0 + k, k - 1.0, 0.0);
    }

    static BiquadCoeffs lowPass (double fs, double f, double q = 0.7071)
    {
        f = clampFreq (fs, f);
        const double w = 2.0 * pi * f / fs, cw = std::cos (w), alpha = std::sin (w) / (2.0 * q);
        return normalise ((1 - cw) / 2, 1 - cw, (1 - cw) / 2, 1 + alpha, -2 * cw, 1 - alpha);
    }
};

struct Biquad
{
    BiquadCoeffs c;
    double z1 = 0, z2 = 0;

    void reset() { z1 = z2 = 0; }

    inline float process (float xf) noexcept
    {
        const double x = xf;
        const double y = c.b0 * x + z1;
        z1 = c.b1 * x - c.a1 * y + z2;
        z2 = c.b2 * x - c.a2 * y;
        return (float) y;
    }
};

// One-pole DC blocker (~5 Hz).
struct DCBlocker
{
    double R = 0.9995, x1 = 0, y1 = 0;
    void prepare (double fs) { R = std::exp (-2.0 * BiquadCoeffs::pi * 5.0 / fs); reset(); }
    void reset() { x1 = y1 = 0; }
    inline float process (float x) noexcept
    {
        const double y = x - x1 + R * y1;
        x1 = x; y1 = y;
        return (float) y;
    }
};
} // namespace sa670
