# COHESION — SYNTH · backlog

## Presets
- [x] **100 presets per categorie** (1.101 totaal): handgemaakt + procedurele generator
  (10 archetypes per categorie), elke preset offline gerenderd en in luidheid
  gelijkgetrokken (tests/PresetRender -> Source/PresetGains.h).
- [x] **DX7 SysEx-import** (32-voice banks + single voices, bestanden of hele map):
  banks worden bewaard in %APPDATA%/Pepperwood/Cohesion Synth/SysEx en verschijnen
  als categorie "DX7 · <bank>"; lege plekken en dubbelen worden overgeslagen.
- [x] **MT-32 / CM-32L ROM-import** (IMPORT ROMS: map, zips worden mee gescand): control- en
  PCM-ROM worden zelf gekoppeld, identieke klankversies overgeslagen; sets bewaard in
  %APPDATA%/Pepperwood/Cohesion Synth/ROMs, categorieën "<set> · A / B / Rhythm".
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
- [x] **LA-engine** (MT-32 / CM-32L-compatibel): eigen synthese van het timbreformaat
  (4 partials, synth vierkant/zaag + TVF, PCM-samples uit de ROM, P-ENV/LFO, TVA,
  13 structuren incl. ringmodulatie) + LA-tab; offline getest (tests/LaTest.cpp).
- [ ] LA-klank fijnslijpen tegen een echte MT-32/PI-MT32 (envelope-tijden, TVF-curve,
  bias, ringmodulatie-varianten, toonhoogte-keyfollow s1/s2).

## COHESION — MT-32 MODULE (fase B, aparte plugin)
- [ ] Multi-timbrale MT-32-emulator op basis van de LA-engine: 8 melodische partijen +
  ritmepartij, elk met eigen MIDI-kanaal (standaard 2–9 + 10), program changes en
  MT-32/CM-32L SysEx (timbres, patches, systeem), 32 partials met voice-allocatie,
  ritme-toetsindeling uit de ROM, reverb en echo, mixer per partij.
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
