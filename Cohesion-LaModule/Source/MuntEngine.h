#pragma once

#include "ModuleEngine.h"

// ===========================================================================
//  MUNT engine. Munt (mt32emu, LGPL 2.1+, https://github.com/munt/munt) is NOT
//  part of this plugin's code: it ships as a separate mt32emu.dll next to the
//  plugin binary and is loaded at run time through its public C interface.
//  Without the DLL the module simply offers the COHESION LA engine only.
//
//  Multi-out: one Munt instance per part (9). Each instance only receives its
//  own part's channel messages plus every SysEx, so instance k plays part k.
//  Munt's reverb is linear, so the sum of the nine outputs equals one unit.
// ===========================================================================
namespace munt
{
    // the part of the mt32emu C interface we use (declared from its public header)
    struct DacStreams { float *nonReverbL, *nonReverbR, *reverbDryL, *reverbDryR, *reverbWetL, *reverbWetR; };
    struct ReportHandler { const void* v0; };
    using Ctx = void*;

    struct Lib
    {
        juce::DynamicLibrary dll;
        bool ok = false;
        juce::String error;
        Ctx      (*createContext) (ReportHandler, void*) = nullptr;
        void     (*freeContext) (Ctx) = nullptr;
        int      (*addRomData) (Ctx, const uint8_t*, size_t, const void*) = nullptr;
        void     (*setAnalogOutputMode) (Ctx, int) = nullptr;
        void     (*setMidiDelayMode) (Ctx, int) = nullptr;
        uint32_t (*setMidiEventQueueSize) (Ctx, uint32_t) = nullptr;
        int      (*openSynth) (Ctx) = nullptr;
        void     (*closeSynth) (Ctx) = nullptr;
        uint32_t (*renderedCount) (Ctx) = nullptr;
        int      (*playMsgAt) (Ctx, uint32_t, uint32_t) = nullptr;
        int      (*playSysexAt) (Ctx, const uint8_t*, uint32_t, uint32_t) = nullptr;
        void     (*renderFloatStreams) (Ctx, const DacStreams*, uint32_t) = nullptr;
        uint32_t (*partialCount) (Ctx) = nullptr;
        void     (*partialStates) (Ctx, uint8_t*) = nullptr;

        static juce::File dllFile()
        {
            return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("mt32emu.dll");
        }

        static Lib& get()
        {
            static Lib lib;                       // loaded once per process, kept for its lifetime
            return lib;
        }

    private:
        Lib()
        {
            const auto f = dllFile();
            if (! f.existsAsFile()) { error = "mt32emu.dll not found next to the plugin"; return; }
            if (! dll.open (f.getFullPathName())) { error = "mt32emu.dll could not be loaded"; return; }
            bool all = true;
            auto bind = [&] (auto& fn, const char* name)
            {
                fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (dll.getFunction (name));
                if (fn == nullptr) all = false;
            };
            bind (createContext, "mt32emu_create_context");
            bind (freeContext, "mt32emu_free_context");
            bind (addRomData, "mt32emu_add_rom_data");
            bind (setAnalogOutputMode, "mt32emu_set_analog_output_mode");
            bind (setMidiDelayMode, "mt32emu_set_midi_delay_mode");
            bind (setMidiEventQueueSize, "mt32emu_set_midi_event_queue_size");
            bind (openSynth, "mt32emu_open_synth");
            bind (closeSynth, "mt32emu_close_synth");
            bind (renderedCount, "mt32emu_get_internal_rendered_sample_count");
            bind (playMsgAt, "mt32emu_play_msg_at");
            bind (playSysexAt, "mt32emu_play_sysex_at");
            bind (renderFloatStreams, "mt32emu_render_float_streams");
            bind (partialCount, "mt32emu_get_partial_count");
            bind (partialStates, "mt32emu_get_partial_states");
            ok = all;
            if (! ok) error = "mt32emu.dll is missing functions (unsupported version)";
        }
    };

    // One Munt synth at 32 kHz plus a cubic resampler to the host rate.
    struct Instance
    {
        Ctx ctx = nullptr;
        int midiChannel = 0;                     // the channel this instance's part listens to
        static constexpr int kRing = 8192, kChunk = 64;
        std::vector<float> ring[4];              // dry L/R, wet L/R
        double readPos = 0.0;                    // absolute Munt sample index of the next output
        uint32_t written = 0;

        bool open (const juce::MemoryBlock& control, const juce::MemoryBlock& pcm)
        {
            auto& L = Lib::get();
            close();
            if (! L.ok) return false;
            ctx = L.createContext ({ nullptr }, nullptr);
            if (ctx == nullptr) return false;
            L.addRomData (ctx, static_cast<const uint8_t*> (control.getData()), control.getSize(), nullptr);
            L.addRomData (ctx, static_cast<const uint8_t*> (pcm.getData()), pcm.getSize(), nullptr);
            L.setAnalogOutputMode (ctx, 0);      // digital only: 32 kHz DAC streams
            L.setMidiEventQueueSize (ctx, 4096);
            if (L.openSynth (ctx) != 0) { L.freeContext (ctx); ctx = nullptr; return false; }
            L.setMidiDelayMode (ctx, 0);         // immediate (timing comes from the host)
            for (auto& r : ring) r.assign (kRing, 0.0f);
            written = L.renderedCount (ctx);
            readPos = (double) written;
            return true;
        }
        void close()
        {
            if (ctx != nullptr) { auto& L = Lib::get(); L.closeSynth (ctx); L.freeContext (ctx); ctx = nullptr; }
        }
        ~Instance() { close(); }

        void renderChunk()
        {
            float s[6][kChunk];
            DacStreams st { s[0], s[1], s[2], s[3], s[4], s[5] };
            Lib::get().renderFloatStreams (ctx, &st, kChunk);
            for (int i = 0; i < kChunk; ++i)
            {
                const int w = (int) ((written + (uint32_t) i) & (kRing - 1));
                ring[0][(size_t) w] = s[0][i] + s[2][i];
                ring[1][(size_t) w] = s[1][i] + s[3][i];
                ring[2][(size_t) w] = s[4][i];
                ring[3][(size_t) w] = s[5][i];
            }
            written += kChunk;
        }

        // n output samples at `ratio` Munt samples per output sample
        void pull (float* dl, float* dr, float* wl, float* wr, int n, double ratio)
        {
            for (int i = 0; i < n; ++i)
            {
                const auto i0 = (uint32_t) readPos;
                while ((int32_t) (written - (i0 + 3)) <= 0) renderChunk();
                const float t = (float) (readPos - (double) i0);
                float* o[4] = { dl, dr, wl, wr };
                for (int c = 0; c < 4; ++c)
                {
                    const auto& r = ring[c];
                    auto at = [&] (uint32_t k) { return r[(size_t) (k & (kRing - 1))]; };
                    const float ym1 = at (i0 - 1), y0 = at (i0), y1 = at (i0 + 1), y2 = at (i0 + 2);
                    const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.0f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
                    o[c][i] = ((c3 * t + c2) * t + c1) * t + y0;
                }
                readPos += ratio;
            }
        }
    };
}

class MuntEngine : public ModuleEngine
{
public:
    static bool available() { return munt::Lib::get().ok; }
    static juce::String unavailableReason() { return munt::Lib::get().error; }

    // can Munt play this control + PCM ROM pair? (opens one test unit)
    static bool canOpen (const juce::File& controlRom, const juce::File& pcmRom)
    {
        juce::MemoryBlock c, p;
        if (! controlRom.loadFileAsData (c) || ! pcmRom.loadFileAsData (p)) return false;
        return canOpenData (c, p);
    }
    static bool canOpenData (const juce::MemoryBlock& c, const juce::MemoryBlock& p)
    {
        if (! available()) return false;
        munt::Instance test;
        const bool ok = test.open (c, p);
        test.close();
        return ok;
    }

    bool fxIsReverbWet() const override { return true; }
    bool isOpen() const { return opened; }

    // (message thread, audio stopped) open nine instances on a ROM pair
    bool open (const juce::File& controlRom, const juce::File& pcmRom)
    {
        opened = false;
        juce::MemoryBlock c, p;
        if (! available() || ! controlRom.loadFileAsData (c) || ! pcmRom.loadFileAsData (p)) return false;
        for (int k = 0; k < mt::kParts; ++k)
        {
            inst[k].midiChannel = k < 8 ? k + 1 : 9;
            if (! inst[k].open (c, p)) { for (auto& i : inst) i.close(); return false; }
        }
        opened = true;
        return true;
    }
    void close() { opened = false; for (auto& i : inst) i.close(); }

    void prepare (double s, int) override { sr = s; }

    void allSoundOff() override
    {
        if (! opened) return;
        for (auto& in : inst)
            for (uint32_t cc : { 120u, 123u, 121u })
                munt::Lib::get().playMsgAt (in.ctx, 0xB0u | (uint32_t) in.midiChannel | cc << 8, stamp (in, 0));
    }

    void message (int part, const uint8_t* msg, int len, int offset) override
    {
        if (! opened || part < 0 || part >= mt::kParts || len < 1) return;
        auto& in = inst[part];
        uint32_t m = (uint32_t) ((msg[0] & 0xF0) | in.midiChannel);
        if (len > 1) m |= (uint32_t) (msg[1] & 127) << 8;
        if (len > 2) m |= (uint32_t) (msg[2] & 127) << 16;
        munt::Lib::get().playMsgAt (in.ctx, m, stamp (in, offset));
    }

    void sysex (const uint8_t* d, int len, int offset) override
    {
        if (! opened) return;
        for (auto& in : inst) munt::Lib::get().playSysexAt (in.ctx, d, (uint32_t) len, stamp (in, offset));
        // the plugin routes parts itself: keep Munt's channel assignment at its defaults
        static const uint8_t chans[9] = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        const auto fix = mt::Memory::dt1 (mt::kSystem + 13, chans, 9);
        for (auto& in : inst) munt::Lib::get().playSysexAt (in.ctx, fix.data(), (uint32_t) fix.size(), stamp (in, offset));
    }

    // a SysEx for one part's instance only (per-part system settings such as the reverb)
    void sysexTo (int part, const uint8_t* d, int len, int offset)
    {
        if (! opened || part < 0 || part >= mt::kParts) return;
        munt::Lib::get().playSysexAt (inst[part].ctx, d, (uint32_t) len, stamp (inst[part], offset));
    }

    void render (const PartBuffers& out, int n) override
    {
        const double ratio = 32000.0 / sr;
        for (int k = 0; k < mt::kParts; ++k)
        {
            if (! opened) { for (float* b : { out.dryL[k], out.dryR[k], out.fxL[k], out.fxR[k] }) juce::FloatVectorOperations::clear (b, n); continue; }
            inst[k].pull (out.dryL[k], out.dryR[k], out.fxL[k], out.fxR[k], n, ratio);
        }
        if (opened && (++meterTick & 7) == 0)
        {
            int c = 0; uint8_t st[64];
            for (int k = 0; k < mt::kParts; ++k)
            {
                auto& in = inst[k];
                const int np = (int) juce::jmin (256u, munt::Lib::get().partialCount (in.ctx));
                munt::Lib::get().partialStates (in.ctx, st);
                int pc = 0;
                for (int i = 0; i < np; ++i) if ((st[i >> 2] >> ((i & 3) * 2)) & 3) ++pc;
                partCount[k].store (pc, std::memory_order_relaxed);
                c += pc;
            }
            active.store (c, std::memory_order_relaxed);
        }
    }

    int activePartials() const override { return active.load (std::memory_order_relaxed); }
    int partPartials (int part) const { return part >= 0 && part < mt::kParts ? partCount[part].load (std::memory_order_relaxed) : 0; }

private:
    static constexpr double kLatency = 128.0;   // Munt samples (4 ms): keeps event timing inside a block
    munt::Instance inst[mt::kParts];
    bool opened = false;
    double sr = 44100.0;
    int meterTick = 0;
    std::atomic<int> active { 0 };
    std::atomic<int> partCount[mt::kParts] {};

    uint32_t stamp (const munt::Instance& in, int offset) const
    {
        return (uint32_t) (in.readPos + offset * (32000.0 / sr) + kLatency);
    }
};
