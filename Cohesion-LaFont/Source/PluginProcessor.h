#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "FontEngine.h"

namespace fpid
{
    inline juce::String part (int p, const char* what) { return "p" + juce::String (p + 1) + "_" + what; }
    static const char* master = "master";
}

class LaFontProcessor : public juce::AudioProcessor
{
public:
    static constexpr int kParts = FontEngine::kParts;

    LaFontProcessor();
    ~LaFontProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Cohesion LA Font"; }
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

    // ---- for the editor (message thread) ----
    static bool engineAvailable() { return FontEngine::available(); }
    static juce::String engineStatus() { return FontEngine::unavailableReason(); }
    juce::String partFont (int part) const { return fonts[part]; }
    std::shared_ptr<const sf2::Info> partInfo (int part) const { return fonts[part].isNotEmpty() ? sf2::get (fonts[part]) : nullptr; }
    int partBank (int part) const    { return engine.partBank (part); }
    int partProgram (int part) const { return engine.partProgram (part); }
    FontEngine::State partState (int part) const { return engine.partState (part); }
    juce::String partPresetName (int part) const;
    void setPartFont (int part, const juce::String& path, bool keepProgram = false);
    void setFontForAllParts (const juce::String& path);
    void selectPreset (int part, int bank, int program);
    void stepPreset (int part, int delta);
    int voices (int part) const { return engine.voices (part); }
    int totalVoices() const     { return engine.totalVoices(); }
    float takePartActivity (int part) { return activity[part].exchange (0.0f); }
    float takeMidiActivity()          { return midiActivity.exchange (0.0f); }
    void requestReset() { resetRequest = true; }
    void requestPanic() { panicRequest = true; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static BusesProperties makeBuses();
    void handleMidi (const juce::MidiMessage& m);
    void renderEngine (int from, int to);
    void updateEffects();

    FontEngine engine;
    juce::String fonts[kParts];                 // the SoundFont of each part (message thread)
    double sr = 44100.0;
    int blockMax = 512;
    std::vector<float> buf;                     // 16 parts x 6 channels (dry, reverb, chorus) x blockMax
    float* pb (int part, int ch) { return buf.data() + (size_t) ((part * 6 + ch) * blockMax); }
    float rvGain[kParts], choGain[kParts];
    float lastFx[kParts][8];

    struct PartParams
    {
        std::atomic<float> *ch = nullptr, *lvl = nullptr, *pan = nullptr, *mute = nullptr, *solo = nullptr, *out = nullptr;
        std::atomic<float> *rvOn = nullptr, *rvRoom = nullptr, *rvDamp = nullptr, *rvWidth = nullptr, *rvLevel = nullptr;
        std::atomic<float> *choOn = nullptr, *choVoices = nullptr, *choLevel = nullptr, *choRate = nullptr, *choDepth = nullptr;
    } pp[kParts];
    std::atomic<float>* pMaster = nullptr;
    std::atomic<float> activity[kParts] {}, midiActivity { 0.0f };
    std::atomic<bool> resetRequest { false }, panicRequest { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LaFontProcessor)
};
