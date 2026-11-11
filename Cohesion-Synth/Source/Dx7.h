#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <cmath>
#include <cstdint>
#include <cstring>

// ===========================================================================
//  COHESION - SYNTH · DX7-compatible 6-operator FM.
//  * Reads Yamaha DX7 SysEx: 32-voice bulk (VMEM, packed) and single voice
//    (VCED), with or without SysEx framing.
//  * Own FM engine: 32 algorithms, 4-rate/4-level operator envelopes,
//    ratio / fixed frequency, detune, feedback, velocity, keyboard level and
//    rate scaling, pitch EG and LFO (pitch + amplitude).
//  All code is original; DX7 behaviour is approximated from its documented
//  parameter ranges.
// ===========================================================================
namespace dx7
{
    // ------------------------------------------------------------------ patch
    struct Op
    {
        uint8_t r[4] {}, l[4] {};            // EG rates / levels 0..99
        uint8_t bp = 39, ld = 0, rd = 0;     // level scaling break point / depths
        uint8_t lc = 0, rc = 0;              // curves: 0 -LIN, 1 -EXP, 2 +EXP, 3 +LIN
        uint8_t rs = 0, ams = 0, kvs = 0;    // rate scaling 0..7, amp mod sens 0..3, key velocity sens 0..7
        uint8_t ol = 0, mode = 0;            // output level 0..99, 0 ratio / 1 fixed
        uint8_t coarse = 1, fine = 0, det = 7;
    };

    struct Patch
    {
        std::array<Op, 6> op;                // op[0] = OP1 ... op[5] = OP6
        uint8_t pr[4] { 99, 99, 99, 99 }, pl[4] { 50, 50, 50, 50 };
        uint8_t alg = 0, fb = 0, oks = 1;
        uint8_t lfoSpeed = 35, lfoDelay = 0, lfoPmd = 0, lfoAmd = 0, lfoSync = 1, lfoWave = 0, pms = 3;
        uint8_t transpose = 24;
        char name[11] {};

        juce::String getName() const { return juce::String (name).trimEnd(); }

        bool isBlank() const
        {
            const auto n = getName();
            if (n.isEmpty() || n.containsOnly ("- ") ) return true;
            for (auto& o : op) if (o.ol > 0) return false;
            return true;
        }

        // 155-byte VCED image (used to store the voice in the plugin state)
        std::array<uint8_t, 155> toVced() const
        {
            std::array<uint8_t, 155> d {};
            for (int k = 0; k < 6; ++k)
            {
                const auto& o = op[(size_t) (5 - k)];   // OP6 first
                uint8_t* p = d.data() + k * 21;
                for (int i = 0; i < 4; ++i) { p[i] = o.r[i]; p[4 + i] = o.l[i]; }
                p[8] = o.bp; p[9] = o.ld; p[10] = o.rd; p[11] = o.lc; p[12] = o.rc; p[13] = o.rs;
                p[14] = o.ams; p[15] = o.kvs; p[16] = o.ol; p[17] = o.mode; p[18] = o.coarse; p[19] = o.fine; p[20] = o.det;
            }
            uint8_t* g = d.data() + 126;
            for (int i = 0; i < 4; ++i) { g[i] = pr[i]; g[4 + i] = pl[i]; }
            g[8] = alg; g[9] = fb; g[10] = oks; g[11] = lfoSpeed; g[12] = lfoDelay; g[13] = lfoPmd; g[14] = lfoAmd;
            g[15] = lfoSync; g[16] = lfoWave; g[17] = pms; g[18] = transpose;
            for (int i = 0; i < 10; ++i) g[19 + i] = (uint8_t) (name[i] ? name[i] : ' ');
            return d;
        }
    };

    // DX7-style "INIT VOICE": algorithm 1, OP1 sine at full level
    inline Patch initVoice()
    {
        Patch p; p.alg = 0;
        for (auto& o : p.op) { o.ol = 0; for (int i = 0; i < 4; ++i) { o.r[i] = 99; o.l[i] = i == 3 ? 0 : 99; } }
        p.op[0].ol = 99;
        std::memcpy (p.name, "INIT VOICE", 10);
        return p;
    }

    inline uint8_t clampU (int v, int hi) { return (uint8_t) juce::jlimit (0, hi, v); }

    // single voice, VCED order (155 bytes, OP6 first)
    inline Patch fromVced (const uint8_t* d)
    {
        Patch pt;
        for (int k = 0; k < 6; ++k)
        {
            auto& o = pt.op[(size_t) (5 - k)];
            const uint8_t* p = d + k * 21;
            for (int i = 0; i < 4; ++i) { o.r[i] = clampU (p[i], 99); o.l[i] = clampU (p[4 + i], 99); }
            o.bp = clampU (p[8], 99); o.ld = clampU (p[9], 99); o.rd = clampU (p[10], 99);
            o.lc = clampU (p[11], 3); o.rc = clampU (p[12], 3); o.rs = clampU (p[13], 7);
            o.ams = clampU (p[14], 3); o.kvs = clampU (p[15], 7); o.ol = clampU (p[16], 99);
            o.mode = clampU (p[17], 1); o.coarse = clampU (p[18], 31); o.fine = clampU (p[19], 99); o.det = clampU (p[20], 14);
        }
        const uint8_t* g = d + 126;
        for (int i = 0; i < 4; ++i) { pt.pr[i] = clampU (g[i], 99); pt.pl[i] = clampU (g[4 + i], 99); }
        pt.alg = clampU (g[8], 31); pt.fb = clampU (g[9], 7); pt.oks = clampU (g[10], 1);
        pt.lfoSpeed = clampU (g[11], 99); pt.lfoDelay = clampU (g[12], 99); pt.lfoPmd = clampU (g[13], 99); pt.lfoAmd = clampU (g[14], 99);
        pt.lfoSync = clampU (g[15], 1); pt.lfoWave = clampU (g[16], 5); pt.pms = clampU (g[17], 7); pt.transpose = clampU (g[18], 48);
        for (int i = 0; i < 10; ++i) { const char c = (char) (g[19 + i] & 0x7f); pt.name[i] = (c >= 32 && c < 127) ? c : ' '; }
        return pt;
    }

    // one voice of a 32-voice bulk dump (VMEM, 128 bytes packed, OP6 first)
    inline Patch fromVmem (const uint8_t* v)
    {
        Patch pt;
        for (int k = 0; k < 6; ++k)
        {
            auto& o = pt.op[(size_t) (5 - k)];
            const uint8_t* p = v + k * 17;
            for (int i = 0; i < 4; ++i) { o.r[i] = clampU (p[i] & 0x7f, 99); o.l[i] = clampU (p[4 + i] & 0x7f, 99); }
            o.bp = clampU (p[8] & 0x7f, 99); o.ld = clampU (p[9] & 0x7f, 99); o.rd = clampU (p[10] & 0x7f, 99);
            o.lc = (uint8_t) (p[11] & 3); o.rc = (uint8_t) ((p[11] >> 2) & 3);
            o.rs = (uint8_t) (p[12] & 7); o.det = clampU ((p[12] >> 3) & 15, 14);
            o.ams = (uint8_t) (p[13] & 3); o.kvs = (uint8_t) ((p[13] >> 2) & 7);
            o.ol = clampU (p[14] & 0x7f, 99);
            o.mode = (uint8_t) (p[15] & 1); o.coarse = (uint8_t) ((p[15] >> 1) & 31);
            o.fine = clampU (p[16] & 0x7f, 99);
        }
        for (int i = 0; i < 4; ++i) { pt.pr[i] = clampU (v[102 + i] & 0x7f, 99); pt.pl[i] = clampU (v[106 + i] & 0x7f, 99); }
        pt.alg = (uint8_t) (v[110] & 31);
        pt.fb = (uint8_t) (v[111] & 7); pt.oks = (uint8_t) ((v[111] >> 3) & 1);
        pt.lfoSpeed = clampU (v[112] & 0x7f, 99); pt.lfoDelay = clampU (v[113] & 0x7f, 99);
        pt.lfoPmd = clampU (v[114] & 0x7f, 99); pt.lfoAmd = clampU (v[115] & 0x7f, 99);
        pt.lfoSync = (uint8_t) (v[116] & 1); pt.lfoWave = clampU ((v[116] >> 1) & 7, 5); pt.pms = (uint8_t) ((v[116] >> 4) & 7);
        pt.transpose = clampU (v[117] & 0x7f, 48);
        for (int i = 0; i < 10; ++i) { const char c = (char) (v[118 + i] & 0x7f); pt.name[i] = (c >= 32 && c < 127) ? c : ' '; }
        return pt;
    }

    struct ParseResult { std::vector<Patch> voices; int banks = 0, singles = 0, badChecksums = 0; };

    // Accepts SysEx files with one or more messages, or a raw 4096-byte VMEM dump.
    inline ParseResult parseSysEx (const void* data, size_t size)
    {
        ParseResult res;
        auto* b = static_cast<const uint8_t*> (data);
        auto checksum = [] (const uint8_t* p, size_t n) { int s = 0; for (size_t i = 0; i < n; ++i) s += p[i]; return (uint8_t) ((-s) & 0x7f); };

        bool foundFramed = false;
        for (size_t i = 0; i + 6 <= size; ++i)
        {
            if (b[i] != 0xF0 || b[i + 1] != 0x43) continue;
            const uint8_t fmt = b[i + 3];
            const size_t len = ((size_t) b[i + 4] << 7) | b[i + 5];
            if ((b[i + 2] & 0xF0) != 0x00) continue;
            if (fmt == 9 && len == 4096 && i + 6 + 4096 < size)
            {
                const uint8_t* d = b + i + 6;
                if (checksum (d, 4096) != (b[i + 6 + 4096] & 0x7f)) ++res.badChecksums;
                for (int v = 0; v < 32; ++v) res.voices.push_back (fromVmem (d + v * 128));
                ++res.banks; foundFramed = true; i += 6 + 4096;
            }
            else if (fmt == 0 && len == 155 && i + 6 + 155 < size)
            {
                const uint8_t* d = b + i + 6;
                if (checksum (d, 155) != (b[i + 6 + 155] & 0x7f)) ++res.badChecksums;
                res.voices.push_back (fromVced (d));
                ++res.singles; foundFramed = true; i += 6 + 155;
            }
        }
        if (! foundFramed && size == 4096)
        {
            for (int v = 0; v < 32; ++v) res.voices.push_back (fromVmem (b + v * 128));
            ++res.banks;
        }
        return res;
    }

    // -------------------------------------------------------------- algorithms
    // mods[i] = bitmask of operators (bit k = OP k+1) that modulate OP i+1;
    // carrier bitmask; feedback operator and the operator whose output feeds it.
    struct Algo { uint8_t mods[6]; uint8_t carriers; int8_t fbOp, fbSrc; };

    inline const Algo& algorithm (int index)
    {
        auto B = [] (std::initializer_list<int> ops) { uint8_t m = 0; for (int o : ops) m |= (uint8_t) (1u << (o - 1)); return m; };
        static const Algo table[32] = {
            /* 1 */ { { B({2}), 0, B({4}), B({5}), B({6}), 0 }, B({1,3}),       6, 6 },
            /* 2 */ { { B({2}), 0, B({4}), B({5}), B({6}), 0 }, B({1,3}),       2, 2 },
            /* 3 */ { { B({2}), B({3}), 0, B({5}), B({6}), 0 }, B({1,4}),       6, 6 },
            /* 4 */ { { B({2}), B({3}), 0, B({5}), B({6}), 0 }, B({1,4}),       6, 4 },
            /* 5 */ { { B({2}), 0, B({4}), 0, B({6}), 0 }, B({1,3,5}),          6, 6 },
            /* 6 */ { { B({2}), 0, B({4}), 0, B({6}), 0 }, B({1,3,5}),          6, 5 },
            /* 7 */ { { B({2}), 0, B({4,5}), 0, B({6}), 0 }, B({1,3}),          6, 6 },
            /* 8 */ { { B({2}), 0, B({4,5}), 0, B({6}), 0 }, B({1,3}),          4, 4 },
            /* 9 */ { { B({2}), 0, B({4,5}), 0, B({6}), 0 }, B({1,3}),          2, 2 },
            /*10 */ { { B({2}), B({3}), 0, B({5,6}), 0, 0 }, B({1,4}),         3, 3 },
            /*11 */ { { B({2}), B({3}), 0, B({5,6}), 0, 0 }, B({1,4}),         6, 6 },
            /*12 */ { { B({2}), 0, B({4,5,6}), 0, 0, 0 }, B({1,3}),            2, 2 },
            /*13 */ { { B({2}), 0, B({4,5,6}), 0, 0, 0 }, B({1,3}),            6, 6 },
            /*14 */ { { B({2}), 0, B({4}), B({5,6}), 0, 0 }, B({1,3}),         6, 6 },
            /*15 */ { { B({2}), 0, B({4}), B({5,6}), 0, 0 }, B({1,3}),         2, 2 },
            /*16 */ { { B({2,3,5}), 0, B({4}), 0, B({6}), 0 }, B({1}),         6, 6 },
            /*17 */ { { B({2,3,5}), 0, B({4}), 0, B({6}), 0 }, B({1}),         2, 2 },
            /*18 */ { { B({2,3,4}), 0, 0, B({5}), B({6}), 0 }, B({1}),         3, 3 },
            /*19 */ { { B({2}), B({3}), 0, B({6}), B({6}), 0 }, B({1,4,5}),    6, 6 },
            /*20 */ { { B({3}), B({3}), 0, B({5,6}), 0, 0 }, B({1,2,4}),       3, 3 },
            /*21 */ { { B({3}), B({3}), 0, B({6}), B({6}), 0 }, B({1,2,4,5}),  3, 3 },
            /*22 */ { { B({2}), 0, B({6}), B({6}), B({6}), 0 }, B({1,3,4,5}),  6, 6 },
            /*23 */ { { 0, B({3}), 0, B({6}), B({6}), 0 }, B({1,2,4,5}),       6, 6 },
            /*24 */ { { 0, 0, B({6}), B({6}), B({6}), 0 }, B({1,2,3,4,5}),     6, 6 },
            /*25 */ { { 0, 0, 0, B({6}), B({6}), 0 }, B({1,2,3,4,5}),          6, 6 },
            /*26 */ { { 0, B({3}), 0, B({5,6}), 0, 0 }, B({1,2,4}),            6, 6 },
            /*27 */ { { 0, B({3}), 0, B({5,6}), 0, 0 }, B({1,2,4}),            3, 3 },
            /*28 */ { { B({2}), 0, B({4}), B({5}), 0, 0 }, B({1,3,6}),         5, 5 },
            /*29 */ { { 0, 0, B({4}), 0, B({6}), 0 }, B({1,2,3,5}),            6, 6 },
            /*30 */ { { 0, 0, B({4}), B({5}), 0, 0 }, B({1,2,3,6}),            5, 5 },
            /*31 */ { { 0, 0, 0, 0, B({6}), 0 }, B({1,2,3,4,5}),               6, 6 },
            /*32 */ { { 0, 0, 0, 0, 0, 0 }, B({1,2,3,4,5,6}),                  6, 6 },
        };
        return table[juce::jlimit (0, 31, index)];
    }

    // ------------------------------------------------------------ level maths
    // DX7 output / EG levels are logarithmic: one unit = 0.75 dB, 99 -> 127 units.
    inline int scaleLevel (int l)
    {
        static const int low[20] = { 0, 5, 9, 13, 17, 20, 23, 25, 27, 29, 31, 33, 35, 37, 39, 41, 42, 43, 45, 46 };
        l = juce::jlimit (0, 99, l);
        return l < 20 ? low[l] : l + 28;
    }
    inline float unitsToGain (float u) { return u <= 0.0f ? 0.0f : std::exp2 ((u - 127.0f) * 0.75f / 6.0206f); }

    struct SineTable
    {
        static constexpr int N = 4096;
        float t[N + 1];
        SineTable() { for (int i = 0; i <= N; ++i) t[i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / N); }
        inline float at (double phaseCycles) const
        {
            double p = phaseCycles - std::floor (phaseCycles);
            const double x = p * N; const int i = (int) x; const float f = (float) (x - i);
            return t[i] + (t[i + 1] - t[i]) * f;
        }
    };
    inline const SineTable& sine() { static const SineTable s; return s; }

    // --------------------------------------------------------------- envelope
    struct Env
    {
        float level = 0, target = 0, speed = 0;      // level / target in 0..127 units
        int stage = 4;                                // 0..2 key-down, 3 release, 4 idle
        float rateUnits[4] {}, levelUnits[4] {};

        static float speedFor (int rate, int rateScaling, float timeScale)
        {
            const int q = juce::jmin (63, ((rate * 41) >> 6) + rateScaling);
            return 2.3f * std::exp2 ((float) q * 0.25f) / juce::jmax (0.05f, timeScale);   // units per second
        }

        void setup (const Op& o, int rateScaling, float timeScale)
        {
            for (int i = 0; i < 4; ++i) { rateUnits[i] = speedFor (o.r[i], rateScaling, timeScale); levelUnits[i] = (float) scaleLevel (o.l[i]); }
        }
        void noteOn (bool restart)
        {
            if (restart || stage == 4) level = levelUnits[3];
            enterStage (0);
        }
        void noteOff() { if (stage < 3) enterStage (3); }
        void enterStage (int s) { stage = s; if (s < 4) { target = levelUnits[s]; speed = rateUnits[s]; } }

        void advance (float dt)
        {
            if (stage >= 4) return;
            if (level < target)   // rising: fast exponential-ish attack, starting from a floor
            {
                if (level < 40.0f) level = 40.0f;
                level += speed * 3.0f * dt * (1.0f + (127.0f - level) / 32.0f);
                if (level >= target) { level = target; next(); }
            }
            else
            {
                level -= speed * dt;
                if (level <= target) { level = target; next(); }
            }
        }
        void next()
        {
            if (stage < 2)       enterStage (stage + 1);
            else if (stage == 2) speed = 0.0f;             // sustain at L3 until key-up
            else if (stage == 3) stage = 4;                // release finished at L4
        }
        bool idleAtZero() const { return stage == 4 && level <= 0.5f; }
    };

    // ------------------------------------------------------------------ voice
    struct Macros { float bright = 0.0f, time = 1.0f, vel = 1.0f; };

    class Voice
    {
    public:
        void prepare (double sampleRate) { sr = sampleRate; }

        void noteOn (const Patch& p, int midiNote, float velocity, const Macros& m, juce::Random& rng)
        {
            patch = p; macros = m;
            const auto& alg = algorithm (p.alg);
            nCarriers = 0; for (int i = 0; i < 6; ++i) if (alg.carriers & (1 << i)) ++nCarriers;
            outGain = 0.75f / std::sqrt ((float) juce::jmax (1, nCarriers));

            note = juce::jlimit (0, 127, midiNote + (int) p.transpose - 24);
            const double baseHz = 440.0 * std::pow (2.0, (note - 69) / 12.0);
            for (int i = 0; i < 6; ++i)
            {
                const auto& o = p.op[(size_t) i];
                double hz;
                if (o.mode == 1) hz = std::pow (10.0, (o.coarse & 3) + o.fine / 100.0);
                else             hz = baseHz * (o.coarse == 0 ? 0.5 : (double) o.coarse) * (1.0 + o.fine / 100.0);
                hz *= std::pow (2.0, ((int) o.det - 7) * 1.0 / 1200.0);
                inc[i] = hz / sr;
                if (p.oks || ! started) phase[i] = p.oks ? 0.0 : rng.nextDouble();

                // static attenuation (0.75 dB units): output level, velocity, keyboard level scaling, BRIGHT
                float units = (float) scaleLevel (o.ol);
                units -= (float) o.kvs * (1.0f - velocity) * 4.6f * m.vel;
                units += keyScale (o, note);
                if (! (alg.carriers & (1 << i))) units += m.bright * 20.0f;
                opUnits[i] = juce::jlimit (0.0f, 127.0f, units);

                const int rateScaling = (o.rs * juce::jlimit (0, 31, note / 3 - 7)) >> 3;
                env[i].setup (o, rateScaling, m.time);
                env[i].noteOn (true);
                fbHist[0] = fbHist[1] = 0.0f; out[i] = 0.0f;
                gain[i] = gainTarget[i] = 0.0f;
            }
            // pitch EG
            for (int k = 0; k < 4; ++k) { pegRate[k] = 0.4f * std::exp2 (p.pr[k] * 0.15f) / juce::jmax (0.05f, m.time); pegLevel[k] = (float) p.pl[k]; }
            peg = pegLevel[3]; pegStage = 0;

            // LFO
            lfoHz = 0.062f * std::exp2 (p.lfoSpeed * 9.6f / 99.0f);
            if (p.lfoSync || ! started) lfoPhase = p.lfoSync ? 0.0 : rng.nextDouble();
            lfoDelay = p.lfoDelay / 99.0f * 5.0f; lfoTime = 0.0f; sh = rng.nextFloat() * 2.0f - 1.0f;
            ctl = 0; released = false; started = true;
        }

        void noteOff()
        {
            released = true;
            for (auto& e : env) e.noteOff();
            pegStage = 3;
        }

        bool isDone() const
        {
            if (! released) return false;
            const auto& alg = algorithm (patch.alg);
            for (int i = 0; i < 6; ++i)
                if ((alg.carriers & (1 << i)) && ! (env[i].stage == 4 && (env[i].level + opUnits[i] - 127.0f) < 8.0f)) return false;
            return true;
        }

        // one sample; pitchMult from the synth's pitch modulation
        float next (double pitchMult)
        {
            if (ctl-- <= 0) controlUpdate();
            const auto& alg = algorithm (patch.alg);
            const double pm = pitchMult * pitchCtl;
            const float fbAmt = patch.fb ? 0.5f * std::exp2 ((float) patch.fb - 7.0f) : 0.0f;
            const auto& sn = sine();
            float sum = 0.0f;
            for (int i = 5; i >= 0; --i)
            {
                gain[i] += gainStep[i];
                double mod = 0.0;
                const uint8_t m = alg.mods[i];
                if (m) for (int k = 0; k < 6; ++k) if (m & (1 << k)) mod += out[k];
                if (i == alg.fbOp - 1 && fbAmt > 0.0f) mod += fbAmt * 0.5f * (fbHist[0] + fbHist[1]);
                const float y = sn.at (phase[i] + mod) * gain[i];
                phase[i] += inc[i] * pm; if (phase[i] >= 1.0) phase[i] -= 1.0;
                out[i] = y;
                if (i == alg.fbSrc - 1) { fbHist[1] = fbHist[0]; fbHist[0] = y; }
                if (alg.carriers & (1 << i)) sum += y;
            }
            return sum * outGain;
        }

    private:
        static float keyScale (const Op& o, int note)
        {
            const int dist = note - (o.bp + 21);
            const int depth = dist < 0 ? o.ld : o.rd;
            const int curve = dist < 0 ? o.lc : o.rc;
            if (depth == 0 || dist == 0) return 0.0f;
            const float oct = std::abs (dist) / 12.0f;
            float u = (curve == 0 || curve == 3) ? depth * 0.318f * oct
                                                 : depth * 0.318f * oct * oct * 0.5f;
            u = juce::jmin (u, 127.0f);
            return (curve == 0 || curve == 1) ? -u : u;    // -LIN / -EXP attenuate, +EXP / +LIN boost
        }

        static constexpr int kCtl = 8;   // control-rate interval (samples)

        void controlUpdate()
        {
            ctl = kCtl - 1;
            const float dt = (float) (kCtl / sr);

            // LFO
            lfoTime += dt;
            const double prevPh = lfoPhase;
            lfoPhase += lfoHz * dt; if (lfoPhase >= 1.0) lfoPhase -= 1.0;
            if (lfoPhase < prevPh) sh = rnd.nextFloat() * 2.0f - 1.0f;
            float l;
            switch (patch.lfoWave)
            {
                case 0:  l = (float) (lfoPhase < 0.5 ? 4.0 * lfoPhase - 1.0 : 3.0 - 4.0 * lfoPhase); break;   // triangle
                case 1:  l = (float) (1.0 - 2.0 * lfoPhase); break;                                           // saw down
                case 2:  l = (float) (2.0 * lfoPhase - 1.0); break;                                           // saw up
                case 3:  l = lfoPhase < 0.5 ? 1.0f : -1.0f; break;                                            // square
                case 4:  l = (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase); break;         // sine
                default: l = sh; break;                                                                       // S&H
            }
            const float fade = lfoTime < lfoDelay ? 0.0f : juce::jmin (1.0f, (lfoTime - lfoDelay) / 0.5f);
            l *= fade;
            static const float pmsTab[8] = { 0, 10, 20, 33, 55, 92, 153, 255 };
            static const float amsTab[4] = { 0, 66, 109, 255 };
            const float pitchSemis = l * (patch.lfoPmd / 99.0f) * (pmsTab[patch.pms] / 255.0f) * 12.0f;
            const float ampLfo = 0.5f * (l + 1.0f) * fade;

            // pitch EG
            if (pegStage < 4)
            {
                const float tgt = pegStage < 3 ? pegLevel[pegStage] : pegLevel[3];
                const float sp = pegRate[pegStage < 3 ? pegStage : 3] * dt;
                if (std::abs (tgt - peg) <= sp) { peg = tgt; if (pegStage < 2) ++pegStage; else if (pegStage == 3) pegStage = 4; }
                else peg += (tgt > peg ? sp : -sp);
            }
            const float pegSemis = (peg - 50.0f) / 50.0f * 48.0f;
            pitchCtl = std::exp2 ((pitchSemis + pegSemis) / 12.0);

            // operator gains (ramped linearly over the control interval)
            for (int i = 0; i < 6; ++i)
            {
                env[i].advance (dt);
                float u = env[i].level + opUnits[i] - 127.0f;
                u -= amsTab[patch.op[(size_t) i].ams] / 255.0f * (patch.lfoAmd / 99.0f) * ampLfo * 64.0f;
                gainTarget[i] = unitsToGain (u);
                gainStep[i] = (gainTarget[i] - gain[i]) / (float) kCtl;
            }
        }

        double sr = 44100.0;
        Patch patch;
        Macros macros;
        int note = 60, nCarriers = 1, ctl = 0;
        float outGain = 0.75f;
        double phase[6] {}, inc[6] {};
        float out[6] {}, gain[6] {}, gainTarget[6] {}, gainStep[6] {}, opUnits[6] {};
        float fbHist[2] {};
        Env env[6];
        float pegRate[4] {}, pegLevel[4] {}, peg = 50.0f; int pegStage = 4;
        double lfoPhase = 0.0, pitchCtl = 1.0; float lfoHz = 1.0f, lfoDelay = 0.0f, lfoTime = 0.0f, sh = 0.0f;
        bool released = false, started = false;
        juce::Random rnd;
    };
}
