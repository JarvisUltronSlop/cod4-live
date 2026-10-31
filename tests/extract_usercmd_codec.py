"""Build-only extraction of the actual engine codec, without linking the renderer.

The test compiles production functions and production struct declarations. There is
no duplicate serializer implementation that could pass while the engine regresses.
"""
from pathlib import Path
import re
import sys

source_path, output_path = map(Path, sys.argv[1:])
source = source_path.read_text(encoding="utf-8-sig")
names = [
    "MSG_BeginReading", "MSG_WriteBits", "MSG_WriteBit0", "MSG_WriteBit1",
    "MSG_ReadBits", "MSG_GetByte", "MSG_ReadBit", "MSG_WriteByte",
    "MSG_WriteShort", "MSG_WriteLong", "MSG_ReadByte", "MSG_ReadShort", "MSG_ReadLong",
    "MSG_WriteDeltaKey", "MSG_ReadDeltaKey", "MSG_WriteKey", "MSG_ReadKey",
    "MSG_WriteDeltaKeyByte", "MSG_ReadDeltaKeyByte", "MSG_WriteDeltaKeyShort", "MSG_ReadDeltaKeyShort",
    "MSG_HorMoveTo", "MSG_HorMoveFrom", "MSG_WriteDeltaUsercmdKey", "MSG_ReadDeltaUsercmdKey",
]
parts = ['#include <qcommon/msg_mp.h>\n']
masks_path = source_path.with_name("sv_msg_write_mp.h")
masks = masks_path.read_text(encoding="utf-8-sig")
mask_match = re.search(r"static const uint32_t kbitmask\[33\] =\s*\{.*?\};", masks, re.S)
if not mask_match:
    raise SystemExit("Production kbitmask declaration changed; update test extraction")
parts.append(mask_match.group(0))
for name in names:
    matches = list(re.finditer(rf"^[^\n]*\b{name}\([^\n]*\)\n\{{.*?^\}}", source, re.M | re.S))
    if len(matches) != 1:
        raise SystemExit(f"Expected one production function {name}, found {len(matches)}")
    match = matches[0]
    line = source.count("\n", 0, match.start()) + 1
    parts.append(f'#line {line} "{source_path.as_posix()}"\n' + match.group(0))
output_path.parent.mkdir(parents=True, exist_ok=True)
output_path.write_text("\n\n".join(parts) + "\n", encoding="utf-8")
