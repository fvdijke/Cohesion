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
- [x] LA-klank geijkt tegen Munt als meetlat (tests/LaCompare.cpp, alleen lokaal; Munt in
  `_ref/`, niet in de plugin): envelopes in dB met gemeten tijdcurve, TVA-niveau/velocity/
  bias-tabellen, 4-polig TVF met absolute cutoff (volgt coarse tune), resonantie, TVF-bias en
  -velocity, PW, P-ENV-diepte, LFO, structuren 8/9 en ring-niveaus; group-B-tabel MT-32 1.07
  hersteld. Resultaat over 2 × 128 timbres: niveau-afwijking 1,9 -> 1,1 dB, aanzet 0,70 -> 0,26 oct,
  release 0,29 -> 0,18 oct, helderheid 0,68/0,86 -> 0,38/0,30 oct.
- [ ] LA restpunten: Bird Tweet (extreem hoge toon + resonantie, ~-20 dB), sustain-niveau na 1 s
  (gem. 4 dB), TVF-bias aan de "onder"-kant, LFO-snelheid > 85, P-ENV/TVF release-tijden.

## COHESION — MT-32 MODULE (fase B, aparte plugin)
- [ ] **Keuze engine**: Munt (mt32emu, LGPL 2.1+) gebruiken, of als A/B-referentie naast de
  eigen LA-engine. Gevolgen bij Munt in de plugin: mt32emu als aparte DLL naast de VST3
  (dynamisch linken, zodat gebruikers hem kunnen vervangen), LGPL-tekst + bronvermelding
  meeleveren, broncode (of verwijzing) van de gebruikte Munt-versie beschikbaar stellen, en de
  claim "all DSP is original code" geldt dan niet voor die module. Eigen engine blijft
  volledig eigen code. Mogelijke opzet: engine-schakelaar MUNT / COHESION LA per instantie.
- [x] **COHESION — LA MODULE** gebouwd (Cohesion-LaModule/): MUNT / COHESION LA-schakelaar,
  multi-out (MAIN + OUT 2–10; Munt draait per partij als eigen instantie), MT-32-geheugen +
  SysEx, LOAD .SYX, systeemreverb (in sync met SysEx) + echo-send, mixer met mute/solo/dimmen,
  MIDI-kanaal per partij, ROM-sets gedeeld met de synth; offline getest (tests/ModuleTest.cpp).
- [x] LA MODULE: alleen nog de Munt-engine (eigen LA-engine eruit), LIBRARY leest ROM-map in,
  reverb (per Munt-instantie) en echo per partij met aan/uit, NO ROM LOADED op het LCD.
- [x] LA MODULE: in de installer (1.2.x), partial-meters per partij en totaal, RESET = koude
  start, ROM-menu (map / losse bestanden toevoegen, alles door Munt getest en benoemd, elke
  firmwareversie een eigen set, verwijderen, ROM-map tonen), help-pagina met credits/disclaimer,
  ModuleTest --export om werkende sets te verzamelen.
- [ ] LA MODULE vervolg: LCD-berichten (SysEx 20 00 00), timbre-kiezer in huisstijl i.p.v.
  popupmenu, MAME-halve chips (_a/_b, _h/_l) samenvoegen, ctrl_mt32_m9 onderzoeken.
- [ ] (oorspronkelijke opdracht) Multi-timbrale MT-32-emulator op basis van de LA-engine: 8 melodische partijen +
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
