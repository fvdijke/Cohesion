// Offline test for COHESION - LA FONT.
//   FontTest <folder with .sf2 files> [outdir for UI snapshots]
// Uses a throw-away SoundFont list (never the user's real AppData).
#include "../Source/PluginProcessor.h"
#include "../Source/PluginEditor.h"
#include <cstdio>
#include <cstdlib>

static int failures = 0;
static void check (bool ok, const char* what, const juce::String& info = {})
{
    std::printf ("  [%s] %-60s %s\n", ok ? " OK " : "FAIL", what, info.toRawUTF8());
    if (! ok) ++failures;
}

struct Render { float peak = 0, rms = 0; bool finite = true; std::vector<float> main; };
static constexpr double kSr = 48000.0;
static constexpr int kBlock = 480;

static Render run (LaFontProcessor& p, juce::MidiBuffer events, double seconds, int nch, std::vector<float>* aux = nullptr)
{
    Render r;
    juce::AudioBuffer<float> buf (nch, kBlock);
    const int total = (int) (seconds * kSr);
    double sum = 0; int count = 0;
    for (int s = 0; s < total; s += kBlock)
    {
        juce::MidiBuffer mb;
        for (const auto m : events) if (m.samplePosition >= s && m.samplePosition < s + kBlock) mb.addEvent (m.getMessage(), m.samplePosition - s);
        buf.clear();
        p.processBlock (buf, mb);
        for (int i = 0; i < kBlock; ++i)
        {
            const float l = buf.getSample (0, i), rr = buf.getSample (1, i);
            if (! std::isfinite (l) || ! std::isfinite (rr)) r.finite = false;
            r.peak = juce::jmax (r.peak, std::abs (l), std::abs (rr));
            sum += l * l + rr * rr; count += 2;
            r.main.push_back (0.5f * (l + rr));
        }
        if (aux != nullptr)
            for (int c = 2; c < nch; ++c) for (int i = 0; i < kBlock; ++i) (*aux)[(size_t) c] = juce::jmax ((*aux)[(size_t) c], std::abs (buf.getSample (c, i)));
    }
    r.rms = (float) std::sqrt (sum / juce::jmax (1, count));
    return r;
}

static void setParam (LaFontProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

static bool waitReady (LaFontProcessor& p, int nch, double timeoutS)
{
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    while (juce::Time::getMillisecondCounterHiRes() - t0 < timeoutS * 1000.0)
    {
        bool all = true;
        for (int k = 0; k < LaFontProcessor::kParts; ++k)
            all &= p.partFont (k).isEmpty() || p.partState (k) == FontEngine::ready || p.partState (k) == FontEngine::failedToLoad;
        run (p, {}, 0.02, nch);
        if (all) return true;
        juce::Thread::sleep (50);
    }
    return false;
}

static void snapshot (LaFontProcessor& p, const juce::File& outDir, const char* name, int part = 0, bool help = false)
{
    std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
    ed->setSize (1440, 720);
    if (auto* e = dynamic_cast<LaFontEditor*> (ed.get())) { e->selectPart (part); if (help) e->showHelp(); }
    juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
    auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
    auto f = outDir.getChildFile (juce::String (name) + ".png"); f.deleteFile();
    juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
    std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: FontTest <sf2 folder> [outdir]\n"); return 1; }
    const juce::File sfFolder { juce::String (argv[1]) };
    const juce::File outDir { juce::String (argc > 2 ? argv[2] : ".") };
    outDir.createDirectory();
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion-lafont-test");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_LAFONT_DIR", tmp.getFullPathName().toRawUTF8());

    std::printf ("== setup\n");
    check (LaFontProcessor::engineAvailable(), "libfluidsynth-3.dll loaded", LaFontProcessor::engineStatus());
    LaFontProcessor p;
    check (p.partFont (0).isEmpty() && p.partState (0) == FontEngine::empty, "fresh instance without SoundFonts");
    snapshot (p, outDir, "0-no-font");

    // SoundFont list
    const auto added = fontlib::add ({ sfFolder });
    check (added.added.size() >= 2, "SoundFonts added to the list", juce::String (added.added.size()) + " added, " + juce::String (added.rejected.size()) + " rejected");
    juce::String gm, keys;
    for (auto& f : fontlib::paths())
    {
        if (f.containsIgnoreCase ("GeneralUser")) gm = f;
        if (f.containsIgnoreCase ("sfzBanks")) keys = f;
    }
    check (gm.isNotEmpty(), "GeneralUser GS found", gm);
    const auto gmInfo = sf2::get (gm);
    check (gmInfo->ok() && gmInfo->presets.size() > 200 && gmInfo->find (128, 0) != nullptr, "SF2 header reader: presets and drum kits",
           juce::String ((int) gmInfo->presets.size()) + " presets");

    auto layout = p.getBusesLayout();
    for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
    check (p.setBusesLayout (layout), "all 17 stereo outputs can be enabled");
    const int nch = p.getTotalNumOutputChannels();
    p.prepareToPlay (kSr, kBlock);

    p.setFontForAllParts (gm);
    if (keys.isNotEmpty()) p.setPartFont (0, keys);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    const bool ready = waitReady (p, nch, 180.0);
    check (ready, "all parts loaded", juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 1) + " s");
    std::printf ("   part 1: %s | part 2: %s | part 10: %s\n", p.partPresetName (0).toRawUTF8(), p.partPresetName (1).toRawUTF8(), p.partPresetName (9).toRawUTF8());
    check (p.partBank (9) == 128, "part 10 plays a drum kit", p.partPresetName (9));

    // every part on its own channel
    juce::String quiet;
    for (int part = 0; part < LaFontProcessor::kParts; ++part)
    {
        for (int q = 0; q < LaFontProcessor::kParts; ++q) setParam (p, fpid::part (q, "solo"), q == part ? 1.0f : 0.0f);
        juce::MidiBuffer m;
        const int key = part == 9 ? 38 : 60;
        m.addEvent (juce::MidiMessage::noteOn (part + 1, key, (juce::uint8) 110), 100);
        m.addEvent (juce::MidiMessage::noteOff (part + 1, key), 24000);
        if (run (p, m, 0.8, nch).peak < 0.003f) quiet << (part + 1) << " ";
    }
    for (int q = 0; q < LaFontProcessor::kParts; ++q) setParam (p, fpid::part (q, "solo"), 0.0f);
    check (quiet.isEmpty(), "every part sounds on its own MIDI channel", quiet.isEmpty() ? juce::String() : "silent: " + quiet);

    // a small song, voices counted per part
    {
        juce::MidiBuffer m;
        for (int k : { 60, 64, 67 }) m.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (2, 48, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 110), 0);
        const auto r = run (p, m, 0.5, nch);
        check (r.finite && r.peak > 0.02f && r.peak < 2.0f, "chord + bass + kick audible, no overload", "peak " + juce::String (r.peak, 3) + " rms " + juce::String (r.rms, 4));
        check (p.voices (0) >= 3 && p.voices (1) >= 1 && p.totalVoices() >= 4, "voices per part and total",
               "part1 " + juce::String (p.voices (0)) + " part2 " + juce::String (p.voices (1)) + " total " + juce::String (p.totalVoices()));
        p.requestPanic(); run (p, {}, 1.0, nch);
    }

    // program change + bank select (GS variation) on part 2
    {
        juce::MidiBuffer m;
        m.addEvent (juce::MidiMessage::controllerEvent (2, 0, 8), 0);
        m.addEvent (juce::MidiMessage::programChange (2, 4), 1);
        run (p, m, 0.05, nch);
        check (p.partBank (1) == 8 && p.partProgram (1) == 4 && p.partPresetName (1).isNotEmpty(), "bank select 8 + program 5 on part 2",
               juce::String (p.partBank (1)) + ":" + juce::String (p.partProgram (1)) + " " + p.partPresetName (1));
        juce::MidiBuffer d; d.addEvent (juce::MidiMessage::programChange (10, 16), 0);
        run (p, d, 0.05, nch);
        check (p.partBank (9) == 128 && p.partProgram (9) == 16, "program change on channel 10 selects drum kit 17", p.partPresetName (9));
    }

    // multi-out: part 3 incl. its effects on OUT 4 (bus 3 = channels 6/7)
    setParam (p, fpid::part (2, "out"), 1.0f);
    p.requestPanic(); run (p, {}, 2.0, nch);
    {
        std::vector<float> aux ((size_t) nch, 0.0f);
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (3, 50, (juce::uint8) 110), 0); m.addEvent (juce::MidiMessage::noteOff (3, 50), 20000);
        const auto r = run (p, m, 1.0, nch, &aux);
        check (aux[6] > 0.005f && r.peak < 0.0005f, "part 3 + its reverb / chorus on OUT 4 only", "aux " + juce::String (aux[6], 3) + " main " + juce::String (r.peak, 5));
    }
    setParam (p, fpid::part (2, "out"), 0.0f);

    // reverb / chorus switches per part (send levels via CC 91 / 93)
    {
        auto tail = [&] (int part)
        {
            p.requestPanic(); run (p, {}, 2.0, nch);
            juce::MidiBuffer m;
            m.addEvent (juce::MidiMessage::controllerEvent (part + 1, 91, 127), 0);
            m.addEvent (juce::MidiMessage::controllerEvent (part + 1, 93, 127), 0);
            m.addEvent (juce::MidiMessage::noteOn (part + 1, 72, (juce::uint8) 110), 10);
            m.addEvent (juce::MidiMessage::noteOff (part + 1, 72), 4800);
            const auto r = run (p, m, 2.0, nch);
            double e = 0; for (size_t i = 24000; i < r.main.size(); ++i) e += r.main[i] * r.main[i];
            return 10.0 * std::log10 (juce::jmax (1.0e-12, e / 72000.0));
        };
        setParam (p, fpid::part (3, "rv_room"), 0.9f);
        const double both = tail (3);
        setParam (p, fpid::part (3, "rv_on"), 0.0f);
        const double noRev = tail (3);
        setParam (p, fpid::part (3, "cho_on"), 0.0f);
        const double none = tail (3);
        const double other = tail (4);
        check (both > noRev + 10.0 && noRev >= none - 1.5 && other > none + 5.0, "part 4 reverb / chorus switches, part 5 unaffected",
               juce::String (both, 1) + " / -rev " + juce::String (noRev, 1) + " / none " + juce::String (none, 1) + " | part 5 " + juce::String (other, 1) + " dB");
        setParam (p, fpid::part (3, "rv_on"), 1.0f); setParam (p, fpid::part (3, "cho_on"), 1.0f);
    }

    // GM reset (SysEx) -> every part back to program 1
    {
        const uint8_t gmOn[] = { 0x7E, 0x7F, 0x09, 0x01 };
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::createSysExMessage (gmOn, 4), 0);
        run (p, m, 0.1, nch);
        check (p.partProgram (1) == 0 && p.partBank (1) == 0 && p.partBank (9) == 128 && p.partProgram (9) == 0, "GM System On resets the programs");
    }

    // state round trip
    {
        p.selectPreset (4, 0, 40);
        setParam (p, fpid::part (4, "lvl"), 0.5f);
        setParam (p, fpid::part (4, "rv_room"), 0.7f);
        run (p, {}, 0.05, nch);
        juce::MemoryBlock st; p.getStateInformation (st);
        LaFontProcessor q;
        q.setStateInformation (st.getData(), (int) st.getSize());
        q.prepareToPlay (kSr, kBlock);
        waitReady (q, 2, 60.0);
        check (q.partFont (0) == p.partFont (0) && q.partFont (4) == gm && q.partProgram (4) == 40
               && std::abs (q.apvts.getRawParameterValue (fpid::part (4, "rv_room"))->load() - 0.7f) < 1e-3f, "state round trip (fonts, programs, FX)",
               q.partPresetName (4));
    }

    // UI snapshots
    {
        p.selectPreset (1, 0, 48);
        setParam (p, fpid::part (5, "mute"), 1.0f);
        setParam (p, fpid::part (2, "out"), 1.0f);
        setParam (p, fpid::part (1, "cho_on"), 0.0f);
        juce::MidiBuffer m;
        for (int k : { 60, 64, 67 }) m.addEvent (juce::MidiMessage::noteOn (1, k, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (2, 55, (juce::uint8) 100), 0);
        m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
        run (p, m, 0.3, nch);
        snapshot (p, outDir, "1-lafont", 1);
        snapshot (p, outDir, "2-help", 0, true);
    }
    tmp.deleteRecursively();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
