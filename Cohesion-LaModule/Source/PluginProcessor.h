#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RomLibrary.h"
#include "FxDsp.h"
#include "MtMemory.h"
#include "MuntEngine.h"

namespace lpid
{
    inline juce::String part (int p, const char* what) { return "p" + juce::String (p + 1) + "_" + what; }
    static const char* master = "master";
}

class LaModuleProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    LaModuleProcessor();
    ~LaModuleProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Cohesion LA Module"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- for the editor ----
    juce::StringArray romLabels() const;
    juce::String currentRomLabel() const { return romLabel; }
    bool setRom (const juce::String& label, bool keepMemory = false);
    // adds ROM sets from files / folders / zips; sets Munt can't play are not kept (`rejected`)
    struct RomImport { romlib::ImportResult r; juce::StringArray added, rejected; };
    RomImport importRoms (const juce::Array<juce::File>& items, bool singleFiles = false);
    bool romSupported (const juce::String& label);        // Munt can play this set (cached)
    bool removeRom (const juce::String& label);            // deletes the stored copy of a set
    int  removeUnsupportedRoms();
    std::shared_ptr<const la32::Rom> currentRom() const { return mem.rom; }
    bool muntReady() const { return munt.isOpen(); }
    juce::String muntStatus() const;

    juce::String partName (int part) const { return mem.partName (part); }
    int partProgram (int part) const { return mem.program[juce::jlimit (0, 8, part)]; }
    int activePartials() const { return munt.isOpen() ? munt.activePartials() : 0; }              // all parts
    int partPartials (int part) const { return munt.isOpen() ? munt.partPartials (part) : 0; }      // one part (of its own 32)
    float takePartActivity (int part) { return activity[part].exchange (0.0f); }
    float takeMidiActivity()          { return midiActivity.exchange (0.0f); }

    void requestProgram (int part, int program);
    void requestReset();
    void hardReset();                              // power cycle: re-read the ROM list and reload the set
    void requestPanic();
    int  loadSyx (const juce::File& f);           // number of MT-32 messages queued

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    void timerCallback() override;

    // commands from the UI to the audio thread
    struct Cmd { int kind = 0, part = 0, value = 0, len = 0; uint8_t data[264] {}; };
    enum { cmdSysex = 1, cmdProgram, cmdReset, cmdPanic };
    juce::AbstractFifo fifo { 2048 };
    std::vector<Cmd> cmds = std::vector<Cmd> (2048);
    bool push (const Cmd& c);
    void pushSysex (const std::vector<uint8_t>& s);

    void handleMidi (const juce::MidiMessage& m);
    void applySysex (const uint8_t* d, int n, bool fromMidi);
    void resyncMunt();
    void sendPartReverbs (bool force);
    void renderEngine (int from, int to);

    mt::Memory mem;
    MuntEngine munt;
    bool needResync = false;
    juce::String romLabel;
    std::map<juce::String, bool> supportCache;

    fx::Echo echo[mt::kParts];
    double sr = 44100.0;
    int blockMax = 512;
    std::vector<float> partBuf;                 // 9 parts x 4 channels (dry L/R, reverb wet L/R) x blockMax
    std::vector<float> scratch;                 // echo L/R
    float* pb (int part, int ch) { return partBuf.data() + (size_t) ((part * 4 + ch) * blockMax); }

    int lastVolume[mt::kParts], lastPan[mt::kParts], lastExpr[mt::kParts];
    int sentReverb[mt::kParts][3];
    float rvGain[mt::kParts], ecGain[mt::kParts];

    struct PartParams
    {
        std::atomic<float> *ch = nullptr, *lvl = nullptr, *pan = nullptr, *mute = nullptr, *solo = nullptr, *out = nullptr;
        std::atomic<float> *rvOn = nullptr, *rvMode = nullptr, *rvTime = nullptr, *rvLevel = nullptr;
        std::atomic<float> *ecOn = nullptr, *ecSync = nullptr, *ecDiv = nullptr, *ecTime = nullptr, *ecFb = nullptr, *ecLevel = nullptr;
    } pp[mt::kParts];
    std::atomic<float>* pMaster = nullptr;

    std::atomic<int> partials { 0 };
    std::atomic<float> activity[mt::kParts] {}, midiActivity { 0.0f };
    std::atomic<bool> sysSync { false }, chanSync { false };
    int chanFromSysex[mt::kParts] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaModuleProcessor)
};
