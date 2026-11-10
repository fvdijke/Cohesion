#include "PluginProcessor.h"
#include "PluginEditor.h"

CohesionSynthAudioProcessor::CohesionSynthAudioProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", cohesionSynthLayout())
{
    synth.addSound (new SynthSound());
    for (int i = 0; i < 16; ++i)
    {
        auto* v = new SynthVoice();
        v->setSnapshot (&snapshot);
        synth.addVoice (v);
    }
    presets = cohesionSynthPresets();
}

juce::String CohesionSynthAudioProcessor::presetDisplayName (int i) const
{
    if (i < 0 || i >= (int) presets.size()) return {};
    return presets[(size_t) i].category + "  —  " + presets[(size_t) i].name;
}

void CohesionSynthAudioProcessor::loadPreset (int i)
{
    if (presets.empty()) return;
    const int n = (int) presets.size();
    presetIndex = ((i % n) + n) % n;
    applySynthPreset (*this, apvts, presets[(size_t) presetIndex]);
}

void CohesionSynthAudioProcessor::prepareToPlay (double sampleRate, int)
{
    synth.setCurrentPlaybackSampleRate (sampleRate);
    updateSnapshot();
}

bool CohesionSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::stereo() || out == juce::AudioChannelSet::mono();
}

void CohesionSynthAudioProcessor::updateSnapshot()
{
    auto f  = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load(); };
    auto ip = [this] (const juce::String& id) { return (int) apvts.getRawParameterValue (id)->load(); };
    auto bp = [this] (const juce::String& id) { return apvts.getRawParameterValue (id)->load() > 0.5f; };

    for (int i = 0; i < 4; ++i)
    {
        auto& o = snapshot.osc[i];
        const int n = i + 1;
        o.on     = bp (pid::oscOn (n));
        o.mode   = ip (pid::oscMode (n));
        o.wave   = ip (pid::oscWave (n));
        o.oct    = ip (pid::oscOct (n));
        o.semi   = ip (pid::oscSemi (n));
        o.fine   = f  (pid::oscFine (n));
        o.wtpos  = f  (pid::oscWtPos (n));
        o.unison = ip (pid::oscUni (n));
        o.detune = f  (pid::oscDet (n));
        o.pan    = f  (pid::oscPan (n));
        o.level  = f  (pid::oscLevel (n));
        o.phase  = f  (pid::oscPhase (n));
    }

    snapshot.subOn      = bp (pid::subOn);
    snapshot.subWave    = ip (pid::subWave);
    snapshot.subOct     = ip (pid::subOct);
    snapshot.subLevel   = f  (pid::subLevel);
    snapshot.subPan     = f  (pid::subPan);
    snapshot.noiseOn    = bp (pid::noiseOn);
    snapshot.noiseType  = ip (pid::noiseType);
    snapshot.noiseLevel = f  (pid::noiseLevel);
    snapshot.noisePan   = f  (pid::noisePan);
    snapshot.noiseTilt  = f  (pid::noiseTilt);

    snapshot.fmAmt   = f (pid::fmAmt);
    snapshot.ringAmt = f (pid::ringAmt);

    snapshot.fltType    = ip (pid::fltType);
    snapshot.fltCutoff  = f  (pid::fltCutoff);
    snapshot.fltRes     = f  (pid::fltRes);
    snapshot.fltDrive   = f  (pid::fltDrive);
    snapshot.fltKeyTrk  = f  (pid::fltKeyTrk);
    snapshot.fltEnvAmt  = f  (pid::fltEnvAmt);
    snapshot.flt1EnvSrc = ip (pid::flt1EnvSrc);

    snapshot.flt2Type   = ip (pid::flt2Type);
    snapshot.flt2Cutoff = f  (pid::flt2Cutoff);
    snapshot.flt2Res    = f  (pid::flt2Res);
    snapshot.flt2Drive  = f  (pid::flt2Drive);
    snapshot.flt2KeyTrk = f  (pid::flt2KeyTrk);
    snapshot.flt2EnvAmt = f  (pid::flt2EnvAmt);
    snapshot.flt2Slope  = ip (pid::flt2Slope);
    snapshot.flt2EnvSrc = ip (pid::flt2EnvSrc);

    snapshot.routing      = ip (pid::fltRouting);
    snapshot.routeBalance = f  (pid::fltBalance);

    snapshot.env[0] = { f (pid::ampA), f (pid::ampD), f (pid::ampS), f (pid::ampR) };
    for (int n = 2; n <= 4; ++n)
        snapshot.env[n - 1] = { f (pid::envA (n)), f (pid::envD (n)), f (pid::envS (n)), f (pid::envR (n)) };

    for (int n = 1; n <= 4; ++n)
    {
        auto& l = snapshot.lfo[n - 1];
        l.wave   = ip (pid::lfoWave (n));
        l.rateHz = f  (pid::lfoRate (n));
        l.sync   = bp (pid::lfoSync (n));
        l.div    = ip (pid::lfoDiv (n));
        l.fade   = f  (pid::lfoFade (n));
        l.phase  = f  (pid::lfoPhase (n));
        l.smooth = f  (pid::lfoSmooth (n));
        l.mono   = bp (pid::lfoMono (n));
    }

    for (int s = 0; s < syn::kNumMod; ++s)
        snapshot.mod[s] = { ip (pid::modSrc (s)), ip (pid::modDst (s)), f (pid::modAmt (s)) };

    snapshot.mainVol = f (pid::mainVol);
}

void CohesionSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    for (const auto meta : midi)
        if (meta.getMessage().isNoteOn()) { midiActivity.store (1); break; }

    updateSnapshot();
    synth.renderNextBlock (buffer, midi, 0, buffer.getNumSamples());
    buffer.applyGain (snapshot.mainVol);
}

juce::AudioProcessorEditor* CohesionSynthAudioProcessor::createEditor()
{
    return new CohesionSynthAudioProcessorEditor (*this);
}

void CohesionSynthAudioProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void CohesionSynthAudioProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionSynthAudioProcessor();
}
