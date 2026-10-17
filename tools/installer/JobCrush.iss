; Job Crush - Windows installer
;
; Built with Inno Setup 6 (free, https://jrsoftware.org/isinfo.php).
; Do not run this by hand. Run build-installer.ps1 in this folder: it builds
; Job Crush, gathers every file it needs, and then runs this script with the
; right folder and version filled in.
;
; What the installer does:
;   - Installs for the current user only, so it needs no administrator
;     password. Files go in %LOCALAPPDATA%\Programs\Job Crush.
;   - Adds Job Crush to the Start menu, and to the desktop if asked.
;   - Offers to start Job Crush when it finishes.
;   - Adds an uninstaller. Uninstalling removes the program only. The user's
;     jobs, documents and Active Jobs folder are theirs and are never touched.

#ifndef SourceFolder
  #error SourceFolder is not set. Run build-installer.ps1 instead of this file.
#endif
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif

[Setup]
; The AppId must never change, or Windows treats a new version as a different
; program and installs it twice.
AppId={{6F3C2B1E-9A4D-4E7B-8C21-5D0A7F3E9B64}
AppName=Job Crush
AppVersion={#AppVersion}
AppPublisher=Matt Cleere
AppPublisherURL=https://github.com/ultramanjones/jobcrush
DefaultDirName={localappdata}\Programs\Job Crush
DefaultGroupName=Job Crush
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
OutputDir={#SourcePath}\..\..\build\installer-output
OutputBaseFilename=JobCrushSetup-{#AppVersion}
SetupIconFile={#SourcePath}\..\..\assets\jobcrush-icon.ico
UninstallDisplayIcon={app}\jobcrush.exe
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Tasks]
Name: "desktopicon"; Description: "Put Job Crush on the desktop"; Flags: unchecked

[Files]
Source: "{#SourceFolder}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\Job Crush"; Filename: "{app}\jobcrush.exe"
Name: "{autodesktop}\Job Crush"; Filename: "{app}\jobcrush.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\jobcrush.exe"; Description: "Start Job Crush now"; Flags: nowait postinstall skipifsilent
