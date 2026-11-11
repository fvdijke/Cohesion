#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <atomic>
#include <algorithm>
#include <optional>
#include <cstdint>

// ===========================================================================
//  COHESION - SYNTH  arpeggiator + 16-step velocity sequencer.
//  Consumes incoming note events and generates its own (sample accurate),
//  locked to the host grid when the transport runs, free-running otherwise.
// ===========================================================================
namespace arp
{
    enum Mode  { Up = 0, Down, UpDown, DownUp, Random, Chord };
    enum Order { AsPlayed = 0, LowHigh, HighLow };
    static constexpr int kSteps = 16;

    struct ArpParams
    {
        bool  on = false; int mode = Up; int rate = 3; int octaves = 1;
        float gate = 0.7f, swing = 0.0f, random = 0.0f;
        bool  sync = true, hold = false, latch = false; float tempo = 120.0f; int steps = 16;
        bool  stepOn[kSteps]; float stepVel[kSteps];
        int   order = LowHigh; float velAmt = 1.0f, slide = 0.0f, humanize = 0.0f;
        ArpParams() { for (int i = 0; i < kSteps; ++i) { stepOn[i] = true; stepVel[i] = 1.0f; } }
    };

    // step length in beats: 1/4, 1/8, 1/8 T, 1/16, 1/16 T, 1/32
    inline double stepBeats (int rate)
    {
        static const double t[] = { 1.0, 0.5, 1.0 / 3.0, 0.25, 1.0 / 6.0, 0.125 };
        return t[juce::jlimit (0, 5, rate)];
    }

    class Engine
    {
    public:
        std::atomic<int>      uiStep { -1 };
        std::atomic<uint32_t> heldMask[4] {};
        std::atomic<double>   uiBpm { 120.0 };

        void prepare (double sampleRate) { sr = sampleRate; held.reserve (128); down.reserve (128); pending.reserve (512); notes.reserve (256); reset(); }

        void reset()
        {
            held.clear(); down.clear(); pending.clear();
            now = 0; locked = false; k = 0; nextBeat = 0; freeBeat = 0; seqCounter = 0; playCounter = 0;
            lastL = 0; wasOn = false; lastHold = lastLatch = false;
            uiStep.store (-1); publishHeld();
        }

        void process (juce::MidiBuffer& midi, int n, const ArpParams& p, double hostBpm, bool hostPlaying, std::optional<double> ppqStart)
        {
            juce::MidiBuffer out;

            // ---------------- arp switched off: pass everything through ----------------
            if (! p.on)
            {
                if (wasOn) flushAll (out);
                wasOn = false;
                for (const auto meta : midi)
                {
                    const auto m = meta.getMessage();
                    if (m.isNoteOn())       { addDown (m.getNoteNumber()); addHeld (m.getNoteNumber(), m.getFloatVelocity()); }
                    else if (m.isNoteOff()) { removeDown (m.getNoteNumber()); removeHeld (m.getNoteNumber()); }
                    out.addEvent (m, meta.samplePosition);
                }
                midi.swapWith (out);
                now += n; uiStep.store (-1); publishHeld();
                return;
            }

            if (! wasOn) { locked = false; seqCounter = 0; }   // (re)start cleanly
            wasOn = true;

            // releasing HOLD / LATCH drops notes that are no longer pressed
            if ((lastHold && ! p.hold) || (lastLatch && ! p.latch))
                if (! p.hold && ! p.latch) pruneToDown();
            lastHold = p.hold; lastLatch = p.latch;

            const double bpm = p.sync ? juce::jlimit (20.0, 400.0, hostBpm) : juce::jlimit (20.0, 400.0, (double) p.tempo);
            uiBpm.store (bpm);
            const double bps = bpm / 60.0 / sr;           // beats per sample
            const double L = stepBeats (p.rate);
            const bool useGrid = p.sync && hostPlaying && ppqStart.has_value();

            // ---------------- clock (re)alignment ----------------
            if (useGrid)
            {
                const double ppq = *ppqStart;
                if (! locked || std::abs (ppq - expectedPpq) > 1.0e-3 || L != lastL)
                {
                    k = (int64_t) std::floor (ppq / L);
                    if (stepTime (k, L, p.swing) < ppq - 1.0e-9) ++k;
                    nextBeat = stepTime (k, L, p.swing);
                    locked = true;
                }
                expectedPpq = ppq + n * bps;
            }
            else
            {
                if (locked || L != lastL)
                {
                    k = (int64_t) std::ceil (freeBeat / L - 1.0e-9);
                    nextBeat = stepTime (k, L, p.swing);
                    locked = false;
                }
            }
            lastL = L;

            // ---------------- incoming events, sample by sample ----------------
            notes.clear();
            for (const auto meta : midi)
            {
                const auto m = meta.getMessage();
                if (m.isNoteOnOrOff()) notes.push_back ({ meta.samplePosition, m });
                else out.addEvent (m, meta.samplePosition);   // CC, pitch bend, ... pass through
            }

            size_t ni = 0;
            for (int i = 0; i < n; ++i)
            {
                while (ni < notes.size() && notes[ni].pos <= i)
                {
                    const auto& m = notes[ni].m;
                    const bool wasEmpty = held.empty();
                    if (m.isNoteOn())
                    {
                        if (p.latch && ! p.hold && down.empty()) held.clear();   // fresh chord replaces latched one
                        addDown (m.getNoteNumber());
                        addHeld (m.getNoteNumber(), m.getFloatVelocity());
                        if (wasEmpty)
                        {
                            seqCounter = 0;
                            if (! useGrid) { freeBeat = 0.0; k = 0; nextBeat = 0.0; }   // play the first note immediately
                        }
                    }
                    else
                    {
                        removeDown (m.getNoteNumber());
                        if (! p.hold && ! p.latch) removeHeld (m.getNoteNumber());
                    }
                    ++ni;
                }

                const double beat = useGrid ? *ppqStart + i * bps : freeBeat;
                int guard = 0;
                while (beat >= nextBeat && guard++ < 8)
                {
                    const double len = stepTime (k + 1, L, p.swing) - stepTime (k, L, p.swing);
                    fireStep (i, p, len / bps);
                    ++k;
                    nextBeat = stepTime (k, L, p.swing);
                }
                if (! useGrid) freeBeat += bps;
            }
            if (! useGrid && freeBeat > 1.0e6) { const double sh = std::floor (freeBeat / 64.0) * 64.0; freeBeat -= sh; nextBeat -= sh; k -= (int64_t) std::llround (sh / L); }

            // ---------------- emit scheduled events in this block ----------------
            std::stable_sort (pending.begin(), pending.end(), [] (const Ev& a, const Ev& b)
                              { return a.t != b.t ? a.t < b.t : (a.on ? 1 : 0) < (b.on ? 1 : 0); });
            const int64_t end = now + n;
            size_t keep = 0;
            for (size_t e = 0; e < pending.size(); ++e)
            {
                auto& ev = pending[e];
                if (ev.t < end)
                {
                    const int pos = (int) juce::jlimit<int64_t> (0, n - 1, ev.t - now);
                    if (ev.on) out.addEvent (juce::MidiMessage::noteOn (1, ev.note, juce::jlimit (1.0f / 127.0f, 1.0f, ev.vel)), pos);
                    else       out.addEvent (juce::MidiMessage::noteOff (1, ev.note), pos);
                }
                else pending[keep++] = ev;
            }
            pending.resize (keep);

            midi.swapWith (out);
            now = end;
            if (held.empty()) uiStep.store (-1);
            publishHeld();
        }

    private:
        struct Held { int note; float vel; int64_t order; };
        struct In   { int pos; juce::MidiMessage m; };
        std::vector<In> notes;
        struct Ev   { int64_t t; int note; float vel; bool on; };

        static double stepTime (int64_t idx, double L, float swing)
        {
            return (double) idx * L + ((idx & 1) ? (double) juce::jlimit (0.0f, 0.75f, swing) * L * 0.5 : 0.0);
        }

        void fireStep (int i, const ArpParams& p, double stepSamples)
        {
            const int steps = juce::jlimit (1, kSteps, p.steps);
            const int s = (int) (((k % steps) + steps) % steps);
            if (held.empty()) return;
            uiStep.store (s);
            if (! p.stepOn[s]) return;

            // ordered base list
            std::vector<Held> base (held.begin(), held.end());
            if (p.order == LowHigh)      std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.note < b.note; });
            else if (p.order == HighLow) std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.note > b.note; });
            else                         std::sort (base.begin(), base.end(), [] (const Held& a, const Held& b) { return a.order < b.order; });

            const int oct = juce::jlimit (1, 4, p.octaves);
            std::vector<Held> chosen;
            if (p.mode == Chord)
            {
                const int o = (int) (seqCounter % oct);
                for (auto h : base) { h.note += 12 * o; chosen.push_back (h); }
            }
            else
            {
                std::vector<Held> ex;
                for (int o = 0; o < oct; ++o) for (auto h : base) { h.note += 12 * o; ex.push_back (h); }
                const int M = (int) ex.size();
                int idx = 0;
                if (p.mode == Random || rng.nextFloat() < p.random) idx = rng.nextInt (M);
                else
                {
                    const int64_t c = seqCounter;
                    switch (p.mode)
                    {
                        case Down:   idx = M - 1 - (int) (c % M); break;
                        case UpDown: { const int len = juce::jmax (1, 2 * M - 2); const int q = (int) (c % len); idx = M == 1 ? 0 : (q < M ? q : 2 * M - 2 - q); break; }
                        case DownUp: { const int len = juce::jmax (1, 2 * M - 2); const int q = (int) (c % len); idx = M == 1 ? 0 : M - 1 - (q < M ? q : 2 * M - 2 - q); break; }
                        default:     idx = (int) (c % M); break;
                    }
                }
                chosen.push_back (ex[(size_t) juce::jlimit (0, M - 1, idx)]);
            }
            ++seqCounter;

            const int64_t tOn = now + i + (int64_t) (rng.nextFloat() * juce::jlimit (0.0f, 1.0f, p.humanize) * 0.02f * (float) sr);
            const int64_t gateLen = juce::jmax ((int64_t) (sr * 0.003), (int64_t) (juce::jlimit (0.05f, 1.0f, p.gate) * stepSamples));
            const float stepVel = p.stepVel[s];
            for (auto& h : chosen)
            {
                const int note = juce::jlimit (0, 127, h.note);
                for (auto& ev : pending)          // a still-sounding copy of this note ends right before the new one
                    if (! ev.on && ev.note == note && ev.t > tOn) ev.t = tOn;
                const float vel = h.vel * ((1.0f - p.velAmt) + p.velAmt * stepVel);
                pending.push_back ({ tOn, note, vel, true });
                pending.push_back ({ tOn + gateLen, note, 0.0f, false });
            }
        }

        void flushAll (juce::MidiBuffer& out)
        {
            for (auto& ev : pending) if (! ev.on) out.addEvent (juce::MidiMessage::noteOff (1, ev.note), 0);
            pending.clear();
        }

        void addDown (int n)    { if (std::find (down.begin(), down.end(), n) == down.end()) down.push_back (n); }
        void removeDown (int n) { down.erase (std::remove (down.begin(), down.end(), n), down.end()); }
        void addHeld (int n, float v)
        {
            for (auto& h : held) if (h.note == n) { h.vel = v; return; }
            held.push_back ({ n, v, playCounter++ });
        }
        void removeHeld (int n) { held.erase (std::remove_if (held.begin(), held.end(), [n] (const Held& h) { return h.note == n; }), held.end()); }
        void pruneToDown()
        {
            held.erase (std::remove_if (held.begin(), held.end(), [this] (const Held& h)
                        { return std::find (down.begin(), down.end(), h.note) == down.end(); }), held.end());
        }
        void publishHeld()
        {
            uint32_t m[4] = { 0, 0, 0, 0 };
            for (auto& h : held) m[h.note >> 5] |= (1u << (h.note & 31));
            for (int i = 0; i < 4; ++i) heldMask[i].store (m[i]);
        }

        double sr = 44100.0;
        std::vector<Held> held; std::vector<int> down; std::vector<Ev> pending;
        int64_t now = 0, k = 0, seqCounter = 0, playCounter = 0;
        double nextBeat = 0, freeBeat = 0, expectedPpq = 0, lastL = 0;
        bool locked = false, wasOn = false, lastHold = false, lastLatch = false;
        juce::Random rng;
    };
}
