# SPDX-License-Identifier: GPL-3.0-or-later
# Installs Log MetaRaw for DaVinci Resolve on Windows.
#   powershell -ExecutionPolicy Bypass -File install.ps1             install (asks for administrator rights)
#   powershell -ExecutionPolicy Bypass -File install.ps1 -Uninstall remove what this script installed
# Run from the unpacked release (SLogMetaRaw.ofx.bundle, slogmetaraw\, tools\, resolve_script\ next to it) or from
# a checkout after `cmake --build` (the bundle is then in ofx\SLogMetaRaw\build).
param([switch]$Uninstall)
$ErrorActionPreference = 'Stop'
$Here = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = if (Test-Path (Join-Path $Here 'slogmetaraw')) { $Here } else { (Resolve-Path (Join-Path $Here '..\..')).Path }
$Bundle = Join-Path $Here 'SLogMetaRaw.ofx.bundle'
if (-not (Test-Path $Bundle)) { $Bundle = Join-Path $Root 'ofx\SLogMetaRaw\build\SLogMetaRaw.ofx.bundle' }

$Plugins = Join-Path $env:CommonProgramFiles 'OFX\Plugins'
$Support = Join-Path $env:APPDATA 'SLogMetaRaw'
$Scripts = Join-Path $env:APPDATA 'Blackmagic Design\DaVinci Resolve\Support\Fusion\Scripts\Utility'

# The plugin folder is under Program Files, so the plugin copy needs administrator rights. The per-user parts
# (library, launcher) are written first, as this user; only the plugin copy is elevated.
function Invoke-Elevated([string]$Command) {
    $isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
        [Security.Principal.WindowsBuiltInRole]::Administrator)
    if ($isAdmin) { Invoke-Expression $Command; return }
    $p = Start-Process powershell -Verb RunAs -Wait -PassThru -ArgumentList @('-NoProfile', '-Command', $Command)
    if ($p.ExitCode) { throw "Operazione con diritti di amministratore non riuscita ($($p.ExitCode))." }
}

$PluginTarget = Join-Path $Plugins 'SLogMetaRaw.ofx.bundle'
# Log MetaRaw Audio (VST3 for Fairlight), when the release or the build has it
$Vst3 = Join-Path $env:CommonProgramFiles 'VST3'
$Audio = Join-Path $Here 'Log MetaRaw Audio.vst3'
if (-not (Test-Path -LiteralPath $Audio)) { $Audio = Join-Path $Root 'build\audio\VST3\Release\Log MetaRaw Audio.vst3' }
$AudioTarget = Join-Path $Vst3 'Log MetaRaw Audio.vst3'

if ($Uninstall) {
    Invoke-Elevated ("Remove-Item -Recurse -Force -LiteralPath '$PluginTarget' -ErrorAction SilentlyContinue; " +
        "Remove-Item -Recurse -Force -LiteralPath '$AudioTarget' -ErrorAction SilentlyContinue")
    foreach ($p in @('lib', 'lib_path', 'pycache')) {
        Remove-Item -Recurse -Force -LiteralPath (Join-Path $Support $p) -ErrorAction SilentlyContinue
    }
    Remove-Item -Force -LiteralPath (Join-Path $Scripts 'Log MetaRaw.py') -ErrorAction SilentlyContinue
    Remove-Item -Force -LiteralPath (Join-Path $Scripts 'S-Log MetaRaw.py') -ErrorAction SilentlyContinue
    Write-Host "Rimosso. Cache e log restano in $Support."
    exit
}

if (-not (Test-Path $Bundle)) { throw 'SLogMetaRaw.ofx.bundle non trovato (compila con cmake, vedi README).' }
$Lib = Join-Path $Support 'lib'
New-Item -ItemType Directory -Force $Lib, $Scripts, (Join-Path $Support 'cache') | Out-Null
Remove-Item -Recurse -Force -LiteralPath (Join-Path $Lib 'slogmetaraw') -ErrorAction SilentlyContinue
Copy-Item -Recurse (Join-Path $Root 'slogmetaraw') $Lib
Get-ChildItem -Recurse -Directory -Filter '__pycache__' $Lib | Remove-Item -Recurse -Force
# the launcher goes in as is and finds the library through lib_path: no Python needed to install
# (Resolve 20 and earlier have none of their own). UTF-8 without BOM keeps an accented user folder intact.
[IO.File]::WriteAllText((Join-Path $Support 'lib_path'), "$Lib`n", (New-Object Text.UTF8Encoding $false))
Copy-Item -LiteralPath (Join-Path $Root 'resolve_script\SLogMetaRaw.py') -Destination (Join-Path $Scripts 'Log MetaRaw.py') -Force

$Copy = "New-Item -ItemType Directory -Force -Path '$Plugins' | Out-Null; " +
    "Remove-Item -Recurse -Force -LiteralPath '$PluginTarget' -ErrorAction SilentlyContinue; " +
    "Copy-Item -Recurse -LiteralPath '$Bundle' -Destination '$Plugins'"
if (Test-Path -LiteralPath $Audio) {
    $Copy += "; New-Item -ItemType Directory -Force -Path '$Vst3' | Out-Null; " +
        "Remove-Item -Recurse -Force -LiteralPath '$AudioTarget' -ErrorAction SilentlyContinue; " +
        "Copy-Item -Recurse -LiteralPath '$Audio' -Destination '$Vst3'"
}
Invoke-Elevated $Copy
Write-Host 'Fatto. Riavvia DaVinci Resolve: il nodo e in OpenFX, lo script in Workspace > Scripts > Log MetaRaw.'
