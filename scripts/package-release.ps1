param()
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+(\.\d+)?$') { throw 'Invalid release version' }
$head = (& git -C $projectRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -or (& git -C $projectRoot status --porcelain)) { throw 'Release requires a clean committed source tree' }
$distRoot = Join-Path $projectRoot 'dist'
$build = Get-Content -LiteralPath (Join-Path $distRoot 'build.json') -Raw | ConvertFrom-Json
if ($build.sourceRevision -ne $head -or $build.version -ne $version -or $build.configuration -ne 'Release') {
    throw 'Run package-installer.ps1 for this exact committed version first'
}
foreach ($binary in @('dist/cod4-live_setup.exe','runtime/cod4-live.exe','runtime/cod4-live_server.exe')) {
    if ((Get-Item -LiteralPath (Join-Path $projectRoot $binary)).VersionInfo.ProductVersion -ne $version) {
        throw "Version mismatch in $binary; rebuild before preparing the release"
    }
}
$pins = Get-Content -LiteralPath (Join-Path $projectRoot 'upstream-lock.json') -Raw | ConvertFrom-Json
$releaseRoot = Join-Path $distRoot "releases/$version"
New-Item -ItemType Directory -Path $releaseRoot -Force | Out-Null
$files = [ordered]@{
    "cod4-live-$version-windows-setup.exe" = 'dist/cod4-live_setup.exe'
    "cod4-live-$version-source.zip" = 'dist/cod4-live-source.zip'
    "OpenAL-Soft-$($pins.OpenAL.version)-source.zip" = 'dist/OpenAL-Soft-source.zip'
    'INSTALL.txt' = 'docs/INSTALLER.md'
    'build.json' = 'dist/build.json'
}
foreach ($entry in $files.GetEnumerator()) {
    Copy-Item -LiteralPath (Join-Path $projectRoot $entry.Value) -Destination (Join-Path $releaseRoot $entry.Key) -Force
}
$checksums = foreach ($name in $files.Keys) {
    $hash = (Get-FileHash -LiteralPath (Join-Path $releaseRoot $name) -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $name"
}
[IO.File]::WriteAllLines((Join-Path $releaseRoot 'SHA256SUMS.txt'), $checksums, [Text.UTF8Encoding]::new($false))
Write-Output "Release $version artifacts: $releaseRoot"
