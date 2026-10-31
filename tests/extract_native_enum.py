"""Exercise real native menu/focus dispatch and enum code with fixture dvars."""
from pathlib import Path
import re
import sys

source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
names = (
    "Window_HasFocus", "Item_ShouldHandleKey", "Item_List_NextEntryForKey",
    "Item_DvarEnum_CountSettings", "Item_DvarEnum_EnumIndex",
    "Item_DvarEnum_HandleKey", "Item_HandleKey", "Item_SetFocus", "Menu_HandleKey",
)
functions = []
for name in names:
    match = re.search(r"^[^\n{};]*\b" + name + r"\([^;{}]*\)\n\{.*?^\}", source, re.M | re.S)
    if not match:
        raise SystemExit(f"Production native menu function changed: {name}")
    functions.append((source.count("\n", 0, match.start()) + 1, match.group(0)))
prototypes = "\n".join(body.split("\n{")[0] + ";" for _, body in functions)
bodies = "\n".join(f'#line {line} "{source_path.as_posix()}"\n{body}' for line, body in functions)
output_path.write_text(prototypes + "\n" + bodies + "\n", encoding="utf-8")
