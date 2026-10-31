#include <qcommon/net_chan_mp.h>
#include <cstdlib>
#include <iostream>

// Only engine services are substituted. The policy, actual usercmd layout and
// server entry-point bodies are compiled from production sources below.
enum { CS_FREE, CS_ZOMBIE, CS_CLIENTLOADING, CS_ACTIVE, SV_CMD_RELIABLE };
struct gentity_s { struct { int number; } s; };
struct client_t
{
    struct
    {
        struct { netadr_t remoteAddress; } netchan;
        int state = CS_CLIENTLOADING;
        int deltaMessage = 0;
    } header;
    int lastConnectTime = 1, challenge = 1, nextSnapshotTime = 0;
    usercmd_s lastUsercmd{};
    char name[16] = "test";
    char cdkeyHash[33]{};
    gentity_s* gentity = nullptr;
};
struct { client_t clients[64]; int time = 0; } svs;
gentity_s entities[64]{};
bool verified = true;
int warnings = 0, drops = 0, starts = 0, thinks = 0;
void Require(bool condition, const char* message)
{
    if (!condition) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); }
}
void MyAssertHandler(const char*, int, int, const char*, ...) { std::abort(); }
void Com_PrintError(int, const char*, ...) {}
void Com_DPrintf(int, const char*, ...) {}
char* va(const char*, ...) { static char buffer[1]{}; return buffer; }
bool Steam_IsClientVerified(const char*, const netadr_t&, int) { return verified; }
void SV_DropClient(client_t* client, const char*, bool) { ++drops; client->header.state = CS_ZOMBIE; }
void SV_SendServerCommand(client_t*, int, const char*, ...) { ++warnings; }
gentity_s* SV_GentityNum(int index) { return &entities[index]; }
void ClientBegin(int) { ++starts; }
void G_SetLastServerTime(int, int) {}
void ClientThink(int) { ++thinks; }

#include "generated_server_input.inc"

int main()
{
    auto& client = svs.clients[0];
    client.header.netchan.remoteAddress.type = NA_IP;
    usercmd_s command{};
    command.buttons = BUTTON_ATTACK | BUTTON_FRAG;
    command.forwardmove = 127;
    command.angles[0] = 200;
    command.weapon = 5;
    command.serverTime = 100;
    command.inputFlags = live::ControllerConnected | live::KeyboardMouseAttempt;
    SV_ClientEnterWorld(&client, &command);
    Require(starts == 1 && client.header.state == CS_ACTIVE, "initial entry proceeds after verification");
    Require(client.lastUsercmd.buttons == 0 && client.lastUsercmd.forwardmove == 0,
        "first world command must be neutralized before ClientBegin");
    Require(client.lastUsercmd.angles[0] == 0 && client.lastUsercmd.weapon == 0,
        "blocked initial command cannot change view or weapon");
    Require(warnings == 1, "initial mixed command must warn");

    // Server-controlled time, not a large client timestamp, drives punishment.
    command.serverTime = 15000;
    SV_ClientThink(&client, &command);
    Require(drops == 0, "future client timestamp cannot accelerate grace window");
    for (int time = 1000; time <= 5000; time += 1000)
    {
        svs.time = time;
        command.serverTime = time + 15000;
        command.buttons = BUTTON_ATTACK;
        command.forwardmove = 127;
        SV_ClientThink(&client, &command);
    }
    Require(drops == 1 && client.header.state == CS_ZOMBIE, "continued reports kick on server time");
    Require(thinks == 5, "kicking command must not enter game simulation");

    client = {};
    client.header.netchan.remoteAddress.type = NA_IP;
    client.challenge = 2;
    client.lastConnectTime = 5001;
    svs.time = 5001;
    command = {};
    command.serverTime = 5001;
    command.inputFlags = live::ControllerConnected | live::ControllerActive;
    command.forwardmove = 45;
    SV_ClientEnterWorld(&client, &command);
    Require(client.lastUsercmd.forwardmove == 45 && starts == 2, "new session resets old enforcement state");

    // Map restart may pass lastUsercmd itself; this alias must still neutralize safely.
    client.lastUsercmd.inputFlags = 0;
    client.lastUsercmd.buttons = BUTTON_ATTACK;
    SV_ClientEnterWorld(&client, &client.lastUsercmd);
    Require(client.lastUsercmd.buttons == 0 && client.lastUsercmd.forwardmove == 0,
        "map-restart aliased command is neutralized");
    Require(warnings == 2, "disconnect during restart warns");
    Require(client.header.state == CS_CLIENTLOADING && starts == 2,
        "disconnected map restart must wait instead of spawning");

    // Repeating a map restart does not refresh the original reconnect deadline.
    svs.time = 35000;
    client.lastUsercmd.serverTime = 35000;
    SV_ClientEnterWorld(&client, &client.lastUsercmd);
    Require(drops == 1 && starts == 2, "restart retains remaining grace without spawning");
    svs.time = 35001;
    client.lastUsercmd.serverTime = 35001;
    SV_ClientEnterWorld(&client, &client.lastUsercmd);
    Require(drops == 2 && client.header.state == CS_ZOMBIE, "restart must not reset thirty-second timer");

    client = {};
    client.header.netchan.remoteAddress.type = NA_IP;
    client.challenge = 3;
    client.lastConnectTime = 40000;
    svs.time = 40000;
    command = {};
    command.serverTime = 40000;
    command.buttons = BUTTON_ATTACK;
    SV_ClientEnterWorld(&client, &command);
    Require(client.header.state == CS_CLIENTLOADING && starts == 2,
        "initial absent controller stays primed");
    Require(client.lastUsercmd.buttons == 0, "absent initial command is stored neutral");
    command.serverTime = 40100;
    command.inputFlags = live::ControllerConnected;
    SV_ClientEnterWorld(&client, &command);
    Require(client.header.state == CS_ACTIVE && starts == 3, "controller reconnection admits primed player");

    auto& bot = svs.clients[1];
    bot.header.netchan.remoteAddress.type = NA_BOT;
    command = {};
    command.buttons = BUTTON_ATTACK;
    SV_ClientEnterWorld(&bot, &command);
    Require(bot.header.state == CS_ACTIVE && starts == 4, "internal bot may enter without input metadata");
    Require(bot.lastUsercmd.buttons == BUTTON_ATTACK, "bot action must not be neutralized");

    verified = false;
    const int previousStarts = starts;
    SV_ClientEnterWorld(&client, &command);
    Require(starts == previousStarts && drops == 3, "failed Steam verification cannot enter world");
    std::cout << "PASS: production server initial-entry, active-command and restart enforcement\n";
}
