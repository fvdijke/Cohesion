// LOCAL REFERENCE TOOL - not part of the plugin.
// Renders the same MT-32 / CM-32L timbres with Munt (mt32emu, LGPL, cloned into ../../_ref/munt)
// and with COHESION's own LA engine (Source/La32.h), and measures the differences.
//
//   LaCompare timbres <control.rom> <pcm.rom> <outdir> [first count]   per-timbre comparison (CSV + WAVs)
//   LaCompare cal     <control.rom> <pcm.rom>                          single-parameter calibration sweeps
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <mt32emu.h>
#include "../Source/La32.h"
#include <cstdio>
#include <functional>

using Buf = std::vector<float>;
static constexpr double kSr = 32000.0;

// ------------------------------------------------------------------ Munt side
struct Munt
{
    MT32Emu::Synth synth;
    const MT32Emu::ROMImage *ci = nullptr, *pi = nullptr;
    MT32Emu::FileStream fc, fp;

    bool open (const juce::String& ctl, const juce::String& pcm)
    {
        if (! fc.open (ctl.toRawUTF8()) || ! fp.open (pcm.toRawUTF8())) return false;
        ci = MT32Emu::ROMImage::makeROMImage (&fc);
        pi = MT32Emu::ROMImage::makeROMImage (&fp);
        if (ci == nullptr || pi == nullptr || ci->getROMInfo() == nullptr || pi->getROMInfo() == nullptr) return false;
        if (! synth.open (*ci, *pi, MT32Emu::AnalogOutputMode_DIGITAL_ONLY)) return false;
        synth.setReverbEnabled (false);
        synth.setReverbOverridden (true);
        return true;
    }
    void sysex (int a0, int a1, int a2, const uint8_t* d, int n)
    {
        std::vector<uint8_t> m { 0xF0, 0x41, 0x10, 0x16, 0x12, (uint8_t) a0, (uint8_t) a1, (uint8_t) a2 };
        int sum = a0 + a1 + a2;
        for (int i = 0; i < n; ++i) { m.push_back (d[i]); sum += d[i]; }
        m.push_back ((uint8_t) ((128 - (sum & 127)) & 127)); m.push_back (0xF7);
        synth.playSysexNow (m.data(), (MT32Emu::Bit32u) m.size());
    }
    void setTimbre (const la32::Timbre& t)
    {
        // part 1 patch temp: memory timbre 0, no key shift / fine tune, poly, reverb off, level 100, centre
        const uint8_t patch[10] = { 2, 0, 24, 50, 12, 0, 0, 0, 100, 7 };
        sysex (0x03, 0x00, 0x00, patch, 10);
        uint8_t raw[246];
        std::memcpy (raw, t.common, 14);
        for (int k = 0; k < 4; ++k) std::memcpy (raw + 14 + 58 * k, t.partial[k], 58);
        // 246 bytes > one 7-bit address page: split at 128-byte boundaries of the 7-bit address
        sysex (0x04, 0x00, 0x00, raw, 128);
        sysex (0x04, 0x01, 0x00, raw + 128, 246 - 128);
        synth.playMsgNow (0x0079B1u);          // reset controllers (ch 2)
        synth.playMsgNow (0x6407B1u);          // volume 100
        synth.playMsgNow (0x400AB1u);          // pan centre
    }
    Buf render (int key, int vel, int hold, int tail)
    {
        Buf out; out.reserve ((size_t) (hold + tail));
        std::vector<float> st (2 * 256);
        synth.playMsgNow (0x91u | (uint32_t) key << 8 | (uint32_t) vel << 16);
        auto run = [&] (int n) { while (n > 0) { const int k = std::min (n, 256); synth.render (st.data(), (MT32Emu::Bit32u) k);
                                                 for (int i = 0; i < k; ++i) out.push_back (0.5f * (st[2 * i] + st[2 * i + 1])); n -= k; } };
        run (hold);
        synth.playMsgNow (0x81u | (uint32_t) key << 8);
        run (tail);
        synth.playMsgNow (0x7BB1u);            // all notes off
        for (int guard = 0; synth.isActive() && guard < 2000; ++guard) synth.render (st.data(), 256);
        return out;
    }
};

// ------------------------------------------------------------------ own engine
static Buf renderOwn (std::shared_ptr<const la32::Rom> rom, const la32::Timbre& t, int key, int vel, int hold, int tail)
{
    la32::Voice v; v.noteOn (rom, t, key, vel / 127.0f, {}, kSr);
    Buf out; out.reserve ((size_t) (hold + tail));
    for (int i = 0; i < hold; ++i) out.push_back (v.render (1.0, 0.0f));
    v.noteOff();
    for (int i = 0; i < tail; ++i) out.push_back (v.render (1.0, 0.0f));
    return out;
}

// ------------------------------------------------------------------ measurements
static float db (double x) { return (float) (20.0 * std::log10 (std::max (1.0e-9, x))); }
static double rms (const Buf& b, int from, int n)
{
    from = std::max (0, from); n = std::min (n, (int) b.size() - from);
    if (n <= 0) return 0.0;
    double s = 0; for (int i = 0; i < n; ++i) s += (double) b[(size_t) (from + i)] * b[(size_t) (from + i)];
    return std::sqrt (s / n);
}
static Buf envDb (const Buf& b, int frame = 160)   // 5 ms RMS frames
{
    Buf e; for (int i = 0; i + frame <= (int) b.size(); i += frame) e.push_back (db (rms (b, i, frame)));
    return e;
}
// time (ms) for the RMS envelope to first reach peak + rel dB (rel <= 0)
static float riseMs (const Buf& e, float rel)
{
    float pk = -200; for (auto v : e) pk = std::max (pk, v);
    for (size_t i = 0; i < e.size(); ++i) if (e[i] >= pk + rel) return (float) i * 5.0f;
    return -1.0f;
}
// time (ms) after sample `from` until the envelope falls below level(from) + rel dB
static float fallMs (const Buf& e, int fromFrame, float rel)
{
    if (fromFrame >= (int) e.size()) return -1.0f;
    float ref = -200; for (int i = std::max (0, fromFrame - 4); i < fromFrame; ++i) ref = std::max (ref, e[(size_t) i]);
    for (size_t i = (size_t) fromFrame; i < e.size(); ++i) if (e[i] < ref + rel) return (float) (i - (size_t) fromFrame) * 5.0f;
    return -1.0f;
}
// harmonic magnitudes (dB) of a periodic signal around `at`, fundamental f0
static std::vector<float> harmonics (const Buf& b, int at, double f0, int count)
{
    constexpr int order = 14, n = 1 << order;
    juce::dsp::FFT fft (order);
    std::vector<float> d (2 * n, 0.0f);
    for (int i = 0; i < n; ++i)
    {
        const int k = at + i;
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * i / (n - 1));
        d[(size_t) i] = (k < (int) b.size() ? b[(size_t) k] : 0.0f) * w;
    }
    fft.performFrequencyOnlyForwardTransform (d.data());
    std::vector<float> h;
    for (int m = 1; m <= count; ++m)
    {
        const double bin = f0 * m * n / kSr;
        if (bin >= n / 2 - 2) { h.push_back (-200.0f); continue; }
        float pk = 0; for (int j = (int) bin - 2; j <= (int) bin + 2; ++j) pk = std::max (pk, d[(size_t) j]);
        h.push_back (db (pk));
    }
    return h;
}
// fundamental by autocorrelation in a window
static double pitchHz (const Buf& b, int at, int n = 3200)
{
    if (at + 2 * n > (int) b.size()) return 0.0;
    double best = 0; int lag = 0;
    for (int L = 16; L < 1600; ++L)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int i = 0; i < n; ++i) { const double a = b[(size_t) (at + i)], c = b[(size_t) (at + i + L)]; s += a * c; e1 += a * a; e2 += c * c; }
        const double r = s / std::sqrt (std::max (1.0e-12, e1 * e2));
        if (r > best + 1.0e-4 || (lag == 0 && r > 0.9)) { if (r > best) { best = r; lag = L; } }
        if (best > 0.95 && r < best - 0.2) break;   // first strong peak
    }
    if (lag <= 1) return 0.0;
    return kSr / lag;
}
// RMS-difference brightness proxy: Hz such that a sine of that frequency would have the same ratio
static double brightHz (const Buf& b, int at, int n)
{
    double sx = 0, sd = 0;
    for (int i = 1; i < n && at + i < (int) b.size(); ++i) { const double x = b[(size_t) (at + i)], d = x - b[(size_t) (at + i - 1)]; sx += x * x; sd += d * d; }
    if (sx <= 0) return 0.0;
    return kSr / juce::MathConstants<double>::twoPi * std::sqrt (sd / sx);
}

static void writeWav (const juce::File& f, const Buf& b)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    if (os == nullptr) return;
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), kSr, 1, 16, {}, 0));
    if (w == nullptr) return;
    os.release();
    juce::AudioBuffer<float> ab (1, (int) b.size());
    for (int i = 0; i < (int) b.size(); ++i) ab.setSample (0, i, juce::jlimit (-1.0f, 1.0f, b[(size_t) i]));
    w->writeFromAudioSampleBuffer (ab, 0, ab.getNumSamples());
}

// ------------------------------------------------------------------ test timbres
// One synth partial (partial 1), everything neutral; callers poke single parameters.
static la32::Timbre baseTimbre (bool saw = false)
{
    la32::Timbre t;
    std::memcpy (t.name, "CAL       ", 10); std::memcpy (t.common, t.name, 10);
    t.common[10] = 0; t.common[11] = 0; t.common[12] = 1; t.common[13] = 0;   // S1 S2, partial 1 on, sustain
    for (int k = 0; k < 4; ++k)
    {
        uint8_t* p = t.partial[k];
        p[0] = 36; p[1] = 50; p[2] = 11; p[3] = 0; p[4] = saw ? 1 : 0; p[5] = 0; p[6] = 0; p[7] = 7;
        p[8] = 0; p[9] = 0; p[10] = 0; p[11] = 0; p[12] = 0; p[13] = 0; p[14] = 0;
        p[15] = 50; p[16] = 50; p[17] = 50; p[18] = 50; p[19] = 50;          // pitch env flat
        p[0x14] = 0; p[0x15] = 0; p[0x16] = 0;                               // LFO off
        p[0x17] = 100; p[0x18] = 0; p[0x19] = 0; p[0x1a] = 0; p[0x1b] = 7;   // TVF wide open
        p[0x1c] = 0; p[0x1d] = 0; p[0x1e] = 0; p[0x1f] = 0;
        p[0x20] = 0; p[0x21] = 0; p[0x22] = 0; p[0x23] = 0; p[0x24] = 0;
        p[0x25] = 100; p[0x26] = 100; p[0x27] = 100; p[0x28] = 100;
        p[0x29] = 100; p[0x2a] = 50; p[0x2b] = 0; p[0x2c] = 12; p[0x2d] = 0; p[0x2e] = 12;
        p[0x2f] = 0; p[0x30] = 0;
        p[0x31] = 0; p[0x32] = 0; p[0x33] = 0; p[0x34] = 0; p[0x35] = 0;
        p[0x36] = 100; p[0x37] = 100; p[0x38] = 100; p[0x39] = 100;
    }
    return t;
}

struct Pair { Buf ref, own; };

int main (int argc, char** argv)
{

    if (argc < 4) { std::printf ("usage: LaCompare timbres|cal <control.rom> <pcm.rom> [outdir [first count]]\n"); return 1; }
    const juce::String mode (argv[1]);
    const juce::File ctlF { juce::String (argv[2]) }, pcmF { juce::String (argv[3]) };

    juce::MemoryBlock cm, pm;
    ctlF.loadFileAsData (cm); pcmF.loadFileAsData (pm);
    juce::String err;
    auto rom = la32::load (cm, pm, ctlF.getFileNameWithoutExtension(), err);
    if (rom == nullptr) { std::printf ("own loader failed: %s\n", err.toRawUTF8()); return 1; }

    Munt mu;
    if (! mu.open (ctlF.getFullPathName(), pcmF.getFullPathName())) { std::printf ("Munt could not open the ROMs\n"); return 1; }
    std::printf ("ROM %s %s  (%d melodic, %d rhythm)\n", rom->model.toRawUTF8(), rom->version.toRawUTF8(), (int) rom->melodic.size(), (int) rom->rhythm.size());

    auto both = [&] (const la32::Timbre& t, int key, int vel, int hold, int tail)
    {
        mu.setTimbre (t);
        return Pair { mu.render (key, vel, hold, tail), renderOwn (rom, t, key, vel, hold, tail) };
    };

    if (mode == "timbres")
    {
        const juce::File out { juce::String (argc > 4 ? argv[4] : ".") };
        out.createDirectory();
        const int first = argc > 5 ? juce::String (argv[5]).getIntValue() : 0;
        const int count = argc > 6 ? juce::String (argv[6]).getIntValue() : (int) rom->melodic.size();
        juce::String csv = "idx,name,ref_pk,own_pk,dpk,ref_rise,own_rise,ref_s1,own_s1,ref_rel,own_rel,ref_hz,own_hz,ref_br,own_br\n";
        const int hold = (int) (1.5 * kSr), tail = (int) (1.5 * kSr);
        double sumAbsPk = 0, sumBr = 0; int n = 0;
        for (int i = first; i < std::min ((int) rom->melodic.size(), first + count); ++i)
        {
            const auto& t = rom->melodic[(size_t) i];
            auto p = both (t, 60, 100, hold, tail);
            auto er = envDb (p.ref), eo = envDb (p.own);
            float pr = -200, po = -200; for (auto v : er) pr = std::max (pr, v); for (auto v : eo) po = std::max (po, v);
            const int s1 = (int) (1.0 * kSr);
            const float r1 = db (rms (p.ref, s1, 1600)) - pr, o1 = db (rms (p.own, s1, 1600)) - po;
            const int relF = hold / 160;
            const double hr = pitchHz (p.ref, 6400), ho = pitchHz (p.own, 6400);
            const double br = brightHz (p.ref, 6400, 6400), bo = brightHz (p.own, 6400, 6400);
            const juce::String row = juce::String (i) + "," + t.getName().replace (",", " ") + ","
                + juce::String (pr, 1) + "," + juce::String (po, 1) + "," + juce::String (po - pr, 1) + ","
                + juce::String (riseMs (er, -3), 0) + "," + juce::String (riseMs (eo, -3), 0) + ","
                + juce::String (r1, 1) + "," + juce::String (o1, 1) + ","
                + juce::String (fallMs (er, relF, -30), 0) + "," + juce::String (fallMs (eo, relF, -30), 0) + ","
                + juce::String (hr, 1) + "," + juce::String (ho, 1) + ","
                + juce::String (br, 0) + "," + juce::String (bo, 0);
            csv << row << "\n";
            std::printf ("%s\n", row.toRawUTF8());
            if (pr > -90) { sumAbsPk += std::abs (po - pr); sumBr += std::abs (std::log2 (std::max (1.0, bo) / std::max (1.0, br))); ++n; }
            const juce::String base = juce::String (i).paddedLeft ('0', 3) + "-" + juce::File::createLegalFileName (t.getName().trim());
            writeWav (out.getChildFile (base + "-munt.wav"), p.ref);
            writeWav (out.getChildFile (base + "-own.wav"),  p.own);
        }
        out.getChildFile ("compare.csv").replaceWithText (csv);
        std::printf ("SUMMARY n=%d  mean |dPeak| %.2f dB  mean |dBright| %.2f oct\n", n, n ? sumAbsPk / n : 0.0, n ? sumBr / n : 0.0);
        return 0;
    }

    if (mode == "dump")
    {
        for (int a = 4; a < argc; ++a)
        {
            const auto& t = rom->melodic[(size_t) juce::String (argv[a]).getIntValue()];
            std::printf ("%s  struct %d/%d mute %x env %d\n", t.getName().toRawUTF8(), t.common[10], t.common[11], t.common[12], t.common[13]);
            for (int k = 0; k < 4; ++k)
            {
                if (! t.partialOn (k)) continue;
                const uint8_t* q = t.partial[k];
                std::printf (" P%d WG c%d f%d kf%d w%d pcm%d pw%d pwv%d | PENV d%d v%d tk%d T%d %d %d %d L%d %d %d %d %d | LFO r%d d%d m%d\n", k + 1,
                    q[0], q[1], q[2], q[4], q[5], q[6], q[7], q[8], q[9], q[10], q[11], q[12], q[13], q[14], q[15], q[16], q[17], q[18], q[19], q[20], q[21], q[22]);
                std::printf ("    TVF c%d r%d kf%d bp%d bl%d d%d vs%d dk%d tk%d T%d %d %d %d %d L%d %d %d %d\n",
                    q[23], q[24], q[25], q[26], q[27], q[28], q[29], q[30], q[31], q[32], q[33], q[34], q[35], q[36], q[37], q[38], q[39], q[40]);
                std::printf ("    TVA l%d vs%d bp%d bl%d bp%d bl%d tk%d tv%d T%d %d %d %d %d L%d %d %d %d\n",
                    q[41], q[42], q[43], q[44], q[45], q[46], q[47], q[48], q[49], q[50], q[51], q[52], q[53], q[54], q[55], q[56], q[57]);
            }
        }
        return 0;
    }

    if (mode == "parts")
    {
        for (int a = 4; a < argc; ++a)
        {
            const auto base = rom->melodic[(size_t) juce::String (argv[a]).getIntValue()];
            std::printf ("%s\n", base.getName().toRawUTF8());
            for (int k = 0; k < 4; ++k)
            {
                if (! base.partialOn (k)) continue;
                auto t = base; t.common[12] = (uint8_t) (1 << k);
                // keep the pair structure but ring/mix partner muted: plain mix for a fair single-partial test
                auto p = both (t, 60, 100, 32000, 16000);
                auto er = envDb (p.ref), eo = envDb (p.own);
                std::printf ("  P%d  rms50-300ms ref %6.1f own %6.1f  | 1s ref %6.1f own %6.1f | bright ref %5.0f own %5.0f | rise ref %5.0f own %5.0f\n", k + 1,
                    db (rms (p.ref, 1600, 8000)), db (rms (p.own, 1600, 8000)), db (rms (p.ref, 32000 - 3200, 3200)), db (rms (p.own, 32000 - 3200, 3200)),
                    brightHz (p.ref, 1600, 8000), brightHz (p.own, 1600, 8000), riseMs (er, -3), riseMs (eo, -3));
            }
        }
        return 0;
    }

    if (mode == "cal")
    {
        const int hold = (int) (2.0 * kSr), tail = (int) (2.0 * kSr);
        const juce::String what (argc > 4 ? argv[4] : "all");
        auto want = [&] (const char* k) { return what == "all" || what == k; };

        if (want ("level"))
        {
            std::printf ("\n# TVA level -> peak dB (key 60, vel 100)\n");
            for (int L : { 100, 90, 80, 70, 60, 50, 40, 30, 20, 10 })
            { auto t = baseTimbre(); t.partial[0][0x29] = (uint8_t) L; auto p = both (t, 60, 100, 16000, 3200);
              std::printf ("L=%3d  ref %6.2f  own %6.2f\n", L, db (rms (p.ref, 8000, 3200)), db (rms (p.own, 8000, 3200))); }
        }
        if (want ("vel"))
        {
            std::printf ("\n# velocity (sens 50 / 75 / 100) -> dB\n");
            for (int s : { 50, 75, 100 }) for (int v : { 127, 100, 80, 64, 40, 20, 1 })
            { auto t = baseTimbre(); t.partial[0][0x2a] = (uint8_t) s; auto p = both (t, 60, v, 16000, 3200);
              std::printf ("sens %3d vel %3d  ref %6.2f  own %6.2f\n", s, v, db (rms (p.ref, 8000, 3200)), db (rms (p.own, 8000, 3200))); }
        }
        if (want ("envlevel"))
        {
            std::printf ("\n# TVA sustain level -> dB\n");
            for (int L : { 100, 75, 50, 25, 10 })
            { auto t = baseTimbre(); t.partial[0][0x39] = (uint8_t) L; t.partial[0][0x34] = 30; auto p = both (t, 60, 100, hold, 3200);
              std::printf ("L4=%3d  ref %6.2f  own %6.2f\n", L, db (rms (p.ref, hold - 4000, 3200)), db (rms (p.own, hold - 4000, 3200))); }
        }
        if (want ("attack"))
        {
            std::printf ("\n# TVA T1 -> ms to peak-1dB / -6dB / -20dB\n");
            for (int T : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            { auto t = baseTimbre(); t.partial[0][0x31] = (uint8_t) T; auto p = both (t, 60, 100, (int) (12 * kSr), 1600);
              auto er = envDb (p.ref), eo = envDb (p.own);
              std::printf ("T1=%3d  ref %7.0f %7.0f %7.0f   own %7.0f %7.0f %7.0f\n", T, riseMs (er, -1), riseMs (er, -6), riseMs (er, -20), riseMs (eo, -1), riseMs (eo, -6), riseMs (eo, -20)); }
        }
        if (want ("decay"))
        {
            std::printf ("\n# TVA T2 (100 -> L2 0) -> ms to -6 / -20 / -40 dB\n");
            for (int T : { 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x32] = (uint8_t) T; p0[0x37] = 0; p0[0x38] = 0; p0[0x39] = 0;
              auto p = both (t, 60, 100, (int) (12 * kSr), 1600);
              auto er = envDb (p.ref), eo = envDb (p.own);
              std::printf ("T2=%3d  ref %7.0f %7.0f %7.0f   own %7.0f %7.0f %7.0f\n", T, fallMs (er, 1, -6), fallMs (er, 1, -20), fallMs (er, 1, -40), fallMs (eo, 1, -6), fallMs (eo, 1, -20), fallMs (eo, 1, -40)); }
        }
        if (want ("release"))
        {
            std::printf ("\n# TVA T5 (release) -> ms to -6 / -20 / -40 dB after note-off\n");
            for (int T : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            { auto t = baseTimbre(); t.partial[0][0x35] = (uint8_t) T; const int h = 16000;
              auto p = both (t, 60, 100, h, (int) (12 * kSr));
              auto er = envDb (p.ref), eo = envDb (p.own);
              std::printf ("T5=%3d  ref %7.0f %7.0f %7.0f   own %7.0f %7.0f %7.0f\n", T, fallMs (er, h / 160, -6), fallMs (er, h / 160, -20), fallMs (er, h / 160, -40), fallMs (eo, h / 160, -6), fallMs (eo, h / 160, -20), fallMs (eo, h / 160, -40)); }
        }
        if (want ("cutoff"))
        {
            std::printf ("\n# TVF cutoff (saw, key 36, res 0) -> harmonic attenuation vs. cutoff 100; first harmonic below -6/-12 dB (Hz)\n");
            const double f0 = 440.0 * std::pow (2.0, (36 - 69) / 12.0);
            auto open = [&] { auto t = baseTimbre (true); return both (t, 36, 100, 32000, 1600); }();
            auto hr0 = harmonics (open.ref, 12000, f0, 120), ho0 = harmonics (open.own, 12000, f0, 120);
            for (int C : { 100, 90, 80, 70, 60, 50, 40, 30, 20, 10, 0 })
            {
                auto t = baseTimbre (true); t.partial[0][0x17] = (uint8_t) C; auto p = both (t, 36, 100, 32000, 1600);
                auto hr = harmonics (p.ref, 12000, f0, 120), ho = harmonics (p.own, 12000, f0, 120);
                auto edge = [&] (const std::vector<float>& h, const std::vector<float>& h0, float th)
                { for (size_t m = 0; m < h.size(); ++m) if (h[m] - h0[m] < th) return f0 * (double) (m + 1); return -1.0; };
                std::printf ("C=%3d  ref %7.0f %7.0f  own %7.0f %7.0f   (h1 ref %5.1f own %5.1f)\n", C, edge (hr, hr0, -6), edge (hr, hr0, -12), edge (ho, ho0, -6), edge (ho, ho0, -12), hr[0] - hr0[0], ho[0] - ho0[0]);
            }
        }
        if (want ("res"))
        {
            std::printf ("\n# TVF resonance (saw, key 36, cutoff 50) -> max harmonic gain vs cutoff 100 (dB) at Hz\n");
            const double f0 = 440.0 * std::pow (2.0, (36 - 69) / 12.0);
            auto open = [&] { auto t = baseTimbre (true); return both (t, 36, 100, 32000, 1600); }();
            auto hr0 = harmonics (open.ref, 12000, f0, 120), ho0 = harmonics (open.own, 12000, f0, 120);
            for (int R : { 0, 5, 10, 15, 20, 25, 30 })
            {
                auto t = baseTimbre (true); t.partial[0][0x17] = 50; t.partial[0][0x18] = (uint8_t) R; auto p = both (t, 36, 100, 32000, 1600);
                auto hr = harmonics (p.ref, 12000, f0, 120), ho = harmonics (p.own, 12000, f0, 120);
                auto mx = [&] (const std::vector<float>& h, const std::vector<float>& h0, double& at)
                { float m = -200; for (size_t i = 0; i < h.size(); ++i) if (h[i] - h0[i] > m) { m = h[i] - h0[i]; at = f0 * (double) (i + 1); } return m; };
                double ar = 0, ao = 0; const float mr = mx (hr, hr0, ar), mo = mx (ho, ho0, ao);
                std::printf ("R=%2d  ref %+6.1f dB @ %6.0f   own %+6.1f dB @ %6.0f\n", R, mr, ar, mo, ao);
            }
        }
        if (want ("pw"))
        {
            std::printf ("\n# pulse width -> duty estimated from h2/h1 (= |cos(pi d)|)\n");
            const double f0 = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
            for (int W : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            {
                auto t = baseTimbre(); t.partial[0][6] = (uint8_t) W; auto p = both (t, 48, 100, 32000, 1600);
                auto hr = harmonics (p.ref, 12000, f0, 4), ho = harmonics (p.own, 12000, f0, 4);
                auto duty = [] (const std::vector<float>& h) { const double r = std::pow (10.0, (h[1] - h[0]) / 20.0); return std::acos (juce::jlimit (0.0, 1.0, r)) / juce::MathConstants<double>::pi; };
                std::printf ("PW=%3d  ref duty %.3f   own duty %.3f   (h2-h1 ref %+5.1f own %+5.1f)\n", W, duty (hr), duty (ho), hr[1] - hr[0], ho[1] - ho[0]);
            }
        }
        if (want ("pitch"))
        {
            std::printf ("\n# pitch: key / coarse / fine / keyfollow -> Hz\n");
            struct C { int key, coarse, fine, kf; };
            for (C c : { C { 60, 36, 50, 11 }, C { 69, 36, 50, 11 }, C { 60, 48, 50, 11 }, C { 60, 36, 100, 11 }, C { 72, 36, 50, 7 }, C { 72, 36, 50, 0 } })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0] = (uint8_t) c.coarse; p0[1] = (uint8_t) c.fine; p0[2] = (uint8_t) c.kf;
              auto p = both (t, c.key, 100, 32000, 1600);
              std::printf ("key %d coarse %d fine %d kf %2d  ref %8.2f  own %8.2f\n", c.key, c.coarse, c.fine, c.kf, pitchHz (p.ref, 9600), pitchHz (p.own, 9600)); }
        }
        if (want ("penv"))
        {
            std::printf ("\n# pitch env depth: start level 0 -> pitch at t=0+ (semitones vs. steady)\n");
            for (int D : { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[8] = (uint8_t) D; p0[15] = 0; p0[16] = 0; p0[17] = 0; p0[18] = 0; p0[19] = 0; p0[11] = 1;
              auto p = both (t, 60, 100, 32000, 1600);
              const double base = 440.0 * std::pow (2.0, -9 / 12.0);
              std::printf ("depth %2d  ref %+7.2f  own %+7.2f semis\n", D, 12 * std::log2 (std::max (1.0, pitchHz (p.ref, 9600)) / base), 12 * std::log2 (std::max (1.0, pitchHz (p.own, 9600)) / base)); }
        }
        if (want ("lfo"))
        {
            std::printf ("\n# LFO rate -> Hz (pitch-track zero crossings), depth 50\n");
            for (int R : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 100 })
            {
                auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x14] = (uint8_t) R; p0[0x15] = 50;
                auto p = both (t, 60, 100, (int) (6 * kSr), 1600);
                auto lfoHz = [&] (const Buf& b, double& depth)
                {
                    std::vector<double> tr; const int win = 1024, hop = 256;
                    for (int at = 8000; at + 2 * win < (int) (6 * kSr); at += hop) tr.push_back (pitchHz (b, at, win));
                    double mean = 0; for (auto v : tr) mean += v; mean /= std::max<size_t> (1, tr.size());
                    int zc = 0; double mx = 0; for (size_t i = 1; i < tr.size(); ++i) { if ((tr[i - 1] - mean) * (tr[i] - mean) < 0) ++zc; mx = std::max (mx, std::abs (12 * std::log2 (std::max (1.0, tr[i]) / mean))); }
                    depth = mx; return zc / 2.0 / ((double) tr.size() * hop / kSr);
                };
                double dr = 0, dd = 0; const double hr = lfoHz (p.ref, dr), ho = lfoHz (p.own, dd);
                std::printf ("rate %3d  ref %6.2f Hz (+-%.2f st)   own %6.2f Hz (+-%.2f st)\n", R, hr, dr, ho, dd);
            }
        }
        if (want ("resk"))
        {
            std::printf ("\n# resonance level effect: rms(R) - rms(R=0) dB, saw, kf 3, key 48 / 72\n");
            for (int key : { 48, 72 }) for (int C : { 0, 20, 40, 60, 80, 100 })
            {
                juce::String line = "key " + juce::String (key) + " C " + juce::String (C).paddedLeft (' ', 3) + ":";
                auto lvl = [&] (int R, bool ref) { auto tt = baseTimbre (true); tt.partial[0][0x17] = (uint8_t) C; tt.partial[0][0x19] = 3; tt.partial[0][0x18] = (uint8_t) R;
                    auto q = both (tt, key, 100, 16000, 1600); return db (rms (ref ? q.ref : q.own, 8000, 6400)); };
                const float r0 = lvl (0, true), o0 = lvl (0, false);
                for (int R : { 10, 20, 25, 30 }) line << "  R" << R << " ref " << juce::String (lvl (R, true) - r0, 1) << " own " << juce::String (lvl (R, false) - o0, 1);
                std::printf ("%s\n", line.toRawUTF8());
            }
        }
        if (want ("bias"))
        {
            std::printf ("\n# TVA bias: dB relative to the bias note, per key offset\n");
            for (int bp : { 91, 27 }) for (int bl : { 0, 2, 4, 7, 10, 11, 12 })
            {
                juce::String line = "bp " + juce::String (bp) + " bl " + juce::String (bl).paddedLeft (' ', 2) + ":";
                const int sign = bp >= 64 ? 1 : -1;
                auto lvl = [&] (int key, bool ref) { auto tt = baseTimbre(); tt.partial[0][0x2b] = (uint8_t) bp; tt.partial[0][0x2c] = (uint8_t) bl;
                    auto q = both (tt, key, 100, 9600, 1600); return db (rms (ref ? q.ref : q.own, 3200, 4800)); };
                const float r0 = lvl (60, true), o0 = lvl (60, false);
                for (int d : { 6, 12, 24 }) line << "  +" << d << " ref " << juce::String (lvl (60 + sign * d, true) - r0, 1) << " own " << juce::String (lvl (60 + sign * d, false) - o0, 1);
                std::printf ("%s\n", line.toRawUTF8());
            }
        }
        if (want ("clev"))
        {
            std::printf ("\n# absolute level vs cutoff (kf 3, res 0): rms dB\n");
            for (int saw : { 0, 1 }) for (int key : { 48, 72 })
            {
                juce::String line = juce::String (saw ? "saw" : "sq ") + " key " + juce::String (key) + ":";
                for (int C : { 0, 20, 40, 60, 80, 100 })
                { auto tt = baseTimbre (saw != 0); tt.partial[0][0x17] = (uint8_t) C; tt.partial[0][0x19] = 3; auto q = both (tt, key, 100, 9600, 1600);
                  line << "  C" << C << " " << juce::String (db (rms (q.ref, 3200, 4800)), 1) << "/" << juce::String (db (rms (q.own, 3200, 4800)) - 8.77f, 1); }
                std::printf ("%s\n", line.toRawUTF8());
            }
            for (int pw : { 50, 75, 95, 100 })
            { auto tt = baseTimbre(); tt.partial[0][6] = (uint8_t) pw; auto q = both (tt, 60, 100, 9600, 1600);
              std::printf ("pw %3d  ref %6.1f own %6.1f\n", pw, db (rms (q.ref, 3200, 4800)), db (rms (q.own, 3200, 4800)) - 8.77f); }
        }
        if (want ("tvfbias"))
        {
            std::printf ("\n# TVF bias (saw, C=60, kf 3): brightness Hz per key offset from the point\n");
            for (int bp : { 91, 27 }) for (int bl : { 0, 3, 5, 7, 9, 11, 14 })
            {
                juce::String line = "bp " + juce::String (bp) + " bl " + juce::String (bl).paddedLeft (' ', 2) + ":";
                const int sign = bp >= 64 ? 1 : -1;
                for (int d : { 0, 12, 24 })
                { auto tt = baseTimbre (true); auto* q = tt.partial[0]; q[0x17] = 60; q[0x19] = 3; q[0x1a] = (uint8_t) bp; q[0x1b] = (uint8_t) bl;
                  auto r = both (tt, 60 + sign * d, 100, 9600, 1600);
                  line << "  +" << d << " " << juce::String (brightHz (r.ref, 3200, 4800), 0) << "/" << juce::String (brightHz (r.own, 3200, 4800), 0)
                       << " (" << juce::String (db (rms (r.ref, 3200, 4800)), 1) << "/" << juce::String (db (rms (r.own, 3200, 4800)) - 8.77f, 1) << ")"; }
                std::printf ("%s\n", line.toRawUTF8());
            }
        }
        if (want ("struct"))
        {
            std::printf ("\n# structures: partial 1 at 262 Hz, partial 2 at 392 Hz; dB at f1 / f2 / f2-f1 / f1+f2 (ref | own-8.8)\n");
            for (int st = 0; st < 13; ++st)
            {
                auto tt = baseTimbre(); tt.common[10] = (uint8_t) st; tt.common[12] = 3;
                tt.partial[1][0] = 43; tt.partial[0][5] = 0; tt.partial[1][5] = 0;
                auto r = both (tt, 60, 100, 16000, 1600);
                const double b0 = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
                auto hr = harmonics (r.ref, 6400, b0, 5), ho = harmonics (r.own, 6400, b0, 5);
                std::printf ("struct %2d  ref %6.1f %6.1f %6.1f %6.1f  | own %6.1f %6.1f %6.1f %6.1f   rms ref %6.1f own %6.1f\n", st + 1,
                    hr[1], hr[2], hr[0], hr[4], ho[1] - 8.8f, ho[2] - 8.8f, ho[0] - 8.8f, ho[4] - 8.8f, db (rms (r.ref, 6400, 6400)), db (rms (r.own, 6400, 6400)) - 8.8f);
            }
        }
        if (want ("struct2"))
        {
            int lw = 0; for (int w = 0; w < (int) rom->waves.size(); ++w) if (rom->waves[(size_t) w].loop && rom->waves[(size_t) w].length >= 2048) { lw = w; break; }
            std::printf ("\n# structures with looped PCM wave %d. Per partial alone: S/P (h2 of the square absent = S); both: ring products\n", lw);
            const double b0 = 440.0 * std::pow (2.0, (48 - 69) / 12.0);
            for (int st = 0; st < 13; ++st)
            {
                auto mk = [&] (int mute) { auto tt = baseTimbre(); tt.common[10] = (uint8_t) st; tt.common[12] = (uint8_t) mute;
                    tt.partial[1][0] = 43; for (int k = 0; k < 2; ++k) { tt.partial[k][5] = (uint8_t) (lw & 127); tt.partial[k][4] = (uint8_t) (lw >= 128 ? 2 : 0); } return tt; };
                juce::String line = "struct " + juce::String (st + 1).paddedLeft (' ', 2) + ":";
                for (int k = 0; k < 2; ++k)
                {
                    auto r = both (mk (1 << k), 60, 100, 16000, 1600);
                    const double f = k == 0 ? 2 * b0 : 3 * b0;
                    auto kind = [&] (const Buf& b) { auto h = harmonics (b, 6400, f, 2); return h[1] < h[0] - 30 ? "S" : "P"; };
                    line << "  P" << (k + 1) << " ref " << kind (r.ref) << " own " << kind (r.own);
                }
                auto r = both (mk (3), 60, 100, 16000, 1600);
                auto hr = harmonics (r.ref, 6400, b0, 5), ho = harmonics (r.own, 6400, b0, 5);
                auto a1 = both (mk (1), 60, 100, 16000, 1600), a2 = both (mk (2), 60, 100, 16000, 1600);
                line << "  | rms both " << juce::String (db (rms (r.ref, 6400, 6400)), 1) << "/" << juce::String (db (rms (r.own, 6400, 6400)) - 8.8f, 1)
                     << "  P1 " << juce::String (db (rms (a1.ref, 6400, 6400)), 1) << "/" << juce::String (db (rms (a1.own, 6400, 6400)) - 8.8f, 1)
                     << "  P2 " << juce::String (db (rms (a2.ref, 6400, 6400)), 1) << "/" << juce::String (db (rms (a2.own, 6400, 6400)) - 8.8f, 1)
                     << "  diff " << juce::String (hr[0], 1) << "/" << juce::String (ho[0] - 8.8f, 1) << " sum " << juce::String (hr[4], 1) << "/" << juce::String (ho[4] - 8.8f, 1);
                std::printf ("%s\n", line.toRawUTF8());
            }
        }
        if (want ("coarse"))
        {
            std::printf ("\n# cutoff vs coarse tune (saw, key 60): -6 dB edge vs ideal saw (Hz) and rms (ref/own-8.8)\n");
            for (int kf : { 3, 11 }) for (int co : { 36, 48, 60 }) for (int C : { 30, 50, 80 })
            {
                const double g0 = 440.0 * std::pow (2.0, (60 + co - 36 - 69) / 12.0);
                const int nh = juce::jmax (2, (int) (15000.0 / g0));
                auto tt = baseTimbre (true); auto* q = tt.partial[0]; q[0] = (uint8_t) co; q[0x17] = (uint8_t) C; q[0x19] = (uint8_t) kf;
                auto r = both (tt, 60, 100, 9600, 1600);
                auto edge = [&] (const Buf& b) { auto h = harmonics (b, 3200, g0, nh); for (int m = 2; m <= nh; ++m) if (h[(size_t) m - 1] - h[0] + 20 * std::log10 ((double) m) < -6) return g0 * m; return -1.0; };
                std::printf ("kf %2d coarse %d C %2d  edge %7.0f / %7.0f   rms %6.1f / %6.1f\n", kf, co, C, edge (r.ref), edge (r.own), db (rms (r.ref, 3200, 4800)), db (rms (r.own, 3200, 4800)) - 8.8f);
            }
        }
        if (want ("resb"))
        {
            std::printf ("\n# resonance vs brightness (saw, kf 3): bright Hz ref/own, rms ref/own-8.8\n");
            for (int key : { 48, 60 }) for (int C : { 60, 80, 95 })
            {
                juce::String line = "key " + juce::String (key) + " C " + juce::String (C) + ":";
                for (int R : { 0, 15, 20, 26, 30 })
                { auto tt = baseTimbre (true); auto* q = tt.partial[0]; q[0x17] = (uint8_t) C; q[0x19] = 3; q[0x18] = (uint8_t) R;
                  auto r = both (tt, key, 100, 9600, 1600);
                  line << "  R" << R << " " << juce::String (brightHz (r.ref, 3200, 4800), 0) << "/" << juce::String (brightHz (r.own, 3200, 4800), 0)
                       << " (" << juce::String (db (rms (r.ref, 3200, 4800)), 1) << "/" << juce::String (db (rms (r.own, 3200, 4800)) - 8.8f, 1) << ")"; }
                std::printf ("%s\n", line.toRawUTF8());
            }
        }
        if (want ("tvfvel"))
        {
            std::printf ("\n# TVF env depth 60 x velocity sens (saw key 48, C 20, kf 3, env 100): brightness ref/own\n");
            for (int vs : { 0, 50, 100 })
            {
                juce::String line = "vs " + juce::String (vs).paddedLeft (' ', 3) + ":";
                for (int v : { 127, 100, 80, 64, 32 })
                { auto tt = baseTimbre (true); auto* q = tt.partial[0]; q[0x17] = 20; q[0x19] = 3; q[0x1c] = 60; q[0x1d] = (uint8_t) vs; for (int k = 0; k < 4; ++k) q[0x25 + k] = 100;
                  auto r = both (tt, 48, v, 9600, 1600);
                  line << "  v" << v << " " << juce::String (brightHz (r.ref, 3200, 4800), 0) << "/" << juce::String (brightHz (r.own, 3200, 4800), 0); }
                std::printf ("%s\n", line.toRawUTF8());
            }
            std::printf ("# depth sweep vs 0 vel 127\n");
            for (int d : { 0, 20, 40, 60, 80, 100 })
            { auto tt = baseTimbre (true); auto* q = tt.partial[0]; q[0x17] = 20; q[0x19] = 3; q[0x1c] = (uint8_t) d; for (int k = 0; k < 4; ++k) q[0x25 + k] = 100;
              auto r = both (tt, 48, 127, 9600, 1600);
              std::printf ("d %3d  %6.0f / %6.0f\n", d, brightHz (r.ref, 3200, 4800), brightHz (r.own, 3200, 4800)); }
        }
        if (want ("abs"))
        {
            std::printf ("\n# absolute -6 dB edge vs ideal saw (Hz), res 0: key x kf x cutoff\n");
            for (int kf : { 3, 11 }) for (int key : { 36, 48, 60, 72 })
            {
                const double g0 = 440.0 * std::pow (2.0, (key - 69) / 12.0);
                const int nh = (int) (15000.0 / g0);
                juce::String rl = "kf " + juce::String (kf) + " key " + juce::String (key) + "  ref:", ol = "               own:";
                for (int C : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, 95, 100 })
                {
                    auto tt = baseTimbre (true); tt.partial[0][0x17] = (uint8_t) C; tt.partial[0][0x19] = (uint8_t) kf;
                    auto q = both (tt, key, 100, 32000, 1600);
                    auto edge = [&] (const Buf& b)
                    { auto h = harmonics (b, 12000, g0, nh); const float h1 = h[0];
                      // reference level: the louder of h1 or the ideal-saw line fitted to the first 2 harmonics
                      for (int m = 1; m <= nh; ++m) if (h[(size_t) m - 1] - h1 + 20 * std::log10 ((double) m) < -6) return g0 * m; return -1.0; };
                    rl << juce::String (edge (q.ref), 0).paddedLeft (' ', 7); ol << juce::String (edge (q.own), 0).paddedLeft (' ', 7);
                }
                std::printf ("%s\n%s\n", rl.toRawUTF8(), ol.toRawUTF8());
            }
        }
        if (want ("open"))
        {
            std::printf ("\n# C=100 spectrum vs ideal saw (h[m]-h[1]+20log m), key 36, kf 3; then kf sweep\n");
            const double f0 = 440.0 * std::pow (2.0, (36 - 69) / 12.0);
            auto t = baseTimbre (true); t.partial[0][0x19] = 3; auto p = both (t, 36, 100, 32000, 1600);
            auto hr = harmonics (p.ref, 12000, f0, 200), ho = harmonics (p.own, 12000, f0, 200);
            for (int m : { 2, 4, 8, 16, 32, 48, 64, 96, 128, 160, 190 })
                std::printf ("m %3d (%6.0f Hz)  ref %+6.1f  own %+6.1f\n", m, f0 * m, hr[(size_t) m - 1] - hr[0] + 20 * std::log10 ((double) m), ho[(size_t) m - 1] - ho[0] + 20 * std::log10 ((double) m));
            for (int key : { 36, 48, 72, 84 }) for (int C : { 30, 50, 80 }) for (int kf : { 0, 3, 11 })
            {
                const double g0 = 440.0 * std::pow (2.0, (key - 69) / 12.0);
                auto mk = [&] (int c, int k) { auto tt = baseTimbre (true); tt.partial[0][0x17] = (uint8_t) c; tt.partial[0][0x19] = (uint8_t) k; return both (tt, key, 100, 32000, 1600); };
                auto o = mk (100, kf), q = mk (C, kf);
                auto a0 = harmonics (o.ref, 12000, g0, 200), a = harmonics (q.ref, 12000, g0, 200), b0 = harmonics (o.own, 12000, g0, 200), b = harmonics (q.own, 12000, g0, 200);
                auto edge = [&] (const std::vector<float>& h, const std::vector<float>& h0) { for (size_t m = 0; m < h.size(); ++m) if (h[m] - h0[m] < -6) return g0 * (double) (m + 1); return -1.0; };
                std::printf ("key %d C %d kf %2d  ref %7.0f  own %7.0f\n", key, C, kf, edge (a, a0), edge (b, b0));
            }
        }
        if (want ("leveltab"))
        {
            std::printf ("\n# TVA level table (dB rel. 100)\n");
            float r100 = 0;
            for (int L = 100; L >= 0; L -= 5)
            { auto t = baseTimbre(); t.partial[0][0x29] = (uint8_t) L; auto p = both (t, 60, 100, 16000, 3200);
              const float r = db (rms (p.ref, 8000, 3200)); if (L == 100) r100 = r; std::printf ("L=%3d %7.2f\n", L, r - r100); }
        }
        if (want ("velneg"))
        {
            std::printf ("\n# velocity sens < 50\n");
            for (int s : { 0, 25 }) for (int v : { 127, 100, 64, 1 })
            { auto t = baseTimbre(); t.partial[0][0x2a] = (uint8_t) s; auto p = both (t, 60, v, 16000, 3200);
              std::printf ("sens %3d vel %3d  ref %6.2f\n", s, v, db (rms (p.ref, 8000, 3200))); }
        }
        if (want ("dist"))
        {
            std::printf ("\n# segment time vs distance: T2=80, L2 -> ms to reach L2+0.5dB\n");
            for (int L2 : { 0, 25, 50, 75, 90 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x32] = 80; p0[0x37] = (uint8_t) L2; p0[0x38] = (uint8_t) L2; p0[0x39] = (uint8_t) L2;
              auto p = both (t, 60, 100, (int) (8 * kSr), 1600); auto er = envDb (p.ref);
              const float fin = er[(size_t) (7 * 200)];
              float tm = -1; for (size_t i = 0; i < er.size(); ++i) if (er[i] < fin + 0.5f) { tm = i * 5.0f; break; }
              std::printf ("L2=%3d  final %6.2f dB  reached at %6.0f ms\n", L2, fin - er[1], tm); }
            std::printf ("\n# attack to L1 < 100: T1=70, L1 -> ms to reach L1-0.5dB\n");
            for (int L1 : { 100, 50, 25 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x31] = 70; for (int k = 0; k < 4; ++k) p0[0x36 + k] = (uint8_t) L1;
              auto p = both (t, 60, 100, (int) (6 * kSr), 1600); auto er = envDb (p.ref);
              const float fin = er[(size_t) (5 * 200)];
              float tm = -1; for (size_t i = 0; i < er.size(); ++i) if (er[i] > fin - 0.5f) { tm = i * 5.0f; break; }
              std::printf ("L1=%3d  reached at %6.0f ms\n", L1, tm); }
            std::printf ("\n# release from sustain 50: T5=80 -> ms to -6/-20 dB\n");
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x35] = 80; p0[0x39] = 50; p0[0x34] = 1; const int h = 16000;
              auto p = both (t, 60, 100, h, (int) (8 * kSr)); auto er = envDb (p.ref);
              std::printf ("ref %6.0f %6.0f\n", fallMs (er, h / 160, -6), fallMs (er, h / 160, -20)); }
        }
        if (want ("tkf"))
        {
            std::printf ("\n# TVA time keyfollow (T2=80 decay to 0, ms to -20 dB) by key / time-kf\n");
            for (int kf : { 0, 1, 2, 4 }) for (int key : { 36, 60, 84 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x32] = 80; p0[0x37] = 0; p0[0x38] = 0; p0[0x39] = 0; p0[0x2f] = (uint8_t) kf;
              auto p = both (t, key, 100, (int) (8 * kSr), 1600); auto er = envDb (p.ref), eo = envDb (p.own);
              std::printf ("kf %d key %d  ref %6.0f  own %6.0f\n", kf, key, fallMs (er, 1, -20), fallMs (eo, 1, -20)); }
            std::printf ("\n# TVA time velo sens (T1=70, ms to -6 dB) \n");
            for (int vs : { 0, 2, 4 }) for (int v : { 127, 64, 1 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x31] = 70; p0[0x30] = (uint8_t) vs;
              auto p = both (t, 60, v, (int) (6 * kSr), 1600); auto er = envDb (p.ref), eo = envDb (p.own);
              std::printf ("vs %d vel %3d  ref %6.0f  own %6.0f\n", vs, v, riseMs (er, -6), riseMs (eo, -6)); }
        }
        if (want ("cutoff2"))
        {
            std::printf ("\n# TVF cutoff, keyfollow 0 (param 3): -3/-6/-12 dB edges (Hz), keys 36 and 60; also kf param at C=50\n");
            for (int key : { 36, 60 })
            {
                const double f0 = 440.0 * std::pow (2.0, (key - 69) / 12.0);
                auto mk = [&] (int C, int kf) { auto t = baseTimbre (true); t.partial[0][0x17] = (uint8_t) C; t.partial[0][0x19] = (uint8_t) kf; return both (t, key, 100, 32000, 1600); };
                auto o = mk (100, 3);
                auto hr0 = harmonics (o.ref, 12000, f0, 200), ho0 = harmonics (o.own, 12000, f0, 200);
                auto edge = [&] (const std::vector<float>& h, const std::vector<float>& h0, float th)
                { for (size_t m = 0; m < h.size(); ++m) if (h[m] - h0[m] < th) return f0 * (double) (m + 1); return -1.0; };
                for (int C : { 100, 99, 98, 97, 95, 90, 85, 80, 70, 60, 50, 40, 30, 20, 10, 0 })
                { auto p = mk (C, 3); auto hr = harmonics (p.ref, 12000, f0, 200), ho = harmonics (p.own, 12000, f0, 200);
                  std::printf ("key %d C=%3d  ref %7.0f %7.0f %7.0f   own %7.0f %7.0f %7.0f\n", key, C, edge (hr, hr0, -3), edge (hr, hr0, -6), edge (hr, hr0, -12), edge (ho, ho0, -3), edge (ho, ho0, -6), edge (ho, ho0, -12)); }
                for (int kf : { 0, 3, 7, 11, 14 })
                { auto p = mk (50, kf); auto hr = harmonics (p.ref, 12000, f0, 200);
                  std::printf ("key %d C=50 kf %2d  ref -6 edge %7.0f\n", key, kf, edge (hr, hr0, -6)); }
            }
        }
        if (want ("tvfenv"))
        {
            std::printf ("\n# TVF env depth (saw key 36, C=0, kf 3, env levels 100/50): -6 dB edge\n");
            const double f0 = 440.0 * std::pow (2.0, (36 - 69) / 12.0);
            auto o = [&] { auto t = baseTimbre (true); t.partial[0][0x19] = 3; return both (t, 36, 100, 32000, 1600); }();
            auto hr0 = harmonics (o.ref, 12000, f0, 200);
            auto edge = [&] (const std::vector<float>& h, float th) { for (size_t m = 0; m < h.size(); ++m) if (h[m] - hr0[m] < th) return f0 * (double) (m + 1); return -1.0; };
            for (int D : { 0, 25, 50, 75, 100 }) for (int L : { 100, 50 })
            { auto t = baseTimbre (true); auto* p0 = t.partial[0]; p0[0x17] = 0; p0[0x19] = 3; p0[0x1c] = (uint8_t) D; for (int k = 0; k < 4; ++k) p0[0x25 + k] = (uint8_t) L;
              auto p = both (t, 36, 100, 32000, 1600); auto hr = harmonics (p.ref, 12000, f0, 200);
              std::printf ("depth %3d level %3d  ref -6 edge %7.0f\n", D, L, edge (hr, -6)); }
        }
        if (want ("penv2"))
        {
            std::printf ("\n# pitch env: level linearity at depth 7, and depth 10 at key 96\n");
            for (int L : { 0, 25, 40, 60, 75, 100 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[8] = 7; for (int k = 15; k < 20; ++k) p0[k] = (uint8_t) L; p0[11] = 1;
              auto p = both (t, 60, 100, 32000, 1600); const double base = 440.0 * std::pow (2.0, -9 / 12.0);
              std::printf ("level %3d  ref %+7.2f semis\n", L, 12 * std::log2 (std::max (1.0, pitchHz (p.ref, 9600)) / base)); }
            for (int D : { 9, 10 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[8] = (uint8_t) D; for (int k = 15; k < 20; ++k) p0[k] = 0; p0[11] = 1;
              auto p = both (t, 96, 100, 32000, 1600); const double base = 440.0 * std::pow (2.0, 27 / 12.0);
              std::printf ("depth %d key 96 level 0  ref %+7.2f semis\n", D, 12 * std::log2 (std::max (1.0, pitchHz (p.ref, 9600)) / base)); }
            std::printf ("\n# pitch env time: T1 (level 0 -> 50 at depth 7): ms until within 0.5 st\n");
            for (int T : { 20, 40, 60, 80 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[8] = 7; p0[15] = 0; p0[11] = (uint8_t) T;
              auto p = both (t, 60, 100, (int) (8 * kSr), 1600); const double base = 440.0 * std::pow (2.0, -9 / 12.0);
              float tm = -1; for (int at = 0; at + 6400 < (int) (8 * kSr); at += 160) { const double h = pitchHz (p.ref, at, 1024); if (h > 0 && std::abs (12 * std::log2 (h / base)) < 0.5) { tm = at / 32.0f; break; } }
              std::printf ("T1=%2d  ref %6.0f ms\n", T, tm); }
        }
        if (want ("lfo2"))
        {
            std::printf ("\n# LFO depth at rate 50 -> +- semitones\n");
            for (int D : { 10, 25, 50, 75, 100 })
            { auto t = baseTimbre(); auto* p0 = t.partial[0]; p0[0x14] = 50; p0[0x15] = (uint8_t) D;
              auto p = both (t, 60, 100, (int) (4 * kSr), 1600);
              double mn = 1e9, mx = 0; for (int at = 8000; at + 2048 < (int) (4 * kSr); at += 128) { const double h = pitchHz (p.ref, at, 512); if (h > 0) { mn = std::min (mn, h); mx = std::max (mx, h); } }
              std::printf ("depth %3d  ref +-%.3f st\n", D, 6 * std::log2 (mx / mn)); }
        }
        if (want ("pcm"))
        {
            std::printf ("\n# PCM partial level vs synth partial (both L=100): rms dB over 50-150 ms, key 60\n");
            for (int w : { 0, 10, 20, 30, 40, 50, 60, 70, 80, 100, 120 })
            { auto t = baseTimbre(); t.common[10] = 2; auto* p0 = t.partial[0]; p0[5] = (uint8_t) w;
              auto p = both (t, 60, 100, 6400, 1600);
              std::printf ("wave %3d  ref %6.2f  own %6.2f\n", w, db (rms (p.ref, 1600, 3200)), db (rms (p.own, 1600, 3200))); }
        }
        return 0;
    }
    std::printf ("unknown mode\n");
    return 1;
}
