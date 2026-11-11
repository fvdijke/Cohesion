#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "RomLibrary.h"
#include "FxDsp.h"
#include "MtMemory.h"
#include "LaPartEngine.h"
#include "MuntEngine.h"

namespace lpid
{
    inline juce::String part (int p, const char* what) { return "p" + juce::String (p + 1) + "_" + what; }
    static const char* engine = "engine", *master = "master";
    static const char* rvMode = "rv_mode", *rvTime = "rv_time", *rvLevel = "rv_level";
    static const char* ecSync = "ec_sync", *ecDiv = "ec_div", *ecTime = "ec_time", *ecFb = "ec_fb", *ecLevel = "ec_level";
}

class LaModuleProcessor : public juce::AudioProcessor,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::Timer
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
    romlib::ImportResult importRoms (const juce::Array<juce::File>& items);
    std::shared_ptr<const la32::Rom> currentRom() const { return mem.rom; }
    bool muntAvailable() const { return MuntEngine::available() && munt.isOpen(); }
    juce::String muntStatus() const;
    bool usingMunt() const { return activeIsMunt.load(); }

    juce::String partName (int part) const { return mem.partName (part); }
    int partProgram (int part) const { return mem.program[juce::jlimit (0, 8, part)]; }
    uint32_t memoryVersion() const { return mem.version.load(); }
    int activePartials() const { return partials.load(); }
    float takePartActivity (int part) { return activity[part].exchange (0.0f); }
    float takeMidiActivity()          { return midiActivity.exchange (0.0f); }

    void requestProgram (int part, int program);
    void requestReset();
    void requestPanic();
    int  loadSyx (const juce::File& f);           // number of MT-32 messages queued

    static constexpr int kAuxBuses = mt::kParts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    void parameterChanged (const juce::String& id, float v) override;
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
    void renderEngine (int from, int to);

    mt::Memory mem;
    LaPartEngine laEngine { mem };
    MuntEngine munt;
    ModuleEngine* active = nullptr;
    std::atomic<bool> activeIsMunt { false };
    bool needResync = false;
    juce::String romLabel;

    fx::Reverb reverb; fx::Echo tapDelay, echo;
    double sr = 44100.0;
    int blockMax = 512;
    std::vector<float> partBuf;                 // 9 parts x 4 channels x blockMax
    std::vector<float> busBuf;                  // reverb in L/R, echo in L/R, wet L/R, scratch L/R
    float* pb (int part, int ch) { return partBuf.data() + (size_t) ((part * 4 + ch) * blockMax); }

    int lastVolume[mt::kParts], lastPan[mt::kParts], lastExpr[mt::kParts];
    std::atomic<float>* pCh[mt::kParts] {}; std::atomic<float>* pLvl[mt::kParts] {}; std::atomic<float>* pPan[mt::kParts] {};
    std::atomic<float>* pRev[mt::kParts] {}; std::atomic<float>* pEch[mt::kParts] {}; std::atomic<float>* pMute[mt::kParts] {};
    std::atomic<float>* pSolo[mt::kParts] {}; std::atomic<float>* pOut[mt::kParts] {};
    std::atomic<float> *pEngine = nullptr, *pMaster = nullptr, *pEcSync = nullptr, *pEcDiv = nullptr, *pEcTime = nullptr, *pEcFb = nullptr, *pEcLevel = nullptr;
    std::atomic<int> partials { 0 };
    std::atomic<float> activity[mt::kParts] {}, midiActivity { 0.0f };
    std::atomic<bool> sysSync { false }, chanSync { false };
    bool applyingFromMemory = false;
    int chanFromSysex[mt::kParts] {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaModuleProcessor)
};
