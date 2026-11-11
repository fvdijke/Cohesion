// Offline checks for the COHESION - SYNTH arpeggiator (not part of the plugin).
#include <juce_audio_basics/juce_audio_basics.h>
#include "../Source/ArpEngine.h"
#include <cstdio>
#include <vector>

static const double SR = 48000.0;
static int failures = 0;
static void check (bool ok, const char* what, const juce::String& got)
{
    std::printf ("  [%s] %-52s %s\n", ok ? " OK " : "FAIL", what, got.toRawUTF8());
    if (! ok) ++failures;
}

struct Ev { long long t; int note; bool on; };

// Runs the engine for `seconds`; `input` = (sample, note, on) pairs. Free-running unless ppq0 >= 0.
static std::vector<Ev> run (arp::ArpParams p, std::vector<Ev> input, double seconds, double bpm = 120.0, double ppq0 = -1.0)
{
    arp::Engine e; e.prepare (SR);
    std::vector<Ev> out;
    const int blk = 256;
    const long long total = (long long) (seconds * SR);
    for (long long s = 0; s < total; s += blk)
    {
        juce::MidiBuffer mb;
        for (auto& in : input)
            if (in.t >= s && in.t < s + blk)
                mb.addEvent (in.on ? juce::MidiMessage::noteOn (1, in.note, 0.8f) : juce::MidiMessage::noteOff (1, in.note), (int) (in.t - s));
        std::optional<double> ppq;
        if (ppq0 >= 0.0) ppq = ppq0 + (double) s * bpm / 60.0 / SR;
        e.process (mb, blk, p, bpm, ppq0 >= 0.0, ppq);
        for (const auto meta : mb)
        {
            const auto m = meta.getMessage();
            if (m.isNoteOnOrOff()) out.push_back ({ s + meta.samplePosition, m.getNoteNumber(), m.isNoteOn() });
        }
    }
    return out;
}

static juce::String onNotes (const std::vector<Ev>& ev, int max = 12)
{
    juce::String s; int c = 0;
    for (auto& e : ev) if (e.on && c++ < max) s << e.note << " ";
    return s.trim();
}

int main()
{
    const std::vector<Ev> chordCEG = { { 0, 60, true }, { 0, 64, true }, { 0, 67, true } };
    const double stepSamples = 0.25 * 60.0 / 120.0 * SR;   // 1/16 at 120 BPM = 6000

    std::printf ("1. Modes (C-E-G, 1/16 @ 120 BPM, free running)\n");
    {
        arp::ArpParams p; p.on = true; p.sync = false; p.tempo = 120;
        p.mode = arp::Up;     check (onNotes (run (p, chordCEG, 1.0), 7) == "60 64 67 60 64 67 60", "Up",        onNotes (run (p, chordCEG, 1.0), 7));
        p.mode = arp::Down;   check (onNotes (run (p, chordCEG, 1.0), 7) == "67 64 60 67 64 60 67", "Down",      onNotes (run (p, chordCEG, 1.0), 7));
        p.mode = arp::UpDown; check (onNotes (run (p, chordCEG, 1.0), 7) == "60 64 67 64 60 64 67", "Up / Down", onNotes (run (p, chordCEG, 1.0), 7));
        p.mode = arp::Up; p.octaves = 2;
        check (onNotes (run (p, chordCEG, 1.0), 7) == "60 64 67 72 76 79 60", "Up, 2 octaves", onNotes (run (p, chordCEG, 1.0), 7));
        p.octaves = 1; p.mode = arp::Chord;
        auto c = run (p, chordCEG, 0.2);
        check (onNotes (c, 3) == "60 64 67" && c[0].t == c[1].t && c[1].t == c[2].t, "Chord (3 notes at once)", onNotes (c, 3));
    }

    std::printf ("2. Timing\n");
    {
        arp::ArpParams p; p.on = true; p.sync = false; p.tempo = 120; p.gate = 0.7f;
        auto ev = run (p, { { 1000, 60, true } }, 1.0);
        std::vector<long long> ons, offs;
        for (auto& e : ev) (e.on ? ons : offs).push_back (e.t);
        check (ons.size() > 3 && ons[0] == 1000, "first note immediately at key press", juce::String (ons.empty() ? -1 : ons[0]));
        check (ons.size() > 3 && std::abs ((double) (ons[1] - ons[0]) - stepSamples) <= 1.0, "step length 6000 samples", juce::String (ons[1] - ons[0]));
        check (! offs.empty() && std::abs ((double) (offs[0] - ons[0]) - 0.7 * stepSamples) <= 1.0, "gate 70% -> off after 4200", juce::String (offs[0] - ons[0]));

        p.swing = 0.5f;
        auto sw = run (p, { { 0, 60, true } }, 1.0);
        std::vector<long long> so; for (auto& e : sw) if (e.on) so.push_back (e.t);
        const double expect = stepSamples * (1.0 + 0.5 * 0.5);
        check (so.size() > 2 && std::abs ((double) (so[1] - so[0]) - expect) <= 1.0, "swing 50% delays the off-beat step", juce::String (so[1] - so[0]));
    }

    std::printf ("3. Host grid (transport playing, ppq starts at 0.1)\n");
    {
        arp::ArpParams p; p.on = true; p.sync = true;
        auto ev = run (p, { { 0, 60, true } }, 0.5, 120.0, 0.1);
        long long first = -1; for (auto& e : ev) if (e.on) { first = e.t; break; }
        const long long expect = (long long) std::llround ((0.25 - 0.1) * 60.0 / 120.0 * SR);   // next 1/16 boundary
        check (std::abs (first - expect) <= 1, "first note waits for the next 1/16 line", juce::String (first) + " (expected " + juce::String (expect) + ")");
    }

    std::printf ("4. Steps\n");
    {
        arp::ArpParams p; p.on = true; p.sync = false; p.tempo = 120; p.steps = 4;
        p.stepOn[1] = false;                         // pattern x . x x
        auto ev = run (p, { { 0, 60, true } }, 1.0);
        std::vector<long long> ons; for (auto& e : ev) if (e.on) ons.push_back (e.t);
        const bool ok = ons.size() >= 4 && std::llabs (ons[1] - (long long) (2 * stepSamples)) <= 1 && std::llabs (ons[3] - (long long) (4 * stepSamples)) <= 1;
        check (ok, "step 2 off, 4-step loop", juce::String (ons.size() > 3 ? ons[1] : -1) + ", " + juce::String (ons.size() > 3 ? ons[3] : -1));
    }

    std::printf ("5. Release / latch\n");
    {
        arp::ArpParams p; p.on = true; p.sync = false; p.tempo = 120;
        auto ev = run (p, { { 0, 60, true }, { 20000, 60, false } }, 1.0);
        long long lastOn = 0; for (auto& e : ev) if (e.on) lastOn = e.t;
        check (lastOn < 20000, "stops after key release", juce::String (lastOn));
        p.latch = true;
        auto lv = run (p, { { 0, 60, true }, { 20000, 60, false }, { 40000, 72, true }, { 41000, 72, false } }, 1.0);
        long long lastLatched = 0; bool sawNewOnly = true;
        for (auto& e : lv) if (e.on) { lastLatched = e.t; if (e.t > 41000 && e.note != 72) sawNewOnly = false; }
        check (lastLatched > 40000 && sawNewOnly, "latch keeps playing; a new chord replaces it", juce::String (lastLatched));
    }

    std::printf ("6. Note-off balance\n");
    {
        arp::ArpParams p; p.on = true; p.sync = false; p.tempo = 140; p.gate = 1.0f; p.humanize = 1.0f; p.octaves = 3; p.mode = arp::Random;
        auto ev = run (p, { { 0, 60, true }, { 0, 63, true }, { 30000, 60, false }, { 30000, 63, false } }, 2.0);
        int bal[128] = {}; bool neg = false;
        for (auto& e : ev) { bal[e.note] += e.on ? 1 : -1; if (bal[e.note] < 0 || bal[e.note] > 1) neg = true; }
        int open = 0; for (int i = 0; i < 128; ++i) open += bal[i];
        check (! neg && open == 0, "every note-on gets exactly one note-off", juce::String (open) + " open");
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "ALL PASSED" : "FAILURES", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
