#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_dsp/juce_dsp.h>
#include "../PluginProcessor.h"

namespace sa670::ui
{
/** Spectrum analyser of the output with the program-EQ curve on top.
    The curve is computed from the same circuit model the audio runs through. */
class Analyser : public juce::Component, private juce::Timer
{
public:
    explicit Analyser (SA670AudioProcessor&);
    ~Analyser() override;

    void paint (juce::Graphics&) override;
    void visibilityChanged() override;
    static constexpr int height = 300;
    void refreshNow() { timerCallback(); }   // used by the offline test

private:
    void timerCallback() override;
    void updateCurve();
    float xForFreq (float f, juce::Rectangle<float> r) const;

    static constexpr int fftOrder = 12, fftSize = 1 << fftOrder;
    SA670AudioProcessor& proc;
    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> ring = std::vector<float> ((size_t) fftSize, 0.0f);
    int ringPos = 0;
    std::vector<float> fftData = std::vector<float> ((size_t) fftSize * 2, 0.0f);
    std::vector<float> specDb = std::vector<float> ((size_t) fftSize / 2, -120.0f);

    sa670::ProgramEQ model;
    sa670::ProgramEQSettings lastSettings;
    bool lastEqIn = true, haveCurve = false;
    std::vector<float> curveDb;
    juce::Image background;
};
} // namespace sa670::ui
