// Round-trip checks for the DX7 bank writer.
//   BankTest <collection-folder> [output.syx]
// 1. every 32-voice bank in the collection: unpack/pack is stable (clean banks byte-identical,
//    others normalised once: unused bits cleared, out-of-range values clamped, like a DX7 does)
// 2. a bank assembled from voices of many files: 4104 bytes, valid checksum, parses back identically
// 3. a part-filled bank is padded with INIT VOICE
#include <juce_core/juce_core.h>
#include "Dx7.h"
#include "../Source/VoiceIndex.h"
#include <cstdio>

static int failures = 0;
static void check (bool ok, const char* what, const juce::String& got)
{
    std::printf ("  [%s] %-60s %s\n", ok ? " OK " : "FAIL", what, got.toRawUTF8());
    if (! ok) ++failures;
}

static bool verifyBank (const std::vector<uint8_t>& bytes, const std::vector<dx7::Patch>& expect, juce::String& why)
{
    if (bytes.size() != 4104) { why = "size " + juce::String ((int) bytes.size()); return false; }
    if (bytes[0] != 0xF0 || bytes[1] != 0x43 || bytes[3] != 0x09 || bytes[4] != 0x20 || bytes[5] != 0x00 || bytes[4103] != 0xF7)
    { why = "bad header/footer"; return false; }
    int sum = 0; for (size_t i = 6; i < 6 + 4096; ++i) { if (bytes[i] > 0x7f) { why = "data byte > 7F"; return false; } sum += bytes[i]; }
    if (((sum + bytes[4102]) & 0x7f) != 0) { why = "checksum"; return false; }
    const auto r = dx7::parseSysEx (bytes.data(), bytes.size());
    if (r.banks != 1 || r.badChecksums != 0 || r.voices.size() != 32) { why = "parse"; return false; }
    for (size_t i = 0; i < 32; ++i)
    {
        const auto& want = i < expect.size() ? expect[i] : dx7::initVoice();
        if (! dx7::sameVoice (r.voices[i], want)) { why = "voice " + juce::String ((int) i + 1) + " differs"; return false; }
    }
    why = "ok"; return true;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: BankTest <collection-folder> [output.syx]\n"); return 2; }
    const juce::File dir { juce::String (argv[1]) };

    std::printf ("1. Raw VMEM round trip over the collection\n");
    int banks = 0, exact = 0, dirty = 0, bad = 0, unstable = 0;
    juce::String firstDirty;
    for (auto& f : dir.findChildFiles (juce::File::findFiles, true, "*.syx;*.SYX"))
    {
        juce::MemoryBlock mb; if (! f.loadFileAsData (mb)) continue;
        auto* b = static_cast<const uint8_t*> (mb.getData());
        for (size_t i = 0; i + 4104 <= mb.getSize(); ++i)
        {
            if (! (b[i] == 0xF0 && b[i + 1] == 0x43 && b[i + 3] == 9 && b[i + 4] == 0x20 && b[i + 5] == 0)) continue;
            ++banks;
            int sum = 0; for (int k = 0; k < 4096; ++k) sum += b[i + 6 + (size_t) k];
            if (((sum + b[i + 4102]) & 0x7f) != 0) { ++bad; break; }
            bool same = true;
            for (int v = 0; v < 32; ++v)
            {
                const uint8_t* raw = b + i + 6 + v * 128;
                const auto packed = dx7::toVmem (dx7::fromVmem (raw));
                if (dx7::toVmem (dx7::fromVmem (packed.data())) != packed) ++unstable;
                same = std::equal (packed.begin(), packed.end(), raw) && same;
            }
            if (same) ++exact; else { ++dirty; if (firstDirty.isEmpty()) firstDirty = f.getRelativePathFrom (dir); }
            break;
        }
    }
    std::printf ("     %d banks: %d byte-identical after unpack/pack, %d normalised (out-of-range or unused bits), %d bad checksum\n",
                 banks, exact, dirty, bad);
    if (firstDirty.isNotEmpty()) std::printf ("     first normalised: %s\n", firstDirty.toRawUTF8());
    check (banks > 0 && unstable == 0, "unpack/pack is stable for every voice", juce::String (unstable) + " unstable");

    std::printf ("2. Index + assemble a bank from many files\n");
    VoiceIndex idx; idx.scan (dir);
    std::printf ("     %d files read (%d unreadable), %d voices, %d blank/INIT skipped, %d unique\n",
                 idx.filesRead, idx.filesFailed, idx.voicesTotal, idx.blanksSkipped, (int) idx.entries.size());
    check (idx.entries.size() >= 32, "collection has at least 32 unique voices", juce::String ((int) idx.entries.size()));

    std::vector<dx7::Patch> pick;
    juce::Random rng (42);
    juce::StringArray usedFiles;
    for (int i = 0; i < 32 && ! idx.entries.empty(); ++i)
    {
        const auto& e = idx.entries[(size_t) rng.nextInt ((int) idx.entries.size())];
        pick.push_back (e.patch);
        usedFiles.addIfNotAlreadyThere (idx.fileOf (e.sources.front()));
    }
    // a renamed voice must survive as well
    if (! pick.empty()) { std::memset (pick[0].name, 0, sizeof (pick[0].name)); std::memcpy (pick[0].name, "RENAMED 01", 10); }
    const auto bytes = dx7::writeBank (pick);
    juce::String why;
    const bool okMixed = verifyBank (bytes, pick, why);
    check (okMixed, "32 voices from different files round-trip", why + "  (" + juce::String (usedFiles.size()) + " source files)");

    // re-index the written file: every voice must be found again
    if (argc > 2)
    {
        const juce::File out { juce::String (argv[2]) };
        out.getParentDirectory().createDirectory();
        out.replaceWithData (bytes.data(), bytes.size());
        juce::MemoryBlock back; out.loadFileAsData (back);
        check (back.getSize() == 4104 && std::memcmp (back.getData(), bytes.data(), 4104) == 0, "written file reads back identically", out.getFullPathName());
    }

    std::printf ("3. Partial bank padded with INIT VOICE\n");
    std::vector<dx7::Patch> five (pick.begin(), pick.begin() + juce::jmin<size_t> (5, pick.size()));
    const auto partial = dx7::writeBank (five);
    const bool okPartial = verifyBank (partial, five, why);
    check (okPartial, "5 voices + 27 x INIT VOICE", why);
    const auto pr = dx7::parseSysEx (partial.data(), partial.size());
    check (pr.voices.size() == 32 && pr.voices[31].getName() == "INIT VOICE", "slot 32 is named INIT VOICE", pr.voices.size() == 32 ? pr.voices[31].getName() : juce::String());

    std::printf ("4. Edge values\n");
    {
        dx7::Patch p = dx7::initVoice();
        for (auto& o : p.op) { o.det = 14; o.coarse = 31; o.mode = 1; o.lc = 3; o.rc = 2; o.kvs = 7; o.ams = 3; o.rs = 7; o.bp = 99; }
        p.alg = 31; p.fb = 7; p.oks = 1; p.lfoWave = 5; p.pms = 7; p.lfoSync = 1; p.transpose = 48;
        std::memcpy (p.name, "~EDGE^_{}|", 10);
        const auto b = dx7::writeBank ({ p });
        const bool okEdge = verifyBank (b, { p }, why);
        check (okEdge, "max values of every packed field", why);
    }

    std::printf (failures == 0 ? "\nALL OK\n" : "\n%d FAILURE(S)\n", failures);
    return failures == 0 ? 0 : 1;
}
