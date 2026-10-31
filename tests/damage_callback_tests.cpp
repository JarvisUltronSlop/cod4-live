#include <universal/q_shared.h>
#include <game_mp/g_public_mp.h>
#include <bgame/bg_local.h>
#include <script/scr_const.h>
#include <cstdio>
#include <cstdlib>

scr_const_t scr_const;
gentity_s g_entities[MAX_GENTITIES];
static gentity_s* receivedAttacker;
static gentity_s* receivedInflictor;
static int receivedDamage, calls;

void MyAssertHandler(const char* file, int line, int, const char* format, ...)
{
    std::fprintf(stderr, "Engine assertion at %s:%d: ", file, line);
    va_list args; va_start(args, format); std::vfprintf(stderr, format, args); va_end(args);
    std::abort();
}
uint32_t BG_GetNumWeapons() { return 16; }
uint32_t G_GetWeaponIndexForEntity(const gentity_s*) { return 1; }
double G_GetWeaponHitLocationMultiplier(hitLocation_t, uint32_t) { return 1.0; }
void Scr_PlayerDamage(gentity_s*, gentity_s* inflictor, gentity_s* attacker,
    int damage, int, uint32_t, uint32_t, const float*, const float*, hitLocation_t, int)
{
    ++calls; receivedInflictor = inflictor; receivedAttacker = attacker; receivedDamage = damage;
}
#include "generated_damage_callback.inc"

void Require(bool condition, const char* message)
{
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main()
{
    static gclient_s client;
    auto& target = g_entities[0];
    auto& projectile = g_entities[64];
    auto& liveAttacker = g_entities[1];
    target.client = &client;
    target.takedamage = true;
    client.sess.connected = CON_CONNECTED;
    auto damage = [&](gentity_s* attacker) {
        G_DamageClient(&target, &projectile, attacker, nullptr, nullptr,
            25, 0, MOD_GRENADE_SPLASH, 1, HITLOC_NONE, 0);
    };
    damage(nullptr);
    Require(calls == 1 && receivedAttacker == &g_entities[ENTITYNUM_WORLD],
        "disconnected projectile owner attributed to world");
    Require(receivedInflictor == &projectile && receivedDamage == 25,
        "projectile identity and damage preserved");
    damage(&liveAttacker);
    Require(calls == 2 && receivedAttacker == &liveAttacker, "live attacker preserved");
    client.noclip = true;
    damage(nullptr);
    Require(calls == 2, "existing player damage eligibility preserved");
    std::puts("Production player damage callback boundaries passed");
}
