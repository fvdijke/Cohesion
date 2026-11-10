#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "TransientDsp.h"

// ===========================================================================
//  COHESION - TRANSIENT  (Pepperwood)
//  Four-control transient shaper: attack, sustain, sensitivity, output.
// ===========================================================================
class CohesionTransientAudioProcessor : public juce::AudioProcessor
{
public:
    CohesionTransientAudioProcessor();
    ~CohesionTransientAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion Transient"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    double getSampleRateHz() const noexcept { return sampleRate; }
    float getOutPeak() const noexcept { return outPeak.load(); }

    // scope feed for the editor: input + shaped peak envelopes
    static constexpr int kScope = 256;
    void readScope (float* inDst, float* shapedDst) const noexcept
    {
        const int p = scopePos.load();
        for (int i = 0; i < kScope; ++i)
        {
            inDst[i]     = scopeIn[(p + i) % kScope];
            shapedDst[i] = scopeShaped[(p + i) % kScope];
        }
    }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setP (const juce::String& id, float v);

    double sampleRate = 44100.0;
    int    currentProgram = 0;

    trs::Shaper shaper;                // stereo-linked: one detector, same gain on L and R
    juce::SmoothedValue<float> outSm;

    std::atomic<float> outPeak { 0.0f };
    float scopeIn[kScope] { 0.0f }, scopeShaped[kScope] { 0.0f };
    std::atomic<int> scopePos { 0 };
    float inPk = 0.0f, shPk = 0.0f;
    int   scopeCount = 0, scopeHop = 128;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionTransientAudioProcessor)
};
