#pragma once

#include "FluidLib.h"
#include "FontLibrary.h"
#include <set>

// ===========================================================================
//  One FluidSynth synth with 16 parts (= its 16 MIDI channels), each with its
//  own SoundFont, program, output pair and reverb / chorus group.
//
//  Loading: a SoundFont is first loaded by a temporary synth on a background
//  thread (FluidSynth keeps samples in a process-wide cache keyed by file), then
//  attached to the real synth, which then finds the samples in the cache - so
//  the audio thread is never blocked by reading a 1 GB file, and instances share
//  the sample data.
// ===========================================================================
class FontEngine : private juce::Thread
{
public:
    static constexpr int kParts = 16;

    struct Buffers
    {
        float* dryL[kParts] {}; float* dryR[kParts] {};
        float* revL[kParts] {}; float* revR[kParts] {};
        float* choL[kParts] {}; float* choR[kParts] {};
    };
    struct ReverbSettings { double room = 0.2, damp = 0.0, width = 0.5, level = 0.9; };
    struct ChorusSettings { int voices = 3; double level = 2.0, speed = 0.3, depth = 8.0; };

    FontEngine() : juce::Thread ("LA Font loader") {}
    ~FontEngine() override
    {
        signalThreadShouldExit(); notify(); stopThread (8000);
        const juce::ScopedLock sl (lock);
        destroySynth (synth, settings);
    }

    static bool available() { return fluid::Lib::get().ok; }
    static juce::String unavailableReason() { return fluid::Lib::get().error; }

    // ---------------------------------------------------------------- setup (message thread)
    void prepare (double sampleRate, float gain)
    {
        if (! available()) return;
        {
            const juce::ScopedLock sl (lock);
            if (synth != nullptr && sampleRate == sr) return;
            sr = sampleRate;
            fluid::Settings* s = nullptr;
            fluid::Synth* fresh = createSynth (sr, gain, s);
            // attach the loaded SoundFonts to the new synth while the old one keeps their samples cached
            std::map<juce::String, int> ids;
            for (auto& [path, id] : loaded) ids[path] = fluid::Lib::get().sfload (fresh, path.toRawUTF8(), 0);
            destroySynth (synth, settings);
            synth = fresh; settings = s; loaded = ids;
            for (auto& p : parts) p.dirty = true;
            for (int g = 0; g < kParts; ++g) { fxDirty[g] = true; }
        }
        if (! isThreadRunning()) startThread();
        notify();
    }

    // ---------------------------------------------------------------- part sound (message thread)
    void setPartFont (int part, const juce::String& path)
    {
        {
            const juce::ScopedLock sl (lock);
            auto& p = parts[part];
            p.path = path;
            p.info = path.isNotEmpty() ? sf2::get (path) : nullptr;
            p.state = path.isEmpty() ? empty : failed.count (path) ? failedToLoad : loading;
            p.dirty = true;
        }
        notify();
    }
    void setPartProgram (int part, int bank, int program)
    {
        auto& p = parts[part];
        p.bank = bank; p.program = program; p.dirty = true;
    }
    // lock-free reads for the UI
    enum State { empty = 0, loading, ready, failedToLoad };
    int partBank (int part) const    { return parts[part].bank; }
    int partProgram (int part) const { return parts[part].program; }
    State partState (int part) const { return (State) parts[part].state.load(); }

    void setReverb (int group, const ReverbSettings& r) { rev[group] = r; fxDirty[group] = true; }
    void setChorus (int group, const ChorusSettings& c) { cho[group] = c; fxDirty[group] = true; }

    // ---------------------------------------------------------------- audio thread
    // Everything below is called between tryBegin() (true) and end().
    bool tryBegin()
    {
        if (! lock.tryEnter()) return false;
        if (synth == nullptr) { lock.exit(); return false; }
        auto& L = fluid::Lib::get();
        for (int part = 0; part < kParts; ++part)
        {
            auto& p = parts[part];
            if (! p.dirty) continue;
            p.dirty = false;
            auto it = loaded.find (p.path);
            if (p.path.isEmpty() || it == loaded.end()) { L.allSoundsOff (synth, part); continue; }
            L.programSelect (synth, part, it->second, p.bank, p.program);
            p.state = ready;
        }
        for (int g = 0; g < kParts; ++g)
        {
            if (! fxDirty[g].exchange (false)) continue;
            L.setReverbRoom (synth, g, rev[g].room);   L.setReverbDamp (synth, g, rev[g].damp);
            L.setReverbWidth (synth, g, rev[g].width); L.setReverbLevel (synth, g, rev[g].level);
            L.setChorusNr (synth, g, cho[g].voices);   L.setChorusLevel (synth, g, cho[g].level);
            L.setChorusSpeed (synth, g, cho[g].speed); L.setChorusDepth (synth, g, cho[g].depth);
        }
        return true;
    }
    void end() { lock.exit(); }

    // A channel message for a part (the status byte's channel is ignored).
    void message (int part, const uint8_t* m, int len)
    {
        auto& L = fluid::Lib::get();
        auto& p = parts[part];
        const int type = m[0] & 0xF0, d1 = len > 1 ? m[1] & 127 : 0, d2 = len > 2 ? m[2] & 127 : 0;
        switch (type)
        {
            case 0x90: if (d2 > 0) { L.noteOn (synth, part, d1, d2); break; } [[fallthrough]];
            case 0x80: L.noteOff (synth, part, d1); break;
            case 0xA0: L.keyPressure (synth, part, d1, d2); break;
            case 0xD0: L.channelPressure (synth, part, d1); break;
            case 0xE0: L.pitchBend (synth, part, (d2 << 7) | d1); break;
            case 0xB0:
                if (d1 == 0) { p.bankMsb = d2; break; }           // bank select: applied by the next program change
                if (d1 == 32) break;
                L.cc (synth, part, d1, d2);
                break;
            case 0xC0: programChange (part, d1); break;
            default: break;
        }
    }

    // MIDI program change: the part's own SoundFont, bank from bank select (drum part: bank 128)
    void programChange (int part, int program)
    {
        auto& p = parts[part];
        int bank = part == 9 ? 128 : p.bankMsb;
        if (p.info != nullptr && p.info->find (bank, program) == nullptr)
            bank = p.info->find (part == 9 ? 128 : 0, program) != nullptr ? (part == 9 ? 128 : 0) : p.bank.load();
        p.bank = bank; p.program = program; p.dirty = false;
        auto it = loaded.find (p.path);
        if (it != loaded.end()) fluid::Lib::get().programSelect (synth, part, it->second, bank, program);
    }

    // GM / GS / XG reset: controllers off, every part back to its program 0
    void resetAll()
    {
        auto& L = fluid::Lib::get();
        L.systemReset (synth);
        for (int part = 0; part < kParts; ++part)
        {
            auto& p = parts[part];
            p.bankMsb = 0; p.bank = part == 9 ? 128 : 0; p.program = 0; p.dirty = true;
        }
    }
    void sysex (const uint8_t* d, int n)      // without F0 / F7
    {
        int handled = 0;
        fluid::Lib::get().sysex (synth, reinterpret_cast<const char*> (d), n, nullptr, nullptr, &handled, 0);
        // GM on / GS reset / XG on -> our parts follow
        const bool gmOn = n >= 4 && d[0] == 0x7E && d[2] == 0x09 && (d[3] == 0x01 || d[3] == 0x03);
        const bool gsReset = n >= 8 && d[0] == 0x41 && d[2] == 0x42 && d[3] == 0x12 && d[4] == 0x40 && d[5] == 0x00 && d[6] == 0x7F;
        const bool xgOn = n >= 6 && d[0] == 0x43 && d[2] == 0x4C && d[3] == 0x00 && d[4] == 0x00 && d[5] == 0x7E;
        if (gmOn || gsReset || xgOn) resetAll();
    }
    void allSoundsOff() { for (int part = 0; part < kParts; ++part) fluid::Lib::get().allSoundsOff (synth, part); }

    void render (const Buffers& b, int n)
    {
        float* out[2 * kParts]; float* fx[4 * kParts];
        for (int k = 0; k < kParts; ++k)
        {
            out[2 * k] = b.dryL[k]; out[2 * k + 1] = b.dryR[k];
            fx[4 * k] = b.revL[k]; fx[4 * k + 1] = b.revR[k]; fx[4 * k + 2] = b.choL[k]; fx[4 * k + 3] = b.choR[k];
        }
        for (auto* p : out) juce::FloatVectorOperations::clear (p, n);
        for (auto* p : fx)  juce::FloatVectorOperations::clear (p, n);
        fluid::Lib::get().process (synth, n, 4 * kParts, fx, 2 * kParts, out);
        if ((++meterTick & 7) == 0) countVoices();
    }

    int voices (int part) const { return partVoices[part].load (std::memory_order_relaxed); }
    int totalVoices() const     { return total.load (std::memory_order_relaxed); }
    static constexpr int kPolyphony = 256;

private:
    struct Part
    {
        juce::String path;                                   // guarded by lock
        std::shared_ptr<const sf2::Info> info;              // guarded by lock
        std::atomic<int> bank { 0 }, program { 0 };
        int bankMsb = 0;
        std::atomic<bool> dirty { true };
        std::atomic<int> state { empty };
    };

    mutable juce::CriticalSection lock;
    fluid::Synth* synth = nullptr;
    fluid::Settings* settings = nullptr;
    double sr = 0.0;
    Part parts[kParts];
    std::map<juce::String, int> loaded;                      // path -> SoundFont id in `synth`
    std::set<juce::String> failed;
    ReverbSettings rev[kParts]; ChorusSettings cho[kParts];
    std::atomic<bool> fxDirty[kParts] {};
    std::atomic<int> partVoices[kParts] {}, total { 0 };
    int meterTick = 0;

    static fluid::Synth* createSynth (double sampleRate, float gain, fluid::Settings*& s)
    {
        auto& L = fluid::Lib::get();
        s = L.newSettings();
        L.setNum (s, "synth.sample-rate", sampleRate);
        L.setInt (s, "synth.audio-channels", kParts);
        L.setInt (s, "synth.audio-groups", kParts);
        L.setInt (s, "synth.effects-groups", kParts);
        L.setInt (s, "synth.polyphony", kPolyphony);
        L.setInt (s, "synth.dynamic-sample-loading", 0);
        L.setNum (s, "synth.gain", gain);
        return L.newSynth (s);
    }
    static void destroySynth (fluid::Synth*& sy, fluid::Settings*& s)
    {
        auto& L = fluid::Lib::get();
        if (sy != nullptr) L.deleteSynth (sy);
        if (s != nullptr) L.deleteSettings (s);
        sy = nullptr; s = nullptr;
    }

    void countVoices()
    {
        fluid::Voice* list[kPolyphony];
        for (auto& v : list) v = nullptr;
        auto& L = fluid::Lib::get();
        L.getVoiceList (synth, list, kPolyphony, -1);
        int counts[kParts] {}; int t = 0;
        for (auto* v : list)
        {
            if (v == nullptr) break;
            if (! L.voicePlaying (v)) continue;
            const int ch = L.voiceChannel (v);
            if (ch >= 0 && ch < kParts) { ++counts[ch]; ++t; }
        }
        for (int k = 0; k < kParts; ++k) partVoices[k].store (counts[k], std::memory_order_relaxed);
        total.store (t, std::memory_order_relaxed);
    }

    // ---------------------------------------------------------------- loader thread
    void run() override
    {
        while (! threadShouldExit())
        {
            juce::String next;
            std::set<juce::String> used;
            {
                const juce::ScopedLock sl (lock);
                for (auto& p : parts) if (p.path.isNotEmpty()) used.insert (p.path);
                for (auto& u : used) if (loaded.count (u) == 0 && failed.count (u) == 0) { next = u; break; }
                // free SoundFonts no part uses any more
                for (auto it = loaded.begin(); it != loaded.end();)
                {
                    if (used.count (it->first) == 0 && synth != nullptr) { fluid::Lib::get().sfunload (synth, it->second, 0); it = loaded.erase (it); }
                    else ++it;
                }
            }
            if (next.isEmpty()) { wait (-1); continue; }

            // 1. load into a temporary synth: fills FluidSynth's sample cache, no lock held
            auto& L = fluid::Lib::get();
            fluid::Settings* ts = nullptr;
            fluid::Synth* temp = createSynth (44100.0, 0.2f, ts);
            const int tid = juce::File (next).existsAsFile() ? L.sfload (temp, next.toRawUTF8(), 0) : -1;
            // 2. attach to the real synth (samples come from the cache)
            {
                const juce::ScopedLock sl (lock);
                if (tid < 0 || synth == nullptr) failed.insert (next);
                else
                {
                    const int id = L.sfload (synth, next.toRawUTF8(), 0);
                    if (id < 0) failed.insert (next); else loaded[next] = id;
                }
                const bool bad = failed.count (next) > 0;
                for (auto& p : parts) if (p.path == next) { p.dirty = true; if (bad) p.state = failedToLoad; }
            }
            destroySynth (temp, ts);
        }
    }
};
