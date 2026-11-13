#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthDsp.h"
#include "Dx7.h"
#include "La32.h"

// ===========================================================================
//  One polyphonic voice. Reads a shared per-block parameter snapshot owned by
//  the processor (audio thread only). Supports 4 oscillators + sub + noise,
//  two filters (ladder + SVF) with serial/parallel routing, 4 envelopes,
//  4 LFOs (per voice, or shared MONO), an 8-slot modulation matrix, pitch bend,
//  and the FM (DX7) and LA (MT-32) engines.
//
//  Modulation runs per sample; the expensive maths (filter coefficients, pitch,
//  pan) runs at control rate (every kCtl samples). Levels and cutoffs are
//  smoothed, and a stolen voice fades its last output out instead of jumping.
// ===========================================================================
// The current DX7 voice, shared by all voices (copied at note-on under a spin lock).
struct FmSource
{
    juce::SpinLock lock;
    dx7::Patch patch;
};

// The current LA timbre + its ROM set, shared by all voices (copied at note-on).
struct LaSource
{
    juce::SpinLock lock;
    std::shared_ptr<const la32::Rom> rom;
    la32::Timbre timbre;
};

struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    static constexpr int   kCtl = 8;            // control-rate interval (samples)
    static constexpr float kBendSemis = 2.0f;   // pitch-bend range (GM default)

    void setSnapshot (const syn::Snapshot* s) { snap = s; }
    void setGlideSource (double* lastNoteHz)    { lastHz = lastNoteHz; }
    void setFmSource (FmSource* s)              { fmSrc = s; }
    void setLaSource (LaSource* s)              { laSrc = s; }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

    void setCurrentPlaybackSampleRate (double rate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (rate);
        if (rate > 0.0)
        {
            sr = rate;
            for (auto& pr : lad) for (auto& f : pr) f.setSR (rate);
            for (auto& pr : svf) for (auto& f : pr) f.setSR (rate);
            for (auto& e : env) e.setSampleRate (rate);
            smC = 1.0f - std::exp (-1.0f / (float) (sr * 0.005));           // 5 ms parameter smoothing
            declickC = std::exp (-1.0f / (float) (sr * 0.003));             // 3 ms steal / stop fade
        }
    }

    void startNote (int note, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        if (snap == nullptr) return;

        // a stolen voice: let its last output fade out instead of jumping to the new note
        if (sounding)
            for (int b = 0; b < kBuses; ++b) { declick[b][0] += last[b][0]; declick[b][1] += last[b][1]; }
        killing = false;

        // which parts play this note: key range, and with the ARP on the part's ARP switch
        // decides whether it takes the ARP notes (MIDI channel 2) or the played ones (channel 1)
        for (int k = 0; k < kBuses; ++k)
        {
            const auto& pp = snap->part[k];
            const bool inRange = note >= juce::jmin (pp.keyLo, pp.keyHi) && note <= juce::jmax (pp.keyLo, pp.keyHi);
            const bool source  = ! snap->arpOn || (pp.arp ? isPlayingChannel (2) : ! isPlayingChannel (2));
            partOn[k] = inRange && source;
        }

        midiNote = note;
        vel = (1.0f - snap->velSens) + snap->velSens * velocity;
        velRaw = velocity;
        bend = ((float) pitchWheel - 8192.0f) / 8192.0f;
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
        for (auto& pr : lad) for (auto& f : pr) f.reset();
        for (auto& pr : svf) for (auto& f : pr) f.reset();
        for (int i = 0; i < 4; ++i)
            osc[i].startNote (oscFreq (i), sr, snap->osc[i], rng);
        sub.startNote (baseFreq * std::pow (2.0, snap->subOct), sr, snap->subWave);
        for (int i = 0; i < 4; ++i) lfo[i].reset (snap->lfo[i].phase / 360.0);
        applyEnvParams();
        for (auto& e : env) { e.reset(); e.noteOn(); }
        samplesSinceNote = 0;
        released = false;
        ctl = 0;
        snapSmoothed();   // levels / cutoffs start at their targets
        fmStarted = laStarted = false;
        if (snap->dxOn && partOn[1] && fmSrc != nullptr)
        {
            dx7::Patch p;
            { const juce::SpinLock::ScopedLockType sl (fmSrc->lock); p = fmSrc->patch; }
            fmv.prepare (sr);
            fmv.noteOn (p, note, velocity, { snap->dxBright, snap->dxTime, snap->dxVel }, rng);
            fmStarted = true;
        }
        engineHz = baseFreq;
        if (snap->laOn && partOn[2] && laSrc != nullptr)
        {
            std::shared_ptr<const la32::Rom> r; la32::Timbre t;
            { const juce::SpinLock::ScopedLockType sl (laSrc->lock); r = laSrc->rom; t = laSrc->timbre; }
            lav.noteOn (std::move (r), t, note, velocity, { snap->laCutoff, snap->laTime }, sr);
            laStarted = true;
        }
        sounding = true;
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { for (auto& e : env) e.noteOff(); fmv.noteOff(); lav.noteOff(); released = true; }
        else              { killing = true; killGain = 1.0f; }   // fade out over ~3 ms, then free the voice
    }

    // immediate stop without any fade (used while the output is muted, e.g. a preset change)
    void hardStop()
    {
        killing = false; sounding = false; clearTails();
        for (auto& e : env) e.reset();
        if (isVoiceActive()) clearCurrentNote();
    }

    // MONO / LEGATO: the sounding voice moves to a new note (glides when GLIDE is set).
    // retrigger = MONO: the envelopes start again from their current level (analog style).
    bool isHeld() const { return isVoiceActive() && ! released && ! killing; }
    void legatoTo (int note, float velocity, bool retrigger)
    {
        const double curHz = baseFreq * std::pow (2.0, glideOct);
        midiNote = note;
        baseFreq = juce::MidiMessage::getMidiNoteInHertz (note);
        glideOct = 0.0;
        if (snap != nullptr && snap->glideMs > 0.5f)
        {
            glideOct = std::log2 (curHz / baseFreq);
            gCoef = std::exp (-3.0 / (sr * (double) snap->glideMs * 0.001));
        }
        if (lastHz != nullptr) *lastHz = baseFreq;
        if (retrigger && snap != nullptr)
        {
            vel = (1.0f - snap->velSens) + snap->velSens * velocity;
            velRaw = velocity;
            for (auto& e : env) e.noteOn();
        }
    }

    void pitchWheelMoved (int v) override              { bend = ((float) v - 8192.0f) / 8192.0f; }
    void controllerMoved (int cc, int value) override  { if (cc == 1) modWheel = (float) value / 127.0f; }

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (snap == nullptr || ! isVoiceActive()) return;

        applyEnvParams();
        for (int i = 0; i < 4; ++i)
            osc[i].configure (oscFreq (i), sr, snap->osc[i]);
        sub.setFreq (baseFreq * std::pow (2.0, snap->subOct), sr);
        noiseTilt.set (snap->noiseTilt, sr);

        // pan gains of sub and noise (per block)
        {
            const float a = (snap->subPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            subGL = std::cos (a); subGR = std::sin (a);
            const float b = (snap->noisePan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
            noiseGL = std::cos (b); noiseGR = std::sin (b);
        }
        for (int i = 0; i < 4; ++i)
        {
            baseLfoRate[i] = syn::lfoHz (snap->lfo[i], snap->bpm);
            lfo[i].setRate (baseLfoRate[i], sr);
            fadeSamp[i] = snap->lfo[i].fade * 0.001f * (float) sr;
        }

        const float keyTrk = (float) (midiNote - 60) / 24.0f;
        const int   nCh = out.getNumChannels();
        // Every part has its own bus (VA 0/1, FM 2/3, LA 4/5); with a stereo buffer (offline
        // level measurement) all parts go to 0/1. The FM / LA engines carry their own
        // envelopes: the AMP envelope only shapes the VA part. FM / LA run through their own
        // copy of the VA filters when the part's FILTER switch is on. The voice ends when every
        // sounding part is done.
        const bool  vaPlay = partOn[0] && (snap->osc[0].on || snap->osc[1].on || snap->osc[2].on || snap->osc[3].on
                                           || snap->subOn || snap->noiseOn);
        const bool  fmPlay = fmStarted && snap->dxOn;
        const bool  laPlay = laStarted && snap->laOn;
        const bool  fmFilt = fmPlay && snap->part[1].viaFilter, laFilt = laPlay && snap->part[2].viaFilter;
        auto chOf = [nCh] (int bus) { return nCh >= 2 * kBuses ? 2 * bus : 0; };

        for (int s = 0; s < numSamples; ++s)
        {
            // --- smoothed parameters ---
            for (int i = 0; i < 4; ++i) smOsc[i] += (snap->osc[i].level - smOsc[i]) * smC;
            smSub   += (snap->subLevel   - smSub)   * smC;
            smNoise += (snap->noiseLevel - smNoise) * smC;
            smDx    += (snap->dxLevel    - smDx)    * smC;
            smLa    += (snap->laLevel    - smLa)    * smC;
            smFm    += (snap->fmAmt      - smFm)    * smC;
            smRing  += (snap->ringAmt    - smRing)  * smC;

            // --- modulation sources ---
            float src[12];
            src[0] = 0.0f;
            for (int i = 0; i < 4; ++i)
            {
                const float fg = (fadeSamp[i] > 1.0f) ? juce::jmin (1.0f, (float) samplesSinceNote / fadeSamp[i]) : 1.0f;
                const float v  = (snap->lfo[i].mono && snap->monoLfo[i] != nullptr)
                                   ? snap->monoLfo[i][startSample + s]
                                   : lfo[i].next (snap->lfo[i].wave, snap->lfo[i].smooth);
                src[1 + i] = v * fg;
            }
            float e[4];
            for (int i = 0; i < 4; ++i) { e[i] = env[i].getNextSample(); src[5 + i] = e[i]; }
            src[9]  = velRaw;
            src[10] = modWheel;
            src[11] = keyTrk;

            // --- mod matrix -> destinations ---
            float d[8] = { 0,0,0,0,0,0,0,0 };
            static constexpr float dNone[8] = { 0,0,0,0,0,0,0,0 };   // VA FILT+MOD off: no matrix on the VA part
            for (int m = 0; m < syn::kNumMod; ++m)
            {
                const auto& ms = snap->mod[m];
                if (ms.src > 0 && ms.dst > 0) d[ms.dst] += src[ms.src] * ms.amt;
            }

            // --- control rate: pitch, filter coefficients, LFO 1 rate ---
            if (ctl-- <= 0)
            {
                ctl = kCtl - 1;
                const double basePitch = glideOct + (double) (bend * kBendSemis) / 12.0;   // glide + bend: every part
                pMult    = std::pow (2.0, (double) d[3] * 2.0 + basePitch);                 // + the matrix: VA
                pMultEng = d[3] != 0.0f ? std::pow (2.0, basePitch) : pMult;
                if (glideOct != 0.0) { glideOct *= std::pow (gCoef, (double) kCtl); if (std::abs (glideOct) < 1.0e-5) glideOct = 0.0; }

                const float tgt1 = std::log2 (snap->fltCutoff) + snap->fltKeyTrk * keyTrk * 2.0f
                                   + snap->fltEnvAmt * e[snap->flt1EnvSrc] * 4.0f + d[1] * 4.0f;
                const float tgt2 = std::log2 (snap->flt2Cutoff) + snap->flt2KeyTrk * keyTrk * 2.0f
                                   + snap->flt2EnvAmt * e[snap->flt2EnvSrc] * 4.0f + d[2] * 4.0f;
                // cutoff smoothing in octaves (the envelope / LFO moves stay fast: 1 ms)
                const float cC = 1.0f - std::exp (-(float) kCtl / (float) (sr * 0.001));
                cutOct1 = firstCtl ? tgt1 : cutOct1 + (tgt1 - cutOct1) * cC;
                cutOct2 = firstCtl ? tgt2 : cutOct2 + (tgt2 - cutOct2) * cC;
                firstCtl = false;
                const float nyq = (float) (sr * 0.45);
                lad[0][0].setParams (juce::jlimit (20.0f, nyq, std::exp2 (cutOct1)), snap->fltRes, snap->fltDrive, snap->fltType);
                svf[0][0].setParams (juce::jlimit (20.0f, nyq, std::exp2 (cutOct2)), snap->flt2Res, snap->flt2Drive, snap->flt2Type, snap->flt2Slope);
                for (int b = 0; b < kBuses; ++b)
                    for (int c = 0; c < 2; ++c)
                        if (b + c > 0) { lad[b][c].copyCoefs (lad[0][0]); svf[b][c].copyCoefs (svf[0][0]); }

                if (d[6] != 0.0f) lfo[0].setRate (baseLfoRate[0] * std::pow (2.0f, d[6] * 2.0f), sr);
            }

            // --- amp mod, pan mod (equal power, like the oscillator pans), stop fade ---
            // The matrix and VELOCITY belong to the VA part. FM / LA follow the matrix (pitch,
            // amp, pan) only with their VA FILTER + MOD switch on, and never VELOCITY: the DX7
            // and LA engines have their own velocity response.
            const float ampMat = juce::jlimit (0.0f, 2.0f, 1.0f + d[4]);
            const float kill   = killing ? (killGain *= declickC) : 1.0f;
            const float pan    = juce::jlimit (-1.0f, 1.0f, d[5]);
            const float pgL    = pan <= 0.0f ? 1.0f : std::sqrt (1.0f - pan);
            const float pgR    = pan >= 0.0f ? 1.0f : std::sqrt (1.0f + pan);
            float gainL[kBuses], gainR[kBuses];
            if (snap->vaFiltMod) { gainL[0] = vel * ampMat * kill * pgL; gainR[0] = vel * ampMat * kill * pgR; }
            else                 { gainL[0] = gainR[0] = vel * kill; }
            for (int b = 1; b < kBuses; ++b)
            {
                const bool viaMod = snap->part[b].viaFilter;
                gainL[b] = viaMod ? ampMat * kill * pgL : kill;
                gainR[b] = viaMod ? ampMat * kill * pgR : kill;
            }
            auto emit = [&] (int bus, float l, float r)
            {
                last[bus][0] = l * gainL[bus];
                last[bus][1] = r * gainR[bus];
                const int ch = chOf (bus);
                out.addSample (ch, startSample + s, last[bus][0] + declick[bus][0]);
                if (nCh > ch + 1) out.addSample (ch + 1, startSample + s, last[bus][1] + declick[bus][1]);
            };

            if (vaPlay) renderVA (s, snap->vaFiltMod ? d : dNone, e[0], emit);
            else { last[0][0] = last[0][1] = 0.0f; if (declick[0][0] != 0.0f || declick[0][1] != 0.0f) emit (0, 0.0f, 0.0f); }
            if (fmPlay)
            {
                float fs = fmv.next ((fmFilt ? pMult : pMultEng) * baseFreq / engineHz) * smDx, fl = fs;
                if (fmFilt) filterPair (1, fs, fl);
                emit (1, fs, fl);
            }
            else if (declick[1][0] != 0.0f || declick[1][1] != 0.0f) { last[1][0] = last[1][1] = 0.0f; emit (1, 0.0f, 0.0f); }
            if (laPlay)
            {
                float ls = lav.render ((laFilt ? pMult : pMultEng) * baseFreq / engineHz, modWheel) * smLa, ll = ls;
                if (laFilt) filterPair (2, ls, ll);
                emit (2, ls, ll);
            }
            else if (declick[2][0] != 0.0f || declick[2][1] != 0.0f) { last[2][0] = last[2][1] = 0.0f; emit (2, 0.0f, 0.0f); }
            for (auto& dc : declick) { dc[0] *= declickC; dc[1] *= declickC; }

            ++samplesSinceNote;
            bool done;
            if (killing) done = killGain < 1.0e-3f;
            else
            {
                const bool vaDone = ! vaPlay || ! env[0].isActive();
                const bool fmDone = ! fmPlay || (released && fmv.isDone());
                const bool laDone = ! laPlay || (released && lav.isDone());
                done = vaDone && fmDone && laDone;
            }
            if (done)
            {
                killing = false; sounding = false;
                // the remaining declick tail is tiny (< -60 dB within a few ms): drop it with the voice
                clearTails();
                clearCurrentNote();
                break;
            }
        }
    }

private:
    static constexpr int kBuses = 3;

    // the VA part of one sample: oscillators, sub, noise -> filters -> AMP envelope
    template <typename Emit>
    void renderVA (int, const float* d, float ampEnv, Emit& emit)
    {
        {
            const double pMult = snap->vaFiltMod ? this->pMult : pMultEng;   // without the matrix pitch when off
            float sumL = 0.0f, sumR = 0.0f, m2 = 0.0f;
            if (snap->osc[1].on) { osc[1].render (snap->osc[1].wave, 0.0, smOsc[1], pMult, sumL, sumR, wtPos (1, d[7])); m2 = osc[1].lastOut; }
            if (snap->osc[0].on) { double pm = (double) smFm * m2 * 0.5; osc[0].render (snap->osc[0].wave, pm, smOsc[0], pMult, sumL, sumR, wtPos (0, d[7])); }
            float m3 = 0.0f, m4 = 0.0f;
            if (snap->osc[2].on) { osc[2].render (snap->osc[2].wave, 0.0, smOsc[2], pMult, sumL, sumR, wtPos (2, d[7])); m3 = osc[2].lastOut; }
            if (snap->osc[3].on) { osc[3].render (snap->osc[3].wave, 0.0, smOsc[3], pMult, sumL, sumR, wtPos (3, d[7])); m4 = osc[3].lastOut; }
            if (smRing > 0.0f) { float r = smRing * m3 * m4; sumL += r; sumR += r; }

            if (snap->subOn)
            {
                const float sv = sub.next (snap->subWave, pMult) * smSub;
                sumL += sv * subGL; sumR += sv * subGR;
            }
            if (snap->noiseOn)
            {
                const float nz = noiseTilt.process (noise.next (snap->noiseType)) * smNoise;
                sumL += nz * noiseGL; sumR += nz * noiseGR;
            }
            // --- filters (VA FILT+MOD off: dry) ---
            float fL, fR;
            if (! snap->vaFiltMod) { fL = sumL; fR = sumR; }
            else if (snap->routing == 0) // serial F1 -> F2
            {
                fL = svf[0][0].process (lad[0][0].process (sumL));
                fR = svf[0][1].process (lad[0][1].process (sumR));
            }
            else // parallel
            {
                const float b = snap->routeBalance;
                fL = (1.0f - b) * lad[0][0].process (sumL) + b * svf[0][0].process (sumL);
                fR = (1.0f - b) * lad[0][1].process (sumR) + b * svf[0][1].process (sumR);
            }
            emit (0, fL * ampEnv, fR * ampEnv);
        }
    }

    // FM / LA through their own copy of the VA filters (same settings, own state)
    void filterPair (int bus, float& l, float& r)
    {
        if (snap->routing == 0)
        {
            l = svf[bus][0].process (lad[bus][0].process (l));
            r = svf[bus][1].process (lad[bus][1].process (r));
        }
        else
        {
            const float b = snap->routeBalance;
            const float il = l, ir = r;
            l = (1.0f - b) * lad[bus][0].process (il) + b * svf[bus][0].process (il);
            r = (1.0f - b) * lad[bus][1].process (ir) + b * svf[bus][1].process (ir);
        }
    }

    void clearTails() { for (int b = 0; b < kBuses; ++b) for (int c = 0; c < 2; ++c) last[b][c] = declick[b][c] = 0.0f; }

    double oscFreq (int i) const
    {
        const auto& p = snap->osc[i];
        return baseFreq * std::pow (2.0, (double) p.oct + (double) p.semi / 12.0 + (double) p.fine / 1200.0);
    }

    // WT mode: the oscillator's WT POS plus the matrix destination "WT Pos"; VA mode: -1
    float wtPos (int i, float mod) const
    {
        const auto& p = snap->osc[i];
        return p.mode == 1 ? juce::jlimit (0.0f, 1.0f, p.wtpos + mod) : -1.0f;
    }

    void applyEnvParams()
    {
        for (int i = 0; i < 4; ++i)
            env[i].setParameters (snap->env[i].a, snap->env[i].d, snap->env[i].s, snap->env[i].r);
    }

    void snapSmoothed()
    {
        for (int i = 0; i < 4; ++i) smOsc[i] = snap->osc[i].level;
        smSub = snap->subLevel; smNoise = snap->noiseLevel; smDx = snap->dxLevel; smLa = snap->laLevel;
        smFm = snap->fmAmt; smRing = snap->ringAmt;
        firstCtl = true;
    }

    const syn::Snapshot* snap = nullptr;
    double sr = 44100.0, baseFreq = 440.0, pMult = 1.0, pMultEng = 1.0, engineHz = 440.0;
    int midiNote = 69, ctl = 0;
    float vel = 1.0f, velRaw = 1.0f, modWheel = 0.0f, bend = 0.0f;
    double* lastHz = nullptr; double glideOct = 0.0, gCoef = 1.0;
    FmSource* fmSrc = nullptr;
    dx7::Voice fmv;
    LaSource* laSrc = nullptr;
    la32::Voice lav;
    bool released = false, sounding = false, killing = false, firstCtl = true;
    long samplesSinceNote = 0;
    float baseLfoRate[4] { 2,2,2,2 }, fadeSamp[4] { 0,0,0,0 };

    // smoothing / declick
    float smC = 0.01f, declickC = 0.99f, killGain = 1.0f;
    float smOsc[4] {}, smSub = 0, smNoise = 0, smDx = 0, smLa = 0, smFm = 0, smRing = 0;
    float cutOct1 = 13.0f, cutOct2 = 14.0f;
    float last[kBuses][2] {}, declick[kBuses][2] {};   // per part bus
    bool  partOn[kBuses] { true, true, true }, fmStarted = false, laStarted = false;
    float subGL = 0.7f, subGR = 0.7f, noiseGL = 0.7f, noiseGR = 0.7f;

    syn::Osc  osc[4];
    syn::Sub  sub;
    syn::Noise noise;
    syn::Tilt noiseTilt;
    syn::Ladder lad[kBuses][2];   // [part][L/R]: VA always, FM / LA only with their FILTER switch on
    syn::SVF    svf[kBuses][2];
    syn::Env    env[4];
    syn::LfoGen lfo[4];
    juce::Random rng;
};

// ===========================================================================
//  juce::Synthesiser with MONO and LEGATO modes: one voice follows the last
//  held key (note priority = last); releasing it returns to the previous held
//  key. In POLY mode it behaves exactly like juce::Synthesiser.
// ===========================================================================
class CohesionSynthesiser : public juce::Synthesiser
{
public:
    int mode = 0;   // 0 poly, 1 mono (retrigger), 2 legato; set by the processor per block
    const syn::Snapshot* snap = nullptr;   // the voices' snapshot: which parts play a note

    // true when at least one sounding part takes this note (key range, ARP / played channel);
    // a note no part plays takes no voice (it would steal one from the notes that do sound)
    bool anyPartPlays (int ch, int note) const
    {
        if (snap == nullptr) return true;
        const bool active[3] = { snap->osc[0].on || snap->osc[1].on || snap->osc[2].on || snap->osc[3].on
                                   || snap->subOn || snap->noiseOn,
                                 snap->dxOn, snap->laOn };
        for (int k = 0; k < 3; ++k)
        {
            const auto& pp = snap->part[k];
            const bool inRange = note >= juce::jmin (pp.keyLo, pp.keyHi) && note <= juce::jmax (pp.keyLo, pp.keyHi);
            const bool source  = ! snap->arpOn || (pp.arp ? ch == 2 : ch != 2);
            if (active[k] && inRange && source) return true;
        }
        return false;
    }

    void noteOn (int ch, int note, float velocity) override
    {
        if (! anyPartPlays (ch, note)) return;
        if (mode == 0) { juce::Synthesiser::noteOn (ch, note, velocity); return; }
        removeHeld (note);
        if (nHeld < 128) held[nHeld++] = note;
        lastVel = velocity;
        if (auto* v = monoVoice()) { v->legatoTo (note, velocity, mode == 1); soundingNote = note; return; }
        juce::Synthesiser::noteOn (ch, note, velocity);
        regNote = note; regCh = ch; soundingNote = note;
    }

    void noteOff (int ch, int note, float velocity, bool allowTailOff) override
    {
        if (mode == 0 || regNote < 0) { juce::Synthesiser::noteOff (ch, note, velocity, allowTailOff); return; }
        removeHeld (note);
        if (note != soundingNote) return;                          // a key that was not sounding
        if (nHeld > 0)
            if (auto* v = monoVoice())
            {
                soundingNote = held[nHeld - 1];
                v->legatoTo (soundingNote, lastVel, mode == 1);
                return;
            }
        juce::Synthesiser::noteOff (regCh, regNote, velocity, allowTailOff);   // the voice knows its first note
        regNote = soundingNote = -1;
    }

    void allNotesOff (int ch, bool allowTailOff) override
    {
        nHeld = 0; regNote = soundingNote = -1;
        juce::Synthesiser::allNotesOff (ch, allowTailOff);
    }

private:
    SynthVoice* monoVoice()
    {
        if (regNote < 0) return nullptr;
        for (int i = 0; i < getNumVoices(); ++i)
            if (auto* v = dynamic_cast<SynthVoice*> (getVoice (i)))
                if (v->getCurrentlyPlayingNote() == regNote && v->isHeld()) return v;
        return nullptr;
    }
    void removeHeld (int note)
    {
        int w = 0;
        for (int r = 0; r < nHeld; ++r) if (held[r] != note) held[w++] = held[r];
        nHeld = w;
    }

    int held[128] {}; int nHeld = 0;
    int regNote = -1, regCh = 1, soundingNote = -1;
    float lastVel = 0.8f;
};
