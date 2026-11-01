"""Compile the production menu extension and teardown with owned asset fixtures."""
from pathlib import Path
import re
import sys

source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
storage = source[source.index("struct Extension {"):source.index("constexpr const char* layoutValues")]
functions = []
for name in ("Append", "Extend", "BeforeInitialize", "Back"):
    match = re.search(r"^void " + name + r"\([^\n]*\) \{\n.*?^\}", source, re.M | re.S)
    if not match:
        raise SystemExit(f"Production menu lifecycle function changed: {name}")
    line = source.count("\n", 0, match.start()) + 1
    functions.append(f'#line {line} "{source_path.as_posix()}"\n{match.group(0)}')
output_path.write_text(storage + "\n" + "\n".join(functions) + "\n", encoding="utf-8")
