# SPDX-License-Identifier: GPL-3.0-or-later
# Installs S-Log MetaRaw for DaVinci Resolve on Windows.
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

if ($Uninstall) {
    Invoke-Elevated "Remove-Item -Recurse -Force -LiteralPath '$PluginTarget' -ErrorAction SilentlyContinue"
    foreach ($p in @('lib', 'lib_path', 'pycache')) {
        Remove-Item -Recurse -Force -LiteralPath (Join-Path $Support $p) -ErrorAction SilentlyContinue
    }
    Remove-Item -Force -LiteralPath (Join-Path $Scripts 'S-Log MetaRaw.py') -ErrorAction SilentlyContinue
    Write-Host "Rimosso. Cache e log restano in $Support."
    exit
}

if (-not (Test-Path $Bundle)) { throw 'SLogMetaRaw.ofx.bundle non trovato (compila con cmake, vedi README).' }
$Python = (Get-Command python -ErrorAction SilentlyContinue).Source
if (-not $Python) { $Python = (Get-Command py -ErrorAction SilentlyContinue).Source }
if (-not $Python) { throw 'Python 3 non trovato.' }

$Lib = Join-Path $Support 'lib'
New-Item -ItemType Directory -Force $Lib, $Scripts, (Join-Path $Support 'cache') | Out-Null
Remove-Item -Recurse -Force -LiteralPath (Join-Path $Lib 'slogmetaraw') -ErrorAction SilentlyContinue
Copy-Item -Recurse (Join-Path $Root 'slogmetaraw') $Lib
Get-ChildItem -Recurse -Directory -Filter '__pycache__' $Lib | Remove-Item -Recurse -Force
& $Python (Join-Path $Root 'tools\render_launcher.py') (Join-Path $Root 'resolve_script\SLogMetaRaw.py') $Lib (Join-Path $Scripts 'S-Log MetaRaw.py')
if ($LASTEXITCODE) { throw 'Creazione dello script di Resolve non riuscita.' }
Set-Content -Path (Join-Path $Support 'lib_path') -Value $Lib -Encoding ASCII

Invoke-Elevated ("New-Item -ItemType Directory -Force -Path '$Plugins' | Out-Null; " +
    "Remove-Item -Recurse -Force -LiteralPath '$PluginTarget' -ErrorAction SilentlyContinue; " +
    "Copy-Item -Recurse -LiteralPath '$Bundle' -Destination '$Plugins'")
Write-Host 'Fatto. Riavvia DaVinci Resolve: il nodo e in OpenFX, lo script in Workspace > Scripts > S-Log MetaRaw.'
