#pragma once

#include "PresetGen.h"

// ===========================================================================
//  General MIDI bank: the 128 GM programs as COHESION presets, grouped in the
//  16 GM families ("GM · Piano" ... "GM · Sound FX"). Names carry the GM number
//  (001-128) so MIDI files map one to one; a MIDI program change loads the
//  matching preset. All sounds use the VA engine (oscillators, FM 2>1, ring,
//  filters, envelopes, LFOs, FX); levels are matched by tests/PresetRender.
//  One patch plays all keys, so the GM drum kits (channel 10) are not covered.
// ===========================================================================
namespace gm
{
    using pgen::G;
    using namespace pgen;   // Saw / Square / Tri / Sine, LP / HP / BP, sources and destinations

    // ------------------------------------------------------------ building blocks
    inline void velBright (G& g, float amt = 0.3f) { g.mod (sVel, dF1, amt); }
    inline void trem (G& g, float rate, float depth) { g.lfo (3, 0, rate); g.mod (sLFO3, dAmp, -depth); }

    // decaying, struck/plucked tone: cutoff falls with ENV 2, the amp decays to `sus`
    inline void struck (G& g, float cut, float envAmt, float fltDecay, float ampDecay, float sus, float rel)
    {
        g.f1 (cut, 0.1f, envAmt, 0.0f, 0, 0.5f);
        g.env (2, 1, fltDecay, 0.0f, rel);
        g.amp (1, ampDecay, sus, rel);
        velBright (g);
    }
    // sustained, bowed/blown tone with a swell and delayed vibrato
    inline void blown (G& g, float cut, float attack, float envAmt, float vibAmt, float vibRate)
    {
        g.f1 (cut, 0.15f, envAmt, 0.0f, 0, 0.4f);
        g.env (2, attack * 0.8f, 600, 0.6f, 200);
        g.amp (attack, 300, 0.9f, 220);
        if (vibAmt > 0) g.vibrato (vibAmt, vibRate, 350);
        velBright (g, 0.25f);
    }
    inline void bell (G& g, int modSemi, float fmAmt, float decay, float cut = 9000)
    {
        // modulator ratio: OSC 2 reaches at most +2 oct +12 semi (x8)
        const int oct = juce::jmin (2, modSemi / 12), semi = juce::jmin (12, modSemi - 12 * oct);
        g.osc (1, Sine, 0.8f); g.osc (2, Sine, 0.0f, oct, semi); g.fm (fmAmt);
        g.f1 (cut, 0.0f, 0.3f, 0.0f, 0, 0.3f); g.env (2, 1, decay * 0.4f, 0.0f, decay * 0.5f);
        g.amp (1, decay, 0.0f, decay * 0.6f); velBright (g, 0.2f);
    }
    inline void breath (G& g, float lvl) { g.noise (lvl, 1); }

    struct Prog { const char* name; std::function<void (G&)> build; };

    inline std::vector<std::pair<juce::String, std::vector<Prog>>> families()
    {
        std::vector<std::pair<juce::String, std::vector<Prog>>> f;

        f.push_back ({ "Piano", {
            { "Acoustic Grand Piano", [] (G& g) { g.osc (1, Tri, 0.7f); g.osc (2, Saw, 0.3f, 0, 0, 3); struck (g, 1400, 0.55f, 900, 2600, 0.0f, 450); g.reverb (0.18f, 0.6f, 0.45f); } },
            { "Bright Acoustic Piano", [] (G& g) { g.osc (1, Tri, 0.6f); g.osc (2, Saw, 0.45f, 0, 0, 4); struck (g, 2600, 0.5f, 700, 2300, 0.0f, 400); g.reverb (0.15f, 0.55f, 0.4f); } },
            { "Electric Grand Piano", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Square, 0.3f, 0, 0, 6); struck (g, 1800, 0.5f, 600, 2200, 0.05f, 350); g.chorus (0.25f); } },
            { "Honky-tonk Piano", [] (G& g) { g.osc (1, Tri, 0.6f, 0, 0, 0, 2, 22); g.osc (2, Saw, 0.35f, 0, 0, -14); struck (g, 1700, 0.5f, 700, 2200, 0.0f, 400); } },
            { "Electric Piano 1", [] (G& g) { g.osc (1, Sine, 0.8f); g.osc (2, Sine, 0.0f, 1); g.fm (0.35f); struck (g, 3500, 0.4f, 500, 2400, 0.15f, 400); trem (g, 4.5f, 0.15f); g.chorus (0.2f); } },
            { "Electric Piano 2", [] (G& g) { g.osc (1, Sine, 0.75f); g.osc (2, Sine, 0.0f, 2, 2); g.fm (0.5f); struck (g, 6000, 0.3f, 300, 2000, 0.1f, 350); g.chorus (0.3f); g.reverb (0.15f, 0.5f, 0.4f); } },
            { "Harpsichord", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Square, 0.35f, 1); g.f2 (HP, 300, 0.1f); struck (g, 5000, 0.3f, 300, 1500, 0.0f, 250); } },
            { "Clavinet", [] (G& g) { g.osc (1, Square, 0.7f); g.osc (2, Saw, 0.3f, 0, 0, 5); g.f2 (HP, 450, 0.2f); struck (g, 2600, 0.5f, 220, 900, 0.25f, 120); g.mod (sVel, dF1, 0.45f); } },
        } });

        f.push_back ({ "Chromatic Percussion", {
            { "Celesta", [] (G& g) { bell (g, 12, 0.25f, 1400, 7000); g.reverb (0.22f, 0.6f, 0.45f); } },
            { "Glockenspiel", [] (G& g) { bell (g, 36 + 7, 0.3f, 1600, 12000); g.osc (1, Sine, 0.8f, 1); g.reverb (0.2f, 0.55f, 0.5f); } },
            { "Music Box", [] (G& g) { bell (g, 24 + 7, 0.35f, 1300, 10000); g.osc (1, Sine, 0.8f, 1); g.echo (4, 0.25f, 0.12f); g.reverb (0.25f, 0.6f, 0.5f); } },
            { "Vibraphone", [] (G& g) { bell (g, 48, 0.12f, 2600, 6000); trem (g, 5.5f, 0.35f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Marimba", [] (G& g) { bell (g, 48, 0.15f, 450, 4000); g.osc (1, Sine, 0.8f); g.reverb (0.12f, 0.5f, 0.35f); } },
            { "Xylophone", [] (G& g) { bell (g, 36 + 7, 0.2f, 320, 9000); g.osc (1, Sine, 0.8f, 1); g.reverb (0.12f, 0.45f, 0.35f); } },
            { "Tubular Bells", [] (G& g) { bell (g, 30, 0.45f, 3800, 8000); g.osc (3, Sine, 0.3f, 1, 6); g.reverb (0.3f, 0.75f, 0.6f); } },
            { "Dulcimer", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 2, 12); g.osc (2, Tri, 0.4f, 1); struck (g, 3500, 0.4f, 400, 1800, 0.0f, 500); g.reverb (0.18f, 0.55f, 0.45f); } },
        } });

        f.push_back ({ "Organ", {
            { "Drawbar Organ", [] (G& g) { g.osc (1, Sine, 0.6f); g.osc (2, Sine, 0.45f, 1); g.osc (3, Sine, 0.3f, 1, 7); g.osc (4, Sine, 0.25f, 2); g.f1 (9000, 0.0f); g.amp (3, 10, 1.0f, 60); g.chorus (0.3f, 5.5f); } },
            { "Percussive Organ", [] (G& g) { g.osc (1, Sine, 0.6f); g.osc (2, Sine, 0.4f, 1); g.osc (3, Sine, 0.45f, 1, 7); g.f1 (2500, 0.0f, 0.6f); g.env (2, 1, 250, 0.0f, 60); g.amp (2, 10, 1.0f, 60); g.chorus (0.25f, 5.5f); } },
            { "Rock Organ", [] (G& g) { g.osc (1, Sine, 0.55f); g.osc (2, Square, 0.3f, 1); g.osc (3, Sine, 0.35f, 1, 7); g.osc (4, Sine, 0.3f, 2); g.f1 (6000, 0.1f, 0.0f, 0.5f); g.amp (2, 10, 1.0f, 70); g.chorus (0.4f, 6.0f); g.comp (-16, 3, 3); } },
            { "Church Organ", [] (G& g) { g.osc (1, Square, 0.45f, -1); g.osc (2, Saw, 0.3f); g.osc (3, Square, 0.25f, 1); g.osc (4, Sine, 0.25f, 2); g.f1 (4500, 0.0f); g.amp (60, 10, 1.0f, 600); g.reverb (0.4f, 0.95f, 0.8f, 40); } },
            { "Reed Organ", [] (G& g) { g.osc (1, Square, 0.55f); g.osc (2, Saw, 0.3f, 0, 0, 4); g.f1 (2200, 0.15f); g.amp (40, 10, 1.0f, 150); g.reverb (0.15f, 0.5f, 0.4f); } },
            { "Accordion", [] (G& g) { g.osc (1, Square, 0.5f); g.osc (2, Saw, 0.4f, 0, 0, 9); g.osc (3, Square, 0.25f, 1, 0, -7); g.f1 (2600, 0.2f); g.amp (30, 10, 1.0f, 120); trem (g, 5.0f, 0.08f); } },
            { "Harmonica", [] (G& g) { g.osc (1, Square, 0.7f); breath (g, 0.08f); g.f2 (BP, 1400, 0.35f, 0); blown (g, 3000, 25, 0.3f, 0.006f, 5.5f); } },
            { "Tango Accordion", [] (G& g) { g.osc (1, Saw, 0.5f); g.osc (2, Square, 0.45f, 0, 0, -11); g.f1 (2000, 0.2f); g.amp (25, 10, 1.0f, 120); trem (g, 6.0f, 0.1f); } },
        } });

        f.push_back ({ "Guitar", {
            { "Acoustic Guitar (nylon)", [] (G& g) { g.osc (1, Tri, 0.75f); g.osc (2, Saw, 0.2f); struck (g, 1600, 0.55f, 400, 1800, 0.0f, 350); g.reverb (0.15f, 0.5f, 0.4f); } },
            { "Acoustic Guitar (steel)", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Tri, 0.4f, 1, 0, 4); struck (g, 2600, 0.55f, 350, 1700, 0.0f, 350); g.reverb (0.15f, 0.5f, 0.4f); } },
            { "Electric Guitar (jazz)", [] (G& g) { g.osc (1, Tri, 0.7f); g.osc (2, Saw, 0.25f); struck (g, 1100, 0.4f, 500, 2400, 0.1f, 250); } },
            { "Electric Guitar (clean)", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Square, 0.25f, 0, 0, 5); struck (g, 2400, 0.45f, 450, 2200, 0.1f, 250); g.chorus (0.3f); } },
            { "Electric Guitar (muted)", [] (G& g) { g.osc (1, Saw, 0.7f); struck (g, 1300, 0.6f, 120, 220, 0.0f, 80); } },
            { "Overdriven Guitar", [] (G& g) { g.osc (1, Saw, 0.65f); g.osc (2, Square, 0.4f, 0, 0, 7); g.f1 (2200, 0.25f, 0.2f, 0.75f); g.env (2, 1, 600, 0.5f, 200); g.amp (2, 900, 0.75f, 200); g.comp (-20, 5, 5); } },
            { "Distortion Guitar", [] (G& g) { g.osc (1, Saw, 0.65f, 0, 0, 0, 2, 12); g.osc (2, Square, 0.45f, -1); g.f1 (2600, 0.3f, 0.1f, 1.0f); g.amp (2, 1200, 0.8f, 200); g.comp (-24, 8, 6); } },
            { "Guitar Harmonics", [] (G& g) { g.osc (1, Sine, 0.7f, 1); g.osc (2, Sine, 0.3f, 2, 7); struck (g, 6000, 0.2f, 800, 2500, 0.0f, 600); g.reverb (0.25f, 0.6f, 0.5f); } },
        } });

        f.push_back ({ "Bass", {
            { "Acoustic Bass", [] (G& g) { g.osc (1, Tri, 0.8f); g.sub (0.3f); struck (g, 700, 0.5f, 300, 1500, 0.0f, 200); } },
            { "Electric Bass (finger)", [] (G& g) { g.osc (1, Saw, 0.6f); g.sub (0.45f); struck (g, 800, 0.45f, 300, 1800, 0.35f, 150); g.comp (-16, 3, 3); } },
            { "Electric Bass (pick)", [] (G& g) { g.osc (1, Saw, 0.7f); g.sub (0.35f); struck (g, 1300, 0.55f, 180, 1400, 0.3f, 120); g.comp (-16, 4, 3); } },
            { "Fretless Bass", [] (G& g) { g.osc (1, Tri, 0.75f); g.sub (0.4f); g.f1 (900, 0.15f, 0.2f); g.env (2, 30, 500, 0.4f, 200); g.amp (8, 900, 0.6f, 200); g.vibrato (0.004f, 5.0f, 500); } },
            { "Slap Bass 1", [] (G& g) { g.osc (1, Saw, 0.65f); g.osc (2, Square, 0.3f, 1); g.sub (0.35f); struck (g, 1600, 0.7f, 90, 900, 0.3f, 100); g.comp (-18, 4, 4); } },
            { "Slap Bass 2", [] (G& g) { g.osc (1, Square, 0.6f); g.osc (2, Saw, 0.35f, 1, 0, 5); g.sub (0.3f); struck (g, 2000, 0.7f, 70, 800, 0.25f, 100); g.comp (-18, 4, 4); } },
            { "Synth Bass 1", [] (G& g) { g.osc (1, Saw, 0.75f); g.sub (0.5f); g.f1 (600, 0.45f, 0.5f, 0.2f); g.env (2, 1, 220, 0.1f, 100); g.amp (1, 400, 0.8f, 100); } },
            { "Synth Bass 2", [] (G& g) { g.osc (1, Square, 0.7f); g.osc (2, Saw, 0.5f, 0, 0, 10); g.sub (0.4f); g.f1 (900, 0.35f, 0.4f); g.env (2, 1, 300, 0.2f, 100); g.amp (2, 500, 0.85f, 120); } },
        } });

        f.push_back ({ "Strings", {
            { "Violin", [] (G& g) { g.osc (1, Saw, 0.75f); g.f2 (BP, 2200, 0.2f, 1, 0.35f); blown (g, 4200, 90, 0.15f, 0.006f, 5.8f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Viola", [] (G& g) { g.osc (1, Saw, 0.75f); g.f2 (BP, 1600, 0.2f, 1, 0.35f); blown (g, 3200, 100, 0.15f, 0.006f, 5.5f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Cello", [] (G& g) { g.osc (1, Saw, 0.75f); g.f2 (BP, 900, 0.2f, 1, 0.35f); blown (g, 2400, 110, 0.15f, 0.005f, 5.2f); g.reverb (0.2f, 0.65f, 0.55f); } },
            { "Contrabass", [] (G& g) { g.osc (1, Saw, 0.7f); g.sub (0.3f); blown (g, 1300, 120, 0.15f, 0.004f, 5.0f); g.reverb (0.15f, 0.6f, 0.5f); } },
            { "Tremolo Strings", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 3, 14); blown (g, 3500, 60, 0.1f, 0.0f, 0); trem (g, 9.0f, 0.5f); g.reverb (0.25f, 0.7f, 0.55f); } },
            { "Pizzicato Strings", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 2, 10); g.osc (2, Tri, 0.4f); struck (g, 1800, 0.5f, 150, 380, 0.0f, 250); g.reverb (0.25f, 0.65f, 0.5f); } },
            { "Orchestral Harp", [] (G& g) { g.osc (1, Tri, 0.8f); g.osc (2, Sine, 0.3f, 1); struck (g, 2600, 0.4f, 600, 2200, 0.0f, 900); g.reverb (0.3f, 0.7f, 0.6f); } },
            { "Timpani", [] (G& g) { g.osc (1, Sine, 0.85f); g.osc (2, Tri, 0.35f, 0, 0, 30); g.noise (0.1f, 1); g.env (3, 1, 160, 0.0f, 160); g.mod (sENV3, dPitch, 0.05f);
                                     struck (g, 900, 0.4f, 300, 1400, 0.0f, 900); g.reverb (0.25f, 0.75f, 0.6f); } },
        } });

        f.push_back ({ "Ensemble", {
            { "String Ensemble 1", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 4, 18); g.osc (2, Saw, 0.4f, -1, 0, 0, 2, 12); blown (g, 3800, 160, 0.1f, 0.003f, 5.0f); g.chorus (0.3f); g.reverb (0.3f, 0.75f, 0.6f); } },
            { "String Ensemble 2", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 22); blown (g, 2800, 350, 0.1f, 0.003f, 4.6f); g.chorus (0.35f); g.reverb (0.35f, 0.8f, 0.65f); } },
            { "Synth Strings 1", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 25); g.osc (2, Saw, 0.4f, 1, 0, 0, 3, 20); g.f1 (4500, 0.15f, 0.2f); g.env (2, 200, 900, 0.6f, 500); g.amp (120, 300, 0.9f, 500); g.chorus (0.4f); g.reverb (0.25f, 0.7f, 0.55f); } },
            { "Synth Strings 2", [] (G& g) { g.osc (1, Square, 0.55f, 0, 0, 0, 4, 20); g.osc (2, Saw, 0.45f, 0, 0, 0, 3, 15); g.f1 (2600, 0.25f, 0.3f); g.env (2, 400, 1200, 0.5f, 600); g.amp (250, 300, 0.9f, 600); g.phaser (0.3f, 0.2f); g.reverb (0.25f, 0.7f, 0.55f); } },
            { "Choir Aahs", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 4, 14); breath (g, 0.05f); g.f2 (BP, 800, 0.55f, 1, 0.6f); blown (g, 3000, 200, 0.05f, 0.004f, 5.0f); g.reverb (0.35f, 0.8f, 0.65f); } },
            { "Voice Oohs", [] (G& g) { g.osc (1, Tri, 0.7f, 0, 0, 0, 3, 10); g.f2 (BP, 450, 0.55f, 1, 0.55f); blown (g, 1500, 180, 0.05f, 0.004f, 5.0f); g.reverb (0.35f, 0.8f, 0.65f); } },
            { "Synth Voice", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 3, 18); g.osc (2, Sine, 0.4f, 1); g.f2 (BP, 1100, 0.5f, 1, 0.5f); blown (g, 2500, 120, 0.1f, 0.004f, 5.5f); g.chorus (0.35f); g.reverb (0.3f, 0.75f, 0.6f); } },
            { "Orchestra Hit", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 5, 25); g.osc (2, Saw, 0.5f, -1, 7, 0, 3, 20); g.osc (3, Square, 0.4f, 1); g.noise (0.15f);
                                           struck (g, 3500, 0.4f, 200, 450, 0.0f, 450); g.reverb (0.35f, 0.85f, 0.6f); } },
        } });

        f.push_back ({ "Brass", {
            { "Trumpet", [] (G& g) { g.osc (1, Saw, 0.75f); blown (g, 1500, 25, 0.45f, 0.005f, 5.5f); g.reverb (0.15f, 0.6f, 0.45f); } },
            { "Trombone", [] (G& g) { g.osc (1, Saw, 0.75f); g.osc (2, Square, 0.25f); blown (g, 1000, 40, 0.45f, 0.004f, 5.0f); g.reverb (0.15f, 0.6f, 0.45f); } },
            { "Tuba", [] (G& g) { g.osc (1, Saw, 0.65f); g.sub (0.35f); blown (g, 600, 50, 0.4f, 0.003f, 4.5f); } },
            { "Muted Trumpet", [] (G& g) { g.osc (1, Saw, 0.75f); g.f2 (BP, 1400, 0.5f, 1, 0.6f); blown (g, 2000, 20, 0.3f, 0.004f, 5.5f); } },
            { "French Horn", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Tri, 0.4f); blown (g, 750, 70, 0.35f, 0.003f, 5.0f); g.reverb (0.25f, 0.7f, 0.55f); } },
            { "Brass Section", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 3, 12); g.osc (2, Saw, 0.45f, 0, 0, 0, 2, 9); blown (g, 1300, 30, 0.5f, 0.004f, 5.2f); g.reverb (0.2f, 0.65f, 0.5f); } },
            { "Synth Brass 1", [] (G& g) { g.osc (1, Saw, 0.65f, 0, 0, 0, 2, 12); g.osc (2, Saw, 0.5f, 0, 0, 7); g.f1 (900, 0.25f, 0.6f); g.env (2, 15, 400, 0.4f, 200); g.amp (5, 300, 0.85f, 200); velBright (g); } },
            { "Synth Brass 2", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 4, 18); g.osc (2, Square, 0.4f, -1); g.f1 (1300, 0.2f, 0.45f); g.env (2, 80, 600, 0.5f, 300); g.amp (40, 300, 0.9f, 300); g.chorus (0.3f); } },
        } });

        f.push_back ({ "Reed", {
            { "Soprano Sax", [] (G& g) { g.osc (1, Saw, 0.5f); g.osc (2, Square, 0.4f); breath (g, 0.05f); g.f2 (BP, 1800, 0.3f, 1, 0.4f); blown (g, 3000, 25, 0.3f, 0.006f, 5.5f); } },
            { "Alto Sax", [] (G& g) { g.osc (1, Saw, 0.5f); g.osc (2, Square, 0.4f); breath (g, 0.05f); g.f2 (BP, 1300, 0.3f, 1, 0.4f); blown (g, 2400, 30, 0.3f, 0.006f, 5.2f); } },
            { "Tenor Sax", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Square, 0.4f); breath (g, 0.06f); g.f2 (BP, 900, 0.3f, 1, 0.4f); blown (g, 1800, 30, 0.35f, 0.005f, 5.0f); } },
            { "Baritone Sax", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Square, 0.45f); breath (g, 0.05f); blown (g, 1200, 35, 0.35f, 0.004f, 4.8f); } },
            { "Oboe", [] (G& g) { g.osc (1, Square, 0.5f); g.osc (2, Saw, 0.4f); g.f2 (BP, 1500, 0.45f, 1, 0.6f); blown (g, 3500, 30, 0.2f, 0.005f, 5.5f); } },
            { "English Horn", [] (G& g) { g.osc (1, Square, 0.5f); g.osc (2, Saw, 0.4f); g.f2 (BP, 1000, 0.45f, 1, 0.6f); blown (g, 2600, 40, 0.2f, 0.005f, 5.2f); } },
            { "Bassoon", [] (G& g) { g.osc (1, Square, 0.55f); g.osc (2, Saw, 0.35f); g.f2 (BP, 550, 0.4f, 1, 0.55f); blown (g, 1500, 40, 0.2f, 0.004f, 5.0f); } },
            { "Clarinet", [] (G& g) { g.osc (1, Square, 0.75f); breath (g, 0.03f); blown (g, 2200, 35, 0.2f, 0.004f, 5.0f); } },
        } });

        f.push_back ({ "Pipe", {
            { "Piccolo", [] (G& g) { g.osc (1, Sine, 0.75f, 1); g.osc (2, Tri, 0.2f, 1); breath (g, 0.06f); blown (g, 7000, 30, 0.1f, 0.006f, 5.8f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Flute", [] (G& g) { g.osc (1, Sine, 0.75f); g.osc (2, Tri, 0.25f); breath (g, 0.07f); blown (g, 5000, 50, 0.1f, 0.006f, 5.5f); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Recorder", [] (G& g) { g.osc (1, Tri, 0.75f); breath (g, 0.04f); blown (g, 4000, 30, 0.05f, 0.0f, 0); g.reverb (0.18f, 0.55f, 0.45f); } },
            { "Pan Flute", [] (G& g) { g.osc (1, Sine, 0.7f); breath (g, 0.12f); g.f2 (BP, 1200, 0.2f, 1, 0.3f); blown (g, 3500, 60, 0.15f, 0.005f, 5.0f); g.reverb (0.3f, 0.7f, 0.6f); } },
            { "Blown Bottle", [] (G& g) { g.osc (1, Sine, 0.6f); breath (g, 0.2f); g.f2 (BP, 900, 0.5f, 1, 0.5f); blown (g, 2000, 80, 0.1f, 0.003f, 4.5f); g.reverb (0.25f, 0.65f, 0.55f); } },
            { "Shakuhachi", [] (G& g) { g.osc (1, Sine, 0.65f); g.osc (2, Tri, 0.2f); breath (g, 0.18f); blown (g, 3000, 90, 0.3f, 0.007f, 4.8f); g.reverb (0.3f, 0.7f, 0.6f); } },
            { "Whistle", [] (G& g) { g.osc (1, Sine, 0.8f, 1); breath (g, 0.03f); blown (g, 8000, 25, 0.0f, 0.008f, 6.0f); } },
            { "Ocarina", [] (G& g) { g.osc (1, Sine, 0.8f); breath (g, 0.04f); blown (g, 3000, 40, 0.05f, 0.005f, 5.2f); g.reverb (0.2f, 0.6f, 0.5f); } },
        } });

        f.push_back ({ "Synth Lead", {
            { "Lead 1 (square)", [] (G& g) { g.osc (1, Square, 0.7f); g.osc (2, Square, 0.3f, 0, 0, 7); g.f1 (4000, 0.2f); g.amp (3, 200, 0.9f, 150); g.vibrato (0.004f, 5.5f, 500); } },
            { "Lead 2 (sawtooth)", [] (G& g) { g.osc (1, Saw, 0.65f, 0, 0, 0, 2, 12); g.osc (2, Saw, 0.4f, 0, 0, 8); g.f1 (4500, 0.25f, 0.2f); g.env (2, 1, 400, 0.6f, 150); g.amp (3, 200, 0.9f, 150); g.vibrato (0.004f, 5.5f, 500); } },
            { "Lead 3 (calliope)", [] (G& g) { g.osc (1, Tri, 0.6f); g.osc (2, Sine, 0.4f, 1); breath (g, 0.08f); blown (g, 4000, 20, 0.1f, 0.006f, 6.0f); } },
            { "Lead 4 (chiff)", [] (G& g) { g.osc (1, Tri, 0.65f); g.osc (2, Square, 0.25f); g.noise (0.15f); g.f1 (2500, 0.2f, 0.5f); g.env (2, 1, 80, 0.2f, 100); g.amp (2, 200, 0.85f, 200); } },
            { "Lead 5 (charang)", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Square, 0.45f, 0, 0, 6); g.f1 (2800, 0.3f, 0.3f, 0.7f); g.env (2, 1, 500, 0.4f, 150); g.amp (2, 600, 0.8f, 200); g.comp (-18, 4, 4); } },
            { "Lead 6 (voice)", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 2, 10); g.f2 (BP, 1000, 0.6f, 1, 0.6f); blown (g, 3000, 60, 0.1f, 0.005f, 5.5f); g.chorus (0.3f); } },
            { "Lead 7 (fifths)", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Saw, 0.5f, 0, 7); g.f1 (3500, 0.25f, 0.3f); g.env (2, 1, 400, 0.5f, 150); g.amp (3, 200, 0.9f, 150); } },
            { "Lead 8 (bass + lead)", [] (G& g) { g.osc (1, Saw, 0.65f); g.osc (2, Square, 0.45f, -1); g.sub (0.3f); g.f1 (2200, 0.35f, 0.4f, 0.3f); g.env (2, 1, 300, 0.3f, 120); g.amp (2, 300, 0.85f, 120); } },
        } });

        f.push_back ({ "Synth Pad", {
            { "Pad 1 (new age)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 3, 15); g.osc (2, Sine, 0.4f, 1, 7); g.f1 (2800, 0.2f, 0.3f); g.env (2, 400, 1500, 0.5f, 900); g.amp (300, 500, 0.9f, 1200); g.chorus (0.35f); g.reverb (0.35f, 0.85f, 0.7f); } },
            { "Pad 2 (warm)", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 4, 18); g.osc (2, Tri, 0.4f, -1); g.f1 (1300, 0.15f, 0.2f); g.env (2, 500, 1500, 0.6f, 900); g.amp (400, 500, 0.9f, 1200); g.chorus (0.3f); g.reverb (0.3f, 0.8f, 0.65f); } },
            { "Pad 3 (polysynth)", [] (G& g) { g.osc (1, Saw, 0.6f, 0, 0, 0, 2, 12); g.osc (2, Square, 0.4f, 0, 0, 7); g.f1 (1800, 0.3f, 0.45f); g.env (2, 20, 700, 0.3f, 400); g.amp (15, 400, 0.85f, 500); g.chorus (0.35f); } },
            { "Pad 4 (choir)", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 5, 18); g.f2 (BP, 700, 0.5f, 1, 0.6f); blown (g, 2500, 450, 0.05f, 0.003f, 4.8f); g.reverb (0.4f, 0.9f, 0.75f); } },
            { "Pad 5 (bowed)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 3, 12); g.osc (2, Tri, 0.4f, 1); g.f1 (2000, 0.2f, 0.35f); g.env (2, 800, 1500, 0.5f, 900); g.amp (500, 500, 0.85f, 1000); g.reverb (0.35f, 0.8f, 0.7f); } },
            { "Pad 6 (metallic)", [] (G& g) { g.osc (1, Sine, 0.6f); g.osc (2, Sine, 0.0f, 1, 7); g.fm (0.4f); g.osc (3, Saw, 0.3f, 0, 0, 0, 3, 20); g.f1 (5000, 0.2f); g.amp (300, 600, 0.85f, 1200); g.flanger (0.3f, 0.15f, 0.4f); g.reverb (0.35f, 0.85f, 0.7f); } },
            { "Pad 7 (halo)", [] (G& g) { g.osc (1, Tri, 0.55f, 0, 0, 0, 4, 15); g.osc (2, Sine, 0.4f, 1); g.f2 (BP, 1500, 0.4f, 1, 0.4f); g.f1 (3500, 0.1f); g.amp (600, 500, 0.9f, 1500); g.chorus (0.35f); g.reverb (0.45f, 0.95f, 0.8f); } },
            { "Pad 8 (sweep)", [] (G& g) { g.osc (1, Saw, 0.55f, 0, 0, 0, 4, 20); g.osc (2, Saw, 0.4f, -1); g.f1 (600, 0.45f); g.lfo (1, 1, 0.12f); g.mod (sLFO1, dF1, 0.6f); g.amp (300, 500, 0.9f, 1200); g.phaser (0.35f, 0.1f); g.reverb (0.35f, 0.85f, 0.7f); } },
        } });

        f.push_back ({ "Synth Effects", {
            { "FX 1 (rain)", [] (G& g) { bell (g, 31, 0.3f, 900, 9000); g.noise (0.08f, 1); g.echo (4, 0.45f, 0.3f, true); g.reverb (0.35f, 0.85f, 0.7f); } },
            { "FX 2 (soundtrack)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 5, 25); g.osc (2, Sine, 0.4f, -1); g.f1 (900, 0.3f); g.lfo (1, 0, 0.07f); g.mod (sLFO1, dF1, 0.5f); g.amp (900, 500, 0.9f, 2000); g.reverb (0.45f, 0.95f, 0.85f); } },
            { "FX 3 (crystal)", [] (G& g) { bell (g, 24 + 5, 0.45f, 1800, 12000); g.osc (3, Sine, 0.3f, 2); g.echo (5, 0.4f, 0.25f, true); g.reverb (0.35f, 0.85f, 0.7f); } },
            { "FX 4 (atmosphere)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 3, 15); g.osc (2, Tri, 0.45f, 1); struck (g, 3000, 0.4f, 900, 2500, 0.5f, 1500); g.chorus (0.35f); g.reverb (0.4f, 0.9f, 0.75f); } },
            { "FX 5 (brightness)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 5, 20); g.osc (2, Square, 0.35f, 1); g.f1 (6000, 0.3f); g.amp (200, 500, 0.85f, 1500); g.chorus (0.4f); g.reverb (0.4f, 0.9f, 0.75f); } },
            { "FX 6 (goblins)", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Square, 0.4f, 0, 0, 30); g.f2 (BP, 700, 0.6f, 1, 0.7f); g.lfo (1, 4, 6.0f); g.mod (sLFO1, dF2, 0.6f); g.amp (600, 500, 0.85f, 1500); g.reverb (0.35f, 0.85f, 0.7f); } },
            { "FX 7 (echoes)", [] (G& g) { g.osc (1, Saw, 0.5f, 0, 0, 0, 2, 10); g.osc (2, Sine, 0.4f, 1); g.f1 (2500, 0.25f, 0.3f); g.env (2, 50, 800, 0.4f, 600); g.amp (60, 600, 0.7f, 900); g.echo (4, 0.6f, 0.4f, true); g.reverb (0.3f, 0.8f, 0.7f); } },
            { "FX 8 (sci-fi)", [] (G& g) { g.osc (1, Sine, 0.6f); g.osc (2, Sine, 0.0f, 0, 5); g.fm (0.5f); g.lfo (2, 0, 3.0f); g.mod (sLFO2, dPitch, 0.02f); g.f1 (5000, 0.3f); g.amp (100, 500, 0.85f, 1000); g.echo (5, 0.5f, 0.3f, true); g.reverb (0.35f, 0.85f, 0.7f); } },
        } });

        f.push_back ({ "Ethnic", {
            { "Sitar", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Saw, 0.3f, 1, 0, 6); g.f2 (BP, 2500, 0.6f, 1, 0.5f); struck (g, 4000, 0.4f, 600, 2500, 0.1f, 600); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Banjo", [] (G& g) { g.osc (1, Saw, 0.6f); g.osc (2, Square, 0.35f, 1); g.f2 (HP, 500, 0.2f); struck (g, 3500, 0.5f, 150, 700, 0.0f, 200); } },
            { "Shamisen", [] (G& g) { g.osc (1, Saw, 0.65f); g.f2 (BP, 1800, 0.5f, 1, 0.5f); struck (g, 3000, 0.5f, 130, 600, 0.0f, 200); } },
            { "Koto", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Tri, 0.4f, 1); struck (g, 2800, 0.45f, 300, 1500, 0.0f, 500); g.reverb (0.25f, 0.65f, 0.55f); } },
            { "Kalimba", [] (G& g) { bell (g, 24, 0.2f, 700, 5000); g.reverb (0.2f, 0.55f, 0.45f); } },
            { "Bagpipe", [] (G& g) { g.osc (1, Saw, 0.55f); g.osc (2, Saw, 0.35f, -1); g.osc (3, Saw, 0.3f, -2); g.f2 (BP, 1600, 0.4f, 1, 0.5f); g.f1 (3500, 0.15f); g.amp (30, 10, 1.0f, 80); } },
            { "Fiddle", [] (G& g) { g.osc (1, Saw, 0.75f); g.f2 (BP, 2600, 0.3f, 1, 0.4f); blown (g, 4500, 40, 0.2f, 0.007f, 6.0f); } },
            { "Shanai", [] (G& g) { g.osc (1, Square, 0.55f); g.osc (2, Saw, 0.4f); g.f2 (BP, 1800, 0.55f, 1, 0.6f); blown (g, 4000, 30, 0.2f, 0.008f, 6.2f); } },
        } });

        f.push_back ({ "Percussive", {
            { "Tinkle Bell", [] (G& g) { bell (g, 36 + 3, 0.4f, 1200, 12000); g.osc (1, Sine, 0.8f, 1); g.reverb (0.25f, 0.6f, 0.5f); } },
            { "Agogo", [] (G& g) { bell (g, 19, 0.35f, 350, 8000); g.reverb (0.15f, 0.5f, 0.4f); } },
            { "Steel Drums", [] (G& g) { bell (g, 24 + 2, 0.25f, 900, 6000); g.osc (3, Sine, 0.25f, 1); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Woodblock", [] (G& g) { bell (g, 31, 0.25f, 90, 5000); g.osc (1, Sine, 0.8f, 1); } },
            { "Taiko Drum", [] (G& g) { g.osc (1, Sine, 0.85f, -1); g.noise (0.25f, 1); g.env (3, 1, 120, 0.0f, 120); g.mod (sENV3, dPitch, 0.08f); struck (g, 800, 0.5f, 120, 600, 0.0f, 500); g.reverb (0.3f, 0.8f, 0.6f); } },
            { "Melodic Tom", [] (G& g) { g.osc (1, Sine, 0.85f); g.noise (0.15f, 1); g.env (3, 1, 180, 0.0f, 180); g.mod (sENV3, dPitch, 0.06f); struck (g, 1500, 0.5f, 120, 500, 0.0f, 400); g.reverb (0.2f, 0.6f, 0.5f); } },
            { "Synth Drum", [] (G& g) { g.osc (1, Tri, 0.85f); g.noise (0.15f); g.env (3, 1, 140, 0.0f, 140); g.mod (sENV3, dPitch, 0.15f); struck (g, 2500, 0.5f, 100, 380, 0.0f, 300); } },
            { "Reverse Cymbal", [] (G& g) { g.noise (0.7f); g.osc (1, Sine, 0.0f); g.f2 (HP, 3000, 0.2f); g.f1 (12000, 0.0f); g.amp (1800, 10, 1.0f, 30); g.reverb (0.3f, 0.8f, 0.6f); } },
        } });

        f.push_back ({ "Sound Effects", {
            { "Guitar Fret Noise", [] (G& g) { g.noise (1.0f); g.osc (1, Saw, 0.6f); g.f2 (BP, 2500, 0.3f, 1, 0.5f); g.lfo (1, 4, 30.0f); g.mod (sLFO1, dF2, 0.3f); g.amp (5, 220, 0.0f, 80); } },
            { "Breath Noise", [] (G& g) { g.noise (1.0f, 0); g.osc (1, Sine, 0.0f); g.f2 (BP, 1400, 0.15f, 1, 0.4f); g.amp (120, 300, 0.7f, 300); } },
            { "Seashore", [] (G& g) { g.noise (0.7f, 1); g.osc (1, Sine, 0.0f); g.f1 (1500, 0.1f); g.lfo (1, 0, 0.12f); g.mod (sLFO1, dF1, 0.6f); g.mod (sLFO1, dAmp, 0.4f); g.amp (1500, 500, 0.9f, 2500); g.reverb (0.35f, 0.9f, 0.7f); } },
            { "Bird Tweet", [] (G& g) { g.osc (1, Sine, 0.7f, 2); g.lfo (2, 2, 14.0f); g.mod (sLFO2, dPitch, 0.08f); g.env (3, 1, 150, 0.0f, 100); g.mod (sENV3, dPitch, 0.1f); g.amp (5, 250, 0.3f, 150); g.reverb (0.25f, 0.6f, 0.5f); } },
            { "Telephone Ring", [] (G& g) { g.osc (1, Square, 0.4f, 1); g.osc (2, Square, 0.3f, 1, 4); g.lfo (3, 3, 18.0f); g.mod (sLFO3, dAmp, -0.9f); g.f1 (4000, 0.1f); g.amp (2, 10, 1.0f, 60); } },
            { "Helicopter", [] (G& g) { g.noise (0.7f, 1); g.osc (1, Saw, 0.3f, -2); g.f1 (900, 0.3f); g.lfo (3, 3, 11.0f); g.mod (sLFO3, dAmp, -0.85f); g.amp (400, 10, 1.0f, 800); } },
            { "Applause", [] (G& g) { g.noise (0.6f); g.osc (1, Sine, 0.0f); g.f2 (BP, 2200, 0.2f, 1, 0.8f); g.lfo (3, 4, 25.0f); g.mod (sLFO3, dAmp, -0.6f); g.amp (500, 500, 0.9f, 1500); g.reverb (0.3f, 0.8f, 0.6f); } },
            { "Gunshot", [] (G& g) { g.noise (0.9f); g.osc (1, Sine, 0.5f, -2); g.env (3, 1, 60, 0.0f, 60); g.mod (sENV3, dPitch, 0.2f); g.f1 (3000, 0.2f, 0.5f); g.env (2, 1, 150, 0.0f, 150); g.amp (1, 350, 0.0f, 300); g.reverb (0.35f, 0.9f, 0.6f); } },
        } });

        return f;
    }

    // Appends the 128 GM presets; `Preset` is SynthPreset (name, category, values).
    template <typename Preset>
    inline void addBank (std::vector<Preset>& out)
    {
        int prog = 0;
        for (auto& fam : families())
            for (auto& pr : fam.second)
            {
                ++prog;
                G g (900000 + prog);
                pr.build (g);
                out.push_back ({ juce::String (prog).paddedLeft ('0', 3) + " " + pr.name,
                                 juce::String::fromUTF8 ("GM \xc2\xb7 ") + fam.first, std::move (g.v) });
            }
        jassert (prog == 128);
    }

    inline bool isGmCategory (const juce::String& cat) { return cat.startsWith (juce::String::fromUTF8 ("GM \xc2\xb7 ")); }
}
