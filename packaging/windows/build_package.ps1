# SPDX-License-Identifier: GPL-3.0-or-later
# Builds dist\SLogMetaRaw-<version>-windows-x64.zip (plugin bundle, Python library, launcher, install.ps1) and,
# when Inno Setup (iscc) is installed, dist\SLogMetaRaw-<version>-windows-x64-setup.exe.
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

if (Test-Path -LiteralPath $Stage) { Remove-Item -Recurse -Force -LiteralPath $Stage }
New-Item -ItemType Directory -Force (Join-Path $Stage 'tools'), (Join-Path $Stage 'resolve_script'), $Dist | Out-Null
Copy-Item -Recurse (Join-Path $Build 'SLogMetaRaw.ofx.bundle') $Stage
Copy-Item -Recurse (Join-Path $Root 'slogmetaraw') $Stage
Get-ChildItem -Recurse -Directory -Filter '__pycache__' $Stage | Remove-Item -Recurse -Force
Copy-Item (Join-Path $Root 'tools\render_launcher.py') (Join-Path $Stage 'tools')
Copy-Item (Join-Path $Root 'resolve_script\SLogMetaRaw.py') (Join-Path $Stage 'resolve_script')
Copy-Item (Join-Path $Root 'packaging\windows\install.ps1'), (Join-Path $Root 'LICENSE'), (Join-Path $Root 'README.md') $Stage
$Zip = Join-Path $Dist "$Name.zip"
if (Test-Path -LiteralPath $Zip) { Remove-Item -Force -LiteralPath $Zip }
Compress-Archive -Path $Stage -DestinationPath $Zip
Write-Host $Zip

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
