# FluidSynth as a plain shared library: no audio / MIDI drivers, no network, no extra libraries
# (the DAW does audio and MIDI; the C++11 OS layer replaces GLib).
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)
foreach(opt libsndfile dbus jack pulseaudio pipewire alsa sdl3 readline network ladspa midishare oss
            dsound wasapi waveout winmidi openmp portaudio aufile signalsmith native-dls systemd)
    set(enable-${opt} OFF CACHE BOOL "" FORCE)
endforeach()
set(osal "cpp11" CACHE STRING "" FORCE)
add_subdirectory("${FLUID_DIR}" fluidsynth EXCLUDE_FROM_ALL)
