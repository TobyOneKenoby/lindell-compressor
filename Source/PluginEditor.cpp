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
    : AudioProcessorEditor (&p), proc (p), bottomBar (p.apvts)
{
    panel.art = ui::loadAsset ("panel_jpg");
    panel.setSize (panelW, panelH);
    addAndMakeVisible (panel);

    // ---- photographic controls, positioned exactly where they sit in the artwork ----
    for (auto& c : layout::controls)
    {
        auto ctl = std::make_unique<ui::SpriteControl> (proc.apvts, c, c.name, &bubble);
        ctl->placeAt (c.x, c.y);
        panel.addAndMakeVisible (*ctl);
        controls.push_back (std::move (ctl));
    }

    // pilot lamp follows the ON switch
    lamp.setBounds (Rectangle<int> (40, 40).withCentre ({ 133, 738 }));
    panel.addAndMakeVisible (lamp);
    lamp.setLit (proc.apvts.getRawParameterValue (pid::power)->load() > 0.5f);
    for (auto& ctl : controls)
        if (&ctl->getParam() == proc.apvts.getParameter (pid::power))
            ctl->onValueChange = [this] (float v) { lamp.setLit (v > 0.5f); };

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
        }, c == 0 ? 0.0f : 270.0f);
        panel.addAndMakeVisible (*needles[c]);
    }

    bottomBar.setBounds (0, rackH, panelW, ui::BottomBar::height);
    panel.addAndMakeVisible (bottomBar);

    panel.addChildComponent (bubble);
    bubble.enabled = proc.apvts.getRawParameterValue (pid::display)->load() > 0.5f;
    proc.apvts.addParameterListener (pid::display, this);

    // L/R link mirroring
    proc.apvts.addParameterListener (pid::chLink, this);
    for (auto* m : mirroredParams)
        proc.apvts.addParameterListener (params::ch (m, 0), this);

    setResizable (true, true);
    setResizeLimits (panelW / 3, panelH / 3, panelW * 2, panelH * 2);
    if (auto* cons = getConstrainer()) cons->setFixedAspectRatio ((double) panelW / (double) panelH);
    setSize ((int) (panelW * 0.62f), (int) (panelH * 0.62f));
}

SA670AudioProcessorEditor::~SA670AudioProcessorEditor()
{
    proc.apvts.removeParameterListener (pid::display, this);
    proc.apvts.removeParameterListener (pid::chLink, this);
    for (auto* m : mirroredParams)
        proc.apvts.removeParameterListener (params::ch (m, 0), this);
}

void SA670AudioProcessorEditor::parameterChanged (const String& id, float v)
{
    // may arrive on the audio thread (automation) -> hop to the message thread
    MessageManager::callAsync ([safe = Component::SafePointer<SA670AudioProcessorEditor> (this), id, v]
    {
        if (safe == nullptr) return;
        if (id == pid::display) { safe->bubble.enabled = v > 0.5f; if (! safe->bubble.enabled) safe->bubble.hide(); return; }
        safe->mirrorLeftToRight();
    });
}

void SA670AudioProcessorEditor::mirrorLeftToRight()
{
    if (mirroring || proc.apvts.getRawParameterValue (pid::chLink)->load() < 0.5f)
        return;

    const ScopedValueSetter<bool> svs (mirroring, true);
    for (auto* m : mirroredParams)
    {
        auto* l = proc.apvts.getParameter (params::ch (m, 0));
        auto* r = proc.apvts.getParameter (params::ch (m, 1));
        if (l != nullptr && r != nullptr && std::abs (r->getValue() - l->getValue()) > 1.0e-6f)
        {
            r->beginChangeGesture();
            r->setValueNotifyingHost (l->getValue());
            r->endChangeGesture();
        }
    }
}

void SA670AudioProcessorEditor::paint (Graphics& g)
{
    g.fillAll (Colours::black);
}

void SA670AudioProcessorEditor::resized()
{
    panel.setTransform (AffineTransform::scale ((float) getWidth() / (float) panelW));
}
