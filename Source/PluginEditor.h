#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
class RackLook final : public juce::LookAndFeel_V4 {
public:
 void drawRotarySlider(juce::Graphics&,int,int,int,int,float,float,float,juce::Slider&) override;
 void drawToggleButton(juce::Graphics&,juce::ToggleButton&,bool,bool) override;
 juce::Font getLabelFont(juce::Label&) override;
};
class FineKnob final : public juce::Slider {
public:
 void mouseDrag(const juce::MouseEvent& e) override {setMouseDragSensitivity(e.mods.isShiftDown()?2400:240);juce::Slider::mouseDrag(e);}
};
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 Processor& processor;
 RackLook look;
 std::array<FineKnob,5> knobs;
 std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,5> attachments;
 juce::ToggleButton bypass{"BYPASS"}, knee{"SOFT KNEE"};
 std::array<juce::ToggleButton,3> meterButtons;
 std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttach,kneeAttach;
 juce::Image panel;
 juce::TooltipWindow tooltips{this,700};
 int meterMode=2;
 float needle=1.0f;
 void timerCallback() override;
 void drawMeter(juce::Graphics&);
 void makePanel();
public:
 explicit Editor(Processor&);
 ~Editor() override;
 void paint(juce::Graphics&) override;
 void resized() override;
};
