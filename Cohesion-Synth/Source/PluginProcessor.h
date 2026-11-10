#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Params.h"
#include "SynthDsp.h"
#include "SynthVoice.h"

class CohesionSynthAudioProcessor : public juce::AudioProcessor
{
public:
    CohesionSynthAudioProcessor();
    ~CohesionSynthAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion Synth"; }
    bool acceptsMidi() const override  { return true; }
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

    // MIDI activity flag for the on-screen indicator (editor polls + clears).
    std::atomic<int> midiActivity { 0 };

private:
    void updateSnapshot();

    juce::Synthesiser synth;
    syn::Snapshot snapshot;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionSynthAudioProcessor)
};
