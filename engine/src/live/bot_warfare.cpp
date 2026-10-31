// Native adapter for INeedBots' IW3 Bot Warfare. See docs/BOT_WARFARE.md.
#include "bot_warfare.h"
#include "bot_policy.h"
#include <universal/q_shared.h>
#include <qcommon/qcommon.h>
#include <qcommon/msg_mp.h>
#include <qcommon/cmd.h>
#include <universal/com_files.h>
#include <server_mp/server_mp.h>
#include <server/sv_game.h>
#include <game_mp/g_public_mp.h>
#include <script/scr_vm.h>
#include <script/scr_main.h>
#include <array>
#include <cmath>
#include <cstring>
#include <string>

namespace live::bots
{
namespace
{
const dvar_t* count;
const dvar_t* difficulty;
int initScript;
struct Input
{
    int buttons = 0, forward = 0, right = 0;
    int weapon = -1, offhand = -1;
    float meleeYaw = 0;
    unsigned char meleeDistance = 0;
};
std::array<Input, 64> inputs;
struct ScriptFile { bool open = false; std::string data; std::size_t cursor = 0; };
std::array<ScriptFile, 16> files;
cmd_function_s statusCommand, statusServerCommand;

void Status()
{
    if (!com_sv_running->current.enabled) { Com_Printf(CON_CHANNEL_SERVER, "No local server is running.\n"); return; }
    for (int i = 0; sv_maxclients && i < sv_maxclients->current.integer; ++i)
    {
        const auto& client = svs.clients[i];
        if (client.header.state < CS_CONNECTED || client.header.netchan.remoteAddress.type != NA_BOT) continue;
        const auto* ps = SV_GameClientNum(i);
        const auto& input = inputs[i];
        Com_Printf(CON_CHANNEL_SERVER, "bot %d state=%d health=%d origin=(%.1f %.1f %.1f) move=(%d %d) buttons=%x weapon=%d\n",
            i, client.header.state, g_entities[i].health, ps->origin[0], ps->origin[1], ps->origin[2],
            input.forward, input.right, input.buttons, ps->weapon);
    }
}
void PrintConsole() { Com_Printf(CON_CHANNEL_SERVER, "[Bot Warfare] %s\n", Scr_GetString(0)); }

int BotIndex(scr_entref_t ref)
{
    if (ref.classnum || ref.entnum >= 64 || !g_entities[ref.entnum].client ||
        svs.clients[ref.entnum].header.netchan.remoteAddress.type != NA_BOT)
    {
        Scr_ObjectError("Bot Warfare input requires a server-created bot");
        return -1;
    }
    return ref.entnum;
}
void Action(scr_entref_t ref)
{
    const int index = BotIndex(ref);
    if (index < 0) return;
    const char* text = Scr_GetString(0);
    if (text[0] != '+' && text[0] != '-') { Scr_ParamError(0, "Expected +action or -action"); return; }
    struct Binding { const char* name; int mask; };
    static constexpr Binding bindings[] = {
        {"fire", BUTTON_ATTACK}, {"attack", BUTTON_ATTACK}, {"sprint", BUTTON_SPRINT},
        {"melee", BUTTON_MELEE}, {"activate", BUTTON_USE}, {"reload", BUTTON_RELOAD},
        {"gostand", BUTTON_JUMP}, {"gocrouch", BUTTON_CROUCH}, {"goprone", BUTTON_PRONE},
        {"ads", BUTTON_ADS}, {"holdbreath", BUTTON_BREATH}, {"frag", BUTTON_FRAG},
        {"smoke", BUTTON_SMOKE}, {"throw", BUTTON_THROW}, {"usereload", BUTTON_USE_RELOAD}
    };
    for (const auto& binding : bindings)
        if (!strcmp(text + 1, binding.name))
        {
            if (text[0] == '+') inputs[index].buttons |= binding.mask;
            else inputs[index].buttons &= ~binding.mask;
            return;
        }
    Scr_ParamError(0, "Unknown Bot Warfare action");
}
void Stop(scr_entref_t ref)
{
    const int index = BotIndex(ref);
    if (index < 0) return;
    // Retain selected weapons, as a physical client does when releasing controls.
    inputs[index].buttons = inputs[index].forward = inputs[index].right = 0;
    inputs[index].meleeDistance = 0;
}
void Movement(scr_entref_t ref)
{
    const int index = BotIndex(ref);
    if (index < 0) return;
    inputs[index].forward = std::clamp(Scr_GetInt(0), -127, 127);
    inputs[index].right = std::clamp(Scr_GetInt(1), -127, 127);
}
void Melee(scr_entref_t ref)
{
    const int index = BotIndex(ref);
    if (index < 0) return;
    const float yaw = Scr_GetFloat(0);
    inputs[index].meleeYaw = std::isfinite(yaw) ? yaw : 0;
    inputs[index].meleeDistance = static_cast<unsigned char>(std::clamp(Scr_GetInt(1), 0, 255));
}
void IsBot(scr_entref_t ref)
{
    Scr_AddBool(!ref.classnum && ref.entnum < 64 && g_entities[ref.entnum].client &&
        svs.clients[ref.entnum].header.netchan.remoteAddress.type == NA_BOT);
}
const char* DataPath()
{
    const char* path = Scr_GetString(0);
    if (!SafeDataPath(path)) { Scr_ParamError(0, "Bot data must stay under scriptdata/"); return nullptr; }
    return path;
}
void FileExists()
{
    const auto* path = DataPath();
    if (!path) return;
    int handle = 0;
    const int length = static_cast<int>(FS_FOpenFileRead(path, &handle));
    if (handle) FS_FCloseFile(handle);
    Scr_AddBool(length >= 0);
}
void FileOpen()
{
    const auto* path = DataPath();
    if (!path) return;
    if (strcmp(Scr_GetString(1), "read")) { Scr_ParamError(1, "Packaged Bot Warfare data is read-only"); return; }
    for (std::size_t i = 0; i < files.size(); ++i)
        if (!files[i].open)
        {
            void* buffer = nullptr;
            const int length = FS_ReadFile(path, &buffer);
            if (length < 0) { Scr_AddInt(-1); return; }
            if (length > 2 * 1024 * 1024) { FS_FreeFile(static_cast<char*>(buffer)); Scr_Error("Bot data file exceeds 2 MiB"); return; }
            files[i].data.assign(static_cast<const char*>(buffer), length);
            FS_FreeFile(static_cast<char*>(buffer));
            files[i].cursor = 0;
            files[i].open = true;
            Scr_AddInt(static_cast<int>(i + 1));
            return;
        }
    Scr_Error("Too many open Bot Warfare data files");
}
ScriptFile* FileHandle()
{
    const int handle = Scr_GetInt(0);
    if (handle < 1 || handle > static_cast<int>(files.size()) || !files[handle - 1].open)
    { Scr_ParamError(0, "Invalid Bot Warfare file handle"); return nullptr; }
    return &files[handle - 1];
}
void FileClose() { if (auto* file = FileHandle()) *file = {}; }
void ReadLine()
{
    auto* file = FileHandle();
    if (!file) return;
    if (file->cursor >= file->data.size()) { Scr_AddUndefined(); return; }
    const auto start = file->cursor;
    auto end = file->data.find('\n', start);
    if (end == std::string::npos) end = file->data.size();
    file->cursor = end < file->data.size() ? end + 1 : end;
    if (end > start && file->data[end - 1] == '\r') --end;
    if (end - start > 4095) { Scr_Error("Bot data line exceeds 4095 bytes"); return; }
    const auto line = file->data.substr(start, end - start);
    Scr_AddString(line.c_str());
}
int Humans()
{
    int humans = 0;
    for (int i = 0; sv_maxclients && i < sv_maxclients->current.integer; ++i)
        if (svs.clients[i].header.state >= CS_CONNECTED &&
            svs.clients[i].header.netchan.remoteAddress.type != NA_BOT) ++humans;
    return humans;
}
}

void Initialize()
{
    if (count) return;
    count = Dvar_RegisterInt("live_botCount", 0, 0, 63, DVAR_ARCHIVE, "Desired Bot Warfare bots; one player slot is reserved");
    difficulty = Dvar_RegisterInt("live_botDifficulty", 3, 0, 7, DVAR_ARCHIVE, "Bot Warfare skill: 0 random, 1 easiest, 7 hardest");
    Cmd_AddCommandInternal("live_botStatus", Cbuf_AddServerText_f, &statusCommand);
    Cmd_AddServerCommandInternal("live_botStatus", Status, &statusServerCommand);
}
void Frame()
{
    Initialize();
    const int limit = CountLimit(sv_maxclients ? sv_maxclients->current.integer : 0, Humans());
    Dvar_SetIntByName("bots_manage_fill", std::clamp(count->current.integer, 0, limit));
    Dvar_SetIntByName("bots_manage_fill_mode", 1);
    Dvar_SetIntByName("bots_manage_fill_kick", 1);
    Dvar_SetIntByName("bots_skill", difficulty->current.integer);
}
bool CanAdd()
{
    if (!sv_maxclients) return false;
    int bots = 0, occupied = 0;
    for (int i = 0; i < sv_maxclients->current.integer; ++i)
        if (svs.clients[i].header.state != CS_FREE)
        {
            ++occupied;
            if (svs.clients[i].header.netchan.remoteAddress.type == NA_BOT) ++bots;
        }
    return bots < CountLimit(sv_maxclients->current.integer, Humans()) && occupied < sv_maxclients->current.integer;
}
void ResetClient(int index) { if (index >= 0 && index < 64) inputs[index] = {}; }
bool SelectWeapon(int index, int weapon, bool offhand)
{
    if (index < 0 || index >= 64 || svs.clients[index].header.netchan.remoteAddress.type != NA_BOT) return false;
    if (offhand) inputs[index].offhand = weapon;
    else inputs[index].weapon = weapon;
    return true;
}
void UserMove(client_t* client)
{
    // Full map changes reconnect clients through gamestate/stats. An internal
    // bot has no network peer to send those acknowledgements or a first cmd.
    if (client->header.state == CS_CONNECTED) SV_SendClientGameState(client);
    if (client->header.state == CS_CLIENTLOADING)
    {
        usercmd_s initial{};
        initial.serverTime = svs.time;
        SV_ClientEnterWorld(client, &initial);
    }
    if (!client->gentity || client->header.state != CS_ACTIVE) return;
    const int index = static_cast<int>(client - svs.clients);
    if (index < 0 || index >= 64) return;
    const auto& input = inputs[index];
    const auto* ps = SV_GameClientNum(index);
    usercmd_s cmd{};
    cmd.serverTime = svs.time;
    cmd.weapon = static_cast<unsigned char>(input.weapon >= 0 ? input.weapon : ps->weapon);
    cmd.offHandIndex = static_cast<unsigned char>(input.offhand >= 0 ? input.offhand : ps->offHandIndex);
    if (!BG_IsWeaponValid(ps, cmd.weapon)) cmd.weapon = static_cast<unsigned char>(ps->weapon);
    if (!BG_IsWeaponValid(ps, cmd.offHandIndex)) cmd.offHandIndex = static_cast<unsigned char>(ps->offHandIndex);
    for (int axis = 0; axis < 3; ++axis)
        cmd.angles[axis] = static_cast<unsigned short>(static_cast<int>((ps->viewangles[axis] - ps->delta_angles[axis]) * (65536.0f / 360.0f)));
    cmd.buttons = input.buttons;
    cmd.forwardmove = static_cast<char>(input.forward);
    cmd.rightmove = static_cast<char>(input.right);
    cmd.meleeChargeYaw = input.meleeYaw;
    cmd.meleeChargeDist = input.meleeDistance;
    client->header.deltaMessage = client->header.netchan.outgoingSequence - 1;
    client->lastPacketTime = svs.time;
    SV_ClientThink(client, &cmd);
}
void LoadScripts()
{
    Shutdown();
    Initialize();
    Frame();
    Dvar_SetIntByName("bots_main_firstIsHost", 0);
    Dvar_SetIntByName("bots_main_menu", 0);
    Dvar_SetIntByName("bots_main_waitForHostTime", 0);
    initScript = GScr_LoadScriptAndLabel("maps/mp/bots/_cod4live", "init", 1);
}
void StartScripts()
{
    // Fast restarts keep compiled scripts, but must not retain held controls or
    // a selected weapon from the previous match while clients reconnect.
    inputs = {};
    for (auto& file : files) file = {};
    Frame();
    if (initScript) Scr_FreeThread(Scr_ExecThread(initScript, 0));
}
void Shutdown()
{
    initScript = 0;
    inputs = {};
    for (auto& file : files) file = {};
}
Function GetFunction(const char** name)
{
    static const struct { const char* name; Function function; } entries[] = {
        {"live_botprint", PrintConsole},
        {"live_botfileexists", FileExists}, {"live_botfileopen", FileOpen},
        {"live_botfileclose", FileClose}, {"live_botreadline", ReadLine}
    };
    for (const auto& entry : entries)
        if (!strcmp(*name, entry.name)) { *name = entry.name; return entry.function; }
    return nullptr;
}
Method GetMethod(const char** name)
{
    static const struct { const char* name; Method function; } entries[] = {
        {"live_botaction", Action}, {"live_botstop", Stop}, {"live_botmovement", Movement},
        {"live_botmelee", Melee}, {"live_isbot", IsBot}
    };
    for (const auto& entry : entries)
        if (!strcmp(*name, entry.name)) { *name = entry.name; return entry.function; }
    return nullptr;
}
}
