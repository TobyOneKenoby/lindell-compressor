#include "PluginEditor.h"
#include "Parameters.h"

using namespace juce;
using namespace sa670;
namespace pid = sa670::params::id;
namespace cid = sa670::params::chid;

namespace
{
const char* mirroredParams[] = { cid::inGain, cid::thresh, cid::tc, cid::attack, cid::release,
                                 cid::scFilter, cid::dcThr, cid::mix, cid::output };

const layout::Control* findControl (const char* name)
{
    for (auto& c : layout::controls)
        if (String (c.name) == name) return &c;
    return nullptr;
}
} // namespace

SA670AudioProcessorEditor::SA670AudioProcessorEditor (SA670AudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), bottomBar (p.apvts), analyser (p)
{
    panel.setSize (panelW, panelH);
    addAndMakeVisible (panel);
    art.setBounds (0, 0, ui::PanelArt::width, ui::PanelArt::height);
    panel.addAndMakeVisible (art);

    // pilot lamp + the glow it throws on the paint (under the controls)
    lamp.setBounds (Rectangle<int> (150, 150).withCentre ({ 133, 738 }));
    panel.addAndMakeVisible (lamp);

    // ---- photographic controls, positioned exactly where they sit in the artwork ----
    for (auto& c : layout::controls)
    {
        auto ctl = std::make_unique<ui::SpriteControl> (proc.apvts, c, c.name, &bubble);
        ctl->placeAt (c.x, c.y);
        panel.addAndMakeVisible (*ctl);
        controls.push_back (std::move (ctl));
    }

    // pilot lamp + VU backlight follow the ON switch (warm-up / cool-down)
    lamp.setOn (proc.apvts.getRawParameterValue (pid::power)->load() > 0.5f);
    for (auto& ctl : controls)
        if (&ctl->getParam() == proc.apvts.getParameter (pid::power))
            ctl->onValueChange = [this] (float v) { lamp.setOn (v > 0.5f); };

    // manual attack / release (small knobs, only shown in MANUAL time constant)
    if (auto* fl = findControl ("lfBoost"))
    {
        const float tcY[] = { 582.0f, 929.0f };
        for (int c = 0; c < 2; ++c)
        {
            const char* ids[] = { cid::attack, cid::release };
            for (int k = 0; k < 2; ++k)
            {
                auto ctl = std::make_unique<ui::SpriteControl> (proc.apvts, *fl, params::ch (ids[k], c), &bubble, 0.24f);
                ctl->placeAt (1058.0f, tcY[c] - 32.0f + (float) k * 44.0f);
                ctl->setTooltip (k == 0 ? "Manual attack" : "Manual release");
                panel.addAndMakeVisible (*ctl);
                manualKnobs[c][k] = ctl.get();
                controls.push_back (std::move (ctl));
            }
            for (auto& ctl : controls)
                if (&ctl->getParam() == proc.apvts.getParameter (params::ch (cid::tc, c)))
                {
                    auto update = [this, c] (float idx) { for (auto* k : manualKnobs[c]) k->setVisible (std::lround (idx) == 6); };
                    ctl->onValueChange = update;
                    update (proc.apvts.getRawParameterValue (params::ch (cid::tc, c))->load());
                }
        }
    }

    // ---- VU needles ----
    for (int c = 0; c < 2; ++c)
    {
        auto* procPtr = &proc;
        auto* meterParam = proc.apvts.getRawParameterValue (params::ch (cid::meter, c));
        auto* zeroParam = proc.apvts.getRawParameterValue (params::ch (cid::zero, c));
        needles[c] = std::make_unique<ui::VUNeedle> ([procPtr, meterParam, zeroParam, c]
        {
            // BAL (left) = input, ZERO (centre) = gain reduction, BAL (right) = output.
            // In/out: 0 VU = -18 dBFS. GR rests at 0 VU and swings left.
            const int mode = (int) std::lround (meterParam->load());
            float db;
            if (mode == 1) db = -procPtr->meterGR[c].load();
            else
            {
                const float lin = mode == 0 ? procPtr->meterIn[c].load() : procPtr->meterOut[c].load();
                db = 20.0f * std::log10 (lin + 1.0e-9f) + 18.0f;
            }
            return db + zeroParam->load();
        }, c == 0 ? 0.0f : 269.0f);
        panel.addAndMakeVisible (*needles[c]);
    }
    lamp.onBrightness = [this] (float b) { for (auto& n : needles) if (n) n->setBacklight (0.12f + 0.88f * b); };

    bottomBar.setBounds (0, rackH, panelW, ui::BottomBar::height);
    panel.addAndMakeVisible (bottomBar);

    // DISPLAY: spectrum analyser + EQ curve below the unit
    analyser.setBounds (0, panelH, panelW, ui::Analyser::height);
    panel.addChildComponent (analyser);

    panel.addChildComponent (bubble);
    bubble.enabled = true;
    proc.apvts.addParameterListener (pid::display, this);

    // L/R control ganging (LINK switch, or AGC in stereo LINK mode): both directions
    proc.apvts.addParameterListener (pid::chLink, this);
    proc.apvts.addParameterListener (pid::agcMode, this);
    for (auto* m : mirroredParams)
        for (int c = 0; c < 2; ++c)
            proc.apvts.addParameterListener (params::ch (m, c), this);

    setResizable (true, true);
    setSize ((int) (panelW * 0.62f), (int) (panelH * 0.62f));
    showAnalyser (proc.apvts.getRawParameterValue (pid::display)->load() > 0.5f);
}

SA670AudioProcessorEditor::~SA670AudioProcessorEditor()
{
    proc.apvts.removeParameterListener (pid::display, this);
    proc.apvts.removeParameterListener (pid::chLink, this);
    proc.apvts.removeParameterListener (pid::agcMode, this);
    for (auto* m : mirroredParams)
        for (int c = 0; c < 2; ++c)
            proc.apvts.removeParameterListener (params::ch (m, c), this);
}

void SA670AudioProcessorEditor::parameterChanged (const String& id, float v)
{
    // may arrive on the audio thread (automation) -> hop to the message thread
    MessageManager::callAsync ([safe = Component::SafePointer<SA670AudioProcessorEditor> (this), id, v]
    {
        if (safe == nullptr) return;
        if (id == pid::display) { safe->showAnalyser (v > 0.5f); return; }
        if (id == pid::chLink || id == pid::agcMode) { safe->mirrorChannels (0); return; }   // link engaged: right takes left
        safe->mirrorChannels (id.endsWithChar ('2') ? 1 : 0);
    });
}

bool SA670AudioProcessorEditor::controlsLinked() const
{
    // LINK switch, or AGC in stereo "LINK" mode
    return proc.apvts.getRawParameterValue (pid::chLink)->load() > 0.5f
        || std::lround (proc.apvts.getRawParameterValue (pid::agcMode)->load()) == 1;
}

void SA670AudioProcessorEditor::mirrorChannels (int from)
{
    if (mirroring || ! controlsLinked())
        return;

    const ScopedValueSetter<bool> svs (mirroring, true);
    for (auto* m : mirroredParams)
    {
        auto* src = proc.apvts.getParameter (params::ch (m, from));
        auto* dst = proc.apvts.getParameter (params::ch (m, 1 - from));
        if (src != nullptr && dst != nullptr && std::abs (dst->getValue() - src->getValue()) > 1.0e-6f)
        {
            dst->beginChangeGesture();
            dst->setValueNotifyingHost (src->getValue());
            dst->endChangeGesture();
        }
    }
}

void SA670AudioProcessorEditor::showAnalyser (bool show)
{
    const float scale = (float) getWidth() / (float) panelW;
    logicalH = panelH + (show ? ui::Analyser::height : 0);
    panel.setSize (panelW, logicalH);
    analyser.setVisible (show);

    setResizeLimits (panelW / 3, logicalH / 3, panelW * 2, logicalH * 2);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) panelW / (double) logicalH);
    setSize (getWidth(), roundToInt ((float) logicalH * scale));
}

void SA670AudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
}

void SA670AudioProcessorEditor::resized()
{
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) panelW));
}
