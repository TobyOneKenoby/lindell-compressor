#pragma once
#include "Biquad.h"
#include <cmath>

namespace sa670
{
// Static non-linear output stages, run at 2x oversampling.
//  Tube: EQP-1A make-up amp topology (from the Pulse Techniques schematic):
//        HS-56 in -> passive EQ -> HS-29 interstage -> 12AX7 cathode-coupled
//        phase splitter (820R common cathode, 200k plate loads) -> 0.25uF ->
//        12AU7 push-pull (4k7 bypassed cathodes) -> Peerless S-217-D, with
//        overall negative feedback from the output transformer tertiary.
//        Push-pull cancels most even harmonics: mainly soft 3rd harmonic plus a
//        small amount of 2nd from section imbalance; the NFB keeps it clean
//        until it is pushed.
//  SS:   API 2520 discrete op-amp + output transformer style - tighter,
//        more odd harmonics, harder knee near clipping, slight LF "iron".
namespace OutputStage
{
    // one triode half: biased soft transfer curve, zero at rest
    inline float triode (float x) noexcept
    {
        constexpr float drive = 1.4f, bias = 0.22f;
        return std::tanh (drive * (x + bias)) - std::tanh (drive * bias);
    }

    inline float tube (float x) noexcept
    {
        constexpr float imbalance = 0.12f;                  // mismatched halves -> some 2nd harmonic
        static const float norm = [] {
            const float h = 1.0e-4f;
            const float d = (triode (h) - triode (-h)) / (2.0f * h);   // slope of one half at 0
            return 1.0f / d;                                            // push-pull slope = d
        }();
        const float a = triode (x), b = triode (-x);
        return (0.5f * (a - b) + imbalance * 0.5f * (a + b)) * norm;
    }

    inline float solidState (float x) noexcept
    {
        // Mostly linear until close to the rails, then a firmer knee
        const float d = 1.15f;
        const float y = d * x;
        const float a = std::abs (y);
        float s;
        if (a < 0.6f)       s = y - 0.08f * y * y * y;                              // gentle 3rd
        else                s = std::copysign (0.6f - 0.08f * 0.216f
                                               + (1.0f - 0.6f) * std::tanh ((a - 0.6f) / 0.4f) * 0.9f, y);
        return s / d;
    }
}

// Transformer "iron": LF content drives a soft saturation slightly harder.
struct Transformer
{
    Biquad lf;
    void prepare (double fs) { lf.c = BiquadCoeffs::lowPass (fs, 120.0, 0.6); lf.reset(); }
    void reset() { lf.reset(); }
    inline float process (float x, float amount) noexcept
    {
        const float low = lf.process (x);
        const float sat = std::tanh (low * 1.5f) / 1.5f;
        return x + amount * (sat - low);
    }
};
} // namespace sa670
