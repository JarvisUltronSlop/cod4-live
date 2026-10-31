"""Port pinned IW4x UI image containers to CoD4 without changing pixel blocks.

IW4's 32-byte IWI8 header differs from CoD4's 28-byte IWI6 header. These
upstream glyphs are single-mip 2D images. Eight unused 'big' variants declare
42x42 but have truncated DXT data; those are retained only as upstream source.
The original bytes, upstream license and source hashes are retained.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / 'vendor/iw4x-controller-images'
DESTINATION = ROOT / 'assets/controller/main/images/live_controller'
MANIFEST = ROOT / 'assets/controller/notices/IW4x-controller-images.json'


def convert(data):
    if len(data) < 32 or data[:4] != b'IWi\x08':
        raise ValueError('Expected an IW4 IWI8 container')
    flags, = struct.unpack_from('<I', data, 4)
    fmt = data[8]
    width, height, depth = struct.unpack_from('<3H', data, 10)
    sizes = struct.unpack_from('<4I', data, 16)
    if flags != 0x173 or depth != 1 or any(size != len(data) for size in sizes):
        raise ValueError('Expected the pinned single-mip, 2D controller image')
    pixels = data[32:]
    original_size = [width, height]
    if fmt == 1:
        expected = width * height * 4
    elif fmt in (11, 12, 13):
        block = 8 if fmt == 11 else 16
        expected = ((width + 3) // 4) * ((height + 3) // 4) * block
    else:
        raise ValueError(f'Unsupported glyph format {fmt}')
    if expected != len(pixels) or not width or not height:
        raise ValueError('Image dimensions do not match the encoded pixel block count')
    # No picmip, no mipmaps, clamp U/V. Format enumeration and pixel ordering
    # match in the pinned IW4x and KisakCOD image loaders.
    length = 28 + len(pixels)
    header = struct.pack('<3sBBB3H4I', b'IWi', 6, fmt, 0xC3, width, height, 1, *([length] * 4))
    return header + pixels, original_size, [width, height]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true', help='Verify shipped containers and provenance without writing')
    args = parser.parse_args()
    manifest = json.loads(MANIFEST.read_text(encoding='utf-8'))
    source_entries = {Path(entry['path']).name: entry for entry in manifest['files']}
    conversions = []
    excluded = []
    for name, entry in sorted(source_entries.items()):
        source = (SOURCE / name).read_bytes()
        if hashlib.sha256(source).hexdigest() != entry['sha256']:
            raise ValueError(f'Upstream image hash mismatch: {name}')
        if name.endswith('_big.iwi'):
            # None of these are referenced by the pinned glyph table. Retain
            # their originals for provenance, but never feed malformed DXT to
            # the renderer or guess replacement pixel data.
            if len(source) != 1632 or struct.unpack_from('<3H', source, 10) != (42, 42, 1):
                raise ValueError(f'Unexpected optional image: {name}')
            excluded.append({'file': name, 'reason': 'Unused upstream variant has incomplete DXT data for its declared dimensions'})
            if (DESTINATION / name).exists():
                raise ValueError(f'Malformed optional image must not be staged: {name}')
            continue
        output, before, after = convert(source)
        target = DESTINATION / name
        if args.check:
            if target.read_bytes() != output:
                raise ValueError(f'Converted image mismatch: {name}')
        else:
            target.write_bytes(output)
        conversions.append({'file': name, 'installedPath': 'main/images/live_controller/' + name,
                            'sha256': hashlib.sha256(output).hexdigest(),
                            'sourceDimensions': before, 'dimensions': after})
    if args.check:
        if manifest.get('convertedFiles') != conversions:
            raise ValueError('Converted provenance manifest mismatch')
        if manifest.get('excludedFiles') != excluded:
            raise ValueError('Excluded provenance manifest mismatch')
    else:
        manifest['conversion'] = 'IWI8 to IWI6 container; identical pixel payload and dimensions; unused malformed big variants excluded'
        manifest['convertedFiles'] = conversions
        manifest['excludedFiles'] = excluded
        MANIFEST.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf-8')
    print(f'Validated {len(conversions)} IW4x controller images; pixel payloads unchanged')


if __name__ == '__main__':
    main()
