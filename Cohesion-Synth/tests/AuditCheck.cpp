// Quality audit: measures suspected weak spots of the DSP models and the processor logic.
//   AuditCheck <dx7 bank.syx>
#include <juce_audio_utils/juce_audio_utils.h>
#include "../Source/PluginProcessor.h"
#include <cstdio>
#include <complex>

static const double SR = 48000.0;
static double db (double x) { return 20.0 * std::log10 (juce::jmax (1.0e-9, x)); }
static void setP (CohesionSynthAudioProcessor& p, const juce::String& id, float v)
{ auto* prm = p.apvts.getParameter (id); prm->setValueNotifyingHost (prm->convertTo0to1 (v)); }

struct Rec { std::vector<float> L, R; };
template <typename Ev>
static Rec render (CohesionSynthAudioProcessor& p, double sec, Ev events)
{
    const int bs = 256; juce::AudioBuffer<float> b (2, bs); Rec r;
    for (int pos = 0; pos < (int) (sec * SR); pos += bs)
    {
        juce::MidiBuffer m; events (pos, m);
        p.processBlock (b, m);
        for (int i = 0; i < bs; ++i) { r.L.push_back (b.getSample (0, i)); r.R.push_back (b.getSample (1, i)); }
    }
    return r;
}
static double rms (const std::vector<float>& x, size_t a, size_t b)
{ double s = 0; for (size_t i = a; i < b && i < x.size(); ++i) s += (double) x[i] * x[i]; return std::sqrt (s / (double) juce::jmax<size_t> (1, b - a)); }
static double freqOf (const std::vector<float>& x, size_t a, size_t b)
{
    int up = 0; size_t first = 0, last = 0;
    for (size_t i = a + 1; i < b && i < x.size(); ++i) if (x[i - 1] < 0.0f && x[i] >= 0.0f) { if (up == 0) first = i; last = i; ++up; }
    return up > 1 ? (up - 1) * SR / (double) (last - first) : 0.0;
}
// magnitude response of a filter process() at f (steady-state sine)
template <typename F> static double gainAt (F& flt, double f)
{
    double peak = 0;
    for (int i = 0; i < 48000; ++i)
    {
        const float y = flt.process ((float) (0.01 * std::sin (2.0 * juce::MathConstants<double>::pi * f * i / SR)));
        if (i > 36000) peak = std::max (peak, (double) std::abs (y));
    }
    return peak / 0.01;
}

static int fails = 0;
static void expect (bool ok, const char* what)
{
    if (! ok) { std::printf ("   FAIL: %s\n", what); ++fails; }
}
static int finish()
{
    std::printf (fails == 0 ? "\nAuditCheck: all checks passed\n" : "\nAuditCheck: %d check(s) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    auto tmp = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("cohesion_audit");
    tmp.deleteRecursively(); tmp.createDirectory();
    _putenv_s ("COHESION_SYNTH_SYSEX_DIR", tmp.getChildFile ("sysex").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_PRESET_DIR", tmp.getChildFile ("presets").getFullPathName().toRawUTF8());
    _putenv_s ("COHESION_SYNTH_ROM_DIR",   tmp.getChildFile ("roms").getFullPathName().toRawUTF8());

    // ---- A. triangle oscillator (integrated square): start-up DC and low-pitch shape ----
    for (double hz : { 30.0, 110.0, 880.0 })
    {
        syn::Core c; c.setInc (hz / SR); c.setPhase01 (0.3);
        std::vector<float> y; for (int i = 0; i < (int) SR; ++i) y.push_back (c.next (2));
        const int per = (int) (SR / hz);
        double dc0 = 0; for (int i = 0; i < per; ++i) dc0 += y[(size_t) i]; dc0 /= per;
        double dc1 = 0, pk = 0; for (int i = (int) SR - per; i < (int) SR; ++i) { dc1 += y[(size_t) i]; pk = std::max (pk, (double) std::abs (y[(size_t) i])); } dc1 /= per;
        std::printf ("A. triangle %4.0f Hz: DC first cycle %+.3f, last cycle %+.3f, peak %.3f, rms %.3f (ideal 0.577)\n",
                     hz, dc0, dc1, pk, rms (y, (size_t) (SR - per), (size_t) SR));
        expect (std::abs (dc0) < 0.05 && std::abs (dc1) < 0.01 && pk > 0.95, "triangle starts without DC and keeps its full shape");
    }

    // ---- B. ladder resonance tuning (LP 24, res 0.95) ----
    double ladMin = 999, ladMax = -999;
    for (float fc : { 1000.0f, 4000.0f, 8000.0f, 12000.0f })
    {
        double best = 0, bestF = 0;
        for (double f = fc * 0.5; f < fc * 1.6 && f < 21000; f *= 1.01)
        {
            syn::Ladder l; l.setSR (SR); l.setParams (fc, 0.95f, 0.0f, 0);
            const double g = gainAt (l, f);
            if (g > best) { best = g; bestF = f; }
        }
        std::printf ("B. ladder cutoff %5.0f Hz: resonance peak at %6.0f Hz (%.2f x), %+.1f dB\n", fc, bestF, bestF / fc, db (best));
        expect (bestF / fc > 0.95 && bestF / fc < 1.03, "ladder resonance sits on the cutoff");
        ladMin = std::min (ladMin, db (best)); ladMax = std::max (ladMax, db (best));
    }
    expect (ladMax - ladMin < 3.0, "ladder resonance peak is equally high at every cutoff");

    // ---- C. SVF resonance: 12 vs 24 dB peak gain, and DRIVE ----
    double svfPeak[2] {};
    for (int slope : { 0, 1 })
    {
        double best = 0;
        for (double f = 500; f < 2000; f *= 1.005)
        {
            syn::SVF s; s.setSR (SR); s.setParams (1000.0f, 1.0f, 0.0f, 0, slope);
            best = std::max (best, gainAt (s, f));
        }
        std::printf ("C. SVF LP %s at res 100 %%: peak gain %+.1f dB\n", slope ? "24" : "12", db (best));
        svfPeak[slope] = db (best);
    }
    expect (std::abs (svfPeak[1] - svfPeak[0]) < 2.0, "SVF 24 dB resonance peak matches 12 dB");
    {
        syn::SVF a, b; a.setSR (SR); b.setSR (SR);
        a.setParams (2000.0f, 0.3f, 0.0f, 0, 1); b.setParams (2000.0f, 0.3f, 1.0f, 0, 1);
        double diff = 0;
        for (int i = 0; i < 4800; ++i) { const float x = 0.8f * (float) std::sin (2 * juce::MathConstants<double>::pi * 200 * i / SR); diff = std::max (diff, (double) std::abs (a.process (x) - b.process (x))); }
        std::printf ("C. SVF DRIVE 0 vs 100 %%: largest output difference %.2e\n", diff);
        expect (diff > 0.05, "SVF DRIVE changes the sound");
    }

    // ---- D. LFO SMOOTH on a square ----
    {
        syn::LfoGen l; l.setRate (1.0f, SR); l.reset (0.0);
        int t10 = -1, t90 = -1;
        for (int i = 0; i < (int) SR; ++i)
        {
            const float v = l.next (3, 1.0f);
            if (i > (int) (SR * 0.5) - 10)
            {   // the edge at 0.5 s: +1 -> -1
                if (t10 < 0 && v < 0.8f) t10 = i;
                if (t90 < 0 && v < -0.8f) t90 = i;
            }
        }
        std::printf ("D. LFO square 1 Hz, SMOOTH 100 %%: edge 90 %% -> 10 %% takes %.2f ms\n", (t90 - t10) * 1000.0 / SR);
        expect ((t90 - t10) * 1000.0 / SR > 100.0, "LFO SMOOTH rounds the edge relative to the LFO period");
    }

    // ---- E. echo input saturation (feedback 0, 100 % wet) ----
    {
        fx::Echo e; e.prepare (SR); fx::FxParams p; p.ecOn = true; p.ecMix = 1.0f; p.ecFb = 0.0f; p.ecTime = 10.0f; p.ecWidth = 1.0f; p.ecDamp = 0.0f;
        std::vector<float> L ((size_t) SR / 4), R ((size_t) SR / 4);
        for (size_t i = 0; i < L.size(); ++i) L[i] = R[i] = 0.7f * (float) std::sin (2 * juce::MathConstants<double>::pi * 220 * (double) i / SR);
        e.process (L.data(), R.data(), (int) L.size(), p);
        double pk = 0; for (size_t i = L.size() / 2; i < L.size(); ++i) pk = std::max (pk, (double) std::abs (L[i]));
        std::printf ("E. echo of a 0.70 sine (no feedback): repeat peak %.3f (%+.1f dB)\n", pk, db (pk / 0.7));
        expect (std::abs (db (pk / 0.7)) < 0.2, "echo input stays clean without feedback");
    }

    CohesionSynthAudioProcessor p;
    p.setPlayConfigDetails (0, 2, SR, 256);
    p.prepareToPlay (SR, 256);

    // ---- F. header name after a project is reopened ----
    {
        p.loadPreset (5);
        const auto before = p.headerName();
        juce::MemoryBlock mb; p.getStateInformation (mb);
        CohesionSynthAudioProcessor q; q.setStateInformation (mb.getData(), (int) mb.getSize());
        std::printf ("F. header before save '%s', after reopening '%s'\n", before.toRawUTF8(), q.headerName().toRawUTF8());
        expect (before == q.headerName(), "header name survives reopening the project");
    }

    // ---- G. pitch bend / mod wheel reach the ARP's voices ----
    {
        p.loadPreset (0);
        setP (p, pid::oscWave (1), 3); setP (p, pid::oscOn (2), 0); setP (p, pid::fltCutoff, 20000); setP (p, pid::ampR, 5);
        setP (p, pid::arpOn, 1); setP (p, pid::arpSync, 0); setP (p, pid::arpRate, 0); setP (p, pid::arpGate, 1.0f); setP (p, pid::arpTempo, 60);
        const auto r = render (p, 2.0, [] (int pos, juce::MidiBuffer& m)
        {
            if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 69, (juce::uint8) 100), 0);
            if (pos == 256 * 188) m.addEvent (juce::MidiMessage::pitchWheel (1, 16383), 0);
        });
        const double f0 = freqOf (r.L, 4800, 40000), f1 = freqOf (r.L, 60000, 90000);
        std::printf ("G. ARP voice: %.1f Hz before pitch bend, %.1f Hz after full bend up (expect 493.9)\n", f0, f1);
        expect (std::abs (f0 - 440.0) < 3.0 && std::abs (f1 - 493.9) < 3.0, "pitch bend reaches the ARP voices");
        setP (p, pid::arpOn, 0);
        render (p, 0.5, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) { m.addEvent (juce::MidiMessage::allNotesOff (1), 0); m.addEvent (juce::MidiMessage::pitchWheel (1, 8192), 0); } });
    }

    // ---- H. voices taken by notes that no part plays (split, 16 voices) ----
    {
        p.loadPreset (0);
        setP (p, pid::ampA, 1); setP (p, pid::ampS, 1); setP (p, pid::ampR, 20);
        setP (p, pid::limOn, 0); setP (p, pid::oscLevel (1), 0.1f); setP (p, pid::oscOn (2), 0); setP (p, pid::oscWave (1), 3);
        setP (p, pid::partKeyHi (pid::kVA), 59);
        const auto r = render (p, 2.0, [] (int pos, juce::MidiBuffer& m)
        {
            if (pos == 0) for (int k = 0; k < 16; ++k) m.addEvent (juce::MidiMessage::noteOn (1, 36 + k, (juce::uint8) 100), 0);
            if (pos == 256 * 188) for (int k = 0; k < 12; ++k) m.addEvent (juce::MidiMessage::noteOn (1, 72 + k, (juce::uint8) 100), 0);
        });
        const double h0 = db (rms (r.L, 20000, 44000)), h1 = db (rms (r.L, 60000, 90000));
        std::printf ("H. 16 held low notes: %.1f dB; after 12 keys outside every part's range: %.1f dB\n", h0, h1);
        expect (std::abs (h1 - h0) < 1.0, "keys outside every part's range take no voices");
        setP (p, pid::partKeyHi (pid::kVA), 127);
        render (p, 0.5, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); });
    }

    if (argc < 2) { std::printf ("(no DX7 bank given: FM checks skipped)\n"); return finish(); }
    juce::Array<juce::File> items; items.add (juce::File (juce::String (argv[1])));
    const auto imp = p.importSysEx (items);
    int dx = -1; for (int i = 0; i < p.numPresets(); ++i) if (p.presetCategory (i) == imp.firstCategory) { dx = i; break; }

    // ---- I. the VA part's modulation matrix (LFO -> pan) reaches the FM part ----
    {
        p.loadPreset (0);
        p.loadPresetIntoPart (dx, pid::kFM);
        setP (p, pid::partSolo (pid::kFM), 1);
        setP (p, pid::lfoRate (1), 2.0f); setP (p, pid::modSrc (0), 1); setP (p, pid::modDst (0), 5); setP (p, pid::modAmt (0), 1.0f);
        auto swing = [&p]
        {
            const auto r = render (p, 1.5, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, (juce::uint8) 100), 0); });
            double mn = 99, mx = -99;
            for (size_t a = 4800; a + 2400 < 60000; a += 2400) { const double d = db (rms (r.L, a, a + 2400)) - db (rms (r.R, a, a + 2400)); mn = std::min (mn, d); mx = std::max (mx, d); }
            render (p, 1.5, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); });
            return std::make_pair (mn, mx);
        };
        const auto off = swing();
        std::printf ("I. FM part (solo) with the VA part's LFO 1 -> Pan: L-R balance swings %+.1f .. %+.1f dB\n", off.first, off.second);
        expect (off.second - off.first < 1.0, "the VA part's modulation stays out of the FM part");
        setP (p, pid::partFilter (pid::kFM), 1);
        const auto on = swing();
        std::printf ("I. ... with VA FILT+MOD on: swings %+.1f .. %+.1f dB\n", on.first, on.second);
        expect (on.second - on.first > 6.0, "VA FILT+MOD lets the VA modulation into the FM part");
        setP (p, pid::partFilter (pid::kFM), 0);
        setP (p, pid::modDst (0), 0); setP (p, pid::partSolo (pid::kFM), 0);
        render (p, 0.5, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::allNotesOff (1), 0); });
    }

    // ---- J. VELOCITY (VA) also scales the FM part, on top of the DX7's own velocity ----
    {
        p.loadPreset (dx);
        auto lvl = [&] (float sens)
        {
            setP (p, pid::velSens, sens);
            const auto r = render (p, 1.0, [] (int pos, juce::MidiBuffer& m) { if (pos == 0) m.addEvent (juce::MidiMessage::noteOn (1, 60, 0.3f), 0);
                                                                                   if (pos == 256 * 150) m.addEvent (juce::MidiMessage::noteOff (1, 60), 0); });
            render (p, 1.5, [] (int, juce::MidiBuffer&) {});
            return db (rms (r.L, 2400, 30000));
        };
        const double a = lvl (0.0f), b = lvl (1.0f);
        std::printf ("J. DX7 voice at velocity 0.3: VELOCITY 0 %% %.1f dB, VELOCITY 100 %% %.1f dB (%.1f dB extra)\n", a, b, b - a);
        expect (std::abs (b - a) < 0.5, "VELOCITY (VA) does not scale the FM part a second time");
    }
    tmp.deleteRecursively();
    return finish();
}
