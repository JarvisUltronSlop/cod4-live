"""Compile the production central binding/localization pipeline without a renderer."""
from pathlib import Path
import re
import sys

root, output_path = map(Path, sys.argv[1:])

def source(path):
    full = root / path
    return full, full.read_text(encoding="utf-8-sig")

def function(path, name):
    full, text = source(path)
    match = re.search(r"^[^\n{};]*\b" + name + r"\([^;{}]*\)\s*\n?\{.*?^\}", text, re.M | re.S)
    if not match:
        raise SystemExit(f"Production prompt function changed: {name}")
    line = text.count("\n", 0, match.start()) + 1
    body = match.group(0)
    return body[:body.index("{")].strip() + ";", f'#line {line} "{full.as_posix()}"\n{body}\n'

controller = [function("live/controller.cpp", name) for name in ("PromptsActive", "KeyName", "CommandKeys")]
central = [function(path, name) for path, name in (
    ("client/cl_keys.cpp", "Key_KeynumToString"),
    ("client/cl_keys.cpp", "Key_GetCommandAssignmentInternal"),
    ("client/cl_keys.cpp", "CL_GetKeyBindingInternal"),
    ("client/cl_keys.cpp", "CL_GetKeyBinding"),
    ("client_mp/cl_ui_mp.cpp", "Key_KeynumToStringBuf"),
    ("ui/ui_expressions.cpp", "GetKeyBindingLocalizedString"),
)]
_, glyph_source = source("live/controller_glyph.cpp")
glyphs = re.search(r"^Glyph glyphs\[\]\{.*?^\};", glyph_source, re.M | re.S)
_, controller_source = source("live/controller.cpp")
keys = re.search(r"^constexpr std::array<engine_key, 16> buttonKeys\{.*?\};", controller_source, re.M | re.S)
if not glyphs or not keys:
    raise SystemExit("Production prompt key/glyph table changed")
output = ["namespace live::controller_glyph {", "struct Glyph { const char* name; const char* image; };",
          glyphs.group(0), "std::array<std::string,32> encoded;", function("live/controller_glyph.cpp", "Encode")[1], "}",
          "namespace live::controller {", keys.group(0), *(body for _, body in controller), "}",
          *(prototype for prototype, _ in central), *(body for _, body in central)]
output_path.write_text("\n".join(output), encoding="utf-8")
