// Offline checks for the DX7-compatible FM engine; optionally renders a .syx bank to WAV.
//   FmTest [bank.syx] [wav-output-dir]
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Dx7.h"
#include <cstdio>

static const double SR = 48000.0;
static int failures = 0;
static void check (bool ok, const char* what, const juce::String& got)
{
    std::printf ("  [%s] %-56s %s\n", ok ? " OK " : "FAIL", what, got.toRawUTF8());
    if (! ok) ++failures;
}

static dx7::Patch sineOp1 (int alg = 31)
{
    dx7::Patch p; p.alg = (uint8_t) alg;
    for (auto& o : p.op) { o.ol = 0; for (int i = 0; i < 4; ++i) { o.r[i] = 99; o.l[i] = i == 3 ? 0 : 99; } }
    p.op[0].ol = 99; p.op[0].coarse = 1;
    return p;
}

static std::vector<float> render (const dx7::Patch& p, int note, double secs, double offAt = -1)
{
    dx7::Voice v; v.prepare (SR); juce::Random r (1);
    v.noteOn (p, note, 0.8f, {}, r);
    std::vector<float> out;
    for (int i = 0; i < (int) (secs * SR); ++i)
    {
        if (offAt >= 0 && i == (int) (offAt * SR)) v.noteOff();
        out.push_back (v.next (1.0));
    }
    return out;
}

static double freqOf (const std::vector<float>& x, int from)
{
    int cross = 0, first = -1, last = -1;
    for (int i = from + 1; i < (int) x.size(); ++i)
        if (x[(size_t) i - 1] < 0.0f && x[(size_t) i] >= 0.0f)
        { if (first < 0) first = i; last = i; ++cross; }
    return cross > 1 ? (cross - 1) * SR / (last - first) : 0.0;
}

int main (int argc, char** argv)
{

    std::printf ("1. Algorithm table\n");
    {
        static const int carriers[32] = { 2,2,2,2,3,3,2,2,2,2,2,2,2,2,2,1,1,1,3,3,4,4,4,5,5,3,3,3,4,4,5,6 };
        bool ok = true; juce::String bad;
        for (int a = 0; a < 32; ++a)
        {
            const auto& al = dx7::algorithm (a);
            int c = 0; for (int i = 0; i < 6; ++i) if (al.carriers & (1 << i)) ++c;
            bool order = true;
            for (int i = 0; i < 6; ++i) for (int k = 0; k <= i; ++k) if (al.mods[i] & (1 << k)) order = false;   // modulators are higher-numbered
            if (c != carriers[a] || ! order || al.fbOp < 1 || al.fbOp > 6) { ok = false; bad << (a + 1) << " "; }
        }
        check (ok, "carrier counts + modulator order for all 32", ok ? juce::String ("ok") : bad);
    }

    std::printf ("2. Pitch\n");
    {
        auto x = render (sineOp1(), 69, 0.5);
        const double f = freqOf (x, 4800);
        check (std::abs (f - 440.0) < 0.5, "ratio 1, A4 -> 440 Hz", juce::String (f, 2));
        auto p2 = sineOp1(); p2.op[0].coarse = 2;
        const double f2 = freqOf (render (p2, 69, 0.5), 4800);
        check (std::abs (f2 - 880.0) < 1.0, "ratio 2 -> 880 Hz", juce::String (f2, 2));
        auto p3 = sineOp1(); p3.op[0].coarse = 0;
        const double f3 = freqOf (render (p3, 69, 0.5), 4800);
        check (std::abs (f3 - 220.0) < 0.5, "coarse 0 = ratio 0.5 -> 220 Hz", juce::String (f3, 2));
        auto p4 = sineOp1(); p4.op[0].mode = 1; p4.op[0].coarse = 2; p4.op[0].fine = 0;
        const double f4a = freqOf (render (p4, 40, 0.5), 4800), f4b = freqOf (render (p4, 90, 0.5), 4800);
        check (std::abs (f4a - 100.0) < 0.5 && std::abs (f4b - 100.0) < 0.5, "fixed 100 Hz on any key", juce::String (f4a, 1) + " / " + juce::String (f4b, 1));
        auto p5 = sineOp1(); p5.transpose = 36;
        const double f5 = freqOf (render (p5, 69, 0.5), 4800);
        check (std::abs (f5 - 880.0) < 1.0, "transpose +12", juce::String (f5, 2));
    }

    std::printf ("3. Modulation does something\n");
    {
        auto plain = sineOp1 (0);                         // alg 1: OP2 -> OP1
        auto fm = plain; fm.op[1].ol = 99; fm.op[1].coarse = 1;
        for (int i = 0; i < 4; ++i) { fm.op[1].r[i] = 99; fm.op[1].l[i] = i == 3 ? 0 : 99; }
        auto a = render (plain, 57, 0.3), b = render (fm, 57, 0.3);
        double diff = 0, en = 0;
        for (size_t i = 4800; i < a.size(); ++i) { diff += (a[i] - b[i]) * (a[i] - b[i]); en += a[i] * a[i]; }
        check (diff / en > 0.2, "OP2 at full level changes the OP1 waveform", juce::String (diff / en, 3));
    }

    std::printf ("4. Stability (every algorithm, all operators full, feedback 7)\n");
    {
        bool ok = true; float pk = 0;
        for (int a = 0; a < 32; ++a)
        {
            dx7::Patch p; p.alg = (uint8_t) a; p.fb = 7;
            for (auto& o : p.op) { o.ol = 99; o.coarse = 1; for (int i = 0; i < 4; ++i) { o.r[i] = 99; o.l[i] = i == 3 ? 0 : 99; } }
            for (float s : render (p, 60, 0.4)) { if (! std::isfinite (s)) ok = false; pk = std::max (pk, std::abs (s)); }
        }
        check (ok && pk < 2.0f, "finite, peak bounded", juce::String (pk, 3));
    }

    std::printf ("5. Release\n");
    {
        auto p = sineOp1(); p.op[0].r[3] = 60;    // moderate release
        dx7::Voice v; v.prepare (SR); juce::Random r (1);
        v.noteOn (p, 60, 0.8f, {}, r);
        for (int i = 0; i < 4800; ++i) v.next (1.0);
        v.noteOff();
        int n = 0; while (! v.isDone() && n < (int) (10 * SR)) { v.next (1.0); ++n; }
        check (v.isDone() && n > 100, "voice finishes after key-up", juce::String (n / SR * 1000.0, 1) + " ms");
    }

    // ---- optional: parse + render a bank ----
    if (argc > 1)
    {
        std::printf ("6. Bank %s\n", argv[1]);
        juce::MemoryBlock mb; juce::File (juce::String (argv[1])).loadFileAsData (mb);
        auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
        int named = 0; juce::StringArray names;
        for (auto& v : res.voices) if (! v.isBlank()) { ++named; names.add (v.getName()); }
        check (res.banks == 1 && res.voices.size() == 32 && res.badChecksums == 0, "one valid 32-voice bank",
               juce::String (res.banks) + " bank(s), " + juce::String ((int) res.voices.size()) + " voices, bad checksums " + juce::String (res.badChecksums));
        check (named > 0, "non-blank voices", juce::String (named) + ": " + names.joinIntoString (", "));
        // VCED round trip
        bool rt = true;
        for (auto& v : res.voices) { auto d = v.toVced(); auto back = dx7::fromVced (d.data()); if (back.toVced() != d) rt = false; }
        check (rt, "VCED round trip (state storage)", rt ? "ok" : "mismatch");

        if (argc > 2)
        {
            const juce::File dir { juce::String (argv[2]) }; dir.createDirectory();
            juce::WavAudioFormat wav;
            int idx = 0;
            for (auto& v : res.voices)
            {
                if (v.isBlank()) continue;
                ++idx;
                // C major chord, 2.5 s held, 1.5 s release
                const int len = (int) (4.0 * SR);
                juce::AudioBuffer<float> buf (1, len); buf.clear();
                for (int nn : { 48, 52, 55, 60 })
                {
                    dx7::Voice voice; voice.prepare (SR); juce::Random r (nn);
                    voice.noteOn (v, nn, 0.8f, {}, r);
                    for (int i = 0; i < len; ++i) { if (i == (int) (2.5 * SR)) voice.noteOff(); buf.addSample (0, i, voice.next (1.0) * 0.3f); }
                }
                const float pk = buf.getMagnitude (0, len);
                if (pk > 0.0f) buf.applyGain (0.7f / pk);
                auto f = dir.getChildFile (juce::String (idx).paddedLeft ('0', 2) + "-" + juce::File::createLegalFileName (v.getName()) + ".wav");
                f.deleteFile();
                std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (f), SR, 1, 16, {}, 0));
                if (w) w->writeFromAudioSampleBuffer (buf, 0, len);
                std::printf ("    wrote %s (peak before normalise %.3f)\n", f.getFileName().toRawUTF8(), pk);
            }
        }
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
