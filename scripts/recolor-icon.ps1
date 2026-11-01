param([Parameter(Mandatory = $true)][string]$SourceIcon)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$sourcePath = (Resolve-Path -LiteralPath $SourceIcon).Path
$destination = Join-Path $projectRoot 'engine/src/win32/cod4-live.ico'
if ($sourcePath -eq $destination) { throw 'Supply the original CoD4 icon separately' }

# Preserve every byte except green pixel colors in the original executable's
# single 32x32, 24-bit DIB icon, including its mask and geometry.
$pixels = [IO.File]::ReadAllBytes($sourcePath)
if ($pixels.Length -lt 62 -or [BitConverter]::ToUInt16($pixels, 0) -ne 0 -or
    [BitConverter]::ToUInt16($pixels, 2) -ne 1 -or
    [BitConverter]::ToUInt16($pixels, 4) -ne 1) { throw 'Expected a single-image ICO' }
$offset = [int][BitConverter]::ToUInt32($pixels, 18)
if ($offset -lt 22 -or $offset -gt $pixels.Length - 40) { throw 'Invalid DIB offset' }
$width = [BitConverter]::ToInt32($pixels, $offset + 4)
$height = [BitConverter]::ToInt32($pixels, $offset + 8) / 2
if ([BitConverter]::ToUInt32($pixels, $offset) -ne 40 -or $width -ne 32 -or $height -ne 32 -or
    [BitConverter]::ToUInt16($pixels, $offset + 12) -ne 1 -or
    [BitConverter]::ToUInt16($pixels, $offset + 14) -ne 24 -or
    [BitConverter]::ToUInt32($pixels, $offset + 16) -ne 0) { throw 'Expected the original 32x32 24-bit DIB' }
$stride = [int]([Math]::Ceiling($width * 3 / 4.0) * 4)
if ($pixels.Length -lt $offset + 40 + $stride * $height + 128) { throw 'Truncated icon' }
$changed = 0
for ($y = 0; $y -lt $height; ++$y) {
    for ($x = 0; $x -lt $width; ++$x) {
        $pixel = $offset + 40 + $y * $stride + $x * 3
        $blue = [int]$pixels[$pixel]
        $green = [int]$pixels[$pixel + 1]
        $red = [int]$pixels[$pixel + 2]
        $minimum = [Math]::Min($red, $blue)
        if ($green -ge $red -and $green -ge $blue -and $green -gt $minimum) {
            # Xbox LIVE Orange #F17922 hue, keeping saturation and brightness.
            $pixels[$pixel] = [byte]$minimum
            $pixels[$pixel + 1] = [byte][Math]::Round($minimum + ($green - $minimum) * (121.0 - 34) / (241 - 34))
            $pixels[$pixel + 2] = [byte]$green
            ++$changed
        }
    }
}
[IO.File]::WriteAllBytes($destination, $pixels)
Add-Type -AssemblyName System.Drawing
$stream = [IO.MemoryStream]::new($pixels, $false)
$icon = [Drawing.Icon]::new($stream)
$bitmap = $icon.ToBitmap()
try {
    $bitmap.Save((Join-Path $projectRoot 'native/branding/cod4-live.png'), [Drawing.Imaging.ImageFormat]::Png)
} finally {
    $bitmap.Dispose()
    $icon.Dispose()
    $stream.Dispose()
}
Write-Output "Recolored $changed pixels; original icon geometry and mask retained"
