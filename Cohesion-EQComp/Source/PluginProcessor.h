#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <algorithm>
#include "EqDsp.h"
#include "MultibandDsp.h"
#include "Limiter.h"

// ===========================================================================
//  COHESION - EQ/COMP  (Pepperwood)
//  Foundation + high-quality parametric EQ (TPT/ZDF SVF).
//  Multiband dynamics and the Assist engine come next.
// ===========================================================================
class CohesionEQCompAudioProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kNumBands = 6;

    CohesionEQCompAudioProcessor();
    ~CohesionEQCompAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion EQ-Comp"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // --- spectrum analyser feed (for the EQ display) ---
    static constexpr int fftOrder = 12;
    static constexpr int fftSize  = 1 << fftOrder;   // 4096 (finer low-frequency bins)
    void pushAnalysisSample (float s) noexcept
    {
        if (fifoIndex == fftSize)
        {
            if (! fftReady.load()) { std::copy (fifo, fifo + fftSize, fftBlock); fftReady.store (true); }
            fifoIndex = 0;
        }
        fifo[fifoIndex++] = s;
    }
    bool isFftBlockReady() const noexcept { return fftReady.load(); }
    void readFftBlock (float* dest) noexcept { std::copy (fftBlock, fftBlock + fftSize, dest); fftReady.store (false); }
    double getSampleRateHz() const noexcept { return sampleRate; }

    // --- pre (input) analyser feed, for before/after display ---
    void pushAnalysisSamplePre (float s) noexcept
    {
        if (fifoIndexPre == fftSize)
        {
            if (! fftReadyPre.load()) { std::copy (fifoPre, fifoPre + fftSize, fftBlockPre); fftReadyPre.store (true); }
            fifoIndexPre = 0;
        }
        fifoPre[fifoIndexPre++] = s;
    }
    bool isFftBlockReadyPre() const noexcept { return fftReadyPre.load(); }
    void readFftBlockPre (float* dest) noexcept { std::copy (fftBlockPre, fftBlockPre + fftSize, dest); fftReadyPre.store (false); }

    float getBandGainReductionDb (int b) const noexcept
    { return (b >= 0 && b < ceqc::Multiband::kMaxBands) ? bandGaindB[b].load() : 0.0f; }

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    double sampleRate = 44100.0;

    float fifo[fftSize] { 0.0f };
    float fftBlock[fftSize] { 0.0f };
    int   fifoIndex = 0;
    std::atomic<bool> fftReady { false };

    float fifoPre[fftSize] { 0.0f };
    float fftBlockPre[fftSize] { 0.0f };
    int   fifoIndexPre = 0;
    std::atomic<bool> fftReadyPre { false };
    std::atomic<float> bandGaindB[ceqc::Multiband::kMaxBands];

    ceqc::SVF hp[2], lp[2];
    ceqc::SVF band[kNumBands][2];
    ceqc::Multiband multiband;
    ceqc::Limiter limiter;

    juce::SmoothedValue<float> inGainSm, outGainSm;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionEQCompAudioProcessor)
};
