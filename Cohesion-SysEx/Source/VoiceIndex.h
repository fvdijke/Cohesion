#pragma once

#include <juce_core/juce_core.h>
#include <unordered_map>
#include <string>
#include <atomic>
#include "Dx7.h"

// ===========================================================================
//  Index of every DX7 voice in a folder tree of .syx files.
//  Identical voices (same 128-byte VMEM image, name included) are merged into
//  one entry that remembers every place it was found.
// ===========================================================================
struct VoiceIndex
{
    struct Source { int file = 0; int slot = 0; };      // slot 0..31 in the file
    struct Entry
    {
        dx7::Patch patch;
        juce::String name, nameLower;
        std::vector<Source> sources;                     // first = canonical
    };

    juce::File root;
    juce::StringArray files;                             // paths relative to root ('/' separated)
    juce::StringArray folders;                           // top-level folder names (sorted)
    std::vector<Entry> entries;
    int filesRead = 0, filesFailed = 0, voicesTotal = 0, blanksSkipped = 0, badChecksums = 0;

    juce::String fileOf (const Source& s) const { return files[s.file]; }
    juce::String folderOf (const Source& s) const
    {
        const auto f = files[s.file];
        return f.containsChar ('/') ? f.upToFirstOccurrenceOf ("/", false, false) : juce::String ("(root)");
    }
    juce::String bankOf (const Source& s) const
    {
        return files[s.file].fromLastOccurrenceOf ("/", false, false).upToLastOccurrenceOf (".", false, true);
    }

    static bool isInitVoice (const dx7::Patch& p) { return p.getName().equalsIgnoreCase ("INIT VOICE"); }

    // Scans root (recursively). progress / cancel are optional and polled from another thread.
    void scan (const juce::File& dir, std::atomic<float>* progress = nullptr, const std::atomic<bool>* cancel = nullptr)
    {
        *this = VoiceIndex();
        root = dir;
        auto list = dir.findChildFiles (juce::File::findFiles, true, "*.syx;*.SYX");
        std::sort (list.begin(), list.end(), [] (const juce::File& a, const juce::File& b)
                   { return a.getFullPathName().compareNatural (b.getFullPathName()) < 0; });

        std::unordered_map<std::string, int> seen;
        juce::StringArray folderSet;
        for (int fi = 0; fi < list.size(); ++fi)
        {
            if (cancel != nullptr && cancel->load()) return;
            if (progress != nullptr) progress->store ((float) fi / (float) juce::jmax (1, list.size()));
            const auto& f = list.getReference (fi);
            juce::MemoryBlock mb;
            if (f.getSize() > 4 * 1024 * 1024 || ! f.loadFileAsData (mb)) { ++filesFailed; continue; }
            const auto res = dx7::parseSysEx (mb.getData(), mb.getSize());
            if (res.voices.empty()) { ++filesFailed; continue; }
            ++filesRead;
            badChecksums += res.badChecksums;

            const int fileIdx = files.size();
            files.add (f.getRelativePathFrom (dir).replaceCharacter ('\\', '/'));
            folderSet.addIfNotAlreadyThere (folderOf ({ fileIdx, 0 }));

            for (int v = 0; v < (int) res.voices.size(); ++v)
            {
                const auto& p = res.voices[(size_t) v];
                ++voicesTotal;
                if (p.isBlank() || isInitVoice (p)) { ++blanksSkipped; continue; }
                const auto img = dx7::toVmem (p);
                std::string key ((const char*) img.data(), img.size());
                auto it = seen.find (key);
                if (it != seen.end()) { entries[(size_t) it->second].sources.push_back ({ fileIdx, v }); continue; }
                Entry e;
                e.patch = p; e.name = p.getName(); e.nameLower = e.name.toLowerCase();
                e.sources.push_back ({ fileIdx, v });
                seen.emplace (std::move (key), (int) entries.size());
                entries.push_back (std::move (e));
            }
        }
        folderSet.sortNatural();
        folders = folderSet;
        if (progress != nullptr) progress->store (1.0f);
    }
};
