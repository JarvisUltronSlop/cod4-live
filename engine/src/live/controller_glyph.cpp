#include "controller_glyph.h"
#include <universal/q_shared.h>
#include <gfx_d3d/r_material.h>
#include <gfx_d3d/r_image.h>
#include <database/database.h>
#include <universal/com_files.h>
#include <array>
#include <cstring>
#include <string>

namespace live::controller_glyph {
namespace {
struct Glyph { const char* name; const char* image; Material material{}; MaterialTextureDef texture{}; GfxImage* loaded{}; std::string imagePath; GfxImage imageData{}; };
Glyph glyphs[]{
    {"button_a", "button_a"}, {"button_b", "button_b"}, {"button_x", "button_x"}, {"button_y", "button_y"},
    {"button_lshldr", "button_lshldr"}, {"button_rshldr", "button_rshldr"},
    {"button_start", "button_start"}, {"button_back", "button_back"},
    {"button_lstick", "button_lstick1"}, {"button_rstick", "button_rstick1"},
    {"button_ltrig", "button_ltrig"}, {"button_rtrig", "button_rtrig"},
    {"dpad_up", "dpad_up"}, {"dpad_down", "dpad_down"}, {"dpad_left", "dpad_left"}, {"dpad_right", "dpad_right"},
    {"button_ps3_cross", "button_ps3_cross"}, {"button_ps3_circle", "button_ps3_circle"},
    {"button_ps3_square", "button_ps3_square"}, {"button_ps3_triangle", "button_ps3_triangle"},
    {"button_ps3_l1", "button_ps3_l1"}, {"button_ps3_r1", "button_ps3_r1"},
    {"button_ps3_start", "button_ps3_start"}, {"button_ps3_back", "button_ps3_back"},
    {"button_ps3_l3", "button_ps3_l3"}, {"button_ps3_r3", "button_ps3_r3"},
    {"button_ps3_l2", "button_ps3_l2"}, {"button_ps3_r2", "button_ps3_r2"},
    {"dpad_ps3_up", "dpad_ps3_up"}, {"dpad_ps3_down", "dpad_ps3_down"},
    {"dpad_ps3_left", "dpad_ps3_left"}, {"dpad_ps3_right", "dpad_ps3_right"}
};
std::array<std::string, 32> encoded;
}
void Initialize() {
    // The legacy image scratch buffer and database file slot are shared. Wait
    // for fastfile IO, then read UI additions through the main-thread pool.
    DB_SyncXAssets();
    Material* white = Material_RegisterHandle("white", IMAGE_TRACK_UI);
    for (auto& glyph : glyphs) {
        // Fastfile mode skips the LoadObj image hash on shutdown. Keep these
        // small UI textures outside Hunk memory and release them explicitly.
        if (!glyph.loaded) {
            glyph.imagePath = std::string("live_controller/") + glyph.image;
            glyph.imageData = {};
            auto* image = &glyph.imageData;
            image->name = glyph.imagePath.c_str();
            image->category = IMG_CATEGORY_LOAD_FROM_FILE;
            image->semantic = TS_COLOR_MAP;
            image->track = IMAGE_TRACK_MISC; // code-owned, outside DB image accounting
            if (!Image_LoadFromFileWithReader(image, [](const char* name, int* file) -> int { return static_cast<int>(FS_FOpenFileRead(name,file)); })) {
                Com_PrintWarning(CON_CHANNEL_UI, "cod4-live: missing controller glyph images/%s.iwi\n", glyph.imagePath.c_str());
                Image_LoadWhite(image);
            }
            glyph.loaded = image;
        }
        glyph.material = *white;
        glyph.material.info.name = glyph.name;
        glyph.texture = white->textureTable[0];
        glyph.texture.u.image = glyph.loaded;
        glyph.material.textureTable = &glyph.texture;
        glyph.material.textureCount = 1;
    }
}
void Shutdown() {
    for (auto& glyph : glyphs) {
        if (glyph.loaded) Image_Release(glyph.loaded);
        glyph.loaded = nullptr;
        glyph.imageData = {};
    }
}
Material* Resolve(const char* p) {
    if (!Named(p)) return nullptr;
    const auto length = static_cast<unsigned char>(p[3]);
    for (auto& glyph : glyphs)
        if (std::strlen(glyph.name) == length && !std::memcmp(glyph.name, p + 4, length))
            return glyph.loaded ? &glyph.material : nullptr;
    return nullptr;
}
const char* Encode(const char* source) {
    if (!source || source[0] != '^' || source[1] != 1) return source;
    for (std::size_t i = 0; i < std::size(glyphs); ++i) {
        if (!std::strcmp(glyphs[i].name, source + 5)) {
            if (encoded[i].empty()) { encoded[i] = source; encoded[i][1] = 3; }
            return encoded[i].c_str();
        }
    }
    return source;
}
}
