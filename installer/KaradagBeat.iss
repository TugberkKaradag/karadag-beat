; Karadag Beat - Windows kurulum dosyasi (Inno Setup 6)
;
; Derlemek icin once eklentiyi Release olarak derle, sonra:
;   ISCC.exe installer\KaradagBeat.iss /DAppVersion=0.2.0
; ya da kisaca:  .\build.ps1 -Installer
;
; Cikti: build\installer\KaradagBeat-<surum>-Setup.exe

#define AppName "Karadag Beat"

#ifndef AppVersion
  #define AppVersion "0.2.0"
#endif

#ifndef BuildDir
  #define BuildDir "..\build\KaradagBeat_artefacts\Release"
#endif

[Setup]
AppId={{195CE2F2-C8CC-4CFE-8653-9E83E0B44262}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher=Karadag
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayName={#AppName}
OutputDir=..\build\installer
OutputBaseFilename=KaradagBeat-{#AppVersion}-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
; VST3 sistem klasorune yazmak yonetici izni ister - FL yalnizca orayi tariyor
PrivilegesRequired=admin

[Types]
Name: "full";    Description: "VST3 plugin and standalone app"
Name: "plugin";  Description: "VST3 plugin only"
Name: "custom";  Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plugin  (C:\Program Files\Common Files\VST3)"; Types: full plugin custom; Flags: fixed
Name: "standalone"; Description: "Standalone app";                                  Types: full

[Files]
Source: "{#BuildDir}\VST3\Karadag Beat.vst3\*"; DestDir: "{commoncf64}\VST3\Karadag Beat.vst3"; \
    Flags: ignoreversion recursesubdirs createallsubdirs; Components: vst3
Source: "{#BuildDir}\Standalone\Karadag Beat.exe"; DestDir: "{app}"; \
    Flags: ignoreversion; Components: standalone

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\Karadag Beat.exe"; Components: standalone
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"

[UninstallDelete]
; Kaydettigin pattern'ler (%APPDATA%\Karadag\KaradagBeat) bilerek silinmiyor
Type: filesandordirs; Name: "{commoncf64}\VST3\Karadag Beat.vst3"

[Messages]
FinishedLabel=Karadag Beat is installed.%n%nIn FL Studio open Options > Manage plugins and click Find more plugins. It then shows up under Effects > VST3.
