#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace sa670::ui
{
/** Fonts bundled with the plugin (Jost, SIL OFL). */
juce::Font panelFont (float height, bool semiBold = false);

/** The rack panel drawn entirely as vectors: powder coat, rack ears, screws,
    every legend and scale, the VU housings and the pilot lamp bezel.
    Rendered once per scale into a cache, so it is sharp at any size/DPI. */
class PanelArt : public juce::Component
{
public:
    PanelArt();
    void paint (juce::Graphics&) override;
    static constexpr int width = 1308, height = 1203;
};

/** Living details on top of the panel: pilot lamp glow that lights up the
    paint around it, and the warm-up of the lamp when the unit is switched on. */
class LampGlow : public juce::Component, private juce::Timer
{
public:
    LampGlow();
    void setOn (bool on);
    float getBrightness() const { return level; }
    void paint (juce::Graphics&) override;
    std::function<void (float)> onBrightness;   // 0..1, drives the VU backlight

private:
    void timerCallback() override;
    bool target = true;
    float level = 1.0f, flicker = 0.0f;
    juce::Random rng;
};
} // namespace sa670::ui
