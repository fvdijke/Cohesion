#pragma once

#include <juce_core/juce_core.h>

// ===========================================================================
//  FluidSynth (LGPL 2.1+, https://github.com/FluidSynth/fluidsynth) is NOT part
//  of this plugin's code: it ships as the separate libfluidsynth-3.dll next to
//  the plugin binary and is loaded at run time through its public C API.
// ===========================================================================
namespace fluid
{
    using Settings = void; using Synth = void; using Voice = void;

    struct Lib
    {
        juce::DynamicLibrary dll;
        bool ok = false;
        juce::String error;

        Settings* (*newSettings)() = nullptr;
        void      (*deleteSettings) (Settings*) = nullptr;
        int       (*setInt) (Settings*, const char*, int) = nullptr;
        int       (*setNum) (Settings*, const char*, double) = nullptr;
        Synth*    (*newSynth) (Settings*) = nullptr;
        void      (*deleteSynth) (Synth*) = nullptr;
        int       (*sfload) (Synth*, const char*, int) = nullptr;
        int       (*sfunload) (Synth*, int, int) = nullptr;
        int       (*programSelect) (Synth*, int, int, int, int) = nullptr;
        int       (*noteOn) (Synth*, int, int, int) = nullptr;
        int       (*noteOff) (Synth*, int, int) = nullptr;
        int       (*cc) (Synth*, int, int, int) = nullptr;
        int       (*pitchBend) (Synth*, int, int) = nullptr;
        int       (*channelPressure) (Synth*, int, int) = nullptr;
        int       (*keyPressure) (Synth*, int, int, int) = nullptr;
        int       (*sysex) (Synth*, const char*, int, char*, int*, int*, int) = nullptr;
        int       (*allNotesOff) (Synth*, int) = nullptr;
        int       (*allSoundsOff) (Synth*, int) = nullptr;
        int       (*systemReset) (Synth*) = nullptr;
        int       (*process) (Synth*, int, int, float**, int, float**) = nullptr;
        void      (*getVoiceList) (Synth*, Voice**, int, int) = nullptr;
        int       (*voiceChannel) (const Voice*) = nullptr;
        int       (*voicePlaying) (const Voice*) = nullptr;
        int       (*setReverbRoom) (Synth*, int, double) = nullptr;
        int       (*setReverbDamp) (Synth*, int, double) = nullptr;
        int       (*setReverbWidth) (Synth*, int, double) = nullptr;
        int       (*setReverbLevel) (Synth*, int, double) = nullptr;
        int       (*setChorusNr) (Synth*, int, int) = nullptr;
        int       (*setChorusLevel) (Synth*, int, double) = nullptr;
        int       (*setChorusSpeed) (Synth*, int, double) = nullptr;
        int       (*setChorusDepth) (Synth*, int, double) = nullptr;
        void*     (*setLogFunction) (int, void*, void*) = nullptr;

        static juce::File dllFile()
        {
            return juce::File::getSpecialLocation (juce::File::currentExecutableFile).getSiblingFile ("libfluidsynth-3.dll");
        }
        static Lib& get() { static Lib lib; return lib; }

    private:
        Lib()
        {
            const auto f = dllFile();
            if (! f.existsAsFile()) { error = "libfluidsynth-3.dll not found next to the plugin"; return; }
            if (! dll.open (f.getFullPathName())) { error = "libfluidsynth-3.dll could not be loaded"; return; }
            bool all = true;
            auto bind = [&] (auto& fn, const char* name)
            {
                fn = reinterpret_cast<std::remove_reference_t<decltype (fn)>> (dll.getFunction (name));
                if (fn == nullptr) all = false;
            };
            bind (newSettings, "new_fluid_settings");          bind (deleteSettings, "delete_fluid_settings");
            bind (setInt, "fluid_settings_setint");            bind (setNum, "fluid_settings_setnum");
            bind (newSynth, "new_fluid_synth");                bind (deleteSynth, "delete_fluid_synth");
            bind (sfload, "fluid_synth_sfload");               bind (sfunload, "fluid_synth_sfunload");
            bind (programSelect, "fluid_synth_program_select");
            bind (noteOn, "fluid_synth_noteon");               bind (noteOff, "fluid_synth_noteoff");
            bind (cc, "fluid_synth_cc");                       bind (pitchBend, "fluid_synth_pitch_bend");
            bind (channelPressure, "fluid_synth_channel_pressure"); bind (keyPressure, "fluid_synth_key_pressure");
            bind (sysex, "fluid_synth_sysex");
            bind (allNotesOff, "fluid_synth_all_notes_off");   bind (allSoundsOff, "fluid_synth_all_sounds_off");
            bind (systemReset, "fluid_synth_system_reset");
            bind (process, "fluid_synth_process");
            bind (getVoiceList, "fluid_synth_get_voicelist");
            bind (voiceChannel, "fluid_voice_get_channel");    bind (voicePlaying, "fluid_voice_is_playing");
            bind (setReverbRoom, "fluid_synth_set_reverb_group_roomsize");
            bind (setReverbDamp, "fluid_synth_set_reverb_group_damp");
            bind (setReverbWidth, "fluid_synth_set_reverb_group_width");
            bind (setReverbLevel, "fluid_synth_set_reverb_group_level");
            bind (setChorusNr, "fluid_synth_set_chorus_group_nr");
            bind (setChorusLevel, "fluid_synth_set_chorus_group_level");
            bind (setChorusSpeed, "fluid_synth_set_chorus_group_speed");
            bind (setChorusDepth, "fluid_synth_set_chorus_group_depth");
            bind (setLogFunction, "fluid_set_log_function");
            ok = all;
            if (! ok) { error = "libfluidsynth-3.dll is missing functions (unsupported version)"; return; }
            for (int level = 0; level < 5; ++level) setLogFunction (level, nullptr, nullptr);   // quiet
        }
    };
}
