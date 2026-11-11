#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "Dx7.h"

// ===========================================================================
//  8-voice DX7 preview synth on the FM engine of COHESION - SYNTH.
//  Plays from MIDI input (via the collector) and the on-screen keyboard.
// ===========================================================================
class PreviewSynth : public juce::AudioSource
{
public:
    explicit PreviewSynth (juce::MidiKeyboardState& k) : kb (k) { pending = patch = dx7::initVoice(); }

    juce::MidiMessageCollector collector;

    void setPatch (const dx7::Patch& p) { const juce::SpinLock::ScopedLockType l (lock); pending = p; dirty = true; }
    void setVolume (float v) { volume.store (v); }

    void prepareToPlay (int, double sampleRate) override
    {
        sr = sampleRate;
        collector.reset (sampleRate);
        for (auto& v : voices) { v.voice.prepare (sampleRate); v.active = false; }
    }
    void releaseResources() override {}

    void getNextAudioBlock (const juce::AudioSourceChannelInfo& info) override
    {
        info.clearActiveBufferRegion();
        const int n = info.numSamples;
        {
            const juce::SpinLock::ScopedTryLockType l (lock);
            if (l.isLocked() && dirty) { patch = pending; dirty = false; }
        }
        juce::MidiBuffer midi;
        collector.removeNextBlockOfMessages (midi, n);
        kb.processNextMidiBuffer (midi, 0, n, true);

        float* L = info.buffer->getWritePointer (0, info.startSample);
        float* R = info.buffer->getNumChannels() > 1 ? info.buffer->getWritePointer (1, info.startSample) : nullptr;
        int pos = 0;
        for (const auto meta : midi)
        {
            const int at = juce::jlimit (pos, n, meta.samplePosition);
            render (L, R, pos, at); pos = at;
            handle (meta.getMessage());
        }
        render (L, R, pos, n);
        for (auto& v : voices) if (v.active && v.voice.isDone()) v.active = false;
    }

private:
    struct Slot { dx7::Voice voice; bool active = false, held = false, sustained = false; int note = -1; uint32_t age = 0; };

    void render (float* L, float* R, int from, int to)
    {
        const float g = volume.load();
        for (int s = from; s < to; ++s)
        {
            float sum = 0.0f;
            for (auto& v : voices) if (v.active) sum += v.voice.next (pitchMult);
            const float y = std::tanh (sum * g);
            L[s] += y; if (R != nullptr) R[s] += y;
        }
    }

    void release (Slot& v) { v.held = false; v.sustained = false; v.voice.noteOff(); }

    void handle (const juce::MidiMessage& m)
    {
        if (m.isNoteOn())
        {
            Slot* target = nullptr;
            for (auto& v : voices) if (v.active && v.note == m.getNoteNumber()) { target = &v; break; }   // retrigger same key
            if (target == nullptr) for (auto& v : voices) if (! v.active) { target = &v; break; }
            if (target == nullptr) { target = &voices[0]; for (auto& v : voices) if (v.age < target->age) target = &v; }
            target->voice.noteOn (patch, m.getNoteNumber(), m.getFloatVelocity(), {}, rng);
            target->active = true; target->held = true; target->sustained = false;
            target->note = m.getNoteNumber(); target->age = ++counter;
        }
        else if (m.isNoteOff())
        {
            for (auto& v : voices)
                if (v.active && v.held && v.note == m.getNoteNumber())
                {
                    if (sustain) { v.held = false; v.sustained = true; }
                    else release (v);
                }
        }
        else if (m.isSustainPedalOn())  sustain = true;
        else if (m.isSustainPedalOff()) { sustain = false; for (auto& v : voices) if (v.sustained) release (v); }
        else if (m.isAllNotesOff() || m.isAllSoundOff()) { for (auto& v : voices) if (v.active) release (v); }
        else if (m.isPitchWheel()) pitchMult = std::exp2 ((m.getPitchWheelValue() - 8192) / 8192.0 * 2.0 / 12.0);
    }

    juce::MidiKeyboardState& kb;
    juce::SpinLock lock;
    dx7::Patch pending, patch;
    bool dirty = false, sustain = false;
    std::atomic<float> volume { 0.7f };
    std::array<Slot, 8> voices;
    juce::Random rng;
    double sr = 48000.0, pitchMult = 1.0;
    uint32_t counter = 0;
};
