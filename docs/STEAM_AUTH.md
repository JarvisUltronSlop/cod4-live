# Steam ownership and admission

cod4-live uses Steam app 7940, Call of Duty 4: Modern Warfare (2007). The client
requires Steam to be running, signed in, and to report that the signed-in user
owns the game. Family Sharing is excluded: the license owner must match the
playing account. Selecting an installation directory establishes an asset source;
it is not used as ownership evidence.

The dedicated server initializes Steam's GameServer API independently of the
client user API. It logs on anonymously in authentication mode without enabling
VAC, and does not advertise itself on Steam's master list. The Steam query socket
uses the gameplay port plus one; both must be available. Failure to initialize or
log on closes player admission. Listen servers use the same GameServer verifier.

Admission proceeds as follows:

1. The client obtains a fresh ticket bound to the destination address and waits
   for Steam's ticket-creation callback.
2. A bounded `getchallenge` request supplies the ticket and account ID. The server
   first issues a random `steamChallenge` nonce. The client must echo it from the
   same address before the server starts a Steam authentication session.
3. `BeginAuthSession` success leaves the request pending. Only an asynchronous
   successful ownership callback with matching user and owner IDs permits a
   `challengeResponse` and subsequent `connect` admission.
4. Pending verification expires after 30 seconds of monotonic wall time. Later
   invalidation or ticket cancellation drops the matching player. Disconnects,
   abandoned challenges and server shutdown close their Steam sessions.

SteamID strings and Base64 ticket lengths/padding are strictly checked. Per-IP
request windows and a fixed maximum of 128 authentication records bound admission
work. Resends reuse a pending session. Only engine-created bot addresses bypass
Steam validation; localhost human players authenticate normally.

Steam callbacks identify an account, not a particular ticket or connection
generation. The implementation allows one active authentication session per
account, calls `EndAuthSession` before reuse, and retains a five-second tombstone
while callbacks drain. This avoids the old unqualified delayed-kick queue, but the
five-second delay is not cryptographic proof that no older callback can arrive.
Authentication relies on Steam's session lifecycle. It is not a client anti-tamper
system or a guarantee about modified Steam runtimes.

## Validation

The `steam_auth` CTest case exercises the production strict decoder and ownership state
machine, including malformed/oversized tickets, overflowed IDs, Family Sharing,
timeout boundaries, revocation, and late success after rejection.

Manual integration probes can be enabled with `-DCOD4LIVE_STEAM_PROBES=ON` in a
Win32 CMake build. They require the actual logged-in owner account and Steam
service availability, so they are not automatically run by CTest:

- `steam_auth_probe`: real app-7940 client ticket, anonymous GameServer login,
  asynchronous ownership success, and cancellation/revocation. `--ip-bound`
  exercises the IPv4 binding used by the game; `--server-only` omits client API
  initialization.
- `steam_game_handshake_probe 28970`: exercises a running local game server's
  real nonce/ownership/admission path and checks that a premature connect fails.
  `28970 --tampered` checks rejection of a corrupted Steam ticket;
  `28970 --full-ticket` verifies that a complete 1024-byte invalid ticket reaches
  the verifier without being truncated by the legacy command reader.

On October 31, 2026, the API probe passed owner validation, anonymous server login,
asynchronous success, and cancellation revocation for both SteamID-bound and
IPv4-bound tickets. The server-only API initialization probe also passed. Steam
was running on that host; this does not prove a clean machine without Steam client
installation has all dedicated redistributables. No non-owner or Family Sharing
account was available for a live account test; those policy cases are covered by
the deterministic tests.

The packaged `install-server-runtime.ps1` was then run against an isolated server
folder. It downloaded app 1007 build 25329704 directly through anonymous Valve
SteamCMD, verified the four Win32 DLL signatures, and installed them locally. The
server-only API probe reported that it loaded that folder's `steamclient.dll` and
connected anonymously without initializing the client/user API. This checks local
redistributable loading; a separate clean Windows VM was not available for testing.

The compiled OpenAL dedicated server was also exercised on `127.0.0.1:28970`
with `mp_crash` loaded. The real UDP probe passed address-challenge exchange,
rejection of a connect before ownership validation, asynchronous verified
challenge delivery, authenticated connection admission, and server disconnect
after ticket cancellation. Separate runs rejected a corrupted signed ticket and
an invalid full-size 1024-byte ticket, with the latter receiving its address
challenge before rejection (confirming that the legacy line limit no longer
truncates the request). A fresh valid ticket could reconnect after cleanup.

References: [Steam authentication](https://partner.steamgames.com/doc/features/auth),
[GameServer API](https://partner.steamgames.com/doc/api/ISteamGameServer), and
[callback owner identity](https://partner.steamgames.com/doc/api/ISteamUser#ValidateAuthTicketResponse_t).
