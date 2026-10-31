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
         "FindFirstSibling", "FindNextSibling", "ClearObjectInternal",
         "GetObject", "GetArray", "FindObject", "GetVariable", "GetNewVariable"]
parts = []
for name in names:
    matches = list(re.finditer(rf"^[^\n]*\b{name}\([^\n]*\)\n\{{.*?^\}}", source, re.M | re.S))
    if len(matches) != 1:
        raise SystemExit(f"Expected one production function {name}, found {len(matches)}")
    match = matches[0]
    line = source.count("\n", 0, match.start()) + 1
    parts.append(f'#line {line} "{source_path.as_posix()}"\n' + match.group(0))
anim_path = source_path.with_name("scr_animtree.cpp")
anim_source = anim_path.read_text(encoding="utf-8-sig")
match = re.search(r"^[^\n]*\bScr_UsingTreeInternal\([^\n]*\)\n\{.*?^\}", anim_source, re.M | re.S)
if not match:
    raise SystemExit("Production Scr_UsingTreeInternal changed")
line = anim_source.count("\n", 0, match.start()) + 1
parts.append(f'#line {line} "{anim_path.as_posix()}"\n' + match.group(0))
hud_path = source_path.parent.parent / "game" / "g_hudelem.cpp"
hud_source = hud_path.read_text(encoding="utf-8-sig")
for name in ["HudElem_Alloc", "HudElem_SetDefaults", "HudElem_ClearTypeSettings", "HudElem_Free"]:
    match = re.search(rf"^[^\n]*\b{name}\([^\n]*\)\n\{{.*?^\}}", hud_source, re.M | re.S)
    if not match:
        raise SystemExit(f"Production HUD function {name} changed")
    line = hud_source.count("\n", 0, match.start()) + 1
    parts.append(f'#line {line} "{hud_path.as_posix()}"\n' + match.group(0))
output_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
