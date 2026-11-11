# COHESION — SYSEX · DX7 Bank Builder

Standalone app that builds new Yamaha DX7 32-voice SysEx banks from voices picked out of an
existing `.syx` collection (for example the MiniDexed `sysex/voice` folder). It shares the DX7
parser/packer and the FM engine with COHESION — SYNTH (`../Cohesion-Synth/Source/Dx7.h`).

## Workflow

1. **COLLECTION…**: pick the folder with your `.syx` files (scanned recursively, remembered).
   32-voice banks (VMEM), single voices (VCED) and unframed 32 × VCED dumps are read. Blank
   `----------` slots and `INIT VOICE`s are skipped.
2. **Find voices**: search by voice name, bank file or folder (several words = all must match),
   filter by top-level folder, sort by collection order, name or most duplicated.
   **UNIQUE ONLY** merges identical voices (same data and name); the `×N` badge shows how often
   a voice occurs, the tooltip lists where.
3. **Preview**: clicking a voice (or moving with the arrow keys) plays a note (C2–C5, top bar)
   through the FM engine; play it from a MIDI keyboard (**AUDIO / MIDI**) or the on-screen keys.
   The panel below shows the algorithm and all operator parameters.
4. **Build the bank**: double-click / Enter / **ADD TO BANK** fills empty slots; drag one or more
   voices onto a slot to place them there; drag slots to swap; **UP/DOWN**, **RENAME** (double-click,
   F2), **CLEAR SLOT** (Del), **COMPACT**. **OPEN .SYX** loads an existing bank to edit.
5. **EXPORT .SYX** writes a standard 4104-byte bulk dump (`F0 43 00 09 20 00` + 4096 bytes +
   checksum + `F7`). Empty slots become `INIT VOICE`. The file is read back and verified after
   writing.

Exported voices are normalised the way a DX7 reads them: unused bits cleared, out-of-range values
clamped, non-printable name characters turned into spaces.

## Build

```
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

Optional `-DCOHESION_JUCE_DIR=<JUCE 7.0.12 source>`; otherwise a sibling project's
`build/_deps/juce-src` is reused or JUCE is fetched.

## Tests

`BankTest <collection-folder> [out.syx]`: unpack/pack stability for every bank in the collection,
a bank assembled from 32 different files (size, header, checksum, voice-by-voice comparison after
parsing), INIT VOICE padding and extreme parameter values.

`"Cohesion SysEx.exe" --snapshot out.png <collection-folder>` renders the UI to a PNG.
