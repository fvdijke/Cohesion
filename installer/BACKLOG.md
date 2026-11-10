# Installer — backlog

Feedback op de custom full-height-banner wizard (COHESION-Setup-1.0.3, nog **niet** gereleased).

- [x] **Letters links zagen er ingedrukt uit.** Opgelost: nieuwe banner-afbeelding
  (golfvorm, 763×2062, verhouding 0.37) die vrijwel exact op de bannerstrook past,
  dus de stretch vervormt "COHESION / by Pepperwood" niet meer.
- [x] **Graphics mogen spannender.** Vervangen door de groene golfvorm-afbeelding.
- [x] **Afrondpagina kopieerde de banner in het rechter content-vak.** Opgelost:
  eigen `TBitmapImage`-banner + Inno's standaard welkom/afrond-afbeelding verborgen
  (`WizardBitmapImage.Visible := False`), zodat die niet meer dubbel verschijnt.

(Bovenstaande nog door de gebruiker te verifiëren; daarna v1.0.3 releasen.)

Daarna pas v1.0.3 (of hoger) releasen en README/screenshots bijwerken.
