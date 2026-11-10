#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>
#include "Params.h"

// ===========================================================================
//  Factory presets (foundation set). Each preset lists only the parameters it
//  changes; applying a preset first resets every parameter to its default.
//  Values are the real parameter values (converted to 0..1 on apply).
// ===========================================================================
struct SynthPreset
{
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> v;
};

inline std::vector<SynthPreset> cohesionSynthPresets()
{
    using V = std::vector<std::pair<juce::String, float>>;
    std::vector<SynthPreset> out;

    out.push_back ({ "Default", "Init", { } });

    out.push_back ({ "Sub Driver", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.8f }, { pid::oscUni (1), 3 }, { pid::oscDet (1), 12 },
        { pid::oscOn (2), 0 },
        { pid::subOn, 1 }, { pid::subWave, 0 }, { pid::subOct, -1 }, { pid::subLevel, 0.7f },
        { pid::fltCutoff, 800 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.55f }, { pid::fltKeyTrk, 0.3f }, { pid::flt1EnvSrc, 1 },
        { pid::ampA, 3 }, { pid::ampD, 300 }, { pid::ampS, 0.8f }, { pid::ampR, 150 },
        { pid::envA (2), 2 }, { pid::envD (2), 250 }, { pid::envS (2), 0.0f }, { pid::envR (2), 150 } } });

    out.push_back ({ "Reese", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 2 }, { pid::oscDet (1), 30 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.7f }, { pid::oscUni (2), 2 }, { pid::oscDet (2), 40 },
        { pid::subOn, 1 }, { pid::subLevel, 0.4f },
        { pid::fltCutoff, 520 }, { pid::fltRes, 0.35f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.4f }, { pid::lfoRate (1), 0.3f }, { pid::lfoWave (1), 0 },
        { pid::ampA, 5 }, { pid::ampD, 400 }, { pid::ampS, 0.85f }, { pid::ampR, 200 } } });

    out.push_back ({ "Acid", "Bass", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.85f }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 600 }, { pid::fltRes, 0.6f }, { pid::fltEnvAmt, 0.6f }, { pid::fltDrive, 0.4f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 200 }, { pid::envS (2), 0.0f }, { pid::envR (2), 100 },
        { pid::ampA, 1 }, { pid::ampD, 300 }, { pid::ampS, 0.6f }, { pid::ampR, 100 },
        { pid::modSrc (1), 10 }, { pid::modDst (1), 1 }, { pid::modAmt (1), 0.5f } } });

    out.push_back ({ "Super Saw", "Lead", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.7f }, { pid::oscUni (1), 7 }, { pid::oscDet (1), 35 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 7 }, { pid::oscDet (2), 35 }, { pid::oscFine (2), 6 },
        { pid::fltCutoff, 14000 }, { pid::fltRes, 0.1f },
        { pid::ampA, 6 }, { pid::ampD, 300 }, { pid::ampS, 0.9f }, { pid::ampR, 220 } } });

    out.push_back ({ "Square Lead", "Lead", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 1 }, { pid::oscLevel (1), 0.8f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 1 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.45f },
        { pid::fltCutoff, 6000 }, { pid::fltRes, 0.2f }, { pid::fltEnvAmt, 0.3f }, { pid::flt1EnvSrc, 1 },
        { pid::ampA, 2 }, { pid::ampD, 200 }, { pid::ampS, 0.85f }, { pid::ampR, 150 } } });

    out.push_back ({ "Pluck", "Pluck", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.75f }, { pid::oscOn (2), 0 },
        { pid::fltCutoff, 3000 }, { pid::fltRes, 0.3f }, { pid::fltEnvAmt, 0.7f }, { pid::flt1EnvSrc, 1 },
        { pid::envA (2), 1 }, { pid::envD (2), 180 }, { pid::envS (2), 0.0f }, { pid::envR (2), 120 },
        { pid::ampA, 1 }, { pid::ampD, 220 }, { pid::ampS, 0.0f }, { pid::ampR, 130 } } });

    out.push_back ({ "Soft EP", "Keys", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.7f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 12 }, { pid::oscLevel (2), 0.3f },
        { pid::fltCutoff, 8000 },
        { pid::ampA, 3 }, { pid::ampD, 500 }, { pid::ampS, 0.4f }, { pid::ampR, 300 } } });

    out.push_back ({ "Warm Pad", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 20 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 25 }, { pid::oscFine (2), 6 },
        { pid::fltCutoff, 1800 }, { pid::fltRes, 0.1f },
        { pid::ampA, 800 }, { pid::ampD, 1000 }, { pid::ampS, 0.8f }, { pid::ampR, 1200 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.25f }, { pid::lfoRate (1), 0.5f }, { pid::lfoSmooth (1), 0.3f } } });

    out.push_back ({ "Glass Pad", "Pad", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 2 }, { pid::oscLevel (1), 0.6f },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 3 }, { pid::oscSemi (2), 7 }, { pid::oscLevel (2), 0.4f },
        { pid::oscOn (3), 1 }, { pid::oscWave (3), 3 }, { pid::oscSemi (3), 12 }, { pid::oscLevel (3), 0.3f },
        { pid::fltCutoff, 12000 },
        { pid::ampA, 1200 }, { pid::ampD, 800 }, { pid::ampS, 0.7f }, { pid::ampR, 1500 } } });

    out.push_back ({ "Ensemble", "Strings", V {
        { pid::oscOn (1), 1 }, { pid::oscWave (1), 0 }, { pid::oscLevel (1), 0.6f }, { pid::oscUni (1), 5 }, { pid::oscDet (1), 18 },
        { pid::oscOn (2), 1 }, { pid::oscWave (2), 0 }, { pid::oscLevel (2), 0.6f }, { pid::oscUni (2), 5 }, { pid::oscDet (2), 22 }, { pid::oscFine (2), 4 },
        { pid::fltCutoff, 6000 }, { pid::fltRes, 0.1f },
        { pid::ampA, 400 }, { pid::ampD, 600 }, { pid::ampS, 0.85f }, { pid::ampR, 500 },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 3 }, { pid::modAmt (0), 0.015f }, { pid::lfoRate (1), 5.0f } } });

    out.push_back ({ "Noise Sweep", "FX", V {
        { pid::oscOn (1), 0 }, { pid::oscOn (2), 0 },
        { pid::noiseOn, 1 }, { pid::noiseType, 1 }, { pid::noiseLevel, 0.6f },
        { pid::fltCutoff, 500 }, { pid::fltRes, 0.5f },
        { pid::modSrc (0), 1 }, { pid::modDst (0), 1 }, { pid::modAmt (0), 0.7f }, { pid::lfoRate (1), 0.2f }, { pid::lfoWave (1), 0 },
        { pid::ampA, 500 }, { pid::ampS, 1.0f }, { pid::ampR, 1000 } } });

    return out;
}

inline void applySynthPreset (juce::AudioProcessor& proc, juce::AudioProcessorValueTreeState& apvts, const SynthPreset& preset)
{
    for (auto* param : proc.getParameters())
        param->setValueNotifyingHost (param->getDefaultValue());

    for (auto& kv : preset.v)
        if (auto* p = apvts.getParameter (kv.first))
            p->setValueNotifyingHost (p->convertTo0to1 (kv.second));
}
