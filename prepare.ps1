param([string]$GameDirectory = 'D:\SteamLibrary\steamapps\common\Call of Duty 4', [switch]$SkipAssets)
$ErrorActionPreference = 'Stop'
$projectRoot = $PSScriptRoot
$pins = Get-Content -LiteralPath (Join-Path $projectRoot 'upstream-lock.json') -Raw | ConvertFrom-Json
foreach ($name in @('iw3mp.exe','localization.txt','binkw32.dll','main','zone')) {
    if (!(Test-Path -LiteralPath (Join-Path $GameDirectory $name))) { throw "Select your owned Steam CoD4 (2007) installation: missing $name" }
}
$sdkRoot = Join-Path $projectRoot '.cache/dxsdk'
if (!(Test-Path -LiteralPath (Join-Path $sdkRoot 'build/native/include/d3dx9.h'))) {
    New-Item -ItemType Directory -Force -Path $sdkRoot | Out-Null
    $archive = Join-Path $sdkRoot 'dxsdk.zip'
    Invoke-WebRequest -Uri 'https://api.nuget.org/v3-flatcontainer/microsoft.dxsdk.d3dx/9.29.952.8/microsoft.dxsdk.d3dx.9.29.952.8.nupkg' -OutFile $archive
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash.ToLowerInvariant() -ne $pins.DirectX.sha256) { throw 'DirectX checksum mismatch' }
    Expand-Archive -LiteralPath $archive -DestinationPath $sdkRoot -Force
}
# Restore build-only binary dependencies from the exact source revision; never commit them.
foreach ($relative in @('binklib/binkw32.lib','steamsdk/steam_api.lib','steamsdk/steam_api.dll')) {
    $target = Join-Path $projectRoot ('engine/deps/' + $relative)
    if (!(Test-Path -LiteralPath $target)) {
        Invoke-WebRequest -Uri ('https://raw.githubusercontent.com/SwagSoftware/KisakCOD/' + $pins.kisakcod.revision + '/deps/' + $relative) -OutFile $target
    }
}
$runtimeRoot = Join-Path $projectRoot 'runtime'
New-Item -ItemType Directory -Path $runtimeRoot -Force | Out-Null
if (!$SkipAssets) {
    foreach ($directory in @('main','zone')) {
        $source = Join-Path $GameDirectory $directory
        if ((Test-Path -LiteralPath $source) -and !(Test-Path -LiteralPath (Join-Path $runtimeRoot $directory))) { Copy-Item -LiteralPath $source -Destination $runtimeRoot -Recurse }
    }
    Copy-Item -LiteralPath (Join-Path $GameDirectory 'localization.txt') -Destination $runtimeRoot -Force
}
foreach ($name in @('binkw32.dll')) { Copy-Item -LiteralPath (Join-Path $GameDirectory $name) -Destination $runtimeRoot -Force }
Copy-Item -LiteralPath (Join-Path $projectRoot 'engine/deps/steamsdk/steam_api.dll') -Destination $runtimeRoot -Force
Copy-Item -LiteralPath (Join-Path $sdkRoot 'build/native/release/bin/x86/D3DX9_43.dll') -Destination $runtimeRoot -Force
Write-Output "Prepared cod4-live at $runtimeRoot"
