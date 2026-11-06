#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <memory>
#include "Compressor.h"
#include "Presets.h"

// ===========================================================================
//  Cohesion by Pepperwood - master-bus "glue" processor.
//  Chain:  EQ shelves  ->  bus compressor  ->  limiter.
//  "Magic" engages the whole chain; "Amount" is a macro that pushes it.
// ===========================================================================
class CohesionAudioProcessor : public juce::AudioProcessor
{
public:
    CohesionAudioProcessor();
    ~CohesionAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion Glue"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int) override;
    const juce::String getProgramName (int) override;
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // --- shared state ---
    juce::AudioProcessorValueTreeState apvts;

    float getCompGainReductionDb()  const { return compGR.load(); }
    float getLimitGainReductionDb() const { return limitGR.load(); }
    float getInputLevelDb()         const { return inLevelDb.load(); }
    float getOutputLevelDb()        const { return outLevelDb.load(); }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void applyProgram (int index);

    int currentProgram = 0;

    cohesion::Biquad        hpFilter, lpFilter;   // cleanup HPF / LPF (main path)
    cohesion::Biquad        lowShelf, highShelf;
    cohesion::BusCompressor comp;
    cohesion::Limiter       limiter;
    cohesion::OnePoleHighpass monHpf;   // detection high-pass for SC Listen monitoring

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;

    juce::SmoothedValue<float> thrSm, makeupSm, mixSm, inGainSm, outGainSm;

    std::atomic<float> compGR    { 0.0f };
    std::atomic<float> limitGR   { 0.0f };
    std::atomic<float> inLevelDb  { -100.0f };
    std::atomic<float> outLevelDb { -100.0f };

    double currentSampleRate = 44100.0;

    // fixed shelf frequencies for the musical "weight" / "air" bands
    static constexpr float kLowShelfHz  = 120.0f;
    static constexpr float kHighShelfHz = 9000.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionAudioProcessor)
};
