# COHESION — SYNTH · backlog

## Presets
- [x] **100 presets per categorie** (1.101 totaal): handgemaakt + procedurele generator
  (10 archetypes per categorie), elke preset offline gerenderd en in luidheid
  gelijkgetrokken (tests/PresetRender -> Source/PresetGains.h).
- [x] **DX7 SysEx-import** (32-voice banks + single voices, bestanden of hele map):
  banks worden bewaard in %APPDATA%/Pepperwood/Cohesion Synth/SysEx en verschijnen
  als categorie "DX7 · <bank>"; lege plekken en dubbelen worden overgeslagen.
- [ ] Library-zoekfunctie (nodig bij grote collecties, bv. MiniDexed ~100k voices).
- [ ] Geïmporteerde DX7-voices in luidheid gelijktrekken (zoals de factory-presets).
- [ ] **SysEx bank-samensteller**: voices uit verschillende .syx-bestanden kiezen
  en samenvoegen tot een nieuwe 32-voice bank (eigen taak/sessie).
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
- [x] FM-engine (DX7-compatibel, 6 operators, 32 algoritmes, operator-EG's, feedback,
  ratio/fixed, velocity, key scaling, pitch-EG, LFO) + FM-tab; offline getest
  (tests/FmTest.cpp).
- [x] FX-keten (comp/flanger/phaser/echo/reverb + limiter), versleepbare volgorde,
  klikvrij aan/uit, tempo-sync; offline getest met `tests/FxTest.cpp`.
- [x] Arpeggiator + 16-staps velocity-sequencer (host-sync, swing, hold/latch,
  slide, humanize); offline getest met `tests/ArpTest.cpp`.

## GUI
- [x] MIDI-indicator in de header (zichtbaar op elke tab) — tijdelijke controle.
- [x] FILTER/MOD-tab.
- [x] FX-tab.
- [x] ARP-tab.
- [x] Resizable (60–200%, vaste verhouding, grootte onthouden).
- [x] Limiter-paneel (drive / ceiling / release + GR-meter); knoppen dimmen bij FX uit.
