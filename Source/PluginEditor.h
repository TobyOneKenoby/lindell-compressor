#pragma once
#include "PluginProcessor.h"
#include "UI/Sprites.h"

class SA670AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SA670AudioProcessorEditor (SA670AudioProcessor&);
    ~SA670AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int panelW = 1308, rackH = 1203;
    static constexpr int panelH = rackH + sa670::ui::BottomBar::height;

private:
    /** Panel at native artwork resolution; the editor scales it as a whole. */
    struct Panel : public juce::Component
    {
        juce::Image art;
        void paint (juce::Graphics& g) override
        {
            g.drawImage (art, juce::Rectangle<float> (0, 0, (float) panelW, (float) rackH));
        }
    };

    void parameterChanged (const juce::String& id, float newValue) override;
    void mirrorLeftToRight();

    SA670AudioProcessor& proc;
    Panel panel;
    sa670::ui::ValueBubble bubble;
    std::vector<std::unique_ptr<sa670::ui::SpriteControl>> controls;
    std::unique_ptr<sa670::ui::VUNeedle> needles[2];
    sa670::ui::LampDimmer lamp;
    sa670::ui::BottomBar bottomBar;
    sa670::ui::SpriteControl* manualKnobs[2][2] {};
    bool mirroring = false;
    juce::TooltipWindow tooltips { this, 900 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA670AudioProcessorEditor)
};
