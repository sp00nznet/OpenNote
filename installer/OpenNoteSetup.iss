; OpenNote Installer Script
; Inno Setup Script
; https://jrsoftware.org/isinfo.php

#define MyAppName "OpenNote"
#define MyAppVersion "1.0.0"
#define MyAppPublisher "sp00nz"
#define MyAppURL "https://sp00.nz/releases/OpenNote/"
#define MyAppExeName "OpenNote.exe"

[Setup]
; NOTE: The value of AppId uniquely identifies this application.
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}
DefaultDirName={autopf}\{#MyAppName}
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
; Output settings
OutputDir=..\build\installer
OutputBaseFilename=OpenNoteSetup
SetupIconFile=..\res\icon.ico
Compression=lzma
SolidCompression=yes
WizardStyle=modern
; Require admin for Program Files installation
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=dialog
; Uninstall settings
UninstallDisplayIcon={app}\{#MyAppExeName}
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "quicklaunchicon"; Description: "{cm:CreateQuickLaunchIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; OnlyBelowVersion: 6.1; Check: not IsAdminInstallMode

[Files]
; Main executable
Source: "..\build\bin\OpenNote.exe"; DestDir: "{app}"; Flags: ignoreversion

; Include any DLLs if needed (uncomment if required)
; Source: "..\build\bin\*.dll"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon
Name: "{userappdata}\Microsoft\Internet Explorer\Quick Launch\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: quicklaunchicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Add to "Open with" context menu for text files
Root: HKCR; Subkey: "*\shell\OpenWithOpenNote"; ValueType: string; ValueName: ""; ValueData: "Open with OpenNote"; Flags: uninsdeletekey
Root: HKCR; Subkey: "*\shell\OpenWithOpenNote"; ValueType: string; ValueName: "Icon"; ValueData: "{app}\{#MyAppExeName},0"
Root: HKCR; Subkey: "*\shell\OpenWithOpenNote\command"; ValueType: string; ValueName: ""; ValueData: """{app}\{#MyAppExeName}"" ""%1"""

[Code]
// Check if .NET Framework or VC++ Redistributable is needed (optional)
function InitializeSetup(): Boolean;
begin
  Result := True;
end;
