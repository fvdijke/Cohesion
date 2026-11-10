#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// ===========================================================================
//  Parameter IDs + layout for COHESION - SYNTH (foundation set).
//  Covers: 4 oscillators, sub, noise, mixer routing, one ladder filter,
//  amp & filter ADSR envelopes, main volume. Filter 2 / LFOs / mod-matrix /
//  FX / arp are added in later stages.
// ===========================================================================
namespace pid
{
    // --- oscillator (i = 1..4) ---
    inline juce::String oscOn    (int i) { return "osc" + juce::String (i) + "_on"; }
    inline juce::String oscMode  (int i) { return "osc" + juce::String (i) + "_mode"; }
    inline juce::String oscWave  (int i) { return "osc" + juce::String (i) + "_wave"; }
    inline juce::String oscOct   (int i) { return "osc" + juce::String (i) + "_oct"; }
    inline juce::String oscSemi  (int i) { return "osc" + juce::String (i) + "_semi"; }
    inline juce::String oscFine  (int i) { return "osc" + juce::String (i) + "_fine"; }
    inline juce::String oscWtPos (int i) { return "osc" + juce::String (i) + "_wtpos"; }
    inline juce::String oscUni   (int i) { return "osc" + juce::String (i) + "_unison"; }
    inline juce::String oscDet   (int i) { return "osc" + juce::String (i) + "_detune"; }
    inline juce::String oscPan   (int i) { return "osc" + juce::String (i) + "_pan"; }
    inline juce::String oscLevel (int i) { return "osc" + juce::String (i) + "_level"; }
    inline juce::String oscPhase (int i) { return "osc" + juce::String (i) + "_phase"; }

    // --- sub / noise ---
    static const char* subOn    = "sub_on";
    static const char* subWave  = "sub_wave";
    static const char* subOct   = "sub_oct";
    static const char* subLevel = "sub_level";
    static const char* subPan   = "sub_pan";
    static const char* noiseOn    = "noise_on";
    static const char* noiseType  = "noise_type";
    static const char* noiseLevel = "noise_level";
    static const char* noisePan   = "noise_pan";
    static const char* noiseTilt  = "noise_filter";

    // --- mixer routing ---
    static const char* fmAmt   = "fm_amount";    // osc2 -> osc1
    static const char* ringAmt = "ring_amount";  // osc3 x osc4

    // --- filter (ladder, stage 1) ---
    static const char* fltType   = "flt_type";
    static const char* fltCutoff = "flt_cutoff";
    static const char* fltRes    = "flt_res";
    static const char* fltDrive  = "flt_drive";
    static const char* fltKeyTrk = "flt_keytrk";
    static const char* fltEnvAmt = "flt_envamt";

    // --- envelopes ---
    static const char* ampA = "amp_a", *ampD = "amp_d", *ampS = "amp_s", *ampR = "amp_r";
    static const char* fenA = "fenv_a", *fenD = "fenv_d", *fenS = "fenv_s", *fenR = "fenv_r";

    // --- global ---
    static const char* mainVol = "main_vol";
}

inline juce::AudioProcessorValueTreeState::ParameterLayout cohesionSynthLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;

    auto pct    = [] (float) { return NormalisableRange<float> (0.0f, 1.0f, 0.0001f); };
    auto bipol  = []         { return NormalisableRange<float> (-1.0f, 1.0f, 0.0001f); };

    for (int i = 1; i <= 4; ++i)
    {
        const bool onByDefault = (i <= 2);
        p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::oscOn (i), 1 }, "OSC " + String (i) + " On", onByDefault));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::oscMode (i), 1 }, "OSC " + String (i) + " Mode",
                                                             StringArray { "VA", "WT" }, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::oscWave (i), 1 }, "OSC " + String (i) + " Wave",
                                                             StringArray { "Saw", "Square", "Triangle", "Sine" }, i == 3 ? 1 : 0));
        p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::oscOct (i), 1 }, "OSC " + String (i) + " Oct", -2, 2, 0));
        p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::oscSemi (i), 1 }, "OSC " + String (i) + " Semi", -12, 12, 0));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscFine (i), 1 }, "OSC " + String (i) + " Fine",
                                                             NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscWtPos (i), 1 }, "OSC " + String (i) + " WT Pos", pct (0.0f), 0.0f));
        p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::oscUni (i), 1 }, "OSC " + String (i) + " Unison", 1, 7, 1));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscDet (i), 1 }, "OSC " + String (i) + " Detune",
                                                             NormalisableRange<float> (0.0f, 100.0f, 0.1f), 18.0f));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscPan (i), 1 }, "OSC " + String (i) + " Pan", bipol(), 0.0f));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscLevel (i), 1 }, "OSC " + String (i) + " Level", pct (0.0f), i <= 2 ? 0.75f : 0.0f));
        p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::oscPhase (i), 1 }, "OSC " + String (i) + " Phase",
                                                             NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f));
    }

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::subOn, 1 }, "Sub On", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::subWave, 1 }, "Sub Wave", StringArray { "Sine", "Triangle", "Square" }, 0));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::subOct, 1 }, "Sub Oct", -2, 0, -1));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::subLevel, 1 }, "Sub Level", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::subPan, 1 }, "Sub Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::noiseOn, 1 }, "Noise On", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::noiseType, 1 }, "Noise Type", StringArray { "White", "Pink" }, 0));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::noiseLevel, 1 }, "Noise Level", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.3f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::noisePan, 1 }, "Noise Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::noiseTilt, 1 }, "Noise Filter", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::fmAmt, 1 }, "FM 2>1", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::ringAmt, 1 }, "Ring 3x4", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::fltType, 1 }, "Filter Type", StringArray { "LP 24", "LP 12", "HP 24" }, 0));
    {
        NormalisableRange<float> cut (20.0f, 20000.0f, 0.0f, 0.3f);
        p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::fltCutoff, 1 }, "Filter Cutoff", cut, 12000.0f));
    }
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::fltRes, 1 }, "Filter Res", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.12f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::fltDrive, 1 }, "Filter Drive", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::fltKeyTrk, 1 }, "Filter Key Trk", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterFloat>  (ParameterID { pid::fltEnvAmt, 1 }, "Filter Env Amt", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::ampA, 1 }, "Amp Attack",  NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 5.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::ampD, 1 }, "Amp Decay",   NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 400.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::ampS, 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.8f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::ampR, 1 }, "Amp Release", NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 250.0f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::fenA, 1 }, "Filter Attack",  NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 5.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::fenD, 1 }, "Filter Decay",   NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 300.0f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::fenS, 1 }, "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.4f));
    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::fenR, 1 }, "Filter Release", NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f), 250.0f));

    p.push_back (std::make_unique<AudioParameterFloat> (ParameterID { pid::mainVol, 1 }, "Main", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.8f));

    return { p.begin(), p.end() };
}
