#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <array>
#include <memory>
#include <cmath>
#include <cstdint>
#include <cstring>

// ===========================================================================
//  COHESION - SYNTH · LA engine (Roland MT-32 / CM-32L compatible).
//  * Reads the user's own control + PCM ROM dumps (never bundled). Table
//    locations are detected from the data, so different firmware versions work.
//  * Own synthesis of the documented timbre format: up to 4 partials per
//    timbre (synth square/saw with pulse width + resonant TVF, or PCM sample),
//    pitch envelope + LFO, TVA envelope, the 13 partial-pair structures incl.
//    ring modulation. All code is original; behaviour is approximated.
// ===========================================================================
namespace la32
{
    static constexpr double kPcmRate = 32000.0;

    struct PcmWave { int start = 0, length = 0; bool loop = false; float rootNote = 60.0f; };

    struct Timbre
    {
        char name[11] {};
        uint8_t common[14] {};
        uint8_t partial[4][58] {};
        juce::String getName() const { return juce::String (name).trimEnd(); }
        bool partialOn (int k) const { return (common[12] >> k) & 1; }
    };

    struct Rom
    {
        juce::String model, version, label;
        uint64_t contentHash = 0;
        std::vector<float> pcm;
        std::vector<PcmWave> waves;
        std::vector<Timbre> melodic;   // 128: group A (0-63), group B (64-127)
        std::vector<Timbre> rhythm;           // rhythm timbres without "OFF" placeholders (library)
        std::vector<Timbre> rhythmAll;        // all rhythm timbres in ROM order (rhythm numbers 64..)
        // rhythm key map from the control ROM: key rhythmFirstKey + i -> timbre / level / pan / reverb
        struct RhythmKey { uint8_t timbre = 0xff, level = 0, pan = 7, reverb = 0; };
        std::vector<RhythmKey> rhythmKeys;
        int rhythmFirstKey = 24, rhythmMapAt = -1;
    };

    inline uint64_t fnv1a (const uint8_t* p, size_t n, uint64_t h = 1469598103934665603ULL)
    {
        for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
        return h;
    }

    // ------------------------------------------------------------ PCM decode
    // 16-bit words with a scrambled bit order, log-encoded: sign + 14-bit log magnitude
    // (1024 units per octave, larger = louder).
    inline std::vector<float> decodePcm (const uint8_t* b, size_t size)
    {
        static const int order[16] = { 0, 9, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15, 8 };
        const size_t n = size / 2;
        std::vector<float> out (n);
        float expTab[1024];
        for (int i = 0; i < 1024; ++i) expTab[i] = (float) std::exp2 (i / 1024.0);
        for (size_t i = 0; i < n; ++i)
        {
            const unsigned w = ((unsigned) b[2 * i] << 8) | b[2 * i + 1];
            unsigned v = 0;
            for (int u = 0; u < 16; ++u) v |= ((w >> (15 - order[u])) & 1u) << (15 - u);
            const int mag = (int) ((v >> 1) & 0x3fff) - 16383;            // -16383 .. 0
            const int oct = (mag - 1023) / 1024;                            // floor division for negatives
            const float a = std::ldexp (expTab[mag - oct * 1024], oct);
            out[i] = (v & 0x8000) ? -a : a;
        }
        return out;
    }

    // ----------------------------------------------------- control-ROM parsing
    inline bool plausibleName (const uint8_t* p)
    {
        int letters = 0;
        for (int i = 0; i < 10; ++i)
        {
            if (p[i] < 0x20 || p[i] > 0x7e) return false;
            if ((p[i] >= 'A' && p[i] <= 'Z') || (p[i] >= 'a' && p[i] <= 'z')) ++letters;
        }
        return letters >= 2;
    }

    inline bool plausiblePartial (const uint8_t* q)
    {
        return q[0] <= 96 && q[1] <= 100 && q[2] <= 16 && q[3] <= 1 && q[4] <= 3 && q[5] <= 127 && q[6] <= 100
            && q[0x17] <= 100 && q[0x18] <= 30 && q[0x29] <= 100 && q[0x2a] <= 100;
    }

    inline bool plausibleTimbre (const uint8_t* rom, size_t size, size_t p)
    {
        if (p + 14 + 58 > size) return false;
        const uint8_t* c = rom + p;
        return plausibleName (c) && c[10] <= 12 && c[11] <= 12 && c[12] <= 15 && c[13] <= 1 && plausiblePartial (c + 14);
    }

    struct PtrMap { size_t at = 0; int count = 0; long base = 0; std::vector<size_t> targets; };

    // Finds runs of little-endian 16-bit pointers (plus a bank offset) to plausible timbres.
    // A table can be valid with more than one offset; every variant is returned (same `at`).
    inline std::vector<PtrMap> findMaps (const uint8_t* rom, size_t size, int minCount)
    {
        std::vector<PtrMap> maps;
        const long bases[] = { 0x0000, 0x4000, 0x8000, 0xC000, 0x10000, 0x14000, 0x18000 };
        for (size_t at = 0; at + 2 * (size_t) minCount <= size; at += 2)
        {
            int longest = 0;
            for (long base : bases)
            {
                PtrMap m; m.at = at; m.base = base;
                for (size_t k = at; k + 1 < size && m.count < 128; k += 2)
                {
                    const size_t t = (size_t) ((long) (rom[k] | (rom[k + 1] << 8)) + base);
                    if (t >= size || ! plausibleTimbre (rom, size, t)) break;
                    if (! m.targets.empty() && t <= m.targets.back()) break;     // timbres are stored in order
                    m.targets.push_back (t); ++m.count;
                }
                if (m.count >= minCount) { longest = std::max (longest, m.count); maps.push_back (std::move (m)); }
            }
            if (longest > 0) at += 2 * (size_t) longest - 2;
        }
        return maps;
    }

    inline Timbre readTimbre (const uint8_t* rom, size_t size, size_t p, bool compressed)
    {
        Timbre t;
        std::memcpy (t.name, rom + p, 10);
        std::memcpy (t.common, rom + p, 14);
        size_t q = p + 14;
        for (int k = 0; k < 4; ++k)
        {
            const bool stored = compressed ? ((t.common[12] >> k) & 1) : true;
            if (stored && q + 58 <= size) { std::memcpy (t.partial[k], rom + q, 58); q += 58; }
        }
        return t;
    }

    inline bool isCompressed (const PtrMap& m, const uint8_t* rom)
    {
        for (int i = 0; i + 1 < m.count; ++i)
            if (m.targets[(size_t) i + 1] - m.targets[(size_t) i] != 246)
            {
                const unsigned mbits = rom[m.targets[(size_t) i] + 12] & 15u;
                const int stored = (int) ((mbits & 1u) + ((mbits >> 1) & 1u) + ((mbits >> 2) & 1u) + ((mbits >> 3) & 1u));
                if (m.targets[(size_t) i + 1] - m.targets[(size_t) i] == (size_t) (14 + 58 * stored)) return true;
            }
        return false;
    }

    // PCM table: `count` records of (block, length/loop, pitch LSB, pitch MSB); drums first (pitch 0x5000).
    inline long findPcmTable (const uint8_t* rom, size_t size, int count, int blocks)
    {
        for (size_t at = 0; at + 4 * (size_t) count <= size; ++at)
        {
            bool ok = true;
            for (int k = 0; k < 5 && ok; ++k) ok = rom[at + 4 * k + 2] == 0x00 && rom[at + 4 * k + 3] == 0x50;
            for (int k = 0; k < count && ok; ++k)
            {
                const uint8_t pos = rom[at + 4 * k], ln = rom[at + 4 * k + 1];
                ok = (int) pos + (1 << ((ln >> 4) & 7)) <= blocks;      // low nibble = flags (ignored)
            }
            if (ok) return (long) at;
        }
        return -1;
    }

    inline std::shared_ptr<Rom> load (const juce::MemoryBlock& control, const juce::MemoryBlock& pcmData,
                                      const juce::String& label, juce::String& error)
    {
        auto* c = static_cast<const uint8_t*> (control.getData());
        const size_t cs = control.getSize();
        const size_t blocks = pcmData.getSize() / 2 / 0x800;
        const int waveCount = blocks >= 256 ? 256 : 128;

        auto rom = std::make_shared<Rom>();
        rom->label = label;

        const long pcmAt = findPcmTable (c, cs, waveCount, (int) blocks);
        if (pcmAt < 0) { error = "no PCM table matching a " + juce::String ((int) (pcmData.getSize() / 1024)) + " KB PCM ROM"; return nullptr; }

        // melodic timbres: one map of 128 pointers (A+B contiguous) or two maps of 64 (A, B);
        // rhythm timbres: the longest remaining map
        auto maps = findMaps (c, cs, 10);
        std::vector<const PtrMap*> mel;
        for (auto& m : maps) if (m.count >= 128) { mel.push_back (&m); break; }
        // two tables of 64 (group A, group B) that point at different timbres
        for (size_t i = 0; mel.empty() && i < maps.size(); ++i)
            for (size_t j = i + 1; j < maps.size(); ++j)
                if (maps[i].count >= 64 && maps[j].count >= 64 && maps[j].at != maps[i].at && maps[j].targets[0] != maps[i].targets[0])
                { mel = { &maps[i], &maps[j] }; break; }
        int have = 0; for (auto* m : mel) have += m->count;
        if (have < 128) { error = "timbre tables not found (unsupported control ROM?)"; return nullptr; }
        for (auto* m : mel)
        {
            const bool comp = isCompressed (*m, c);
            for (int i = 0; i < m->count && (int) rom->melodic.size() < 128; ++i)
                rom->melodic.push_back (readTimbre (c, cs, m->targets[(size_t) i], comp));
        }
        const PtrMap* best = nullptr;
        for (auto& m : maps)
        {
            bool used = false; for (auto* u : mel) if (u->at == m.at) used = true;
            if (! used && (best == nullptr || m.count > best->count)) best = &m;
        }
        if (best != nullptr)
        {
            const bool comp = isCompressed (*best, c);
            for (int i = 0; i < best->count; ++i)
            {
                auto t = readTimbre (c, cs, best->targets[(size_t) i], comp);
                rom->rhythmAll.push_back (t);
                if (t.getName() != "OFF") rom->rhythm.push_back (t);
            }
        }

        // rhythm key map: the longest run of (timbre, level 0-100, pan 0-14, reverb 0-1) records
        // that mostly points at rhythm timbres (numbers 64..)
        {
            const int nR = (int) rom->rhythmAll.size();
            size_t bestAt = 0; int bestLen = 0;
            for (size_t at = 0; at + 4 * 40 <= cs; ++at)
            {
                int len = 0, drums = 0;
                for (size_t q = at; q + 4 <= cs && len < 128; q += 4, ++len)
                {
                    const uint8_t t = c[q], l = c[q + 1], pn = c[q + 2], rv = c[q + 3];
                    const bool tOk = t < 64 + nR || t == 127 || t == 0xff;       // 127 = off
                    if (! tOk || l > 100 || pn > 14 || rv > 2) break;
                    if (t >= 64 && t < 64 + nR && l > 0) ++drums;
                }
                if (drums >= 20 && len > bestLen) { bestLen = len; bestAt = at; }
            }
            for (int i = 0; i < bestLen; ++i)
            {
                const uint8_t* q = c + bestAt + 4 * (size_t) i;
                rom->rhythmKeys.push_back ({ q[0], q[1], q[2], (uint8_t) (q[3] != 0 ? 1 : 0) });
            }
            rom->rhythmFirstKey = 24;
            rom->rhythmMapAt = (int) bestAt;
        }

        for (int k = 0; k < waveCount; ++k)
        {
            const uint8_t* e = c + pcmAt + 4 * k;
            PcmWave w;
            w.start = e[0] * 0x800;
            w.length = 0x800 << ((e[1] >> 4) & 7);
            w.loop = (e[1] & 0x80) != 0;
            const int pitch = (e[3] << 8) | e[2];
            w.rootNote = 120.0f - (float) pitch * 12.0f / 4096.0f;
            rom->waves.push_back (w);
        }
        rom->pcm = decodePcm (static_cast<const uint8_t*> (pcmData.getData()), pcmData.getSize());

        rom->model = waveCount == 256 ? "CM-32L" : "MT-32";
        // version text inside the control ROM, e.g. "ver1.07" / "Ver 1.02"
        for (size_t i = 0; i + 8 < cs; ++i)
            if ((c[i] == 'v' || c[i] == 'V') && c[i + 1] == 'e' && c[i + 2] == 'r')
            {
                size_t j = i + 3; while (j < cs && (c[j] == ' ' || c[j] == '.')) ++j;
                if (j + 3 < cs && c[j] >= '0' && c[j] <= '9' && c[j + 1] == '.' && c[j + 2] >= '0' && c[j + 2] <= '9')
                { rom->version = juce::String::charToString ((juce::juce_wchar) c[j]) + "." + juce::String::charToString ((juce::juce_wchar) c[j + 2])
                               + (c[j + 3] >= '0' && c[j + 3] <= '9' ? juce::String::charToString ((juce::juce_wchar) c[j + 3]) : juce::String()); break; }
            }
        if (rom->version.isEmpty())      // fall back to the file name, e.g. "..._1_02" / "v1.02"
        {
            auto digits = label.retainCharacters ("0123456789._");
            for (int i = 0; i + 3 < digits.length(); ++i)
                if (juce::CharacterFunctions::isDigit (digits[i]) && (digits[i + 1] == '.' || digits[i + 1] == '_')
                    && juce::CharacterFunctions::isDigit (digits[i + 2]) && juce::CharacterFunctions::isDigit (digits[i + 3]))
                { rom->version = juce::String::charToString (digits[i]) + "." + digits.substring (i + 2, i + 4); break; }
        }
        // identity of the *sound* (timbres + PCM), used to skip duplicate firmware versions
        uint64_t h = 1469598103934665603ULL;
        for (auto& t : rom->melodic) { h = fnv1a (t.common, 14, h); for (int k = 0; k < 4; ++k) h = fnv1a (t.partial[k], 58, h); }
        h = fnv1a (static_cast<const uint8_t*> (pcmData.getData()), pcmData.getSize(), h);
        rom->contentHash = h;
        return rom;
    }

    // ------------------------------------------------------------------ voice
    // Parameter curves below were fitted to measurements of a reference renderer
    // (tests/LaCompare: single-parameter sweeps); the code itself is original.

    // envelope segment time (ms) - independent of the distance travelled; doubles every ~7.9 steps
    inline float envMs (int t) { return t <= 0 ? 0.0f : 3790.0f * std::exp2 ((float) (t - 80) / 7.92f); }

    // linear interpolation in a table sampled every `step` parameter units
    template <size_t N> inline float lerpTab (const float (&tab)[N], float x, float step)
    {
        const float f = juce::jlimit (0.0f, (float) (N - 1), x / step);
        const int i = juce::jmin ((int) f, (int) N - 2);
        return tab[i] + (tab[i + 1] - tab[i]) * (f - (float) i);
    }
    // TVA level 0..100 -> dB (0 = silent)
    inline float tvaLevelDb (int level)
    {
        static const float t[21] = { -200.0f, -53.5f, -46.6f, -38.5f, -32.8f, -28.3f, -24.9f, -21.5f, -18.8f, -16.6f, -14.3f,
                                     -12.4f, -10.5f, -9.0f, -7.5f, -6.0f, -4.5f, -3.4f, -2.3f, -1.1f, 0.0f };
        return lerpTab (t, (float) level, 5.0f);
    }
    // TVF cutoff -> log2 Hz of the -6 dB point at key 60. Cutoff 100 is about 1.6 kHz; the TVF
    // envelope can push the effective value well above 100.
    inline float tvfCutoffOct (float c) { return 10.617f + (c - 100.0f) * 0.0566f; }
    // time keyfollow 0..4 -> octaves of time per two octaves of key
    inline float timeKeyfollow (int k) { static const float t[5] = { 0.0f, 0.19f, 0.38f, 0.75f, 1.5f }; return t[juce::jlimit (0, 4, k)]; }

    struct Seg { float target, ms; };

    // Multi-segment linear envelope: key-down segments, a hold, then release.
    struct Env
    {
        float value = 0, from = 0, target = 0, inc = 0;
        int stage = 0, nDown = 0; bool releasing = false, done = true, autoRelease = false;
        Seg down[4] {}; Seg rel {};
        double sr = 44100.0;

        void start (float startValue, const Seg* segs, int n, Seg release, bool noSustain, double sampleRate)
        {
            sr = sampleRate; value = startValue; nDown = n; for (int i = 0; i < n; ++i) down[i] = segs[i];
            rel = release; autoRelease = noSustain; releasing = false; done = false; enter (0);
        }
        void enter (int s)
        {
            stage = s;
            const Seg& g = releasing ? rel : down[s];
            target = g.target;
            const float samples = juce::jmax (1.0f, g.ms * 0.001f * (float) sr);
            inc = (target - value) / samples;
        }
        void release() { if (! releasing && ! done) { releasing = true; enter (0); } }
        // advance by n samples (control rate)
        void advance (int n)
        {
            if (done) return;
            const bool hold = ! releasing && stage >= nDown;
            if (hold) return;
            value += inc * (float) n;
            if ((inc >= 0.0f && value >= target) || (inc < 0.0f && value <= target))
            {
                value = target;
                if (releasing) { done = true; return; }
                if (stage + 1 < nDown) enter (stage + 1);
                else { stage = nDown; inc = 0.0f; if (autoRelease) release(); }
            }
        }
    };

    struct Lp2   // TPT 2-pole low-pass with resonance (for synth partials' TVF)
    {
        float s1 = 0, s2 = 0;
        float process (float x, float g, float k)
        {
            const float a1 = 1.0f / (1.0f + g * (g + k)), a2 = g * a1, a3 = g * a2;
            const float v3 = x - s2, v1 = a1 * s1 + a2 * v3, v2 = s2 + a2 * s1 + a3 * v3;
            s1 = 2.0f * v1 - s1; s2 = 2.0f * v2 - s2;
            return v2;
        }
        void reset() { s1 = s2 = 0.0f; }
    };

    inline float keyfollow (int idx)
    {
        static const float t[17] = { -1.0f, -0.5f, -0.25f, 0.0f, 0.125f, 0.25f, 0.375f, 0.5f, 0.625f, 0.75f, 0.875f, 1.0f, 1.25f, 1.5f, 2.0f, 1.0f, 1.0f };
        return t[juce::jlimit (0, 16, idx)];
    }

    struct Macros { float cutoff = 0.0f, time = 1.0f; };

    class Partial
    {
    public:
        bool active = false, pcmMode = false;

        void noteOn (const Rom& rom, const uint8_t* p, bool isPcm, int key, float vel, bool noSustain, const Macros& m, double sampleRate)
        {
            sr = sampleRate; P = p; pcmMode = isPcm; active = true; finished = false;
            romRef = &rom;
            const float kf = keyfollow (p[2]);
            basePitch = 60.0f + (float) (p[0] - 36) + (float) (key - 60) * kf + (p[1] - 50) / 100.0f;

            if (pcmMode)
            {
                const int bank = (p[4] >> 1) & 1;
                const int idx = juce::jlimit (0, (int) rom.waves.size() - 1, p[5] + 128 * bank);
                wave = rom.waves[(size_t) idx];
                pos = (double) wave.start;
            }
            else
            {
                phase = 0.0; saw = (p[4] & 1) != 0;
                // pulse width only narrows the pulse above 50 (100 -> 1/8 duty)
                const float pw = juce::jlimit (0.0f, 100.0f, p[6] + (p[7] - 7) / 7.0f * 30.0f * (vel - 0.5f));
                duty = pw <= 50.0f ? 0.5f : 0.5f * std::exp2 (-2.0f * (pw - 50.0f) / 50.0f);
                lp.reset(); lp2.reset();
            }

            const float tScale = m.time;
            auto T = [&] (int t, int keyfIdx) { return envMs (t) * tScale * std::exp2 (-(float) (key - 60) / 24.0f * timeKeyfollow (keyfIdx)); };

            // --- pitch envelope (levels -50..+50 -> semitones by depth) ---
            static const float depthSemis[11] = { 0, 0.1f, 0.24f, 0.38f, 0.79f, 1.58f, 3.15f, 6.3f, 12.5f, 25.0f, 50.0f };
            penvRange = depthSemis[juce::jlimit (0, 10, (int) p[8])] * (1.0f - p[9] / 3.0f * (1.0f - vel));
            const int ptk = p[0x0a];
            Seg ps[3] = { { (p[0x10] - 50) / 50.0f, T (p[0x0b], ptk) }, { (p[0x11] - 50) / 50.0f, T (p[0x0c], ptk) }, { (p[0x12] - 50) / 50.0f, T (p[0x0d], ptk) } };
            penv.start ((p[0x0f] - 50) / 50.0f, ps, 3, { (p[0x13] - 50) / 50.0f, T (p[0x0e], ptk) }, false, sr);

            // --- LFO ---
            lfoHz = 1.49f * std::exp2 ((p[0x14] - 50) / 7.92f);
            lfoDepth = p[0x15] * 0.00568f; lfoModSens = p[0x16] * 0.00568f; lfoPhase = 0.0;

            // --- TVF (synth partials only) ---
            if (! pcmMode)
            {
                cutoffBase = (float) p[0x17];
                cutoffShift = m.cutoff * 3.0f + keyfollow (p[0x19]) * (float) (key - 60) / 12.0f
                            + (float) (p[0] - 36) / 12.0f                                       // the cutoff follows coarse tune
                            + biasOct (p[0x1a], tvfBiasSlope (p[0x1b]), key);
                // 4-pole: Butterworth pair, resonance raises the Q of the second stage
                resQ = 1.307f + 4.5f * std::pow (p[0x18] / 30.0f, 6.0f);
                resDb = -0.19f * (float) p[0x18];                  // resonance costs level (R30 = -5.7 dB)
                // envelope depth in cutoff units (depth 100 x level 100 = +58); velocity scales it around 64
                tvfDepth = p[0x1c] * 0.58f * juce::jmax (0.0f, 1.0f + 0.8f * p[0x1d] / 100.0f * (vel * 127.0f - 64.0f) / 63.0f)
                         * std::exp2 (-(float) (key - 60) / 12.0f * p[0x1e] * 0.25f);
                const int ftk = p[0x1f];
                Seg fs[4] = { { p[0x25] / 100.0f, T (p[0x20], ftk) }, { p[0x26] / 100.0f, T (p[0x21], ftk) },
                              { p[0x27] / 100.0f, T (p[0x22], ftk) }, { p[0x28] / 100.0f, T (p[0x23], ftk) } };
                tvf.start (0.0f, fs, 4, { 0.0f, T (p[0x24], ftk) }, noSustain, sr);
            }

            // --- TVA ---
            float levelDb = tvaLevelDb (p[0x29]);
            const float s = (p[0x2a] - 50) / 50.0f, v127 = juce::jlimit (1.0f, 127.0f, vel * 127.0f);
            levelDb -= std::abs (s) * 0.296f * (s > 0.0f ? 127.0f - v127 : v127 - 1.0f);
            levelDb += juce::jmax (-54.0f, biasDb (p[0x2b], p[0x2c], key) + biasDb (p[0x2d], p[0x2e], key));
            levelGain = std::pow (10.0f, levelDb / 20.0f);
            const int atk = p[0x2f];
            static const float velTk[5] = { 0.0f, 0.22f, 0.44f, 0.9f, 1.87f };
            const float velT = std::exp2 (-(v127 - 64.0f) / 63.0f * velTk[juce::jlimit (0, 4, (int) p[0x30])]);
            Seg as[4] = { { p[0x36] / 100.0f, T (p[0x31], atk) * velT }, { p[0x37] / 100.0f, T (p[0x32], atk) },
                          { p[0x38] / 100.0f, T (p[0x33], atk) }, { p[0x39] / 100.0f, T (p[0x34], atk) } };
            tva.start (0.0f, as, 4, { 0.0f, T (p[0x35], atk) / 2.41f }, noSustain, sr);
            ctl = 0; tvfGain = 1.0f;
        }

        void noteOff() { tva.release(); tvf.release(); penv.release(); }
        bool isDone() const { return ! active || tva.done || finished; }

        float render (double pitchMult, float modWheel)
        {
            if (! active || finished) return 0.0f;
            if (ctl-- <= 0) control (modWheel);
            gain += gainStep;
            const double ratio = pitchMult * pitchCtl;
            float y;
            if (pcmMode)
            {
                const double inc = kPcmRate / sr * ratio * std::exp2 ((basePitch - wave.rootNote) / 12.0);
                const auto& s = romRef->pcm;
                const int end = wave.start + wave.length;
                const int i0 = (int) pos; const float fr = (float) (pos - i0);
                auto at = [&] (int i)
                {
                    if (i >= end) i = wave.loop ? wave.start + (i - end) % wave.length : end - 1;
                    return (i >= 0 && i < (int) s.size()) ? s[(size_t) i] : 0.0f;
                };
                const float ym1 = at (i0 > wave.start ? i0 - 1 : i0), y0 = at (i0), y1 = at (i0 + 1), y2 = at (i0 + 2);
                const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
                y = ((c3 * fr + c2) * fr + c1) * fr + y0;
                pos += inc;
                if (pos >= end) { if (wave.loop) pos -= wave.length; else finished = true; }
            }
            else
            {
                const double inc = 440.0 * std::exp2 ((basePitch - 69.0) / 12.0) * ratio / sr;
                const double t = phase;
                if (saw) { y = 1.26f * ((float) (2.0 * t - 1.0) - blep (t, inc)); }   // +2 dB: same level relation as the reference
                else
                {
                    y = t < duty ? 1.0f : -1.0f;
                    y += blep (t, inc);
                    double t2 = t - duty; if (t2 < 0.0) t2 += 1.0;
                    y -= blep (t2, inc);
                }
                phase += inc; if (phase >= 1.0) phase -= 1.0;
                y = lp2.process (lp.process (y, gCut, 1.848f), gCut, res);
            }
            return y * gain;
        }

    private:
        static float blep (double t, double dt)
        {
            if (dt <= 0.0) return 0.0f;
            if (t < dt)       { t /= dt; return (float) (t + t - t * t - 1.0); }
            if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
            return 0.0f;
        }
        static float tvfBiasSlope (int level)
        {
            static const float t[15] = { -1.2f, -1.05f, -0.9f, -0.7f, -0.45f, -0.17f, -0.05f, 0.0f, 0.1f, 0.25f, 0.4f, 0.55f, 0.75f, 1.0f, 1.25f };
            return t[juce::jlimit (0, 14, level)];
        }
        static float biasOct (int point, float octPerOct, int key)
        {
            const int note = 33 + (point & 63);
            const float d = (float) (key - note) / 12.0f;
            if (point < 64) return d < 0.0f ? -d * octPerOct : 0.0f;   // keys below the point
            return d > 0.0f ? d * octPerOct : 0.0f;                     // keys above the point
        }
        static float biasDb (int point, int level, int key)
        {
            // dB per octave away from the bias point: roughly halves every two level steps, 12 = none
            static const float slope[13] = { 36.0f, 26.5f, 19.3f, 14.0f, 10.2f, 7.5f, 5.4f, 3.9f, 2.6f, 1.7f, 1.1f, 0.5f, 0.0f };
            const int note = 33 + (point & 63);
            const float d = (float) (key - note) / 12.0f;
            const float dbPerOct = slope[juce::jlimit (0, 12, level)];
            if (point < 64) return d < 0.0f ? d * dbPerOct : 0.0f;    // below the point
            return d > 0.0f ? -d * dbPerOct : 0.0f;                    // above the point
        }

        static constexpr int kCtl = 16;
        void control (float modWheel)
        {
            ctl = kCtl - 1;
            penv.advance (kCtl); tva.advance (kCtl);
            lfoPhase += lfoHz * kCtl / sr; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            const float lfo = (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase) * (lfoDepth + modWheel * lfoModSens);
            pitchCtl = std::exp2 ((penv.value * penvRange + lfo) / 12.0);
            if (! pcmMode)
            {
                tvf.advance (kCtl);
                // -6 dB point -> 4-pole Butterworth corner (|H| = 1/2 at 1.147 fc)
                const float oct6 = tvfCutoffOct (cutoffBase + tvf.value * tvfDepth) + cutoffShift;   // -6 dB point
                const float oct = oct6 - 0.198f;
                // the LA32 waveform always keeps its fundamental: the corner never drops below it
                const float f0 = 440.0f * std::exp2 ((basePitch - 69.0f) / 12.0f) * (float) pitchCtl;
                const float fMin = 1.1f * f0, fc = std::exp2 (oct);
                const float hz = juce::jlimit (20.0f, (float) (sr * 0.45), juce::jmax (fc, fMin));
                // no resonant peak while the corner sits on the fundamental; full peak one octave above it
                const float qb = juce::jlimit (0.0f, 1.0f, std::log2 (juce::jmax (1.0e-3f, fc / fMin)));
                res = 1.0f / (1.307f + (resQ - 1.307f) * qb);
                // a cutoff below the fundamental leaves a fading sine (-12.8 dB/oct); resonance costs
                // level, partly made up by the peak once the corner is clear of the fundamental
                const float below = juce::jmax (0.0f, std::log2 (f0) - oct6);
                const float loss = -12.8f * juce::jmin (below, 2.9f) + 13.0f * juce::jmax (0.0f, below - 2.9f);
                tvfGain = std::pow (10.0f, (juce::jmax (-38.0f, juce::jmin (0.0f, loss)) + resDb * (1.0f - 0.3f * qb)) / 20.0f);
                gCut = std::tan (juce::MathConstants<float>::pi * hz / (float) sr);
            }
            // TVA runs in the log domain: 0.375 dB per level unit, level 0 = off
            const float v = tva.value;
            const float envGain = v <= 0.0f ? 0.0f : std::pow (10.0f, (v - 1.0f) * 37.5f / 20.0f) * juce::jmin (1.0f, v * 100.0f);
            const float target = envGain * levelGain * tvfGain;
            gainStep = (target - gain) / (float) kCtl;
            if (tva.done) finished = true;
        }

        const uint8_t* P = nullptr;
        const Rom* romRef = nullptr;
        double sr = 44100.0, phase = 0.0, pos = 0.0, lfoPhase = 0.0, pitchCtl = 1.0;
        PcmWave wave;
        bool saw = false, finished = false;
        float duty = 0.5f, basePitch = 60.0f, penvRange = 0, lfoHz = 1, lfoDepth = 0, lfoModSens = 0;
        float tvfGain = 1.0f, cutoffBase = 100.0f, cutoffShift = 0.0f, res = 0.765f, resQ = 1.307f, resDb = 0.0f, tvfDepth = 0.0f, gCut = 0.5f, levelGain = 1.0f, gain = 0.0f, gainStep = 0.0f;
        int ctl = 0;
        Env penv, tvf, tva;
        Lp2 lp, lp2;
    };

    // partial-pair structures 1..13: which partial is PCM, and how the pair is mixed
    inline void structure (int s, bool& firstPcm, bool& secondPcm, int& mix)
    {
        static const uint8_t pcmBits[13] = { 0, 0, 2, 2, 1, 3, 3, 0, 3, 0, 2, 1, 3 };
        static const uint8_t mixType[13] = { 0, 1, 0, 1, 1, 0, 1, 0, 0, 2, 2, 2, 2 };   // 0 a+b, 1 a+ring, 2 ring
        s = juce::jlimit (0, 12, s);
        firstPcm = (pcmBits[s] & 2) != 0; secondPcm = (pcmBits[s] & 1) != 0; mix = mixType[s];
    }

    class Voice
    {
    public:
        void noteOn (std::shared_ptr<const Rom> r, const Timbre& t, int key, float vel, const Macros& m, double sampleRate)
        {
            rom = std::move (r); timbre = t; released = false;
            if (rom == nullptr) return;
            const bool noSustain = timbre.common[13] == 1;
            for (int pair = 0; pair < 2; ++pair)
            {
                bool aPcm, bPcm; structure (timbre.common[10 + pair], aPcm, bPcm, mix[pair]);
                if (! timbre.partialOn (pair * 2) || ! timbre.partialOn (pair * 2 + 1)) mix[pair] = 0;
                for (int k = 0; k < 2; ++k)
                {
                    const int idx = pair * 2 + k;
                    part[idx].active = false;
                    if (timbre.partialOn (idx))
                        part[idx].noteOn (*rom, timbre.partial[idx], k == 0 ? aPcm : bPcm, key, vel, noSustain, m, sampleRate);
                }
            }
        }
        void noteOff() { released = true; for (auto& p : part) p.noteOff(); }
        bool isDone() const
        {
            if (rom == nullptr) return true;
            for (auto& p : part) if (! p.isDone()) return false;
            return true;
        }
        float render (double pitchMult, float modWheel)
        {
            if (rom == nullptr) return 0.0f;
            float out = 0.0f;
            for (int pair = 0; pair < 2; ++pair)
            {
                const float a = part[pair * 2].render (pitchMult, modWheel);
                const float b = part[pair * 2 + 1].render (pitchMult, modWheel);
                switch (mix[pair])
                {
                    case 1:  out += a + 0.97f * a * b; break;   // partial + ring
                    case 2:  out += 1.78f * a * b; break;       // ring only
                    default: out += a + b; break;
                }
            }
            return out * 0.35f;
        }

    private:
        std::shared_ptr<const Rom> rom;
        Timbre timbre;
        Partial part[4];
        int mix[2] { 0, 0 };
        bool released = false;
    };
}
