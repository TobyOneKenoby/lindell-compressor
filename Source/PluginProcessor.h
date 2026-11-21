#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "DSP/ProgramEQ.h"
#include "DSP/VariMu.h"
#include "DSP/OutputStage.h"

class SA670AudioProcessor : public juce::AudioProcessor
{
public:
    SA670AudioProcessor();
    ~SA670AudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

    // Meter feed for the editor (linear RMS for in/out, dB for GR)
    std::atomic<float> meterIn[2] {}, meterOut[2] {}, meterGR[2] {};

private:
    struct ChannelParams
    {
        std::atomic<float>* inGain; std::atomic<float>* thresh; std::atomic<float>* tc;
        std::atomic<float>* attack; std::atomic<float>* release; std::atomic<float>* bal;
        std::atomic<float>* scFilter; std::atomic<float>* dcThr; std::atomic<float>* mix;
        std::atomic<float>* output;
    };

    std::atomic<float>* p (const char* id) { return apvts.getRawParameterValue (id); }
    void readEqSettings (sa670::ProgramEQSettings& s);
    void processChunk (float* const* chans, int nCh, int n, float* grMax, double* inSum, double* outSum);

    // EQ params
    std::atomic<float> *eqIn, *lfBoost, *lfAtten, *lfFreq, *midAmt, *midBoost, *midFreq,
                       *hfBoost, *hfFreq, *bandwidth, *hfAtten, *attenSel, *tube, *lowCut, *highCut;
    std::atomic<float> *power, *bypass, *compIn, *agcMode, *chLink, *soloMid, *soloSide, *tolL, *tolR;
    ChannelParams chp[2];
    juce::AudioParameterBool* bypassParam = nullptr;

    sa670::ProgramEQ eq;
    sa670::ProgramEQSettings eqSmoothed;
    int osFactor = 4;
    sa670::VariMuChannel comp[2];
    sa670::Transformer transformer[2];
    sa670::DCBlocker dcBlock[2];
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    // smoothed (per sub-block) values
    struct Smooth { float cur = 0, target = 0; void snap() { cur = target; } };
    Smooth sEqIn, sCompIn, sPower, sTube, sBypass;
    Smooth sInGain[2], sOutGain[2], sMix[2];
    bool firstBlock = true;

    // dry path for latency-compensated global bypass
    juce::AudioBuffer<float> dryDelay;
    int dryDelayPos = 0, latency = 0;
    juce::AudioBuffer<float> dryCopy;

    double sr = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA670AudioProcessor)
};
