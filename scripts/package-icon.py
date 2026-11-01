"""Package the reviewed branding PNG into Windows ICO sizes (no artwork edits).

Only needed when updating the icon: python -m pip install Pillow==13.0.0
Normal client/server builds consume the committed ICO directly.
"""
from pathlib import Path
from PIL import Image

root = Path(__file__).resolve().parent.parent
source = root / "native/branding/cod4-live.png"
destination = root / "engine/src/win32/cod4-live.ico"
sizes = [(n,n) for n in (16,20,24,32,40,48,64,128,256)]
with Image.open(source) as image:
    if image.width != image.height or image.width < 256:
        raise SystemExit("The reviewed icon must be square and at least 256 pixels")
    image.convert("RGBA").save(destination, format="ICO", sizes=sizes)
with Image.open(destination) as icon:
    assert icon.ico.sizes() == set(sizes)
print(f"Packaged {destination.name}: {len(sizes)} sizes, 16 through 256 pixels")
