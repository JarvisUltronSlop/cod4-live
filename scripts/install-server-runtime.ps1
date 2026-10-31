param(
    [string]$InstallDirectory = '',
    [string]$SteamCmdDirectory = (Join-Path $env:LOCALAPPDATA 'cod4-live-tools/steamcmd')
)
$ErrorActionPreference = 'Stop'
if (!$InstallDirectory) {
    $InstallDirectory = if (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'cod4-live_server.exe')) {
        $PSScriptRoot
    } else { Join-Path (Split-Path -Parent $PSScriptRoot) 'runtime' }
}
$installRoot = (Resolve-Path -LiteralPath $InstallDirectory).Path
if (!(Test-Path -LiteralPath (Join-Path $installRoot 'cod4-live_server.exe') -PathType Leaf)) {
    throw 'Select the standalone cod4-live installation containing cod4-live_server.exe.'
}
if (Test-Path -LiteralPath (Join-Path $installRoot 'iw3mp.exe')) {
    throw 'Select the standalone cod4-live folder, not the original Steam game.'
}
$running = Get-CimInstance Win32_Process | Where-Object {
    $_.ExecutablePath -and
    ([IO.Path]::GetDirectoryName($_.ExecutablePath) -eq $installRoot) -and
    ($_.Name -eq 'cod4-live.exe' -or $_.Name -eq 'cod4-live_server.exe')
}
if ($running) { throw 'Close cod4-live and its dedicated server before installing the runtime.' }
function Assert-ValveSignature([string]$Path) {
    $signature = Get-AuthenticodeSignature -LiteralPath $Path
    if ($signature.Status -ne 'Valid' -or
        $signature.SignerCertificate.Subject -notmatch '(^|,\s*)O=Valve( Corp\.)?(,|$)') {
        throw "Valve signature verification failed: $Path"
    }
}
New-Item -ItemType Directory -Path $SteamCmdDirectory -Force | Out-Null
$cmdRoot = (Resolve-Path -LiteralPath $SteamCmdDirectory).Path
$steamCmd = Join-Path $cmdRoot 'steamcmd.exe'
if (!(Test-Path -LiteralPath $steamCmd)) {
    [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
    $archive = Join-Path $cmdRoot 'steamcmd.zip'
    Invoke-WebRequest -UseBasicParsing -Uri 'https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip' -OutFile $archive
    Expand-Archive -LiteralPath $archive -DestinationPath $cmdRoot -Force
}
Assert-ValveSignature $steamCmd
$redist = Join-Path $cmdRoot 'server-redist'
# Download the dedicated runtime directly from Valve. This requires no account
# credentials, game publisher privileges or modification of the user's Steam.
& $steamCmd '+@ShutdownOnFailedCommand' '1' '+@NoPromptForPassword' '1' '+force_install_dir' $redist '+login' 'anonymous' '+app_update' '1007' 'validate' '+quit'
# SteamCMD's first self-update may exit before the requested app operation.
$manifest = Join-Path $redist 'steamapps/appmanifest_1007.acf'
if (!(Test-Path -LiteralPath $manifest)) {
    Assert-ValveSignature $steamCmd
    & $steamCmd '+@ShutdownOnFailedCommand' '1' '+@NoPromptForPassword' '1' '+force_install_dir' $redist '+login' 'anonymous' '+app_update' '1007' 'validate' '+quit'
}
if (!(Test-Path -LiteralPath $manifest)) { throw 'SteamCMD did not install app 1007. Check its network/download output.' }
$state = Get-Content -LiteralPath $manifest -Raw
if ($state -notmatch '"appid"\s+"1007"' -or $state -notmatch '"StateFlags"\s+"4"' -or $state -notmatch '"UpdateResult"\s+"0"') {
    throw 'SteamCMD has not finished a successful runtime installation. Retry after the download completes.'
}
$names = @('steamclient.dll', 'tier0_s.dll', 'vstdlib_s.dll', 'steamwebrtc.dll')
foreach ($name in $names) {
    $source = Join-Path $redist $name
    Assert-ValveSignature $source
    $data = [IO.File]::ReadAllBytes($source)
    $pe = [BitConverter]::ToInt32($data, 60)
    if ($pe -lt 64 -or $pe + 6 -ge $data.Length -or
        [BitConverter]::ToUInt32($data, $pe) -ne 0x4550 -or
        [BitConverter]::ToUInt16($data, $pe + 4) -ne 0x14c) {
        throw "Runtime is not a Win32 PE binary: $name"
    }
}
foreach ($name in $names) {
    Copy-Item -LiteralPath (Join-Path $redist $name) -Destination (Join-Path $installRoot $name) -Force
}
Copy-Item -LiteralPath $manifest -Destination (Join-Path $installRoot 'steam-server-runtime.acf') -Force
Write-Output "Valve's Win32 dedicated-server runtime is installed in $installRoot."
