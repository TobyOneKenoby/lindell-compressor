#include "Sprites.h"
#include "BinaryData.h"

namespace sa670::ui
{
using namespace juce;

static Font uiFont (float h) { return Font (FontOptions (h, Font::bold)).withExtraKerningFactor (0.06f); }
static const Colour cream (0xffe6dfcf);

void paintPowderCoat (Graphics& g, Rectangle<float> r, Colour top, Colour bottom, int seed, float corner)
{
    Path shape;
    shape.addRoundedRectangle (r, corner);
    g.saveState();
    g.reduceClipRegion (shape);

    ColourGradient base (top, r.getX(), r.getY(), bottom, r.getX(), r.getBottom(), false);
    base.addColour (0.5, top.interpolatedWith (bottom, 0.4f));
    g.setGradientFill (base);
    g.fillRect (r);

    // powder-coat grain (deterministic)
    Random rng (seed);
    const int dots = (int) (r.getWidth() * r.getHeight() * 0.045f);
    for (int i = 0; i < dots; ++i)
    {
        g.setColour ((rng.nextBool() ? Colours::white : Colours::black).withAlpha (0.03f + 0.025f * rng.nextFloat()));
        g.fillRect (r.getX() + rng.nextFloat() * r.getWidth(), r.getY() + rng.nextFloat() * r.getHeight(), 1.2f, 1.2f);
    }
    // soft studio light: brighter towards the top-centre, darker corners
    ColourGradient vg (Colours::white.withAlpha (0.04f), r.getCentreX(), r.getY(),
                       Colours::black.withAlpha (0.30f), r.getX(), r.getBottom(), true);
    g.setGradientFill (vg);
    g.fillRect (r);
    g.restoreState();

    // bevel
    g.setColour (Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (r.reduced (0.75f).translated (0.5f, 0.5f), corner, 1.2f);
    g.setColour (Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
}

Image loadAsset (const String& resourceName)
{
    static std::map<String, Image> cache;
    if (auto it = cache.find (resourceName); it != cache.end())
        return it->second;
    int size = 0;
    Image img;
    if (auto* data = BinaryData::getNamedResource (resourceName.toRawUTF8(), size))
        img = ImageCache::getFromMemory (data, size);
    jassert (img.isValid());
    cache[resourceName] = img;
    return img;
}

static String resourceNameFor (const String& file)
{
    // juce_add_binary_data turns "lfBoost_sh.png" into "lfBoost_sh_png"
    return file.replaceCharacter ('.', '_').replaceCharacter ('-', '_');
}

//==============================================================================
void ValueBubble::show (Component& source, const String& t)
{
    if (! enabled || getParentComponent() == nullptr) { setVisible (false); return; }
    text = t;
    const int w = (int) GlyphArrangement::getStringWidth (uiFont (13.0f), text) + 22;
    auto r = getParentComponent()->getLocalArea (&source, source.getLocalBounds());
    setBounds (r.getCentreX() - w / 2, r.getY() - 6, w, 24);
    setVisible (true);
    toFront (false);
    repaint();
}

void ValueBubble::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (Colours::black.withAlpha (0.78f));
    g.fillRoundedRectangle (b, 5.0f);
    g.setColour (cream.withAlpha (0.35f));
    g.drawRoundedRectangle (b, 5.0f, 1.0f);
    g.setColour (cream);
    g.setFont (uiFont (13.0f));
    g.drawText (text, b, Justification::centred);
}

//==============================================================================
SpriteControl::SpriteControl (AudioProcessorValueTreeState& s, const layout::Control& c, const String& paramId,
                              ValueBubble* vb, float sc)
    : info (c), param (*s.getParameter (paramId)), scale (sc), bubble (vb),
      attachment (param, [this] (float v)
                  {
                      value01 = param.convertTo0to1 (v);
                      animateTo();
                      if (onValueChange) onValueChange (v);
                  }, nullptr)
{
    sprite = loadAsset (resourceNameFor (String (c.name) + ".png"));
    shadow = loadAsset (resourceNameFor (String (c.name) + "_sh.png"));

    if (auto* ch = dynamic_cast<AudioParameterChoice*> (&param)) numChoices = ch->choices.size();
    if (dynamic_cast<AudioParameterBool*> (&param) != nullptr) isToggle = true;

    const int side = (int) std::ceil ((float) c.size * scale * 1.45f);
    setSize (side, side);
    setTooltip (param.getName (64));
    setMouseCursor (MouseCursor::PointingHandCursor);
    attachment.sendInitialUpdate();
}

void SpriteControl::animateTo()
{
    const float targetAngle = currentAngle();
    const float targetFlip = (info.kind == layout::Kind::Toggle && value01 < 0.5f) ? -1.0f : 1.0f;
    if (! initialised || (dragging && numChoices == 0))
    {
        // first paint, or a continuous drag: follow the hand directly
        shownAngle = targetAngle; shownFlip = targetFlip; initialised = true;
        repaint();
        return;
    }
    if (std::abs (shownAngle - targetAngle) > 0.05f || std::abs (shownFlip - targetFlip) > 0.01f)
        startTimerHz (120);
}

void SpriteControl::timerCallback()
{
    const float targetAngle = currentAngle();
    const float targetFlip = (info.kind == layout::Kind::Toggle && value01 < 0.5f) ? -1.0f : 1.0f;
    // knobs glide (~35 ms), levers swing over centre (~70 ms)
    shownAngle += (targetAngle - shownAngle) * 0.32f;
    const float df = targetFlip - shownFlip;
    shownFlip += jlimit (-0.25f, 0.25f, df);
    if (std::abs (targetAngle - shownAngle) < 0.05f && std::abs (targetFlip - shownFlip) < 0.01f)
    {
        shownAngle = targetAngle; shownFlip = targetFlip;
        stopTimer();
    }
    repaint();
}

void SpriteControl::placeAt (float x, float y)
{
    setBounds (Rectangle<int> (getWidth(), getHeight()).withCentre ({ (int) std::round (x), (int) std::round (y) }));
}

float SpriteControl::currentAngle() const
{
    if (info.numSteps == 0) return info.baked;
    if (numChoices > 0 && info.numSteps >= numChoices)
    {
        const int i = jlimit (0, numChoices - 1, (int) std::lround (value01 * (float) (numChoices - 1)));
        return info.steps[i];
    }
    return info.steps[0] + value01 * (info.steps[info.numSteps - 1] - info.steps[0]);
}

void SpriteControl::paint (Graphics& g)
{
    const auto c = getLocalBounds().toFloat().getCentre();
    const float S = (float) info.size;
    const float shadowOffX = 5.0f * scale, shadowOffY = 9.0f * scale;

    auto spriteTransform = [&] (float rotationDeg, float flipV, float pad)
    {
        auto t = AffineTransform::translation (-(S * 0.5f + pad), -(S * 0.5f + pad));
        // lever swinging through centre: foreshortened, never quite zero height
        if (flipV != 1.0f) t = t.scaled (1.0f, std::copysign (jmax (0.12f, std::abs (flipV)), flipV));
        t = t.rotated (degreesToRadians (rotationDeg)).scaled (scale).translated (c);
        return t;
    };

    float rot = 0.0f;
    float flip = 1.0f;
    switch (info.kind)
    {
        case layout::Kind::Chicken: rot = shownAngle - info.baked; break;
        case layout::Kind::Toggle:  flip = shownFlip; break;
        case layout::Kind::Screw:   rot = (value01 - 0.5f) * 270.0f; break;
        case layout::Kind::Fluted:  break;
    }

    if (info.kind == layout::Kind::Fluted || info.kind == layout::Kind::Chicken)
    {
        // contact shadow right at the base (knob sits on the panel)
        const float rr = (info.kind == layout::Kind::Fluted ? 0.47f : 0.30f) * S * scale;
        ColourGradient ao (Colours::black.withAlpha (0.45f), c.x + 1.5f * scale, c.y + 3.0f * scale,
                           Colours::transparentBlack, c.x + 1.5f * scale + rr * 1.18f, c.y + 3.0f * scale, true);
        g.setGradientFill (ao);
        g.fillEllipse (c.x + 1.5f * scale - rr * 1.18f, c.y + 3.0f * scale - rr * 1.18f, rr * 2.36f, rr * 2.36f);
    }
    if (info.kind != layout::Kind::Screw)
    {
        g.setOpacity (info.kind == layout::Kind::Toggle ? 0.55f : 0.75f);
        g.drawImageTransformed (shadow, spriteTransform (rot, flip, 16.0f).translated (shadowOffX, shadowOffY));
        g.setOpacity (1.0f);
    }
    g.drawImageTransformed (sprite, spriteTransform (rot, flip, 0.0f));

    if (info.kind == layout::Kind::Fluted)
    {
        // index line on the skirt, like the original white engraving
        const float R = S * 0.5f * scale - 6.0f * scale;
        const float a = degreesToRadians (shownAngle);
        const Point<float> dir (std::sin (a), -std::cos (a));
        const Line<float> line (c + dir * (R * 0.70f), c + dir * (R * 0.93f));
        g.setColour (Colours::black.withAlpha (0.55f));
        g.drawLine (line.withShortenedStart (-0.5f), 5.2f * scale);
        g.setColour (Colour (0xffe9dfc4));
        g.drawLine (line, 3.4f * scale);
        g.setColour (Colours::white.withAlpha (0.35f));
        g.drawLine (Line<float> (line.getStart() + Point<float> (-0.6f, -0.6f), line.getEnd() + Point<float> (-0.6f, -0.6f)), 1.0f * scale);
    }
}

void SpriteControl::setNormalised (float v01)
{
    v01 = jlimit (0.0f, 1.0f, v01);
    attachment.setValueAsPartOfGesture (param.convertFrom0to1 (v01));
}

void SpriteControl::showValue()
{
    if (bubble != nullptr)
        bubble->show (*this, param.getName (32) + "  " + param.getCurrentValueAsText() + (param.getLabel().isNotEmpty() ? " " + param.getLabel() : String()));
}

void SpriteControl::mouseEnter (const MouseEvent&) { showValue(); }
void SpriteControl::mouseExit (const MouseEvent&) { if (bubble != nullptr && ! dragging) bubble->hide(); }

void SpriteControl::mouseDown (const MouseEvent&)
{
    dragging = true; moved = false;
    dragStart01 = value01;
    if (! isToggle) attachment.beginGesture();
}

void SpriteControl::mouseDrag (const MouseEvent& e)
{
    if (isToggle) return;
    const float dist = (float) (e.getDistanceFromDragStartX() - e.getDistanceFromDragStartY());
    if (std::abs (dist) > 2.0f) moved = true;
    if (numChoices > 0)
    {
        const int start = (int) std::lround (dragStart01 * (float) (numChoices - 1));
        const int idx = jlimit (0, numChoices - 1, start + (int) (dist / 16.0f));
        setNormalised ((float) idx / (float) (numChoices - 1));
    }
    else
    {
        const float sens = e.mods.isShiftDown() ? 1200.0f : 240.0f;
        setNormalised (dragStart01 + dist / sens);
    }
    showValue();
}

void SpriteControl::mouseUp (const MouseEvent&)
{
    dragging = false;
    if (isToggle)
        attachment.setValueAsCompleteGesture (value01 > 0.5f ? 0.0f : 1.0f);
    else
    {
        if (! moved && numChoices > 0)
        {
            const int idx = (int) std::lround (value01 * (float) (numChoices - 1));
            setNormalised ((float) ((idx + 1) % numChoices) / (float) (numChoices - 1));
        }
        attachment.endGesture();
    }
    showValue();
}

void SpriteControl::mouseDoubleClick (const MouseEvent&)
{
    if (isToggle) return;
    attachment.setValueAsCompleteGesture (param.convertFrom0to1 (param.getDefaultValue()));
}

void SpriteControl::mouseWheelMove (const MouseEvent&, const MouseWheelDetails& w)
{
    if (isToggle) return;
    attachment.beginGesture();
    if (numChoices > 0)
    {
        wheelAccum += w.deltaY;
        if (std::abs (wheelAccum) > 0.12f)
        {
            const int idx = (int) std::lround (value01 * (float) (numChoices - 1)) + (wheelAccum > 0 ? 1 : -1);
            setNormalised ((float) jlimit (0, numChoices - 1, idx) / (float) (numChoices - 1));
            wheelAccum = 0;
        }
    }
    else
        setNormalised (value01 + w.deltaY * 0.08f);
    attachment.endGesture();
    showValue();
}

//==============================================================================
// VU meter face: drawn as vectors (crisp at any size) into the window of the
// bakelite housing from the artwork. Classic VU scale: -20..+3 VU, linear in
// voltage, red zone above 0 VU, percentage scale underneath.
namespace
{
    // window outline measured from the artwork (meter 1; meter 2 is 270 px lower)
    constexpr float facePoly[][2] = { { 349, 560 }, { 354, 577 }, { 354, 617 }, { 353, 622 }, { 348, 626 }, { 205, 625 },
                                      { 201, 623 }, { 197, 618 }, { 196, 615 }, { 196, 582 }, { 197, 572 }, { 200, 561 },
                                      { 213, 541 }, { 221, 534 }, { 240, 524 }, { 261, 519 }, { 288, 519 }, { 302, 522 },
                                      { 316, 527 }, { 329, 535 }, { 342, 548 } };
    const Rectangle<float> meterArea (190.0f, 512.0f, 172.0f, 122.0f);   // component bounds (meter 1)
    constexpr float pivotX = 275.0f, pivotY = 668.0f, arcR = 102.0f, halfSpan = 36.0f;
    inline float dbToFrac (float db) { return (std::pow (10.0f, db / 20.0f) - 0.1f) / (std::pow (10.0f, 0.15f) - 0.1f); }
    inline float fracToDb (float f) { return 20.0f * std::log10 (jmax (1.0e-4f, 0.1f + f * (std::pow (10.0f, 0.15f) - 0.1f))); }
    inline float fracToDeg (float f) { return -halfSpan + f * 2.0f * halfSpan; }
    inline Point<float> polarP (Point<float> c, float r, float deg)
    {
        const float a = degreesToRadians (deg);
        return { c.x + r * std::sin (a), c.y - r * std::cos (a) };
    }
}

VUNeedle::VUNeedle (std::function<float()> s, float yOffset) : source (std::move (s)), dy (yOffset)
{
    setInterceptsMouseClicks (false, false);
    setBounds (meterArea.translated (0.0f, dy).toNearestInt());
    startTimerHz (60);
}

Point<float> VUNeedle::scalePoint (float db) const
{
    return polarP ({ pivotX, pivotY }, arcR, fracToDeg (dbToFrac (db)));
}

void VUNeedle::timerCallback()
{
    // Moving-coil movement: damped 2nd-order system tuned to the VU standard
    // (99 % of a step in 300 ms, ~1-1.5 % overshoot), integrated in "voltage" space.
    const float target = jlimit (-0.05f, 1.08f, dbToFrac (source()));
    float frac = dbToFrac (needleDb);
    constexpr float w = 13.5f, zeta = 0.80f;
    constexpr int sub = 4;
    const float dt = 1.0f / (60.0f * (float) sub);
    for (int k = 0; k < sub; ++k)
    {
        const float acc = w * w * (target - frac) - 2.0f * zeta * w * velocity;
        velocity += acc * dt;
        frac += velocity * dt;
        if (frac < -0.05f) { frac = -0.05f; velocity = -velocity * 0.2f; }   // end stops
        if (frac > 1.09f)  { frac = 1.09f;  velocity = -velocity * 0.2f; }
    }
    needleDb = fracToDb (frac);
    repaint();
}

void VUNeedle::paint (Graphics& g)
{
    // work in panel coordinates of meter 1
    g.addTransform (AffineTransform::translation (-meterArea.getX(), -meterArea.getY()));
    const Point<float> pv (pivotX, pivotY);

    Path face;
    for (size_t i = 0; i < std::size (facePoly); ++i)
        if (i == 0) face.startNewSubPath (facePoly[i][0], facePoly[i][1]); else face.lineTo (facePoly[i][0], facePoly[i][1]);
    face.closeSubPath();
    const auto fb = face.getBounds();

    g.saveState();
    g.reduceClipRegion (face);

    // card lit from behind by two incandescent lamps: warm centre, darker amber edges
    // backlight warms up / cools down with the unit (unlit card is a dull ivory)
    const float bl = backlight;
    auto lit = [bl] (uint32 on, uint32 off) { return Colour (off).interpolatedWith (Colour (on), bl); };
    ColourGradient card (lit (0xfffcecc4, 0xff8f8672), fb.getCentreX(), fb.getY() + fb.getHeight() * 0.62f,
                         lit (0xffc98c43, 0xff5c5446), fb.getX() - 6.0f, fb.getBottom(), true);
    card.addColour (0.55, lit (0xfff1d08e, 0xff7d7563));
    g.setGradientFill (card);
    g.fillRect (fb);

    const Colour ink (0xff1d1712), red (0xffc4261b);
    const float f0 = dbToFrac (0.0f);

    // main arc: black to 0 VU, heavy red band above
    Path arcBlack, arcRed;
    arcBlack.addCentredArc (pv.x, pv.y, arcR, arcR, 0, degreesToRadians (fracToDeg (0)), degreesToRadians (fracToDeg (f0)), true);
    arcRed.addCentredArc (pv.x, pv.y, arcR + 2.0f, arcR + 2.0f, 0, degreesToRadians (fracToDeg (f0)), degreesToRadians (fracToDeg (1)), true);
    g.setColour (ink);
    g.strokePath (arcBlack, PathStrokeType (1.1f));
    g.setColour (red);
    g.strokePath (arcRed, PathStrokeType (4.0f));

    // ticks + numerals
    struct Mark { float db; const char* txt; bool major; };
    const Mark marks[] = { { -20, "20", true }, { -15, "", false }, { -10, "10", true }, { -7, "7", true }, { -6, "", false },
                           { -5, "5", true }, { -4, "", false }, { -3, "3", true }, { -2, "2", true }, { -1, "1", true },
                           { -0.5f, "", false }, { 0, "0", true }, { 0.5f, "", false }, { 1, "1", true }, { 1.5f, "", false },
                           { 2, "2", true }, { 2.5f, "", false }, { 3, "3", true } };
    for (auto& m : marks)
    {
        const float d = fracToDeg (dbToFrac (m.db));
        const bool isRed = m.db > 0.0f;
        g.setColour (isRed ? red : ink);
        const float len = m.major ? 7.0f : 4.0f;
        g.drawLine (Line<float> (polarP (pv, arcR, d), polarP (pv, arcR + len, d)), m.db == 0.0f ? 1.6f : 1.0f);
        if (*m.txt)
        {
            g.setFont (Font (FontOptions (std::abs (m.db) >= 10 ? 8.2f : 9.0f, Font::bold)));
            g.drawText (String (m.txt), Rectangle<float> (16, 10).withCentre (polarP (pv, arcR + 14.0f, d)), Justification::centred);
        }
    }
    // + / - signs at the ends
    g.setFont (Font (FontOptions (10.0f, Font::bold)));
    g.setColour (ink);
    g.drawText ("-", Rectangle<float> (10, 10).withCentre (polarP (pv, arcR + 14.0f, fracToDeg (0) - 7.0f)), Justification::centred);
    g.setColour (red);
    g.drawText ("+", Rectangle<float> (10, 10).withCentre (polarP (pv, arcR + 14.0f, fracToDeg (1) + 7.0f)), Justification::centred);

    // percentage scale (inside the arc)
    g.setColour (ink.withAlpha (0.85f));
    for (int pc = 0; pc <= 100; pc += 10)
    {
        const float fr = ((float) pc / 100.0f - 0.1f) / (std::pow (10.0f, 0.15f) - 0.1f);
        if (fr < 0.0f) continue;
        const float d = fracToDeg (fr);
        g.drawLine (Line<float> (polarP (pv, arcR - 4.0f, d), polarP (pv, arcR - 1.0f, d)), 0.8f);
        if (pc % 20 == 0 && pc > 0)
        {
            g.setFont (Font (FontOptions (6.8f, Font::plain)));
            g.drawText (String (pc), Rectangle<float> (16, 8).withCentre (polarP (pv, arcR - 10.0f, d)), Justification::centred);
        }
    }

    // legend
    g.setFont (Font (FontOptions (15.0f, Font::bold)).withExtraKerningFactor (0.06f));
    g.setColour (ink);
    g.drawText ("VU", Rectangle<float> (40, 16).withCentre ({ pv.x, pv.y - arcR * 0.66f }), Justification::centred);

    // needle (with soft shadow on the card)
    const float nd = fracToDeg (dbToFrac (needleDb));
    const Point<float> tip = polarP (pv, arcR + 9.0f, nd);
    const Point<float> base = polarP (pv, 44.0f, nd);
    g.setColour (Colours::black.withAlpha (0.20f));
    g.drawLine (Line<float> (base.translated (2.2f, 2.6f), tip.translated (2.2f, 2.6f)), 1.6f);
    g.setColour (Colour (0xff121010));
    g.drawLine (Line<float> (base, tip), 1.25f);

    // pivot cover (black dome) and inner shadow of the bezel
    ColourGradient dome (Colour (0xff3a3632), pv.x, fb.getBottom() - 14.0f, Colour (0xff070707), pv.x, fb.getBottom() + 4.0f, false);
    g.setGradientFill (dome);
    g.fillEllipse (pv.x - 26.0f, fb.getBottom() - 11.0f, 52.0f, 26.0f);
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawEllipse (pv.x - 26.0f, fb.getBottom() - 11.0f, 52.0f, 26.0f, 0.8f);

    ColourGradient topShade (Colours::black.withAlpha (0.28f), fb.getCentreX(), fb.getY(), Colours::transparentBlack, fb.getCentreX(), fb.getY() + 14.0f, false);
    g.setGradientFill (topShade);
    g.fillRect (fb.withHeight (14.0f));

    // glass reflection
    Path glare;
    glare.addEllipse (fb.getX() - fb.getWidth() * 0.15f, fb.getY() - fb.getHeight() * 0.65f, fb.getWidth() * 0.9f, fb.getHeight() * 1.05f);
    g.setColour (Colours::white.withAlpha (0.07f));
    g.fillPath (glare);
    g.restoreState();

    g.setColour (Colours::black.withAlpha (0.65f));
    g.strokePath (face, PathStrokeType (1.2f));
}

//==============================================================================
void LampDimmer::paint (Graphics& g)
{
    if (lit) return;
    auto r = getLocalBounds().toFloat().reduced (getWidth() * 0.12f);
    ColourGradient cg (Colour (0xff3a0808).withAlpha (0.93f), r.getCentreX(), r.getCentreY(),
                       Colour (0xff100202).withAlpha (0.9f), r.getRight(), r.getBottom(), true);
    g.setGradientFill (cg);
    g.fillEllipse (r);
    g.setColour (Colours::white.withAlpha (0.18f));
    g.fillEllipse (r.getX() + r.getWidth() * 0.22f, r.getY() + r.getHeight() * 0.18f, r.getWidth() * 0.25f, r.getHeight() * 0.18f);
}

//==============================================================================
void LitButton::paint (Graphics& g)
{
    auto b = getLocalBounds().toFloat().reduced (2.0f);
    // bezel
    ColourGradient bz (Colour (0xff4a4a4a), b.getX(), b.getY(), Colour (0xff0c0c0c), b.getX(), b.getBottom(), false);
    g.setGradientFill (bz);
    g.fillRoundedRectangle (b, 4.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (b, 4.0f, 1.0f);

    auto face = b.reduced (5.0f).translated (0.0f, pressed ? 1.0f : 0.0f);
    if (lit)
    {
        g.setColour (Colour (0xffffc977).withAlpha (0.25f));
        g.fillRoundedRectangle (face.expanded (3.0f), 4.0f);
    }
    ColourGradient fg (lit ? Colour (0xfffff1d2) : Colour (0xffb9b0a0), face.getCentreX(), face.getCentreY(),
                       lit ? Colour (0xffe6a65a) : Colour (0xff6d665b), face.getX(), face.getBottom(), true);
    g.setGradientFill (fg);
    g.fillRoundedRectangle (face, 2.5f);
    g.setColour (Colours::white.withAlpha (0.3f));
    g.drawLine (face.getX() + 2, face.getY() + 1, face.getRight() - 2, face.getY() + 1, 1.0f);

    if (icon != Icon::None)
    {
        Path tri;
        const auto cc = face.getCentre();
        const float s = face.getHeight() * 0.28f;
        if (icon == Icon::Left) tri.addTriangle (cc.x - s, cc.y, cc.x + s * 0.8f, cc.y - s, cc.x + s * 0.8f, cc.y + s);
        else                    tri.addTriangle (cc.x + s, cc.y, cc.x - s * 0.8f, cc.y - s, cc.x - s * 0.8f, cc.y + s);
        g.setColour (Colour (0xff1f5a52));
        g.fillPath (tri);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.strokePath (tri, PathStrokeType (1.0f));
    }
}

ParamButton::ParamButton (AudioProcessorValueTreeState& s, const String& id)
    : param (*s.getParameter (id)),
      attachment (param, [this] (float v) { setLit (v > 0.5f); }, nullptr)
{
    attachment.sendInitialUpdate();
    setTooltip (param.getName (64));
    onClick = [this] { attachment.setValueAsCompleteGesture (param.getValue() > 0.5f ? 0.0f : 1.0f); };
}

//==============================================================================
void LedDisplay::paint (Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Colour (0xff050806));
    g.fillRoundedRectangle (r, 4.0f);
    g.setColour (Colour (0xff2b2b2b));
    g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, 1.5f);

    auto txt = r.reduced (8.0f, 4.0f);
    const String s = String (a).paddedLeft ('0', 2) + " " + String (b).paddedLeft ('0', 2);
    auto f = Font (FontOptions (Font::getDefaultMonospacedFontName(), txt.getHeight() * 0.95f, Font::bold)).withHorizontalScale (0.9f);
    g.setFont (f);
    // unlit segments
    g.setColour (Colour (0xff0f2a12));
    g.drawText ("88 88", txt, Justification::centred);
    // glow + lit digits
    g.setColour (Colour (0xff35ff5a).withAlpha (0.25f));
    for (auto o : { Point<float> (-1, 0), Point<float> (1, 0), Point<float> (0, -1), Point<float> (0, 1) })
        g.drawText (s, txt.translated (o.x, o.y), Justification::centred);
    g.setColour (Colour (0xff5dff7a));
    g.drawText (s, txt, Justification::centred);

    ColourGradient glass (Colours::white.withAlpha (0.10f), r.getX(), r.getY(), Colours::transparentWhite, r.getX(), r.getCentreY(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (r.withHeight (r.getHeight() * 0.5f), 4.0f);
}

//==============================================================================
BottomBar::BottomBar (AudioProcessorValueTreeState& s)
    : state (s), display (s, "display"), soloM (s, "soloMid"), soloS (s, "soloSide"),
      tolLAttach (*s.getParameter ("tolL"), [this] (float) { refreshDisplay(); }, nullptr),
      tolRAttach (*s.getParameter ("tolR"), [this] (float) { refreshDisplay(); }, nullptr)
{
    for (auto* c : std::initializer_list<Component*> { &display, &soloM, &soloS, &random, &prev, &next, &led })
        addAndMakeVisible (c);

    setBufferedToImage (true);
    random.setTooltip ("Random component tolerances for L and R");
    prev.setTooltip ("Previous tolerance channel pair");
    next.setTooltip ("Next tolerance channel pair");
    led.setTooltip ("Component tolerance channels (L R): every channel is a slightly different 'unit'");

    random.onClick = [this]
    {
        Random r;
        const int a = r.nextInt (100);
        int b = r.nextInt (99); if (b >= a) ++b;
        tolLAttach.setValueAsCompleteGesture ((float) a);
        tolRAttach.setValueAsCompleteGesture ((float) b);
    };
    prev.onClick = [this] { stepTolerance (-1); };
    next.onClick = [this] { stepTolerance (1); };
    refreshDisplay();
}

BottomBar::~BottomBar() = default;

void BottomBar::stepTolerance (int d)
{
    auto wrap = [] (int v) { return (v % 100 + 100) % 100; };
    const int a = (int) std::lround (state.getRawParameterValue ("tolL")->load());
    const int b = (int) std::lround (state.getRawParameterValue ("tolR")->load());
    tolLAttach.setValueAsCompleteGesture ((float) wrap (a + d));
    tolRAttach.setValueAsCompleteGesture ((float) wrap (b + d));
}

void BottomBar::refreshDisplay()
{
    led.setValues ((int) std::lround (state.getRawParameterValue ("tolL")->load()),
                   (int) std::lround (state.getRawParameterValue ("tolR")->load()));
}

void BottomBar::resized()
{
    const int by = 64, bs = 44;
    display.setBounds (248, by, bs, bs);
    random.setBounds (528, by, bs, bs);
    prev.setBounds (624, by, bs, bs);
    led.setBounds (690, by + 1, 140, bs - 2);
    next.setBounds (852, by, bs, bs);
    soloM.setBounds (1018, by, bs, bs);
    soloS.setBounds (1108, by, bs, bs);
}

void BottomBar::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    // black powder coat, same finish as the blue panel
    paintPowderCoat (g, b, Colour (0xff1e1f20), Colour (0xff0c0c0d), 23);

    // rack ear slots (continuing the panel's)
    for (float x : { 0.0f, b.getWidth() - 42.0f })
    {
        g.setColour (Colour (0xff020202));
        g.fillRoundedRectangle (x - (x == 0 ? 20.0f : 0.0f), 66.0f, 62.0f, 18.0f, 9.0f);
    }

    // tube icons
    for (int i = 0; i < 3; ++i)
    {
        const float x = 66.0f + (float) i * 42.0f, y = 52.0f;
        Path tube;
        tube.addRoundedRectangle (x, y, 24.0f, 46.0f, 10.0f);
        g.setColour (cream.withAlpha (0.85f));
        g.strokePath (tube, PathStrokeType (1.6f));
        g.drawLine (x + 12, y + 46, x + 12, y + 56, 1.4f);
        g.drawLine (x + 6, y + 46, x + 6, y + 54, 1.2f);
        g.drawLine (x + 18, y + 46, x + 18, y + 54, 1.2f);
        g.drawRect (x + 6.0f, y + 16.0f, 12.0f, 16.0f, 1.2f);
        g.drawLine (x + 8, y + 24, x + 16, y + 24, 1.0f);
    }

    // TOLERANCE and SOLO groups on the same blue as the panel
    paintPowderCoat (g, { 494.0f, 26.0f, 412.0f, 104.0f }, colours::panelTop, colours::panelBottom, 31, 6.0f);
    paintPowderCoat (g, { 982.0f, 26.0f, 184.0f, 104.0f }, colours::panelTop, colours::panelBottom, 37, 6.0f);

    g.setColour (cream);
    g.setFont (uiFont (13.0f));
    auto lab = [&g] (const String& s, float cx, float y) { g.drawText (s, Rectangle<float> (140.0f, 18.0f).withCentre ({ cx, y }), Justification::centred); };
    lab ("DISPLAY", 270, 48);
    lab ("RANDOM", 550, 48);
    lab ("TOLERANCE", 760, 48);
    lab ("SOLO M", 1040, 48);
    lab ("SOLO S", 1130, 48);

    g.setFont (uiFont (13.5f).withExtraKerningFactor (0.1f));
    g.drawText (String (CharPointer_UTF8 ("LINDELL PLUGINS  \xc2\xb7  VIBEALIZER")), Rectangle<float> (40.0f, 116.0f, 420.0f, 22.0f), Justification::centred);
}
} // namespace sa670::ui
