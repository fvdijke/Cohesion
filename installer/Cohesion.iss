; ===========================================================================
;  COHESION Suite - installer  (Pepperwood)
;  Bouw met Inno Setup 6:  ISCC.exe "Cohesion.iss"
;
;  Custom wizard: full-height COHESION-banner links op elke pagina.
;  Keuzes in de wizard:
;   - welke plugin(s): Glue / EQ-Comp / ADT / Transient / Synth / LA Module / LA Font (los of alle)
;   - optioneel de standalone app COHESION SysEx (DX7 bank builder)
;   - installatiemap: standaard de VST3-map (C:\Program Files\Common Files\VST3)
; ===========================================================================

#define AppName        "COHESION"
#define AppPublisher   "Pepperwood"
#define AppVersion     "1.3.1"

; Absolute bronpaden naar de gebouwde VST3-bundels (Release)
#define GlueSrc      "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Glue\build\Cohesion_artefacts\Release\VST3\Cohesion Glue.vst3"
#define EqCompSrc    "E:\OneDrive\Projectbestanden\COHESION\Cohesion-EQComp\build\CohesionEQComp_artefacts\Release\VST3\Cohesion EQ-Comp.vst3"
#define AdtSrc       "E:\OneDrive\Projectbestanden\COHESION\Cohesion-ADT\build\CohesionADT_artefacts\Release\VST3\Cohesion ADT.vst3"
#define TransientSrc "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Transient\build\CohesionTransient_artefacts\Release\VST3\Cohesion Transient.vst3"
#define SynthSrc     "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Synth\build\CohesionSynth_artefacts\Release\VST3\Cohesion Synth.vst3"
; LA Module bundle incl. mt32emu.dll (Munt, LGPL) + its licence text
#define LaModuleSrc  "E:\OneDrive\Projectbestanden\COHESION\Cohesion-LaModule\build\CohesionLaModule_artefacts\Release\VST3\Cohesion LA Module.vst3"
; LA Font bundle incl. libfluidsynth-3.dll (FluidSynth, LGPL) + its licence text
#define LaFontSrc    "E:\OneDrive\Projectbestanden\COHESION\Cohesion-LaFont\build\CohesionLaFont_artefacts\Release\VST3\Cohesion LA Font.vst3"
#define SysExExe     "E:\OneDrive\Projectbestanden\COHESION\Cohesion-SysEx\build\CohesionSysEx_artefacts\Release\Cohesion SysEx.exe"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} Suite {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={commoncf64}\VST3
AppendDefaultDirName=no
DisableDirPage=no
DisableWelcomePage=yes
DisableReadyPage=yes
DisableProgramGroupPage=yes
DisableReadyMemo=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
OutputBaseFilename=COHESION-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
WizardSizePercent=100
WizardImageFile=wizard-banner.png
WizardImageStretch=yes
AlwaysShowComponentsList=yes
ShowComponentSizes=yes
UninstallDisplayName={#AppName} Suite

[Languages]
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupWindowTitle=COHESION Suite  -  Setup
SelectComponentsLabel2=Kies welke plugin(s) en apps je installeert:
SelectDirLabel3=De plugins worden geplaatst in onderstaande map (standaard de VST3-map).

[Types]
Name: "full";   Description: "Alles"
Name: "custom"; Description: "Aangepast"; Flags: iscustom

[Components]
Name: "glue";      Description: "Cohesion Glue (VST3)";      Types: full custom
Name: "eqcomp";    Description: "Cohesion EQ-Comp (VST3)";   Types: full custom
Name: "adt";       Description: "Cohesion ADT (VST3)";       Types: full custom
Name: "transient"; Description: "Cohesion Transient (VST3)"; Types: full custom
Name: "synth";     Description: "Cohesion Synth (VST3 instrument)"; Types: full custom
Name: "lafont";    Description: "Cohesion LA Font - SoundFont player (VST3 instrument, met FluidSynth)"; Types: full custom
Name: "lamodule";  Description: "Cohesion LA Module - MT-32 emulator (VST3 instrument, met Munt)"; Types: full custom
Name: "sysex";     Description: "COHESION SysEx - DX7 bank builder (app)"; Types: full custom

[Files]
; .vst3 is op Windows een map-bundel -> hele map recursief kopieren
Source: "{#GlueSrc}\*";      DestDir: "{app}\Cohesion Glue.vst3";      Components: glue;      Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#EqCompSrc}\*";    DestDir: "{app}\Cohesion EQ-Comp.vst3";   Components: eqcomp;    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AdtSrc}\*";       DestDir: "{app}\Cohesion ADT.vst3";       Components: adt;       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#TransientSrc}\*"; DestDir: "{app}\Cohesion Transient.vst3"; Components: transient; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#SynthSrc}\*";     DestDir: "{app}\Cohesion Synth.vst3";     Components: synth;     Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#LaFontSrc}\*";    DestDir: "{app}\Cohesion LA Font.vst3";   Components: lafont;    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#LaModuleSrc}\*";  DestDir: "{app}\Cohesion LA Module.vst3"; Components: lamodule;  Flags: ignoreversion recursesubdirs createallsubdirs
; the standalone app goes to Program Files, independent of the chosen VST3 folder
Source: "{#SysExExe}";        DestDir: "{commonpf64}\Pepperwood\COHESION SysEx"; Components: sysex; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\COHESION SysEx"; Filename: "{commonpf64}\Pepperwood\COHESION SysEx\Cohesion SysEx.exe"; Components: sysex

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Cohesion Glue.vst3"
Type: filesandordirs; Name: "{app}\Cohesion EQ-Comp.vst3"
Type: filesandordirs; Name: "{app}\Cohesion ADT.vst3"
Type: filesandordirs; Name: "{app}\Cohesion Transient.vst3"
Type: filesandordirs; Name: "{app}\Cohesion Synth.vst3"

; ===========================================================================
;  Custom single-look wizard: banner over de volle hoogte, titel rechts.
; ===========================================================================
[Code]
var
  Banner: TBitmapImage;

procedure InitializeWizard;
var
  BannerW, BannerH: Integer;
begin
  { --- Eigen full-height COHESION-banner links. De breedte volgt uit de hoogte in de
        verhouding van het plaatje (763 x 2062), zodat het nooit uitgerekt wordt,
        ook niet bij Windows-schaling. --- }
  BannerH := WizardForm.CancelButton.Top - ScaleY(12);
  BannerW := (BannerH * 763) div 2062;

  { Hergebruik de door Inno geladen PNG (WizardImageFile) als bitmap. }
  Banner := TBitmapImage.Create(WizardForm);
  Banner.Parent  := WizardForm;
  Banner.Bitmap  := WizardForm.WizardBitmapImage.Bitmap;
  Banner.Stretch := True;
  Banner.Left    := 0;
  Banner.Top     := 0;
  Banner.Width   := BannerW;
  Banner.Height  := BannerH;

  { --- Standaard welkom/afrond-afbeelding en kleine logo weg
        (voorkomt de dubbele banner op de afrondpagina). De kop-strip van Inno
        blijft staan: die vult de rechterkant netjes op. --- }
  WizardForm.WizardBitmapImage.Visible      := False;
  WizardForm.WizardSmallBitmapImage.Visible := False;

  { --- Inno's eigen inhoud (kop + pagina) compleet naar rechts van de banner;
        de bestaande anchors laten alle controls meelopen en de breedte vullen. --- }
  WizardForm.OuterNotebook.Left  := BannerW;
  WizardForm.OuterNotebook.Width := WizardForm.ClientWidth - BannerW;

  { --- Afrondpagina: geen afbeelding (Inno's eigen WizardBitmapImage2 ook niet);
        de tekst krijgt daar de volle breedte. --- }
  WizardForm.WizardBitmapImage2.Visible := False;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  W: Integer;
begin
  if CurPageID = wpFinished then
  begin
    Banner.Visible := False;
    WizardForm.OuterNotebook.Left  := 0;
    WizardForm.OuterNotebook.Width := WizardForm.ClientWidth;
    W := WizardForm.FinishedPage.Width - ScaleX(48);
    WizardForm.FinishedHeadingLabel.Left  := ScaleX(24);
    WizardForm.FinishedHeadingLabel.Width := W;
    WizardForm.FinishedLabel.Left  := ScaleX(24);
    WizardForm.FinishedLabel.Width := W;
  end
  else
  begin
    Banner.Visible := True;
    WizardForm.OuterNotebook.Left  := Banner.Width;
    WizardForm.OuterNotebook.Width := WizardForm.ClientWidth - Banner.Width;
  end;
end;
