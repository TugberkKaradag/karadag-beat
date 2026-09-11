#define AppName "Karadag Beat"
#define RepoUrl "https://github.com/TugberkKaradag/karadag-beat"

#ifndef AppVersion
  #define AppVersion "0.2.2"
#endif

#ifndef BuildDir
  #define BuildDir "..\build\KaradagBeat_artefacts\Release"
#endif

#ifdef TestRoot
  #define VstDir     TestRoot + "\VST3"
  #define AppDir     TestRoot + "\App"
  #define PrivMode   "lowest"
  #define AppIdGuid  "{{195CE2F2-C8CC-4CFE-8653-9E83E0B44262}-test"
#else
  #define VstDir     "{commoncf64}\VST3"
  #define AppDir     "{autopf}\" + AppName
  #define PrivMode   "admin"
  #define AppIdGuid  "{{195CE2F2-C8CC-4CFE-8653-9E83E0B44262}"
#endif

[Setup]
AppId={#AppIdGuid}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher=Karadag
AppPublisherURL={#RepoUrl}
AppSupportURL={#RepoUrl}/issues
AppUpdatesURL={#RepoUrl}/releases
VersionInfoVersion={#AppVersion}
VersionInfoDescription={#AppName} Setup

DefaultDirName={#AppDir}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableWelcomePage=no
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\icon.ico

SetupIconFile=icon.ico
WizardStyle=modern dark
WizardImageFile=wizard.png,wizard-150.png,wizard-200.png
WizardSmallImageFile=wizard-small.png,wizard-small-150.png,wizard-small-200.png

OutputDir=..\build\installer
OutputBaseFilename=KaradagBeat-{#AppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired={#PrivMode}

CloseApplications=yes
CloseApplicationsFilter=*.exe,*.dll,*.vst3
RestartApplications=no

[Messages]
FinishedLabel=[name] is installed as a VST3 effect.%n%nIf your DAW is open, rescan plugins so it shows up.

[CustomMessages]
TypeFull=VST3 plugin and standalone app
TypeCustom=Custom
CompVst3=VST3 plugin
CompStandalone=Standalone app (runs without a DAW)
OpenStandalone=Open the standalone app

[Types]
Name: "full";   Description: "{cm:TypeFull}"
Name: "custom"; Description: "{cm:TypeCustom}"; Flags: iscustom

[Components]
Name: "vst3";       Description: "{cm:CompVst3}";       Types: full custom; Flags: fixed
Name: "standalone"; Description: "{cm:CompStandalone}"; Types: full

[Files]
Source: "{#BuildDir}\VST3\Karadag Beat.vst3\*"; DestDir: "{#VstDir}\Karadag Beat.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\Karadag Beat.exe"; DestDir: "{app}"; \
    Flags: ignoreversion; Components: standalone
Source: "icon.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\Karadag Beat.exe"; Components: standalone

[Run]
Filename: "{app}\Karadag Beat.exe"; Description: "{cm:OpenStandalone}"; \
    Flags: postinstall nowait skipifsilent unchecked; Components: standalone

[UninstallDelete]
Type: filesandordirs; Name: "{#VstDir}\Karadag Beat.vst3"
