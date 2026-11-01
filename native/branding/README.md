# Executable icon

`cod4-live.png` is the reviewed artwork. `scripts/package-icon.py` converts it
to `engine/src/win32/cod4-live.ico` at 16, 20, 24, 32, 40, 48, 64, 128 and 256
pixels. The client, dedicated server and installer embed the same ICO as resource
1. Builds use the committed ICO and do not require image-generation tools.

The requested reference is the classic green numeral **4** from the owned CoD4
multiplayer executable, not the Steam library's square cover art. The reference
was used locally and is not included here. Call of Duty branding remains
Activision's property; see `docs/THIRD-PARTY.md`.

The accent target is legacy Xbox LIVE Orange, **#F17922**, from the HEX column of
the [November 2010 Xbox color palette](https://alfredotroche.com/assets/xbox-brand-guidelines.pdf).
The glow also contains darker orange and near-white highlights. The original
black-backed square presentation is retained.

Artwork was edited with the built-in `image_gen` tool on October 31, 2026.
The prompt set was:

1. "Use case: precise-object-edit. Asset type: Windows executable icon for
   cod4-live client and dedicated server. Image 1 is the edit target: the original
   Call of Duty 4 green numeral 4 icon with horizontal luminous scan-line streaks.
   Change only its GREEN hue to legacy Xbox LIVE Orange, HEX #F17922, preserving
   the dark shades and pale near-white highlights. Preserve the original
   distinctive angular numeral 4 silhouette, the positions and lengths of its
   horizontal scan-line streaks, its glow and compact square composition. Do not
   add text, logos, circles, borders, backgrounds, artwork or new objects. Make the
   numeral fully legible as the same 4, with no cropping. The exterior must be
   genuinely transparent; keep the original dark interior shading. Deliver a
   clean square icon suitable for conversion to Windows ICO sizes, with the icon
   filling the canvas as in the original."
2. "Precise production icon edit: preserve this orange Call of Duty 4 numeral
   icon and all its shapes, horizontal scan-line streaks, orange #F17922 hue,
   highlights and dark interiors exactly. Restore a completely solid pure BLACK
   opaque background everywhere outside the icon, matching the original
   executable's black-backed square icon. No transparency. No added objects,
   borders or text. Keep the same square composition and complete numeral 4.
   This is only a background-alpha correction; do not redesign the icon."
