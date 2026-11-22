#pragma once
#include "PluginProcessor.h"
#include "UI/Sprites.h"
#include "UI/Analyser.h"
#include "UI/PanelArt.h"

class SA670AudioProcessorEditor : public juce::AudioProcessorEditor, private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit SA670AudioProcessorEditor (SA670AudioProcessor&);
    ~SA670AudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    static constexpr int panelW = 1308, rackH = 1203;
    static constexpr int panelH = rackH + sa670::ui::BottomBar::height;   // without analyser

private:
    /** Container in panel coordinates; the editor scales it as a whole. */
    struct Panel : public juce::Component {};

    void parameterChanged (const juce::String& id, float newValue) override;
    void mirrorChannels (int fromChannel);
    void showAnalyser (bool show);
    bool controlsLinked() const;

    SA670AudioProcessor& proc;
    Panel panel;
    sa670::ui::ValueBubble bubble;
    std::vector<std::unique_ptr<sa670::ui::SpriteControl>> controls;
    std::unique_ptr<sa670::ui::VUNeedle> needles[2];
    sa670::ui::PanelArt art;
    sa670::ui::LampGlow lamp;
    sa670::ui::BottomBar bottomBar;
    sa670::ui::Analyser analyser;
    int logicalH = panelH;
    sa670::ui::SpriteControl* manualKnobs[2][2] {};
    bool mirroring = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SA670AudioProcessorEditor)
};
