param([string]$Configuration = 'Release', [switch]$SkipBuild, [switch]$TestPayload)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$releaseVersion = (Get-Content -LiteralPath (Join-Path $projectRoot 'VERSION') -Raw).Trim()
$outputRoot = Join-Path $projectRoot 'dist'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
if (!$SkipBuild -and !$TestPayload) {
    & (Join-Path $projectRoot 'build.ps1') -Configuration $Configuration
    if ($LASTEXITCODE) { throw 'Engine build failed' }
}
if (!$TestPayload) {
    $changes = & git -C $projectRoot status --porcelain
    if ($changes) { throw 'Commit the tested source before packaging so the source archive matches the installer.' }
}
$stage = Join-Path $outputRoot ('payload-' + [Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
$entries = [Collections.Generic.List[object]]::new()
function Add-PayloadFile([string]$Source, [string]$Relative) {
    if ($Relative -match '\.(iwd|ff|bik|flt)$' -or $Relative -match '(^|/)(iw3mp|iw3sp|binkw32|mss32)\.' -or $Relative.Contains('..')) { throw "Forbidden payload: $Relative" }
    if (!(Test-Path -LiteralPath $Source -PathType Leaf)) { throw "Missing payload: $Source" }
    if ($entries | Where-Object { $_.relative -ieq $Relative }) { throw "Duplicate payload path: $Relative" }
    $name = 'file' + $entries.Count.ToString('D5')
    Copy-Item -LiteralPath $Source -Destination (Join-Path $stage $name)
    $entries.Add([pscustomobject]@{file=$name; relative=$Relative; sha256=(Get-FileHash -LiteralPath $Source -Algorithm SHA256).Hash.ToLowerInvariant()})
}
foreach ($name in @('cod4-live.exe','cod4-live_server.exe','steam_api.dll','D3DX9_43.dll')) {
    $source = Join-Path $projectRoot ('runtime/' + $name)
    if ($TestPayload) {
        $source = Join-Path $stage ('fixture-' + $name)
        [IO.File]::WriteAllText($source, 'TEST FIXTURE: ' + $name)
    }
    Add-PayloadFile $source $name
}
Add-PayloadFile (Join-Path $projectRoot 'LICENSE') 'notices/LICENSE.txt'
Add-PayloadFile (Join-Path $projectRoot 'upstream-lock.json') 'notices/upstream-lock.json'
Add-PayloadFile (Join-Path $projectRoot 'scripts/install-server-runtime.ps1') 'install-server-runtime.ps1'
foreach ($tree in @('botwarfare', 'controller')) {
    $sourceRoot = Join-Path $projectRoot "assets/$tree"
    if (!(Test-Path -LiteralPath $sourceRoot -PathType Container)) { continue }
    foreach ($file in Get-ChildItem -LiteralPath $sourceRoot -File -Recurse) {
        $relative = $file.FullName.Substring($sourceRoot.Length).TrimStart('\','/').Replace('\','/')
        Add-PayloadFile $file.FullName $relative
    }
}
if (Test-Path -LiteralPath (Join-Path $projectRoot 'docs/INSTALLER.md')) { Add-PayloadFile (Join-Path $projectRoot 'docs/INSTALLER.md') 'INSTALLER.txt' }
foreach ($guide in @('CONTROLLER','NETWORK','STEAM_AUTH','BOT_WARFARE','VALIDATION')) {
    Add-PayloadFile (Join-Path $projectRoot "docs/$guide.md") "docs/$guide.txt"
}
if (Test-Path -LiteralPath (Join-Path $projectRoot 'docs/THIRD-PARTY.md')) { Add-PayloadFile (Join-Path $projectRoot 'docs/THIRD-PARTY.md') 'notices/THIRD-PARTY.txt' }
foreach ($notice in @('LICENSE.txt', 'NOTICE.md')) {
    $source = Join-Path $projectRoot ('.cache/dxsdk/' + $notice)
    if (!$TestPayload -or (Test-Path -LiteralPath $source)) {
        Add-PayloadFile $source ('notices/Microsoft-D3DX-' + $notice)
    }
}
if (!$TestPayload) {
    Add-PayloadFile (Join-Path $projectRoot 'build/_deps/openal-src/COPYING') 'notices/OpenAL-COPYING.txt'
    Add-PayloadFile (Join-Path $projectRoot 'build/_deps/openal-src/LICENSE-pffft') 'notices/OpenAL-pffft-LICENSE.txt'
    $revision = (& git -C $projectRoot rev-parse HEAD).Trim()
    if ($LASTEXITCODE) { throw 'Cannot identify source revision' }
    $releaseInfo = [ordered]@{ project='cod4-live'; version=$releaseVersion; platform='windows-x86'; protocol=10003; sourceRevision=$revision; configuration=$Configuration; packagedUtc=[DateTime]::UtcNow.ToString('o') }
    $releaseInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $stage 'build.json') -Encoding utf8
    Add-PayloadFile (Join-Path $stage 'build.json') 'notices/build.json'
}
$manifest = ($entries | ForEach-Object { "$($_.file)`t$($_.sha256)`t$($_.relative)" }) -join "`n"
[IO.File]::WriteAllText((Join-Path $stage 'manifest.tsv'), $manifest + "`n", [Text.UTF8Encoding]::new($false))
$cabName = if ($TestPayload) { 'test-payload.cab' } else { 'cod4-live-payload.cab' }
$cab = Join-Path $outputRoot $cabName
$ddf = @('.OPTION EXPLICIT','.Set Cabinet=ON','.Set Compress=ON','.Set CompressionType=MSZIP','.Set MaxDiskSize=0',('.Set DiskDirectoryTemplate="'+$outputRoot+'"'),('.Set CabinetNameTemplate='+$cabName),('.Set RptFileName="'+(Join-Path $stage 'setup.rpt')+'"'),('.Set InfFileName="'+(Join-Path $stage 'setup.inf')+'"'))
$ddf += '"'+(Join-Path $stage 'manifest.tsv')+'" manifest.tsv'
$ddf += $entries | ForEach-Object { '"'+(Join-Path $stage $_.file)+'" '+$_.file }
[IO.File]::WriteAllLines((Join-Path $stage 'payload.ddf'), $ddf, [Text.UTF8Encoding]::new($false))
& makecab.exe /F (Join-Path $stage 'payload.ddf') | Out-File (Join-Path $stage 'cabinet-build.log')
if ($LASTEXITCODE) { throw 'CAB creation failed' }
$buildRoot = Join-Path $projectRoot 'build-installer'
& cmake -S $projectRoot -B $buildRoot -G 'Visual Studio 17 2022' -A Win32 '-DCOD4LIVE_BUILD_ENGINE=OFF' ('-DCOD4LIVE_INSTALLER_PAYLOAD='+$cab.Replace('\','/'))
if ($LASTEXITCODE) { throw 'Installer configure failed' }
& cmake --build $buildRoot --config $Configuration --parallel 8
if ($LASTEXITCODE) { throw 'Installer build failed' }
& ctest --test-dir $buildRoot -C $Configuration --output-on-failure
if ($LASTEXITCODE) { throw 'Installer tests failed' }
if (!$TestPayload) {
    Copy-Item -LiteralPath (Join-Path $buildRoot "$Configuration/cod4-live_setup.exe") -Destination $outputRoot -Force
    & git -C $projectRoot archive --format=zip ('--output='+ (Join-Path $outputRoot 'cod4-live-source.zip')) HEAD
    if ($LASTEXITCODE) { throw 'Source archive failed' }
    & git -C (Join-Path $projectRoot 'build/_deps/openal-src') archive --format=zip ('--output='+ (Join-Path $outputRoot 'OpenAL-Soft-source.zip')) HEAD
    if ($LASTEXITCODE) { throw 'OpenAL corresponding source archive failed' }
    $entries | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'payload-manifest.json') -Encoding utf8
    $releaseInfo | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $outputRoot 'build.json') -Encoding utf8
}
Write-Output $(if ($TestPayload) { 'Installer fixture tests passed; no playable installer produced.' } else { Join-Path $outputRoot 'cod4-live_setup.exe' })
