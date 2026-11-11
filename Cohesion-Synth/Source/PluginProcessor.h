#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include "Params.h"
#include "SynthDsp.h"
#include "SynthVoice.h"
#include "Presets.h"
#include "FxDsp.h"
#include "ArpEngine.h"
#include "ArpPatterns.h"

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
    double getTailLengthSeconds() const override { return 12.0; }   // reverb / echo tails

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

    // --- factory presets ---
    int          numPresets() const          { return (int) presets.size(); }
    int          currentPresetIndex() const  { return presetIndex; }
    juce::String presetDisplayName (int i) const;
    juce::String presetName (int i) const      { return (i >= 0 && i < (int) presets.size()) ? presets[(size_t) i].name : juce::String(); }
    juce::String presetCategory (int i) const  { return (i >= 0 && i < (int) presets.size()) ? presets[(size_t) i].category : juce::String(); }
    void         loadPreset (int i);

    // --- FX chain order (0 COMP, 1 FLANGER, 2 PHASER, 3 ECHO, 4 REVERB) ---
    void  getFxOrder (int* out5) const;
    void  setFxOrder (const int* in5);          // message thread; stored in the plugin state
    float getCompGainReductionDb() const        { return fxChain.comp.grDb.load(); }
    double getFxBpm() const                     { return fxBpm.load(); }
    arp::Engine& getArp()                       { return arpEngine; }
    float getLimiterGainReductionDb() const     { return fxChain.lim.grDb.load(); }
    float getOutputPeak() const                 { return fxChain.lim.outPk.load(); }

private:
    void updateSnapshot();
    void updateFxParams();
    void syncFxOrderFromState();
    void updateArpParams();

    fx::FxChain  fxChain;
    arp::Engine  arpEngine;
    arp::ArpParams arpp;
    double lastNoteHz = 0.0;
    fx::FxParams fxp;
    std::atomic<int> fxOrder[fx::kNumFx] { {0}, {1}, {2}, {3}, {4} };
    std::atomic<double> fxBpm { 120.0 };
    juce::AudioBuffer<float> monoScratch;

    juce::Synthesiser synth;
    syn::Snapshot snapshot;
    std::vector<SynthPreset> presets;
    int presetIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionSynthAudioProcessor)
};
