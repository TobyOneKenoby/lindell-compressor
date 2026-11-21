#include "Sprites.h"
#include "BinaryData.h"

namespace sa670::ui
{
using namespace juce;

static Font uiFont (float h) { return Font (FontOptions (h, Font::bold)).withExtraKerningFactor (0.06f); }
static const Colour cream (0xffe6dfcf);

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
                      repaint();
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

    auto spriteTransform = [&] (float rotationDeg, bool flipV, float pad)
    {
        auto t = AffineTransform::translation (-(S * 0.5f + pad), -(S * 0.5f + pad));
        if (flipV) t = t.scaled (1.0f, -1.0f);
        t = t.rotated (degreesToRadians (rotationDeg)).scaled (scale).translated (c);
        return t;
    };

    float rot = 0.0f;
    bool flip = false;
    switch (info.kind)
    {
        case layout::Kind::Chicken: rot = currentAngle() - info.baked; break;
        case layout::Kind::Toggle:  flip = value01 < 0.5f; break;
        case layout::Kind::Screw:   rot = (value01 - 0.5f) * 270.0f; break;
        case layout::Kind::Fluted:  break;
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
        const float a = degreesToRadians (currentAngle());
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
// VU needle. The printed scale in the artwork is not a perfect arc, so the tick
// positions were measured (source pixels) and the needle is aimed at them.
namespace
{
    constexpr float vuDb[] = { -23, -20, -10, -7, -5, -3, -2, -1, 0, 1, 2, 3, 4 };
    constexpr float vuX[]  = { 194, 200, 220, 241, 251, 267, 285, 293, 302, 316, 330, 344, 349 };
    constexpr float vuY[]  = { 575, 572, 566, 561, 558, 555, 554, 555, 557, 562, 568, 580, 586 };
    constexpr float pivotX = 281.0f, pivotY = 690.0f;
    const Rectangle<float> meterArea (185.0f, 500.0f, 185.0f, 140.0f);   // component bounds (meter 1)
    inline float dbToFrac (float db) { return (std::pow (10.0f, db / 20.0f) - 0.1f) / (std::pow (10.0f, 0.15f) - 0.1f); }
    inline float fracToDb (float f) { return 20.0f * std::log10 (jmax (1.0e-4f, 0.1f + f * (std::pow (10.0f, 0.15f) - 0.1f))); }
}

VUNeedle::VUNeedle (std::function<float()> s, float yOffset) : source (std::move (s)), dy (yOffset)
{
    setInterceptsMouseClicks (false, false);
    setBounds (meterArea.translated (0.0f, dy).toNearestInt());
    startTimerHz (60);
}

Point<float> VUNeedle::scalePoint (float db) const
{
    db = jlimit (vuDb[0], vuDb[12], db);
    int i = 0;
    while (i < 11 && db > vuDb[i + 1]) ++i;
    const float t = (db - vuDb[i]) / (vuDb[i + 1] - vuDb[i]);
    return { vuX[i] + t * (vuX[i + 1] - vuX[i]), vuY[i] + t * (vuY[i + 1] - vuY[i]) };
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
        if (frac < -0.04f) { frac = -0.04f; velocity = -velocity * 0.2f; }   // end stops
        if (frac > 1.09f)  { frac = 1.09f;  velocity = -velocity * 0.2f; }
    }
    needleDb = fracToDb (frac);
    repaint();
}

void VUNeedle::paint (Graphics& g)
{
    const Point<float> origin (meterArea.getX(), meterArea.getY() + dy);
    const Point<float> pivot = Point<float> (pivotX, pivotY + dy) - origin;
    const Point<float> onScale = scalePoint (needleDb) + Point<float> (0.0f, dy) - origin;
    const Point<float> tip = onScale + (onScale - pivot) / onScale.getDistanceFrom (pivot) * 7.0f;

    // visible window of the face, minus the pivot dome
    Path clip;
    clip.addRoundedRectangle (Rectangle<float> (194.0f, 512.0f, 165.0f, 113.0f).translated (0.0f, dy) - origin, 6.0f);
    Path dome;
    dome.addEllipse (Rectangle<float> (255.0f, 619.0f, 52.0f, 26.0f).translated (0.0f, dy) - origin);
    clip.addPath (dome);
    clip.setUsingNonZeroWinding (false);

    g.saveState();
    g.reduceClipRegion (clip);
    g.setColour (Colours::black.withAlpha (0.18f));
    g.drawLine (Line<float> (pivot.translated (2.5f, 3.0f), tip.translated (2.5f, 3.0f)), 1.8f);
    g.setColour (Colour (0xff17130f));
    g.drawLine (Line<float> (pivot, tip), 1.5f);
    g.restoreState();
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
    // black anodised, brushed
    ColourGradient bg (Colour (0xff1d1e1f), 0, 0, Colour (0xff0a0a0b), 0, b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (b);
    {
        Random rng (7);
        for (int i = 0; i < 900; ++i)
        {
            const float y = rng.nextFloat() * b.getHeight();
            g.setColour (Colours::white.withAlpha (0.012f + 0.012f * rng.nextFloat()));
            g.drawHorizontalLine ((int) y, rng.nextFloat() * b.getWidth() * 0.5f, b.getWidth() * (0.5f + 0.5f * rng.nextFloat()));
        }
    }
    g.setColour (Colours::white.withAlpha (0.12f));
    g.drawHorizontalLine (1, 0, b.getWidth());
    g.setColour (Colours::black);
    g.drawHorizontalLine (0, 0, b.getWidth());

    // rack ear slots
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

    // group frames
    auto frame = [&g] (Rectangle<float> r)
    {
        g.setColour (Colour (0xff0e2c2a));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Colour (0xff3d4a4a));
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
        g.setColour (Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.reduced (2.5f), 5.0f, 1.0f);
    };
    frame ({ 494.0f, 26.0f, 412.0f, 104.0f });
    frame ({ 982.0f, 26.0f, 184.0f, 104.0f });

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

    // screws
    for (auto p : { Point<float> (30, 24), Point<float> (b.getWidth() - 30, 24), Point<float> (30, 126), Point<float> (b.getWidth() - 30, 126) })
    {
        ColourGradient sg (Colour (0xff9a9a96), p.x - 4, p.y - 4, Colour (0xff2c2c2a), p.x + 4, p.y + 5, false);
        g.setGradientFill (sg);
        g.fillEllipse (p.x - 6, p.y - 6, 12, 12);
        g.setColour (Colour (0xff111111));
        g.drawLine (p.x - 4, p.y + 2, p.x + 4, p.y - 2, 1.6f);
    }
}
} // namespace sa670::ui
