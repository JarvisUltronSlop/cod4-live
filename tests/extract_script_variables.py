"""Compile actual VM allocation/hash functions, without renderer or script compiler."""
from pathlib import Path
import re
import sys
source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
names = ["Scr_InitVariables", "Scr_InitVariableRange", "AllocValue", "FreeValue",
         "AllocVariable", "FreeVariable", "AllocObject", "Scr_AllocArray",
         "AddRefToObject", "RemoveRefToEmptyObject", "IsValidArrayIndex",
         "GetInternalVariableIndex", "Scr_GetArrayIndexValue", "FindVariable",
         "FindVariableIndexInternal", "FindVariableIndexInternal2",
         "GetNewVariableIndexInternal3", "GetNewVariableIndexInternal2",
         "GetNewVariableIndexInternal", "GetNewVariableIndexReverseInternal2",
         "GetNewVariableIndexReverseInternal", "GetVariableIndexInternal",
         "MakeVariableExternal", "FreeChildValue", "RemoveVariable",
         "FindFirstSibling", "FindNextSibling", "ClearObjectInternal"]
parts = []
for name in names:
    matches = list(re.finditer(rf"^[^\n]*\b{name}\([^\n]*\)\n\{{.*?^\}}", source, re.M | re.S))
    if len(matches) != 1:
        raise SystemExit(f"Expected one production function {name}, found {len(matches)}")
    match = matches[0]
    line = source.count("\n", 0, match.start()) + 1
    parts.append(f'#line {line} "{source_path.as_posix()}"\n' + match.group(0))
output_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
