#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "SynthDsp.h"

// ===========================================================================
//  One polyphonic voice. Reads a shared per-block parameter snapshot owned by
//  the processor (audio thread only).
// ===========================================================================
struct SynthSound : public juce::SynthesiserSound
{
    bool appliesToNote (int) override    { return true; }
    bool appliesToChannel (int) override { return true; }
};

class SynthVoice : public juce::SynthesiserVoice
{
public:
    void setSnapshot (const syn::Snapshot* s) { snap = s; }

    bool canPlaySound (juce::SynthesiserSound* s) override { return dynamic_cast<SynthSound*> (s) != nullptr; }

    void setCurrentPlaybackSampleRate (double rate) override
    {
        juce::SynthesiserVoice::setCurrentPlaybackSampleRate (rate);
        if (rate > 0.0)
        {
            sr = rate;
            ladL.setSR (rate); ladR.setSR (rate);
            ampEnv.setSampleRate (rate); fltEnv.setSampleRate (rate);
        }
    }

    void startNote (int note, float velocity, juce::SynthesiserSound*, int) override
    {
        if (snap == nullptr) return;
        midiNote = note;
        vel = 0.3f + 0.7f * velocity;
        baseFreq = juce::MidiMessage::getMidiNoteInHertz (note);
        ladL.reset(); ladR.reset();
        for (int i = 0; i < 4; ++i)
            osc[i].startNote (oscFreq (i), sr, snap->osc[i], rng);
        sub.startNote (baseFreq * std::pow (2.0, snap->subOct), sr);
        applyEnvParams();
        ampEnv.noteOn(); fltEnv.noteOn();
    }

    void stopNote (float, bool allowTailOff) override
    {
        if (allowTailOff) { ampEnv.noteOff(); fltEnv.noteOff(); }
        else              { clearCurrentNote(); ampEnv.reset(); fltEnv.reset(); }
    }

    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}

    void renderNextBlock (juce::AudioBuffer<float>& out, int startSample, int numSamples) override
    {
        if (snap == nullptr || ! isVoiceActive()) return;

        applyEnvParams();
        for (int i = 0; i < 4; ++i)
            osc[i].configure (oscFreq (i), sr, snap->osc[i]);
        sub.setFreq (baseFreq * std::pow (2.0, snap->subOct), sr);

        const float baseCut = snap->fltCutoff;
        const float res   = snap->fltRes;
        const float drive = snap->fltDrive;
        const int   ftype = snap->fltType;
        const float keyOct = snap->fltKeyTrk * (float) (midiNote - 60) / 12.0f;
        const float envAmt = snap->fltEnvAmt;
        const int   nCh = out.getNumChannels();

        for (int s = 0; s < numSamples; ++s)
        {
            float sumL = 0.0f, sumR = 0.0f;

            float m2 = 0.0f;
            if (snap->osc[1].on) { osc[1].render (snap->osc[1].wave, 0.0, snap->osc[1].level, sumL, sumR); m2 = osc[1].lastOut; }
            if (snap->osc[0].on) { double pm = (double) snap->fmAmt * m2 * 0.5; osc[0].render (snap->osc[0].wave, pm, snap->osc[0].level, sumL, sumR); }

            float m3 = 0.0f, m4 = 0.0f;
            if (snap->osc[2].on) { osc[2].render (snap->osc[2].wave, 0.0, snap->osc[2].level, sumL, sumR); m3 = osc[2].lastOut; }
            if (snap->osc[3].on) { osc[3].render (snap->osc[3].wave, 0.0, snap->osc[3].level, sumL, sumR); m4 = osc[3].lastOut; }
            if (snap->ringAmt > 0.0f) { float r = snap->ringAmt * m3 * m4; sumL += r; sumR += r; }

            if (snap->subOn)
            {
                float sv = sub.next (snap->subWave) * snap->subLevel;
                float a = (snap->subPan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                sumL += sv * std::cos (a); sumR += sv * std::sin (a);
            }
            if (snap->noiseOn)
            {
                float nz = noise.next (snap->noiseType) * snap->noiseLevel;
                float a = (snap->noisePan * 0.5f + 0.5f) * juce::MathConstants<float>::halfPi;
                sumL += nz * std::cos (a); sumR += nz * std::sin (a);
            }

            const float env = fltEnv.getNextSample();
            float cutoff = baseCut * std::pow (2.0f, keyOct) * std::pow (2.0f, envAmt * env * 4.0f);
            cutoff = juce::jlimit (20.0f, (float) (sr * 0.45), cutoff);
            ladL.setParams (cutoff, res, drive, ftype);
            ladR.setParams (cutoff, res, drive, ftype);
            float fL = ladL.process (sumL);
            float fR = ladR.process (sumR);

            const float a = ampEnv.getNextSample() * vel;
            out.addSample (0, startSample + s, fL * a);
            if (nCh > 1) out.addSample (1, startSample + s, fR * a);

            if (! ampEnv.isActive()) { clearCurrentNote(); break; }
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
        ampEnv.setParameters ({ snap->ampA / 1000.0f, snap->ampD / 1000.0f, snap->ampS, snap->ampR / 1000.0f });
        fltEnv.setParameters ({ snap->fenA / 1000.0f, snap->fenD / 1000.0f, snap->fenS, snap->fenR / 1000.0f });
    }

    const syn::Snapshot* snap = nullptr;
    double sr = 44100.0, baseFreq = 440.0;
    int midiNote = 69;
    float vel = 1.0f;

    syn::Osc  osc[4];
    syn::Sub  sub;
    syn::Noise noise;
    syn::Ladder ladL, ladR;
    juce::ADSR ampEnv, fltEnv;
    juce::Random rng;
};
