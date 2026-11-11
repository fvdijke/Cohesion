// Offline checks for the LA (MT-32 / CM-32L) engine. Uses the user's own ROM dumps.
//   LaTest <control.rom> <pcm.rom> [wav-output-dir]   (may be given twice: MT-32 + CM-32L)
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/La32.h"
#include <cstdio>

static const double SR = 48000.0;
static int failures = 0;
static void check (bool ok, const char* what, const juce::String& got)
{
    std::printf ("  [%s] %-58s %s\n", ok ? " OK " : "FAIL", what, got.toRawUTF8());
    if (! ok) ++failures;
}

static double freqOf (const std::vector<float>& x, int from)
{
    int cross = 0, first = -1, last = -1;
    for (int i = from + 1; i < (int) x.size(); ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f) { if (first < 0) first = i; last = i; ++cross; }
    return cross > 1 ? (cross - 1) * SR / (last - first) : 0.0;
}

static double f0Auto (const std::vector<float>& x, int from, int len)
{
    // autocorrelation pitch: first peak within 90 % of the maximum (avoids octave errors)
    std::vector<double> c;
    double e0 = 0; for (int i = from; i < from + len; ++i) e0 += (double) x[(size_t) i] * x[(size_t) i];
    for (int lag = 20; lag < 2400; ++lag)
    {
        double a = 0; for (int i = from; i < from + len; ++i) a += (double) x[(size_t) i] * x[(size_t) i + lag];
        c.push_back (a / (e0 + 1e-12));
    }
    double mx = 0; for (double v : c) mx = std::max (mx, v);
    for (size_t i = 1; i + 1 < c.size(); ++i)
        if (c[i] >= 0.9 * mx && c[i] >= c[i - 1] && c[i] >= c[i + 1]) return SR / (double) (i + 20);
    return 0;
}

static std::vector<float> play (std::shared_ptr<const la32::Rom> rom, const la32::Timbre& t, std::initializer_list<int> keys, double hold, double total)
{
    std::vector<float> out ((size_t) (total * SR), 0.0f);
    for (int k : keys)
    {
        la32::Voice v; v.noteOn (rom, t, k, 0.8f, {}, SR);
        for (size_t i = 0; i < out.size(); ++i)
        {
            if (i == (size_t) (hold * SR)) v.noteOff();
            out[i] += v.render (1.0, 0.0f);
        }
    }
    return out;
}

static la32::Timbre singlePartial (bool pcm, int pcmWave)
{
    la32::Timbre t; std::memcpy (t.name, "TEST      ", 10);
    t.common[10] = pcm ? 2 : 0;   // structure 3 (P+S) or 1 (S+S)
    t.common[11] = 0; t.common[12] = 1; t.common[13] = 0;
    auto* p = t.partial[0];
    p[0] = 36; p[1] = 50; p[2] = 11; p[4] = 0; p[5] = (uint8_t) pcmWave; p[6] = 0; p[7] = 7;
    p[8] = 0; p[0x0f] = p[0x10] = p[0x11] = p[0x12] = p[0x13] = 50;
    p[0x17] = 100; p[0x19] = 11;
    p[0x29] = 100; p[0x2a] = 50; p[0x2c] = 12; p[0x2e] = 12;
    p[0x36] = p[0x37] = p[0x38] = p[0x39] = 100;
    return t;
}

static void writeWav (const juce::File& f, const std::vector<float>& x)
{
    float pk = 0; for (float v : x) pk = std::max (pk, std::abs (v));
    juce::AudioBuffer<float> b (1, (int) x.size());
    for (size_t i = 0; i < x.size(); ++i) b.setSample (0, (int) i, pk > 0 ? x[i] * 0.7f / pk : 0.0f);
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 1, 16, {}, 0));
    if (w) w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

int main (int argc, char** argv)
{
    if (argc < 3) { std::printf ("usage: LaTest control.rom pcm.rom [control2 pcm2] [outdir]\n"); return 2; }
    juce::File outDir;
    std::vector<std::pair<juce::String, juce::String>> sets { { argv[1], argv[2] } };
    if (argc >= 5 && juce::File { juce::String (argv[3]) }.existsAsFile()) sets.push_back ({ argv[3], argv[4] });
    if (argc == 4) outDir = juce::File { juce::String (argv[3]) };
    if (argc >= 6) outDir = juce::File { juce::String (argv[5]) };

    for (auto& [cf, pf] : sets)
    {
        juce::MemoryBlock c, p;
        juce::File { cf }.loadFileAsData (c); juce::File { pf }.loadFileAsData (p);
        juce::String err;
        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto rom = la32::load (c, p, juce::File { cf }.getFileNameWithoutExtension(), err);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;
        std::printf ("== %s\n", juce::File { cf }.getFileName().toRawUTF8());
        check (rom != nullptr, "ROM set recognised", rom ? rom->model + " " + rom->version + "  (" + juce::String (ms, 0) + " ms)" : err);
        if (rom == nullptr) continue;
        check (rom->melodic.size() == 128, "128 melodic timbres", rom->melodic[0].getName() + ", " + rom->melodic[1].getName() + " ... " + rom->melodic[127].getName());
        check (rom->rhythm.size() >= 20, "rhythm timbres", juce::String ((int) rom->rhythm.size()) + ": " + (rom->rhythm.empty() ? juce::String() : rom->rhythm[0].getName() + ", " + rom->rhythm[1].getName()));
        check (rom->waves.size() == 128 || rom->waves.size() == 256, "PCM waves", juce::String ((int) rom->waves.size()));

        // pitch: synth partial
        {
            auto x = play (rom, singlePartial (false, 0), { 69 }, 0.5, 0.5);
            const double f = freqOf (x, 4800);
            check (std::abs (f - 440.0) < 1.0, "synth partial: A4 -> 440 Hz", juce::String (f, 2));
        }
        // pitch: PCM playback speed follows the keyboard (a one-shot wave is half as long an octave up)
        {
            auto duration = [&] (int key)
            {
                la32::Voice v; v.noteOn (rom, singlePartial (true, 0), key, 0.8f, {}, SR);
                int n = 0; while (! v.isDone() && n < (int) (10 * SR)) { v.render (1.0, 0.0f); ++n; }
                return (double) n / SR;
            };
            const double expect = rom->waves[0].length / la32::kPcmRate;
            const double d60 = duration (juce::roundToInt (rom->waves[0].rootNote)), d72 = duration (juce::roundToInt (rom->waves[0].rootNote) + 12);
            check (std::abs (d60 - expect) < 0.01 && std::abs (d60 / d72 - 2.0) < 0.02, "PCM playback rate: root key = original speed, +12 = double",
                   juce::String (d60 * 1000.0, 1) + " ms -> " + juce::String (d72 * 1000.0, 1) + " ms (expected " + juce::String (expect * 1000.0, 1) + " ms)");
        }
        // every timbre: finite, bounded, audible, and it ends after key-up
        {
            int silent = 0, nonfinite = 0, hanging = 0; float peak = 0; juce::StringArray quiet;
            auto all = rom->melodic; all.insert (all.end(), rom->rhythm.begin(), rom->rhythm.end());
            for (auto& t : all)
            {
                la32::Voice v; v.noteOn (rom, t, 60, 0.8f, {}, SR);
                float pk = 0; bool fin = true;
                for (int i = 0; i < (int) (1.0 * SR); ++i) { const float s = v.render (1.0, 0.0f); if (! std::isfinite (s)) fin = false; pk = std::max (pk, std::abs (s)); }
                v.noteOff();
                int n = 0; while (! v.isDone() && n < (int) (30 * SR)) { v.render (1.0, 0.0f); ++n; }
                if (! fin) ++nonfinite;
                if (pk < 0.001f) { ++silent; quiet.add (t.getName()); }
                if (! v.isDone()) ++hanging;
                peak = std::max (peak, pk);
            }
            check (nonfinite == 0, "all timbres finite", juce::String (nonfinite));
            check (peak < 4.0f, "peak bounded", juce::String (peak, 2));
            check (silent <= 4, "audible at C4", juce::String (silent) + (quiet.isEmpty() ? juce::String() : " silent: " + quiet.joinIntoString (", ")));
            check (hanging == 0, "every timbre ends after key-up (30 s)", juce::String (hanging));
        }

        if (outDir != juce::File())
        {
            auto dir = outDir.getChildFile (rom->model + " " + rom->version);
            dir.createDirectory();
            for (int k = 0; k < 128; k += 8)
            {
                const auto& t = rom->melodic[(size_t) k];
                writeWav (dir.getChildFile (juce::String (k + 1).paddedLeft ('0', 3) + " " + juce::File::createLegalFileName (t.getName()) + ".wav"),
                          play (rom, t, { 48, 55, 60, 64 }, 2.0, 3.5));
            }
            for (int k = 0; k < juce::jmin (6, (int) rom->rhythm.size()); ++k)
            {
                const auto& t = rom->rhythm[(size_t) k];
                writeWav (dir.getChildFile ("R" + juce::String (k + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (t.getName()) + ".wav"),
                          play (rom, t, { 60 }, 0.4, 1.2));
            }
            std::printf ("  wrote WAVs to %s\n", dir.getFullPathName().toRawUTF8());
        }
    }
    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
