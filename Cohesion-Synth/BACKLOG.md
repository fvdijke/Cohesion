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
- [x] Library-zoekfunctie (alle woorden moeten in naam of categorie voorkomen; zoekt in alle categorieën).
- [x] Geïmporteerde DX7- / LA-voices in luidheid gelijktrekken: bij het laden offline gemeten
  (zelfde maat als PresetRender) en MAIN / DRIVE gezet.
- [x] DX7-banken uit de library verwijderen (REMOVE BANK / rechtsklik; originelen blijven staan).
- [x] **User-presets opslaan**: SAVE in de header bewaart de hele klank (parameters, FX-volgorde,
  DX7-voice, LA-timbre, ARP-patroon; niet de venstergrootte) als .cspreset in
  %APPDATA%/Pepperwood/Cohesion Synth/Presets; categorie "User" (direct onder Init) in de
  LIBRARY; vervangen na bevestiging, RENAME / DELETE (prullenbak), ook via rechtsklik.
- [ ] User-presets in eigen submappen / categorieën (nu één categorie "User").
- [x] **Multi sounds**: VA, FM en LA zijn drie parts; per part laden (LOAD VA... op OSC / MIX,
  LOAD op de FM- / LA-tab, LOAD... in de MIXER), de rest blijft staan; part-level = gematchte
  luidheid van de preset. User-presets bewaren de hele multi; één part eruit laden kan ook.
- [x] **MIXER-tab**: per part level + meter, pan, echo- / reverb-send, key range (split), ARP aan/uit
  (ARP-noten op kanaal 2, gespeelde noten op kanaal 1), VA FILTER voor FM / LA, mute, solo;
  echo / reverb return, limiter, MAIN. Comp / flanger / phaser per part, echo / reverb als
  send (MIX werkt op het gestuurde deel) — bij sends 100 % exact de oude keten.
- [x] **Kwaliteitscontrole** (tests/AuditCheck, 17 gemeten checks): bend / modwiel / sustain naar
  de ARP-voices; toetsen buiten elke key range nemen geen voice; de VA-modmatrix lekt niet meer
  naar FM / LA (VA FILT+MOD schakelt hem in); VELOCITY alleen op VA; presetnaam blijft na heropenen;
  ladder met zero-delay feedback; SVF-DRIVE werkt, SVF 24 dB-resonantie = 12 dB; driehoek start
  zonder DC; LFO SMOOTH relatief aan de periode; echo verzadigt alleen de feedback; flanger /
  phaser van alle parts op één LFO; sustain volgt de knop zonder sprongen; part-level tot +18 dB.
  Presetluidheid opnieuw gemeten (PresetGains.h).
- [x] Multi-User-preset bleef stil: parameters die JUCE zonder waarde opsloeg (project uit een oudere
  versie) werden als 0 gelezen (KEY HIGH 0). Lege waarden = default; opslaan schrijft nu elke waarde.
- [x] VA-part: ON-schakelaar en VA FILT+MOD in de MIXER; klik op de presetnaam opent de LIBRARY;
  alleen IMPORT .SYX / IMPORT ROMS (SYX FOLDER weg), de laatste importmap per soort onthouden.
- [ ] Multi: een part laden kort het geheel uit (5 ms fade, alle voices stoppen), ook de andere parts.
- [ ] Multi: in MONO / LEGATO delen alle parts (en ARP- + gespeelde noten) één monostem.
- [ ] Gemeten luidheid van geïmporteerde DX7-voices met vrijlopende LFO / willekeurige
  operatorfases varieert per render (Brass-bank: tot +4 dB): meerdere renders middelen.
- [ ] **SysEx bank-samensteller**: voices uit verschillende .syx-bestanden kiezen
  en samenvoegen tot een nieuwe 32-voice bank (eigen taak/sessie).
- [x] LIBRARY-venster (categorieën → presets) naast de preset-kiezer.
- [x] 16 ARP-patronen (step-sequencer presets).
- [x] **General MIDI-bank**: 128 GM-programma's (Source/GmBank.h) in 16 families
  ("GM · Piano" ...), MIDI program change laadt het GM-preset. GM-drumkits (kanaal 10,
  per toets een ander geluid) kunnen niet met één patch.

## Engine (gefaseerd)
- [x] Fundament: 4 VA-oscillators + unison, sub, noise, mixer/FM/ring,
  ZDF-ladderfilter, amp/filter-ADSR, polyfonie.
- [x] Filter 2 (SVF multimode) + serie/parallel-routing.
- [x] 4 LFO's + 4 envelopes + mod-matrix (8 slots).
- [x] Wavetable-modus (WT): 12 banken (Analog, PWM, Harmonic, Vocal, Sync, FM, Fold, Organ,
  Odd/Even, Comb, Chip, Digital) x 8 frames, per octaaf band-begrensd (FFT), gelijke RMS per
  frame, eigen BANK-keuze per OSC, WT POS-morph + mod-bestemming "WT Pos".
- [ ] Eigen wavetables importeren (.wav, frames van 2048 samples, Serum/Vital-formaat).
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
- [x] Bij het laden van een patch de bijbehorende tab openen (DX7-voice → FM, LA-voice → LA,
  anders OSC / MIX; met een VA-laag erbij → OSC / MIX).

## Review november 2026 (afgerond)
- [x] Audio-thread zonder string-bouw / lookups (parameterpointers vastgelegd bij de start).
- [x] Voice stealing en harde stops klikvrij (3 ms fade), parameter-smoothing (levels, cutoff).
- [x] Patchwissel klikvrij (5 ms fade, voices + FX-staarten gewist); DX/LA-engines los van de AMP-envelope.
- [x] LFO SYNC/DIV en MONO werken, noise FILTER werkt, pitch bend (± 2), echte HP 24.
- [x] Filter- en toonhoogte-berekeningen op control-rate (8 samples), phaser idem.
- [x] Ladder: basverlies bij resonantie deels gecompenseerd; envelopes met analoge (RC) curves.
- [x] VOICE POLY / MONO / LEGATO + GLIDE + VELOCITY in de header.
- [x] Geparste SysEx-banken gedeeld tussen plugin-instanties.
