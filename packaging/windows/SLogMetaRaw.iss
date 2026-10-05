; SPDX-License-Identifier: GPL-3.0-or-later
; Inno Setup script: iscc /DAppVersion=2.3.0 /DStageDir=<staged folder> /DOutDir=<dist> SLogMetaRaw.iss
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
; the launcher goes in as is: it finds the library through lib_path, so no Python is needed to install it
; (Resolve 20 and earlier have none of their own, and python.org leaves its Python off the PATH)
Source: "{#StageDir}\resolve_script\SLogMetaRaw.py"; DestDir: "{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility"; DestName: "S-Log MetaRaw.py"; Flags: ignoreversion

[Code]
procedure CurStepChanged(CurStep: TSetupStep);
var
  Lines: TArrayOfString;
begin
  if CurStep = ssInstall then
    ForceDirectories(ExpandConstant('{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility'));
  if CurStep = ssPostInstall then begin
    { UTF-8: a user folder with accents must reach the plugin and the launcher intact }
    SetArrayLength(Lines, 1);
    Lines[0] := ExpandConstant('{userappdata}\SLogMetaRaw\lib');
    SaveStringsToUTF8File(ExpandConstant('{userappdata}\SLogMetaRaw\lib_path'), Lines, False);
  end;
end;

[UninstallDelete]
Type: files; Name: "{userappdata}\Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility\S-Log MetaRaw.py"
Type: filesandordirs; Name: "{userappdata}\SLogMetaRaw\lib"
Type: files; Name: "{userappdata}\SLogMetaRaw\lib_path"
