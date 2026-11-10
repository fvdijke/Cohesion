# COHESION

**COHESION** is a suite of four original JUCE/VST3 audio plugins by **Pepperwood**,
sharing one dark/teal house style and a consistent workflow. All DSP is original code.

| Plugin | What it does |
|--------|--------------|
| **COHESION — GLUE** | Master-bus "glue" compressor for cohesion on the mix bus. |
| **COHESION — EQ/COMP** | Parametric EQ (TPT/ZDF) + flexible multiband dynamics + brick-wall limiter, with an **ASSIST** engine that listens to the signal and proposes EQ/comp settings per instrument & style. |
| **COHESION — ADT** | **Artificial Double Tracking** — the Abbey Road trick: a modulated, delayed double of a vocal to make it thicker and wider. |
| **COHESION — TRANSIENT** | Four-control transient shaper (attack, sustain, sensitivity, output) — add punch or soften attacks. |

## Plugins

### COHESION — ADT
Artificial Double Tracking: a short modulated delay (≈15–35 ms, no feedback) with subtle
LFO pitch-wobble, phase controls, and a WIDTH that separates the original and the double
from centred (thickening) to hard left/right. Factory presets named after classic
double-tracked vocals (Lennon Wide, Abbey Road '66, Tomorrow Never…, Elliott Smith, …).

<img src="Cohesion-ADT/screenshots/adt.png" width="860" height="520" alt="COHESION - ADT">

### COHESION — TRANSIENT
A four-control transient shaper using a differential-envelope detector: **Attack** and
**Sustain** (±dB) reshape the onset and the body, **Sensitivity** sets how strongly the
detector reacts, and **Output** is makeup. Stereo-linked, zero-latency, with a live
rounded input-vs-shaped display and factory presets (Punch Up, Snap, Soften, Drum Smack, …).

<img src="Cohesion-Transient/screenshots/transient.png" width="860" height="520" alt="COHESION - TRANSIENT">

### COHESION — EQ/COMP
6-band parametric EQ + HPF/LPF, 2–5 band multiband dynamics, output limiter, live
before/after spectrum, and the ASSIST analysis engine.

<img src="Cohesion-EQComp/screenshots/assist.png" width="860" height="520" alt="COHESION - EQ/COMP · ASSIST">
<img src="Cohesion-EQComp/screenshots/multiband.png" width="860" height="520" alt="COHESION - EQ/COMP · MULTIBAND">
<img src="Cohesion-EQComp/screenshots/eq.png" width="860" height="520" alt="COHESION - EQ/COMP · EQ">

### COHESION — GLUE
Master-bus glue compressor (the first plugin of the suite).

<img src="Cohesion-Glue/screenshots/glue.png" width="860" height="520" alt="COHESION - GLUE">

## Repository layout

```
COHESION/
├─ Cohesion-Glue/     # COHESION - GLUE      (VST3 "Cohesion Glue")
├─ Cohesion-EQComp/   # COHESION - EQ/COMP   (VST3 "Cohesion EQ-Comp")
├─ Cohesion-ADT/       # COHESION - ADT       (VST3 "Cohesion ADT")
├─ Cohesion-Transient/ # COHESION - TRANSIENT (VST3 "Cohesion Transient")
├─ Cohesion-Synth/     # COHESION - SYNTH     (design mockups only, not yet built)
└─ installer/         # Inno Setup script for the combined Windows installer
```

## Build

Each plugin builds on its own with CMake; JUCE 7.0.12 is fetched automatically on the
first configure (one plugin's fetched JUCE is reused by the others, so build one first).

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run this inside a plugin folder (e.g. `Cohesion-ADT`). The built `.vst3` lands in
`build/<Target>_artefacts/Release/VST3/`. On Windows, copy it to
`C:\Program Files\Common Files\VST3\`.

## Installer

A combined Windows installer is built from `installer/Cohesion.iss` with Inno Setup 6
(`ISCC.exe installer/Cohesion.iss`). It lets you choose which plugin(s) to install and
the location (defaults to the VST3 folder). Pre-built installers are attached to the
[releases](../../releases).

## License

© Pepperwood. All rights reserved.
