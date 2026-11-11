// Checks every factory preset against the real parameter layout.
#include <juce_audio_processors/juce_audio_processors.h>
#include "../Source/Params.h"
#include "../Source/Presets.h"
#include <cstdio>

struct Dummy : juce::AudioProcessor
{
    juce::AudioProcessorValueTreeState apvts { *this, nullptr, "PARAMS", cohesionSynthLayout() };
    const juce::String getName() const override { return "dummy"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    double getTailLengthSeconds() const override { return 0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    Dummy d;
    int problems = 0, checked = 0;
    juce::StringArray cats;
    const auto presets = cohesionSynthPresets();
    for (auto& p : presets)
    {
        cats.addIfNotAlreadyThere (p.category);
        for (auto& kv : p.v)
        {
            ++checked;
            auto* prm = d.apvts.getParameter (kv.first);
            if (prm == nullptr) { std::printf ("  [FAIL] %s / %s: unknown parameter '%s'\n", p.category.toRawUTF8(), p.name.toRawUTF8(), kv.first.toRawUTF8()); ++problems; continue; }
            const auto& range = prm->getNormalisableRange();
            if (kv.second < range.start - 1.0e-4f || kv.second > range.end + 1.0e-4f)
            { std::printf ("  [FAIL] %s / %s: %s = %g outside [%g, %g]\n", p.category.toRawUTF8(), p.name.toRawUTF8(), kv.first.toRawUTF8(), kv.second, range.start, range.end); ++problems; }
        }
        applySynthPreset (d, d.apvts, p);   // must not assert / crash
    }
    std::printf ("%d presets, %d categories (%s), %d values checked\n", (int) presets.size(), cats.size(), cats.joinIntoString (", ").toRawUTF8(), checked);
    std::printf ("%s (%d problem%s)\n", problems == 0 ? "ALL PASSED" : "FAILURES", problems, problems == 1 ? "" : "s");
    return problems == 0 ? 0 : 1;
}
