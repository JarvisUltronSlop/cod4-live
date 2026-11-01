# Executable icon

`cod4-live.png` is the original 32x32 CoD4 multiplayer executable icon with a
palette-only change. No shapes, scan-line streaks, glow or geometry were redrawn.
The black background, neutral pixels and original transparency mask are retained.

The accent hue is legacy Xbox LIVE Orange, **#F17922**, from the HEX column of
the [November 2010 Xbox color palette](https://alfredotroche.com/assets/xbox-brand-guidelines.pdf).
For each green-dominant pixel, `scripts/recolor-icon.ps1` maps its hue to orange
while preserving its minimum and maximum color channels. This retains the
original brightness and saturation across dark shades and pale highlights.

To reproduce the color conversion, supply the single-image 32x32 ICO extracted
from your own CoD4 multiplayer executable to `scripts/recolor-icon.ps1 -SourceIcon`.
The unmodified reference is not committed. Then run `scripts/package-icon.py`
with Pillow 13.0.0 to package Windows sizes 16, 20, 24, 32, 40, 48, 64, 128 and
256. The 32px frame is exact; larger frames use nearest-neighbor scaling to keep
the original pixels, and smaller frames use Lanczos downsampling.

Client, dedicated server and installer embed the same committed ICO as resource
1. Normal builds need neither the reference icon nor image-processing tools.
Call of Duty branding remains Activision's property; see `docs/THIRD-PARTY.md`.
