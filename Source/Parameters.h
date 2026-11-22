#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace sa670::params
{
// ---- Choice tables (values used by the DSP) --------------------------------
inline const juce::StringArray lfFreqNames    { "20", "30", "60", "100" };
inline constexpr float         lfFreqHz[]     { 20, 30, 60, 100 };

inline const juce::StringArray midFreqNames   { "200", "300", "500", "700", "1k", "1.5k", "2k", "3k", "4k", "5k", "7k" };
inline constexpr float         midFreqHz[]    { 200, 300, 500, 700, 1000, 1500, 2000, 3000, 4000, 5000, 7000 };

inline const juce::StringArray hfFreqNames    { "3k", "4k", "5k", "8k", "10k", "12k", "16k" };
inline constexpr float         hfFreqHz[]     { 3000, 4000, 5000, 8000, 10000, 12000, 16000 };

inline const juce::StringArray attenSelNames  { "5k", "10k", "20k" };
inline constexpr float         attenSelHz[]   { 5000, 10000, 20000 };

inline const juce::StringArray lowCutNames    { "Off", "50", "80", "100", "150", "250", "500", "750", "1000", "1500", "2000" };
inline constexpr float         lowCutHz[]     { 0, 50, 80, 100, 150, 250, 500, 750, 1000, 1500, 2000 };

inline const juce::StringArray highCutNames   { "1.5k", "2k", "3k", "4k", "5k", "6k", "8k", "10k", "12k", "15k", "Off" };
inline constexpr float         highCutHz[]    { 1500, 2000, 3000, 4000, 5000, 6000, 8000, 10000, 12000, 15000, 0 };

inline const juce::StringArray tcNames        { "1", "2", "3", "4", "5", "6", "Manual" };
inline const juce::StringArray agcNames       { "Independent", "Link", "Mid-Side", "Mid-Side Link" };
inline const juce::StringArray scFilterNames  { "Off", "90Hz", "180Hz", "Mid" };
inline const juce::StringArray meterNames     { "In", "GR", "Out" };

// ---- IDs -------------------------------------------------------------------
namespace id
{
    inline constexpr const char* eqIn       = "eqIn";
    inline constexpr const char* lfBoost    = "lfBoost";
    inline constexpr const char* lfAtten    = "lfAtten";
    inline constexpr const char* lfFreq     = "lfFreq";
    inline constexpr const char* midAmt     = "midAmt";
    inline constexpr const char* midBoost   = "midBoost";
    inline constexpr const char* midFreq    = "midFreq";
    inline constexpr const char* hfBoost    = "hfBoost";
    inline constexpr const char* hfFreq     = "hfFreq";
    inline constexpr const char* bandwidth  = "bandwidth";
    inline constexpr const char* hfAtten    = "hfAtten";
    inline constexpr const char* attenSel   = "attenSel";
    inline constexpr const char* tube       = "tube";
    inline constexpr const char* lowCut     = "lowCut";
    inline constexpr const char* highCut    = "highCut";

    inline constexpr const char* power      = "power";
    inline constexpr const char* bypass     = "bypass";
    inline constexpr const char* compIn     = "compIn";
    inline constexpr const char* agcMode    = "agcMode";
    inline constexpr const char* chLink     = "chLink";
    inline constexpr const char* soloMid    = "soloMid";
    inline constexpr const char* soloSide   = "soloSide";
    inline constexpr const char* tolL       = "tolL";
    inline constexpr const char* tolR       = "tolR";
    inline constexpr const char* display    = "display";
}

// Per compressor channel: base + "1" (Left/Lat) or "2" (Right/Vert)
namespace chid
{
    inline constexpr const char* inGain   = "inGain";
    inline constexpr const char* thresh   = "thresh";
    inline constexpr const char* tc       = "tc";
    inline constexpr const char* attack   = "attack";
    inline constexpr const char* release  = "release";
    inline constexpr const char* meter    = "meter";
    inline constexpr const char* zero     = "zero";
    inline constexpr const char* bal      = "bal";
    inline constexpr const char* scFilter = "scFilter";
    inline constexpr const char* dcThr    = "dcThr";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* output   = "output";
}

inline juce::String ch (const char* base, int channel) { return juce::String (base) + juce::String (channel + 1); }

inline juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto knob = [] (const String& pid, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { pid, 1 }, name,
                                                      NormalisableRange<float> (0.0f, 10.0f, 0.0f), def,
                                                      AudioParameterFloatAttributes().withStringFromValueFunction (
                                                          [] (float v, int) { return String (v, 1); }));
    };
    auto choice = [] (const String& pid, const String& name, const StringArray& items, int def)
    { return std::make_unique<AudioParameterChoice> (ParameterID { pid, 1 }, name, items, def); };
    auto toggle = [] (const String& pid, const String& name, bool def)
    { return std::make_unique<AudioParameterBool> (ParameterID { pid, 1 }, name, def); };

    // ---- EQ ----
    auto eq = std::make_unique<AudioProcessorParameterGroup> ("eq", "Program EQ", "|");
    eq->addChild (toggle (id::eqIn, "EQ In", true));
    eq->addChild (knob (id::lfBoost, "Low Boost", 0));
    eq->addChild (knob (id::lfAtten, "Low Atten", 0));
    eq->addChild (choice (id::lfFreq, "Low Frequency", lfFreqNames, 2));
    eq->addChild (knob (id::midAmt, "Mid Atten/Boost", 0));
    eq->addChild (toggle (id::midBoost, "Mid Boost/Atten", true));
    eq->addChild (choice (id::midFreq, "Mid Frequency", midFreqNames, 4));
    eq->addChild (knob (id::hfBoost, "High Boost", 0));
    eq->addChild (choice (id::hfFreq, "High Frequency", hfFreqNames, 4));
    eq->addChild (knob (id::bandwidth, "Bandwidth", 5));
    eq->addChild (knob (id::hfAtten, "High Atten", 0));
    eq->addChild (choice (id::attenSel, "Atten Select", attenSelNames, 1));
    eq->addChild (choice (id::lowCut, "Low Cut-Off", lowCutNames, 0));
    eq->addChild (choice (id::highCut, "High Cut-Off", highCutNames, 10));
    eq->addChild (toggle (id::tube, "Output Tube/SS", true));
    layout.add (std::move (eq));

    // ---- Global ----
    auto glob = std::make_unique<AudioProcessorParameterGroup> ("global", "Global", "|");
    glob->addChild (toggle (id::power, "Output Stage On", true));
    glob->addChild (toggle (id::bypass, "Bypass", false));
    glob->addChild (toggle (id::compIn, "Compressor In/Out", true));
    glob->addChild (toggle (id::chLink, "L/R Control Link", false));
    glob->addChild (toggle (id::soloMid, "Solo Mid", false));
    glob->addChild (toggle (id::soloSide, "Solo Side", false));
    glob->addChild (std::make_unique<AudioParameterInt> (ParameterID { id::tolL, 1 }, "Tolerance Channel L", 0, 99, 10));
    glob->addChild (std::make_unique<AudioParameterInt> (ParameterID { id::tolR, 1 }, "Tolerance Channel R", 0, 99, 11));
    glob->addChild (toggle (id::display, "Analyser Display", false));
    glob->addChild (choice (id::agcMode, "AGC Mode", agcNames, 1));
    layout.add (std::move (glob));

    // ---- Compressor channels ----
    for (int c = 0; c < 2; ++c)
    {
        const String nm = c == 0 ? "L/Lat " : "R/Vert ";
        auto g = std::make_unique<AudioProcessorParameterGroup> ("comp" + String (c + 1), "Compressor " + nm, "|");

        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ch (chid::inGain, c), 1 }, nm + "Input Gain",
                         NormalisableRange<float> (0.0f, 20.0f, 0.0f), 0.0f,
                         AudioParameterFloatAttributes().withLabel ("dB")));
        g->addChild (knob (ch (chid::thresh, c), nm + "Threshold", 3.0f));
        g->addChild (choice (ch (chid::tc, c), nm + "Time Constant", tcNames, 1));

        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ch (chid::attack, c), 1 }, nm + "Manual Attack",
                         NormalisableRange<float> (0.1f, 100.0f, 0.0f, 0.35f), 5.0f,
                         AudioParameterFloatAttributes().withLabel ("ms")));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ch (chid::release, c), 1 }, nm + "Manual Release",
                         NormalisableRange<float> (20.0f, 5000.0f, 0.0f, 0.35f), 400.0f,
                         AudioParameterFloatAttributes().withLabel ("ms")));

        g->addChild (choice (ch (chid::meter, c), nm + "Metering", meterNames, 1));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ch (chid::zero, c), 1 }, nm + "Meter Zero",
                         NormalisableRange<float> (-3.0f, 3.0f, 0.0f), 0.0f,
                         AudioParameterFloatAttributes().withLabel ("dB")));
        g->addChild (std::make_unique<AudioParameterFloat> (ParameterID { ch (chid::bal, c), 1 }, nm + "Balance Trim",
                         NormalisableRange<float> (-2.0f, 2.0f, 0.0f), 0.0f,
                         AudioParameterFloatAttributes().withLabel ("dB")));
        g->addChild (choice (ch (chid::scFilter, c), nm + "AGC Filter", scFilterNames, 0));
        g->addChild (knob (ch (chid::dcThr, c), nm + "DC Threshold", 5.0f));
        g->addChild (knob (ch (chid::mix, c), nm + "Dry/Wet", 10.0f));
        g->addChild (knob (ch (chid::output, c), nm + "Output", 5.0f));
        layout.add (std::move (g));
    }

    return layout;
}
} // namespace sa670::params
