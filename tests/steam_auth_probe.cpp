// Live integration probe. Requires the Steam account that owns CoD4, no game assets.
// Uses app 7940 exclusively and never logs ticket contents or account credentials.
#include <steamsdk/steam/steam_api.h>
#include <steamsdk/steam/steam_gameserver.h>
#include <Windows.h>
#include <cstdio>
#include <cstring>
struct Probe {
    HAuthTicket handle = k_HAuthTicketInvalid;
    bool ready = false, server_connected = false, validated = false, revoked = false;
    bool direct_owner = false;
    int auth_result = -1, login_result = -1;
    STEAM_CALLBACK(Probe, Ticket, GetAuthSessionTicketResponse_t);
    STEAM_GAMESERVER_CALLBACK(Probe, Validate, ValidateAuthTicketResponse_t);
    STEAM_GAMESERVER_CALLBACK(Probe, Connected, SteamServersConnected_t);
    STEAM_GAMESERVER_CALLBACK(Probe, Failed, SteamServerConnectFailure_t);
};
void Probe::Ticket(GetAuthSessionTicketResponse_t* r) { if (r->m_hAuthTicket == handle) ready = r->m_eResult == k_EResultOK; }
void Probe::Validate(ValidateAuthTicketResponse_t* r) {
    auth_result = int(r->m_eAuthSessionResponse);
    direct_owner = r->m_OwnerSteamID == r->m_SteamID;
    if (r->m_eAuthSessionResponse == k_EAuthSessionResponseOK) validated = true; else revoked = true;
}
void Probe::Connected(SteamServersConnected_t*) { server_connected = true; }
void Probe::Failed(SteamServerConnectFailure_t* r) { login_result = int(r->m_eResult); }
static void pump(unsigned timeout, bool& done) {
    const auto start = GetTickCount64();
    while (!done && GetTickCount64() - start < timeout) {
        SteamAPI_RunCallbacks(); SteamGameServer_RunCallbacks(); Sleep(10);
    }
}
int main(int argc, char** argv) {
    const bool headless = argc > 1 && !strcmp(argv[1], "--server-only");
    SetEnvironmentVariableA("SteamAppId", "7940");
    SteamErrMsg error{};
    if (!headless && SteamAPI_InitEx(&error) != k_ESteamAPIInitResult_OK) {
        std::printf("client_init=failed error=%s\n", error); return 2;
    }
    if (!headless) {
        const bool owner = SteamUtils()->GetAppID() == 7940 && SteamUser()->BLoggedOn() &&
            SteamApps()->BIsSubscribedApp(7940) && SteamApps()->GetAppOwner() == SteamUser()->GetSteamID();
        std::printf("local_direct_owner=%d appid=%u\n", owner, SteamUtils()->GetAppID());
        if (!owner) { SteamAPI_Shutdown(); return 3; }
    }
    if (SteamGameServer_InitEx(0, 28990, 28991, eServerModeAuthentication, "0.1.0.0", &error) != k_ESteamAPIInitResult_OK) {
        std::printf("server_init=failed error=%s\n", error); if (!headless) SteamAPI_Shutdown(); return 4;
    }
    int outcome = 0;
    {
        Probe probe;
        auto* server = SteamGameServer();
        server->SetProduct("7940"); server->SetModDir("cod4-live"); server->SetGameDescription("cod4-live authentication probe");
        server->SetDedicatedServer(true); server->SetAdvertiseServerActive(false); server->LogOnAnonymous();
        pump(30000, probe.server_connected);
        std::printf("server_connected=%d login_result=%d\n", probe.server_connected, probe.login_result);
        if (!probe.server_connected) outcome = 5;
        else if (!headless) {
            unsigned char ticket[1024]{}; uint32 size = 0;
            SteamNetworkingIdentity identity{};
            if (argc > 1 && !strcmp(argv[1], "--ip-bound")) identity.SetIPv4Addr(0x7f000001u, 28990);
            else identity.SetSteamID(server->GetSteamID());
            probe.handle = SteamUser()->GetAuthSessionTicket(ticket, sizeof(ticket), &size, &identity);
            pump(30000, probe.ready);
            const auto result = probe.ready ? server->BeginAuthSession(ticket, size, SteamUser()->GetSteamID()) : k_EBeginAuthSessionResultInvalidTicket;
            std::printf("ticket_ready=%d begin_result=%d\n", probe.ready, int(result));
            if (result != k_EBeginAuthSessionResultOK) outcome = 6;
            else {
                pump(30000, probe.validated);
                std::printf("async_verified=%d direct_owner=%d auth_result=%d\n", probe.validated, probe.direct_owner, probe.auth_result);
                if (!probe.validated || !probe.direct_owner) outcome = 7;
                SteamUser()->CancelAuthTicket(probe.handle); probe.handle = k_HAuthTicketInvalid;
                pump(10000, probe.revoked);
                std::printf("cancel_revocation=%d auth_result=%d\n", probe.revoked, probe.auth_result);
                if (!probe.revoked && !outcome) outcome = 8;
                server->EndAuthSession(SteamUser()->GetSteamID());
            }
            if (probe.handle != k_HAuthTicketInvalid) SteamUser()->CancelAuthTicket(probe.handle);
            SecureZeroMemory(ticket, sizeof(ticket));
        }
        server->LogOff();
    }
    SteamGameServer_Shutdown(); if (!headless) SteamAPI_Shutdown();
    return outcome;
}
