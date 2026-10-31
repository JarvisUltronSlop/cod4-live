"""Exercise production packed string/vector reference increments in isolation."""
from pathlib import Path
import re
import sys

script_dir, output_path = map(Path, sys.argv[1:])
functions = {
    "scr_stringlist.cpp": [
        r"SL_ConvertFromRefString\(RefString \*refString\)",
        r"SL_AddPackedStringReference\(RefString\* refStr, uint32_t user, bool addUser\)",
        r"SL_AddUserInternal\(RefString\* refStr, uint32_t user\)",
        r"SL_AddRefToString\(uint32_t stringValue\)",
        r"GetRefString\(uint32_t stringValue\)",
    ],
    "scr_variable.cpp": [r"AddRefToVector\(float const\* vectorValue\)"],
}
parts = []
for filename, signatures in functions.items():
    path = script_dir / filename
    source = path.read_text(encoding="utf-8-sig")
    for signature in signatures:
        matches = list(re.finditer(rf"^[^\n]*\b{signature}\n\{{.*?^\}}", source, re.M | re.S))
        if len(matches) != 1:
            raise SystemExit(f"Expected one production definition {signature}, found {len(matches)}")
        match = matches[0]
        line = source.count("\n", 0, match.start()) + 1
        parts.append(f'#line {line} "{path.as_posix()}"\n' + match.group(0))
output_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
