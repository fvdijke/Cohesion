#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // choice tables
    const float kRatios  [] = { 2.0f, 4.0f, 10.0f };
    const float kAttacks [] = { 0.1f, 0.3f, 1.0f, 3.0f, 10.0f, 30.0f };   // ms
    const float kReleases[] = { 0.1f, 0.3f, 0.6f, 1.2f };                 // s (index 4 = Auto)
}

CohesionAudioProcessor::CohesionAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CohesionAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "magic", 1 }, "Magic", true));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "amount", 1 }, "Amount",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "inputGain", 1 }, "Input",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "outputGain", 1 }, "Output",
                                                       NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "threshold", 1 }, "Threshold",
                                                       NormalisableRange<float> (-40.0f, 10.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "ratio", 1 }, "Ratio",
                                                        StringArray { "2:1", "4:1", "10:1" }, 1));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "attack", 1 }, "Attack",
                                                        StringArray { "0.1 ms", "0.3 ms", "1 ms", "3 ms", "10 ms", "30 ms" }, 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "release", 1 }, "Release",
                                                        StringArray { "0.1 s", "0.3 s", "0.6 s", "1.2 s", "Auto" }, 4));
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { "knee", 1 }, "Knee",
                                                        StringArray { "Soft", "Hard" }, 0));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "makeup", 1 }, "Makeup",
                                                       NormalisableRange<float> (-10.0f, 20.0f, 0.1f), 0.0f));

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "scOn", 1 }, "SC HPF", false));
    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "scListen", 1 }, "SC Listen", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "scFreq", 1 }, "SC Freq",
                                                       NormalisableRange<float> (20.0f, 500.0f, 1.0f, 0.3f), 20.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "mix", 1 }, "Mix",
                                                       NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f));

    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "hpf", 1 }, "HPF",
                                                       NormalisableRange<float> (20.0f, 300.0f, 1.0f, 0.3f), 20.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "lpf", 1 }, "LPF",
                                                       NormalisableRange<float> (2000.0f, 20000.0f, 1.0f, 0.3f), 20000.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "low", 1 }, "Low",
                                                       NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "air", 1 }, "Air",
                                                       NormalisableRange<float> (-6.0f, 6.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "ceiling", 1 }, "Ceiling",
                                                       NormalisableRange<float> (-6.0f, 0.0f, 0.1f), -0.3f));

    // view-only: scrolling graph response (false = dynamic/peak, true = averaged)
    layout.add (std::make_unique<AudioParameterBool> (ParameterID { "avgView", 1 }, "Graph Avg", false));
    return layout;
}

void CohesionAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // 2x oversampling around the whole chain (anti-aliases the limiter).
    const int osChannels = juce::jmax (1, getTotalNumInputChannels());
    oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
        (size_t) osChannels, 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple);
    oversampler->initProcessing ((size_t) juce::jmax (1, samplesPerBlock));
    oversampler->reset();

    const double osRate = sampleRate * 2.0;
    currentSampleRate = osRate;

    comp.prepare (osRate);
    limiter.prepare (osRate);

    // total latency = oversampling FIR + limiter look-ahead (osRate -> base rate)
    const double limiterLatencyBase = limiter.getLatencySamples() * 0.5;
    setLatencySamples ((int) std::lround (oversampler->getLatencyInSamples() + limiterLatencyBase));
    monHpf.prepare (osRate);
    hpFilter.reset();
    lpFilter.reset();
    lowShelf.reset();
    highShelf.reset();

    thrSm.reset     (osRate, 0.02);
    makeupSm.reset  (osRate, 0.02);
    mixSm.reset     (osRate, 0.02);
    inGainSm.reset  (osRate, 0.02);
    outGainSm.reset (osRate, 0.02);
}

bool CohesionAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CohesionAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh   = juce::jmin (buffer.getNumChannels(), 2);
    const int numSmps = buffer.getNumSamples();

    // incoming level (pre-processing, peak across channels)
    float inPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* r = buffer.getReadPointer (ch);
        for (int n = 0; n < numSmps; ++n) inPeak = std::max (inPeak, std::fabs (r[n]));
    }

    const bool magic = apvts.getRawParameterValue ("magic")->load() > 0.5f;

    juce::dsp::AudioBlock<float> ioBlock (buffer);
    auto osBlock = oversampler->processSamplesUp (ioBlock);
    const int osN  = (int) osBlock.getNumSamples();
    const int osCh = juce::jmin ((int) osBlock.getNumChannels(), 2);

    if (magic)
    {
        const float amount   = apvts.getRawParameterValue ("amount")->load() * 0.01f;
        const float thr      = apvts.getRawParameterValue ("threshold")->load() - 10.0f * amount;
        const float makeupDb = apvts.getRawParameterValue ("makeup")->load()    +  6.0f * amount;
        const float lowDb    = apvts.getRawParameterValue ("low")->load()       +  3.0f * amount;
        const float airDb    = apvts.getRawParameterValue ("air")->load()       +  3.0f * amount;
        const float mix      = apvts.getRawParameterValue ("mix")->load() * 0.01f;
        const float inDb     = apvts.getRawParameterValue ("inputGain")->load();
        const float outDb    = apvts.getRawParameterValue ("outputGain")->load();
        const float hpfHz    = apvts.getRawParameterValue ("hpf")->load();
        const float lpfHz    = apvts.getRawParameterValue ("lpf")->load();

        const int  ratioIdx = (int) apvts.getRawParameterValue ("ratio")->load();
        const int  atkIdx   = (int) apvts.getRawParameterValue ("attack")->load();
        const int  relIdx   = (int) apvts.getRawParameterValue ("release")->load();
        const int  kneeIdx  = (int) apvts.getRawParameterValue ("knee")->load();
        const bool autoRel  = (relIdx == 4);

        const bool  scOn     = apvts.getRawParameterValue ("scOn")->load() > 0.5f;
        const bool  scListen = apvts.getRawParameterValue ("scListen")->load() > 0.5f;
        const float scFreq   = apvts.getRawParameterValue ("scFreq")->load();
        const float ceiling  = apvts.getRawParameterValue ("ceiling")->load();

        comp.setRatio (kRatios[ratioIdx]);
        comp.setKnee (kneeIdx == 1 ? 0.0f : cohesion::kKneeDb);
        comp.setSidechain (scFreq, scOn);
        comp.setTimes (kAttacks[atkIdx], autoRel ? 0.3f : kReleases[relIdx], autoRel);
        limiter.setCeiling (ceiling);
        monHpf.setFrequency (scFreq);
        hpFilter.setHighpass (hpfHz, currentSampleRate);
        lpFilter.setLowpass  (lpfHz, currentSampleRate);
        lowShelf.setLowShelf   (kLowShelfHz,  lowDb, currentSampleRate);
        highShelf.setHighShelf (kHighShelfHz, airDb, currentSampleRate);

        thrSm.setTargetValue (thr);
        makeupSm.setTargetValue (cohesion::dbToLin (makeupDb));
        mixSm.setTargetValue (mix);
        inGainSm.setTargetValue  (cohesion::dbToLin (inDb));
        outGainSm.setTargetValue (cohesion::dbToLin (outDb));

        float* chans[2] { nullptr, nullptr };
        for (int ch = 0; ch < osCh; ++ch)
            chans[ch] = osBlock.getChannelPointer ((size_t) ch);

        float frame[2];
        for (int n = 0; n < osN; ++n)
        {
            const float ig = inGainSm.getNextValue();
            for (int ch = 0; ch < osCh; ++ch) frame[ch] = chans[ch][n] * ig;

            // cleanup filters (HPF/LPF) + tone shelves
            for (int ch = 0; ch < osCh; ++ch)
            {
                float x = hpFilter.process (frame[ch], ch);
                x = lpFilter.process (x, ch);
                x = highShelf.process (lowShelf.process (x, ch), ch);
                frame[ch] = x;
            }

            const float og = outGainSm.getNextValue();

            if (scListen)
            {
                float sc = (osCh > 1) ? 0.5f * (frame[0] + frame[1]) : frame[0];
                const float det = scOn ? monHpf.process (sc) : sc;
                for (int ch = 0; ch < osCh; ++ch) chans[ch][n] = det;
                continue;
            }

            const float tCur  = thrSm.getNextValue();
            const float mkCur = makeupSm.getNextValue();
            const float mxCur = mixSm.getNextValue();
            comp.processFrame    (frame, osCh, tCur, mkCur, mxCur);
            limiter.processFrame (frame, osCh);

            for (int ch = 0; ch < osCh; ++ch) chans[ch][n] = frame[ch] * og;
        }

        if (scListen) { compGR.store (0.0f); limitGR.store (0.0f); }
        else          { compGR.store (comp.fetchGainReductionDb()); limitGR.store (limiter.fetchGainReductionDb()); }
    }
    else
    {
        compGR.store (0.0f);
        limitGR.store (0.0f);
    }

    oversampler->processSamplesDown (ioBlock);

    // outgoing level (post-processing)
    float outPeak = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* r = buffer.getReadPointer (ch);
        for (int n = 0; n < numSmps; ++n) outPeak = std::max (outPeak, std::fabs (r[n]));
    }
    inLevelDb.store  (cohesion::linToDb (inPeak));
    outLevelDb.store (cohesion::linToDb (outPeak));
}

int CohesionAudioProcessor::getNumPrograms()            { return (int) cohesion::presets().size(); }
int CohesionAudioProcessor::getCurrentProgram()         { return currentProgram; }

const juce::String CohesionAudioProcessor::getProgramName (int index)
{
    const auto& ps = cohesion::presets();
    if (juce::isPositiveAndBelow (index, (int) ps.size()))
        return ps[(size_t) index].name;
    return {};
}

void CohesionAudioProcessor::setCurrentProgram (int index)
{
    if (! juce::isPositiveAndBelow (index, (int) cohesion::presets().size()))
        return;
    currentProgram = index;
    applyProgram (index);
}

void CohesionAudioProcessor::applyProgram (int index)
{
    const auto& p = cohesion::presets()[(size_t) index];

    auto set = [this] (const char* id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    set ("magic",     p.magic ? 1.0f : 0.0f);
    set ("amount",    p.amount);
    set ("threshold", p.threshold);
    set ("ratio",     (float) p.ratio);
    set ("attack",    (float) p.attack);
    set ("release",   (float) p.release);
    set ("makeup",    p.makeup);
    set ("scOn",      p.scOn ? 1.0f : 0.0f);
    set ("scFreq",    p.scFreq);
    set ("mix",       p.mix);
    set ("low",       p.low);
    set ("air",       p.air);
    set ("ceiling",   p.ceiling);
    set ("knee",      (float) p.knee);
    set ("hpf",       p.hpf);
    set ("lpf",       p.lpf);
}

void CohesionAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CohesionAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CohesionAudioProcessor::createEditor()
{
    return new CohesionAudioProcessorEditor (*this);
}

// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionAudioProcessor();
}
