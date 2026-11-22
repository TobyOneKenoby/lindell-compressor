#include "PanelArt.h"
#include "Layout.h"
#include "BinaryData.h"

namespace sa670::ui
{
using namespace juce;

//==============================================================================
Font panelFont (float h, bool semi)
{
    static Typeface::Ptr medium = Typeface::createSystemTypefaceFor (BinaryData::JostMedium_ttf, BinaryData::JostMedium_ttfSize);
    static Typeface::Ptr bold   = Typeface::createSystemTypefaceFor (BinaryData::JostSemiBold_ttf, BinaryData::JostSemiBold_ttfSize);
    return Font (FontOptions (semi ? bold : medium).withHeight (h * 1.12f)).withExtraKerningFactor (0.035f);
}

namespace
{
    const Colour legend (0xffe9e5d8);          // cream screen-print
    const Colour paintTop (0xff2e6171), paintMid (0xff285a69), paintBottom (0xff1f4e5c);

    const layout::Control* ctl (const char* name)
    {
        for (auto& c : layout::controls)
            if (String (c.name) == name) return &c;
        jassertfalse;
        return &layout::controls[0];
    }

    Point<float> polar (Point<float> c, float r, float deg)
    {
        const float a = degreesToRadians (deg);
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }

    void text (Graphics& g, const String& s, float cx, float cy, float size, bool semi = false,
               Justification j = Justification::centred)
    {
        g.setFont (panelFont (size, semi));
        const float w = 320.0f, h = size * 1.4f;
        Rectangle<float> r (cx - w * 0.5f, cy - h * 0.5f, w, h);
        if (j == Justification::centredRight) r = { cx - w, cy - h * 0.5f, w, h };
        g.drawText (s, r, j, false);
    }

    /** Text rotated tangentially around a centre (like the BAL / ZERO / BAL legend). */
    void textOnArc (Graphics& g, const String& s, Point<float> c, float r, float deg, float size)
    {
        const auto p = polar (c, r, deg);
        Graphics::ScopedSaveState ss (g);
        g.addTransform (AffineTransform::rotation (degreesToRadians (deg), p.x, p.y));
        text (g, s, p.x, p.y, size);
    }

    // ---- scales -------------------------------------------------------------
    void numberScale (Graphics& g, const char* name, const StringArray& labels, float r, float size, int dotsPerStep,
                      float dotR, float dotSize)
    {
        const auto* c = ctl (name);
        const Point<float> pc (c->x, c->y);
        const float a0 = c->steps[0], a1 = c->steps[c->numSteps - 1];
        const int n = labels.size();
        for (int i = 0; i < n; ++i)
        {
            const float d = a0 + (a1 - a0) * (float) i / (float) (n - 1);
            const auto p = polar (pc, r, d);
            text (g, labels[i], p.x, p.y, size);
        }
        if (dotsPerStep > 0)
        {
            const int total = (n - 1) * dotsPerStep;
            for (int i = 0; i <= total; ++i)
            {
                const float d = a0 + (a1 - a0) * (float) i / (float) total;
                const bool major = i % dotsPerStep == 0;
                const float rr = major ? dotSize : dotSize * 0.7f;
                const auto p = polar (pc, dotR, d);
                g.fillEllipse (p.x - rr, p.y - rr, rr * 2, rr * 2);
            }
        }
    }

    void stepScale (Graphics& g, const char* name, const StringArray& labels, float r, float size)
    {
        const auto* c = ctl (name);
        for (int i = 0; i < labels.size() && i < c->numSteps; ++i)
        {
            const float rr = labels[i].length() > 4 ? r + 12.0f : r;   // long words sit a bit further out
            const auto p = polar ({ c->x, c->y }, rr, c->steps[i]);
            text (g, labels[i], p.x, p.y, size);
        }
    }

    /** Vertical list with a small ring per position (AGC, AGC FILTER). */
    void listScale (Graphics& g, const char* name, const StringArray& labels, float r, float size)
    {
        const auto* c = ctl (name);
        for (int i = 0; i < labels.size() && i < c->numSteps; ++i)
        {
            const auto p = polar ({ c->x, c->y }, r, c->steps[i]);
            g.drawEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f, 1.1f);
            text (g, labels[i], p.x - 6.0f, p.y, size, false, Justification::centredRight);
        }
    }

    // ---- hardware -------------------------------------------------------------
    void screw (Graphics& g, Point<float> p, float r, float slotDeg)
    {
        Path head; head.addEllipse (p.x - r, p.y - r, r * 2, r * 2);
        DropShadow (Colours::black.withAlpha (0.55f), (int) (r * 0.9f), { 1, 2 }).drawForPath (g, head);
        ColourGradient cg (Colour (0xff9c9e9b), p.x - r * 0.6f, p.y - r * 0.7f, Colour (0xff2a2b2a), p.x + r * 0.7f, p.y + r * 0.8f, true);
        g.setGradientFill (cg);
        g.fillPath (head);
        g.setColour (Colours::black.withAlpha (0.8f));
        g.drawEllipse (p.x - r, p.y - r, r * 2, r * 2, 0.8f);
        // cross slot
        for (float d : { slotDeg, slotDeg + 90.0f })
        {
            g.setColour (Colour (0xff141414));
            g.drawLine (Line<float> (polar (p, r * 0.72f, d), polar (p, r * 0.72f, d + 180.0f)), r * 0.28f);
        }
        g.setColour (Colours::white.withAlpha (0.25f));
        g.drawEllipse (p.x - r * 0.75f, p.y - r * 0.85f, r * 1.2f, r * 0.9f, 0.6f);
    }

    void vuHousing (Graphics& g, Rectangle<float> r)
    {
        Path body; body.addRoundedRectangle (r, 15.0f);
        DropShadow (Colours::black.withAlpha (0.75f), 14, { 4, 8 }).drawForPath (g, body);

        // moulded bakelite
        ColourGradient bk (Colour (0xff2a2725), r.getX(), r.getY(), Colour (0xff0a0909), r.getX(), r.getBottom(), false);
        bk.addColour (0.45, Colour (0xff171514));
        g.setGradientFill (bk);
        g.fillPath (body);
        ColourGradient sheen (Colours::white.withAlpha (0.10f), r.getX(), r.getY(), Colours::transparentWhite, r.getX() + r.getWidth() * 0.4f, r.getY() + r.getHeight() * 0.35f, false);
        g.setGradientFill (sheen);
        g.fillPath (body);
        g.setColour (Colours::white.withAlpha (0.16f));
        g.strokePath (body, PathStrokeType (1.2f), AffineTransform::translation (0.4f, 0.6f));
        g.setColour (Colours::black);
        g.strokePath (body, PathStrokeType (1.0f));

        // raised inner frame around the window
        auto frame = Rectangle<float> (r.getX() + 12.0f, r.getY() + 12.0f, r.getWidth() - 24.0f, 132.0f);
        Path fp; fp.addRoundedRectangle (frame, 11.0f);
        g.setColour (Colour (0xff050505));
        g.fillPath (fp);
        g.setColour (Colours::white.withAlpha (0.09f));
        g.strokePath (fp, PathStrokeType (1.2f));

        // lower moulding: ridges and the zero-adjust boss
        const float y0 = r.getY() + 150.0f, y1 = r.getBottom() - 14.0f;
        for (int side = 0; side < 2; ++side)
            for (int i = 0; i < 5; ++i)
            {
                const float x = side == 0 ? r.getX() + 16.0f + (float) i * 8.0f : r.getRight() - 16.0f - (float) i * 8.0f;
                const float top = y0 + (float) i * 6.0f;
                g.setColour (Colours::black.withAlpha (0.8f));
                g.drawLine (x + 0.8f, top, x + 0.8f, y1, 1.6f);
                g.setColour (Colours::white.withAlpha (0.12f));
                g.drawLine (x - 0.6f, top, x - 0.6f, y1, 0.9f);
            }
        auto boss = Rectangle<float> (r.getCentreX() - 17.0f, y0 - 8.0f, 34.0f, y1 - y0 + 8.0f);
        ColourGradient bg (Colour (0xff1c1a19), boss.getX(), 0, Colour (0xff0b0a0a), boss.getRight(), 0, false);
        g.setGradientFill (bg);
        g.fillRoundedRectangle (boss, 4.0f);
        for (int i = 0; i < 6; ++i)
        {
            g.setColour (Colours::white.withAlpha (0.08f));
            g.drawVerticalLine ((int) (boss.getX() + 6.0f + (float) i * 4.5f), boss.getY() + 3.0f, boss.getY() + 18.0f);
        }
        const Point<float> zp (r.getCentreX(), y1 - 22.0f);
        g.setColour (Colour (0xff050505));
        g.fillEllipse (zp.x - 11, zp.y - 11, 22, 22);
        ColourGradient zg (Colour (0xff3a3735), zp.x - 6, zp.y - 7, Colour (0xff0d0c0c), zp.x + 6, zp.y + 8, false);
        g.setGradientFill (zg);
        g.fillEllipse (zp.x - 9, zp.y - 9, 18, 18);
        g.setColour (Colour (0xff020202));
        g.drawLine (zp.x, zp.y - 7, zp.x, zp.y + 7, 2.4f);
    }

    void lampBezel (Graphics& g, Point<float> c)
    {
        Path ring; ring.addEllipse (c.x - 27, c.y - 27, 54, 54);
        DropShadow (Colours::black.withAlpha (0.6f), 8, { 2, 4 }).drawForPath (g, ring);
        ColourGradient cg (Colour (0xffe7e6e1), c.x - 20, c.y - 24, Colour (0xff3e3f3d), c.x + 20, c.y + 26, false);
        cg.addColour (0.5, Colour (0xff9a9b97));
        g.setGradientFill (cg);
        g.fillPath (ring);
        g.setColour (Colour (0xff1a1a1a));
        g.drawEllipse (c.x - 27, c.y - 27, 54, 54, 1.0f);
        g.setColour (Colour (0xff0d0d0d));
        g.fillEllipse (c.x - 19.5f, c.y - 19.5f, 39, 39);
    }
} // namespace

//==============================================================================
PanelArt::PanelArt()
{
    setInterceptsMouseClicks (false, false);
    setBufferedToImage (true);   // cached at the physical pixel scale -> sharp on Retina
}

void PanelArt::paint (Graphics& g)
{
    // the photograph of the panel, AI-upscaled to 2x for Retina
    static const Image photo = []
    {
        int size = 0;
        auto* data = BinaryData::getNamedResource ("panel2x_jpg", size);
        return data != nullptr ? ImageCache::getFromMemory (data, size) : Image();
    }();
    g.fillAll (Colour (0xff0d0f10));
    g.setImageResamplingQuality (Graphics::highResamplingQuality);
    g.drawImage (photo, getLocalBounds().toFloat());
}

//==============================================================================
LampGlow::LampGlow()
{
    setInterceptsMouseClicks (false, false);
    startTimerHz (30);
}

void LampGlow::setOn (bool on) { target = on; }

void LampGlow::timerCallback()
{
    // incandescent warm-up / cool-down (~0.6 s / 0.4 s) with a faint filament flicker
    const float rate = target ? 0.10f : 0.16f;
    level += ((target ? 1.0f : 0.0f) - level) * rate;
    flicker = target ? (rng.nextFloat() - 0.5f) * 0.03f : 0.0f;
    if (onBrightness) onBrightness (level);
    repaint();
}

void LampGlow::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float lv = jlimit (0.0f, 1.0f, level + flicker);
    const float rj = 15.0f;

    // light spilling onto the paint around the bezel
    if (lv > 0.01f)
    {
        ColourGradient spill (Colour (0xffff3a1e).withAlpha (0.20f * lv), c.x, c.y,
                              Colour (0xffff3a1e).withAlpha (0.0f), c.x + 70.0f, c.y, true);
        g.setGradientFill (spill);
        g.fillEllipse (c.x - 70, c.y - 70, 140, 140);
        // hot filament core behind the jewel
        ColourGradient core (Colour (0xffffc6a0).withAlpha (0.30f * lv), c.x, c.y, Colours::transparentWhite, c.x + rj * 0.8f, c.y, true);
        g.setGradientFill (core);
        g.fillEllipse (c.x - rj, c.y - rj, rj * 2, rj * 2);
    }
    // unlit: the jewel goes dark (the photo shows it lit)
    if (lv < 0.99f)
    {
        ColourGradient off (Colour (0xff2a0606).withAlpha (0.88f * (1.0f - lv)), c.x, c.y,
                            Colour (0xff0e0202).withAlpha (0.80f * (1.0f - lv)), c.x + rj, c.y + rj, true);
        g.setGradientFill (off);
        g.fillEllipse (c.x - rj, c.y - rj, rj * 2, rj * 2);
        g.setColour (Colours::white.withAlpha (0.16f * (1.0f - lv)));
        g.fillEllipse (c.x - rj * 0.5f, c.y - rj * 0.6f, rj * 0.45f, rj * 0.3f);
    }
}
} // namespace sa670::ui
