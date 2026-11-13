# COHESION — SYNTH

Original JUCE/VST3 instrument by **Pepperwood** — part of the [COHESION](../README.md) suite.

A polyphonic hybrid synthesizer: four virtual-analog oscillators plus a **DX7-compatible
6-operator FM engine** and an **MT-32 / CM-32L-compatible LA engine**, two filters, a modulation matrix, an FX chain and a programmable
arpeggiator, in the Cohesion house style. All DSP is original code.

<img src="screenshots/osc-mix.png" width="860" height="520" alt="COHESION - SYNTH · OSC / MIX">

## Sound sources — OSC / MIX and FM

- **4 oscillators**, each **VA** (band-limited saw / square / triangle / sine) or **WT**
  (wavetable: 12 banks — Analog, PWM, Harmonic, Vocal, Sync, FM, Fold, Organ, Odd/Even, Comb,
  Chip, Digital — of 8 frames each, picked with the BANK box, morphed with WT POS, equal
  loudness per frame and band-limited per octave), with unison & detune, octave / semitone / fine, pan,
  level and phase; **SUB** and **NOISE** (white / pink, with a dark ↔ thin FILTER);
  **FM 2 → 1** and **ring 3 × 4**; a mixer with faders per source.
- **VOICE** POLY / MONO / LEGATO (last-note priority) with **GLIDE**, **VELOCITY** sensitivity,
  pitch bend (± 2 semitones), 16 voices; a stolen voice fades out instead of clicking.
- **FM engine (DX7-compatible)**: 6 operators, all 32 algorithms, 4-rate / 4-level operator
  envelopes, ratio or fixed frequency, detune, feedback, velocity and keyboard scaling,
  pitch EG and LFO. Macros: level, brightness, envelope time and velocity.

<img src="screenshots/fm.png" width="860" height="520" alt="COHESION - SYNTH · FM">

- **LA engine (MT-32 / CM-32L-compatible)**: plays the timbres of your own Roland control + PCM
  ROM dumps (never included) — up to 4 partials per timbre (square/saw with pulse width and a
  resonant filter, or PCM samples from the ROM), pitch envelope and LFO, amp envelope and the
  13 partial-pair structures including ring modulation. Macros: level, cutoff, envelope time.
  **IMPORT ROMS** scans a folder (and the .zip files in it), pairs control and PCM ROMs,
  skips versions with identical sound and lists each set as `<set> · A / B / Rhythm`.

## FILTER / MOD

Ladder filter (LP 24 / LP 12 / HP 24, zero-delay feedback: the resonance stays on the cutoff
at every frequency, bass compensation at high resonance) and a state-variable filter (LP / HP /
BP / notch, 12 / 24 dB with the same resonance peak, DRIVE = tanh saturation) in series or
parallel; four envelopes with analog (RC) curves, four LFOs (free or tempo-synced, per voice or
MONO = one shared LFO; SMOOTH rounds the shape relative to the LFO period) and an 8-slot mod
matrix (LFO / envelope / velocity / mod wheel / key track → cutoff, pitch,
amp, pan, LFO rate, WT position). Levels and cutoffs are smoothed; filter coefficients and
pitch are computed at control rate.

## FX

Compressor, flanger, phaser, echo (ping-pong, tempo sync) and an 8-line FDN reverb in a
**drag-and-drop signal chain**, followed by a lookahead limiter. Effects switch on and off
without clicks.

## ARP

Arpeggiator with a 16-step velocity sequencer: up / down / up-down / down-up / random /
chord, 1–4 octaves, rate with triplets, gate, swing, random, humanize, slide, hold & latch,
host-grid sync or free tempo, and 16 step patterns.

<img src="screenshots/arp.png" width="860" height="520" alt="COHESION - SYNTH · ARP">

## MIXER — multi sounds

The three engines are three **parts** of one sound: **VA** (OSC / MIX with FILTER / MOD),
**FM** (DX7 voice) and **LA** (MT-32 / CM-32L timbre). Each part loads its own sound —
**LOAD VA...** on OSC / MIX, **LOAD / IMPORT** on the FM and LA tabs, or **LOAD...** on the
MIXER — so a pad, an FM piano and an LA bass can play together. Loading into a part leaves
the other parts, the MIXER, FX and ARP as they are, and sets the part's level to the
preset's matched loudness.

The **MIXER** tab has a strip per part: level fader with meter, pan, **ECHO** and **REVERB**
sends, **KEY LOW / KEY HIGH** (splits), **ARP** (on: the part plays the ARP's notes, off: the
keys you press), **ON**, **VA FILT+MOD** (FM / LA through the VA part's filters and its mod
matrix: pitch, amp, pan — off, they follow only bend and glide; on the VA part: off = the
oscillators go dry, AMP envelope only), **MUTE** and **SOLO**;
plus the echo / reverb returns, the limiter and MAIN. COMP, FLANGER and PHASER run on every
part; ECHO and REVERB are send effects whose MIX acts on what is sent. With the default
settings (sends 100 %, full keyboard) a single sound plays exactly as before. A multi sound
is saved whole as a **User** preset; a User preset's FM, LA or VA part can also be loaded alone.
VELOCITY in the header shapes the VA part; the DX7 and LA engines use their own velocity
response. Keys that no part plays (outside every key range) take no voice. Pitch bend, mod
wheel and sustain reach both the ARP's notes and the played ones.

## Presets and DX7 SysEx

- **1,229 factory presets**: 100 per category (Arp, Bass, Drums, Flute, FX, Keys, Lead, Pad,
  Pluck, Strings, Synth), Init, and a **General MIDI bank**: the 128 GM programs (`001 Acoustic
  Grand Piano` … `128 Gunshot`) in the 16 GM families. A MIDI program change loads the matching
  GM preset (one patch plays all keys, so the GM drum kits are not covered). Every preset was
  rendered offline and loudness-matched.
- **LIBRARY** browser with categories, presets and a **search** field (every word must match
  the name or category). Loading a preset opens the tab of its engine (FM for a DX7 voice,
  LA for an MT-32 / CM-32L timbre, otherwise OSC / MIX).
- **SAVE** stores the current sound as a **User** preset (parameters, FX order, DX7 voice,
  LA timbre, ARP pattern) in `%APPDATA%\Pepperwood\Cohesion Synth\Presets` (`.cspreset`);
  the User category in the LIBRARY has **RENAME** and **DELETE** (to the recycle bin), also
  on right-click.
- **IMPORT .SYX / IMPORT FOLDER** loads Yamaha DX7 banks (32-voice bulk dumps and single
  voices). Banks are kept in `%APPDATA%\Pepperwood\Cohesion Synth\SysEx` and appear as
  `DX7 · <bank>` categories; blank slots and duplicates are skipped. Imported voices are
  loudness-matched when loaded. **REMOVE BANK** (or right-click a bank) takes a bank out of
  the library again; the original files are never touched.
- New DX7 banks can be assembled with [COHESION — SYSEX](../Cohesion-SysEx/README.md).

The editor is resizable (60–200 %, fixed aspect ratio) and remembers its size.

## Build

```bash
cmake -S . -B build
cmake --build build --config Release
```

Offline tests (FX, arpeggiator, FM engine, LA engine, presets, loudness, UI snapshots;
`VoiceTest`: pitch bend, voice stealing, HP 24, noise filter, LFO sync, envelopes, legato,
wavetables, CPU; `SwitchTest <bank.syx>`: FM + VA layer, click-free preset changes, imported
loudness, removing a bank, user presets, multi sounds; `AuditCheck [bank.syx]`: measured
checks of the oscillators, filters, LFO, echo, header state and multi-sound routing).
`LaCompare` is only built when Munt has been cloned into `../_ref/munt` (outside git): a local
measuring tool that renders the same MT-32 timbres with Munt and with the LA engine. Munt is
never part of the plugin.

```bash
cmake -S tests -B build-tests
cmake --build build-tests --config Release
```

## License

© Pepperwood. All rights reserved.
