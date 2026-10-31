// Exercise the actual cod4-live UDP admission path against a local dedicated server.
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <Windows.h>
#include <steamsdk/steam/steam_api.h>
#include <live/input_policy.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#pragma comment(lib, "ws2_32.lib")
struct TicketResult {
    HAuthTicket handle = k_HAuthTicketInvalid;
    bool ready = false;
    STEAM_CALLBACK(TicketResult, OnTicket, GetAuthSessionTicketResponse_t);
};
void TicketResult::OnTicket(GetAuthSessionTicketResponse_t* r) { if (r->m_hAuthTicket == handle) ready = r->m_eResult == k_EResultOK; }
static std::string encode(const unsigned char* bytes, unsigned size) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    for (unsigned i = 0; i < size; i += 3) {
        const unsigned n = (unsigned(bytes[i]) << 16) | (i + 1 < size ? unsigned(bytes[i + 1]) << 8 : 0) | (i + 2 < size ? bytes[i + 2] : 0);
        result += alphabet[(n >> 18) & 63]; result += alphabet[(n >> 12) & 63];
        result += i + 1 < size ? alphabet[(n >> 6) & 63] : '=';
        result += i + 2 < size ? alphabet[n & 63] : '=';
    }
    return result;
}
static std::string receive(SOCKET socket, unsigned timeout) {
    const auto start = GetTickCount64();
    while (GetTickCount64() - start < timeout) {
        SteamAPI_RunCallbacks();
        char buffer[4096]; const int n = recv(socket, buffer, sizeof(buffer) - 1, 0);
        if (n >= 4 && static_cast<unsigned char>(buffer[0]) == 255) {
            buffer[n] = 0; return std::string(buffer + 4, n - 4);
        }
        Sleep(10);
    }
    return {};
}
static void transmit(SOCKET socket, const std::string& message) {
    std::string packet(4, static_cast<char>(255)); packet += message;
    send(socket, packet.c_str(), static_cast<int>(packet.size() + 1), 0);
}
int main(int argc, char** argv) {
    const int port = argc > 1 ? std::atoi(argv[1]) : 28970;
    const bool tampered = argc > 2 && !std::strcmp(argv[2], "--tampered");
    if (port <= 0 || port > 65535) return 1;
    SetEnvironmentVariableA("SteamAppId", "7940");
    SteamErrMsg error{};
    if (SteamAPI_InitEx(&error) != k_ESteamAPIInitResult_OK) { std::printf("init_failed=%s\n", error); return 2; }
    int outcome = 0;
    {
        TicketResult result;
        unsigned char ticket[1024]{}; uint32 size = 0;
        SteamNetworkingIdentity remote{}; remote.SetIPv4Addr(0x7f000001, static_cast<uint16>(port));
        result.handle = SteamUser()->GetAuthSessionTicket(ticket, sizeof(ticket), &size, &remote);
        const auto started = GetTickCount64();
        while (!result.ready && GetTickCount64() - started < 30000) { SteamAPI_RunCallbacks(); Sleep(10); }
        if (!result.ready) outcome = 3;
        else {
            if (tampered) ticket[size / 2] ^= 0x80;
            const auto encoded = encode(ticket, size);
            WSADATA wsa{}; WSAStartup(MAKEWORD(2, 2), &wsa);
            SOCKET socket_handle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            sockaddr_in address{}; address.sin_family = AF_INET; address.sin_port = htons(static_cast<u_short>(port)); address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
            connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address));
            u_long nonblocking = 1; ioctlsocket(socket_handle, FIONBIO, &nonblocking);
            auto challenge_request = [&](int nonce) {
                return "getchallenge " + std::to_string(nonce) + " \"" + encoded + "\" \"" + std::to_string(SteamUser()->GetSteamID().ConvertToUint64()) + "\"";
            };
            auto connect_request = [&](int nonce) {
                return "connect \"\\protocol\\" + std::to_string(live::Protocol) + "\\challenge\\" + std::to_string(nonce) +
                    "\\qport\\17001\\name\\SteamProbe\\rate\\25000\\snaps\\20\\cl_anonymous\\0\"";
            };
            transmit(socket_handle, challenge_request(0));
            auto reply = receive(socket_handle, 5000);
            int nonce = 0;
            const bool cookie = std::sscanf(reply.c_str(), "steamChallenge %d", &nonce) == 1 && nonce > 0;
            std::printf("address_challenge=%d\n", cookie);
            if (!cookie) { std::printf("reply=%s\n", reply.c_str()); outcome = 4; }
            else {
                transmit(socket_handle, connect_request(nonce));
                reply = receive(socket_handle, 5000);
                const bool premature_rejected = reply.find("error") == 0;
                std::printf("preauth_connect_rejected=%d\n", premature_rejected);
                if (!premature_rejected) outcome = 5;
                else {
                    int accepted = 0;
                    const auto start = GetTickCount64();
                    do {
                        transmit(socket_handle, challenge_request(nonce));
                        reply = receive(socket_handle, 3000);
                        if (reply.find("error") == 0) break;
                    } while (std::sscanf(reply.c_str(), "challengeResponse %d", &accepted) != 1 && GetTickCount64() - start < 35000);
                    if (tampered) {
                        const bool denied = reply.find("error") == 0;
                        std::printf("tampered_ticket_rejected=%d\n", denied);
                        if (!denied) outcome = 6;
                    } else {
                        const bool authenticated = accepted == nonce;
                        std::printf("ownership_challenge_verified=%d\n", authenticated);
                        if (!authenticated) { std::printf("reply=%s\n", reply.c_str()); outcome = 7; }
                        else {
                            transmit(socket_handle, connect_request(nonce));
                            reply = receive(socket_handle, 5000);
                            const bool admitted = reply.find("connectResponse") == 0;
                            std::printf("authenticated_connect_admitted=%d\n", admitted);
                            if (!admitted) { std::printf("reply=%s\n", reply.c_str()); outcome = 8; }
                            SteamUser()->CancelAuthTicket(result.handle); result.handle = k_HAuthTicketInvalid;
                            reply = receive(socket_handle, 10000);
                            const bool revoked = reply.find("disconnect") == 0 || reply.find("error") == 0;
                            std::printf("cancelled_ticket_disconnected=%d\n", revoked);
                            if (!revoked && !outcome) outcome = 9;
                        }
                    }
                }
            }
            closesocket(socket_handle); WSACleanup();
        }
        if (result.handle != k_HAuthTicketInvalid) SteamUser()->CancelAuthTicket(result.handle);
        SecureZeroMemory(ticket, sizeof(ticket));
    }
    SteamAPI_Shutdown();
    return outcome;
}
