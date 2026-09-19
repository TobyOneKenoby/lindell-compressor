#pragma once
#include <array>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Compressor.h"
class Processor final : public juce::AudioProcessor {
public:
 Processor();
 juce::AudioProcessorValueTreeState state;
 std::atomic<float> inputDb{-100},outputDb{-100},grDb{0};
 std::atomic<float> inputVuDb{-100},outputVuDb{-100};
 lindell::Compressor core;
 lindell::Settings settings() const;
 void prepareToPlay(double,int) override;
 void releaseResources() override {}
 bool isBusesLayoutSupported(const BusesLayout&) const override;
 void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
 juce::AudioProcessorEditor* createEditor() override;
 bool hasEditor() const override{return true;}
 const juce::String getName() const override{return "Lindell Rack Compressor";}
 bool acceptsMidi() const override{return false;} bool producesMidi() const override{return false;}
 double getTailLengthSeconds() const override{return 0;}
 int getNumPrograms() override{return 1;} int getCurrentProgram() override{return 0;}
 void setCurrentProgram(int) override{} const juce::String getProgramName(int) override{return {};}
 void changeProgramName(int,const juce::String&) override{}
 void getStateInformation(juce::MemoryBlock&) override;
 void setStateInformation(const void*,int) override;
private:
 std::array<std::atomic<float>*,7> values{};
 float inMeter=0,outMeter=0; double meterDecay=0, inputPower=0, outputPower=0;
};
