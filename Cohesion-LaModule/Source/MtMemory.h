#pragma once

#include "La32.h"

// ===========================================================================
//  COHESION - LA MODULE · MT-32 memory model.
//  The documented MT-32 / CM-32L address map (Roland DT1 SysEx, model 16H):
//    03 00 00  patch temp      9 x 16   (parts 1-8 + rhythm)
//    03 01 10  rhythm setup    85 x 4   (keys 24..108)
//    04 00 00  timbre temp     8 x 246
//    05 00 00  patch memory    128 x 8
//    08 00 00  timbre memory   64 x 256
//    10 00 00  system area     23 bytes
//    7F 00 00  reset
//  It is the single source of truth for both engines: the own LA engine reads it
//  directly, the Munt engine is fed the same messages (and a dump on resync).
// ===========================================================================
namespace mt
{
    constexpr int addr (int a, int b, int c) { return (a << 14) | (b << 7) | c; }
    constexpr int kPatchTemp = addr (3, 0, 0), kRhythmTemp = addr (3, 1, 16), kTimbreTemp = addr (4, 0, 0),
                  kPatches = addr (5, 0, 0), kTimbres = addr (8, 0, 0), kSystem = addr (16, 0, 0), kReset = addr (127, 0, 0);
    constexpr int kRhythmKeys = 85, kTimbreSize = 246, kSysSize = 23;
    constexpr int kMemSize = kSystem + kSysSize;
    constexpr int kParts = 9, kRhythmPart = 8;

    struct RhythmKey { int timbre = 127, level = 0, pan = 7; bool reverb = false; };

    class Memory
    {
    public:
        Memory() : m ((size_t) kMemSize, 0) {}

        std::shared_ptr<const la32::Rom> rom;
        int program[kParts] { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
        std::atomic<uint32_t> version { 0 };        // bumped on every change (UI refresh)
        bool systemChanged = false;                 // set by writes to the system area

        // ------------------------------------------------------------ init
        void init (std::shared_ptr<const la32::Rom> r)
        {
            rom = std::move (r);
            std::fill (m.begin(), m.end(), (uint8_t) 0);
            for (int i = 0; i < 128; ++i)
            {
                uint8_t* p = &m[(size_t) (kPatches + 8 * i)];
                p[0] = (uint8_t) (i / 64); p[1] = (uint8_t) (i % 64); p[2] = 24; p[3] = 50; p[4] = 12; p[5] = 0; p[6] = 1; p[7] = 0;
            }
            if (rom != nullptr)
                for (int i = 0; i < 64 && i < (int) rom->melodic.size(); ++i) putTimbre (kTimbres + 256 * i, rom->melodic[(size_t) i]);
            // rhythm setup from the ROM's key map
            for (int k = 0; k < kRhythmKeys; ++k)
            {
                uint8_t* q = &m[(size_t) (kRhythmTemp + 4 * k)];
                if (rom != nullptr && k < (int) rom->rhythmKeys.size())
                { const auto& e = rom->rhythmKeys[(size_t) k]; q[0] = e.timbre; q[1] = e.level; q[2] = e.pan; q[3] = e.reverb; }
                else { q[0] = 127; q[1] = 0; q[2] = 7; q[3] = 0; }
            }
            // system: master tune 440, room / time 5 / level 3, reserve, channels 2-10, volume 100
            static const uint8_t sys[kSysSize] = { 0x4A, 0, 5, 3, 3, 10, 6, 4, 3, 0, 0, 0, 6, 1, 2, 3, 4, 5, 6, 7, 8, 9, 100 };
            std::memcpy (&m[(size_t) kSystem], sys, kSysSize);
            static const char* defaults[8] = { "AcouPiano1", "Str Sect 1", "Acou Bass1", "Trumpet 1", "Slow Strng", "Elec Gtr 1", "Syn Brass1", "Flute 1" };
            for (int p = 0; p < 8; ++p)
            {
                uint8_t* t = patchTemp (p);
                t[8] = 100; t[9] = 7;
                programChange (p, findProgram (defaults[p], p * 8));
            }
            uint8_t* rt = patchTemp (kRhythmPart); rt[0] = 3; rt[2] = 24; rt[3] = 50; rt[4] = 12; rt[6] = 1; rt[8] = 100; rt[9] = 7;
            systemChanged = true;
            ++version;
        }

        int findProgram (const char* name, int fallback) const
        {
            if (rom != nullptr)
                for (int i = 0; i < (int) rom->melodic.size(); ++i)
                    if (rom->melodic[(size_t) i].getName().equalsIgnoreCase (name)) return i;
            return fallback;
        }

        // ------------------------------------------------------------ access
        uint8_t* patchTemp (int part)             { return &m[(size_t) (kPatchTemp + 16 * part)]; }
        const uint8_t* patchTemp (int part) const { return &m[(size_t) (kPatchTemp + 16 * part)]; }

        la32::Timbre timbreAt (int a) const
        {
            la32::Timbre t;
            std::memcpy (t.name, &m[(size_t) a], 10);
            std::memcpy (t.common, &m[(size_t) a], 14);
            for (int k = 0; k < 4; ++k) std::memcpy (t.partial[k], &m[(size_t) (a + 14 + 58 * k)], 58);
            return t;
        }
        la32::Timbre timbreTemp (int part) const { return timbreAt (kTimbreTemp + kTimbreSize * juce::jlimit (0, 7, part)); }

        // timbre by (group, number): 0 = A, 1 = B, 2 = memory, 3 = rhythm
        la32::Timbre timbreByNumber (int group, int num) const
        {
            num &= 63;
            if (group == 2) return timbreAt (kTimbres + 256 * num);
            if (rom != nullptr)
            {
                if (group == 3 && num < (int) rom->rhythmAll.size()) return rom->rhythmAll[(size_t) num];
                const int i = (group & 1) * 64 + num;
                if (group < 2 && i < (int) rom->melodic.size()) return rom->melodic[(size_t) i];
            }
            return {};
        }
        // rhythm timbre number 0..63 = memory, 64.. = rhythm group, 127 = off
        bool rhythmTimbre (int number, la32::Timbre& out) const
        {
            if (number < 64) { out = timbreAt (kTimbres + 256 * number); return true; }
            if (rom != nullptr && number - 64 < (int) rom->rhythmAll.size()) { out = rom->rhythmAll[(size_t) number - 64]; return out.getName() != "OFF"; }
            return false;
        }
        RhythmKey rhythmKey (int key) const
        {
            const int k = key - 24;
            if (k < 0 || k >= kRhythmKeys) return {};
            const uint8_t* q = &m[(size_t) (kRhythmTemp + 4 * k)];
            return { q[0], q[1], q[2], q[3] != 0 };
        }

        int reverbMode() const   { return m[(size_t) kSystem + 1] & 3; }
        int reverbTime() const   { return m[(size_t) kSystem + 2] & 7; }
        int reverbLevel() const  { return m[(size_t) kSystem + 3] & 7; }
        int masterVolume() const { return juce::jmin (100, (int) m[(size_t) kSystem + 22]); }
        int channelAssign (int part) const { return m[(size_t) kSystem + 13 + part]; }   // 0-15, 16 = off

        juce::String partName (int part) const
        {
            if (part == kRhythmPart) return "Rhythm";
            return timbreTemp (part).getName().trim();
        }

        // ------------------------------------------------------------ changes
        void programChange (int part, int pc)
        {
            if (part < 0 || part >= 8) return;
            pc &= 127;
            program[part] = pc;
            std::memcpy (patchTemp (part), &m[(size_t) (kPatches + 8 * pc)], 8);
            loadTimbreTemp (part);
            ++version;
        }

        // Roland DT1 for the MT-32 (model 16H). Returns true when the message was for us.
        bool sysex (const uint8_t* d, int n)
        {
            if (n > 0 && d[0] == 0xF0) { ++d; --n; }
            if (n > 0 && d[n - 1] == 0xF7) --n;
            if (n < 7 || d[0] != 0x41 || d[2] != 0x16 || d[3] != 0x12) return false;
            const int a = addr (d[4] & 127, d[5] & 127, d[6] & 127);
            const uint8_t* data = d + 7;
            int len = n - 8;                         // minus address and checksum
            if (len <= 0) { if (a == kReset) { auto r = rom; init (r); } return true; }
            int sum = 0; for (int i = 4; i < n; ++i) sum += d[i];
            if ((sum & 127) != 0) return true;      // bad checksum: ignored like the hardware
            write (a, data, len);
            return true;
        }

        void write (int a, const uint8_t* data, int len)
        {
            if (a == kReset) { auto r = rom; init (r); return; }
            if (a < 0 || a >= kMemSize) return;
            len = juce::jmin (len, kMemSize - a);
            std::memcpy (&m[(size_t) a], data, (size_t) len);
            // patch temp: reload the part's timbre when the timbre selection was touched
            if (a < kPatchTemp + 16 * kParts && a + len > kPatchTemp)
                for (int p = 0; p < 8; ++p)
                {
                    const int lo = kPatchTemp + 16 * p;
                    if (a <= lo + 1 && a + len > lo) loadTimbreTemp (p);
                }
            if (a < kSystem + kSysSize && a + len > kSystem) systemChanged = true;
            ++version;
        }

        // ------------------------------------------------------------ dumps
        // DT1 messages that rebuild this memory on another MT-32 (Munt resync). Channel
        // assignment is left at its defaults: part routing is done by the plugin.
        std::vector<std::vector<uint8_t>> dump() const
        {
            std::vector<std::vector<uint8_t>> out;
            auto region = [&] (int a, int len)
            {
                for (int off = 0; off < len; off += 128) out.push_back (dt1 (a + off, &m[(size_t) (a + off)], juce::jmin (128, len - off)));
            };
            region (kTimbres, 64 * 256);
            region (kPatches, 128 * 8);
            region (kPatchTemp, 16 * kParts);
            region (kRhythmTemp, 4 * kRhythmKeys);
            region (kTimbreTemp, kTimbreSize * 8);
            uint8_t sys[kSysSize]; std::memcpy (sys, &m[(size_t) kSystem], kSysSize);
            for (int i = 0; i < 9; ++i) sys[13 + i] = (uint8_t) (i + 1);
            out.push_back (dt1 (kSystem, sys, kSysSize));
            return out;
        }

        static std::vector<uint8_t> dt1 (int a, const uint8_t* data, int len)
        {
            std::vector<uint8_t> s { 0xF0, 0x41, 0x10, 0x16, 0x12, (uint8_t) ((a >> 14) & 127), (uint8_t) ((a >> 7) & 127), (uint8_t) (a & 127) };
            int sum = s[5] + s[6] + s[7];
            for (int i = 0; i < len; ++i) { s.push_back (data[i]); sum += data[i]; }
            s.push_back ((uint8_t) ((128 - (sum & 127)) & 127)); s.push_back (0xF7);
            return s;
        }
        static std::vector<uint8_t> systemWrite (int offset, int value)
        {
            const uint8_t v = (uint8_t) value;
            return dt1 (kSystem + offset, &v, 1);
        }

        // ------------------------------------------------------------ state
        juce::String toBase64() const
        {
            juce::MemoryOutputStream os;
            os.writeInt (1);
            os.write (&m[(size_t) kPatchTemp], (size_t) (kTimbreTemp - kPatchTemp + kTimbreSize * 8));
            os.write (&m[(size_t) kPatches], 128 * 8);
            os.write (&m[(size_t) kTimbres], 64 * 256);
            os.write (&m[(size_t) kSystem], kSysSize);
            for (int p = 0; p < kParts; ++p) os.writeByte ((char) program[p]);
            return juce::Base64::toBase64 (os.getData(), os.getDataSize());
        }
        bool fromBase64 (const juce::String& s)
        {
            juce::MemoryOutputStream mo;
            if (! juce::Base64::convertFromBase64 (mo, s)) return false;
            juce::MemoryInputStream is (mo.getData(), mo.getDataSize(), false);
            if (is.readInt() != 1) return false;
            const size_t need = (size_t) (kTimbreTemp - kPatchTemp + kTimbreSize * 8) + 128 * 8 + 64 * 256 + kSysSize + kParts;
            if ((size_t) is.getNumBytesRemaining() < need) return false;
            is.read (&m[(size_t) kPatchTemp], kTimbreTemp - kPatchTemp + kTimbreSize * 8);
            is.read (&m[(size_t) kPatches], 128 * 8);
            is.read (&m[(size_t) kTimbres], 64 * 256);
            is.read (&m[(size_t) kSystem], kSysSize);
            for (int p = 0; p < kParts; ++p) program[p] = (uint8_t) is.readByte() & 127;
            systemChanged = true;
            ++version;
            return true;
        }

    private:
        std::vector<uint8_t> m;

        void putTimbre (int a, const la32::Timbre& t)
        {
            std::memcpy (&m[(size_t) a], t.common, 14);
            for (int k = 0; k < 4; ++k) std::memcpy (&m[(size_t) (a + 14 + 58 * k)], t.partial[k], 58);
        }
        void loadTimbreTemp (int part)
        {
            const uint8_t* p = patchTemp (part);
            putTimbre (kTimbreTemp + kTimbreSize * part, timbreByNumber (p[0] & 3, p[1]));
        }
    };
}
