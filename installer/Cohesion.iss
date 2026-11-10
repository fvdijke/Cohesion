; ===========================================================================
;  Cohesion Audio Plugins - installer  (Pepperwood)
;  Bouw met Inno Setup 6:  ISCC.exe "Cohesion.iss"
;
;  Keuzes in de wizard:
;   - welke plugin(s): Cohesion Glue en/of Cohesion EQ-Comp (los of beide)
;   - installatiemap: standaard de VST3-map (C:\Program Files\Common Files\VST3),
;     vrij aan te passen op de locatie-pagina
; ===========================================================================

#define AppName        "COHESION"
#define AppPublisher   "Pepperwood"
#define AppVersion     "1.0.1"

; Absolute bronpaden naar de gebouwde VST3-bundels (Release)
#define GlueSrc    "E:\OneDrive\Projectbestanden\COHESION\Cohesion-Glue\build\Cohesion_artefacts\Release\VST3\Cohesion Glue.vst3"
#define EqCompSrc  "E:\OneDrive\Projectbestanden\COHESION\Cohesion-EQComp\build\CohesionEQComp_artefacts\Release\VST3\Cohesion EQ-Comp.vst3"
#define AdtSrc     "E:\OneDrive\Projectbestanden\COHESION\Cohesion-ADT\build\CohesionADT_artefacts\Release\VST3\Cohesion ADT.vst3"

[Setup]
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={commoncf64}\VST3
AppendDefaultDirName=no
DisableDirPage=no
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
OutputBaseFilename=COHESION-Setup-{#AppVersion}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#AppName}

[Languages]
Name: "nl"; MessagesFile: "compiler:Languages\Dutch.isl"
Name: "en"; MessagesFile: "compiler:Default.isl"

[Messages]
SetupWindowTitle=COHESION  -  Setup
WelcomeLabel1=COHESION
WelcomeLabel2=By Pepperwood%n%nDeze installer plaatst de COHESION VST3-plugins. Kies welke plugin(s) je installeert en de locatie (standaard de VST3-map).

[Types]
Name: "full";   Description: "Alle plugins"
Name: "custom"; Description: "Aangepast"; Flags: iscustom

[Components]
Name: "glue";   Description: "Cohesion Glue (VST3)";     Types: full custom
Name: "eqcomp"; Description: "Cohesion EQ-Comp (VST3)";  Types: full custom
Name: "adt";    Description: "Cohesion ADT (VST3)";      Types: full custom

[Files]
; .vst3 is op Windows een map-bundel -> hele map recursief kopieren
Source: "{#GlueSrc}\*";   DestDir: "{app}\Cohesion Glue.vst3";    Components: glue;   Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#EqCompSrc}\*"; DestDir: "{app}\Cohesion EQ-Comp.vst3"; Components: eqcomp; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#AdtSrc}\*";    DestDir: "{app}\Cohesion ADT.vst3";     Components: adt;    Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: filesandordirs; Name: "{app}\Cohesion Glue.vst3"
Type: filesandordirs; Name: "{app}\Cohesion EQ-Comp.vst3"
Type: filesandordirs; Name: "{app}\Cohesion ADT.vst3"
