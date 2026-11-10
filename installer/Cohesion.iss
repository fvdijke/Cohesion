; ===========================================================================
;  COHESION Suite - installer  (Pepperwood)
;  Bouw met Inno Setup 6:  ISCC.exe "Cohesion.iss"
;
;  Custom wizard: full-height COHESION-banner links op elke pagina.
;  Keuzes in de wizard:
;   - welke plugin(s): Glue / EQ-Comp / ADT / Transient (los of alle)
;   - installatiemap: standaard de VST3-map (C:\Program Files\Common Files\VST3)
; ===========================================================================

#define AppName        "COHESION"
#define AppPublisher   "Pepperwood"
#define AppVersion     "1.0.3"

; Absolute bronpaden naar de gebouwde VST3-bundels (Release)
#define GlueSrc      "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Glue\build\Cohesion_artefacts\Release\VST3\Cohesion Glue.vst3"
#define EqCompSrc    "E:\OneDrive\Projectbestanden\COHESION\Cohesion-EQComp\build\CohesionEQComp_artefacts\Release\VST3\Cohesion EQ-Comp.vst3"
#define AdtSrc       "E:\OneDrive\Projectbestanden\COHESION\Cohesion-ADT\build\CohesionADT_artefacts\Release\VST3\Cohesion ADT.vst3"
#define TransientSrc "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Transient\build\CohesionTransient_artefacts\Release\VST3\Cohesion Transient.vst3"

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
SelectComponentsLabel2=Kies welke plugin(s) je installeert:
SelectDirLabel3=De plugins worden geplaatst in onderstaande map (standaard de VST3-map).

[Types]
Name: "full";   Description: "Alle plugins"
Name: "custom"; Description: "Aangepast"; Flags: iscustom

[Components]
Name: "glue";      Description: "Cohesion Glue (VST3)";      Types: full custom
Name: "eqcomp";    Description: "Cohesion EQ-Comp (VST3)";   Types: full custom
Name: "adt";       Description: "Cohesion ADT (VST3)";       Types: full custom
Name: "transient"; Description: "Cohesion Transient (VST3)"; Types: full custom

[Files]
; .vst3 is op Windows een map-bundel -> hele map recursief kopieren
Source: "{#GlueSrc}\*";      DestDir: "{app}\Cohesion Glue.vst3";      Components: glue;      Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#EqCompSrc}\*";    DestDir: "{app}\Cohesion EQ-Comp.vst3";   Components: eqcomp;    Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AdtSrc}\*";       DestDir: "{app}\Cohesion ADT.vst3";       Components: adt;       Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#TransientSrc}\*"; DestDir: "{app}\Cohesion Transient.vst3"; Components: transient; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Cohesion Glue.vst3"
Type: filesandordirs; Name: "{app}\Cohesion EQ-Comp.vst3"
Type: filesandordirs; Name: "{app}\Cohesion ADT.vst3"
Type: filesandordirs; Name: "{app}\Cohesion Transient.vst3"

; ===========================================================================
;  Custom single-look wizard: banner over de volle hoogte, titel rechts.
; ===========================================================================
[Code]
var
  TitleLabel, SubLabel: TNewStaticText;
  Banner: TBitmapImage;
  BannerW: Integer;

procedure PlaceHeading(ACaption, ASub: String);
begin
  TitleLabel.Caption := ACaption;
  SubLabel.Caption   := ASub;
end;

procedure InitializeWizard;
begin
  BannerW := ScaleX(170);

  { --- Eigen full-height COHESION-banner links op ELKE pagina.
        Hergebruik de door Inno geladen PNG (WizardImageFile) als bitmap. --- }
  Banner := TBitmapImage.Create(WizardForm);
  Banner.Parent  := WizardForm;
  Banner.Bitmap  := WizardForm.WizardBitmapImage.Bitmap;
  Banner.Stretch := True;
  Banner.Left    := 0;
  Banner.Top     := 0;
  Banner.Width   := BannerW;
  Banner.Height  := WizardForm.CancelButton.Top - ScaleY(10);

  { --- Standaard welkom/afrond-afbeelding, kop-strip en kleine logo weg
        (voorkomt de dubbele banner op de afrondpagina). --- }
  WizardForm.WizardBitmapImage.Visible      := False;
  WizardForm.MainPanel.Visible              := False;
  WizardForm.Bevel.Visible                  := False;
  WizardForm.WizardSmallBitmapImage.Visible := False;

  { --- Alle pagina-inhoud naar rechts van de banner schuiven --- }
  WizardForm.OuterNotebook.Left   := BannerW + ScaleX(24);
  WizardForm.OuterNotebook.Width  := WizardForm.ClientWidth - WizardForm.OuterNotebook.Left - ScaleX(16);
  WizardForm.OuterNotebook.Top    := ScaleY(70);
  WizardForm.OuterNotebook.Height := WizardForm.CancelButton.Top - WizardForm.OuterNotebook.Top - ScaleY(18);

  { --- Blijvende titel + subtitel rechtsboven (als in de referentie) --- }
  TitleLabel := TNewStaticText.Create(WizardForm);
  TitleLabel.Parent     := WizardForm;
  TitleLabel.Left       := BannerW + ScaleX(24);
  TitleLabel.Top        := ScaleY(22);
  TitleLabel.AutoSize   := True;
  TitleLabel.Font.Size  := 15;
  TitleLabel.Font.Style := [fsBold];
  TitleLabel.Font.Color := clBlack;
  TitleLabel.Caption    := 'Install COHESION Suite';

  SubLabel := TNewStaticText.Create(WizardForm);
  SubLabel.Parent      := WizardForm;
  SubLabel.Left        := BannerW + ScaleX(24);
  SubLabel.Top         := ScaleY(48);
  SubLabel.AutoSize    := True;
  SubLabel.Font.Size   := 9;
  SubLabel.Font.Color  := clGray;
  SubLabel.Caption     := 'Original VST3 audio plugins by Pepperwood.';
end;

procedure CurPageChanged(CurPageID: Integer);
begin
  case CurPageID of
    wpSelectComponents: PlaceHeading('Install COHESION Suite', 'Choose which plugins to install.');
    wpSelectDir:        PlaceHeading('Install COHESION Suite', 'Pick the install location (defaults to the VST3 folder).');
    wpInstalling:       PlaceHeading('Installing COHESION Suite', 'Copying the selected VST3 plugins...');
    wpFinished:         PlaceHeading('COHESION Suite installed', 'Your plugins are ready to use.');
  else
    PlaceHeading('Install COHESION Suite', 'Original VST3 audio plugins by Pepperwood.');
  end;
end;
