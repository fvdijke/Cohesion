#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <unordered_set>
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
    bool         presetIsImported (int i) const { return i >= 0 && i < (int) presets.size() && (presets[(size_t) i].fm != nullptr || presets[(size_t) i].la != nullptr); }
    juce::String presetCategory (int i) const  { return (i >= 0 && i < (int) presets.size()) ? presets[(size_t) i].category : juce::String(); }
    void         loadPreset (int i);

    // --- FM engine (DX7 voice) ---
    void        setFmPatch (const dx7::Patch& p, const juce::String& bank);
    dx7::Patch  getFmPatch();
    juce::String getFmBank() const               { return fmBank; }
    int         getFmVersion() const             { return fmVersion.load(); }

    // --- SysEx import (banks are copied to the user folder and reloaded at start-up) ---
    struct ImportResult { int files = 0, banks = 0, voices = 0, blank = 0, duplicates = 0; juce::StringArray failed; juce::String firstCategory; };
    ImportResult importSysEx (const juce::Array<juce::File>& filesOrFolders);
    static juce::File userSysExDir();

    // --- LA engine (MT-32 / CM-32L ROM sets) ---
    void setLaTimbre (std::shared_ptr<const la32::Rom> rom, const la32::Timbre& t, const juce::String& bank);
    std::pair<std::shared_ptr<const la32::Rom>, la32::Timbre> getLaTimbre();
    juce::String getLaBank() const               { return laBank; }
    int          getLaVersion() const            { return laVersion.load(); }
    int          numRomSets() const              { return (int) romSets.size(); }
    struct RomImportResult { int sets = 0, duplicates = 0, files = 0; juce::StringArray failed; juce::String firstCategory; };
    RomImportResult importRoms (const juce::Array<juce::File>& filesFoldersOrZips);
    static juce::File userRomDir();

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
    void loadUserBanks();
    int  addBank (const juce::File& f, ImportResult& r);
    void restoreFmFromState();
    void loadUserRoms();
    bool addRomSet (std::shared_ptr<const la32::Rom> rom, RomImportResult* r);
    void restoreLaFromState();

    FmSource fmSource;
    LaSource laSource;
    std::atomic<int> laVersion { 0 };
    juce::String laBank;
    std::vector<std::shared_ptr<const la32::Rom>> romSets;
    std::atomic<int> fmVersion { 0 };
    juce::String fmBank;
    std::unordered_set<std::string> fmSeen;
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
