// cod4-live Steam ownership authentication. Retail assets are never ownership proof.
#include <universal/q_shared.h>
#include "win_steam.h"
#include <qcommon/qcommon.h>
#include <qcommon/cmd.h>
#include <live/steam_auth_policy.h>
#include <steamsdk/steam/steam_api.h>
#include <steamsdk/steam/steam_gameserver.h>
#include <Windows.h>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#include <array>
#include <atomic>
#include <vector>
#ifdef KISAK_MP
#include <server_mp/server_mp.h>
#endif

namespace {
constexpr AppId_t app_id = 7940;
constexpr std::size_t max_sessions = 128;
bool client_api = false;
bool server_api = false;
bool server_attempted = false;
std::atomic<bool> ticket_ready{false};
std::atomic<bool> ticket_failed{false};
HAuthTicket client_ticket = k_HAuthTicketInvalid;
unsigned char ticket_bytes[cod4live::steam_auth::max_ticket_bytes]{};
uint32 ticket_size = 0;
ULONGLONG ticket_started = 0;

#ifdef KISAK_MP
struct AuthSession {
    uint64 id = 0;
    netadr_t address{};
    int challenge = 0;
    cod4live::steam_auth::Verification verification;
    std::vector<unsigned char> ticket;
    bool attached = false;
    bool ended = false;
    bool notified = false;
    ULONGLONG retired_at = 0;
};
std::vector<AuthSession> sessions;
struct RateEntry { bool used = false; netadr_t address{}; ULONGLONG start = 0; unsigned count = 0; };
std::array<RateEntry, 128> rates{};

AuthSession* Find(uint64 id) {
    for (auto& session : sessions) if (session.id == id) return &session;
    return nullptr;
}
void End(AuthSession& session) {
    if (!session.ended && server_api) SteamGameServer()->EndAuthSession(CSteamID(session.id));
    for (auto& challenge : svs.challenges) {
        if (challenge.challenge == session.challenge && NET_CompareAdr(challenge.adr, session.address))
            memset(&challenge, 0, sizeof(challenge));
    }
    session.ended = true;
    session.verification.state = cod4live::steam_auth::State::rejected;
    session.retired_at = GetTickCount64();
    session.ticket.clear();
}
bool RateAllow(netadr_t address) {
    const auto now = GetTickCount64();
    RateEntry* free_entry = nullptr;
    for (auto& rate : rates) {
        if (!rate.used || now - rate.start >= 10000) { if (!free_entry) free_entry = &rate; }
        if (rate.used && NET_CompareBaseAdr(rate.address, address) && now - rate.start < 10000)
            return ++rate.count <= 32;
    }
    if (!free_entry) return false;
    *free_entry = {true, address, now, 1};
    return true;
}
class ServerCallbacks {
public:
    STEAM_GAMESERVER_CALLBACK(ServerCallbacks, Validate, ValidateAuthTicketResponse_t);
    STEAM_GAMESERVER_CALLBACK(ServerCallbacks, Connected, SteamServersConnected_t);
    STEAM_GAMESERVER_CALLBACK(ServerCallbacks, Failed, SteamServerConnectFailure_t);
};
ServerCallbacks* server_callbacks = nullptr;
void ServerCallbacks::Validate(ValidateAuthTicketResponse_t* result) {
    auto* session = Find(result->m_SteamID.ConvertToUint64());
    if (!session || session->ended) return;
    session->verification.complete(result->m_eAuthSessionResponse == k_EAuthSessionResponseOK,
        session->id, result->m_OwnerSteamID.ConvertToUint64(), GetTickCount64());
    Com_Printf(CON_CHANNEL_SERVER, "[cod4-live] Steam verification %llu: result=%d directOwner=%d\n",
        session->id, int(result->m_eAuthSessionResponse), result->m_OwnerSteamID == result->m_SteamID);
}
void ServerCallbacks::Connected(SteamServersConnected_t*) {
    Com_Printf(CON_CHANNEL_SERVER, "[cod4-live] Steam authentication server connected for app 7940.\n");
}
void ServerCallbacks::Failed(SteamServerConnectFailure_t* result) {
    Com_PrintError(CON_CHANNEL_SERVER, "[cod4-live] Steam server login failed (%d); ownership admission remains closed.\n", int(result->m_eResult));
}
bool InitServer() {
    if (server_api) return true;
    if (server_attempted) return false;
    server_attempted = true;
    SetEnvironmentVariableA("SteamAppId", "7940");
    const int game_port = Dvar_GetInt("net_port");
    if (game_port <= 0 || game_port >= 65534) return false;
    SteamErrMsg error{};
    if (SteamGameServer_InitEx(0, static_cast<uint16>(game_port), static_cast<uint16>(game_port + 1),
        eServerModeAuthentication, "0.1.0.0", &error) != k_ESteamAPIInitResult_OK) {
        Com_PrintError(CON_CHANNEL_SERVER, "[cod4-live] SteamGameServer initialization failed: %s\n", error);
        return false;
    }
    server_api = true;
    server_callbacks = new ServerCallbacks;
    auto* server = SteamGameServer();
    server->SetProduct("7940");
    server->SetModDir("cod4-live");
    server->SetGameDescription("cod4-live");
    server->SetDedicatedServer(com_dedicated && com_dedicated->current.integer != 0);
    server->SetAdvertiseServerActive(false);
    server->LogOnAnonymous();
    return true;
}
#endif

class ClientCallbacks {
public:
    STEAM_CALLBACK(ClientCallbacks, Ticket, GetAuthSessionTicketResponse_t);
};
ClientCallbacks* client_callbacks = nullptr;
void ClientCallbacks::Ticket(GetAuthSessionTicketResponse_t* result) {
    if (result->m_hAuthTicket != client_ticket) return;
    ticket_ready = result->m_eResult == k_EResultOK;
    ticket_failed = result->m_eResult != k_EResultOK;
}
void __cdecl DebugText(int, const char* message) {
    Com_Printf(CON_CHANNEL_DONT_FILTER, "%s", message);
}
}

bool g_steamInitialized = false;
uint64_t g_steamID = 0;
char g_steamName[64]{};

bool Steam_IsLocalOwner() {
    return client_api && SteamUser()->BLoggedOn() && SteamUtils()->GetAppID() == app_id &&
        SteamApps()->BIsSubscribedApp(app_id) && SteamApps()->GetAppOwner() == SteamUser()->GetSteamID();
}
void Steam_Init() {
#ifdef KISAK_DEDICATED
    // A dedicated server does not initialize a logged-in Steam user.
    return;
#else
    if (client_api) return;
    SetEnvironmentVariableA("SteamAppId", "7940");
    SetEnvironmentVariableA("SteamGameId", "7940");
    SteamErrMsg error{};
    if (SteamAPI_InitEx(&error) != k_ESteamAPIInitResult_OK) {
        Sys_Error("cod4-live requires Steam running and an owned copy of Call of Duty 4 (2007). Steam: %s", error);
        return;
    }
    client_api = true;
    if (!Steam_IsLocalOwner()) {
        SteamAPI_Shutdown();
        client_api = false;
        Sys_Error("Sign into the Steam account that owns Call of Duty 4 (2007). Family Sharing is not supported by cod4-live.");
        return;
    }
    g_steamInitialized = true;
    g_steamID = SteamUser()->GetSteamID().ConvertToUint64();
    I_strncpyz(g_steamName, SteamFriends()->GetPersonaName(), sizeof(g_steamName));
    SteamClient()->SetWarningMessageHook(DebugText);
    client_callbacks = new ClientCallbacks;
#endif
}
void Steam_ClientFrame() {
    if (client_api) SteamAPI_RunCallbacks();
}
uint64_t Steam_GetClientSteamID64() { return g_steamID; }
bool Steam_GetRawClientTicket(unsigned char** buffer, uint32* size) {
    Steam_ClientFrame();
    if (!buffer || !size || !ticket_ready || ticket_failed || !ticket_size || !Steam_IsLocalOwner()) return false;
    *buffer = ticket_bytes;
    *size = ticket_size;
    return true;
}
bool Steam_ClientTicketFailed() {
    return ticket_failed || client_ticket == k_HAuthTicketInvalid || !Steam_IsLocalOwner() ||
        (!ticket_ready && GetTickCount64() - ticket_started >= cod4live::steam_auth::verification_timeout_ms);
}
void Steam_CancelClientTicket() {
    if (client_api && client_ticket != k_HAuthTicketInvalid) SteamUser()->CancelAuthTicket(client_ticket);
    client_ticket = k_HAuthTicketInvalid;
    ticket_ready = false;
    ticket_failed = false;
    ticket_size = 0;
    SecureZeroMemory(ticket_bytes, sizeof(ticket_bytes));
}
bool Steam_UpdateClientAuthTicket(netadr_t address) {
    Steam_CancelClientTicket();
    if (!Steam_IsLocalOwner()) return false;
    SteamNetworkingIdentity identity{};
    uint32 ip = address.type == NA_LOOPBACK ? 0x7f000001u :
        (uint32(address.ip[0]) << 24) | (uint32(address.ip[1]) << 16) | (uint32(address.ip[2]) << 8) | address.ip[3];
    // netadr_t stores its port in network byte order; Steam expects host order.
    const uint16 port = address.port ? static_cast<uint16>(BigShort(address.port)) : static_cast<uint16>(Dvar_GetInt("net_port"));
    identity.SetIPv4Addr(ip, port);
    ticket_started = GetTickCount64();
    client_ticket = SteamUser()->GetAuthSessionTicket(ticket_bytes, sizeof(ticket_bytes), &ticket_size, &identity);
    if (client_ticket == k_HAuthTicketInvalid || ticket_size == 0 || ticket_size > sizeof(ticket_bytes)) {
        Steam_CancelClientTicket();
        return false;
    }
    return true;
}

#ifdef KISAK_MP
bool Steam_AllowChallenge(netadr_t address) { return RateAllow(address); }
int Steam_NewChallenge() {
    unsigned int nonce = 0;
    if (BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&nonce), sizeof(nonce), BCRYPT_USE_SYSTEM_PREFERRED_RNG) < 0)
        return 0;
    return static_cast<int>((nonce & 0x7ffffffeu) + 1u);
}
SteamAuthResult Steam_CheckClientTicket(const void* ticket, uint32 size, uint64_t id, netadr_t address, int challenge) {
    if (!ticket || !size || size > cod4live::steam_auth::max_ticket_bytes || !CSteamID(id).IsValid() ||
        !CSteamID(id).BIndividualAccount() || !InitServer() || !SteamGameServer()->BLoggedOn())
        return SteamAuthResult::rejected;
    if (auto* prior = Find(id)) {
        if (prior->ended || !NET_CompareAdr(prior->address, address) || prior->challenge != challenge ||
            prior->ticket.size() != size || memcmp(prior->ticket.data(), ticket, size)) return SteamAuthResult::rejected;
        if (prior->verification.expire(GetTickCount64())) return SteamAuthResult::rejected;
        return prior->verification.state == cod4live::steam_auth::State::verified ? SteamAuthResult::verified : SteamAuthResult::pending;
    }
    if (sessions.size() >= max_sessions) return SteamAuthResult::rejected;
    AuthSession session;
    session.id = id;
    session.address = address;
    session.challenge = challenge;
    session.verification.started = GetTickCount64();
    session.ticket.assign(static_cast<const unsigned char*>(ticket), static_cast<const unsigned char*>(ticket) + size);
    if (SteamGameServer()->BeginAuthSession(ticket, size, CSteamID(id)) != k_EBeginAuthSessionResultOK)
        return SteamAuthResult::rejected;
    sessions.push_back(std::move(session));
    return SteamAuthResult::pending;
}
bool Steam_IsClientVerified(const char* text_id, netadr_t address, int challenge) {
    if (address.type == NA_BOT) return true; // Only engine-created bots have this address type.
    uint64_t id;
    if (!text_id || !cod4live::steam_auth::parse_id(text_id, id)) return false;
    const auto* session = Find(id);
    return session && !session->ended && session->verification.state == cod4live::steam_auth::State::verified &&
        NET_CompareAdr(session->address, address) && session->challenge == challenge;
}
void Steam_AttachClient(const char* text_id, netadr_t address, int challenge) {
    uint64_t id;
    if (!text_id || !cod4live::steam_auth::parse_id(text_id, id)) return;
    auto* session = Find(id);
    if (session && NET_CompareAdr(session->address, address) && session->challenge == challenge) session->attached = true;
}
void Steam_OnClientDropped(const char* text_id, netadr_t address, int challenge) {
    uint64_t id;
    if (!text_id || !cod4live::steam_auth::parse_id(text_id, id)) return;
    auto* session = Find(id);
    if (session && !session->ended && NET_CompareAdr(session->address, address) && session->challenge == challenge) End(*session);
}
void Steam_CheckClients() {
    if (!InitServer()) return;
    SteamGameServer_RunCallbacks(); // Auth state and game clients are owned by this server thread.
    const auto now = GetTickCount64();
    for (auto& session : sessions) {
        if (session.ended) continue;
        if (!session.attached && now - session.verification.started >= 60000)
            session.verification.state = cod4live::steam_auth::State::rejected;
        if (session.verification.expire(now)) {
            // Match the precise admission, never a later client that happens to reuse a slot or ID.
            for (int i = 0; svs.clients && sv_maxclients && i < sv_maxclients->current.integer; ++i) {
                auto& client = svs.clients[i];
                uint64_t id;
                if (client.header.state >= CS_CONNECTED && cod4live::steam_auth::parse_id(client.cdkeyHash, id) &&
                    id == session.id && client.challenge == session.challenge && NET_CompareAdr(client.header.netchan.remoteAddress, session.address))
                    SV_DropClient(&client, "Steam ownership verification failed. Use the account that owns CoD4.", true);
            }
            if (!session.notified) NET_OutOfBandPrint(NS_SERVER, session.address,
                "error\nSteam ownership verification failed or timed out. Sign into the owner account and retry.");
            if (!session.ended) End(session);
        } else if (session.verification.state == cod4live::steam_auth::State::verified && !session.notified) {
            for (auto& challenge : svs.challenges) {
                if (!challenge.connected && challenge.challenge == session.challenge && NET_CompareAdr(challenge.adr, session.address)) {
                    challenge.pingTime = svs.time;
                    NET_OutOfBandPrint(NS_SERVER, session.address, va("challengeResponse %i", session.challenge));
                    break;
                }
            }
            session.notified = true;
        }
    }
    // Keep tombstones while late callbacks drain; no overlapping BeginAuthSession for the same ID.
    for (auto it = sessions.begin(); it != sessions.end();) {
        if (it->ended && now - it->retired_at >= 5000) it = sessions.erase(it); else ++it;
    }
}
void Steam_ServerShutdown() {
    if (server_api) {
        for (auto& session : sessions) if (!session.ended) End(session);
        delete server_callbacks;
        server_callbacks = nullptr;
        SteamGameServer()->LogOff();
        SteamGameServer_Shutdown();
    }
    sessions.clear();
    rates = {};
    server_api = false;
    server_attempted = false;
}
#endif
void Steam_Shutdown() {
#ifdef KISAK_MP
    Steam_ServerShutdown();
#endif
    Steam_CancelClientTicket();
    delete client_callbacks;
    client_callbacks = nullptr;
    if (client_api) SteamAPI_Shutdown();
    client_api = false;
    g_steamInitialized = false;
    g_steamID = 0;
    memset(g_steamName, 0, sizeof(g_steamName));
}
