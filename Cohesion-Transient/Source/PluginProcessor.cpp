#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    struct Preset { const char* name; float attackDb, sustainDb, sens, outDb; };
    const Preset kPresets[] = {
        { "Flat",        0.0f,  0.0f, 50.0f,  0.0f },
        { "Punch Up",    4.0f, -2.0f, 65.0f,  1.5f },
        { "Snap",        6.0f, -3.0f, 80.0f,  0.0f },
        { "Soften",     -4.0f,  2.0f, 50.0f,  0.0f },
        { "Tame",       -3.0f, -2.0f, 60.0f,  0.0f },
        { "Fat",         1.0f,  4.0f, 55.0f, -1.0f },
        { "Drum Smack",  7.0f, -4.0f, 85.0f, -1.0f },
        { "Room Kill",   2.0f, -6.0f, 75.0f,  0.0f },
    };
    constexpr int kNumPresets = (int) (sizeof (kPresets) / sizeof (kPresets[0]));
}

CohesionTransientAudioProcessor::CohesionTransientAudioProcessor()
    : juce::AudioProcessor (BusesProperties()
          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout CohesionTransientAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "attack", 1 }, "Attack",
                NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sustain", 1 }, "Sustain",
                NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "sensitivity", 1 }, "Sensitivity",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 50.0f));
    layout.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
                NormalisableRange<float> (-24.0f, 24.0f, 0.1f), 0.0f));

    return layout;
}

void CohesionTransientAudioProcessor::setP (const juce::String& id, float v)
{
    if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (p->convertTo0to1 (v));
}

int CohesionTransientAudioProcessor::getNumPrograms() { return kNumPresets; }

const juce::String CohesionTransientAudioProcessor::getProgramName (int i)
{
    return (i >= 0 && i < kNumPresets) ? juce::String (kPresets[i].name) : juce::String();
}

void CohesionTransientAudioProcessor::setCurrentProgram (int index)
{
    currentProgram = juce::jlimit (0, kNumPresets - 1, index);
    const auto& p = kPresets[currentProgram];
    setP ("attack", p.attackDb);
    setP ("sustain", p.sustainDb);
    setP ("sensitivity", p.sens);
    setP ("output", p.outDb);
}

void CohesionTransientAudioProcessor::prepareToPlay (double fs, int)
{
    sampleRate = fs;
    shaper.prepare (fs);
    outSm.reset (fs, 0.02);
    scopeHop = juce::jmax (16, (int) (fs / (double) (kScope * 30)));
}

bool CohesionTransientAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void CohesionTransientAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh   = juce::jmin (buffer.getNumChannels(), 2);
    const int numSmps = buffer.getNumSamples();

    if (apvts.getRawParameterValue ("bypass")->load() > 0.5f)
        return;

    shaper.setParams (apvts.getRawParameterValue ("attack")->load(),
                      apvts.getRawParameterValue ("sustain")->load(),
                      apvts.getRawParameterValue ("sensitivity")->load() * 0.01f);
    outSm.setTargetValue (juce::Decibels::decibelsToGain (apvts.getRawParameterValue ("output")->load()));

    float* L = buffer.getWritePointer (0);
    float* R = numCh > 1 ? buffer.getWritePointer (1) : nullptr;
    float blockPeak = 0.0f;

    auto softclip = [] (float x)
    {
        const float t = 0.95f;
        if (x >  t) return  t + (1.0f - t) * std::tanh ((x - t) / (1.0f - t));
        if (x < -t) return -t + (1.0f - t) * std::tanh ((x + t) / (1.0f - t));
        return x;
    };

    for (int n = 0; n < numSmps; ++n)
    {
        const float inL = L[n];
        const float inR = R ? R[n] : L[n];
        const float level = juce::jmax (std::abs (inL), std::abs (inR));   // stereo-linked

        const float g  = shaper.processGain (level);   // transient gain (same for both)
        const float og = outSm.getNextValue();

        const float outL = softclip (inL * g * og);
        const float outR = softclip (inR * g * og);
        L[n] = outL;
        if (R) R[n] = outR;

        blockPeak = juce::jmax (blockPeak, juce::jmax (std::abs (outL), std::abs (outR)));

        // scope: input vs shaped peak envelopes
        inPk = juce::jmax (inPk, level);
        shPk = juce::jmax (shPk, level * g);
        if (++scopeCount >= scopeHop)
        {
            const int p = scopePos.load();
            scopeIn[p] = inPk; scopeShaped[p] = shPk;
            scopePos.store ((p + 1) % kScope);
            inPk = shPk = 0.0f; scopeCount = 0;
        }
    }

    outPeak.store (juce::jmax (outPeak.load() * 0.8f, blockPeak));
}

void CohesionTransientAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void CohesionTransientAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* CohesionTransientAudioProcessor::createEditor()
{
    return new CohesionTransientAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CohesionTransientAudioProcessor();
}
