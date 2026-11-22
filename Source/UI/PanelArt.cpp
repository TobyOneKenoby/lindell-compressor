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
    const auto b = getLocalBounds().toFloat();

    // ---- panel shape with rack-ear slots ----
    Path panel;
    panel.addRoundedRectangle (b.reduced (1.5f), 5.0f);
    for (float y : { 120.0f, 440.0f, 765.0f, 1080.0f })
    {
        Path l, r;
        l.addRoundedRectangle (-20.0f, y - 9.0f, 62.0f, 18.0f, 9.0f);
        r.addRoundedRectangle (b.getWidth() - 42.0f, y - 9.0f, 62.0f, 18.0f, 9.0f);
        panel.addPath (l);
        panel.addPath (r);
    }
    panel.setUsingNonZeroWinding (false);

    g.fillAll (Colour (0xff0d0f10));
    {
        Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (panel);

        ColourGradient paint (paintTop, 0, 0, paintBottom, 0, b.getHeight(), false);
        paint.addColour (0.45, paintMid);
        g.setGradientFill (paint);
        g.fillRect (b);

        // powder-coat grain: a fine noise tile at 1/3 unit per texel (crisp up to 3x)
        static const Image grain = []
        {
            Image img (Image::ARGB, 384, 384, true);
            Random rng (670);
            Image::BitmapData bd (img, Image::BitmapData::writeOnly);
            for (int y = 0; y < img.getHeight(); ++y)
                for (int x = 0; x < img.getWidth(); ++x)
                {
                    const float v = rng.nextFloat();
                    const bool light = v > 0.5f;
                    const uint8 a = (uint8) (std::pow (std::abs (v - 0.5f) * 2.0f, 2.2f) * 34.0f);
                    bd.setPixelColour (x, y, Colour (light ? 0xffffffffu : 0xff000000u).withAlpha (a));
                }
            return img;
        }();
        g.setFillType (FillType (grain, AffineTransform::scale (1.0f / 3.0f)));
        g.fillRect (b);
        // second, coarser layer: the orange-peel of real powder coat
        g.setFillType (FillType (grain, AffineTransform::scale (0.9f).rotated (0.7f)));
        g.setOpacity (0.35f);
        g.fillRect (b);
        g.setOpacity (1.0f);

        // studio lighting: soft hot-spot upper centre, falloff to the corners
        ColourGradient light (Colours::white.withAlpha (0.06f), b.getCentreX(), b.getHeight() * 0.28f,
                              Colours::black.withAlpha (0.32f), 0.0f, b.getHeight() * 1.05f, true);
        g.setGradientFill (light);
        g.fillRect (b);

        // section split between EQ and compressor
        g.setColour (Colours::black.withAlpha (0.38f));
        g.drawLine (44, 452.5f, b.getWidth() - 44, 452.5f, 1.4f);
        g.setColour (Colours::white.withAlpha (0.07f));
        g.drawLine (44, 454.2f, b.getWidth() - 44, 454.2f, 1.0f);
    }
    // bevelled edge
    g.setColour (Colours::white.withAlpha (0.14f));
    g.strokePath (panel, PathStrokeType (1.3f), AffineTransform::translation (0.5f, 0.6f));
    g.setColour (Colours::black.withAlpha (0.75f));
    g.strokePath (panel, PathStrokeType (1.0f));

    // ---- panel screws ----
    const Point<float> screws[] = { { 88, 162 }, { 1222, 162 }, { 88, 460 }, { 1222, 460 }, { 88, 740 },
                                    { 1222, 740 }, { 88, 1030 }, { 1222, 1030 }, { 655, 1172 } };
    int k = 0;
    for (auto s : screws) screw (g, s, 7.5f, 18.0f + 37.0f * (float) (k++));

    // ---- hardware drawn on the panel ----
    vuHousing (g, { 172.0f, 489.0f, 206.0f, 216.0f });
    vuHousing (g, { 172.0f, 759.0f, 206.0f, 216.0f });
    lampBezel (g, { 133.0f, 738.0f });

    // ---- legends (screen print) ----
    g.setColour (legend);
    const StringArray zeroTen { "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10" };

    // EQ row 1
    text (g, "BOOST", 216, 47, 14); text (g, "ATTEN", 397, 47, 14); text (g, "ATTEN/BOOST", 588, 47, 14);
    text (g, "BOOST", 776, 47, 14); text (g, "ATTEN", 965, 47, 14); text (g, "ATTEN SEL", 1126, 47, 14);
    for (auto* n : { "lfBoost", "lfAtten", "midAmt", "hfBoost", "hfAtten", "bandwidth" })
        numberScale (g, n, zeroTen, 83.0f, 14.0f, 2, 72.0f, 1.9f);
    stepScale (g, "attenSel", { "5", "10", "20" }, 56.0f, 13.5f);
    text (g, "IN", 118, 193, 13.5f); text (g, "TUBE", 1171, 182, 13.5f); text (g, "SS", 1171, 259, 13.5f);

    // EQ row 2
    text (g, "CPS", 172, 259, 13.5f); text (g, "CPS", 346, 259, 13.5f); text (g, "KCS", 928, 260, 13.5f); text (g, "KCS", 1110, 260, 13.5f);
    stepScale (g, "lowCut", { "OFF", "50", "80", "100", "150", "250", "500", "750", "1000", "1500", "2000" }, 63.0f, 13.0f);
    stepScale (g, "lfFreq", { "20", "30", "60", "100" }, 64.0f, 13.5f);
    stepScale (g, "midFreq", { "200", "300", "500", "700", "1", "1.5", "2", "3", "4", "5", "7" }, 62.0f, 13.0f);
    stepScale (g, "hfFreq", { "3", "4", "5", "8", "10", "12", "16" }, 63.0f, 13.5f);
    stepScale (g, "highCut", { "1.5", "2", "3", "4", "5", "6", "8", "10", "12", "15", "OFF" }, 64.0f, 13.0f);
    text (g, "BOOST", 627, 315, 13.5f); text (g, "ATTEN", 627, 381, 13.5f);
    text (g, "SHARP", 670, 416, 12.0f); text (g, "BROAD", 849, 416, 12.0f);
    text (g, "LOW CUT-OFF", 173, 445, 14.5f); text (g, "LOW FREQUENCY", 338, 444, 14.5f); text (g, "MID", 513, 444, 14.5f);
    text (g, "BANDWIDTH", 758, 444, 14.5f); text (g, "HIGH FREQUENCY", 928, 444, 14.5f); text (g, "HIGH CUT-OFF", 1112, 445, 14.5f);

    // compressor
    const float titleY[] = { 486.0f, 745.0f };
    const char* side[] = { "LEFT - LAT", "RIGHT - VERT" };
    for (int c = 0; c < 2; ++c)
    {
        const float ty = titleY[c];
        text (g, side[c], 486, ty, 12.5f); text (g, "METERING", 486, ty + 14, 12.5f);
        text (g, c == 0 ? "LEFT CHANNEL" : "RIGHT CHANNEL", 700, ty, 12.5f); text (g, "INPUT GAIN", 700, ty + 14, 12.5f);
        text (g, side[c], 928, ty, 12.5f); text (g, "THRESHOLD", 928, ty + 14, 12.5f);

        const auto* m = ctl (c == 0 ? "meter1" : "meter2");
        textOnArc (g, "BAL", { m->x, m->y }, 80.0f, m->steps[0] - 3.0f, 12.0f);
        textOnArc (g, "ZERO", { m->x, m->y }, 82.0f, m->steps[1], 12.0f);
        textOnArc (g, "BAL", { m->x, m->y }, 80.0f, m->steps[2] + 3.0f, 12.0f);
        const float sy = c == 0 ? 670.0f : 940.0f;
        text (g, "ZERO", 428, sy, 12.0f); text (g, "BAL", 547, sy, 12.0f);

        numberScale (g, c == 0 ? "inGain1" : "inGain2", { "0", "2", "4", "6", "8", "10", "12", "14", "16", "18", "20" }, 83.0f, 12.0f, 2, 72.0f, 1.8f);
        numberScale (g, c == 0 ? "thresh1" : "thresh2", zeroTen, 83.0f, 12.5f, 2, 72.0f, 1.8f);

        const char* tc = c == 0 ? "tc1" : "tc2";
        const float tcTitle = c == 0 ? 486.0f : 828.0f;
        text (g, side[c], 1138, tcTitle, 12.5f); text (g, "TIME CONSTANT", 1138, tcTitle + 14, 12.5f);
        stepScale (g, tc, { "1", "2", "3", "4", "5", "6", "MANUAL" }, 44.0f, 12.0f);
    }
    text (g, "ON", 132, 577, 13.5f);
    text (g, "LINK", 130, 848, 13.5f);
    text (g, "AGC", 1138, 663, 13.5f);
    listScale (g, "agcMode", { "INDEPENDENT", "LINK", "MID-SIDE", "MID-SIDE LINK" }, 48.0f, 11.0f);

    // bottom row
    text (g, "AGC FILTER", 230, 1006, 13.5f);
    text (g, "LEFT - LAT", 169, 1025, 12.5f); text (g, "RIGHT - VERT", 300, 1025, 12.5f);
    listScale (g, "scFilter1", { "OFF", "90Hz", "180Hz", "MID" }, 47.0f, 11.0f);
    listScale (g, "scFilter2", { "OFF", "90Hz", "180Hz", "MID" }, 47.0f, 11.0f);
    text (g, "DC THRESHOLD", 490, 1006, 13.5f);
    text (g, "LEFT - LAT", 416, 1025, 12.5f); text (g, "RIGHT - VERT", 563, 1025, 12.5f);
    text (g, "DRY/WET", 830, 1006, 13.5f); text (g, "LEFT", 757, 1025, 12.5f); text (g, "RIGHT", 900, 1025, 12.5f);
    text (g, "OUTPUT", 1104, 1006, 13.5f); text (g, "LEFT", 1027, 1025, 12.5f); text (g, "RIGHT", 1173, 1025, 12.5f);
    for (auto* n : { "dcThr1", "dcThr2", "mix1", "mix2", "output1", "output2" })
        numberScale (g, n, zeroTen, 47.0f, 10.5f, 0, 0, 0);
    text (g, "IN", 658, 1048, 13.5f); text (g, "BYPASS", 657, 1138, 13.5f);

    g.setColour (legend);
    g.setFont (panelFont (20.0f).withExtraKerningFactor (0.08f));
    g.drawText ("MODEL SA-670", Rectangle<float> (300, 28).withCentre ({ 880, 1172 }), Justification::centred);
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

    // light spilling onto the paint around the bezel
    if (lv > 0.01f)
    {
        ColourGradient spill (Colour (0xffff3a1e).withAlpha (0.22f * lv), c.x, c.y,
                              Colour (0xffff3a1e).withAlpha (0.0f), c.x + 70.0f, c.y, true);
        g.setGradientFill (spill);
        g.fillEllipse (c.x - 70, c.y - 70, 140, 140);
    }

    // faceted jewel
    const float rj = 17.5f;
    Rectangle<float> jewel (c.x - rj, c.y - rj, rj * 2, rj * 2);
    ColourGradient jg (Colour (0xff5a1410).interpolatedWith (Colour (0xffff8a70), lv), c.x - rj * 0.3f, c.y - rj * 0.4f,
                       Colour (0xff1e0403).interpolatedWith (Colour (0xffa30a06), lv), c.x + rj, c.y + rj, true);
    g.setGradientFill (jg);
    g.fillEllipse (jewel);
    g.setColour (Colours::black.withAlpha (0.28f));
    for (int i = 0; i < 6; ++i)
        g.drawLine (Line<float> (polar (c, rj * 0.25f, 60.0f * (float) i), polar (c, rj, 60.0f * (float) i + 30.0f)), 0.7f);
    g.drawEllipse (jewel.reduced (rj * 0.5f), 0.7f);
    if (lv > 0.01f)
    {
        ColourGradient core (Colour (0xffffd0b0).withAlpha (0.55f * lv), c.x, c.y, Colours::transparentWhite, c.x + rj * 0.7f, c.y, true);
        g.setGradientFill (core);
        g.fillEllipse (jewel.reduced (rj * 0.25f));
    }
    g.setColour (Colours::white.withAlpha (0.25f + 0.35f * lv));
    g.fillEllipse (c.x - rj * 0.5f, c.y - rj * 0.6f, rj * 0.45f, rj * 0.32f);
}
} // namespace sa670::ui
