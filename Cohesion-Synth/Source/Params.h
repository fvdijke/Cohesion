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
    static const char* fltType    = "flt_type";
    static const char* fltCutoff  = "flt_cutoff";
    static const char* fltRes     = "flt_res";
    static const char* fltDrive   = "flt_drive";
    static const char* fltKeyTrk  = "flt_keytrk";
    static const char* fltEnvAmt  = "flt_envamt";
    static const char* flt1EnvSrc = "flt1_envsrc";

    // --- filter 2 (SVF multimode) ---
    static const char* flt2Type   = "flt2_type";
    static const char* flt2Cutoff = "flt2_cutoff";
    static const char* flt2Res    = "flt2_res";
    static const char* flt2Drive  = "flt2_drive";
    static const char* flt2KeyTrk = "flt2_keytrk";
    static const char* flt2EnvAmt = "flt2_envamt";
    static const char* flt2Slope  = "flt2_slope";
    static const char* flt2EnvSrc = "flt2_envsrc";

    // --- filter routing ---
    static const char* fltRouting = "flt_routing";    // Serial / Parallel
    static const char* fltBalance = "flt_balance";    // parallel F1<->F2 mix

    // --- envelopes (1 = AMP) ---
    static const char* ampA = "amp_a", *ampD = "amp_d", *ampS = "amp_s", *ampR = "amp_r";
    inline juce::String envA (int n) { return "env" + juce::String (n) + "_a"; }
    inline juce::String envD (int n) { return "env" + juce::String (n) + "_d"; }
    inline juce::String envS (int n) { return "env" + juce::String (n) + "_s"; }
    inline juce::String envR (int n) { return "env" + juce::String (n) + "_r"; }

    // --- LFOs (n = 1..4) ---
    inline juce::String lfoWave   (int n) { return "lfo" + juce::String (n) + "_wave"; }
    inline juce::String lfoRate   (int n) { return "lfo" + juce::String (n) + "_rate"; }
    inline juce::String lfoSync   (int n) { return "lfo" + juce::String (n) + "_sync"; }
    inline juce::String lfoDiv    (int n) { return "lfo" + juce::String (n) + "_div"; }
    inline juce::String lfoFade   (int n) { return "lfo" + juce::String (n) + "_fade"; }
    inline juce::String lfoPhase  (int n) { return "lfo" + juce::String (n) + "_phase"; }
    inline juce::String lfoSmooth (int n) { return "lfo" + juce::String (n) + "_smooth"; }
    inline juce::String lfoMono   (int n) { return "lfo" + juce::String (n) + "_mono"; }

    // --- mod matrix (slot = 0..7) ---
    inline juce::String modSrc (int s) { return "mod" + juce::String (s) + "_src"; }
    inline juce::String modDst (int s) { return "mod" + juce::String (s) + "_dst"; }
    inline juce::String modAmt (int s) { return "mod" + juce::String (s) + "_amt"; }

    static const juce::StringArray modSources { "Off", "LFO 1", "LFO 2", "LFO 3", "LFO 4",
                                                "ENV 1", "ENV 2", "ENV 3", "ENV 4",
                                                "Velocity", "Mod Wheel", "Key Track" };
    static const juce::StringArray modDests   { "Off", "Filter 1 Cutoff", "Filter 2 Cutoff",
                                                "Pitch", "Amp", "Pan", "LFO 1 Rate" };

    // --- FX ---
    static const char* compOn = "fx_comp_on", *compThresh = "fx_comp_thresh", *compRatio = "fx_comp_ratio", *compAtk = "fx_comp_attack",
                     * compRel = "fx_comp_release", *compMakeup = "fx_comp_makeup", *compKnee = "fx_comp_knee", *compMix = "fx_comp_mix";
    static const char* flOn = "fx_fl_on", *flRate = "fx_fl_rate", *flDepth = "fx_fl_depth", *flFb = "fx_fl_fb",
                     * flDelay = "fx_fl_delay", *flWidth = "fx_fl_width", *flSync = "fx_fl_sync", *flMix = "fx_fl_mix";
    static const char* phOn = "fx_ph_on", *phRate = "fx_ph_rate", *phDepth = "fx_ph_depth", *phFb = "fx_ph_fb",
                     * phStages = "fx_ph_stages", *phSpread = "fx_ph_spread", *phSync = "fx_ph_sync", *phMix = "fx_ph_mix";
    static const char* ecOn = "fx_ec_on", *ecTime = "fx_ec_time", *ecSync = "fx_ec_sync", *ecFb = "fx_ec_fb",
                     * ecDamp = "fx_ec_damp", *ecWidth = "fx_ec_width", *ecPing = "fx_ec_ping", *ecMix = "fx_ec_mix";
    static const char* rvOn = "fx_rv_on", *rvSize = "fx_rv_size", *rvDecay = "fx_rv_decay", *rvPre = "fx_rv_pre",
                     * rvDamp = "fx_rv_damp", *rvWidth = "fx_rv_width", *rvLowCut = "fx_rv_lowcut", *rvMix = "fx_rv_mix";
    static const char* limOn = "fx_lim_on", *limCeil = "fx_lim_ceil", *limRel = "fx_lim_release", *limDrive = "fx_lim_drive";

    static const juce::StringArray modSyncItems  { "Free", "1/1", "1/2", "1/4", "1/8", "1/16" };
    static const juce::StringArray echoSyncItems { "Free", "1/2", "1/4", "1/4 D", "1/8", "1/8 D", "1/8 T", "1/16" };

    // --- FM engine (DX7-compatible) ---
    static const char* dxOn = "dx_on", *dxLevel = "dx_level", *dxBright = "dx_bright", *dxTime = "dx_time", *dxVel = "dx_vel";

    // --- LA engine (MT-32 / CM-32L) ---
    static const char* laOn = "la_on", *laLevel = "la_level", *laCutoff = "la_cutoff", *laTime = "la_time";

    // --- ARP ---
    static const char* arpOn = "arp_on", *arpMode = "arp_mode", *arpRate = "arp_rate", *arpOct = "arp_oct",
                     * arpGate = "arp_gate", *arpSwing = "arp_swing", *arpRandom = "arp_random", *arpSync = "arp_sync",
                     * arpHold = "arp_hold", *arpLatch = "arp_latch", *arpTempo = "arp_tempo", *arpSteps = "arp_steps",
                     * arpOrder = "arp_order", *arpVelAmt = "arp_velamt", *arpSlide = "arp_slide", *arpHuman = "arp_humanize";
    inline juce::String arpStepOn  (int i) { return "arp_s" + juce::String (i) + "_on"; }
    inline juce::String arpStepVel (int i) { return "arp_s" + juce::String (i) + "_vel"; }
    static const juce::StringArray arpModes  { "Up", "Down", "Up / Down", "Down / Up", "Random", "Chord" };
    static const juce::StringArray arpRates  { "1/4", "1/8", "1/8 T", "1/16", "1/16 T", "1/32" };
    static const juce::StringArray arpOrders { "As Played", "Low > High", "High > Low" };

    // --- global ---
    static const char* mainVol = "main_vol";
}

// ---------------------------------------------------------------------------
//  Float parameters are shown with ONE decimal:
//   * 0..1 / -1..1 ranges without a unit  -> percent      ("60.0 %")
//   * Hz >= 1000 -> kHz, ms >= 1000 -> s, "deg" -> degree sign
// ---------------------------------------------------------------------------
inline std::unique_ptr<juce::AudioParameterFloat> makeFloat (juce::ParameterID id, const juce::String& name,
                                                             juce::NormalisableRange<float> r, float def,
                                                             const juce::String& unit = {})
{
    const bool pct = unit.isEmpty() && r.start >= -1.0f && r.end <= 1.0f;
    auto toText = [pct, unit] (float v, int) -> juce::String
    {
        if (pct)                                    return juce::String (v * 100.0f, 1) + " %";
        if (unit == "Hz" && std::abs (v) >= 1000.f) return juce::String (v / 1000.0f, 1) + " kHz";
        if (unit == "ms" && std::abs (v) >= 1000.f) return juce::String (v / 1000.0f, 1) + " s";
        if (unit == "deg")                          return juce::String (v, 1) + juce::String::fromUTF8 ("\xc2\xb0");
        return juce::String (v, 1) + (unit.isNotEmpty() ? " " + unit : juce::String());
    };
    auto fromText = [pct, unit] (const juce::String& t) -> float
    {
        const auto txt = t.trim().toLowerCase();
        float v = txt.getFloatValue();
        if (txt.contains ("khz")) v *= 1000.0f;
        else if (unit == "ms" && txt.endsWith ("s") && ! txt.endsWith ("ms")) v *= 1000.0f;
        if (pct && (txt.contains ("%") || std::abs (v) > 1.0f)) v /= 100.0f;
        return v;
    };
    return std::make_unique<juce::AudioParameterFloat> (id, name, r, def,
             juce::AudioParameterFloatAttributes().withStringFromValueFunction (toText)
                                                  .withValueFromStringFunction (fromText)
                                                  .withLabel (unit == "deg" ? juce::String() : unit));
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
        p.push_back (makeFloat  (ParameterID { pid::oscFine (i), 1 }, "OSC " + String (i) + " Fine",
                                                             NormalisableRange<float> (-100.0f, 100.0f, 0.1f), 0.0f, "ct"));
        p.push_back (makeFloat  (ParameterID { pid::oscWtPos (i), 1 }, "OSC " + String (i) + " WT Pos", pct (0.0f), 0.0f));
        p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::oscUni (i), 1 }, "OSC " + String (i) + " Unison", 1, 7, 1));
        p.push_back (makeFloat  (ParameterID { pid::oscDet (i), 1 }, "OSC " + String (i) + " Detune",
                                                             NormalisableRange<float> (0.0f, 100.0f, 0.1f), 18.0f, "ct"));
        p.push_back (makeFloat  (ParameterID { pid::oscPan (i), 1 }, "OSC " + String (i) + " Pan", bipol(), 0.0f));
        p.push_back (makeFloat  (ParameterID { pid::oscLevel (i), 1 }, "OSC " + String (i) + " Level", pct (0.0f), i <= 2 ? 0.75f : 0.0f));
        p.push_back (makeFloat  (ParameterID { pid::oscPhase (i), 1 }, "OSC " + String (i) + " Phase",
                                                             NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f, "deg"));
    }

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::subOn, 1 }, "Sub On", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::subWave, 1 }, "Sub Wave", StringArray { "Sine", "Triangle", "Square" }, 0));
    p.push_back (std::make_unique<AudioParameterInt>    (ParameterID { pid::subOct, 1 }, "Sub Oct", -2, 0, -1));
    p.push_back (makeFloat  (ParameterID { pid::subLevel, 1 }, "Sub Level", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));
    p.push_back (makeFloat  (ParameterID { pid::subPan, 1 }, "Sub Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::noiseOn, 1 }, "Noise On", false));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::noiseType, 1 }, "Noise Type", StringArray { "White", "Pink" }, 0));
    p.push_back (makeFloat  (ParameterID { pid::noiseLevel, 1 }, "Noise Level", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.3f));
    p.push_back (makeFloat  (ParameterID { pid::noisePan, 1 }, "Noise Pan", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::noiseTilt, 1 }, "Noise Filter", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (makeFloat  (ParameterID { pid::fmAmt, 1 }, "FM 2>1", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::ringAmt, 1 }, "Ring 3x4", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));

    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::fltType, 1 }, "Filter Type", StringArray { "LP 24", "LP 12", "HP 24" }, 0));
    {
        NormalisableRange<float> cut (20.0f, 20000.0f, 0.0f, 0.3f);
        p.push_back (makeFloat (ParameterID { pid::fltCutoff, 1 }, "Filter Cutoff", cut, 12000.0f, "Hz"));
    }
    p.push_back (makeFloat  (ParameterID { pid::fltRes, 1 }, "Filter Res", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.12f));
    p.push_back (makeFloat  (ParameterID { pid::fltDrive, 1 }, "Filter Drive", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::fltKeyTrk, 1 }, "Filter Key Trk", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::fltEnvAmt, 1 }, "Filter Env Amt", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::flt1EnvSrc, 1 }, "Filter 1 Env Src", StringArray { "ENV 1", "ENV 2", "ENV 3", "ENV 4" }, 1));

    // filter 2 (SVF multimode)
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::flt2Type, 1 }, "Filter 2 Type", StringArray { "LP", "HP", "BP", "Notch" }, 0));
    {
        NormalisableRange<float> cut (20.0f, 20000.0f, 0.0f, 0.3f);
        p.push_back (makeFloat (ParameterID { pid::flt2Cutoff, 1 }, "Filter 2 Cutoff", cut, 20000.0f, "Hz"));
    }
    p.push_back (makeFloat  (ParameterID { pid::flt2Res, 1 }, "Filter 2 Res", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.12f));
    p.push_back (makeFloat  (ParameterID { pid::flt2Drive, 1 }, "Filter 2 Drive", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::flt2KeyTrk, 1 }, "Filter 2 Key Trk", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (makeFloat  (ParameterID { pid::flt2EnvAmt, 1 }, "Filter 2 Env Amt", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::flt2Slope, 1 }, "Filter 2 Slope", StringArray { "12 dB", "24 dB" }, 1));
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::flt2EnvSrc, 1 }, "Filter 2 Env Src", StringArray { "ENV 1", "ENV 2", "ENV 3", "ENV 4" }, 1));

    // routing
    p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::fltRouting, 1 }, "Filter Routing", StringArray { "Serial", "Parallel" }, 0));
    p.push_back (makeFloat  (ParameterID { pid::fltBalance, 1 }, "Filter Balance", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.5f));

    // envelopes (1 = AMP, uses amp_* ids; 2..4 generic)
    auto msR = [] { return NormalisableRange<float> (1.0f, 8000.0f, 0.0f, 0.3f); };
    p.push_back (makeFloat (ParameterID { pid::ampA, 1 }, "Amp Attack",  msR(), 5.0f, "ms"));
    p.push_back (makeFloat (ParameterID { pid::ampD, 1 }, "Amp Decay",   msR(), 400.0f, "ms"));
    p.push_back (makeFloat (ParameterID { pid::ampS, 1 }, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.8f));
    p.push_back (makeFloat (ParameterID { pid::ampR, 1 }, "Amp Release", msR(), 250.0f, "ms"));
    for (int n = 2; n <= 4; ++n)
    {
        const float defD = n == 2 ? 300.0f : 500.0f;
        const float defS = n == 2 ? 0.4f : 0.5f;
        p.push_back (makeFloat (ParameterID { pid::envA (n), 1 }, "ENV " + String (n) + " Attack",  msR(), 5.0f, "ms"));
        p.push_back (makeFloat (ParameterID { pid::envD (n), 1 }, "ENV " + String (n) + " Decay",   msR(), defD, "ms"));
        p.push_back (makeFloat (ParameterID { pid::envS (n), 1 }, "ENV " + String (n) + " Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), defS));
        p.push_back (makeFloat (ParameterID { pid::envR (n), 1 }, "ENV " + String (n) + " Release", msR(), 250.0f, "ms"));
    }

    // LFOs
    for (int n = 1; n <= 4; ++n)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfoWave (n), 1 }, "LFO " + String (n) + " Wave",
                                                             StringArray { "Sine", "Triangle", "Saw", "Square", "S&H" }, 0));
        p.push_back (makeFloat  (ParameterID { pid::lfoRate (n), 1 }, "LFO " + String (n) + " Rate",
                                                             NormalisableRange<float> (0.01f, 40.0f, 0.0f, 0.3f), 2.0f, "Hz"));
        p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::lfoSync (n), 1 }, "LFO " + String (n) + " Sync", false));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfoDiv (n), 1 }, "LFO " + String (n) + " Div",
                                                             StringArray { "1/1", "1/2", "1/4", "1/8", "1/16", "1/32" }, 3));
        p.push_back (makeFloat  (ParameterID { pid::lfoFade (n), 1 }, "LFO " + String (n) + " Fade",   NormalisableRange<float> (0.0f, 5000.0f, 0.0f, 0.3f), 0.0f, "ms"));
        p.push_back (makeFloat  (ParameterID { pid::lfoPhase (n), 1 }, "LFO " + String (n) + " Phase", NormalisableRange<float> (0.0f, 360.0f, 1.0f), 0.0f, "deg"));
        p.push_back (makeFloat  (ParameterID { pid::lfoSmooth (n), 1 }, "LFO " + String (n) + " Smooth", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.0f));
        p.push_back (std::make_unique<AudioParameterBool>   (ParameterID { pid::lfoMono (n), 1 }, "LFO " + String (n) + " Mono", false));
    }

    // mod matrix
    for (int s = 0; s < 8; ++s)
    {
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::modSrc (s), 1 }, "Mod " + String (s + 1) + " Src", pid::modSources, 0));
        p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { pid::modDst (s), 1 }, "Mod " + String (s + 1) + " Dst", pid::modDests, 0));
        p.push_back (makeFloat  (ParameterID { pid::modAmt (s), 1 }, "Mod " + String (s + 1) + " Amt", NormalisableRange<float> (-1.0f, 1.0f, 0.0001f), 0.0f));
    }

    // ---------------- FX ----------------
    auto fl = [&] (const char* id, const char* name, NormalisableRange<float> r, float def, const char* unit = "")
    {
        p.push_back (makeFloat (ParameterID { id, 1 }, name, r, def, unit));
    };
    auto on = [&] (const char* id, const char* name, bool def) { p.push_back (std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def)); };
    auto ch = [&] (const char* id, const char* name, const StringArray& items, int def)
    { p.push_back (std::make_unique<AudioParameterChoice> (ParameterID { id, 1 }, name, items, def)); };
    auto lin = [] (float a, float b) { return NormalisableRange<float> (a, b, 0.0f); };
    auto skw = [] (float a, float b, float s) { return NormalisableRange<float> (a, b, 0.0f, s); };

    on (pid::compOn, "Comp On", false);
    fl (pid::compThresh, "Comp Threshold", lin (-60.0f, 0.0f), -18.0f, "dB");
    fl (pid::compRatio,  "Comp Ratio",     skw (1.0f, 20.0f, 0.4f), 4.0f, ":1");
    fl (pid::compAtk,    "Comp Attack",    skw (0.1f, 100.0f, 0.35f), 10.0f, "ms");
    fl (pid::compRel,    "Comp Release",   skw (10.0f, 1000.0f, 0.4f), 150.0f, "ms");
    fl (pid::compMakeup, "Comp Makeup",    lin (0.0f, 24.0f), 0.0f, "dB");
    fl (pid::compKnee,   "Comp Knee",      lin (0.0f, 12.0f), 6.0f, "dB");
    fl (pid::compMix,    "Comp Mix",       lin (0.0f, 1.0f), 1.0f);

    on (pid::flOn, "Flanger On", false);
    fl (pid::flRate,  "Flanger Rate",     skw (0.02f, 10.0f, 0.35f), 0.3f, "Hz");
    fl (pid::flDepth, "Flanger Depth",    lin (0.0f, 1.0f), 0.6f);
    fl (pid::flFb,    "Flanger Feedback", lin (-0.95f, 0.95f), 0.5f);
    fl (pid::flDelay, "Flanger Delay",    skw (0.5f, 10.0f, 0.5f), 2.0f, "ms");
    fl (pid::flWidth, "Flanger Width",    lin (0.0f, 1.0f), 0.8f);
    ch (pid::flSync,  "Flanger Sync",     pid::modSyncItems, 0);
    fl (pid::flMix,   "Flanger Mix",      lin (0.0f, 1.0f), 0.5f);

    on (pid::phOn, "Phaser On", false);
    fl (pid::phRate,   "Phaser Rate",     skw (0.02f, 10.0f, 0.35f), 0.4f, "Hz");
    fl (pid::phDepth,  "Phaser Depth",    lin (0.0f, 1.0f), 0.7f);
    fl (pid::phFb,     "Phaser Feedback", lin (-0.95f, 0.95f), 0.5f);
    ch (pid::phStages, "Phaser Stages",   StringArray { "2", "4", "6", "8", "12" }, 1);
    fl (pid::phSpread, "Phaser Spread",   lin (0.0f, 1.0f), 0.5f);
    ch (pid::phSync,   "Phaser Sync",     pid::modSyncItems, 0);
    fl (pid::phMix,    "Phaser Mix",      lin (0.0f, 1.0f), 0.5f);

    on (pid::ecOn, "Echo On", false);
    fl (pid::ecTime,  "Echo Time",     skw (10.0f, 2000.0f, 0.4f), 375.0f, "ms");
    ch (pid::ecSync,  "Echo Sync",     pid::echoSyncItems, 0);
    fl (pid::ecFb,    "Echo Feedback", lin (0.0f, 0.95f), 0.4f);
    fl (pid::ecDamp,  "Echo Damp",     lin (0.0f, 1.0f), 0.3f);
    fl (pid::ecWidth, "Echo Width",    lin (0.0f, 1.0f), 0.7f);
    on (pid::ecPing,  "Echo PingPong", false);
    fl (pid::ecMix,   "Echo Mix",      lin (0.0f, 1.0f), 0.25f);

    on (pid::rvOn, "Reverb On", false);
    fl (pid::rvSize,   "Reverb Size",    lin (0.0f, 1.0f), 0.6f);
    fl (pid::rvDecay,  "Reverb Decay",   lin (0.0f, 1.0f), 0.5f);
    fl (pid::rvPre,    "Reverb Predelay", skw (0.0f, 250.0f, 0.5f), 20.0f, "ms");
    fl (pid::rvDamp,   "Reverb Damp",    lin (0.0f, 1.0f), 0.4f);
    fl (pid::rvWidth,  "Reverb Width",   lin (0.0f, 1.0f), 1.0f);
    fl (pid::rvLowCut, "Reverb Low Cut", skw (20.0f, 1000.0f, 0.4f), 120.0f, "Hz");
    fl (pid::rvMix,    "Reverb Mix",     lin (0.0f, 1.0f), 0.25f);

    on (pid::limOn, "Limiter On", true);
    fl (pid::limCeil,  "Limiter Ceiling", lin (-12.0f, 0.0f), -0.3f, "dB");
    fl (pid::limRel,   "Limiter Release", skw (5.0f, 1000.0f, 0.4f), 80.0f, "ms");
    fl (pid::limDrive, "Limiter Drive",   lin (0.0f, 18.0f), 0.0f, "dB");

    // ---------------- FM engine ----------------
    on (pid::dxOn, "FM On", false);
    fl (pid::dxLevel,  "FM Level",      lin (0.0f, 1.0f), 0.8f);
    fl (pid::dxBright, "FM Brightness", lin (-1.0f, 1.0f), 0.0f);
    fl (pid::dxTime,   "FM Env Time",   skw (0.25f, 4.0f, 0.5f), 1.0f, "x");
    fl (pid::dxVel,    "FM Velocity",   lin (0.0f, 2.0f), 1.0f, "x");

    // ---------------- LA engine ----------------
    on (pid::laOn, "LA On", false);
    fl (pid::laLevel,  "LA Level",  lin (0.0f, 1.0f), 0.8f);
    fl (pid::laCutoff, "LA Cutoff", lin (-1.0f, 1.0f), 0.0f);
    fl (pid::laTime,   "LA Env Time", skw (0.25f, 4.0f, 0.5f), 1.0f, "x");

    // ---------------- ARP ----------------
    on (pid::arpOn, "Arp On", false);
    ch (pid::arpMode,  "Arp Mode",  pid::arpModes, 0);
    ch (pid::arpRate,  "Arp Rate",  pid::arpRates, 3);
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::arpOct, 1 }, "Arp Octaves", 1, 4, 1));
    fl (pid::arpGate,   "Arp Gate",   lin (0.05f, 1.0f), 0.7f);
    fl (pid::arpSwing,  "Arp Swing",  lin (0.0f, 0.75f), 0.0f);
    fl (pid::arpRandom, "Arp Random", lin (0.0f, 1.0f), 0.0f);
    on (pid::arpSync,  "Arp Sync",  true);
    on (pid::arpHold,  "Arp Hold",  false);
    on (pid::arpLatch, "Arp Latch", false);
    fl (pid::arpTempo, "Arp Tempo", lin (40.0f, 240.0f), 120.0f, "BPM");
    p.push_back (std::make_unique<AudioParameterInt> (ParameterID { pid::arpSteps, 1 }, "Arp Steps", 1, 16, 16));
    ch (pid::arpOrder, "Arp Note Order", pid::arpOrders, 1);
    fl (pid::arpVelAmt, "Arp Vel Amount", lin (0.0f, 1.0f), 1.0f);
    fl (pid::arpSlide,  "Arp Slide",      lin (0.0f, 1.0f), 0.0f);
    fl (pid::arpHuman,  "Arp Humanize",   lin (0.0f, 1.0f), 0.0f);
    for (int i = 0; i < 16; ++i)
    {
        p.push_back (std::make_unique<AudioParameterBool>  (ParameterID { pid::arpStepOn (i), 1 },  "Arp Step " + String (i + 1) + " On", true));
        p.push_back (makeFloat (ParameterID { pid::arpStepVel (i), 1 }, "Arp Step " + String (i + 1) + " Vel",
                                                            NormalisableRange<float> (0.0f, 1.0f, 0.0f), 1.0f));
    }

    p.push_back (makeFloat (ParameterID { pid::mainVol, 1 }, "Main", NormalisableRange<float> (0.0f, 1.0f, 0.0001f), 0.8f));

    return { p.begin(), p.end() };
}
