// Offline test: runs audio through the processor and renders the editor to PNG.
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cstdio>

static double rmsDb (const juce::AudioBuffer<float>& b, int ch, int from)
{
    double s = 0; int n = 0;
    for (int i = from; i < b.getNumSamples(); ++i) { s += b.getSample (ch, i) * b.getSample (ch, i); ++n; }
    return 10.0 * std::log10 (s / juce::jmax (1, n) + 1e-20);
}

static void set (SA670AudioProcessor& p, const char* id, float value)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (value));
}

static double sineGain (SA670AudioProcessor& p, double f, float ampDb, double fs = 48000, bool* finite = nullptr)
{
    const int bs = 512, blocks = (int) (fs * 1.0 / bs);
    juce::AudioBuffer<float> buf (2, bs), all (2, bs * blocks);
    juce::MidiBuffer midi;
    const float amp = juce::Decibels::decibelsToGain (ampDb);
    p.prepareToPlay (fs, bs);
    double ph = 0;
    for (int b = 0; b < blocks; ++b)
    {
        for (int i = 0; i < bs; ++i) { const float v = amp * (float) std::sin (ph); ph += 2 * juce::MathConstants<double>::pi * f / fs; buf.setSample (0, i, v); buf.setSample (1, i, v); }
        p.processBlock (buf, midi);
        for (int c = 0; c < 2; ++c) all.copyFrom (c, b * bs, buf, c, 0, bs);
    }
    if (finite)
    {
        *finite = true;
        for (int c = 0; c < 2; ++c) for (int i = 0; i < all.getNumSamples(); ++i) if (! std::isfinite (all.getSample (c, i))) *finite = false;
    }
    return rmsDb (all, 0, all.getNumSamples() / 2) - (ampDb - 3.0103);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI gui;
    SA670AudioProcessor p;
    p.setPlayConfigDetails (2, 2, 48000, 512);

    // neutral: EQ flat, comp out, colour off
    set (p, "compIn", 0); set (p, "power", 0);
    printf ("Neutral (EQ in, flat, comp out, amp linear) gain @100/1k/10k: %.2f %.2f %.2f dB\n",
            sineGain (p, 100, -20), sineGain (p, 1000, -20), sineGain (p, 10000, -20));

    set (p, "lfBoost", 10); set (p, "lfFreq", 2);
    printf ("LOW BOOST 10 @60: 30Hz %.1f  1k %.1f dB\n", sineGain (p, 30, -30), sineGain (p, 1000, -30));
    set (p, "lfAtten", 5);
    printf ("+ LOW ATTEN 5:    30Hz %.1f  300Hz %.1f dB (Pultec trick)\n", sineGain (p, 30, -30), sineGain (p, 300, -30));
    set (p, "lfBoost", 0); set (p, "lfAtten", 0);
    set (p, "hfBoost", 10); set (p, "hfFreq", 4);
    printf ("HIGH BOOST 10 @10k: 10k %.1f dB\n", sineGain (p, 10000, -30));
    set (p, "hfBoost", 0);
    set (p, "midAmt", 10); set (p, "midBoost", 0); set (p, "midFreq", 4);
    printf ("MID ATTEN 10 @1k: 1k %.1f dB\n", sineGain (p, 1000, -30));
    set (p, "midAmt", 0);

    // EQ filters
    set (p, "lowCut", 5);   // 250 Hz, 24 dB/oct
    printf ("LOW CUT 250 Hz: 62.5Hz %.1f  125Hz %.1f  250Hz %.1f  1k %.1f dB\n", sineGain (p, 62.5, -20), sineGain (p, 125, -20), sineGain (p, 250, -20), sineGain (p, 1000, -20));
    set (p, "lowCut", 0); set (p, "highCut", 1);  // 2 kHz, 6 dB/oct
    printf ("HIGH CUT 2 kHz: 1k %.1f  2k %.1f  4k %.1f  8k %.1f dB\n", sineGain (p, 1000, -20), sineGain (p, 2000, -20), sineGain (p, 4000, -20), sineGain (p, 8000, -20));
    set (p, "highCut", 10);

    // colour
    set (p, "power", 1); set (p, "tube", 1);
    printf ("Tube amp on, 1k @ -6 dBFS gain: %.2f dB\n", sineGain (p, 1000, -6));
    set (p, "tube", 0);
    printf ("SS amp on,   1k @ -6 dBFS gain: %.2f dB\n", sineGain (p, 1000, -6));

    // compressor
    set (p, "compIn", 1); set (p, "thresh1", 6); set (p, "thresh2", 6);
    bool fin = false;
    const double g = sineGain (p, 1000, -6, 48000, &fin);
    printf ("Comp thresh 6, -6 dBFS sine: gain %.1f dB, GR meter %.1f dB, finite=%d\n", g, p.meterGR[0].load(), (int) fin);
    set (p, "agcMode", 3);
    printf ("Mid-side link: gain %.1f dB\n", sineGain (p, 1000, -6, 48000, &fin));
    {
        // M/S: Lat (mid) compressed hard, Vert (side) untouched -> side must pass at unity
        set (p, "agcMode", 2); set (p, "thresh1", 10); set (p, "thresh2", 0); set (p, "inGain2", 0);
        const int bs = 512; juce::AudioBuffer<float> buf (2, bs); juce::MidiBuffer midi; p.prepareToPlay (48000, bs);
        double ph = 0, sm = 0, ss = 0; int cnt = 0;
        for (int b = 0; b < 200; ++b)
        {
            for (int i = 0; i < bs; ++i)
            {   // L = mid + side, R = mid - side ; mid 1 kHz -10 dBFS, side 3 kHz -30 dBFS
                const float m = 0.316f * (float) std::sin (ph), sd = 0.0316f * (float) std::sin (3 * ph);
                ph += 2 * juce::MathConstants<double>::pi * 1000 / 48000;
                buf.setSample (0, i, m + sd); buf.setSample (1, i, m - sd);
            }
            p.processBlock (buf, midi);
            if (b > 100) for (int i = 0; i < bs; ++i)
            {
                const double M = 0.5 * (buf.getSample (0, i) + buf.getSample (1, i)), S = 0.5 * (buf.getSample (0, i) - buf.getSample (1, i));
                sm += M * M; ss += S * S; ++cnt;
            }
        }
        printf ("Mid-Side mode: mid gain %.1f dB (compressed), side gain %.1f dB (untouched), GR lat %.1f / vert %.1f\n",
                10 * std::log10 (sm / cnt) - 20 * std::log10 (0.316 / std::sqrt (2.0)),
                10 * std::log10 (ss / cnt) - 20 * std::log10 (0.0316 / std::sqrt (2.0)),
                p.meterGR[0].load(), p.meterGR[1].load());
        set (p, "thresh1", 6); set (p, "thresh2", 6);
    }
    printf ("44.1k run finite: "); sineGain (p, 1000, 0, 44100, &fin); printf ("%d\n", (int) fin);

    // CPU
    {
        p.prepareToPlay (48000, 512);
        juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi;
        juce::Random r; for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, r.nextFloat() * 0.5f - 0.25f);
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        const int blocks = 940; // ~10 s audio
        for (int b = 0; b < blocks; ++b) p.processBlock (buf, midi);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        printf ("CPU: 10 s stereo audio processed in %.0f ms (%.1f %% of one core)\n", ms, ms / 100.0);
    }

    // GUI snapshot
    {
        set (p, "lfBoost", 6); set (p, "lfAtten", 3); set (p, "hfBoost", 4); set (p, "bandwidth", 4); set (p, "hfAtten", 2); set (p, "midAmt", 3); set (p, "lfBoost", 8); set (p, "lfAtten", 5); set (p, "hfBoost", 6); set (p, "lowCut", 1); set (p, "highCut", 9); set (p, "power", 1); set (p, "eqIn", 1); set (p, "soloMid", 0); set (p, "agcMode", 1); set (p, "tc1", 1); set (p, "tc2", 1); set (p, "thresh1", 5); set (p, "thresh2", 5); set (p, "mix1", 8); set (p, "mix2", 8); set (p, "dcThr1", 5); set (p, "dcThr2", 5); set (p, "output1", 5); set (p, "output2", 5); set (p, "inGain1", 6); set (p, "inGain2", 6); set (p, "meter2", 1); set (p, "display", 1);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        ed->setSize (SA670AudioProcessorEditor::panelW, SA670AudioProcessorEditor::panelH + sa670::ui::Analyser::height);
        {
            juce::AudioBuffer<float> buf (2, 512); juce::MidiBuffer midi; juce::Random r;
            p.prepareToPlay (48000, 512);
            for (int b = 0; b < 40; ++b)
            {
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 512; ++i) buf.setSample (c, i, (r.nextFloat() - 0.5f) * 0.3f);
                p.processBlock (buf, midi);
            }
            std::function<void (juce::Component&)> visit = [&] (juce::Component& c)
            {
                if (auto* a = dynamic_cast<sa670::ui::Analyser*> (&c)) for (int k = 0; k < 8; ++k) a->refreshNow();
                for (auto* ch : c.getChildren()) visit (*ch);
            };
            visit (*ed);
        }
        auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 2.0f);
        juce::File out (juce::File::getCurrentWorkingDirectory().getChildFile ("sa670_gui.png"));
        out.deleteFile();
        juce::FileOutputStream os (out);
        juce::PNGImageFormat().writeImageToStream (img, os);
        printf ("GUI snapshot: %s\n", out.getFullPathName().toRawUTF8());
    }
    return 0;
}
