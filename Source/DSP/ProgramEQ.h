#pragma once
#include "PultecNetworks.h"
#include "Biquad.h"
#include <array>

namespace sa670
{
// Complete passive program-EQ chain, run at the oversampled rate:
//   EQP-1A network -> MEQ-5 network -> low cut (24 dB/oct) -> high cut (6 dB/oct)
struct ProgramEQSettings
{
    EQP1ANetwork::Settings eqp;
    double midAmt = 0;  bool midBoost = true;  double midFreq = 1000;
    double lowCut = 0;  double highCut = 0;   // Hz, 0 = off
};

class ProgramEQ
{
public:
    void prepare (double fs)
    {
        rate = fs;
        for (int c = 0; c < 2; ++c) { eqp[c].prepare (fs); meq[c].prepare (fs); }
        tolSeed[0] = tolSeed[1] = -1;
        for (auto& ch : cuts) for (auto& b : ch) b.reset();
        lastLowCut = lastHighCut = -1;
    }

    void reset()
    {
        for (int c = 0; c < 2; ++c) { eqp[c].reset(); meq[c].reset(); }
        for (auto& ch : cuts) for (auto& b : ch) b.reset();
    }

    void update (const ProgramEQSettings& s)
    {
        for (int c = 0; c < 2; ++c)
        {
            eqp[c].update (s.eqp);
            meq[c].update (s.midAmt, s.midBoost, s.midFreq);
        }

        if (s.lowCut != lastLowCut || s.highCut != lastHighCut)
        {
            lastLowCut = s.lowCut; lastHighCut = s.highCut;
            const auto h1 = s.lowCut > 0 ? BiquadCoeffs::highPass (rate, s.lowCut, 0.54119610) : BiquadCoeffs::identity();
            const auto h2 = s.lowCut > 0 ? BiquadCoeffs::highPass (rate, s.lowCut, 1.30656296) : BiquadCoeffs::identity();
            const auto l1 = s.highCut > 0 ? BiquadCoeffs::lowPass1 (rate, s.highCut) : BiquadCoeffs::identity();
            for (auto& ch : cuts) { ch[0].c = h1; ch[1].c = h2; ch[2].c = l1; }
        }
    }

    inline float process (int ch, float x) noexcept
    {
        x = eqp[(size_t) ch].process (0, x);
        x = meq[(size_t) ch].process (0, x);
        auto& c = cuts[(size_t) ch];
        return c[2].process (c[1].process (c[0].process (x)));
    }

    /** Overall magnitude (linear) of channel 0 at f - used for the EQ curve display. */
    double magnitude (double f) const
    {
        const auto& c = cuts[0];
        return std::abs (eqp[0].response (f)) * std::abs (meq[0].response (f))
             * c[0].c.magnitude (rate, f) * c[1].c.magnitude (rate, f) * c[2].c.magnitude (rate, f);
    }

    /** Component tolerance "channel" per side (0..99). */
    void setTolerance (int ch, int seed)
    {
        if (tolSeed[ch] == seed) return;
        tolSeed[ch] = seed;
        eqp[(size_t) ch].setTolerance (seed);
        meq[(size_t) ch].setTolerance (seed + 1000);
    }

private:
    double rate = 44100;
    std::array<EQP1ANetwork, 2> eqp;
    std::array<MEQ5Network, 2> meq;
    int tolSeed[2] { -1, -1 };
    std::array<std::array<Biquad, 3>, 2> cuts;
    double lastLowCut = -1, lastHighCut = -1;
};
} // namespace sa670
