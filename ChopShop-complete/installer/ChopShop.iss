; Windows installer (Inno Setup 6). Auto-installs to the standard plugin folders.
; Compile with:  ISCC.exe ChopShop.iss          (VST3 only)
;                ISCC.exe /DHasAAX ChopShop.iss (VST3 + AAX)
#define AppVer "1.0.0"
#define Art "..\build\ChopShop_artefacts\Release"

[Setup]
AppName=ChopShop
AppVersion={#AppVer}
AppPublisher=ChopShop Audio
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=.
#ifdef AAXOnly
OutputBaseFilename=ChopShop-AAX-Setup-{#AppVer}
#else
OutputBaseFilename=ChopShop-Setup-{#AppVer}
#endif
Compression=lzma2
SolidCompression=yes
UninstallDisplayName=ChopShop

[Types]
Name: "full"; Description: "Full installation"
Name: "custom"; Description: "Custom installation"; Flags: iscustom

[Components]
#ifndef AAXOnly
Name: "vst3"; Description: "VST3 (FL Studio, Ableton, Cubase, Reaper...)"; Types: full custom; Flags: fixed
#endif
#ifdef HasAAX
Name: "aax"; Description: "AAX (Pro Tools)"; Types: full custom
#endif

[Files]
#ifndef AAXOnly
Source: "{#Art}\VST3\ChopShop.vst3\*"; DestDir: "{commoncf64}\VST3\ChopShop.vst3"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: vst3
#endif
#ifdef HasAAX
Source: "{#Art}\AAX\ChopShop.aaxplugin\*"; DestDir: "{commoncf64}\Avid\Audio\Plug-Ins\ChopShop.aaxplugin"; Flags: recursesubdirs createallsubdirs ignoreversion; Components: aax
#endif

