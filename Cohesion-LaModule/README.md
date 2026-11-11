# COHESION — LA MODULE

JUCE/VST3 instrument by **Pepperwood** — part of the [COHESION](../README.md) suite.

A multi-timbral **MT-32 / CM-32L-compatible sound module**: eight melodic parts plus a rhythm
part, each on its own MIDI channel, with the MT-32 system reverb, an echo send, a mixer and
separate outputs per part. It plays the timbres of your own Roland control + PCM ROM dumps
(never included).

<img src="screenshots/module.png" width="860" height="546" alt="COHESION - LA MODULE">

## Two engines

- **MUNT** — the [Munt](https://github.com/munt/munt) MT-32 emulator (mt32emu, LGPL 2.1+) for the
  most faithful sound. Munt is *not* part of this plugin's code: it ships as a separate
  `mt32emu.dll` next to the plugin binary (with its licence text) and is loaded at run time.
  For separate outputs every part runs in its own Munt instance; its reverb is linear, so
  the nine outputs add up to one unit (each part has 32 partials of its own).
- **COHESION LA** — the own LA engine of [COHESION — SYNTH](../Cohesion-Synth/README.md),
  calibrated against Munt; 32 partials shared by all parts like the hardware. All code is
  original.

Without `mt32emu.dll` (or for a ROM set Munt does not recognise) the module plays COHESION LA.

## Features

- **Parts 1–8 + rhythm** on MIDI channels 2–9 and 10 by default; each part's channel is
  selectable (OFF / 1–16). Program changes, controllers (volume, pan, expression, mod wheel,
  sustain, pitch bend) and **MT-32 SysEx** (timbres, patches, rhythm setup, system area,
  reset) as on the hardware. **LOAD .SYX** sends a SysEx file (e.g. a game's MT-32 setup).
- **LCD** with the selected part's timbre and program, a partial meter, RESET and PANIC.
  **LIBRARY** (or a click on a part's name) picks a timbre from group A / B.
- **Mixer** per part: level, pan, reverb and echo send, mute / solo (muted parts dim).
- **Outputs**: MAIN or the part's own stereo output (OUT 2–10, enable the extra outputs in
  the DAW).
- **Reverb**: the MT-32 system reverb (Room / Hall / Plate / Tap Delay, time and level 0–7),
  kept in sync with SysEx in both directions. **Echo**: tempo-synced send.
- **ROM sets** are shared with COHESION — SYNTH (`%APPDATA%\Pepperwood\Cohesion Synth\ROMs`);
  *Import ROMs...* in the ROM picker scans a folder (and the .zip files in it).
- Resizable (60–200 %), size and full MT-32 memory saved with the project.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

The MUNT engine needs Munt's source in `../_ref/munt` (outside git):

```bash
git clone --depth 1 https://github.com/munt/munt.git ../_ref/munt
```

Offline test (both engines, all parts, SysEx, routing, state, UI snapshots):

```bash
cmake -S tests -B build-tests
cmake --build build-tests --config Release
build-tests/ModuleTest_artefacts/Release/ModuleTest.exe "<folder with ROMs>" "<output folder>"
```

## License

© Pepperwood. All rights reserved. `mt32emu.dll` is Munt, licensed under the GNU LGPL 2.1 or
later (see `mt32emu-COPYING.LESSER.txt` in the plugin folder and https://github.com/munt/munt).
