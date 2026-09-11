; Karadag Beat - Windows kurulum dosyasi (Inno Setup 6.7+)
;
; Once eklentiyi Release olarak derle, sonra:
;   ISCC.exe installer\KaradagBeat.iss /DAppVersion=0.2.0
; ya da kisaca:  .\build.ps1 -Installer
;
; Cikti: build\installer\KaradagBeat-<surum>-Setup.exe
;
; Gorseller (icon.ico, wizard*.png) Tests\BrandRender.cpp ile logodan uretilir.
; /DTestRoot=<klasor> verilirse her sey o klasore, yonetici izni istemeden kurulur
; (yalnizca kurulum / kaldirma testi icin).

#define AppName "Karadag Beat"
#define RepoUrl "https://github.com/TugberkKaradag/karadag-beat"

#ifndef AppVersion
  #define AppVersion "0.2.0"
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
  ; FL Studio yalnizca sistem VST3 klasorunu tariyor
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

; Dil penceresi yalnizca Windows dili listede yoksa cikar
ShowLanguageDialog=auto

; FL acikken eklenti dosyasi kilitli olur: Setup bunu fark edip kapatmayi teklif eder
CloseApplications=yes
CloseApplicationsFilter=*.exe,*.dll,*.vst3
RestartApplications=no

[Languages]
Name: "en"; MessagesFile: "compiler:Default.isl"
Name: "tr"; MessagesFile: "compiler:Languages\Turkish.isl"

[CustomMessages]
en.TypeFull=VST3 plugin and standalone app
en.TypeCustom=Custom
en.CompVst3=VST3 plugin
en.CompStandalone=Standalone app (runs without a DAW)
en.OpenStandalone=Open the standalone app

tr.TypeFull=VST3 eklentisi ve bağımsız uygulama
tr.TypeCustom=Özel
tr.CompVst3=VST3 eklentisi
tr.CompStandalone=Bağımsız uygulama (DAW olmadan çalışır)
tr.OpenStandalone=Bağımsız uygulamayı aç

[Messages]
en.FinishedLabel=[name] is installed.%n%nIn FL Studio open Options > Manage plugins and click Find more plugins. It then shows up under Effects.
tr.FinishedLabel=[name] kuruldu.%n%nFL Studio'da Options > Manage plugins menüsünden Find more plugins'e tıkla. Eklenti Effects altında görünür.

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
; Kaydedilen pattern'ler (%APPDATA%\Karadag\KaradagBeat) bilerek silinmiyor
Type: filesandordirs; Name: "{#VstDir}\Karadag Beat.vst3"
