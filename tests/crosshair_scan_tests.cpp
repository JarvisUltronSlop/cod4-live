#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// Physics, smoke visibility and the engine's surrounding state are fixtures;
// target classification, range checks and fade history are production code.
constexpr int MAX_CLIENTS = 64;
constexpr int MASK_PLAYER_VISIBILITY = 0x02803001;
enum team_t { TEAM_FREE, TEAM_AXIS, TEAM_ALLIES, TEAM_SPECTATOR };
struct PlayerState { uint32_t weapFlags = 0; int clientNum = 0, weapon = 1; };
struct Snapshot { PlayerState ps; };
struct cg_s
{
    Snapshot* nextSnap = nullptr;
    PlayerState predictedPlayerState;
    struct { float vieworg[3]{}; float viewaxis[3][3]{}; } refdef;
    struct { struct { team_t team = TEAM_FREE; } clientinfo[MAX_CLIENTS]; } bgs;
    int crosshairClientNum = MAX_CLIENTS;
    int crosshairClientStartTime = 0, crosshairClientLastTime = 0, time = 1000;
};
struct centity_s { struct { float origin[3]{}; } pose; };
struct trace_t { float fraction; uint16_t hit; };
struct WeaponDef { float enemyCrosshairRange = 500; };
struct Dvar { struct { int integer; } current; };
static cg_s game;
static Snapshot snapshot;
static centity_s entities[MAX_CLIENTS];
static WeaponDef weapon;
static Dvar friendlyFade{{500}}, enemyFade{{250}};
static const Dvar* cg_friendlyNameFadeOut = &friendlyFade;
static const Dvar* cg_enemyNameFadeOut = &enemyFade;
static bool flashed;
static uint16_t hit;
static float visibility;
static int traceCalls, visibilityCalls;
static const float vec3_origin[3]{};

static void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
#define iassert(value) Require(!!(value), #value)
#define bcassert(value, bound) Require(static_cast<unsigned>(value) < (bound), #value)
#define PROF_SCOPED(name) (void)0
cg_s* CG_GetLocalClientGlobals(int) { return &game; }
centity_s* CG_GetEntity(int, uint16_t index) { Require(index < MAX_CLIENTS, "entity index"); return &entities[index]; }
WeaponDef* BG_GetWeaponDef(int) { return &weapon; }
bool CG_Flashbanged(int) { return flashed; }
void Vec3Mad(const float* start, float distance, const float* direction, float* end)
{ for (int i = 0; i < 3; ++i) end[i] = start[i] + distance * direction[i]; }
void Vec3Sub(const float* a, const float* b, float* result)
{ for (int i = 0; i < 3; ++i) result[i] = a[i] - b[i]; }
float Vec3LengthSq(const float* value)
{ return value[0] * value[0] + value[1] * value[1] + value[2] * value[2]; }
void Vec3Lerp(const float* a, const float* b, float t, float* result)
{ for (int i = 0; i < 3; ++i) result[i] = a[i] + t * (b[i] - a[i]); }
void CG_TraceCapsule(trace_t* result, const float*, float*, float*, const float*, int, int)
{ ++traceCalls; *result = {0.1f, hit}; }
uint16_t Trace_GetEntityHitId(const trace_t* trace) { return trace->hit; }
float FX_GetClientVisibility(int, const float*, const float*) { ++visibilityCalls; return visibility; }
#include "generated_crosshair_scan.inc"

static void Reset()
{
    game = {}; snapshot = {}; weapon = {};
    std::memset(entities, 0, sizeof(entities));
    game.nextSnap = &snapshot;
    game.time = 1000;
    game.refdef.viewaxis[0][0] = 1;
    game.predictedPlayerState.weapFlags = 0x418;
    game.bgs.clientinfo[0].team = TEAM_AXIS;
    game.bgs.clientinfo[1].team = TEAM_ALLIES;
    game.bgs.clientinfo[2].team = TEAM_AXIS;
    entities[1].pose.origin[0] = 100;
    entities[2].pose.origin[0] = 100;
    hit = 1; flashed = false; visibility = 1;
    traceCalls = visibilityCalls = 0;
}
int main()
{
    Reset(); game.crosshairClientNum = 2;
    CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x410 && game.crosshairClientNum == 1,
        "enemy is classified from actual hit after previous teammate");
    Reset(); game.crosshairClientNum = 1; hit = 2; entities[2].pose.origin[0] = 700;
    CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x408 && game.crosshairClientNum == 2,
        "teammate is classified from actual hit without enemy range limit");
    Reset(); CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x410,
        "first enemy scan never indexes invalid previous target");
    Reset(); game.bgs.clientinfo[0].team = TEAM_FREE; game.bgs.clientinfo[1].team = TEAM_FREE;
    CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x410, "free-for-all opponent is hostile");
    Reset(); entities[1].pose.origin[0] = 500; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x410, "enemy exactly at weapon range accepted");
    Reset(); entities[1].pose.origin[0] = 501; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x400 && game.crosshairClientNum == MAX_CLIENTS,
        "enemy beyond weapon range cannot set color or target history");
    Reset(); flashed = true; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x400 && traceCalls == 0,
        "flashbang clears prior feedback without scanning");
    Reset(); hit = 1023; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x400 && visibilityCalls == 0,
        "world obstruction or missed trace does not identify enemies");
    Reset(); visibility = 0.199f; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x400 && game.crosshairClientNum == MAX_CLIENTS,
        "smoke visibility failure cannot expose enemy feedback");
    Reset(); visibility = 0.2f; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x410, "stock visibility threshold accepted");
    Reset(); game.bgs.clientinfo[0].team = TEAM_SPECTATOR; CG_ScanForCrosshairEntity(0);
    Require(game.predictedPlayerState.weapFlags == 0x400 && game.crosshairClientNum == 1,
        "spectator keeps name behavior without friendly or enemy color");
    Reset(); game.crosshairClientNum = 1; game.crosshairClientStartTime = 300;
    game.crosshairClientLastTime = 900; CG_ScanForCrosshairEntity(0);
    Require(game.crosshairClientStartTime == 300 && game.crosshairClientLastTime == 1000,
        "continuous target preserves stock fade-in history");
    game.time = 1300; CG_ScanForCrosshairEntity(0);
    Require(game.crosshairClientStartTime == 1300, "expired target restarts stock fade timer");
    std::puts("Production crosshair target, range and visibility checks passed");
}
