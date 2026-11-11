#pragma once

#include "La32.h"
#include <map>

// ===========================================================================
//  Shared MT-32 / CM-32L ROM library (COHESION - SYNTH and COHESION - LA MODULE).
//  Sets are stored as %APPDATA%/Pepperwood/Cohesion Synth/ROMs/<set>/control.rom + pcm.rom.
//  The user's ROM dumps are only ever read from / copied to the user's own disk.
// ===========================================================================
namespace romlib
{
    inline juce::File userRomDir()
    {
        const auto overrideDir = juce::SystemStats::getEnvironmentVariable ("COHESION_SYNTH_ROM_DIR", {});
        if (overrideDir.isNotEmpty()) return juce::File (overrideDir);
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                 .getChildFile ("Pepperwood").getChildFile ("Cohesion Synth").getChildFile ("ROMs");
    }

    // ROM sets are shared between plugin instances (decoded PCM is ~2-4 MB per set).
    inline std::shared_ptr<const la32::Rom> loadSetCached (const juce::File& dir)
    {
        static juce::CriticalSection lock;
        static std::map<juce::String, std::weak_ptr<const la32::Rom>> cache;
        const juce::ScopedLock sl (lock);
        if (auto hit = cache[dir.getFullPathName()].lock()) return hit;
        juce::MemoryBlock c, p; juce::String err;
        if (! dir.getChildFile ("control.rom").loadFileAsData (c) || ! dir.getChildFile ("pcm.rom").loadFileAsData (p)) return nullptr;
        std::shared_ptr<const la32::Rom> rom = la32::load (c, p, dir.getFileName(), err);
        if (rom != nullptr)
        {
            auto copy = std::make_shared<la32::Rom> (*rom);
            copy->label = dir.getFileName();                  // the folder name is the set's stable label
            rom = copy;
            cache[dir.getFullPathName()] = rom;
        }
        return rom;
    }

    inline juce::File setDir (const juce::String& label) { return userRomDir().getChildFile (label); }

    // all stored sets, sorted by folder name, duplicates (same sound) skipped
    inline std::vector<std::shared_ptr<const la32::Rom>> storedSets()
    {
        std::vector<std::shared_ptr<const la32::Rom>> out;
        const auto dir = userRomDir();
        if (! dir.isDirectory()) return out;
        auto sets = dir.findChildFiles (juce::File::findDirectories, false);
        sets.sort();
        for (auto& d : sets)
            if (auto rom = loadSetCached (d))
            {
                bool dup = false; for (auto& s : out) if (s->contentHash == rom->contentHash) dup = true;
                if (! dup) out.push_back (rom);
            }
        return out;
    }

    struct ImportResult { std::vector<std::shared_ptr<const la32::Rom>> added; int duplicates = 0, files = 0; juce::StringArray failed; };

    // Scans files, folders (recursively) and .zip files for control + PCM ROM pairs and stores
    // new sets. `known` = content hashes that already exist (counted as duplicates).
    inline ImportResult importRoms (const juce::Array<juce::File>& items, std::vector<uint64_t> known)
    {
        ImportResult r;
        struct Blob { juce::String name, container; juce::MemoryBlock data; };
        std::vector<Blob> controls, pcms;
        auto romSize = [] (juce::int64 n) { return n == 65536 || n == 131072 || n == 524288 || n == 1048576; };

        auto consider = [&] (const juce::String& name, const juce::String& container, juce::MemoryBlock data)
        {
            const auto n = data.getSize();
            if (n == 65536 || n == 131072)        controls.push_back ({ name, container, std::move (data) });
            else if (n == 524288 || n == 1048576) pcms.push_back ({ name, container, std::move (data) });
        };
        auto readZip = [&] (const juce::File& z)
        {
            juce::ZipFile zip (z);
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* e = zip.getEntry (i);
                if (e == nullptr || ! romSize (e->uncompressedSize)) continue;
                std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
                if (in == nullptr) continue;
                juce::MemoryBlock mb; in->readIntoMemoryBlock (mb);
                consider (e->filename, z.getFullPathName(), std::move (mb));
            }
        };
        auto readFile = [&] (const juce::File& f)
        {
            ++r.files;
            if (f.hasFileExtension ("zip")) { readZip (f); return; }
            if (! romSize (f.getSize())) return;
            juce::MemoryBlock mb; if (f.loadFileAsData (mb)) consider (f.getFileName(), f.getParentDirectory().getFullPathName(), std::move (mb));
        };
        for (const auto& item : items)
        {
            if (item.isDirectory())
            {
                auto files = item.findChildFiles (juce::File::findFiles, true, "*");
                files.sort();
                for (auto& f : files) readFile (f);
            }
            else if (item.existsAsFile()) readFile (item);
        }

        const auto dir = userRomDir();
        for (auto& c : controls)
        {
            // try PCM ROMs from the same folder / zip first
            std::vector<const Blob*> order;
            for (auto& p : pcms) if (p.container == c.container) order.push_back (&p);
            for (auto& p : pcms) if (p.container != c.container) order.push_back (&p);
            std::shared_ptr<const la32::Rom> rom;
            const Blob* pcmUsed = nullptr;
            for (auto* p : order)
            {
                juce::String err;
                if ((rom = la32::load (c.data, p->data, c.name, err)) != nullptr) { pcmUsed = p; break; }
            }
            if (rom == nullptr) { r.failed.add (c.name); continue; }
            if (std::find (known.begin(), known.end(), rom->contentHash) != known.end()) { ++r.duplicates; continue; }

            // store the pair as <model version>/control.rom + pcm.rom
            juce::String label = rom->model + (rom->version.isNotEmpty() ? " " + rom->version : juce::String());
            auto d = dir.getChildFile (juce::File::createLegalFileName (label));
            for (int k = 2; d.exists(); ++k) d = dir.getChildFile (juce::File::createLegalFileName (label + " (" + juce::String (k) + ")"));
            d.createDirectory();
            d.getChildFile ("control.rom").replaceWithData (c.data.getData(), c.data.getSize());
            d.getChildFile ("pcm.rom").replaceWithData (pcmUsed->data.getData(), pcmUsed->data.getSize());
            if (auto stored = loadSetCached (d)) { known.push_back (stored->contentHash); r.added.push_back (stored); }
        }
        return r;
    }
}
