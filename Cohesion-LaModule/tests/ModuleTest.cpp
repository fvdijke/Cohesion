// Offline test for COHESION - LA MODULE (MUNT engine).
//   ModuleTest <folder with MT-32 / CM-32L ROMs> [outdir for UI snapshots + WAVs]
// Uses a throw-away ROM library folder (never the user's real AppData).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <juce_audio_formats/juce_audio_formats.h>
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void check (bool ok, const char* what, const juce::String& info = {})
{
    std::printf ("  [%s] %-58s %s\n", ok ? " OK " : "FAIL", what, info.toRawUTF8());
    if (! ok) ++failures;
}

struct Render { float peak = 0, rms = 0; bool finite = true; std::vector<float> main; };

static Render run (LaModuleProcessor& p, juce::MidiBuffer events, double seconds, int numBusChannels, std::vector<float>* auxPeaks = nullptr)
{
    const double sr = 48000.0; const int block = 480;
    Render r;
    juce::AudioBuffer<float> buf (numBusChannels, block);
    const int total = (int) (seconds * sr);
    double sum = 0; int count = 0;
    for (int s = 0; s < total; s += block)
    {
        juce::MidiBuffer mb;
        for (const auto m : events)
            if (m.samplePosition >= s && m.samplePosition < s + block) mb.addEvent (m.getMessage(), m.samplePosition - s);
        buf.clear();
        p.processBlock (buf, mb);
        for (int i = 0; i < block; ++i)
        {
            const float l = buf.getSample (0, i), rr = buf.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (rr)) r.finite = false;
            r.peak = juce::jmax (r.peak, std::abs (l), std::abs (rr));
            sum += l * l + rr * rr; count += 2;
            r.main.push_back (0.5f * (l + rr));
        }
        if (auxPeaks != nullptr)
            for (int c = 2; c < numBusChannels; ++c)
                for (int i = 0; i < block; ++i) (*auxPeaks)[(size_t) c] = juce::jmax ((*auxPeaks)[(size_t) c], std::abs (buf.getSample (c, i)));
    }
    r.rms = (float) std::sqrt (sum / juce::jmax (1, count));
    return r;
}

static void setParam (LaModuleProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}
static float getParam (LaModuleProcessor& p, const juce::String& id) { return p.apvts.getRawParameterValue (id)->load(); }

static void writeWav (const juce::File& f, const std::vector<float>& x)
{
    f.deleteFile();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (f.createOutputStream());
    std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (os.get(), 48000.0, 1, 16, {}, 0));
    if (w == nullptr) return;
    os.release();
    juce::AudioBuffer<float> b (1, (int) x.size());
    for (int i = 0; i < (int) x.size(); ++i) b.setSample (0, i, juce::jlimit (-1.0f, 1.0f, x[(size_t) i]));
    w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
}

static juce::MidiBuffer song()
{
    juce::MidiBuffer m;
    const int beat = 24000;                                   // 120 bpm at 48 kHz
    for (int b = 0; b < 8; ++b)
    {
        const int t = b * beat;
        m.addEvent (juce::MidiMessage::noteOn (2, 60 + (b % 4) * 2, (juce::uint8) 100), t);
        m.addEvent (juce::MidiMessage::noteOff (2, 60 + (b % 4) * 2), t + beat - 2000);
        m.addEvent (juce::MidiMessage::noteOn (4, 36 + (b % 2) * 7, (juce::uint8) 110), t);
        m.addEvent (juce::MidiMessage::noteOff (4, 36 + (b % 2) * 7), t + beat / 2);
        m.addEvent (juce::MidiMessage::noteOn (10, b % 2 ? 38 : 36, (juce::uint8) 110), t);
        m.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 90), t + beat / 2);
    }
    m.addEvent (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 90), 0);
    m.addEvent (juce::MidiMessage::noteOn (3, 67, (juce::uint8) 90), 0);
    m.addEvent (juce::MidiMessage::noteOff (3, 64), 8 * beat - 4000);
    m.addEvent (juce::MidiMessage::noteOff (3, 67), 8 * beat - 4000);
    return m;
}

static void snapshot (LaModuleProcessor& p, const juce::File& outDir, const char* name, int part = 0, int w = 0, int h = 0, bool help = false)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    if (auto* e = dynamic_cast<LaModuleEditor*> (ed.get())) { e->selectPart (part); if (help) e->showHelp(); }
    if (w > 0) ed->setSize (w, h);
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    auto f = outDir.getChildFile (juce::String (name) + ".png"); f.deleteFile();
    juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

// ---------------------------------------------------------------------------
//  ModuleTest --export <source folder> <target folder>
//  Finds every control + PCM ROM pair under <source> (files and .zip, recursively) that Munt
//  plays and copies each working set to <target>/<model version>/<MODEL>_CONTROL.ROM + _PCM.ROM.
//  Every distinct control ROM gets its own set. Source files are only read.
// ---------------------------------------------------------------------------
static int exportWorkingSets (const juce::File& src, const juce::File& dst)
{
    struct Blob { juce::String name, container; juce::MemoryBlock data; uint64_t hash = 0; };
    std::vector<Blob> controls, pcms;
    auto hashOf = [] (const juce::MemoryBlock& m) { return la32::fnv1a (static_cast<const uint8_t*> (m.getData()), m.getSize()); };
    auto add = [&] (const juce::String& name, const juce::String& container, juce::MemoryBlock mb)
    {
        const auto n = mb.getSize();
        auto& list = (n == 65536 || n == 131072) ? controls : pcms;
        if (! (n == 65536 || n == 131072 || n == 524288 || n == 1048576)) return;
        Blob b { name, container, std::move (mb), 0 }; b.hash = hashOf (b.data);
        for (auto& e : list) if (e.hash == b.hash) return;          // identical file seen before
        list.push_back (std::move (b));
    };
    int files = 0;
    for (auto& f : src.findChildFiles (juce::File::findFiles, true, "*"))
    {
        if (f.isAChildOf (dst)) continue;
        ++files;
        if (f.hasFileExtension ("zip"))
        {
            juce::ZipFile zip (f);
            for (int i = 0; i < zip.getNumEntries(); ++i)
            {
                const auto* e = zip.getEntry (i);
                const auto sz = e != nullptr ? e->uncompressedSize : 0;
                if (! (sz == 65536 || sz == 131072 || sz == 524288 || sz == 1048576)) continue;
                std::unique_ptr<juce::InputStream> in (zip.createStreamForEntry (i));
                juce::MemoryBlock mb; if (in != nullptr) in->readIntoMemoryBlock (mb);
                add (f.getFileName() + ":" + e->filename, f.getFullPathName(), std::move (mb));
            }
        }
        else
        {
            const auto sz = f.getSize();
            if (! (sz == 65536 || sz == 131072 || sz == 524288 || sz == 1048576)) continue;
            juce::MemoryBlock mb; if (f.loadFileAsData (mb)) add (f.getRelativePathFrom (src), f.getParentDirectory().getFullPathName(), std::move (mb));
        }
    }
    std::printf ("scanned %d files: %d distinct control ROMs, %d distinct PCM ROMs\n", files, (int) controls.size(), (int) pcms.size());

    dst.createDirectory();
    int written = 0;
    for (auto& c : controls)
    {
        std::vector<const Blob*> order;
        for (auto& p : pcms) if (p.container == c.container) order.push_back (&p);
        for (auto& p : pcms) if (p.container != c.container) order.push_back (&p);
        const Blob* used = nullptr; std::shared_ptr<const la32::Rom> rom;
        for (auto* p : order)
        {
            juce::String err;
            if ((rom = la32::load (c.data, p->data, c.name, err)) == nullptr) continue;
            if (! MuntEngine::canOpenData (c.data, p->data)) { rom = nullptr; continue; }
            used = p; break;
        }
        if (used == nullptr) { std::printf ("  --  %-58s no working pair\n", c.name.toRawUTF8()); continue; }
        const juce::String label = rom->model + (rom->version.isNotEmpty() ? " " + rom->version : juce::String());
        auto d = dst.getChildFile (juce::File::createLegalFileName (label));
        for (int k = 2; d.exists(); ++k) d = dst.getChildFile (juce::File::createLegalFileName (label + " (" + juce::String (k) + ")"));
        d.createDirectory();
        const juce::String model = rom->model == "CM-32L" ? "CM32L" : "MT32";
        d.getChildFile (model + "_CONTROL.ROM").replaceWithData (c.data.getData(), c.data.getSize());
        d.getChildFile (model + "_PCM.ROM").replaceWithData (used->data.getData(), used->data.getSize());
        d.getChildFile ("source.txt").replaceWithText ("control: " + c.name + "\r\npcm:     " + used->name + "\r\n");
        std::printf ("  OK  %-58s + %-34s -> %s\n", c.name.toRawUTF8(), used->name.toRawUTF8(), d.getFileName().toRawUTF8());
        ++written;
    }
    std::printf ("\n%d working ROM set(s) written to %s\n", written, dst.getFullPathName().toRawUTF8());
    return 0;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc >= 4 && juce::String (argv[1]) == "--export")
        return exportWorkingSets (juce::File { juce::String (argv[2]) }, juce::File { juce::String (argv[3]) });
    if (argc < 2) { std::printf ("usage: ModuleTest <rom folder> [outdir]\n"); return 1; }
    const juce::File romFolder { juce::String (argv[1]) };
    const juce::File outDir { juce::String (argc > 2 ? argv[2] : ".") };
    outDir.createDirectory();

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion-lamodule-test");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_ROM_DIR", tmp.getFullPathName().toRawUTF8());

    std::printf ("== setup\n");
    LaModuleProcessor p;
    check (p.currentRom() == nullptr && p.muntStatus().isNotEmpty(), "fresh library has no ROM set", p.muntStatus());
    snapshot (p, outDir, "0-no-rom");
    const auto imp = p.importRoms ({ romFolder });
    bool allPlay = ! p.romLabels().isEmpty();
    for (auto& l : p.romLabels()) allPlay &= p.romSupported (l);
    check (imp.added.size() >= 2 && allPlay && p.currentRom() != nullptr, "folder import keeps only sets Munt plays",
           "added " + imp.added.joinIntoString (", ") + " | not playable " + juce::String (imp.r.failed.size() + imp.rejected.size()));
    const auto labels = p.romLabels();
    std::printf ("   sets: %s\n", labels.joinIntoString (", ").toRawUTF8());
    check (MuntEngine::available(), "mt32emu.dll loaded", MuntEngine::unavailableReason());

    auto layout = p.getBusesLayout();
    for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
    check (p.setBusesLayout (layout), "all 10 stereo outputs can be enabled");
    const int nch = p.getTotalNumOutputChannels();
    p.prepareToPlay (48000.0, 480);

    for (const juce::String& label : labels)
    {
        p.setRom (label);
        std::printf ("== %s\n", label.toRawUTF8());
        if (! p.muntReady()) { std::printf ("   (%s)\n", p.muntStatus().toRawUTF8()); continue; }
        p.requestReset();
        auto r = run (p, song(), 5.0, nch);
        check (r.finite, "output finite");
        check (r.peak > 0.02f && r.peak < 2.0f, "song audible, no overload", "peak " + juce::String (r.peak, 3) + "  rms " + juce::String (r.rms, 4));
        writeWav (outDir.getChildFile (juce::File::createLegalFileName (label) + ".wav"), r.main);

        juce::String quiet;
        for (int part = 0; part < mt::kParts; ++part)
        {
            for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "solo"), q == part ? 1.0f : 0.0f);
            juce::MidiBuffer m;
            const int ch = part < 8 ? part + 2 : 10, key = part < 8 ? 60 : 38;
            m.addEvent (juce::MidiMessage::noteOn (ch, key, (juce::uint8) 110), 100);
            m.addEvent (juce::MidiMessage::noteOff (ch, key), 30000);
            if (run (p, m, 1.0, nch).peak < 0.003f) quiet << (part < 8 ? juce::String (part + 1) : juce::String ("R")) << " ";
        }
        for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "solo"), 0.0f);
        check (quiet.isEmpty(), "every part (1-8 + rhythm) sounds on its own channel", quiet.isEmpty() ? juce::String() : "silent: " + quiet);

        p.requestPanic(); run (p, {}, 3.0, nch);
        setParam (p, lpid::part (0, "mute"), 1.0f);
        {
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 110), 0);
            check (run (p, m, 0.6, nch).peak < 0.002f, "mute silences part 1");
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (2), 0); run (p, off, 0.5, nch);
        }
        setParam (p, lpid::part (0, "mute"), 0.0f);

        // multi-out: part 3 incl. its own reverb / echo on OUT 4
        setParam (p, lpid::part (2, "out"), 1.0f); setParam (p, lpid::part (2, "ec_on"), 1.0f);
        p.requestPanic(); run (p, {}, 3.0, nch);
        {
            std::vector<float> aux ((size_t) nch, 0.0f);
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (4, 40, (juce::uint8) 110), 0);
            m.addEvent (juce::MidiMessage::noteOff (4, 40), 20000);
            const auto rr = run (p, m, 1.5, nch, &aux);
            check (aux[6] > 0.01f && rr.peak < 0.001f, "part 3 + its reverb / echo on OUT 4 only", "aux " + juce::String (aux[6], 3) + " main " + juce::String (rr.peak, 4));
        }
        setParam (p, lpid::part (2, "out"), 0.0f); setParam (p, lpid::part (2, "ec_on"), 0.0f);

        {
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::programChange (2, 48), 0);
            run (p, m, 0.05, nch);
            check (p.partProgram (0) == 48 && p.partName (0) == p.currentRom()->melodic[48].getName().trim(), "program change 49 on part 1", p.partName (0));
        }

        // partial meters: per part (of its own 32) and the total of all parts
        {
            p.requestPanic(); run (p, {}, 2.0, nch);
            juce::MidiBuffer m;
            for (int k : { 60, 64, 67 }) m.addEvent (juce::MidiMessage::noteOn (2, k, (juce::uint8) 100), 0);
            m.addEvent (juce::MidiMessage::noteOn (3, 72, (juce::uint8) 100), 0);
            run (p, m, 0.4, nch);
            const int a1 = p.partPartials (0), a2 = p.partPartials (1), a3 = p.partPartials (2);
            int sum = 0; for (int q = 0; q < mt::kParts; ++q) sum += p.partPartials (q);
            check (a1 >= 3 && a2 >= 1 && a3 == 0 && p.activePartials() == sum, "partials per part and total",
                   "part1 " + juce::String (a1) + "  part2 " + juce::String (a2) + "  total " + juce::String (p.activePartials()));
            juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (2), 0); off.addEvent (juce::MidiMessage::allNotesOff (3), 0);
            run (p, off, 0.1, nch);
        }

        // per-part reverb / echo: tails of part 1 with its switches, part 2 unaffected
        auto tailDb = [&] (int part)
        {
            p.requestPanic(); run (p, {}, 2.0, nch);
            const int ch = part + 2;
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (ch, 72, (juce::uint8) 110), 0); m.addEvent (juce::MidiMessage::noteOff (ch, 72), 4800);
            const auto rr = run (p, m, 1.5, nch);
            double e = 0; for (size_t i = 24000; i < rr.main.size(); ++i) e += rr.main[i] * rr.main[i];
            return 10.0 * std::log10 (juce::jmax (1.0e-12, e / 48000.0));
        };
        setParam (p, lpid::part (0, "rv_level"), 7.0f); setParam (p, lpid::part (1, "rv_level"), 7.0f);
        const double rvOnly = tailDb (0);
        setParam (p, lpid::part (0, "ec_on"), 1.0f);
        const double withEcho = tailDb (0);
        setParam (p, lpid::part (0, "rv_on"), 0.0f);
        const double echoOnly = tailDb (0);
        setParam (p, lpid::part (0, "ec_on"), 0.0f);
        const double none = tailDb (0);
        const double other = tailDb (1);
        check (withEcho >= rvOnly - 0.5 && echoOnly > none + 15.0 && rvOnly > none + 15.0, "part 1 reverb / echo switches work",
               "rev " + juce::String (rvOnly, 1) + " / +echo " + juce::String (withEcho, 1) + " / echo " + juce::String (echoOnly, 1) + " / none " + juce::String (none, 1) + " dB");
        check (other > none + 20.0, "part 2 keeps its own reverb", juce::String (other, 1) + " dB");
        setParam (p, lpid::part (0, "rv_on"), 1.0f);

        setParam (p, lpid::part (0, "rv_mode"), 1.0f); setParam (p, lpid::part (1, "rv_mode"), 2.0f);
        run (p, {}, 0.05, nch);
        check ((int) getParam (p, lpid::part (0, "rv_mode")) == 1 && (int) getParam (p, lpid::part (1, "rv_mode")) == 2, "parts keep different reverb modes");
        {
            juce::MidiBuffer m;
            const auto s = mt::Memory::systemWrite (1, 3);    // tap delay
            m.addEvent (juce::MidiMessage::createSysExMessage (s.data() + 1, (int) s.size() - 2), 0);
            run (p, m, 0.05, nch);
            juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
            bool all = true; for (int q = 0; q < mt::kParts; ++q) all &= (int) getParam (p, lpid::part (q, "rv_mode")) == 3;
            check (all, "SysEx reverb mode sets TAP DELAY on every part");
        }
        {
            const char nm[10] = { 'C', 'O', 'H', 'E', 'S', 'I', 'O', 'N', ' ', ' ' };
            const auto s = mt::Memory::dt1 (mt::kTimbreTemp, reinterpret_cast<const uint8_t*> (nm), 10);
            juce::MidiBuffer m; m.addEvent (juce::MidiMessage::createSysExMessage (s.data() + 1, (int) s.size() - 2), 0);
            run (p, m, 0.05, nch);
            check (p.partName (0) == "COHESION", "SysEx timbre-temp write renames part 1", p.partName (0));
        }
    }

    // ROM management: support detection, remove unsupported sets, remove the current set
    {
        juce::String sup;
        int unsupported = 0;
        for (auto& l : p.romLabels()) { const bool ok = p.romSupported (l); sup << l << (ok ? " ok, " : " NOT, "); if (! ok) ++unsupported; }
        std::printf ("   support: %s\n", sup.toRawUTF8());
        const int before = p.romLabels().size();
        const int removed = p.removeUnsupportedRoms();
        check (removed == unsupported && p.romLabels().size() == before - removed, "remove sets Munt can't play", juce::String (removed) + " removed");
        const auto cur = p.currentRomLabel();
        p.removeRom (cur);
        bool allOk = true; for (auto& l : p.romLabels()) allOk &= p.romSupported (l);
        check (! p.romLabels().contains (cur) && p.currentRomLabel().isNotEmpty() && p.muntReady() && allOk,
               "remove the current set falls back to another", "removed " + cur + ", now " + p.currentRomLabel());
        p.importRoms ({ romFolder });                       // restore the library for the next checks
        check (p.romLabels().contains (cur), "a removed set can be added again", p.romLabels().joinIntoString (", "));
    }

    // single ROM files: control ROM alone fails without its PCM, control + PCM works,
    // a control ROM alone pairs with a PCM ROM already in the library
    {
        // find the MT-32 files by name (the user's folder layout may change)
        juce::File ctl, pcm, other;
        for (auto& f : romFolder.findChildFiles (juce::File::findFiles, false, "*"))
        {
            const auto n = f.getFileName().toUpperCase();
            if (n.startsWith ("MT32_CONTROL") && n.contains ("PATCHED")) other = f;
            else if (n.startsWith ("MT32_CONTROL") && f.getSize() == 65536) ctl = f;
            else if (n.startsWith ("MT32_PCM") && f.getSize() == 524288) pcm = f;
        }
        std::printf ("   files: %s + %s, other %s\n", ctl.getFileName().toRawUTF8(), pcm.getFileName().toRawUTF8(), other.getFileName().toRawUTF8());
        for (auto& l : p.romLabels()) if (l.startsWith ("MT-32")) p.removeRom (l);   // no MT-32 PCM ROM left in the library
        auto a = p.importRoms ({ ctl }, true);
        check (a.added.isEmpty() && a.r.failed.size() == 1, "control ROM alone: no matching PCM in the library", a.r.files_.joinIntoString (" | "));
        auto b = p.importRoms ({ ctl, pcm }, true);
        check (b.added.contains ("MT-32 1.07"), "control + PCM files added and checked", b.added.joinIntoString (", "));
        auto c = p.importRoms ({ other }, true);
        check (c.added.size() + c.rejected.size() + (size_t) c.r.duplicates >= 1 && c.r.failed.isEmpty(),
               "control ROM alone pairs with the library's PCM", "added " + c.added.joinIntoString (", ") + " | rejected " + c.rejected.joinIntoString (", ")
               + " | dup " + juce::String (c.r.duplicates));
    }

    // a folder of sorted sets (e.g. made with --export): every firmware version is its own set, named by Munt
    {
        const auto sorted = romFolder.getParentDirectory().getChildFile ("La Module ROMS");
        if (sorted.isDirectory())
        {
            for (auto& l : p.romLabels()) p.removeRom (l);
            const auto im = p.importRoms ({ sorted });
            std::printf ("   sets: %s\n", p.romLabels().joinIntoString (", ").toRawUTF8());
            bool named = true; for (auto& l : p.romLabels()) named &= l.containsAnyOf ("0123456789") || l.containsIgnoreCase ("Blue");
            check (im.added.size() == 11 && p.romLabels().size() == 11 && named, "La Module ROMS: 11 sets, each with its version",
                   juce::String (im.added.size()) + " added");
            p.importRoms ({ romFolder });
        }
    }

    // RESET = power cycle: ROM set reloaded, every part back to its power-on timbre, still playing
    {
        p.setRom (labels[labels.size() - 1]);
        const int prog0 = p.partProgram (0);
        p.requestProgram (0, 100); run (p, {}, 0.05, nch);
        p.hardReset();
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 0); m.addEvent (juce::MidiMessage::noteOff (2, 60), 20000);
        const auto rr = run (p, m, 0.6, nch);
        check (p.partProgram (0) == prog0 && p.muntReady() && rr.peak > 0.01f, "RESET reloads the ROM set and the power-on parts",
               p.currentRomLabel() + " / " + p.partName (0) + " / peak " + juce::String (rr.peak, 3));
    }

    {
        p.setRom (labels[labels.size() - 1]);
        setParam (p, lpid::part (4, "lvl"), 0.5f);
        setParam (p, lpid::part (4, "rv_mode"), 1.0f);
        setParam (p, lpid::part (4, "ec_on"), 1.0f);
        p.requestProgram (3, 77);
        run (p, {}, 0.05, nch);
        juce::MemoryBlock st; p.getStateInformation (st);
        LaModuleProcessor q;
        q.setStateInformation (st.getData(), (int) st.getSize());
        check (q.currentRomLabel() == p.currentRomLabel() && q.partProgram (3) == 77
               && std::abs (getParam (q, lpid::part (4, "lvl")) - 0.5f) < 1e-3f
               && (int) getParam (q, lpid::part (4, "rv_mode")) == 1 && getParam (q, lpid::part (4, "ec_on")) > 0.5f,
               "state round trip (ROM, programs, mixer, per-part FX)", q.currentRomLabel() + " / " + q.partName (3));
    }

    {
        p.requestProgram (0, 0);
        setParam (p, lpid::part (5, "mute"), 1.0f);
        setParam (p, lpid::part (2, "out"), 1.0f);
        setParam (p, lpid::part (1, "ec_on"), 1.0f);
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 0); m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
        run (p, m, 0.1, nch);
        snapshot (p, outDir, "1-module", 1);
        snapshot (p, outDir, "2-scaled-70", 4, 728, 462);
        snapshot (p, outDir, "3-help", 0, 1040, 660, true);
    }
    tmp.deleteRecursively();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
