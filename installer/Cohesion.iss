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
#define AppVersion     "1.4.0"

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

; Version each component carries = the file version of its built binary (from versions.json
; via CMake). The wizard compares it with what is installed and greys out what is up to date.
#define VstBin(Src, Name) Src + "\Contents\x86_64-win\" + Name + ".vst3"
#define GlueVer      GetVersionNumbersString(VstBin(GlueSrc,      "Cohesion Glue"))
#define EqCompVer    GetVersionNumbersString(VstBin(EqCompSrc,    "Cohesion EQ-Comp"))
#define AdtVer       GetVersionNumbersString(VstBin(AdtSrc,       "Cohesion ADT"))
#define TransientVer GetVersionNumbersString(VstBin(TransientSrc, "Cohesion Transient"))
#define SynthVer     GetVersionNumbersString(VstBin(SynthSrc,     "Cohesion Synth"))
#define LaFontVer    GetVersionNumbersString(VstBin(LaFontSrc,    "Cohesion LA Font"))
#define LaModuleVer  GetVersionNumbersString(VstBin(LaModuleSrc,  "Cohesion LA Module"))
#define SysExVer     GetVersionNumbersString(SysExExe)

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

[CustomMessages]
nl.VerUpToDate=al up-to-date (v%1)
en.VerUpToDate=up to date (v%1)
nl.VerUpdate=update v%1 -> v%2
en.VerUpdate=update v%1 -> v%2
nl.VerNew=nieuw, v%1
en.VerNew=new, v%1
nl.VerAllUpToDate=Alle onderdelen zijn al up-to-date; er valt niets te installeren.%n%nKlik op Annuleren om Setup te sluiten.
en.VerAllUpToDate=Every component is already up to date; there is nothing to install.%n%nClick Cancel to close Setup.
nl.VerNothingSelected=Kies minstens 1 onderdeel om te installeren, of klik op Annuleren.
en.VerNothingSelected=Select at least 1 component to install, or click Cancel.

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

{ ---------------------------------------------------------------------------
  Version check per component (same order as [Components]).
  --------------------------------------------------------------------------- }
const
  NumComps = 8;
var
  CompName:     array[0..7] of String;   { bundle / exe name }
  CompVer:      array[0..7] of String;   { version this installer carries }
  CompCaption:  array[0..7] of String;   { original caption }
  CompUpToDate: array[0..7] of Boolean;
  VersionsShown: Boolean;
  OldTypesChange: TNotifyEvent;

function NextVerPart(var S: String): String;
var
  P: Integer;
begin
  P := Pos('.', S);
  if P = 0 then begin Result := S; S := ''; end
  else begin Result := Copy(S, 1, P - 1); Delete(S, 1, P); end;
end;

{ <0: A older than B, 0: equal, >0: A newer. '1.3' = '1.3.0.0' }
function CompareVer(A, B: String): Integer;
var
  X, Y: Integer;
begin
  Result := 0;
  while (Result = 0) and ((A <> '') or (B <> '')) do
  begin
    X := StrToIntDef(NextVerPart(A), 0);
    Y := StrToIntDef(NextVerPart(B), 0);
    if X < Y then Result := -1 else if X > Y then Result := 1;
  end;
end;

{ '1.3.1.0' -> '1.3.1' }
function ShortVer(V: String): String;
begin
  Result := V;
  if (Length(Result) > 2) and (Copy(Result, Length(Result) - 1, 2) = '.0') then
    Result := Copy(Result, 1, Length(Result) - 2);
end;

function InstalledFile(I: Integer): String;
begin
  if I = 7 then
    Result := ExpandConstant('{commonpf64}\Pepperwood\COHESION SysEx\') + CompName[I] + '.exe'
  else
    Result := AddBackslash(WizardDirValue) + CompName[I] + '.vst3\Contents\x86_64-win\' + CompName[I] + '.vst3';
end;

{ '' when not installed (or no version resource) }
function InstalledVersion(I: Integer): String;
begin
  Result := '';
  if FileExists(InstalledFile(I)) then
    if not GetVersionNumbersString(InstalledFile(I), Result) then Result := '';
end;

procedure InitVersionInfo;
var
  I: Integer;
begin
  CompName[0] := 'Cohesion Glue';      CompVer[0] := '{#GlueVer}';
  CompName[1] := 'Cohesion EQ-Comp';   CompVer[1] := '{#EqCompVer}';
  CompName[2] := 'Cohesion ADT';       CompVer[2] := '{#AdtVer}';
  CompName[3] := 'Cohesion Transient'; CompVer[3] := '{#TransientVer}';
  CompName[4] := 'Cohesion Synth';     CompVer[4] := '{#SynthVer}';
  CompName[5] := 'Cohesion LA Font';   CompVer[5] := '{#LaFontVer}';
  CompName[6] := 'Cohesion LA Module'; CompVer[6] := '{#LaModuleVer}';
  CompName[7] := 'Cohesion SysEx';     CompVer[7] := '{#SysExVer}';
  for I := 0 to NumComps - 1 do
  begin
    CompCaption[I] := WizardForm.ComponentsList.ItemCaption[I];
    CompUpToDate[I] := False;
  end;
end;

{ Grey out (disable + uncheck) every component whose installed version is the same or newer;
  re-run whenever the list may have been re-checked (page shown, folder or type changed). }
procedure ApplyVersionStates;
var
  I: Integer;
  Inst: String;
  List: TNewCheckListBox;
begin
  List := WizardForm.ComponentsList;
  for I := 0 to NumComps - 1 do
  begin
    Inst := InstalledVersion(I);
    Log(Format('Version check: %s  installed "%s"  carried "%s"  (%s)', [CompName[I], Inst, CompVer[I], InstalledFile(I)]));
    if (Inst <> '') and (CompareVer(Inst, CompVer[I]) >= 0) then
    begin
      List.Checked[I] := False;
      List.ItemEnabled[I] := False;
      List.ItemCaption[I] := CompCaption[I] + '   -   ' + FmtMessage(CustomMessage('VerUpToDate'), [ShortVer(Inst)]);
      CompUpToDate[I] := True;
    end
    else
    begin
      List.ItemEnabled[I] := True;
      if CompUpToDate[I] then List.Checked[I] := True;   { was greyed out for another folder }
      CompUpToDate[I] := False;
      if Inst = '' then
        List.ItemCaption[I] := CompCaption[I] + '   -   ' + FmtMessage(CustomMessage('VerNew'), [ShortVer(CompVer[I])])
      else
        List.ItemCaption[I] := CompCaption[I] + '   -   ' + FmtMessage(CustomMessage('VerUpdate'), [ShortVer(Inst), ShortVer(CompVer[I])]);
    end;
  end;
  List.Invalidate;
end;

function AllUpToDate: Boolean;
var
  I: Integer;
begin
  Result := True;
  for I := 0 to NumComps - 1 do
    if not CompUpToDate[I] then Result := False;
end;

procedure TypesComboChange(Sender: TObject);
begin
  OldTypesChange(Sender);   { Inno's own handler: (re)checks the components of the chosen type }
  ApplyVersionStates;
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;
  if (CurPageID = wpSelectComponents) and (WizardSelectedComponents(False) = '') then
  begin
    if AllUpToDate then
      MsgBox(CustomMessage('VerAllUpToDate'), mbInformation, MB_OK)
    else
      MsgBox(CustomMessage('VerNothingSelected'), mbInformation, MB_OK);
    Result := False;
  end;
end;

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

  { --- versiecheck per onderdeel --- }
  InitVersionInfo;
  OldTypesChange := WizardForm.TypesCombo.OnChange;
  WizardForm.TypesCombo.OnChange := @TypesComboChange;
end;

procedure CurPageChanged(CurPageID: Integer);
var
  W: Integer;
begin
  if CurPageID = wpSelectComponents then
  begin
    ApplyVersionStates;
    if AllUpToDate and not VersionsShown then
      MsgBox(CustomMessage('VerAllUpToDate'), mbInformation, MB_OK);
    VersionsShown := True;
  end;

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
