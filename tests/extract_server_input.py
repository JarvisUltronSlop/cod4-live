"""Compile the actual server input entry points against a small engine fixture."""
from pathlib import Path
import re
import sys

source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
state = re.search(r"struct LiveInputState\n\{.*?LiveInputState liveInputStates\[64\];", source, re.S)
if not state:
    raise SystemExit("Production input state declaration changed; update extraction")
parts = [state.group(0)]
for name in ["SV_ApplyControllerPolicy", "SV_ClientEnterWorld", "SV_ClientThink", "SV_TryControllerAdmission"]:
    match = re.search(rf"^[^\n]*\b{name}\([^\n]*\)\n\{{.*?^\}}", source, re.M | re.S)
    if not match:
        raise SystemExit(f"Production function {name} changed; update extraction")
    line = source.count("\n", 0, match.start()) + 1
    parts.append(f'#line {line} "{source_path.as_posix()}"\n' + match.group(0))
output_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
