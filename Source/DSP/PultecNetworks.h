#pragma once
#include "PassiveNetwork.h"

namespace sa670
{
// ---------------------------------------------------------------------------
// Potentiometer tapers (position p = 0..1 from CCW to CW)
// ---------------------------------------------------------------------------
namespace taper
{
    inline double linear (double p) { return p; }
    // audio / log ("B"): ~10 % at mid rotation
    inline double audio (double p)  { return (std::pow (10.0, 2.0 * p) - 1.0) / 99.0; }
    // negative log ("C")
    inline double revLog (double p) { return 1.0 - audio (1.0 - p); }
}

constexpr double minR = 0.5; // a pot "at zero" still has contact resistance
inline double potR (double value) { return std::max (value, minR); }

// Load presented by the interstage transformer T2 (10k : 10k+10k, 15k across the
// secondary, DCR 637R primary / 2 x 513R secondary). The effective reflected
// impedance depends on the transformer ratio, which the schematic does not pin
// down; 14k is calibrated so max LOW BOOST / LOW ATTEN match the published
// EQP-1A figures (+13.5 dB / -17.5 dB) as closely as the network allows.
constexpr double interstageLoad = 14000.0;
constexpr double interstageC    = 2.0 * 100e-12;   // 100p across secondary, reflected
// Inductor DC resistance estimate (Ohm per H) - not specified on the schematics.
inline double dcr (double L) { return 5.0 + 250.0 * L; }

// ===========================================================================
//  EQP-1A program equalizer network (Pultec / Tubetec PE-1a schematic)
// ===========================================================================
//
//  IN --[ BOOST HI 10K-A ]-- A --[ CUT LO 100K-B || C_cut ]-- OUT --> T2 (load)
//          |wiper                |
//          +--C_hf--L_hf--Q 2K2--+  (series resonant, bypasses the pot)
//                                 |
//                       A --[ CUT HI 1K-C ]-- B --[ BOOST LO 10K-B || C_boost ]-- GND
//                               |wiper
//                               +--75R--C_hicut-- B
//
//  Source: balanced input via 600:600 T1 with 2 x 62R build-out, 570R across.
class EQP1ANetwork
{
public:
    struct Settings
    {
        double lowBoost = 0, lowAtten = 0;   // 0..10 knob
        int    cps = 2;                      // 20, 30, 60, 100 Hz
        double highBoost = 0;                // 0..10
        int    kcs = 4;                      // 3,4,5,8,10,12,16 kHz
        double bandwidth = 5;                // 0 sharp .. 10 broad
        double highAtten = 0;                // 0..10
        int    attenSel = 1;                 // 5, 10, 20 kHz
    };

    void prepare (double fs)
    {
        enum { IN = 1, W, X1, A, OUT, W2, Y, B, numNodes = 8 };
        net.reset (numNodes, IN, OUT, 124.0);
        net.setSampleRate (fs);

        net.add (PassiveNetwork::Type::R, IN, 0, 570.0);           // input termination
        rIW   = net.add (PassiveNetwork::Type::R, IN, W, 10000.0); // BOOST HI pot, upper part
        rWA   = net.add (PassiveNetwork::Type::R, W, A, minR);     // BOOST HI pot, lower part
        cHf   = net.add (PassiveNetwork::Type::C, W, X1, 10e-9);
        lHf   = net.add (PassiveNetwork::Type::RL, X1, A, 0.0, 65e-3); // L1 + DCR + Q pot (2K2)

        rCutLo = net.add (PassiveNetwork::Type::R, A, OUT, minR);  // CUT LO 100K-B
        cCutLo = net.add (PassiveNetwork::Type::C, A, OUT, 22e-9);

        rAW2  = net.add (PassiveNetwork::Type::R, A, W2, 1000.0);  // CUT HI 1K-C, upper part
        rW2B  = net.add (PassiveNetwork::Type::R, W2, B, minR);    // CUT HI, lower part
        net.add (PassiveNetwork::Type::R, W2, Y, 75.0);
        cHiCut = net.add (PassiveNetwork::Type::C, Y, B, 94e-9);

        rBoostLo = net.add (PassiveNetwork::Type::R, B, 0, minR);  // BOOST LO 10K-B
        cBoostLo = net.add (PassiveNetwork::Type::C, B, 0, 470e-9);

        net.add (PassiveNetwork::Type::R, OUT, 0, interstageLoad);
        net.add (PassiveNetwork::Type::C, OUT, 0, interstageC);

        // Make-up gain: the tube amp restores the network's insertion loss so the
        // unit is 0 dB overall with all controls at zero (as on the original).
        update (Settings {});
        net.setOutputGain (1.0);
        net.setOutputGain (1.0 / std::abs (net.analogResponse (1000.0)));
        net.rebuildIfNeeded();
        net.clearState();
    }

    void update (const Settings& s)
    {
        // CPS: boost and attenuation capacitors (ganged switch)
        static constexpr double boostC[] = { 2.2e-6, 1.0e-6, 470e-9, 330e-9 };   // 20/30/60/100
        static constexpr double cutC[]   = { 100e-9, 47e-9, 22e-9, 15e-9 };
        // KCS: high boost series resonant L/C (schematic table)
        static constexpr double hfC[] = { 15e-9, 15e-9, 10e-9, 10e-9, 6.8e-9, 6.8e-9, 3.3e-9 };
        static constexpr double hfL[] = { 175e-3, 100e-3, 90e-3, 65e-3, 35e-3, 23e-3, 19e-3 };
        // Atten sel: 5k / 10k / 20k
        static constexpr double hiCutC[] = { 150e-9, 94e-9, 47e-9 };

        const int cps = std::clamp (s.cps, 0, 3), kcs = std::clamp (s.kcs, 0, 6), as = std::clamp (s.attenSel, 0, 2);

        const double hb = taper::linear (s.highBoost / 10.0);
        net.set (rIW, potR (10000.0 * (1.0 - hb)));
        net.set (rWA, potR (10000.0 * hb));
        net.set (cHf, hfC[kcs]);
        const double q = 2200.0 * taper::linear (s.bandwidth / 10.0);          // HI BOOST Q 2K2
        net.set (lHf, dcr (hfL[kcs]) + q, hfL[kcs]);

        net.set (rCutLo, potR (100000.0 * taper::audio (s.lowAtten / 10.0)));
        net.set (cCutLo, cutC[cps]);

        const double hc = taper::revLog (s.highAtten / 10.0);
        net.set (rAW2, potR (1000.0 * (1.0 - hc)));
        net.set (rW2B, potR (1000.0 * hc));
        net.set (cHiCut, hiCutC[as]);

        net.set (rBoostLo, potR (10000.0 * taper::audio (s.lowBoost / 10.0)));
        net.set (cBoostLo, boostC[cps]);

        net.rebuildIfNeeded();
    }

    inline float process (int ch, float x) noexcept { return net.process (ch, x); }
    void reset() { net.clearState(); }
    void setTolerance (int seed) { net.setTolerance (seed); net.rebuildIfNeeded(); }
    std::complex<double> response (double f) const { return net.analogResponse (f); }

private:
    PassiveNetwork net;
    int rIW = 0, rWA = 0, cHf = 0, lHf = 0, rCutLo = 0, cCutLo = 0, rAW2 = 0, rW2B = 0, cHiCut = 0, rBoostLo = 0, cBoostLo = 0;
};

// ===========================================================================
//  MEQ-5 mid-range equalizer network (Gyraf MEQ5 filter section schematic)
// ===========================================================================
//
//  IN --[LOW BOOST 2K5-A]-- M1 --510R-- M2 --[HIGH BOOST 10K-A]-- OUT --> T2
//          |wiper           |           |         |wiper          |
//          +--L_lb--C_lb----+          [MID CUT   +--L_hb--C_hb---+
//                           1K          2K5-C]                    5K1
//                           |           |wiper                    |
//                          GND          +--270R--L_mc--C_mc--GND  GND
//
//  Peak (low boost) 200 Hz..1 kHz, peak (high boost) 1.5..5 kHz, dip 200 Hz..7 kHz.
//  Inductor taps from the schematic; capacitors chosen for the panel frequency.
class MEQ5Network
{
public:
    void prepare (double fs)
    {
        enum { IN = 1, W1, P1, M1, M2, W3, Q1, W4, P4, OUT, numNodes = 10 };
        net.reset (numNodes, IN, OUT, 124.0);
        net.setSampleRate (fs);

        net.add (PassiveNetwork::Type::R, IN, 0, 620.0);
        rIW1 = net.add (PassiveNetwork::Type::R, IN, W1, 2500.0);
        rW1M = net.add (PassiveNetwork::Type::R, W1, M1, minR);
        lLb  = net.add (PassiveNetwork::Type::RL, W1, P1, dcr (0.12), 0.12);
        cLb  = net.add (PassiveNetwork::Type::C, P1, M1, 200e-9);
        net.add (PassiveNetwork::Type::R, M1, 0, 1000.0);

        net.add (PassiveNetwork::Type::R, M1, M2, 510.0);
        rM2W3 = net.add (PassiveNetwork::Type::R, M2, W3, 2500.0);
        rW3G  = net.add (PassiveNetwork::Type::R, W3, 0, minR);
        lMc   = net.add (PassiveNetwork::Type::RL, W3, Q1, 270.0 + dcr (0.108), 0.108);
        cMc   = net.add (PassiveNetwork::Type::C, Q1, 0, 220e-9);

        rM2W4 = net.add (PassiveNetwork::Type::R, M2, W4, 10000.0);
        rW4O  = net.add (PassiveNetwork::Type::R, W4, OUT, minR);
        lHb   = net.add (PassiveNetwork::Type::RL, W4, P4, dcr (0.477), 0.477);
        cHb   = net.add (PassiveNetwork::Type::C, P4, OUT, 2e-9);
        net.add (PassiveNetwork::Type::R, OUT, 0, 5100.0);

        net.add (PassiveNetwork::Type::R, OUT, 0, interstageLoad);
        net.add (PassiveNetwork::Type::C, OUT, 0, interstageC);

        update (0.0, true, 1000.0);
        net.setOutputGain (1.0);
        net.setOutputGain (1.0 / std::abs (net.analogResponse (1000.0)));
        net.rebuildIfNeeded();
        net.clearState();
    }

    /** amount 0..10, boost = BOOST/ATTEN switch, freq = panel MID frequency. */
    void update (double amount, bool boost, double freq)
    {
        const double p = amount / 10.0;
        const double twoPiF2 = std::pow (2.0 * 3.14159265358979323846 * freq, 2.0);

        double lb = 0, hb = 0, mc = 0;
        if (boost) { if (freq <= 1000.0) lb = p; else hb = p; }
        else       mc = p;

        // Low boost (inductor taps 120/280/420/520 mH)
        const double Llb = freq >= 1000.0 ? 0.12 : freq >= 700.0 ? 0.28 : freq >= 500.0 ? 0.28 : freq >= 300.0 ? 0.42 : 0.52;
        net.set (rIW1, potR (2500.0 * (1.0 - taper::linear (lb))));
        net.set (rW1M, potR (2500.0 * taper::linear (lb)));
        net.set (lLb, dcr (Llb), Llb);
        net.set (cLb, 1.0 / (twoPiF2 * Llb));

        // Mid cut (inductor taps 34/61/108/145/277/420 mH, 270R in series)
        const double Lmc = freq <= 200 ? 0.420 : freq <= 300 ? 0.277 : freq <= 700 ? 0.145 : freq <= 1000 ? 0.108
                         : freq <= 1500 ? 0.061 : freq <= 2000 ? 0.034 : freq <= 4000 ? 0.108 : freq <= 5000 ? 0.061 : 0.034;
        const double c = taper::revLog (mc);
        net.set (rM2W3, potR (2500.0 * (1.0 - c)));
        net.set (rW3G, potR (2500.0 * c));
        net.set (lMc, 270.0 + dcr (Lmc), Lmc);
        net.set (cMc, 1.0 / (twoPiF2 * Lmc));

        // High boost (inductor taps 477 / 625 mH)
        const double Lhb = freq <= 2000.0 ? 0.625 : 0.477;
        net.set (rM2W4, potR (10000.0 * (1.0 - taper::linear (hb))));
        net.set (rW4O, potR (10000.0 * taper::linear (hb)));
        net.set (lHb, dcr (Lhb), Lhb);
        net.set (cHb, 1.0 / (twoPiF2 * Lhb));

        net.rebuildIfNeeded();
    }

    inline float process (int ch, float x) noexcept { return net.process (ch, x); }
    void reset() { net.clearState(); }
    void setTolerance (int seed) { net.setTolerance (seed); net.rebuildIfNeeded(); }
    std::complex<double> response (double f) const { return net.analogResponse (f); }

private:
    PassiveNetwork net;
    int rIW1 = 0, rW1M = 0, lLb = 0, cLb = 0, rM2W3 = 0, rW3G = 0, lMc = 0, cMc = 0, rM2W4 = 0, rW4O = 0, lHb = 0, cHb = 0;
};
} // namespace sa670
