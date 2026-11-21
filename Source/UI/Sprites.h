#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Layout.h"

namespace sa670::ui
{
/** Loads a PNG/JPEG embedded via juce_add_binary_data (cached). */
juce::Image loadAsset (const juce::String& resourceName);

/** Shows "Name  value" next to the control being touched (DISPLAY button). */
class ValueBubble : public juce::Component
{
public:
    ValueBubble() { setInterceptsMouseClicks (false, false); setAlwaysOnTop (true); }
    void show (juce::Component& source, const juce::String& text);
    void hide() { setVisible (false); }
    void paint (juce::Graphics&) override;
    bool enabled = true;
private:
    juce::String text;
};

//==============================================================================
/** Any rotating control made from a photographic sprite cut out of the panel.
    Continuous parameters rotate between steps[0] and steps[1]; choice
    parameters snap to one angle per position. Fluted knobs keep the sprite
    static (so the studio lighting stays put) and draw the index line. */
class SpriteControl : public juce::Component, public juce::SettableTooltipClient
{
public:
    SpriteControl (juce::AudioProcessorValueTreeState&, const layout::Control&, const juce::String& paramId, ValueBubble*, float scale = 1.0f);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;
    void mouseEnter (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;

    /** Place so that the pivot sits at (x, y) in panel coordinates. */
    void placeAt (float x, float y);

    std::function<void (float)> onValueChange;
    juce::RangedAudioParameter& getParam() { return param; }

private:
    float currentAngle() const;
    void setNormalised (float v01);
    void showValue();

    const layout::Control& info;
    juce::RangedAudioParameter& param;
    juce::Image sprite, shadow;
    float scale;
    float value01 = 0, dragStart01 = 0, wheelAccum = 0;
    int numChoices = 0;
    bool isToggle = false, dragging = false, moved = false;
    ValueBubble* bubble;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Moving-coil needle drawn over the meter face in the artwork. */
class VUNeedle : public juce::Component, private juce::Timer
{
public:
    VUNeedle (std::function<float()> sourceVU, float yOffset);
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    juce::Point<float> scalePoint (float db) const;
    std::function<float()> source;
    float dy;
    float needleDb = -22.0f, velocity = 0.0f;
};

//==============================================================================
/** Pilot lamp: the artwork shows it lit; darkened when the unit is off. */
class LampDimmer : public juce::Component
{
public:
    LampDimmer() { setInterceptsMouseClicks (false, false); }
    void setLit (bool l) { if (lit != l) { lit = l; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    bool lit = true;
};

//==============================================================================
/** Square backlit push button (bottom bar). */
class LitButton : public juce::Component, public juce::SettableTooltipClient
{
public:
    enum class Icon { None, Left, Right };
    explicit LitButton (Icon i = Icon::None) : icon (i) { setMouseCursor (juce::MouseCursor::PointingHandCursor); }
    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override { pressed = true; repaint(); }
    void mouseUp (const juce::MouseEvent& e) override
    {
        pressed = false; repaint();
        if (getLocalBounds().contains (e.getPosition()) && onClick) onClick();
    }
    void setLit (bool l) { if (lit != l) { lit = l; repaint(); } }
    std::function<void()> onClick;
private:
    Icon icon;
    bool lit = false, pressed = false;
};

/** Bound to a bool parameter. */
class ParamButton : public LitButton
{
public:
    ParamButton (juce::AudioProcessorValueTreeState& s, const juce::String& id);
private:
    juce::RangedAudioParameter& param;
    juce::ParameterAttachment attachment;
};

//==============================================================================
/** Green 7-segment style display of the two tolerance channels. */
class LedDisplay : public juce::Component, public juce::SettableTooltipClient
{
public:
    void setValues (int l, int r) { if (l != a || r != b) { a = l; b = r; repaint(); } }
    void paint (juce::Graphics&) override;
private:
    int a = 10, b = 11;
};

//==============================================================================
/** Black anodised strip under the rack panel. */
class BottomBar : public juce::Component
{
public:
    explicit BottomBar (juce::AudioProcessorValueTreeState&);
    ~BottomBar() override;
    void paint (juce::Graphics&) override;
    void resized() override;
    static constexpr int height = 150;

private:
    void refreshDisplay();
    void stepTolerance (int delta);
    juce::AudioProcessorValueTreeState& state;
    ParamButton display, soloM, soloS;
    LitButton random, prev { LitButton::Icon::Left }, next { LitButton::Icon::Right };
    LedDisplay led;
    juce::ParameterAttachment tolLAttach, tolRAttach;
};
} // namespace sa670::ui
