// Offline test for COHESION - LA MODULE.
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

struct Render { float peak = 0, rms = 0, side = 0; bool finite = true; std::vector<float> main; };

// render `seconds` with MIDI events placed by sample position; returns main-bus stats
static Render run (LaModuleProcessor& p, juce::MidiBuffer events, double seconds, int numBusChannels, std::vector<float>* auxPeaks = nullptr)
{
    const double sr = 48000.0; const int block = 480;
    Render r;
    juce::AudioBuffer<float> buf (numBusChannels, block);
    const int total = (int) (seconds * sr);
    double sum = 0, sumM = 0, sumS = 0; int count = 0;
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
            sum += l * l + rr * rr; count += 2; sumM += (l + rr) * (l + rr); sumS += (l - rr) * (l - rr);
            r.main.push_back (0.5f * (l + rr));
        }
        if (auxPeaks != nullptr)
            for (int c = 2; c < numBusChannels; ++c)
                for (int i = 0; i < block; ++i) (*auxPeaks)[(size_t) c] = juce::jmax ((*auxPeaks)[(size_t) c], std::abs (buf.getSample (c, i)));
    }
    r.rms = (float) std::sqrt (sum / juce::jmax (1, count));
    r.side = (float) std::sqrt (sumS / juce::jmax (1.0e-12, sumM));
    return r;
}

static void setParam (LaModuleProcessor& p, const juce::String& id, float plain)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (plain));
}

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
        m.addEvent (juce::MidiMessage::noteOn (2, 60 + (b % 4) * 2, (juce::uint8) 100), t);      // part 1 piano
        m.addEvent (juce::MidiMessage::noteOff (2, 60 + (b % 4) * 2), t + beat - 2000);
        m.addEvent (juce::MidiMessage::noteOn (4, 36 + (b % 2) * 7, (juce::uint8) 110), t);      // part 3 bass
        m.addEvent (juce::MidiMessage::noteOff (4, 36 + (b % 2) * 7), t + beat / 2);
        m.addEvent (juce::MidiMessage::noteOn (10, b % 2 ? 38 : 36, (juce::uint8) 110), t);      // rhythm
        m.addEvent (juce::MidiMessage::noteOn (10, 42, (juce::uint8) 90), t + beat / 2);
    }
    m.addEvent (juce::MidiMessage::noteOn (3, 64, (juce::uint8) 90), 0);                         // part 2 strings, long
    m.addEvent (juce::MidiMessage::noteOn (3, 67, (juce::uint8) 90), 0);
    m.addEvent (juce::MidiMessage::noteOff (3, 64), 8 * beat - 4000);
    m.addEvent (juce::MidiMessage::noteOff (3, 67), 8 * beat - 4000);
    return m;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    if (argc < 2) { std::printf ("usage: ModuleTest <rom folder> [outdir]\n"); return 1; }
    const juce::File romFolder { juce::String (argv[1]) };
    const juce::File outDir { juce::String (argc > 2 ? argv[2] : ".") };
    outDir.createDirectory();

    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion-lamodule-test");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_ROM_DIR", tmp.getFullPathName().toRawUTF8());

    std::printf ("== setup\n");
    LaModuleProcessor p;
    check (p.currentRom() == nullptr, "fresh library has no ROM set");
    const auto imp = p.importRoms ({ romFolder });
    check (! imp.added.empty() && p.currentRom() != nullptr, "ROM import selects a set", juce::String ((int) imp.added.size()) + " set(s)");
    const auto labels = p.romLabels();
    std::printf ("   sets: %s\n", labels.joinIntoString (", ").toRawUTF8());
    std::printf ("   munt: %s\n", MuntEngine::available() ? "dll loaded" : MuntEngine::unavailableReason().toRawUTF8());

    // multi-out: enable every aux bus
    auto layout = p.getBusesLayout();
    for (int b = 1; b < layout.outputBuses.size(); ++b) layout.outputBuses.getReference (b) = juce::AudioChannelSet::stereo();
    check (p.setBusesLayout (layout), "all 10 stereo outputs can be enabled");
    const int nch = p.getTotalNumOutputChannels();
    p.prepareToPlay (48000.0, 480);

    if (argc > 3 && juce::String (argv[3]) == "levels")
    {
        p.setRom ("MT-32 1.07");
        auto db = [] (float x) { return 20.0f * std::log10 (juce::jmax (1.0e-6f, x)); };
        for (int q = 1; q < mt::kParts; ++q) setParam (p, lpid::part (q, "mute"), 1.0f);
        for (int prog : { 0, 4, 16, 24, 32, 40, 48, 52, 56, 61, 64, 72, 80, 88, 96, 104, 112, 120 })
        {
            juce::String line = juce::String (prog + 1).paddedLeft (' ', 3) + " ";
            for (int engine = 0; engine < 2; ++engine)
            {
                setParam (p, lpid::engine, (float) engine);
                juce::MidiBuffer pc; pc.addEvent (juce::MidiMessage::programChange (2, prog), 0); run (p, pc, 0.05, nch);
                if (engine == 0) line << p.partName (0).paddedRight (' ', 11);
                for (float rv : { 0.0f, 1.0f })
                {
                    setParam (p, lpid::part (0, "rev"), rv);
                    p.requestPanic(); run (p, {}, 1.5, nch);
                    juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 0); m.addEvent (juce::MidiMessage::noteOff (2, 60), 40000);
                    const auto r = run (p, m, 1.0, nch);
                    line << (engine == 0 ? " M " : " L ") << juce::String (db (r.rms), 1).paddedLeft (' ', 6) << (rv > 0 ? "(rev)" : "") << " side " << juce::String (r.side, 2);
                }
            }
            std::printf ("%s\n", line.toRawUTF8());
        }
        for (int q = 0; q < mt::kParts; ++q) { setParam (p, lpid::part (q, "mute"), 0.0f); setParam (p, lpid::part (q, "rev"), 0.0f); }
        for (int engine = 0; engine < 2; ++engine)
        {
            setParam (p, lpid::engine, (float) engine);
            { juce::MidiBuffer pc; pc.addEvent (juce::MidiMessage::programChange (2, 0), 0); run (p, pc, 0.05, nch); }
            for (float e : { 0.0f, 1.0f })
            {
                setParam (p, lpid::part (0, "ech"), e);
                p.requestPanic(); run (p, {}, 1.5, nch);
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 72, (juce::uint8) 110), 0); m.addEvent (juce::MidiMessage::noteOff (2, 72), 2400);
                const auto r = run (p, m, 2.0, nch);
                double tail = 0; for (size_t i = 14400; i < 48000; ++i) tail += r.main[i] * r.main[i];
                std::printf ("echo %s send %.0f: 0.3-1 s %.1f dB\n", engine == 0 ? "MUNT" : "LA", e, db ((float) std::sqrt (tail / 33600.0)));
            }
        }
        return 0;
    }

    for (const juce::String& label : labels)
    {
        p.setRom (label);
        for (int engine = 0; engine < 2; ++engine)
        {
            setParam (p, lpid::engine, (float) engine);
            const bool wantMunt = engine == 0;
            std::printf ("== %s / %s\n", label.toRawUTF8(), wantMunt ? "MUNT" : "COHESION LA");
            if (wantMunt && ! p.muntAvailable()) { std::printf ("   (Munt not available: %s)\n", p.muntStatus().toRawUTF8()); continue; }
            p.requestReset();
            auto r = run (p, song(), 5.0, nch);
            check (r.finite, "output finite");
            check (r.peak > 0.02f && r.peak < 2.0f, "song audible, no overload", "peak " + juce::String (r.peak, 3) + "  rms " + juce::String (r.rms, 4));
            check (p.activePartials() >= 0, "partial meter", juce::String (p.activePartials()));
            writeWav (outDir.getChildFile (juce::File::createLegalFileName (label + (wantMunt ? " munt" : " la")) + ".wav"), r.main);

            // each part alone (solo) is audible: melodic parts + rhythm
            juce::String quiet;
            for (int part = 0; part < mt::kParts; ++part)
            {
                for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "solo"), q == part ? 1.0f : 0.0f);
                juce::MidiBuffer m;
                const int ch = part < 8 ? part + 2 : 10, key = part < 8 ? 60 : 38;
                m.addEvent (juce::MidiMessage::noteOn (ch, key, (juce::uint8) 110), 100);
                m.addEvent (juce::MidiMessage::noteOff (ch, key), 30000);
                const auto rr = run (p, m, 1.0, nch);
                if (rr.peak < 0.003f) quiet << (part < 8 ? juce::String (part + 1) : juce::String ("R")) << " ";
            }
            for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "solo"), 0.0f);
            check (quiet.isEmpty(), "every part (1-8 + rhythm) sounds on its own channel", quiet.isEmpty() ? juce::String() : "silent: " + quiet);

            // mute silences a part (after the previous tails have died away)
            p.requestPanic(); run (p, {}, 3.0, nch);
            setParam (p, lpid::part (0, "mute"), 1.0f);
            {
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 110), 0);
                const auto rr = run (p, m, 0.6, nch);
                check (rr.peak < 0.002f, "mute silences part 1", juce::String (rr.peak, 4));
                juce::MidiBuffer off; off.addEvent (juce::MidiMessage::allNotesOff (2), 0); run (p, off, 0.5, nch);
            }
            setParam (p, lpid::part (0, "mute"), 0.0f);

            // multi-out: part 3 on OUT 4 (aux bus 3 -> channels 2*3 .. 2*3+1)
            setParam (p, lpid::part (2, "out"), 1.0f);
            p.requestPanic(); run (p, {}, 3.0, nch);
            {
                std::vector<float> aux ((size_t) nch, 0.0f);
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (4, 40, (juce::uint8) 110), 0);
                m.addEvent (juce::MidiMessage::noteOff (4, 40), 20000);
                for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "rev"), 0.0f);
                const auto rr = run (p, m, 0.8, nch, &aux);
                check (aux[6] > 0.01f && rr.peak < 0.002f, "part 3 routed to OUT 4 only", "aux " + juce::String (aux[6], 3) + " main " + juce::String (rr.peak, 4));
                for (int q = 0; q < mt::kParts; ++q) setParam (p, lpid::part (q, "rev"), 1.0f);
            }
            setParam (p, lpid::part (2, "out"), 0.0f);

            // program change -> name
            {
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::programChange (2, 48), 0);
                run (p, m, 0.05, nch);
                check (p.partProgram (0) == 48 && p.partName (0) == p.currentRom()->melodic[48].getName().trim(), "program change 49 on part 1", p.partName (0));
            }
            // MT-32 SysEx: reverb mode -> parameter (via the processor's timer)
            {
                juce::MidiBuffer m;
                const auto s = mt::Memory::systemWrite (1, 2);    // plate
                m.addEvent (juce::MidiMessage::createSysExMessage (s.data() + 1, (int) s.size() - 2), 0);
                run (p, m, 0.05, nch);
                juce::MessageManager::getInstance()->runDispatchLoopUntil (150);
                check ((int) p.apvts.getRawParameterValue (lpid::rvMode)->load() == 2, "SysEx reverb mode -> PLATE in the UI parameter");
            }
            // reverb / echo on-off switches
            {
                auto tailDb = [&] (bool rvOn, bool ecOn)
                {
                    setParam (p, lpid::rvOn, rvOn ? 1.0f : 0.0f); setParam (p, lpid::ecOn, ecOn ? 1.0f : 0.0f);
                    setParam (p, lpid::part (0, "ech"), 1.0f); setParam (p, lpid::part (0, "rev"), 1.0f);
                    setParam (p, lpid::rvLevel, 7.0f);
                    p.requestPanic(); run (p, {}, 2.0, nch);
                    juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 72, (juce::uint8) 110), 0); m.addEvent (juce::MidiMessage::noteOff (2, 72), 4800);
                    const auto r = run (p, m, 1.5, nch);
                    double e = 0; for (size_t i = 24000; i < r.main.size(); ++i) e += r.main[i] * r.main[i];
                    return 10.0 * std::log10 (juce::jmax (1.0e-12, e / 48000.0));
                };
                const double both = tailDb (true, true), noRv = tailDb (false, true), noEc = tailDb (true, false), none = tailDb (false, false);
                check (none < juce::jmin (noRv, noEc) - 20.0 && both >= juce::jmax (noRv, noEc) - 0.5, "REVERB / ECHO switches remove their tails",
                       juce::String (both, 1) + " / -rv " + juce::String (noRv, 1) + " / -ec " + juce::String (noEc, 1) + " / none " + juce::String (none, 1) + " dB");
                setParam (p, lpid::rvOn, 1.0f); setParam (p, lpid::ecOn, 1.0f); setParam (p, lpid::part (0, "ech"), 0.0f); setParam (p, lpid::rvLevel, 3.0f);
            }
            // timbre write by SysEx (part 1 timbre temp name)
            {
                const char nm[10] = { 'C', 'O', 'H', 'E', 'S', 'I', 'O', 'N', ' ', ' ' };
                const auto s = mt::Memory::dt1 (mt::kTimbreTemp, reinterpret_cast<const uint8_t*> (nm), 10);
                juce::MidiBuffer m; m.addEvent (juce::MidiMessage::createSysExMessage (s.data() + 1, (int) s.size() - 2), 0);
                run (p, m, 0.05, nch);
                check (p.partName (0) == "COHESION", "SysEx timbre-temp write renames part 1", p.partName (0));
            }
        }
    }

    // state round trip
    {
        setParam (p, lpid::part (4, "lvl"), 0.5f);
        p.requestProgram (3, 77);
        run (p, {}, 0.05, nch);
        juce::MemoryBlock st; p.getStateInformation (st);
        LaModuleProcessor q;
        q.setStateInformation (st.getData(), (int) st.getSize());
        check (q.currentRomLabel() == p.currentRomLabel() && q.partProgram (3) == 77
               && std::abs (q.apvts.getRawParameterValue (lpid::part (4, "lvl"))->load() - 0.5f) < 1e-3f, "state round trip (ROM, programs, mixer)",
               q.currentRomLabel() + " / " + q.partName (3));
    }

    // UI snapshot
    {
        p.requestProgram (0, 0);
        juce::MidiBuffer m; m.addEvent (juce::MidiMessage::noteOn (2, 60, (juce::uint8) 100), 0); m.addEvent (juce::MidiMessage::noteOn (10, 36, (juce::uint8) 100), 0);
        setParam (p, lpid::part (5, "mute"), 1.0f);
        setParam (p, lpid::ecOn, 0.0f); // snapshot: echo switched off
        setParam (p, lpid::part (2, "out"), 1.0f);
        run (p, m, 0.1, nch);
        std::unique_ptr<juce::AudioProcessorEditor> ed (p.createEditor());
        auto* e = dynamic_cast<LaModuleEditor*> (ed.get());
        e->selectPart (1);
        juce::MessageManager::getInstance()->runDispatchLoopUntil (200);
        auto save = [&] (const char* name)
        {
            auto img = ed->createComponentSnapshot (ed->getLocalBounds(), true, 1.0f);
            auto f = outDir.getChildFile (juce::String (name) + ".png"); f.deleteFile();
            juce::FileOutputStream os (f); juce::PNGImageFormat png; png.writeImageToStream (img, os);
            std::printf ("wrote %s\n", f.getFullPathName().toRawUTF8());
        };
        save ("1-module");
        ed->setSize (728, 462); save ("2-scaled-70");
    }
    tmp.deleteRecursively();
    std::printf ("\n%s (%d failures)\n", failures == 0 ? "ALL PASSED" : "FAILED", failures);
    return failures == 0 ? 0 : 1;
}
