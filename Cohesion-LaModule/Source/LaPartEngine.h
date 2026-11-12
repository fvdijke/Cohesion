#pragma once

#include "ModuleEngine.h"

// ===========================================================================
//  COHESION LA engine: nine MT-32 parts on the own LA voice (Source/La32.h of
//  COHESION - SYNTH), 32 partials shared by all parts like the hardware.
// ===========================================================================
class LaPartEngine : public ModuleEngine
{
public:
    explicit LaPartEngine (mt::Memory& m) : mem (m) {}

    bool fxIsReverbWet() const override { return false; }

    void prepare (double s, int) override { sr = s; allSoundOff(); }

    void allSoundOff() override
    {
        for (auto& v : voices) v.active = false;
        for (auto& p : parts) p.resetControllers();
        used = 0;
    }

    void message (int part, const uint8_t* msg, int len, int) override
    {
        if (len < 1 || part < 0 || part >= mt::kParts) return;
        auto& P = parts[part];
        const int type = msg[0] & 0xF0, d1 = len > 1 ? msg[1] & 127 : 0, d2 = len > 2 ? msg[2] & 127 : 0;
        switch (type)
        {
            case 0x90: if (d2 > 0) { noteOn (part, d1, d2); break; } [[fallthrough]];
            case 0x80: noteOff (part, d1); break;
            case 0xE0: P.bend = ((d2 << 7) | d1) - 8192; break;
            case 0xB0:
                switch (d1)
                {
                    case 1:   P.mod = d2 / 127.0f; break;
                    case 7:   P.volume = d2; break;
                    case 10:  P.panCc = d2; break;
                    case 11:  P.expression = d2; break;
                    case 64:  P.sustain = d2 >= 64; if (! P.sustain) releaseSustained (part); break;
                    case 120: for (auto& v : voices) if (v.active && v.part == part) v.active = false; break;
                    case 121: P.resetControllers(); releaseSustained (part); break;
                    case 123: for (auto& v : voices) if (v.active && v.part == part) { v.sustained = false; release (v); } break;
                    default: break;
                }
                break;
            default: break;                       // program change is handled by the memory
        }
    }

    void sysex (const uint8_t*, int, int) override {}   // reads the shared memory directly

    void render (const PartBuffers& out, int n) override
    {
        for (int p = 0; p < mt::kParts; ++p)
            for (float* b : { out.dryL[p], out.dryR[p], out.fxL[p], out.fxR[p] }) if (b) juce::FloatVectorOperations::clear (b, n);

        const float master = dbGain (la32::tvaLevelDb (mem.masterVolume()));
        double pm[mt::kParts]; float gainP[mt::kParts], modP[mt::kParts];
        for (int p = 0; p < mt::kParts; ++p)
        {
            const uint8_t* pt = mem.patchTemp (p);
            const auto& P = parts[p];
            const float range = (float) juce::jmin (24, (int) pt[4]);
            pm[p] = std::exp2 ((P.bend / 8192.0 * range + (pt[3] - 50) / 100.0) / 12.0);
            const float vol = P.volume / 127.0f, ex = P.expression / 127.0f;
            gainP[p] = master * dbGain (la32::tvaLevelDb (juce::jmin (100, (int) pt[8]))) * vol * vol * ex * ex * kOutGain;
            modP[p] = P.mod;
        }

        used = 0;
        for (auto& v : voices)
        {
            if (! v.active) continue;
            const int p = v.part;
            float* dl = out.dryL[p]; float* dr = out.dryR[p]; float* fl = out.fxL[p]; float* fr = out.fxR[p];
            const float g = gainP[p] * v.gain;
            for (int i = 0; i < n; ++i)
            {
                float l, r;
                v.voice.renderStereo (pm[p], modP[p], v.panpot, l, r);
                l *= g; r *= g;
                dl[i] += l; dr[i] += r;
                if (v.reverb) { fl[i] += l; fr[i] += r; }
            }
            if (v.voice.isDone()) v.active = false;
            else used += v.partials;
        }
        activeCount.store (used, std::memory_order_relaxed);
    }

    int activePartials() const override { return activeCount.load (std::memory_order_relaxed); }

private:
    static constexpr float kOutGain = 2.14f;       // +6.6 dB: same level as the MUNT engine (median over the timbres)
    static constexpr int kMaxVoices = 32, kPartialBudget = 32;
    static float dbGain (float db) { return db <= -150.0f ? 0.0f : std::pow (10.0f, db / 20.0f); }

    struct PartState
    {
        int volume = 100, expression = 127, panCc = -1, bend = 0; float mod = 0.0f; bool sustain = false;
        void resetControllers() { expression = 127; bend = 0; mod = 0.0f; sustain = false; }
    };
    struct V
    {
        la32::Voice voice; bool active = false, released = false, sustained = false, reverb = false;
        int part = 0, key = 0, partials = 0, panpot = 7; float gain = 1.0f; uint64_t age = 0;
    };

    mt::Memory& mem;
    double sr = 44100.0;
    PartState parts[mt::kParts];
    V voices[kMaxVoices];
    uint64_t clock = 0;
    int used = 0;
    std::atomic<int> activeCount { 0 };

    static int partialCount (const la32::Timbre& t) { int c = 0; for (int k = 0; k < 4; ++k) if (t.partialOn (k)) ++c; return c; }

    void release (V& v) { if (! v.released) { v.released = true; v.voice.noteOff(); } }
    void releaseSustained (int part)
    {
        for (auto& v : voices) if (v.active && v.part == part && v.sustained) { v.sustained = false; release (v); }
    }

    void noteOff (int part, int key)
    {
        for (auto& v : voices)
            if (v.active && v.part == part && v.key == key && ! v.released)
            {
                if (parts[part].sustain) v.sustained = true;
                else release (v);
            }
    }

    void noteOn (int part, int key, int vel)
    {
        if (mem.rom == nullptr) return;
        la32::Timbre t; float levelGain = 1.0f; int panpot = 7; bool rev;
        const uint8_t* pt = mem.patchTemp (part);
        int playKey = key;
        if (part == mt::kRhythmPart)
        {
            const auto rk = mem.rhythmKey (key);
            if (rk.timbre == 127 || rk.level == 0 || ! mem.rhythmTimbre (rk.timbre, t)) return;
            levelGain = dbGain (la32::tvaLevelDb (rk.level));
            panpot = rk.pan;                           // panpot 0 = right, 14 = left
            rev = rk.reverb;
        }
        else
        {
            t = mem.timbreTemp (part);
            playKey = juce::jlimit (0, 127, key + (int) pt[2] - 24);
            const int panCc = parts[part].panCc;
            // CC 10 is inverted on the MT-32 (127 = left); MT-32 divides by 9, CM-32L by 8.5
            const bool mt32 = mem.rom->model.startsWith ("MT");
            panpot = panCc >= 0 ? (mt32 ? panCc / 9 : (panCc * 8) / 68) : (int) pt[9];
            rev = pt[6] != 0;
        }
        const int need = partialCount (t);
        if (need == 0) return;

        // retrigger: the same key on the same part releases the previous note
        for (auto& v : voices) if (v.active && v.part == part && v.key == key && ! v.released) { v.sustained = false; release (v); }

        // partial budget and voice slots (steal the oldest released note, else the oldest note)
        auto countUsed = [&] { int c = 0; for (auto& v : voices) if (v.active) c += v.partials; return c; };
        int inUse = countUsed();
        V* slot = nullptr;
        for (auto& v : voices) if (! v.active) { slot = &v; break; }
        while (slot == nullptr || inUse + need > kPartialBudget)
        {
            V* victim = nullptr;
            for (auto& v : voices) if (v.active && v.released && (victim == nullptr || v.age < victim->age)) victim = &v;
            if (victim == nullptr) for (auto& v : voices) if (v.active && (victim == nullptr || v.age < victim->age)) victim = &v;
            if (victim == nullptr) break;
            victim->active = false; inUse -= victim->partials;
            if (slot == nullptr) slot = victim;
        }
        if (slot == nullptr) return;

        auto& v = *slot;
        v.voice.noteOn (mem.rom, t, playKey, vel / 127.0f, {}, sr);
        v.active = true; v.released = false; v.sustained = false;
        v.part = part; v.key = key; v.partials = need; v.age = ++clock;
        v.gain = levelGain; v.reverb = rev;
        v.panpot = juce::jlimit (0, 14, panpot);
    }
};
