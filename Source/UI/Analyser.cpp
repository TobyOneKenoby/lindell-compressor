#include "Analyser.h"
#include "Sprites.h"

namespace sa670::ui
{
using namespace juce;

namespace
{
    constexpr float fMin = 20.0f, fMax = 20000.0f;
    constexpr float specTop = 6.0f, specBottom = -84.0f;     // spectrum range (dB, tilted)
    constexpr float eqRange = 24.0f;                         // EQ curve +/- dB
    const Colour cream (0xffe6dfcf);

    bool same (const ProgramEQSettings& a, const ProgramEQSettings& b)
    {
        return a.eqp.lowBoost == b.eqp.lowBoost && a.eqp.lowAtten == b.eqp.lowAtten && a.eqp.cps == b.eqp.cps
            && a.eqp.highBoost == b.eqp.highBoost && a.eqp.kcs == b.eqp.kcs && a.eqp.bandwidth == b.eqp.bandwidth
            && a.eqp.highAtten == b.eqp.highAtten && a.eqp.attenSel == b.eqp.attenSel
            && a.midAmt == b.midAmt && a.midBoost == b.midBoost && a.midFreq == b.midFreq
            && a.lowCut == b.lowCut && a.highCut == b.highCut;
    }
}

Analyser::Analyser (SA670AudioProcessor& p) : proc (p)
{
    setInterceptsMouseClicks (false, false);
    model.prepare (176400.0);
}

Analyser::~Analyser()
{
    proc.analyserActive = false;
}

void Analyser::visibilityChanged()
{
    proc.analyserActive = isVisible();
    if (isVisible()) startTimerHz (30); else stopTimer();
}

float Analyser::xForFreq (float f, Rectangle<float> r) const
{
    return r.getX() + r.getWidth() * std::log (f / fMin) / std::log (fMax / fMin);
}

void Analyser::updateCurve()
{
    ProgramEQSettings s;
    proc.readEqSettings (s);
    const bool in = proc.isEqIn();
    if (haveCurve && in == lastEqIn && same (s, lastSettings))
        return;
    lastSettings = s; lastEqIn = in; haveCurve = true;

    model.update (s);
    constexpr int n = 300;
    curveDb.resize (n);
    for (int i = 0; i < n; ++i)
    {
        const double f = fMin * std::pow (fMax / fMin, (double) i / (n - 1));
        curveDb[(size_t) i] = in ? (float) (20.0 * std::log10 (model.magnitude (f) + 1e-12)) : 0.0f;
    }
}

void Analyser::timerCallback()
{
    // pull new output samples into the ring
    auto& fifo = proc.specFifo;
    int s1, n1, s2, n2;
    fifo.prepareToRead (fifo.getNumReady(), s1, n1, s2, n2);
    auto push = [this] (const float* d, int n)
    {
        for (int i = 0; i < n; ++i) { ring[(size_t) ringPos] = d[i]; ringPos = (ringPos + 1) % fftSize; }
    };
    push (proc.specBuffer.data() + s1, n1);
    push (proc.specBuffer.data() + s2, n2);
    fifo.finishedRead (n1 + n2);

    for (int i = 0; i < fftSize; ++i) fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % fftSize)];
    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    const double fs = juce::jmax (1.0, proc.getCurrentRate());
    const float norm = 4.0f / (float) fftSize;   // Hann coherent gain 0.5, one-sided
    for (int b = 1; b < fftSize / 2; ++b)
    {
        const float f = (float) (b * fs / fftSize);
        float db = 20.0f * std::log10 (fftData[(size_t) b] * norm + 1e-9f);
        db += 3.0f * std::log2 (jmax (f, 1.0f) / 1000.0f);      // +3 dB/oct tilt: pink noise reads flat
        auto& v = specDb[(size_t) b];
        v = db > v ? db : v + (db - v) * 0.18f;                  // fast rise, smooth fall
    }

    updateCurve();
    repaint();
}

void Analyser::paint (Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    if (background.isNull() || background.getWidth() != getWidth())
    {
        background = Image (Image::RGB, jmax (1, getWidth()), jmax (1, getHeight()), true);
        Graphics bg (background);
        paintPowderCoat (bg, b, Colour (0xff1c1d1e), Colour (0xff0b0b0c), 11);
    }
    g.drawImageAt (background, 0, 0);

    // screen
    const auto screen = Rectangle<float> (54.0f, 18.0f, b.getWidth() - 108.0f, b.getHeight() - 40.0f);
    paintPowderCoat (g, screen.expanded (6.0f), colours::panelTop, colours::panelBottom, 5, 6.0f);
    g.setColour (Colour (0xff050b0b));
    g.fillRoundedRectangle (screen, 4.0f);
    const auto plot = screen.reduced (34.0f, 12.0f).withTrimmedBottom (14.0f);

    // grid
    g.setFont (Font (FontOptions (11.0f, Font::bold)));
    for (float f : { 20.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 2000.f, 5000.f, 10000.f, 20000.f })
    {
        const float x = xForFreq (f, plot);
        g.setColour (Colour (0xff1d3a3a));
        g.drawVerticalLine ((int) x, plot.getY(), plot.getBottom());
        g.setColour (cream.withAlpha (0.55f));
        const String t = f >= 1000.f ? String (f / 1000.f, 0) + "k" : String ((int) f);
        g.drawText (t, Rectangle<float> (40, 14).withCentre ({ x, plot.getBottom() + 11.0f }), Justification::centred);
    }
    auto yEq = [&plot] (float db) { return plot.getCentreY() - db / eqRange * plot.getHeight() * 0.5f; };
    for (float db : { -18.f, -12.f, -6.f, 0.f, 6.f, 12.f, 18.f })
    {
        const float y = yEq (db);
        g.setColour (db == 0.f ? Colour (0xff2c5655) : Colour (0xff163030));
        g.drawHorizontalLine ((int) y, plot.getX(), plot.getRight());
        g.setColour (cream.withAlpha (0.55f));
        g.drawText ((db > 0 ? "+" : "") + String ((int) db), Rectangle<float> (30, 12).withCentre ({ plot.getRight() + 18.0f, y }), Justification::centred);
    }
    g.setColour (cream.withAlpha (0.4f));
    g.drawText ("SPECTRUM", Rectangle<float> (plot.getX() + 6, plot.getY() + 2, 120, 14), Justification::left);
    g.setColour (cream.withAlpha (0.85f));
    g.drawText ("EQ CURVE", Rectangle<float> (plot.getX() + 6, plot.getY() + 16, 120, 14), Justification::left);

    // spectrum (filled)
    {
        const double fs = jmax (1.0, proc.getCurrentRate());
        Path sp;
        sp.startNewSubPath (plot.getX(), plot.getBottom());
        for (float x = plot.getX(); x <= plot.getRight(); x += 2.0f)
        {
            const float t0 = (x - plot.getX()) / plot.getWidth(), t1 = (x + 2.0f - plot.getX()) / plot.getWidth();
            const float f0 = fMin * std::pow (fMax / fMin, t0), f1 = fMin * std::pow (fMax / fMin, t1);
            const float b0 = (float) (f0 * fftSize / fs), b1 = (float) (f1 * fftSize / fs);
            float db;
            if (b1 - b0 < 1.0f)
            {
                const int i = jlimit (1, fftSize / 2 - 2, (int) b0);
                const float fr = jlimit (0.0f, 1.0f, b0 - (float) i);
                db = specDb[(size_t) i] + fr * (specDb[(size_t) i + 1] - specDb[(size_t) i]);
            }
            else
            {
                db = -200.0f;
                for (int i = jlimit (1, fftSize / 2 - 1, (int) b0); i <= jlimit (1, fftSize / 2 - 1, (int) b1); ++i)
                    db = jmax (db, specDb[(size_t) i]);
            }
            const float y = jmap (jlimit (specBottom, specTop, db), specBottom, specTop, plot.getBottom(), plot.getY());
            sp.lineTo (x, y);
        }
        sp.lineTo (plot.getRight(), plot.getBottom());
        sp.closeSubPath();
        ColourGradient gr (Colour (0xff4fb3ad).withAlpha (0.55f), 0, plot.getY(), Colour (0xff1d5c5a).withAlpha (0.15f), 0, plot.getBottom(), false);
        g.setGradientFill (gr);
        g.fillPath (sp);
        g.setColour (Colour (0xff7fd6cf).withAlpha (0.7f));
        g.strokePath (sp, PathStrokeType (1.0f));
    }

    // EQ curve
    if (! curveDb.empty())
    {
        Path c;
        const int n = (int) curveDb.size();
        for (int i = 0; i < n; ++i)
        {
            const float x = plot.getX() + plot.getWidth() * (float) i / (float) (n - 1);
            const float y = yEq (jlimit (-eqRange, eqRange, curveDb[(size_t) i]));
            if (i == 0) c.startNewSubPath (x, y); else c.lineTo (x, y);
        }
        g.setColour (Colour (0xffffd9a0).withAlpha (0.18f));
        g.strokePath (c, PathStrokeType (7.0f, PathStrokeType::curved, PathStrokeType::rounded));
        g.setColour (lastEqIn ? Colour (0xfff3e3bf) : cream.withAlpha (0.35f));
        g.strokePath (c, PathStrokeType (2.2f, PathStrokeType::curved, PathStrokeType::rounded));
    }

    // glass
    ColourGradient glass (Colours::white.withAlpha (0.05f), screen.getX(), screen.getY(), Colours::transparentWhite, screen.getX(), screen.getCentreY(), false);
    g.setGradientFill (glass);
    g.fillRoundedRectangle (screen.withHeight (screen.getHeight() * 0.45f), 4.0f);
    g.setColour (Colours::black);
    g.drawRoundedRectangle (screen, 4.0f, 1.5f);
}
} // namespace sa670::ui
