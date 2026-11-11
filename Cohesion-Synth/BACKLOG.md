# COHESION — SYNTH · backlog

## Presets
- [ ] **50 fabriekspresets** — nu 42 in 12 categorieën (Init, Arp, Bass, Drums,
  Flute, FX, Keys, Lead, Pad, Pluck, Strings, Synth); nog 8 te gaan.
- [x] LIBRARY-venster (categorieën → presets) naast de preset-kiezer.
- [x] 16 ARP-patronen (step-sequencer presets).
- [ ] **Presets voor alle General MIDI-instrumenten** (de 128 GM-programma's),
  zodat de synth als GM-bank bruikbaar is. Pas na een werkende engine (osc +
  filter + env + FX), want veel GM-klanken (drums, strings, flute) leunen op FX
  en specifieke oscillator-/filterinstellingen.

## Engine (gefaseerd)
- [x] Fundament: 4 VA-oscillators + unison, sub, noise, mixer/FM/ring,
  ZDF-ladderfilter, amp/filter-ADSR, polyfonie.
- [x] Filter 2 (SVF multimode) + serie/parallel-routing.
- [x] 4 LFO's + 4 envelopes + mod-matrix (8 slots).
- [ ] Wavetable-modus (WT) met WT POS-morph.
- [x] FX-keten (comp/flanger/phaser/echo/reverb + limiter), versleepbare volgorde,
  klikvrij aan/uit, tempo-sync; offline getest met `tests/FxTest.cpp`.
- [x] Arpeggiator + 16-staps velocity-sequencer (host-sync, swing, hold/latch,
  slide, humanize); offline getest met `tests/ArpTest.cpp`.

## GUI
- [x] MIDI-indicator in de header (zichtbaar op elke tab) — tijdelijke controle.
- [x] FILTER/MOD-tab.
- [x] FX-tab.
- [x] ARP-tab.
- [x] Limiter-paneel (drive / ceiling / release + GR-meter); knoppen dimmen bij FX uit.
