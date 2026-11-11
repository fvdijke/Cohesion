# COHESION — SYNTH · backlog

## Presets
- [ ] **50 fabriekspresets**, verdeeld over categorieën: ARP, Bass, Drums,
  Strings, Synth, Flute, Keys, Pad, Lead, Pluck, FX, … (categorie-browser in de
  preset-balk).
- [ ] **Presets voor alle General MIDI-instrumenten** (de 128 GM-programma's),
  zodat de synth als GM-bank bruikbaar is. Pas na een werkende engine (osc +
  filter + env + FX), want veel GM-klanken (drums, strings, flute) leunen op FX
  en specifieke oscillator-/filterinstellingen.

## Engine (gefaseerd)
- [x] Fundament: 4 VA-oscillators + unison, sub, noise, mixer/FM/ring,
  ZDF-ladderfilter, amp/filter-ADSR, polyfonie.
- [ ] Filter 2 (SVF multimode) + serie/parallel-routing.
- [ ] 4 LFO's + 4 envelopes volledig + mod-matrix.
- [ ] Wavetable-modus (WT) met WT POS-morph.
- [x] FX-keten (comp/flanger/phaser/echo/reverb + limiter), versleepbare volgorde,
  klikvrij aan/uit, tempo-sync; offline getest met `tests/FxTest.cpp`.
- [ ] Arpeggiator.

## GUI
- [x] MIDI-indicator in de header (zichtbaar op elke tab) — tijdelijke controle.
- [x] FILTER/MOD-tab.
- [x] FX-tab.
- [ ] ARP-tab uitwerken volgens de mockup.
