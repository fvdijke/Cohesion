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

    snapshot.fltType   = ip (pid::fltType);
    snapshot.fltCutoff = f  (pid::fltCutoff);
    snapshot.fltRes    = f  (pid::fltRes);
    snapshot.fltDrive  = f  (pid::fltDrive);
    snapshot.fltKeyTrk = f  (pid::fltKeyTrk);
    snapshot.fltEnvAmt = f  (pid::fltEnvAmt);

    snapshot.ampA = f (pid::ampA); snapshot.ampD = f (pid::ampD); snapshot.ampS = f (pid::ampS); snapshot.ampR = f (pid::ampR);
    snapshot.fenA = f (pid::fenA); snapshot.fenD = f (pid::fenD); snapshot.fenS = f (pid::fenS); snapshot.fenR = f (pid::fenR);
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
