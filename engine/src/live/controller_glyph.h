#pragma once
#include <cstddef>
struct Material;
namespace live::controller_glyph {
// CoD4's markers 1/2 contain binary material handles. Marker3 safely carries
// IW4x's length-prefixed material name through ordinary C strings instead.
inline bool Named(const char* p) {
    if (!p || p[0] != 3 || static_cast<unsigned char>(p[1]) <= 16 || static_cast<unsigned char>(p[2]) <= 16 || p[3] <= 0 || static_cast<unsigned char>(p[3]) >= 64) return false;
    for (unsigned i=0; i<static_cast<unsigned char>(p[3]); ++i) if (!p[4+i]) return false;
    return true;
}
inline std::size_t Bytes(const char* p) { return Named(p) ? 4u + static_cast<unsigned char>(p[3]) : 7u; }
void Initialize();
void Shutdown();
Material* Resolve(const char* encoded);
const char* Encode(const char* iw4xGlyph);
}
