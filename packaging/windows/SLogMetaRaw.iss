; SPDX-License-Identifier: GPL-3.0-or-later
; Inno Setup script: iscc /DAppVersion=2.2.0 /DStageDir=<staged folder> /DOutDir=<dist> SLogMetaRaw.iss
[Setup]
AppName=S-Log MetaRaw
AppVersion={#AppVersion}
AppPublisher=Mecena
DefaultDirName={commonappdata}\SLogMetaRaw\installer
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
OutputDir={#OutDir}
OutputBaseFilename=SLogMetaRaw-{#AppVersion}-windows-x64-setup
LicenseFile={#StageDir}\LICENSE
Compression=lzma2
SolidCompression=yes
Uninstallable=yes

[Files]
; the OpenFX bundle goes where Resolve looks for plugins
Source: "{#StageDir}\SLogMetaRaw.ofx.bundle\*"; DestDir: "{commoncf64}\OFX\Plugins\SLogMetaRaw.ofx.bundle"; Flags: recursesubdirs ignoreversion
; the Python library and the launcher script, per user (the user running the installer)
Source: "{#StageDir}\slogmetaraw\*"; DestDir: "{userappdata}\SLogMetaRaw\lib\slogmetaraw"; Flags: recursesubdirs ignoreversion
Source: "{#StageDir}\tools\render_launcher.py"; DestDir: "{app}\tools"; Flags: ignoreversion
Source: "{#StageDir}\resolve_script\SLogMetaRaw.py"; DestDir: "{app}\resolve_script"; Flags: ignoreversion

[Run]
; render the launcher into Resolve's Scripts folder (needs python on PATH; the .py is skipped otherwise)
Filename: "python"; Parameters: """{app}\tools\render_launcher.py"" ""{app}\resolve_script\SLogMetaRaw.py"" ""{userappdata}\SLogMetaRaw\lib"" ""{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility\S-Log MetaRaw.py"""; Flags: runhidden; StatusMsg: "Installo lo script di Resolve..."

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssInstall then
    ForceDirectories(ExpandConstant('{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility'));
  if CurStep = ssPostInstall then
    SaveStringToFile(ExpandConstant('{userappdata}\SLogMetaRaw\lib_path'), ExpandConstant('{userappdata}\SLogMetaRaw\lib') + #10, False);
end;

[UninstallDelete]
Type: files; Name: "{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility\S-Log MetaRaw.py"
Type: filesandordirs; Name: "{userappdata}\SLogMetaRaw\lib"
Type: files; Name: "{userappdata}\SLogMetaRaw\lib_path"
