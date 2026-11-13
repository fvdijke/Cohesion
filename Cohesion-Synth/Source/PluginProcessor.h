#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include "Params.h"
#include "SynthDsp.h"
#include "SynthVoice.h"
#include "Presets.h"
#include "FxDsp.h"
#include "ArpEngine.h"
#include "ArpPatterns.h"

// Reads a fixed sequence of parameters without looking them up: the first pass (in the
// constructor) records the value pointers, every later pass (audio thread) just loads them.
struct ParamSeq
{
    std::vector<std::atomic<float>*> ptr;
    struct Reader
    {
        juce::AudioProcessorValueTreeState& apvts;
        std::vector<std::atomic<float>*>& ptr;
        bool record;
        size_t k = 0;
        float f (juce::StringRef id)
        {
            if (record) { auto* a = apvts.getRawParameterValue (id); jassert (a != nullptr); ptr.push_back (a); return a->load(); }
            jassert (k < ptr.size());
            return ptr[k++]->load();
        }
        int  i (juce::StringRef id) { return (int) f (id); }
        bool b (juce::StringRef id) { return f (id) > 0.5f; }
    };
    Reader reader (juce::AudioProcessorValueTreeState& s) { return { s, ptr, ptr.empty() }; }
};

class CohesionSynthAudioProcessor : public juce::AudioProcessor,
                                    private juce::AsyncUpdater
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
    std::function<void()> onPresetChanged;    // editor: refresh the preset name (message thread)
    juce::String presetDisplayName (int i) const;
    juce::String presetName (int i) const      { return (i >= 0 && i < (int) presets.size()) ? presets[(size_t) i].name : juce::String(); }
    bool         presetIsImported (int i) const { return i >= 0 && i < (int) presets.size() && (presets[(size_t) i].fm != nullptr || presets[(size_t) i].la != nullptr); }
    juce::String presetCategory (int i) const  { return (i >= 0 && i < (int) presets.size()) ? presets[(size_t) i].category : juce::String(); }
    void         loadPreset (int i);

    // --- multi sounds: load only one part (pid::kVA / kFM / kLA) of a preset; the other parts,
    //     the MIXER, FX and ARP stay as they are ---
    bool         presetFitsPart (int i, int part) const;
    void         loadPresetIntoPart (int i, int part);
    juce::String partSoundName (int part) const;       // what is loaded in that part ("" = nothing)
    bool         isMulti() const                       { return (bool) apvts.state.getProperty ("multi", false); }
    juce::String headerName() const;                   // the preset name, or the parts of a multi sound

    // --- user presets: the whole sound (parameters, FX order, DX7 voice, LA timbre, ARP
    //     pattern) as one file per preset in userPresetDir(); category "User" ---
    static juce::File userPresetDir();
    static constexpr const char* kUserCategory = "User";
    bool presetIsUser (int i) const { return i >= 0 && i < (int) presets.size() && presets[(size_t) i].userFile != juce::File(); }
    juce::File presetUserFile (int i) const { return presetIsUser (i) ? presets[(size_t) i].userFile : juce::File(); }
    bool userPresetExists (const juce::String& name) const;
    enum { kSaveExists = -1, kSaveBadName = -2, kSaveFailed = -3 };
    // Saves the current sound; returns its preset index (now the current preset) or kSave*.
    int  saveUserPreset (const juce::String& name, bool overwrite);
    int  renameUserPreset (int i, const juce::String& newName);   // new index or kSave*
    bool deleteUserPreset (int i);                                  // moved to the recycle bin

    // --- FM engine (DX7 voice) ---
    void        setFmPatch (const dx7::Patch& p, const juce::String& bank);
    dx7::Patch  getFmPatch();
    juce::String getFmBank() const               { return fmBank; }
    int         getFmVersion() const             { return fmVersion.load(); }

    // --- SysEx import (banks are copied to the user folder and reloaded at start-up) ---
    struct ImportResult { int files = 0, banks = 0, voices = 0, blank = 0, duplicates = 0; juce::StringArray failed; juce::String firstCategory; };
    ImportResult importSysEx (const juce::Array<juce::File>& filesOrFolders);
    static juce::File userSysExDir();
    bool presetIsSysEx (int i) const   { return i >= 0 && i < (int) presets.size() && presets[(size_t) i].fm != nullptr; }
    // Removes an imported DX7 bank (its copy in the SysEx folder + its voices) from the library.
    // Pass an empty category to remove every imported bank. Returns the number of voices removed.
    int removeSysExBank (const juce::String& category);

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
    float getCompGainReductionDb() const        { return fxChain.compGrDb(); }
    // MIXER meters: peak of each part since the last call (post fader)
    float takePartPeak (int part)               { return partPeak[juce::jlimit (0, 2, part)].exchange (0.0f); }
    double getFxBpm() const                     { return fxBpm.load(); }
    arp::Engine& getArp()                       { return arpEngine; }
    float getLimiterGainReductionDb() const     { return fxChain.lim.grDb.load(); }
    float getOutputPeak() const                 { return fxChain.lim.outPk.load(); }

private:
    void updateSnapshot()  { fillSnapshot (snapshot); }
    void fillSnapshot (syn::Snapshot& out);
    std::pair<float, float> measureImportedLevel (bool drum, int onlyPart = -1);   // { MAIN, limiter DRIVE dB }
    std::pair<float, float> importedLevel (int presetIndex);                    // measured once per voice
    void setPartSoundName (int part, const juce::String& name);
    void setParam (const juce::String& id, float value);
    void updateFxParams();
    void syncFxOrderFromState();
    void updateArpParams();
    void loadUserBanks();
    void loadUserPresets();                       // (re)reads the "User" presets, keeps the current one
    bool applyUserPreset (const juce::File& f);
    int  addBank (const juce::File& f, ImportResult& r);
    void restoreFmFromState();
    void loadUserRoms();
    bool addRomSet (std::shared_ptr<const la32::Rom> rom, RomImportResult* r);
    void restoreLaFromState();
    void beginPresetSwitch();
    void endPresetSwitch();
    void hardStopVoices();
    void handleAsyncUpdate() override;            // MIDI program change -> GM preset (message thread)
    std::atomic<int> pendingProgram { -1 };
    int gmFirst = -1;                              // index of "001 Acoustic Grand Piano"
    void applySwitchFade (juce::AudioBuffer<float>& buffer, int n);

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
    juce::AudioBuffer<float> partBuf;              // the parts' buses (VA 0/1, FM 2/3, LA 4/5)
    juce::MidiBuffer playedNotes, routedMidi;      // ARP per part: played notes (ch 1) + ARP notes (ch 2)
    bool prevArpOn = false;
    float partGain[3][2] { { 1, 1 }, { 1, 1 }, { 1, 1 } };
    std::atomic<float> partPeak[3] { { 0.0f }, { 0.0f }, { 0.0f } };

    CohesionSynthesiser synth;
    syn::Snapshot snapshot;
    ParamSeq snapSeq, fxSeq, arpSeq;
    syn::LfoGen monoLfo[4];
    std::array<std::vector<float>, 4> monoLfoBuf;
    std::vector<SynthPreset> presets;
    int presetIndex = 0;
    void markCurrentPreset();
    juce::ValueTree completeState();   // apvts state with every parameter value written   // stores the current preset's name in the state (header after reopening)
    std::unordered_map<int, std::pair<float, float>> importedLevels;   // per preset index, measured once

    // Preset change without clicks: the message thread asks for a short fade-out, the audio
    // thread fades, stops every voice and clears the FX tails, the new preset is applied while
    // the output is silent, then the audio fades back in.
    enum { kRun = 0, kFadeOut, kSilent, kFadeIn };
    std::atomic<int> switchState { kRun };
    std::atomic<juce::uint32> lastBlockMs { 0 };
    float switchGain = 1.0f;
    double sampleRateHz = 44100.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CohesionSynthAudioProcessor)
};
