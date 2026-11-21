#pragma once
#include "Biquad.h"
#include <array>
#include <cmath>

namespace sa670
{
// One side (channel 1 = Left/Lateral, channel 2 = Right/Vertical) of a
// variable-mu (670 style) compressor/limiter.
struct VariMuSettings
{
    float thresholdDb = -20;   // sidechain threshold
    float ratio = 4;           // asymptotic ratio (set by DC threshold)
    float kneeDb = 12;         // soft knee width
    int   timeConstant = 1;    // 0..5 = positions 1..6, 6 = manual
    float manualAttackMs = 10;
    float manualReleaseMs = 300;
    int   scFilter = 0;        // 0 off, 1 90 Hz, 2 180 Hz, 3 mid
};

class VariMuChannel
{
public:
    void prepare (double sampleRate)
    {
        fs = sampleRate;
        reset();
        lastScFilter = -1;
        lastTc = -1;
    }

    void reset()
    {
        for (auto& s : stages) s.env = 0;
        scHp.reset(); scLp.reset();
    }

    void update (const VariMuSettings& s)
    {
        set = s;

        if (s.scFilter != lastScFilter)
        {
            lastScFilter = s.scFilter;
            switch (s.scFilter)
            {
                case 1:  scHp.c = BiquadCoeffs::highPass (fs, 90.0);  scLp.c = BiquadCoeffs::identity(); break;
                case 2:  scHp.c = BiquadCoeffs::highPass (fs, 180.0); scLp.c = BiquadCoeffs::identity(); break;
                case 3:  scHp.c = BiquadCoeffs::highPass (fs, 300.0); scLp.c = BiquadCoeffs::lowPass (fs, 5000.0); break;
                default: scHp.c = BiquadCoeffs::identity();          scLp.c = BiquadCoeffs::identity(); break;
            }
        }

        // 670 time constants (attack / release). Positions 5 and 6 are
        // program dependent: several release stages that charge at
        // different speeds; the slowest charged one dominates.
        if (s.timeConstant != lastTc || s.timeConstant == 6)
        {
            lastTc = s.timeConstant;
            numStages = 1;
            switch (s.timeConstant)
            {
                case 0: setStage (0, 0.2, 300);   break;
                case 1: setStage (0, 0.2, 800);   break;
                case 2: setStage (0, 0.4, 2000);  break;
                case 3: setStage (0, 0.8, 5000);  break;
                case 4: setStage (0, 0.2, 2000);  setStage (1, 1500, 10000); numStages = 2; break;
                case 5: setStage (0, 0.4, 300);   setStage (1, 1200, 10000);
                        setStage (2, 6000, 25000); numStages = 3; break;
                default: setStage (0, s.manualAttackMs, s.manualReleaseMs); break;
            }
        }
    }

    // Returns the target gain reduction in dB (>= 0) for the sidechain sample.
    inline float computeTarget (float sc) noexcept
    {
        float x = scLp.process (scHp.process (sc));
        const float lvl = std::abs (x);
        const float lvlDb = 20.0f * std::log10 (lvl + 1.0e-9f);
        const float over = lvlDb - set.thresholdDb;
        const float W = set.kneeDb;
        const float slope = 1.0f - 1.0f / set.ratio;

        float gr;
        if (over <= -0.5f * W)      gr = 0.0f;
        else if (over < 0.5f * W)   { const float k = over + 0.5f * W; gr = slope * k * k / (2.0f * W); }
        else                        gr = slope * over;

        return std::min (gr, 40.0f);
    }

    // Smooth the target through the time-constant network.
    inline float smooth (float target) noexcept
    {
        float out = 0.0f;
        for (int i = 0; i < numStages; ++i)
        {
            auto& st = stages[(size_t) i];
            const float coef = target > st.env ? st.att : st.rel;
            st.env = target + coef * (st.env - target);
            out = std::max (out, st.env);
        }
        return out;
    }

private:
    struct Stage { float att = 0, rel = 0, env = 0; };

    void setStage (int i, double attMs, double relMs)
    {
        stages[(size_t) i].att = (float) std::exp (-1.0 / (std::max (attMs, 0.01) * 0.001 * fs));
        stages[(size_t) i].rel = (float) std::exp (-1.0 / (std::max (relMs, 1.0) * 0.001 * fs));
    }

    double fs = 44100.0;
    VariMuSettings set;
    std::array<Stage, 3> stages;
    int numStages = 1, lastScFilter = -1, lastTc = -1;
    Biquad scHp, scLp;
};
} // namespace sa670
