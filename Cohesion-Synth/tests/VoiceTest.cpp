// Voice checks: pitch bend, voice stealing without clicks, HP 24, noise filter,
// LFO tempo sync, analog envelope curve, and a CPU measurement (16 voices).
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>

static int fails = 0;
static void check (bool ok, const char* what) { std::printf ("  [%s] %s\n", ok ? " OK " : "FAIL", what); if (! ok) ++fails; }
static double db (double x) { return 20.0 * std::log10 (juce::jmax (1.0e-9, x)); }
static const double SR = 48000.0;

static void setP (CohesionSynthAudioProcessor& p, const juce::String& id, float v)
{
    auto* prm = p.apvts.getParameter (id);
    prm->setValueNotifyingHost (prm->convertTo0to1 (v));
}

struct Rec { std::vector<float> L, R; };
// renders `sec` seconds; `events(pos, midi)` adds MIDI for the block starting at sample pos
template <typename Ev>
static Rec render (CohesionSynthAudioProcessor& p, double sec, Ev events)
{
    const int bs = 256; juce::AudioBuffer<float> b (2, bs); Rec r;
    for (int pos = 0; pos < (int) (sec * SR); pos += bs)
    {
        juce::MidiBuffer m; events (pos, bs, m);
        p.processBlock (b, m);
        for (int i = 0; i < bs; ++i) { r.L.push_back (b.getSample (0, i)); r.R.push_back (b.getSample (1, i)); }
    }
    return r;
}
static double rms (const std::vector<float>& x, size_t a, size_t b)
{
    double s = 0; for (size_t i = a; i < b && i < x.size(); ++i) s += (double) x[i] * x[i];
    return std::sqrt (s / (double) juce::jmax<size_t> (1, b - a));
}
static double freqOf (const std::vector<float>& x, size_t a, size_t b)   // zero crossings
{
    int up = 0; size_t first = 0, last = 0;
    for (size_t i = a + 1; i < b; ++i) if (x[i - 1] < 0.0f && x[i] >= 0.0f) { if (up == 0) first = i; last = i; ++up; }
    return up > 1 ? (up - 1) * SR / (double) (last - first) : 0.0;
}
static void sineInit (CohesionSynthAudioProcessor& p)
{
    p.loadPreset (0);
    setP (p, pid::oscOn (1), 1); setP (p, pid::oscWave (1), 3); setP (p, pid::oscLevel (1), 0.5f);
    setP (p, pid::oscOn (2), 0); setP (p, pid::fltCutoff, 20000); setP (p, pid::fltRes, 0);
    setP (p, pid::limOn, 0);
}

int main()
{
    juce::ScopedJuceInitialiser_GUI init;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion_voicetest");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_SYSEX_DIR", tmp.getChildFile ("sysex").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_PRESET_DIR", tmp.getChildFile ("presets").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_ROM_DIR",   tmp.getChildFile ("roms").getFullPathName().toRawUTF8());

    CohesionSynthAudioProcessor p;
    p.setPlayConfigDetails (0, 2, SR, 256);
    p.prepareToPlay (SR, 256);

    // ---- pitch bend ----
    std::printf ("pitch bend\n");
    sineInit (p);
    auto r = render (p, 1.0, [] (int pos, int, juce::MidiBuffer& m)
    {
        if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
        if (pos == 24064) m.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
    });
    const double f0 = freqOf (r.L, 4800, 24000), f1 = freqOf (r.L, 30000, 47000);
    std::printf ("  A4 %.1f Hz, bend up %.1f Hz (expect 493.9)\n", f0, f1);
    check (std::abs (f0 - 440.0) < 1.0 && std::abs (f1 - 493.88) < 1.5, "pitch bend = +2 semitones");
    render (p, 0.1, [] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) { m.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0); m.addEvent (juce::MidiMessage::allNotesOff (1), 1); } });
    render (p, 1.0, [] (int, int, juce::MidiBuffer&) {});

    // ---- voice stealing: 24 overlapping sine notes on 16 voices with a long release ----
    std::printf ("voice stealing\n");
    sineInit (p); setP (p, pid::ampR, 4000);
    r = render (p, 3.0, [] (int pos, int bs, juce::MidiBuffer& m)
    {
        for (int k = 0; k < 24; ++k)
        {
            const int on = 2400 + k * 2400;
            if (on >= pos && on < pos + bs) { m.addEvent (juce::MidiMessage::noteOn (1, 48 + k, (juce::uint8) 100), on - pos); m.addEvent (juce::MidiMessage::noteOff (1, 48 + k), juce::jmin (bs - 1, on - pos + 100)); }
        }
    });
    // largest sample step vs the steepest a sum of 16 sines (up to C6, amp <= 16 * 0.5 * 0.7) can rise
    float step = 0; for (size_t i = 1; i < r.L.size(); ++i) step = juce::jmax (step, std::abs (r.L[i] - r.L[i - 1]));
    const float bound = 16.0f * 0.5f * 0.8f * juce::MathConstants<float>::twoPi * 1050.0f / (float) SR;
    std::printf ("  largest sample step %.4f (smooth sines would stay below %.4f)\n", step, bound);
    check (step < bound, "stolen voices fade out instead of jumping");
    render (p, 5.0, [] (int, int, juce::MidiBuffer&) {});

    // ---- HP 24 ----
    std::printf ("filter 1 HP 24 at 1 kHz\n");
    auto hpLevel = [&] (int note)
    {
        sineInit (p); setP (p, pid::fltType, 2); setP (p, pid::fltCutoff, 1000);
        auto x = render (p, 0.6, [note] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0); });
        render (p, 0.1, [note] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOff (1, note), 0); });
        render (p, 0.5, [] (int, int, juce::MidiBuffer&) {});
        return rms (x.L, 10000, 28000);
    };
    const double lo = hpLevel (43), hi = hpLevel (103);   // ~98 Hz and ~3.1 kHz
    std::printf ("  98 Hz %.1f dB, 3.1 kHz %.1f dB\n", db (lo), db (hi));
    check (db (hi) - db (lo) > 60.0, "a decade below the cutoff is > 60 dB down (24 dB/oct)");

    // ---- noise filter (tilt) ----
    std::printf ("noise FILTER\n");
    auto noiseBright = [&] (float tilt)
    {
        p.loadPreset (0); setP (p, pid::oscOn (1), 0); setP (p, pid::oscOn (2), 0); setP (p, pid::fltCutoff, 20000);
        setP (p, pid::noiseOn, 1); setP (p, pid::noiseLevel, 0.5f); setP (p, pid::noiseTilt, tilt);
        auto x = render (p, 0.5, [] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); });
        render (p, 0.1, [] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0); });
        render (p, 0.5, [] (int, int, juce::MidiBuffer&) {});
        std::vector<float> d; for (size_t i = 1; i < x.L.size(); ++i) d.push_back (x.L[i] - x.L[i - 1]);
        return rms (d, 5000, 20000) / juce::jmax (1.0e-9, rms (x.L, 5000, 20000));   // high-frequency share
    };
    const double dark = noiseBright (-0.8f), flat = noiseBright (0.0f), thin = noiseBright (0.8f);
    std::printf ("  brightness  dark %.3f  flat %.3f  thin %.3f\n", dark, flat, thin);
    check (dark < flat * 0.5 && thin > flat * 1.05, "FILTER darkens to the left, thins to the right");

    // ---- LFO sync ----
    std::printf ("LFO sync\n");
    syn::LfoP l; l.sync = true; l.div = 2;
    check (std::abs (syn::lfoHz (l, 120.0) - 2.0f) < 1.0e-4f, "1/4 at 120 BPM = 2 Hz");
    l.div = 0; check (std::abs (syn::lfoHz (l, 120.0) - 0.5f) < 1.0e-4f, "1/1 at 120 BPM = 0.5 Hz");

    // ---- analog envelope: the decay is exponential (half way in level well before half the time) ----
    std::printf ("envelope\n");
    syn::Env e; e.setSampleRate (SR); e.setParameters (1, 1000, 0.0f, 100); e.noteOn();
    int at = 0; float lvl = 1;
    for (int i = 0; i < (int) SR; ++i) { lvl = e.getNextSample(); if (at == 0 && i > 200 && lvl < 0.5f) at = i; }
    std::printf ("  decay 1000 ms: -6 dB after %.0f ms\n", at * 1000.0 / SR);
    check (at > 0 && at * 1000.0 / SR < 250.0, "exponential decay (RC curve)");

    // ---- LEGATO: one voice follows the last key and returns to the held one ----
    std::printf ("legato\n");
    sineInit (p); setP (p, pid::voiceMode, 2);
    r = render (p, 1.5, [] (int pos, int, juce::MidiBuffer& m)
    {
        if (pos == 0)     m.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);   // A4
        if (pos == 24064) m.addEvent (juce::MidiMessage::noteOn (1, 76, (juce::uint8) 100), 0);   // E5 while A4 held
        if (pos == 48128) m.addEvent (juce::MidiMessage::noteOff (1, 76), 0);                      // back to A4
    });
    const double la1 = freqOf (r.L, 4800, 23000), la2 = freqOf (r.L, 30000, 47000), la3 = freqOf (r.L, 54000, 71000);
    std::printf ("  A4 %.1f -> E5 %.1f -> back %.1f Hz\n", la1, la2, la3);
    // one voice: the level stays that of one sine (two voices would add up)
    const double one = rms (r.L, 4800, 23000), both = rms (r.L, 30000, 47000);
    check (std::abs (la2 - 659.26) < 2.0 && std::abs (la3 - 440.0) < 1.5, "legato follows the last key and returns");
    check (std::abs (db (both) - db (one)) < 1.0, "only one voice sounds");
    render (p, 0.1, [] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); });
    setP (p, pid::voiceMode, 0);
    render (p, 1.0, [] (int, int, juce::MidiBuffer&) {});

    // ---- wavetable: WT POS morphs (Analog: sine -> square), banks differ, high notes stay clean ----
    std::printf ("wavetable\n");
    auto wtRender = [&] (int bank, float pos, int note)
    {
        sineInit (p); setP (p, pid::oscMode (1), 1); setP (p, pid::oscWtBank (1), (float) bank); setP (p, pid::oscWtPos (1), pos);
        auto x = render (p, 0.5, [note] (int ps, int, juce::MidiBuffer& m) { if (ps == 0) m.addEvent (juce::MidiMessage::noteOn (1, note, (juce::uint8) 100), 0); });
        render (p, 0.1, [note] (int ps, int, juce::MidiBuffer& m) { if (ps == 0) m.addEvent (juce::MidiMessage::noteOff (1, note), 0); });
        render (p, 0.5, [] (int, int, juce::MidiBuffer&) {});
        std::vector<float> d; for (size_t i = 1; i < x.L.size(); ++i) d.push_back (x.L[i] - x.L[i - 1]);
        return std::make_pair (rms (x.L, 4000, 20000), rms (d, 4000, 20000) / juce::jmax (1.0e-9, rms (x.L, 4000, 20000)));
    };
    const auto s0 = wtRender (0, 0.0f, 57), s5 = wtRender (0, 0.5f, 57), s1 = wtRender (0, 1.0f, 57);
    std::printf ("  Analog  pos 0 / 0.5 / 1: brightness %.3f / %.3f / %.3f, level %.1f / %.1f / %.1f dB\n",
                 s0.second, s5.second, s1.second, db (s0.first), db (s5.first), db (s1.first));
    check (s0.second < s5.second && s5.second < s1.second * 1.3, "WT POS morphs from sine to brighter waves");
    check (s0.first > 1.0e-3 && s1.first > 1.0e-3, "the wavetable oscillator sounds");
    const auto hiNote = wtRender (0, 1.0f, 108);   // C8, 4.2 kHz: only a few harmonics fit
    std::printf ("  square at C8: level %.1f dB, brightness %.3f\n", db (hiNote.first), hiNote.second);
    check (std::isfinite (hiNote.first) && hiNote.first > 1.0e-3, "high notes use a band-limited table");

    // every bank: tables finite with peak ~1, all frames sound, WT POS changes the timbre
    {
        const auto& wt = syn::wavetables();
        bool tablesOk = true;
        for (int b = 0; b < syn::Wavetables::B; ++b)
            for (int f = 0; f < syn::Wavetables::F; ++f)
            {
                const float* t = wt.table (b, f, 0); float pk = 0.0f; bool fin = true;
                for (int i = 0; i <= syn::Wavetables::N; ++i) { fin &= std::isfinite (t[i]); pk = juce::jmax (pk, std::abs (t[i])); }
                if (! fin || pk < 0.5f || pk > 2.01f) { tablesOk = false; std::printf ("  bank %d frame %d: peak %.3f\n", b, f, pk); }
            }
        check (tablesOk, "all 12 x 8 wavetable frames are finite, peak 0.5 .. 2");
        check (pid::wtBankNames().size() == syn::Wavetables::B, "bank names match the banks");

        bool allSound = true, allMorph = true;
        for (int b = 0; b < syn::Wavetables::B; ++b)
        {
            const auto a = wtRender (b, 0.0f, 57), m = wtRender (b, 0.5f, 57), z = wtRender (b, 1.0f, 57), hi = wtRender (b, 1.0f, 100);
            std::printf ("  %-9s brightness %.3f / %.3f / %.3f, level %5.1f / %5.1f / %5.1f dB, E7 %5.1f dB\n",
                         pid::wtBankNames()[b].toRawUTF8(), a.second, m.second, z.second, db (a.first), db (m.first), db (z.first), db (hi.first));
            // the three positions within 7 dB of each other; a high note may lose the harmonics
            // above Nyquist (Sync at ratio 8 has nothing below harmonic 8) but stays finite
            const double lo = juce::jmin (db (a.first), db (m.first), db (z.first)), up = juce::jmax (db (a.first), db (m.first), db (z.first));
            // (a 4 % pulse is ~6 dB quieter by nature: more gain would push its peak over 2)
            allSound &= lo > -30.0 && up < -6.0 && up - lo < 7.0 && std::isfinite (hi.first);
            const double spread = juce::jmax (a.second, m.second, z.second) / juce::jmax (1.0e-9, juce::jmin (a.second, m.second, z.second));
            allMorph &= spread > 1.05;
        }
        check (allSound, "every bank sounds at an even level across WT POS");
        check (allMorph, "WT POS changes the timbre in every bank");
    }

    // ---- CPU: 16 voices, 2 oscillators x 7 unison, both filters, 10 s ----
    std::printf ("CPU\n");
    p.loadPreset (0);
    setP (p, pid::oscUni (1), 7); setP (p, pid::oscUni (2), 7); setP (p, pid::fltCutoff, 3000); setP (p, pid::fltRes, 0.4f);
    setP (p, pid::fltDrive, 0.3f); setP (p, pid::flt2Type, 0); setP (p, pid::flt2Cutoff, 6000);
    setP (p, pid::modSrc (0), 1); setP (p, pid::modDst (0), 1); setP (p, pid::modAmt (0), 0.3f);
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    render (p, 10.0, [] (int pos, int, juce::MidiBuffer& m) { if (pos == 0) for (int k = 0; k < 16; ++k) m.addEvent (juce::MidiMessage::noteOn (1, 40 + k * 2, (juce::uint8) 100), 0); });
    const double ms = juce::Time::getMillisecondCounterHiRes() - t0;
    std::printf ("  10 s of audio in %.0f ms  ->  %.1f %% of one core\n", ms, ms / 100.0);

    tmp.deleteRecursively();
    std::printf (fails ? "%d FAILED\n" : "all checks passed\n", fails);
    return fails ? 1 : 0;
}
