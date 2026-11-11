#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthDsp.h"
#include "Dx7.h"

// ===========================================================================
//  One polyphonic voice. Reads a shared per-block parameter snapshot owned by
//  the processor (audio thread only). Supports 4 oscillators + sub + noise,
//  two filters (ladder + SVF) with serial/parallel routing, 4 ADSR envelopes,
//  4 LFOs and an 8-slot modulation matrix.
// ===========================================================================
// The current DX7 voice, shared by all voices (copied at note-on under a spin lock).
struct FmSource
{
    juce::SpinLock lock;
    dx7::Patch patch;
};

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    void setSnapshot (const syn::Snapshot* s) { snap = s; }
    void setGlideSource (double* lastNoteHz)    { lastHz = lastNoteHz; }
    void setFmSource (FmSource* s)              { fmSrc = s; }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

    void setCurrentPlaybackSampleRate (double rate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (rate);
        if (rate > 0.0)
        {
            sr = rate;
            lad1L.setSR (rate); lad1R.setSR (rate);
            svf2L.setSR (rate); svf2R.setSR (rate);
            for (auto& e : env) e.setSampleRate (rate);
        }
    }

    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
    {
        if (snap == nullptr) return;
        midiNote = note;
        vel = 0.3f + 0.7f * velocity;
        velRaw = velocity;
        baseFreq = juce::MidiMessage::getMidiNoteInHertz (note);
        glideOct = 0.0;
        if (lastHz != nullptr)
        {
            if (snap->glideMs > 0.5f && *lastHz > 0.0)
            {
                glideOct = std::log2 (*lastHz / baseFreq);
                gCoef = std::exp (-3.0 / (sr * (double) snap->glideMs * 0.001));
            }
            *lastHz = baseFreq;
        }
        lad1L.reset(); lad1R.reset(); svf2L.reset(); svf2R.reset();
        for (int i = 0; i < 4; ++i)
            osc[i].startNote (oscFreq (i), sr, snap->osc[i], rng);
        sub.startNote (baseFreq * std::pow (2.0, snap->subOct), sr);
        for (int i = 0; i < 4; ++i) lfo[i].reset (snap->lfo[i].phase / 360.0);
        applyEnvParams();
        for (auto& e : env) e.noteOn();
        samplesSinceNote = 0;
        released = false;
        if (snap->dxOn && fmSrc != nullptr)
        {
            dx7::Patch p;
            { const juce::SpinLock::ScopedLockType sl (fmSrc->lock); p = fmSrc->patch; }
            fmv.prepare (sr);
            fmv.noteOn (p, note, velocity, { snap->dxBright, snap->dxTime, snap->dxVel }, rng);
        }
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { for (auto& e : env) e.noteOff(); fmv.noteOff(); released = true; }
        else              { clearCurrentNote(); for (auto& e : env) e.reset(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int cc, int value) override { if (cc == 1) modWheel = (float) value / 127.0f; }

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (snap == nullptr || ! isVoiceActive()) return;

        applyEnvParams();
        for (int i = 0; i < 4; ++i)
            osc[i].configure (oscFreq (i), sr, snap->osc[i]);
        sub.setFreq (baseFreq * std::pow (2.0, snap->subOct), sr);

        for (int i = 0; i < 4; ++i)
        {
            baseLfoRate[i] = snap->lfo[i].rateHz;
            lfo[i].setRate (baseLfoRate[i], sr);
            fadeSamp[i] = snap->lfo[i].fade * 0.001f * (float) sr;
        }

        const float keyTrk = (float) (midiNote - 60) / 24.0f;
        const int   nCh = out.getNumChannels();
        const bool  fmOnly = snap->dxOn && ! snap->osc[0].on && ! snap->osc[1].on && ! snap->osc[2].on && ! snap->osc[3].on
                             && ! snap->subOn && ! snap->noiseOn;

        for (int s = 0; s < numSamples; ++s)
        {
            // --- modulation sources ---
            float src[12];
            src[0] = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                float fg = (fadeSamp[i] > 1.0f) ? juce::jmin (1.0f, (float) samplesSinceNote / fadeSamp[i]) : 1.0f;
                src[1 + i] = lfo[i].next (snap->lfo[i].wave, snap->lfo[i].smooth) * fg;
            }
            float e[4];
            for (int i = 0; i < 4; ++i) { e[i] = env[i].getNextSample(); src[5 + i] = e[i]; }
            src[9]  = velRaw;
            src[10] = modWheel;
            src[11] = keyTrk;

            // --- mod matrix -> destinations ---
            float d[7] = { 0,0,0,0,0,0,0 };
            for (int m = 0; m < syn::kNumMod; ++m)
            {
                const auto& ms = snap->mod[m];
                if (ms.src > 0 && ms.dst > 0) d[ms.dst] += src[ms.src] * ms.amt;
            }

            const double pMult = std::pow (2.0, (double) d[3] * 2.0 + glideOct);
            if (glideOct != 0.0) { glideOct *= gCoef; if (std::abs (glideOct) < 1.0e-5) glideOct = 0.0; }

            // --- oscillators ---
            float sumL = 0.0f, sumR = 0.0f, m2 = 0.0f;
            if (snap->osc[1].on) { osc[1].render (snap->osc[1].wave, 0.0, snap->osc[1].level, pMult, sumL, sumR); m2 = osc[1].lastOut; }
            if (snap->osc[0].on) { double pm = (double) snap->fmAmt * m2 * 0.5; osc[0].render (snap->osc[0].wave, pm, snap->osc[0].level, pMult, sumL, sumR); }
            float m3 = 0.0f, m4 = 0.0f;
            if (snap->osc[2].on) { osc[2].render (snap->osc[2].wave, 0.0, snap->osc[2].level, pMult, sumL, sumR); m3 = osc[2].lastOut; }
            if (snap->osc[3].on) { osc[3].render (snap->osc[3].wave, 0.0, snap->osc[3].level, pMult, sumL, sumR); m4 = osc[3].lastOut; }
            if (snap->ringAmt > 0.0f) { float r = snap->ringAmt * m3 * m4; sumL += r; sumR += r; }

            if (snap->subOn)
            {
                float sv = sub.next (snap->subWave, pMult) * snap->subLevel;
                float a = (snap->subPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                sumL += sv * std::cos (a); sumR += sv * std::sin (a);
            }
            if (snap->dxOn)
            {
                const float fs = fmv.next (pMult) * snap->dxLevel;
                sumL += fs; sumR += fs;
            }
            if (snap->noiseOn)
            {
                float nz = noise.next (snap->noiseType) * snap->noiseLevel;
                float a = (snap->noisePan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                sumL += nz * std::cos (a); sumR += nz * std::sin (a);
            }

            // --- filters ---
            float cut1 = snap->fltCutoff * std::pow (2.0f, snap->fltKeyTrk * keyTrk * 2.0f
                                                   + snap->fltEnvAmt * e[snap->flt1EnvSrc] * 4.0f + d[1] * 4.0f);
            cut1 = juce::jlimit (20.0f, (float) (sr * 0.45), cut1);
            lad1L.setParams (cut1, snap->fltRes, snap->fltDrive, snap->fltType);
            lad1R.setParams (cut1, snap->fltRes, snap->fltDrive, snap->fltType);

            float cut2 = snap->flt2Cutoff * std::pow (2.0f, snap->flt2KeyTrk * keyTrk * 2.0f
                                                    + snap->flt2EnvAmt * e[snap->flt2EnvSrc] * 4.0f + d[2] * 4.0f);
            cut2 = juce::jlimit (20.0f, (float) (sr * 0.45), cut2);
            svf2L.setParams (cut2, snap->flt2Res, snap->flt2Drive, snap->flt2Type, snap->flt2Slope);
            svf2R.setParams (cut2, snap->flt2Res, snap->flt2Drive, snap->flt2Type, snap->flt2Slope);

            float fL, fR;
            if (snap->routing == 0) // serial F1 -> F2
            {
                fL = svf2L.process (lad1L.process (sumL));
                fR = svf2R.process (lad1R.process (sumR));
            }
            else // parallel
            {
                const float b = snap->routeBalance;
                fL = (1.0f - b) * lad1L.process (sumL) + b * svf2L.process (sumL);
                fR = (1.0f - b) * lad1R.process (sumR) + b * svf2R.process (sumR);
            }

            // --- amp + pan mod ---
            const float amp  = e[0] * vel * juce::jlimit (0.0f, 2.0f, 1.0f + d[4]);
            const float pan  = juce::jlimit (-1.0f, 1.0f, d[5]);
            const float pgL  = pan <= 0.0f ? 1.0f : 1.0f - pan;
            const float pgR  = pan >= 0.0f ? 1.0f : 1.0f + pan;

            out.addSample (0, startSample + s, fL * amp * pgL);
            if (nCh > 1) out.addSample (1, startSample + s, fR * amp * pgR);

            // LFO 1 rate modulation (applies next sample)
            if (d[6] != 0.0f) lfo[0].setRate (baseLfoRate[0] * std::pow (2.0f, d[6] * 2.0f), sr);

            ++samplesSinceNote;
            if (! env[0].isActive() || (fmOnly && released && fmv.isDone())) { clearCurrentNote(); break; }
        }
    }

private:
    double oscFreq (int i) const
    {
        const auto& p = snap->osc[i];
        return baseFreq * std::pow (2.0, (double) p.oct + (double) p.semi / 12.0 + (double) p.fine / 1200.0);
    }

    void applyEnvParams()
    {
        for (int i = 0; i < 4; ++i)
            env[i].setParameters ({ snap->env[i].a / 1000.0f, snap->env[i].d / 1000.0f,
                                    snap->env[i].s, snap->env[i].r / 1000.0f });
    }

    const syn::Snapshot* snap = nullptr;
    double sr = 44100.0, baseFreq = 440.0;
    int midiNote = 69;
    float vel = 1.0f, velRaw = 1.0f, modWheel = 0.0f;
    double* lastHz = nullptr; double glideOct = 0.0, gCoef = 1.0;
    FmSource* fmSrc = nullptr;
    dx7::Voice fmv;
    bool released = false;
    long samplesSinceNote = 0;
    float baseLfoRate[4] { 2,2,2,2 }, fadeSamp[4] { 0,0,0,0 };

    syn::Osc  osc[4];
    syn::Sub  sub;
    syn::Noise noise;
    syn::Ladder lad1L, lad1R;
    syn::SVF    svf2L, svf2R;
    juce::ADSR  env[4];
    syn::LfoGen lfo[4];
    juce::Random rng;
};
