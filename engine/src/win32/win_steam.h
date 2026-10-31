#pragma once
#include "win_local.h"
#include <cstdint>
void Steam_Init();
void Steam_Shutdown();
void Steam_ClientFrame();
bool Steam_IsLocalOwner();
bool Steam_UpdateClientAuthTicket(netadr_t serverIpv4);
bool Steam_GetRawClientTicket(unsigned char** buffer, uint32_t* size);
bool Steam_ClientTicketFailed();
void Steam_CancelClientTicket();
uint64_t Steam_GetClientSteamID64();
enum class SteamAuthResult { pending, verified, rejected };
bool Steam_AllowChallenge(netadr_t);
int Steam_NewChallenge();
SteamAuthResult Steam_CheckClientTicket(const void*, uint32_t, uint64_t, netadr_t, int challenge);
bool Steam_IsClientVerified(const char* id, netadr_t, int challenge);
void Steam_AttachClient(const char* id, netadr_t, int challenge);
void Steam_OnClientDropped(const char* id, netadr_t, int challenge);
void Steam_CheckClients();
void Steam_ServerShutdown();
extern bool g_steamInitialized;
