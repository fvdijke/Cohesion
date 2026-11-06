#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "AdtDsp.h"

// ===========================================================================
//  COHESION - ADT  (Pepperwood)
//  Artificial Double Tracking: modulated doubling, phase, stereo width.
// ===========================================================================
class CohesionADTAudioProcessor : public juce::AudioProcessor
{
public:
    CohesionADTAudioProcessor();
    ~CohesionADTAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion ADT"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.1; }

    // --- factory presets (also exposed as host programs) ---
    int getNumPrograms() override;
    int getCurrentProgram() override { return currentProgram; }
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    double getSampleRateHz() const noexcept { return sampleRate; }

    // scope feed for the editor (smoothed peak envelope of the input)
    static constexpr int kScope = 256;
    void readScope (float* dst) const noexcept
    { const int p = scopePos.load(); for (int i = 0; i < kScope; ++i) dst[i] = scope[(p + i) % kScope]; }

    float getOutPeak() const noexcept { return outPeak.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void setP (const juce::String& id, float v);

    double sampleRate = 44100.0;
    int    currentProgram = 0;

    adt::DelayBuf delayLine;
    adt::Lfo    lfo[2];
    adt::AllPass1 apL, apR, apL2, apR2;
    adt::OnePole sideHP, vintL, vintR;

    juce::SmoothedValue<float> mixSm, widthSm, outSm, delaySm, spreadSm, depthSm;

    float scope[kScope] { 0.0f };
    std::atomic<int> scopePos { 0 };
    std::atomic<float> outPeak { 0.0f };
    float scopePeak = 0.0f;
    int   scopeCount = 0, scopeHop = 128;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionADTAudioProcessor)
};
