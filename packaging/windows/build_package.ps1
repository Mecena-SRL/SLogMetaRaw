# SPDX-License-Identifier: GPL-3.0-or-later
# Builds dist\SLogMetaRaw-<version>-windows-x64.zip (plugin bundle, Python library, launcher, install.ps1) and,
# when Inno Setup (iscc) is installed, dist\SLogMetaRaw-<version>-windows-x64-setup.exe; also
# dist\LogMetaRawAudio-<version>-windows-x64.zip, the audio plugins alone (VST3) with their own guide.
# Needs cmake, Visual Studio Build Tools (or mingw) and python. $env:OFX_SDK_DIR may point to a local OpenFX SDK.
#   powershell -File packaging\windows\build_package.ps1
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Version = (& python -c "import sys; sys.path.insert(0, sys.argv[1]); import slogmetaraw; print(slogmetaraw.__version__)" $Root).Trim()
$Build = Join-Path $Root 'ofx\SLogMetaRaw\build'
$Name = "SLogMetaRaw-$Version-windows-x64"
$Stage = Join-Path $Root "build\package\$Name"
$Dist = Join-Path $Root 'dist'

$cfg = @('-S', (Join-Path $Root 'ofx\SLogMetaRaw'), '-B', $Build, '-DSLOGMETARAW_BUILD_TESTS=OFF')
if ($env:OFX_SDK_DIR) { $cfg += "-DOFX_SDK_DIR=$env:OFX_SDK_DIR" }
cmake @cfg
if ($LASTEXITCODE) { throw 'cmake configure failed' }
cmake --build $Build --config Release --parallel
if ($LASTEXITCODE) { throw 'cmake build failed' }

# Log MetaRaw Audio (VST3 for Fairlight)
$AudioBuild = Join-Path $Root 'build\audio'
cmake -S (Join-Path $Root 'audio\vst3') -B $AudioBuild
if ($LASTEXITCODE) { throw 'cmake configure (audio) failed' }
cmake --build $AudioBuild --config Release --parallel --target LogMetaRawAudio
if ($LASTEXITCODE) { throw 'cmake build (audio) failed' }

if (Test-Path -LiteralPath $Stage) { Remove-Item -Recurse -Force -LiteralPath $Stage }
New-Item -ItemType Directory -Force (Join-Path $Stage 'tools'), (Join-Path $Stage 'resolve_script'), $Dist | Out-Null
Copy-Item -Recurse (Join-Path $Build 'SLogMetaRaw.ofx.bundle') $Stage
Copy-Item -Recurse (Join-Path $Root 'slogmetaraw') $Stage
Copy-Item -Recurse -LiteralPath (Join-Path $AudioBuild 'VST3\Release\Log MetaRaw Audio.vst3') $Stage
Get-ChildItem -Recurse -Directory -Filter '__pycache__' $Stage | Remove-Item -Recurse -Force
Copy-Item (Join-Path $Root 'tools\render_launcher.py') (Join-Path $Stage 'tools')
Copy-Item (Join-Path $Root 'resolve_script\SLogMetaRaw.py') (Join-Path $Stage 'resolve_script')
Copy-Item (Join-Path $Root 'packaging\windows\install.ps1'), (Join-Path $Root 'LICENSE'), (Join-Path $Root 'README.md') $Stage
$Zip = Join-Path $Dist "$Name.zip"
if (Test-Path -LiteralPath $Zip) { Remove-Item -Force -LiteralPath $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip
Write-Host $Zip

# The audio plugins alone, for any VST3 host: the plugin, the guides (PDF through Edge when present, else HTML)
$AName = "LogMetaRawAudio-$Version-windows-x64"
$AStage = Join-Path $Root "build\package\$AName"
if (Test-Path -LiteralPath $AStage) { Remove-Item -Recurse -Force -LiteralPath $AStage }
New-Item -ItemType Directory -Force $AStage | Out-Null
Copy-Item -Recurse -LiteralPath (Join-Path $AudioBuild 'VST3\Release\Log MetaRaw Audio.vst3') $AStage
Copy-Item (Join-Path $Root 'LICENSE') $AStage
$Guide = Join-Path $Root 'build\package\guide-audio'
if (Test-Path -LiteralPath $Guide) { Remove-Item -Recurse -Force -LiteralPath $Guide }
Copy-Item -Recurse (Join-Path $Root 'packaging\guide') $Guide
$edge = @("${env:ProgramFiles(x86)}\Microsoft\Edge\Application\msedge.exe",
          "$env:ProgramFiles\Microsoft\Edge\Application\msedge.exe") | Where-Object { Test-Path $_ } | Select-Object -First 1
foreach ($g in @(@('guida-audio.html', 'Guida Log MetaRaw Audio (italiano)'), @('guide-audio-en.html', 'Log MetaRaw Audio Guide (english)'))) {
    $html = Join-Path $Guide $g[0]
    (Get-Content -Raw -Encoding UTF8 $html).Replace('@VERSION@', $Version) | Set-Content -Encoding UTF8 $html
    $pdf = Join-Path $AStage ($g[1] + '.pdf')
    if ($edge) {
        & $edge --headless --disable-gpu --no-pdf-header-footer "--print-to-pdf=$pdf" ([System.Uri]$html).AbsoluteUri | Out-Null
        Start-Sleep -Seconds 3
    }
    if (-not (Test-Path -LiteralPath $pdf)) {   # no Edge: the guide as a page, with its figures
        if (-not (Test-Path (Join-Path $AStage 'guida'))) {
            New-Item -ItemType Directory -Force (Join-Path $AStage 'guida\img') | Out-Null
            Copy-Item (Join-Path $Guide 'icon.png') (Join-Path $AStage 'guida')
            Copy-Item (Join-Path $Guide 'img\audio-*.png') (Join-Path $AStage 'guida\img')
        }
        Copy-Item $html (Join-Path $AStage 'guida')
    }
}
@"
Log MetaRaw Audio $Version - VST3 per Windows

Copia la cartella "Log MetaRaw Audio.vst3" in C:\Program Files\Common Files\VST3, poi riapri la DAW
(Fairlight, Cubase, Reaper, Ableton Live, Studio One...) e, se serve, rifai la scansione dei plugin.
Due plugin: Log MetaRaw Ripresa (sulle tracce) e Log MetaRaw Mix e supporto (sul bus o sul master).
Per toglierli cancella la stessa cartella. La guida e' accanto a questo file.

Copy the "Log MetaRaw Audio.vst3" folder to C:\Program Files\Common Files\VST3 and reopen the DAW.
"@ | Set-Content -Encoding UTF8 (Join-Path $AStage 'LEGGIMI.txt')
$AZip = Join-Path $Dist "$AName.zip"
if (Test-Path -LiteralPath $AZip) { Remove-Item -Force -LiteralPath $AZip }
Compress-Archive -Path $AStage -DestinationPath $AZip
Write-Host $AZip

$iscc = Get-Command iscc -ErrorAction SilentlyContinue
$exe = $null
if ($iscc) { $exe = $iscc.Source }
elseif (Test-Path "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe") { $exe = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe" }
if ($exe) {
    & $exe "/DAppVersion=$Version" "/DStageDir=$Stage" "/DOutDir=$Dist" (Join-Path $Root 'packaging\windows\SLogMetaRaw.iss')
    if ($LASTEXITCODE) { throw 'Inno Setup failed' }
} else {
    Write-Host 'Inno Setup non trovato: creato solo lo zip.'
}
