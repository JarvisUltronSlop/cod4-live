"""Compile the production player-damage dispatcher without linking the server."""
from pathlib import Path
import re
import sys
source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
match = re.search(r"^void __cdecl G_DamageClient\(.*?^\}", source, re.M | re.S)
if not match:
    raise SystemExit("Production G_DamageClient changed")
line = source.count("\n", 0, match.start()) + 1
output_path.write_text(f'#line {line} "{source_path.as_posix()}"\n' + match.group(0) + "\n", encoding="utf-8")
