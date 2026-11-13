// Checks: (1) a VA oscillator switched on over a DX7 voice is shaped by the AMP envelope
// (no endless raw tone), (2) a preset change while notes ring is click-free: old voices
// stop under a short fade and never play with the new preset's settings.
//   SwitchTest <file.syx> [out.wav]
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <thread>

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what); if (! ok) ++fails; }
static double db (double x) { return 20.0 * std::log10 (juce::jmax (1.0e-9, x)); }

static void setP (CohesionSynthAudioProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

// render `sec` seconds; note on at 0 (if note >= 0), off at `offSec`; returns per-10ms RMS
static std::vector<double> render (CohesionSynthAudioProcessor& p, double sr, double sec, int note, double offSec)
{
    const int bs = 256; juce::AudioBuffer<float> b (2, bs);
    std::vector<double> rms; double acc = 0; int cnt = 0;
    const int total = (int) (sec * sr), offAt = (int) (offSec * sr);
    for (int pos = 0; pos < total; pos += bs)
    {
        juce::MidiBuffer m;
        if (pos == 0 && note >= 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0);
        if (note >= 0 && offAt >= pos && offAt < pos + bs) m.addEvent (juce::MidiMessage::noteOff (1, note), offAt - pos);
        p.processBlock (b, m);
        for (int i = 0; i < bs; ++i)
        {
            acc += 0.5 * (b.getSample (0, i) * b.getSample (0, i) + b.getSample (1, i) * b.getSample (1, i));
            if (++cnt == (int) (sr * 0.01)) { rms.push_back (std::sqrt (acc / cnt)); acc = 0; cnt = 0; }
        }
    }
    return rms;
}
static double avg (const std::vector<double>& r, double t0, double t1)
{
    double s = 0; int n = 0;
    for (int i = (int) (t0 * 100); i < (int) (t1 * 100) && i < (int) r.size(); ++i) { s += r[(size_t) i] * r[(size_t) i]; ++n; }
    return n ? std::sqrt (s / n) : 0.0;
}

int main (int argc, char** argv)
{
    if (argc < 2) { std::printf ("usage: SwitchTest <file.syx> [out.wav]\n"); return 2; }
    juce::ScopedJuceInitialiser_GUI init;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion_switchtest");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_SYSEX_DIR", tmp.getChildFile ("sysex").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_ROM_DIR",   tmp.getChildFile ("roms").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_PRESET_DIR", tmp.getChildFile ("presets").getFullPathName().toRawUTF8());

    const double sr = 48000.0;
    CohesionSynthAudioProcessor p;
    p.setPlayConfigDetails (0, 2, sr, 256);
    p.prepareToPlay (sr, 256);

    juce::Array<juce::File> items; items.add (juce::File (juce::String (argv[1])));
    const auto imp = p.importSysEx (items);
    int dxIdx = -1;
    for (int i = 0; i < p.numPresets(); ++i) if (p.presetCategory (i) == imp.firstCategory) { dxIdx = i; break; }
    check (dxIdx >= 0, "DX7 bank imported");
    if (dxIdx < 0) return 1;
    std::printf ("  DX voice: %s\n", p.presetName (dxIdx).toRawUTF8());

    // ---- 1. VA layer over a DX7 voice ----
    std::printf ("1. OSC 1 switched on over a DX7 voice\n");
    p.loadPreset (dxIdx);
    const auto fm = render (p, sr, 4.0, 60, 1.0);
    setP (p, pid::oscOn (1), 1.0f);
    const auto lay = render (p, sr, 4.0, 60, 1.0);
    const double fmHold = avg (fm, 0.2, 1.0), layHold = avg (lay, 0.2, 1.0);
    std::printf ("  FM alone %.1f dB, FM + OSC 1 %.1f dB while held\n", db (fmHold), db (layHold));
    std::printf ("  after release: +0.5 s  FM %.1f / layered %.1f dB,  +2.5 s  FM %.1f / layered %.1f dB\n",
                 db (avg (fm, 1.5, 1.6)), db (avg (lay, 1.5, 1.6)), db (avg (fm, 3.5, 3.6)), db (avg (lay, 3.5, 3.6)));
    check (avg (lay, 1.5, 1.6) < avg (fm, 1.5, 1.6) * 1.5 + 1.0e-4, "the OSC layer has faded 0.5 s after release (AMP release 250 ms)");
    check (avg (lay, 3.5, 4.0) < 1.0e-3, "no endless tone after release");
    setP (p, pid::dxOn, 0.0f);
    const auto va = render (p, sr, 2.0, 60, 1.0);
    std::printf ("  OSC 1 alone %.1f dB while held\n", db (avg (va, 0.2, 1.0)));
    check (avg (va, 0.2, 1.0) > 1.0e-3, "OSC 1 sounds on a DX7 preset");
    setP (p, pid::dxOn, 1.0f);
    setP (p, pid::oscOn (1), 0.0f);
    const auto fm2 = render (p, sr, 4.0, 60, 1.0);
    // (the FM voice starts with random operator phases: a few dB of spread between notes is normal)
    check (std::abs (db (avg (fm2, 0.2, 1.0)) - db (fmHold)) < 3.0, "the FM voice level is unchanged by the AMP envelope");

    // ---- 2. preset change while a note rings (audio on its own thread, as in a host) ----
    std::printf ("2. preset change during a ringing note\n");
    p.loadPreset (0);                          // Default (VA)
    setP (p, pid::ampR, 4000.0f);              // long release: the voice still rings when we switch
    std::atomic<bool> run { true }; std::atomic<int> noteReq { 0 }; std::atomic<int> mark { -1 };
    std::vector<float> rec; rec.reserve ((size_t) (sr * 8));
    std::thread audio ([&]
    {
        const int bs = 256; juce::AudioBuffer<float> b (2, bs);
        while (run)
        {
            juce::MidiBuffer m;
            const int r = noteReq.exchange (0);
            if (r > 0) m.addEvent (juce::MidiMessage::noteOn (1, 57, (juce::uint8) 110), 0);
            if (r < 0) m.addEvent (juce::MidiMessage::noteOff (1, 57), 0);
            p.processBlock (b, m);
            for (int i = 0; i < bs; ++i) rec.push_back (b.getSample (0, i));
            std::this_thread::sleep_for (std::chrono::microseconds (2500));   // ~ real time (256 / 48k = 5.3 ms)
        }
    });
    auto waitBlocks = [] (int ms) { std::this_thread::sleep_for (std::chrono::milliseconds (ms)); };
    waitBlocks (100); noteReq = 1; waitBlocks (400); noteReq = -1; waitBlocks (150);
    const size_t before = rec.size();
    p.loadPreset (dxIdx);                      // switch while the VA note rings
    const size_t after = rec.size();
    waitBlocks (300);
    noteReq = 1; waitBlocks (400); noteReq = -1; waitBlocks (100);
    const size_t before2 = rec.size();
    p.loadPreset (1);                          // and back to a VA preset while the DX note decays
    const size_t after2 = rec.size();
    waitBlocks (300);
    run = false; audio.join();

    auto rmsAt = [&] (size_t a, size_t b) { double s = 0; for (size_t i = a; i < b && i < rec.size(); ++i) s += rec[i] * rec[i]; return std::sqrt (s / (double) juce::jmax<size_t> (1, b - a)); };
    auto maxStep = [&] (size_t a, size_t b) { float m = 0; for (size_t i = juce::jmax<size_t> (1, a); i < b && i < rec.size(); ++i) m = juce::jmax (m, std::abs (rec[i] - rec[i - 1])); return m; };
    const size_t ms = (size_t) (sr * 0.001);
    std::printf ("  switch 1 took %.1f ms of audio, switch 2 %.1f ms\n", (after - before) / (sr * 0.001), (after2 - before2) / (sr * 0.001));
    std::printf ("  ringing before switch 1: %.1f dB, after: %.1f dB\n", db (rmsAt (before - 50 * ms, before)), db (rmsAt (after + 20 * ms, after + 250 * ms)));
    check (rmsAt (before - 50 * ms, before) > 1.0e-3, "the note was still ringing at switch 1");
    check (rmsAt (after + 20 * ms, after + 250 * ms) < 1.0e-5, "after switch 1 the old voice is silent (does not play with the new preset)");
    check (rmsAt (after2 + 20 * ms, after2 + 250 * ms) < 1.0e-5, "after switch 2 the old voice is silent");
    const float stepPlay = maxStep (before - 200 * ms, before - 20 * ms), stepSw = maxStep (before - 10 * ms, after + 20 * ms);
    std::printf ("  largest sample step: while ringing %.4f, around the switch %.4f\n", stepPlay, stepSw);
    check (stepSw <= stepPlay * 1.05f + 1.0e-4f, "no jump at the switch larger than in the sound itself");

    // ---- 3. imported voices are loudness-matched like the factory presets (-18 dB) ----
    std::printf ("3. loudness of the imported DX7 voices (chord C3-E3-G3, momentary max 300 ms)\n");
    std::vector<double> dev, raw;
    for (int i = dxIdx; i < p.numPresets() && p.presetCategory (i) == imp.firstCategory; ++i)
    {
        p.loadPreset (i);
        const float main = p.apvts.getRawParameterValue (pid::mainVol)->load();
        const float drive = p.apvts.getRawParameterValue (pid::limDrive)->load();
        const int bs = 512; juce::AudioBuffer<float> b (2, bs); std::vector<float> mono;
        for (int pos = 0; pos < (int) (2.0 * sr); pos += bs)
        {
            juce::MidiBuffer m;
            if (pos == 0) for (int nn : { 48, 52, 55 }) m.addEvent (juce::MidiMessage::noteOn (1, nn, 0.8f), 0);
            if (pos <= (int) (1.5 * sr) && (int) (1.5 * sr) < pos + bs) for (int nn : { 48, 52, 55 }) m.addEvent (juce::MidiMessage::noteOff (1, nn), (int) (1.5 * sr) - pos);
            p.processBlock (b, m);
            for (int k = 0; k < bs; ++k) mono.push_back (0.5f * (b.getSample (0, k) + b.getSample (1, k)));
        }
        const int win = (int) (0.3 * sr); double loud = 0;
        for (int st = 0; st + win <= (int) (1.8 * sr); st += (int) (0.025 * sr))
        { double e = 0; for (int k = 0; k < win; ++k) e += mono[(size_t) (st + k)] * mono[(size_t) (st + k)]; loud = juce::jmax (loud, std::sqrt (e / win)); }
        const double ld = db (loud);
        dev.push_back (ld + 18.0);
        raw.push_back (ld - db (main) - drive + db (0.8));   // what it was at the old fixed MAIN 0.8
        render (p, sr, 2.0, -1, 0.0);                       // let the tail die
    }
    auto spread = [] (std::vector<double> v) { std::sort (v.begin(), v.end()); return std::make_pair (v.front(), v.back()); };
    const auto sNew = spread (dev), sOld = spread (raw);
    std::printf ("  %d voices: before %+.1f .. %+.1f dB around -18, now %+.1f .. %+.1f dB\n", (int) dev.size(), sOld.first + 18.0, sOld.second + 18.0, sNew.first, sNew.second);
    int within = 0; for (double d : dev) if (std::abs (d) <= 3.0) ++within;
    check (within >= (int) dev.size() * 9 / 10, "at least 90 % of the voices within 3 dB of the target");

    // ---- 4. removing an imported bank from the library ----
    std::printf ("4. remove the DX7 bank\n");
    const int before4 = p.numPresets();
    const int gone = p.removeSysExBank (imp.firstCategory);
    int left = 0; for (int i = 0; i < p.numPresets(); ++i) if (p.presetCategory (i) == imp.firstCategory) ++left;
    check (gone == imp.voices && p.numPresets() == before4 - gone && left == 0, "the bank's voices are gone from the library");
    check (CohesionSynthAudioProcessor::userSysExDir().findChildFiles (juce::File::findFiles, true, "*.syx").isEmpty(), "its copy is deleted from the library folder");
    check (juce::File (juce::String (argv[1])).existsAsFile(), "the original file is untouched");
    const auto again = p.importSysEx (items);
    check (again.voices == imp.voices, "the bank can be imported again (no stale duplicates)");

    // ---- 5. user presets: save, load back (parameters, FX order, DX7 voice), rename, delete ----
    std::printf ("5. user presets\n");
    {
        using P = CohesionSynthAudioProcessor;
        int dx = -1;
        for (int i = 0; i < p.numPresets(); ++i) if (p.presetCategory (i) == again.firstCategory) { dx = i; break; }
        p.loadPreset (dx);
        const auto dxName = p.getFmPatch().getName();
        setP (p, pid::oscOn (2), 1.0f); setP (p, pid::fltCutoff, 1234.0f); setP (p, pid::oscWtBank (2), 7.0f);
        const int order[5] = { 4, 3, 2, 1, 0 }; p.setFxOrder (order);
        p.apvts.state.setProperty ("uiScale", 1.25, nullptr);

        const int before5 = p.numPresets();
        const int u = p.saveUserPreset ("  My Pad: warm?  ", false);
        check (u >= 0 && p.presetIsUser (u) && p.currentPresetIndex() == u && p.presetCategory (u) == P::kUserCategory,
               "the sound is saved as a User preset and becomes the current preset");
        check (p.numPresets() == before5 + 1 && P::userPresetDir().getChildFile (p.presetName (u) + ".cspreset").existsAsFile(),
               "one file in the preset folder");
        std::printf ("  saved as \"%s\"\n", p.presetName (u).toRawUTF8());
        check (p.saveUserPreset (p.presetName (u), false) == P::kSaveExists, "saving under an existing name asks first");
        check (p.saveUserPreset (p.presetName (u), true) >= 0, "and replaces it when confirmed");
        check (p.saveUserPreset ("   ", false) == P::kSaveBadName, "an empty name is refused");

        p.loadPreset (dx + 1);                      // another DX voice, other settings
        p.apvts.state.setProperty ("uiScale", 1.7, nullptr);
        const int u2 = [&] { for (int i = 0; i < p.numPresets(); ++i) if (p.presetIsUser (i)) return i; return -1; }();
        p.loadPreset (u2);
        int o[5]; p.getFxOrder (o);
        const float cut = p.apvts.getRawParameterValue (pid::fltCutoff)->load();
        check (std::abs (cut - 1234.0f) < 1.0f && p.apvts.getRawParameterValue (pid::oscOn (2))->load() > 0.5f
               && (int) p.apvts.getRawParameterValue (pid::oscWtBank (2))->load() == 7, "loading it restores the parameters");
        check (o[0] == 4 && o[4] == 0, "and the FX order");
        check (p.getFmPatch().getName() == dxName, "and the DX7 voice");
        check (std::abs ((double) p.apvts.state.getProperty ("uiScale") - 1.7) < 1.0e-6, "but not the window size");
        const auto played = render (p, sr, 1.0, 60, 0.5);
        check (avg (played, 0.1, 0.5) > 1.0e-3, "the loaded User preset plays");

        {   // a second instance (another project / plugin) sees the preset
            P p2; int n = 0; for (int i = 0; i < p2.numPresets(); ++i) if (p2.presetIsUser (i)) ++n;
            check (n == 1, "a new plugin instance lists the User preset");
        }

        const int r = p.renameUserPreset (u2, "Warm Pad");
        check (r >= 0 && p.presetName (r) == "Warm Pad" && p.currentPresetIndex() == r, "rename keeps it the current preset");
        const int r2 = p.renameUserPreset (r, "WARM pad");
        check (r2 >= 0 && p.presetName (r2) == "WARM pad" && P::userPresetDir().getChildFile ("WARM pad.cspreset").existsAsFile(),
               "a rename that only changes the case keeps the file");
        p.saveUserPreset ("Other", false);
        int other = -1; for (int i = 0; i < p.numPresets(); ++i) if (p.presetName (i) == "Other" && p.presetIsUser (i)) other = i;
        check (p.renameUserPreset (other, "warm PAD") == P::kSaveExists, "renaming onto another preset's name is refused");
        const int nBefore = p.numPresets();
        for (int i = p.numPresets() - 1; i >= 0; --i) if (p.presetIsUser (i)) p.deleteUserPreset (i);
        check (p.numPresets() == nBefore - 2 && P::userPresetDir().findChildFiles (juce::File::findFiles, false, "*.cspreset").isEmpty(),
               "delete removes the presets and their files");
    }

    // ---- 6. multi sounds: a part at a time, MIXER level / mute / solo / keys / ARP / sends / filter ----
    std::printf ("6. multi sounds\n");
    {
        int dx = -1, pad = -1;
        for (int i = 0; i < p.numPresets(); ++i)
        {
            if (dx < 0 && p.presetCategory (i) == again.firstCategory) dx = i;
            if (pad < 0 && p.presetCategory (i) == "Pad") pad = i;
        }
        auto peaks = [&] (int note, double sec)   // render, return the peak per part
        {
            render (p, sr, 0.05, -1, 0.0);           // a MIXER change ramps over one block
            for (int k = 0; k < 3; ++k) p.takePartPeak (k);
            render (p, sr, sec, note, sec * 0.8);
            std::array<float, 3> pk {};
            for (int k = 0; k < 3; ++k) pk[(size_t) k] = p.takePartPeak (k);
            render (p, sr, 5.0, -1, 0.0);            // let the pad's release die
            return pk;
        };
        p.loadPreset (pad);
        setP (p, pid::rvOn, 1.0f);                                          // a shared setting the part loads must keep
        p.loadPresetIntoPart (dx, pid::kFM);
        const bool vaKept = p.apvts.getRawParameterValue (pid::oscOn (1))->load() > 0.5f;
        check (vaKept && p.apvts.getRawParameterValue (pid::dxOn)->load() > 0.5f, "loading into the FM part keeps the VA part");
        check (p.apvts.getRawParameterValue (pid::rvOn)->load() > 0.5f, "and leaves the FX alone");
        check (p.isMulti() && p.partSoundName (pid::kVA).contains ("Pad") && p.partSoundName (pid::kFM).isNotEmpty(), "both parts are named");
        std::printf ("  header: %s\n", p.headerName().toRawUTF8());
        std::printf ("  FM part level set to %.1f dB (loudness match)\n", p.apvts.getRawParameterValue (pid::partLevel (pid::kFM))->load());

        auto pk = peaks (60, 1.0);
        std::printf ("  peaks VA %.3f  FM %.3f  LA %.3f\n", pk[0], pk[1], pk[2]);
        check (pk[0] > 1.0e-3f && pk[1] > 1.0e-3f && pk[2] == 0.0f, "VA and FM play together on their own buses");
        setP (p, pid::partMute (pid::kFM), 1.0f);
        pk = peaks (60, 0.6);
        check (pk[0] > 1.0e-3f && pk[1] < 1.0e-5f, "MUTE silences the FM part only");
        setP (p, pid::partMute (pid::kFM), 0.0f); setP (p, pid::partSolo (pid::kFM), 1.0f);
        pk = peaks (60, 0.6);
        check (pk[0] < 1.0e-5f && pk[1] > 1.0e-3f, "SOLO plays only the FM part");
        setP (p, pid::partSolo (pid::kFM), 0.0f);

        setP (p, pid::partKeyHi (pid::kVA), 59.0f); setP (p, pid::partKeyLo (pid::kFM), 60.0f);   // split at C4
        const auto lo = peaks (48, 0.6), hi = peaks (72, 0.6);
        std::printf ("  split: C3 -> VA %.3f FM %.3f,  C5 -> VA %.3f FM %.3f\n", lo[0], lo[1], hi[0], hi[1]);
        check (lo[0] > 1.0e-3f && lo[1] < 1.0e-5f && hi[0] < 1.0e-5f && hi[1] > 1.0e-3f, "key ranges split the keyboard (VA low, FM high)");
        setP (p, pid::partKeyHi (pid::kVA), 127.0f); setP (p, pid::partKeyLo (pid::kFM), 0.0f);

        // ARP on, the VA part takes the played keys: it holds a steady note, the FM part arpeggiates
        p.loadPreset (0);                                   // Init: plain VA, short release
        setP (p, pid::ampA, 1.0f); setP (p, pid::ampR, 10.0f);
        setP (p, pid::arpOn, 1.0f); setP (p, pid::arpSync, 0.0f); setP (p, pid::arpTempo, 120.0f);
        setP (p, pid::partArp (pid::kVA), 0.0f);
        const auto held = render (p, sr, 2.0, 60, 1.8);
        double mn = 1.0, mx = 0.0; for (int i = 60; i < 170; ++i) { mn = std::min (mn, held[(size_t) i]); mx = std::max (mx, held[(size_t) i]); }
        std::printf ("  VA with ARP off (ARP running): 10 ms RMS min %.1f / max %.1f dB while held\n", db (mn), db (mx));
        check (mn > mx * 0.3, "a part with ARP off plays the held key, not the ARP's gated notes");
        setP (p, pid::partArp (pid::kVA), 1.0f);
        const auto arped = render (p, sr, 2.0, 60, 1.8);
        mn = 1.0; mx = 0.0; for (int i = 60; i < 170; ++i) { mn = std::min (mn, arped[(size_t) i]); mx = std::max (mx, arped[(size_t) i]); }
        std::printf ("  VA with ARP on: min %.1f / max %.1f dB\n", db (mn), db (mx));
        check (mn < mx * 0.3, "with its ARP switch on, the part plays the ARP's notes");
        setP (p, pid::arpOn, 0.0f);
        render (p, sr, 1.5, -1, 0.0);

        // sends: echo at 100 % wet; a part without echo send has no echo tail
        p.loadPreset (0);                                   // Init: a plain VA sound
        setP (p, pid::ampR, 50.0f);
        setP (p, pid::ecOn, 1.0f); setP (p, pid::ecMix, 1.0f); setP (p, pid::ecTime, 300.0f); setP (p, pid::ecFb, 0.3f); setP (p, pid::ecSync, 0.0f);
        render (p, sr, 1.0, -1, 0.0);
        const auto wet = render (p, sr, 1.5, 60, 0.2);
        setP (p, pid::partEcho (pid::kVA), 0.0f);
        render (p, sr, 1.5, -1, 0.0);
        const auto dry = render (p, sr, 1.5, 60, 0.2);
        std::printf ("  0.45 .. 0.6 s (echo repeat): send 100 %% %.1f dB, send 0 %% %.1f dB; while held %.1f / %.1f dB\n",
                     db (avg (wet, 0.45, 0.6)), db (avg (dry, 0.45, 0.6)), db (avg (wet, 0.05, 0.2)), db (avg (dry, 0.05, 0.2)));
        check (avg (wet, 0.45, 0.6) > 1.0e-3 && avg (dry, 0.45, 0.6) < avg (wet, 0.45, 0.6) * 0.01, "ECHO SEND 0 keeps the part out of the echo");
        check (avg (dry, 0.05, 0.2) > 1.0e-3, "and the part itself stays dry and audible (the echo MIX does not touch it)");
        setP (p, pid::partEcho (pid::kVA), 1.0f); setP (p, pid::ecOn, 0.0f);

        // FM through the VA filter
        p.loadPresetIntoPart (dx, pid::kFM);
        for (int i = 1; i <= 4; ++i) setP (p, pid::oscOn (i), 0.0f);
        setP (p, pid::fltCutoff, 150.0f); setP (p, pid::fltRes, 0.0f);
        const auto open = peaks (72, 0.6);
        setP (p, pid::partFilter (pid::kFM), 1.0f);
        const auto filt = peaks (72, 0.6);
        std::printf ("  FM part peak: own %.3f, through the VA filter at 150 Hz %.3f\n", open[1], filt[1]);
        check (filt[1] < open[1] * 0.3f, "VA FILTER runs the FM part through the VA filters");

        // a multi sound saved as a User preset comes back whole
        p.loadPreset (pad); p.loadPresetIntoPart (dx, pid::kFM);
        setP (p, pid::partPan (pid::kFM), -0.5f);
        const auto fmName = p.partSoundName (pid::kFM);
        const int u = p.saveUserPreset ("Multi Test", false);
        // only compatible presets per part: DX7 voices only in FM, factory sounds only in VA,
        // a User preset only in the parts that sound in it (this one: VA + FM, no LA)
        bool strict = true;
        for (int i = 0; i < p.numPresets(); ++i)
        {
            if (p.presetIsSysEx (i)) strict = strict && p.presetFitsPart (i, pid::kFM) && ! p.presetFitsPart (i, pid::kVA) && ! p.presetFitsPart (i, pid::kLA);
            else if (! p.presetIsUser (i) && ! p.presetIsImported (i))
                strict = strict && p.presetFitsPart (i, pid::kVA) && ! p.presetFitsPart (i, pid::kFM) && ! p.presetFitsPart (i, pid::kLA);
        }
        check (strict, "each part lists only compatible presets (no DX7 voices in the VA or LA part)");
        check (p.presetFitsPart (u, pid::kVA) && p.presetFitsPart (u, pid::kFM) && ! p.presetFitsPart (u, pid::kLA),
               "a User preset is offered only to the parts that sound in it");
        p.loadPreset (0);
        p.loadPreset (u);
        check (p.apvts.getRawParameterValue (pid::dxOn)->load() > 0.5f && p.apvts.getRawParameterValue (pid::oscOn (1))->load() > 0.5f
               && std::abs (p.apvts.getRawParameterValue (pid::partPan (pid::kFM))->load() + 0.5f) < 1.0e-3f
               && p.partSoundName (pid::kFM) == fmName, "a multi sound saved as User preset loads back with its parts and MIXER");
        {
            const auto pk = peaks (60, 0.6);
            std::printf ("  loaded multi User preset: VA peak %.4f, FM peak %.4f\n", pk[0], pk[1]);
            check (pk[0] > 1.0e-3f && pk[1] > 1.0e-3f, "the loaded multi User preset sounds (VA and FM part)");
            // VA part ON switch and VA FILT+MOD (off: the VA oscillators go dry)
            setP (p, pid::vaOn, 0.0f);
            const auto off = peaks (60, 0.6);
            check (off[0] < 1.0e-5f && off[1] > 1.0e-3f, "VA ON off silences only the VA part");
            setP (p, pid::vaOn, 1.0f);
            setP (p, pid::fltCutoff, 100.0f); setP (p, pid::flt2Cutoff, 100.0f);
            const auto filt = peaks (72, 0.6);
            setP (p, pid::vaFiltMod, 0.0f);
            const auto dry = peaks (72, 0.6);
            std::printf ("  VA part at cutoff 100 Hz: through the filters %.4f, VA FILT+MOD off %.4f\n", filt[0], dry[0]);
            check (dry[0] > filt[0] * 3.0f, "VA FILT+MOD off takes the VA part past its filters");
            setP (p, pid::vaFiltMod, 1.0f);
        }
        {   // a file with parameters stored without a value (JUCE: never changed since an older
            // project) must load them as their default, not as 0 (KEY HIGH 0 = silence)
            auto xml = juce::XmlDocument::parse (p.presetUserFile (u));
            bool blanked = false, complete = true;
            if (auto* st = xml != nullptr ? xml->getChildByName ("PARAMS") : nullptr)
                for (auto* c : st->getChildIterator())
                {
                    if (c->getStringAttribute ("value").isEmpty()) complete = false;
                    if (c->getStringAttribute ("id").startsWith ("part")) { c->setAttribute ("value", ""); blanked = true; }
                }
            check (complete, "a saved User preset stores every parameter value");
            xml->writeTo (p.presetUserFile (u));
            p.loadPreset (0); p.loadPreset (u);
            const auto pk = peaks (60, 0.6);
            std::printf ("  User preset with empty MIXER values: VA peak %.4f, FM peak %.4f\n", pk[0], pk[1]);
            check (blanked && pk[0] > 1.0e-3f && pk[1] > 1.0e-3f, "empty stored values load as defaults (the parts still sound)");
        }
        // ... and its FM part alone can be loaded into another sound
        p.loadPreset (pad);
        p.loadPresetIntoPart (u, pid::kFM);
        check (p.apvts.getRawParameterValue (pid::dxOn)->load() > 0.5f && p.partSoundName (pid::kVA).contains ("Pad"),
               "the FM part of a User preset loads alone");
        p.deleteUserPreset (u);
    }

    if (argc > 2)
    {
        juce::WavAudioFormat wav; juce::File out { juce::String (argv[2]) }; out.deleteFile();
        std::unique_ptr<juce::AudioFormatWriter> w (wav.createWriterFor (new juce::FileOutputStream (out), sr, 1, 24, {}, 0));
        juce::AudioBuffer<float> b (1, (int) rec.size()); b.copyFrom (0, 0, rec.data(), (int) rec.size());
        w->writeFromAudioSampleBuffer (b, 0, b.getNumSamples());
    }
    tmp.deleteRecursively();
    std::printf (fails ? "%d FAILED\n" : "all checks passed\n", fails);
    return fails ? 1 : 0;
}
