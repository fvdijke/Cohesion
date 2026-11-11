# COHESION

**COHESION** is a suite of original JUCE audio tools by **Pepperwood** — four VST3 effects,
a VST3 synthesizer and a standalone DX7 bank builder — sharing one dark/teal house style and a
consistent workflow. All DSP is original code.

| Plugin | What it does |
|--------|--------------|
| **COHESION — GLUE** | Master-bus "glue" compressor for cohesion on the mix bus. |
| **COHESION — EQ/COMP** | Parametric EQ (TPT/ZDF) + flexible multiband dynamics + brick-wall limiter, with an **ASSIST** engine that listens to the signal and proposes EQ/comp settings per instrument & style. |
| **COHESION — ADT** | **Artificial Double Tracking** — the Abbey Road trick: a modulated, delayed double of a vocal to make it thicker and wider. |
| **COHESION — TRANSIENT** | Four-control transient shaper (attack, sustain, sensitivity, output) — add punch or soften attacks. |
| **COHESION — SYNTH** | Polyphonic hybrid synth: 4 VA oscillators + a DX7-compatible 6-operator FM engine, 2 filters, mod matrix, FX chain, arpeggiator, 1,101 presets and DX7 SysEx import. |
| **COHESION — SYSEX** | Standalone app that builds new DX7 32-voice SysEx banks from voices picked out of a `.syx` collection. |

## Plugins

### COHESION — SYNTH
A polyphonic hybrid instrument: four virtual-analog oscillators, sub and noise, plus a
**DX7-compatible 6-operator FM engine** (all 32 algorithms); ladder + state-variable filters,
four envelopes, four LFOs and an 8-slot mod matrix; a drag-and-drop FX chain (comp, flanger,
phaser, echo, reverb) with a limiter; and an arpeggiator with a 16-step velocity sequencer.
1,101 loudness-matched factory presets in a LIBRARY browser, and DX7 SysEx banks can be
imported. See [Cohesion-Synth](Cohesion-Synth/README.md).

<img src="Cohesion-Synth/screenshots/osc-mix.png" width="860" height="520" alt="COHESION - SYNTH · OSC / MIX">
<img src="Cohesion-Synth/screenshots/fm.png" width="860" height="520" alt="COHESION - SYNTH · FM">

### COHESION — SYSEX
Standalone DX7 bank builder: index a `.syx` collection, search and audition voices through
the FM engine, drag up to 32 voices into a bank and export a standard bulk dump that loads
on a DX7, Dexed or MiniDexed. See [Cohesion-SysEx](Cohesion-SysEx/README.md).

<img src="Cohesion-SysEx/screenshots/sysex.png" width="860" height="520" alt="COHESION - SYSEX">

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
├─ Cohesion-Synth/     # COHESION - SYNTH     (VST3 instrument "Cohesion Synth")
├─ Cohesion-SysEx/     # COHESION - SYSEX     (standalone DX7 bank builder, see its README)
└─ installer/         # Inno Setup script for the combined Windows installer
```

## Build

Each plugin builds on its own with CMake; JUCE 7.0.12 is fetched automatically on the
first configure (one plugin's fetched JUCE is reused by the others, so build one first).

```bash
cmake -S . -B build
cmake --build build --config Release
```

Run this inside a project folder (e.g. `Cohesion-ADT`). A built `.vst3` lands in
`build/<Target>_artefacts/Release/VST3/`; on Windows, copy it to
`C:\Program Files\Common Files\VST3\`. COHESION — SYSEX builds a standalone app.

## Installer

A combined, branded Windows installer is built from `installer/Cohesion.iss` with
Inno Setup 6 (`ISCC.exe installer/Cohesion.iss`). It lets you choose which plugin(s) and apps
to install and the plugin location (defaults to the VST3 folder); COHESION — SYSEX goes to
`Program Files\Pepperwood` with a Start-menu shortcut. Download the latest pre-built
installer from the [releases](../../releases/latest).

## License

© Pepperwood. All rights reserved.
