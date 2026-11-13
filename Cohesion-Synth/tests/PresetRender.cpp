// Plays every factory preset through the real synth: checks for silence, NaN and
// excessive level, and writes Source/PresetGains.h (MAIN level per preset) so the
// whole library plays back at a matched loudness.
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <algorithm>
#include <map>

static const double SR = 48000.0;

struct Spec { std::vector<int> notes; double hold; };
static Spec specFor (juce::String cat)
{
    // General MIDI families -> the closest factory category
    if (cat.startsWith ("GM"))
    {
        if (cat.contains ("Bass"))                                         return { { 36 }, 1.5 };
        if (cat.contains ("Lead") || cat.contains ("Reed") || cat.contains ("Brass") || cat.contains ("Pipe")) return { { 60 }, 1.5 };
        if (cat.contains ("Effects"))                                      return { { 48 }, 4.0 };
        if (cat.contains ("Pad") || cat.contains ("Ensemble") || cat.contains ("Strings")) return { { 48, 52, 55 }, 3.0 };
        if (cat.contains ("Percussive"))                                   return { { 48 }, 1.0 };
        return { { 48, 52, 55 }, 1.5 };
    }
    if (cat == "Bass")  return { { 36 }, 1.5 };
    if (cat == "Lead")  return { { 60 }, 1.5 };
    if (cat == "Flute") return { { 72 }, 1.5 };
    if (cat == "Drums") return { { 48 }, 0.3 };
    if (cat == "FX")    return { { 48 }, 4.0 };
    if (cat == "Pad" || cat == "Strings" || cat == "Arp") return { { 48, 52, 55 }, 3.0 };
    return { { 48, 52, 55 }, 1.5 };
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    // never read the user's own SysEx banks / ROM sets: empty throw-away folders
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion_presetrender");
    tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_SYSEX_DIR", tmp.getChildFile ("sysex").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_PRESET_DIR", tmp.getChildFile ("presets").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_ROM_DIR",   tmp.getChildFile ("roms").getFullPathName().toRawUTF8());
    const juce::File gainsFile (argc > 1 ? juce::String (argv[1]) : juce::String());
    const auto& presets = cohesionSynthPresets();
    const float target = -18.0f;

    juce::String table;
    int silent = 0, nan = 0, quiet = 0, n = 0;
    std::vector<float> devs;
    std::map<juce::String, std::vector<float>> catDev;
    for (int i = 0; i < (int) presets.size(); ++i)
    {
        const auto& pr = presets[(size_t) i];
        if (pr.category == "Init") continue;
        CohesionSynthAudioProcessor proc;
        proc.loadPreset (i);
        proc.prepareToPlay (SR, 512);
        const float curMain  = proc.apvts.getRawParameterValue (pid::mainVol)->load();
        const float curDrive = proc.apvts.getRawParameterValue (pid::limDrive)->load();

        const auto sp = specFor (pr.category);
        const int total = (int) ((sp.hold + 1.5) * SR), holdS = (int) (sp.hold * SR);
        juce::AudioBuffer<float> buf (2, 512);
        std::vector<float> mono; mono.reserve ((size_t) total);
        bool bad = false; float peak = 0.0f;
        for (int s = 0; s < total; s += 512)
        {
            juce::MidiBuffer mb;
            if (s == 0) for (int nn : sp.notes) mb.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
            if (s <= holdS && holdS < s + 512) for (int nn : sp.notes) mb.addEvent (juce::MidiMessage::noteOff (1, nn), holdS - s);
            buf.clear();
            proc.processBlock (buf, mb);
            for (int k = 0; k < 512; ++k)
            {
                const float l = buf.getSample (0, k), r = buf.getSample (1, k);
                if (! std::isfinite (l) || ! std::isfinite (r)) bad = true;
                peak = std::max (peak, std::max (std::abs (l), std::abs (r)));
                mono.push_back (0.5f * (l + r));
            }
        }
        // loudness: drums = loudest 20 ms window; everything else = 90th percentile of
        // 50 ms RMS windows over the note + 0.3 s (robust for both pads and plucks)
        const bool drum = pr.category == "Drums";
        const int win = (int) ((drum ? 0.05 : 0.3) * SR), upto = std::min ((int) mono.size(), holdS + (int) (0.3 * SR));
        std::vector<float> w;
        for (int s = 0; s + win <= upto; s += (int) (0.025 * SR))
        {
            double e = 0; for (int k = 0; k < win; ++k) e += (double) mono[(size_t) (s + k)] * mono[(size_t) (s + k)];
            w.push_back ((float) std::sqrt (e / win));
        }
        std::sort (w.begin(), w.end());
        const float loud = w.empty() ? 0.0f : w.back();   // momentary-max loudness
        const float loudDb = juce::Decibels::gainToDecibels (loud, -100.0f);
        const float tgt = drum ? target + 4.0f : target;   // a drum hit's loudest 50 ms sits higher

        const char* flag = "";
        if (bad) { flag = "NaN"; ++nan; }
        else if (loudDb < -60.0f) { flag = "SILENT"; ++silent; }

        // new total gain = current (MAIN + limiter DRIVE) + correction; MAIN first, the rest via DRIVE
        const float totalDb = juce::Decibels::gainToDecibels (curMain, -100.0f) + curDrive + (tgt - loudDb);
        float level = 1.0f, drive = 0.0f;
        if (totalDb <= 0.0f) level = juce::jmax (0.05f, juce::Decibels::decibelsToGain (totalDb));
        else                 drive = totalDb;
        if (drive > 18.0f) { if (! bad && loudDb >= -60.0f) { flag = "QUIET"; ++quiet; } drive = 18.0f; }

        devs.push_back (loudDb - tgt);
        catDev[pr.category].push_back (loudDb - tgt);
        if (*flag) std::printf ("  [%-6s] %-8s %-28s loud %6.1f dB  peak %5.2f\n", flag, pr.category.toRawUTF8(), pr.name.toRawUTF8(), loudDb, peak);
        table << "    { \"" << pr.category << "/" << pr.name << "\", " << juce::String (level, 3) << "f, " << juce::String (drive, 2) << "f },\n";
        ++n;
    }

    std::sort (devs.begin(), devs.end());
    auto pct = [&] (double q) { return devs.empty() ? 0.0f : devs[(size_t) std::min ((double) devs.size() - 1, q * (double) devs.size())]; };
    std::printf ("\n%d presets rendered | silent %d | NaN %d | too quiet even with +18 dB drive %d\n", n, silent, nan, quiet);
    std::printf ("loudness vs target:  p5 %+.1f  median %+.1f  p95 %+.1f dB\n", pct (0.05), pct (0.5), pct (0.95));
    for (auto& [c, d] : catDev)
    {
        std::sort (d.begin(), d.end());
        std::printf ("   %-8s min %+5.1f  median %+5.1f  max %+5.1f dB\n", c.toRawUTF8(), d.front(), d[d.size() / 2], d.back());
    }

    if (gainsFile != juce::File())
    {
        gainsFile.replaceWithText ("#pragma once\n// Generated by tests/PresetRender (loudness matching). Do not edit by hand.\n"
                                   "// { \"Category/Name\", MAIN level, limiter DRIVE dB }\n"
                                   "static const struct { const char* key; float level, drive; } kPresetLevels[] = {\n"
                                   + table + "    { \"\", 0.0f, 0.0f }\n};\n");
        std::printf ("wrote %s\n", gainsFile.getFullPathName().toRawUTF8());
    }
    return (silent + nan) == 0 ? 0 : 1;
}
