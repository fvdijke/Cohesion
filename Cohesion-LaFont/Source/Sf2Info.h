#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <memory>

// ===========================================================================
//  Reads the name and preset list of a SoundFont 2 file from its headers only
//  (INFO and pdta chunks; the sample data is never read). Results are cached.
// ===========================================================================
namespace sf2
{
    struct Preset { int bank = 0, program = 0; juce::String name; };

    struct Info
    {
        juce::String file, name, error;
        int version = 0;
        std::vector<Preset> presets;           // sorted by bank, program
        bool ok() const { return error.isEmpty(); }
        const Preset* find (int bank, int program) const
        {
            for (auto& p : presets) if (p.bank == bank && p.program == program) return &p;
            return nullptr;
        }
        juce::String presetName (int bank, int program) const
        {
            if (auto* p = find (bank, program)) return p->name;
            return {};
        }
        juce::String shortName() const        // e.g. "Essential Keys-sfzBanks-v9.6"
        {
            return juce::File (file).getFileNameWithoutExtension();
        }
    };

    inline std::shared_ptr<const Info> read (const juce::File& f)
    {
        auto info = std::make_shared<Info>();
        info->file = f.getFullPathName();
        juce::FileInputStream in (f);
        if (! in.openedOk()) { info->error = "cannot open the file"; return info; }
        char id[4];
        auto readId = [&] { return in.read (id, 4) == 4; };
        if (! readId() || juce::String (id, 4) != "RIFF") { info->error = "not a SoundFont (RIFF) file"; return info; }
        const auto riffSize = (juce::int64) (juce::uint32) in.readInt();
        if (! readId() || juce::String (id, 4) != "sfbk") { info->error = "not a SoundFont 2 file"; return info; }
        const juce::int64 end = juce::jmin (in.getTotalLength(), 8 + riffSize);
        juce::MemoryBlock phdr;
        while (in.getPosition() + 12 <= end)
        {
            if (! readId()) break;
            const auto size = (juce::int64) (juce::uint32) in.readInt();
            const auto chunkStart = in.getPosition();
            if (juce::String (id, 4) == "LIST")
            {
                char type[4]; in.read (type, 4);
                const juce::String t (type, 4);
                if (t == "INFO" || t == "pdta")
                {
                    while (in.getPosition() + 8 <= chunkStart + size)
                    {
                        char sid[4]; in.read (sid, 4);
                        const auto ssize = (juce::int64) (juce::uint32) in.readInt();
                        const juce::String s (sid, 4);
                        const auto subStart = in.getPosition();
                        if (s == "ifil") { info->version = in.readShort(); }
                        else if (s == "INAM") { juce::MemoryBlock mb; in.readIntoMemoryBlock (mb, (size_t) ssize); info->name = juce::String::fromUTF8 (static_cast<const char*> (mb.getData()), (int) strnlen (static_cast<const char*> (mb.getData()), mb.getSize())).trim(); }
                        else if (s == "phdr") { in.readIntoMemoryBlock (phdr, (size_t) ssize); }
                        in.setPosition (subStart + ssize + (ssize & 1));
                    }
                }
            }
            in.setPosition (chunkStart + size + (size & 1));
        }
        if (info->version != 2) { info->error = info->version == 3 ? "SoundFont 3 (compressed) is not supported, use SF2" : "unsupported SoundFont version"; return info; }
        const auto* p = static_cast<const uint8_t*> (phdr.getData());
        const int count = (int) (phdr.getSize() / 38) - 1;      // last record is the terminal EOP
        for (int i = 0; i < count; ++i)
        {
            const uint8_t* r = p + 38 * i;
            Preset pr;
            pr.name = juce::String (reinterpret_cast<const char*> (r), (size_t) strnlen (reinterpret_cast<const char*> (r), 20)).trim();
            pr.program = r[20] | (r[21] << 8);
            pr.bank = r[22] | (r[23] << 8);
            info->presets.push_back (pr);
        }
        std::sort (info->presets.begin(), info->presets.end(), [] (const Preset& a, const Preset& b)
                   { return a.bank != b.bank ? a.bank < b.bank : a.program < b.program; });
        if (info->presets.empty()) info->error = "no presets in the SoundFont";
        return info;
    }

    // cached by path + size + modification time
    inline std::shared_ptr<const Info> get (const juce::String& path)
    {
        static juce::CriticalSection lock;
        static std::map<juce::String, std::shared_ptr<const Info>> cache;
        const juce::File f (path);
        const auto key = path + "|" + juce::String (f.getSize()) + "|" + juce::String (f.getLastModificationTime().toMilliseconds());
        const juce::ScopedLock sl (lock);
        auto& e = cache[key];
        if (e == nullptr) e = read (f);
        return e;
    }
}
