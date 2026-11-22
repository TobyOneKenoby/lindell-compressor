#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Parameters.h"

using namespace sa670;

namespace
{
constexpr int subBlock = 32;
inline float dbToGain (float db) noexcept { return std::exp (db * 0.11512925f); }
inline int idx (std::atomic<float>* a) noexcept { return (int) std::lround (a->load()); }
} // namespace

SA670AudioProcessor::SA670AudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "SA670", params::createLayout())
{
    namespace id = params::id;
    eqIn = p (id::eqIn); lfBoost = p (id::lfBoost); lfAtten = p (id::lfAtten);
    lfFreq = p (id::lfFreq); midAmt = p (id::midAmt); midBoost = p (id::midBoost);
    midFreq = p (id::midFreq); hfBoost = p (id::hfBoost); hfFreq = p (id::hfFreq);
    bandwidth = p (id::bandwidth); hfAtten = p (id::hfAtten); attenSel = p (id::attenSel);
    tube = p (id::tube); lowCut = p (id::lowCut); highCut = p (id::highCut);
    power = p (id::power); bypass = p (id::bypass); compIn = p (id::compIn);
    agcMode = p (id::agcMode); chLink = p (id::chLink);
    soloMid = p (id::soloMid); soloSide = p (id::soloSide); tolL = p (id::tolL); tolR = p (id::tolR);

    for (int c = 0; c < 2; ++c)
    {
        auto q = [this, c] (const char* base) { return apvts.getRawParameterValue (params::ch (base, c)); };
        chp[c] = { q (params::chid::inGain), q (params::chid::thresh), q (params::chid::tc),
                   q (params::chid::attack), q (params::chid::release), q (params::chid::bal),
                   q (params::chid::scFilter), q (params::chid::dcThr), q (params::chid::mix),
                   q (params::chid::output) };
    }

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (apvts.getParameter (id::bypass));
}

bool SA670AudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void SA670AudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate;
    const int maxBlock = juce::jmax (1, samplesPerBlock);

    // Run the passive networks at >= ~176 kHz so the bilinear frequency warping
    // of the 3-20 kHz resonances stays small.
    const int stages = sr < 60000.0 ? 2 : (sr < 120000.0 ? 1 : 0);
    osFactor = 1 << stages;

    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
        2, (size_t) stages, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) maxBlock);
    oversampling->reset();

    const double osRate = sr * osFactor;
    eq.prepare (osRate);
    for (int c = 0; c < 2; ++c)
    {
        comp[c].prepare (sr);
        transformer[c].prepare (osRate);
        dcBlock[c].prepare (sr);
    }

    latency = (int) std::lround (oversampling->getLatencyInSamples());
    setLatencySamples (latency);

    dryDelay.setSize (2, latency + 1);
    dryDelay.clear();
    dryDelayPos = 0;
    dryCopy.setSize (2, maxBlock);

    firstBlock = true;
}

void SA670AudioProcessor::readEqSettings (ProgramEQSettings& s)
{
    s.eqp.lowBoost = lfBoost->load();
    s.eqp.lowAtten = lfAtten->load();
    s.eqp.cps = juce::jlimit (0, 3, idx (lfFreq));
    s.eqp.highBoost = hfBoost->load();
    s.eqp.kcs = juce::jlimit (0, 6, idx (hfFreq));
    s.eqp.bandwidth = bandwidth->load();
    s.eqp.highAtten = hfAtten->load();
    s.eqp.attenSel = juce::jlimit (0, 2, idx (attenSel));
    s.midAmt = midAmt->load();
    s.midBoost = midBoost->load() > 0.5f;
    s.midFreq = params::midFreqHz[juce::jlimit (0, 10, idx (midFreq))];
    s.lowCut = params::lowCutHz[juce::jlimit (0, 10, idx (lowCut))];
    s.highCut = params::highCutHz[juce::jlimit (0, 10, idx (highCut))];
}

void SA670AudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int totalIn = getTotalNumInputChannels();
    for (int c = totalIn; c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    const int nCh = juce::jmin (2, totalIn, buffer.getNumChannels());
    if (nCh <= 0 || oversampling == nullptr)
        return;

    float grMax[2] = { 0, 0 };
    double inSum[2] = { 0, 0 }, outSum[2] = { 0, 0 };   // sums of |x| (rectified, VU style)

    const int maxChunk = dryCopy.getNumSamples();
    for (int offset = 0; offset < buffer.getNumSamples(); offset += maxChunk)
    {
        const int n = juce::jmin (maxChunk, buffer.getNumSamples() - offset);
        float* chans[2] = { buffer.getWritePointer (0, offset), nCh > 1 ? buffer.getWritePointer (1, offset) : nullptr };
        processChunk (chans, nCh, n, grMax, inSum, outSum);
    }

    // feed the spectrum analyser (mono sum of the output)
    if (analyserActive.load (std::memory_order_relaxed))
    {
        const int num = buffer.getNumSamples();
        int s1, n1, s2, n2;
        specFifo.prepareToWrite (num, s1, n1, s2, n2);
        auto mono = [&] (int i) { return nCh > 1 ? 0.5f * (buffer.getSample (0, i) + buffer.getSample (1, i)) : buffer.getSample (0, i); };
        for (int i = 0; i < n1; ++i) specBuffer[(size_t) (s1 + i)] = mono (i);
        for (int i = 0; i < n2; ++i) specBuffer[(size_t) (s2 + i)] = mono (n1 + i);
        specFifo.finishedWrite (n1 + n2);
    }

    // VU movements respond to the full-wave rectified average; scale so a sine reads its RMS value
    constexpr double sineAvgToRms = 1.1107207345;
    const int N = juce::jmax (1, buffer.getNumSamples());
    for (int c = 0; c < 2; ++c)
    {
        const int src = juce::jmin (c, nCh - 1);
        meterIn[c].store ((float) (inSum[src] / N * sineAvgToRms));
        meterOut[c].store ((float) (outSum[src] / N * sineAvgToRms));
        meterGR[c].store (grMax[src]);
    }
}

void SA670AudioProcessor::processChunk (float* const* chans, int nCh, int n, float* grMax, double* inSum, double* outSum)
{
    for (int c = 0; c < nCh; ++c)
    {
        dryCopy.copyFrom (c, 0, chans[c], n);
        for (int i = 0; i < n; ++i) inSum[c] += std::abs ((double) chans[c][i]);
    }

    // ---------- parameter targets ----------
    ProgramEQSettings eqTarget;
    readEqSettings (eqTarget);
    sEqIn.target   = eqIn->load();
    sCompIn.target = compIn->load();
    sPower.target  = power->load();
    sTube.target   = tube->load();
    sBypass.target = bypass->load();

    const int mode = idx (agcMode);
    const bool stereo = nCh == 2;
    const bool ms = stereo && (mode == 2 || mode == 3);
    const bool link = stereo && (mode == 1 || mode == 3);
    const bool ctrlLink = chLink->load() > 0.5f || mode == 1;   // ganged controls (LINK switch or stereo LINK)

    for (int c = 0; c < nCh; ++c)
    {
        const auto& cp = chp[ctrlLink ? 0 : c];
        sInGain[c].target = dbToGain (cp.inGain->load());
        sOutGain[c].target = dbToGain ((cp.output->load() - 5.0f) * 2.4f + cp.bal->load());
        sMix[c].target = cp.mix->load() / 10.0f;

        VariMuSettings vs;
        const float dc = cp.dcThr->load();
        // tube matching: each tolerance channel shifts the 6386 bias point a little
        const int seed = idx (c == 0 ? tolL : tolR);
        const float tolDb = 0.6f * (float) (((seed * 7919) % 101) - 50) / 50.0f;
        vs.thresholdDb = -2.0f - cp.thresh->load() * 3.2f + tolDb;  // 0..10 -> -2..-34 dBFS
        vs.ratio = 2.0f + dc * 1.8f;                              // 2:1 .. 20:1 (limit)
        vs.kneeDb = 18.0f - dc * 1.5f;                            // soft -> hard knee
        vs.timeConstant = juce::jlimit (0, 6, idx (cp.tc));
        vs.manualAttackMs = cp.attack->load();
        vs.manualReleaseMs = cp.release->load();
        vs.scFilter = juce::jlimit (0, 3, idx (cp.scFilter));
        comp[c].update (vs);
    }

    // component tolerances: each channel behaves like a slightly different unit
    eq.setTolerance (0, idx (tolL));
    eq.setTolerance (1, idx (tolR));

    if (firstBlock)
    {
        eqSmoothed = eqTarget;
        for (auto* s : { &sEqIn, &sCompIn, &sPower, &sTube, &sBypass, &sInGain[0], &sInGain[1],
                         &sOutGain[0], &sOutGain[1], &sMix[0], &sMix[1] })
            s->snap();
        firstBlock = false;
    }

    const float a = 1.0f - std::exp (-(float) subBlock / (0.015f * (float) sr)); // ~15 ms
    auto step = [a] (float& cur, float target) { cur += (target - cur) * a; };
    auto stepD = [a] (double& cur, double target) { cur += (target - cur) * (double) a; };

    // =====================================================================
    //  Program EQ (passive networks) + make-up amp, oversampled
    // =====================================================================
    {
        juce::dsp::AudioBlock<float> block (chans, (size_t) nCh, (size_t) n);
        auto up = oversampling->processSamplesUp (block);
        const int un = (int) up.getNumSamples();
        const int osSub = subBlock * osFactor;

        for (int start = 0; start < un; start += osSub)
        {
            const int len = juce::jmin (osSub, un - start);

            // pots glide, switches jump (like the hardware)
            stepD (eqSmoothed.eqp.lowBoost, eqTarget.eqp.lowBoost);
            stepD (eqSmoothed.eqp.lowAtten, eqTarget.eqp.lowAtten);
            stepD (eqSmoothed.eqp.highBoost, eqTarget.eqp.highBoost);
            stepD (eqSmoothed.eqp.bandwidth, eqTarget.eqp.bandwidth);
            stepD (eqSmoothed.eqp.highAtten, eqTarget.eqp.highAtten);
            stepD (eqSmoothed.midAmt, eqTarget.midAmt);
            eqSmoothed.eqp.cps = eqTarget.eqp.cps;   eqSmoothed.eqp.kcs = eqTarget.eqp.kcs;
            eqSmoothed.eqp.attenSel = eqTarget.eqp.attenSel;
            eqSmoothed.midBoost = eqTarget.midBoost; eqSmoothed.midFreq = eqTarget.midFreq;
            eqSmoothed.lowCut = eqTarget.lowCut;     eqSmoothed.highCut = eqTarget.highCut;
            eq.update (eqSmoothed);

            const float e0 = sEqIn.cur, p0 = sPower.cur, t0 = sTube.cur;
            step (sEqIn.cur, sEqIn.target);
            step (sPower.cur, sPower.target);
            step (sTube.cur, sTube.target);
            const float e1 = sEqIn.cur, p1 = sPower.cur, t1 = sTube.cur;

            for (int c = 0; c < nCh; ++c)
            {
                float* y = up.getChannelPointer ((size_t) c) + start;
                for (int i = 0; i < len; ++i)
                {
                    const float f = (float) i / (float) len;
                    const float em = e0 + (e1 - e0) * f, pw = p0 + (p1 - p0) * f, tb = t0 + (t1 - t0) * f;

                    // passive network (always runs so its state stays continuous)
                    const float in = y[i];
                    const float wet = eq.process (c, in);
                    const float x = in + (wet - in) * em;

                    // make-up amplifier: Tube (EQP-1A push-pull) or SS (API style)
                    const float h = x * 0.5f;                      // +6 dB headroom
                    float sat = (tb * OutputStage::tube (h) + (1.0f - tb) * OutputStage::solidState (h)) * 2.0f;
                    sat = transformer[c].process (sat, 0.25f + 0.25f * (1.0f - tb));
                    y[i] = x + (sat - x) * pw;
                }
            }
        }
        oversampling->processSamplesDown (block);

        for (int c = 0; c < nCh; ++c)
            for (int i = 0; i < n; ++i)
                chans[c][i] = dcBlock[c].process (chans[c][i]);
    }

    // =====================================================================
    //  670 variable-mu compressor / limiter
    // =====================================================================
    constexpr float k = 0.70710678f;
    for (int start = 0; start < n; start += subBlock)
    {
        const int len = juce::jmin (subBlock, n - start);
        for (auto* s : { &sCompIn, &sInGain[0], &sInGain[1], &sOutGain[0], &sOutGain[1], &sMix[0], &sMix[1] })
            step (s->cur, s->target);
        const float compMix = sCompIn.cur;

        for (int i = start; i < start + len; ++i)
        {
            float x[2] = { chans[0][i], stereo ? chans[1][i] : 0.0f };
            float d[2] = { x[0], x[1] };
            if (ms) { d[0] = (x[0] + x[1]) * k; d[1] = (x[0] - x[1]) * k; }   // Lat / Vert

            float s[2] = { 0, 0 }, g[2] = { 0, 0 };
            for (int c = 0; c < nCh; ++c)
            {
                s[c] = d[c] * sInGain[c].cur;
                g[c] = comp[c].smooth (comp[c].computeTarget (s[c]));
            }
            if (link) g[0] = g[1] = juce::jmax (g[0], g[1]);

            for (int c = 0; c < nCh; ++c)
            {
                grMax[c] = juce::jmax (grMax[c], g[c] * compMix);
                const float wet = s[c] * dbToGain (-g[c]);
                const float mixed = (s[c] + (wet - s[c]) * sMix[c].cur) * sOutGain[c].cur;
                d[c] += (mixed - d[c]) * compMix;
            }

            if (ms) { x[0] = (d[0] + d[1]) * k; x[1] = (d[0] - d[1]) * k; }
            else    { x[0] = d[0]; x[1] = d[1]; }

            chans[0][i] = x[0];
            if (stereo) chans[1][i] = x[1];
        }
    }

    // =====================================================================
    //  latency compensated global bypass
    // =====================================================================
    {
        const int dl = dryDelay.getNumSamples();
        const float b0 = sBypass.cur;
        sBypass.cur += (sBypass.target - sBypass.cur) * juce::jmin (1.0f, a * (float) n / (float) subBlock);
        const float b1 = sBypass.cur;
        int pos = dryDelayPos;

        for (int i = 0; i < n; ++i)
        {
            const int rd = (pos + 1) % dl;          // oldest sample = latency samples ago
            const float f = b0 + (b1 - b0) * ((float) i / (float) n);
            for (int c = 0; c < nCh; ++c)
            {
                float* ring = dryDelay.getWritePointer (c);
                ring[pos] = dryCopy.getSample (c, i);
                const float dry = latency > 0 ? ring[rd] : ring[pos];
                chans[c][i] += (dry - chans[c][i]) * f;
            }
            pos = rd;
        }
        dryDelayPos = pos;
    }

    // SOLO M / SOLO S monitoring
    if (stereo)
    {
        const bool sm = soloMid->load() > 0.5f, ss = soloSide->load() > 0.5f;
        if (sm != ss)
            for (int i = 0; i < n; ++i)
            {
                const float m = 0.5f * (chans[0][i] + chans[1][i]), s = 0.5f * (chans[0][i] - chans[1][i]);
                chans[0][i] = sm ? m : s;
                chans[1][i] = sm ? m : -s;
            }
    }

    for (int c = 0; c < nCh; ++c)
        for (int i = 0; i < n; ++i) outSum[c] += std::abs ((double) chans[c][i]);
}

juce::AudioProcessorEditor* SA670AudioProcessor::createEditor() { return new SA670AudioProcessorEditor (*this); }

void SA670AudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void SA670AudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new SA670AudioProcessor(); }
