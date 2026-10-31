#include <universal/q_shared.h>
#include <script/scr_main.h>
#include <script/scr_animtree.h>
#include <game/game_public.h>
void Scr_TerminalError(const char* message);
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

scrVarPub_t scrVarPub;
scrVarDebugPub_t* scrVarDebugPub;
scrVarDebugPub_t scrVarDebugPubBuf;
scrVarGlob_t scrVarGlob;
scrAnimPub_t scrAnimPub;
scrAnimGlob_t scrAnimGlob;
game_hudelem_s g_hudelems[MAX_HUDELEMS_TOTAL];
unsigned hudReleases = 0;
void Scr_FreeHudElem(game_hudelem_s*) { ++hudReleases; }
enum { ANIMTREE_NAMES, ANIMTREE_XANIM };
bool Scr_IsIdentifier(const char*) { return true; }
uint32_t Scr_CreateCanonicalFilename(const char*) { return 1000; }
void SL_RemoveRefToString(uint32_t) {}

void MyAssertHandler(const char* file, int line, int, const char* format, ...)
{
    std::fprintf(stderr, "Engine assertion at %s:%d: ", file, line);
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::abort();
}
void Scr_TerminalError(const char* message) { throw std::runtime_error(message); }
bool IsObject(VariableValueInternal* value) { return (value->w.type & VAR_MASK) >= VAR_THREAD; }
// These fixtures use only integer array keys/values or empty object references.
// All allocation, hashing, sibling linking and release code below is production.
void AddRefToValue(int type, VariableUnion value)
{
    if (type == VAR_POINTER) AddRefToObject(value.pointerValue);
    else if (type != VAR_INTEGER && type != VAR_UNDEFINED) std::abort();
}
void RemoveRefToValue(int type, VariableUnion value)
{
    if (type == VAR_POINTER) RemoveRefToEmptyObject(value.pointerValue);
    else if (type != VAR_INTEGER && type != VAR_UNDEFINED) std::abort();
}
#define FACTOR101 101
#include "generated_script_variables.inc"

void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

int main()
{
    static_assert(sizeof(VariableLink) == 4);
    static_assert(sizeof(VariableValueInternal) == 24);
    static_assert(VARIABLELIST_PARENT_SIZE < 65536);
    static_assert(OBJECT_NOTIFY_LIST >= 0x10000 + VARIABLELIST_PARENT_SIZE);
    Scr_InitVariables();
    scrVarPub.varUsagePos = "script variable boundary fixture";

    // Exercise every parent slot, including IDs beyond the former signed-short
    // boundary, then recycle them. Parent IDs still round-trip through uint16.
    std::vector<uint32_t> parents;
    for (unsigned i = 1; i < VARIABLELIST_PARENT_SIZE; ++i) parents.push_back(AllocObject());
    Require(parents.back() == VARIABLELIST_PARENT_SIZE - 1, "full parent pool");
    Require(Scr_GetArrayIndexValue(0x10000 + 32768).u.pointerValue == 32768, "object key 32768");
    auto lastKey = Scr_GetArrayIndexValue(0x10000 + parents.back());
    Require(lastKey.type == VAR_POINTER && lastKey.u.pointerValue == parents.back(), "highest object key");
    for (auto id : parents) RemoveRefToEmptyObject(id);
    Require(scrVarPub.numScriptObjects == 0, "parent pool released");

    // Every child free-list index must survive past 65535 and near the pool end.
    std::vector<uint32_t> children;
    for (unsigned i = 1; i < VARIABLELIST_CHILD_SIZE; ++i) children.push_back(AllocValue());
    Require(children.back() == VARIABLELIST_CHILD_SIZE - 1, "full child pool");
    for (auto id : children) FreeValue(id);
    Require(scrVarPub.numScriptValues == 0, "child pool released");

    // Three arrays produce 120000 live hashed children, forcing hash collisions,
    // relocations and both sibling directions across the former 16-bit boundary.
    std::vector<uint32_t> arrays;
    for (unsigned a = 0; a < 3; ++a)
    {
        auto parent = Scr_AllocArray(); arrays.push_back(parent);
        for (unsigned i = 0; i < 40000; ++i)
        {
            auto name = GetInternalVariableIndex(i);
            if (i & 1) GetNewVariableIndexReverseInternal(parent, name);
            else GetNewVariableIndexInternal(parent, name);
        }
    }
    Require(scrVarPub.numScriptValues == 120000, "large live child population");
    for (auto parent : arrays)
    {
        for (unsigned i = 0; i < 40000; ++i)
            Require(FindVariable(parent, GetInternalVariableIndex(i)) != 0, "hashed child lookup");
        unsigned siblings = 0;
        for (auto id = FindFirstSibling(parent); id; id = FindNextSibling(id)) ++siblings;
        Require(siblings == 40000, "sibling chain intact");
        for (unsigned i = 0; i < 40000; i += 2) RemoveVariable(parent, GetInternalVariableIndex(i));
        ClearObjectInternal(parent);
        RemoveRefToEmptyObject(parent);
    }
    Require(scrVarPub.numScriptValues == 0 && scrVarPub.numScriptObjects == 0, "hashed pool recycled");
    Require(scrVarPub.totalObjectRefCount == 0, "reference accounting balanced");

    Require(IsValidArrayIndex(static_cast<uint32_t>(-0x7E0000)), "minimum integer key accepted");
    Require(!IsValidArrayIndex(static_cast<uint32_t>(-0x7E0001)), "object-key overlap rejected");
    Require(IsValidArrayIndex(0x7FFFFF) && !IsValidArrayIndex(0x800000), "maximum integer key bound");
    auto integerKey = Scr_GetArrayIndexValue(GetInternalVariableIndex(static_cast<uint32_t>(-0x7E0000)));
    Require(integerKey.type == VAR_INTEGER && integerKey.u.intValue == -0x7E0000, "minimum integer roundtrip");

    auto bounded = Scr_AllocArray();
    auto& entry = scrVarGlob.variableList[VARIABLELIST_PARENT_BEGIN + bounded];
    entry.u.o.u.size = UINT16_MAX;
    auto freeHead = scrVarGlob.variableList[VARIABLELIST_CHILD_BEGIN].u.freeNext;
    bool rejected = false;
    try { GetNewVariableIndexInternal(bounded, GetInternalVariableIndex(1)); }
    catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && freeHead == scrVarGlob.variableList[VARIABLELIST_CHILD_BEGIN].u.freeNext,
        "array overflow rejected before pool mutation");
    entry.u.o.u.size = 0;
    entry.u.o.refCount = UINT16_MAX;
    const auto references = scrVarPub.totalObjectRefCount;
    rejected = false;
    try { AddRefToObject(bounded); } catch (const std::runtime_error&) { rejected = true; }
    Require(rejected && entry.u.o.refCount == UINT16_MAX && references == scrVarPub.totalObjectRefCount,
        "reference overflow rejected before mutation");
    entry.u.o.refCount = 0;
    RemoveRefToEmptyObject(bounded);
    // An animation-tree lookup caches CHILD IDs, even though animation IDs
    // themselves stay 16-bit. Reusing a tree must retain a child hash ID >65535.
    scrAnimPub.animtrees = AllocObject();
    scrAnimPub.animtree_loading = true;
    uint32_t firstIndex = 0, secondIndex = 0;
    const auto firstNames = Scr_UsingTreeInternal("fixture", &firstIndex, 1);
    Require(scrAnimGlob.using_xanim_lookup[1][firstIndex] > 65535, "high animation child ID fixture");
    const auto secondNames = Scr_UsingTreeInternal("fixture", &secondIndex, 1);
    Require(firstNames == secondNames && firstIndex == secondIndex && firstIndex == 1,
        "reused animation tree preserves full child ID");
    static_assert(sizeof(((playerState_s*)nullptr)->hud.current) / sizeof(hudelem_s) == 31);
    static_assert(sizeof(((playerState_s*)nullptr)->hud.archival) / sizeof(hudelem_s) == 31);
    for (unsigned i = 0; i < MAX_HUDELEMS_TOTAL; ++i)
    {
        auto hud = HudElem_Alloc(i % 64, 0);
        Require(hud == &g_hudelems[i] && hud->clientNum == i % 64, "full HUD pool allocation");
    }
    Require(HudElem_Alloc(0, 0) == nullptr, "HUD pool exhaustion bounded");
    for (unsigned i = 0; i < MAX_HUDELEMS_TOTAL; i += 2) HudElem_Free(&g_hudelems[i]);
    for (unsigned i = 0; i < MAX_HUDELEMS_TOTAL; i += 2)
        Require(HudElem_Alloc(1, 0) == &g_hudelems[i], "HUD slot reuse across old 1024 bound");
    Require(hudReleases == MAX_HUDELEMS_TOTAL / 2, "HUD release callbacks preserved");
    std::puts("Script variable/HUD production allocation/hash boundaries passed");
}
