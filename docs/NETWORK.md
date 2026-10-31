# Network compatibility and controller admission

cod4-live uses protocol **10003**. The client connection request, server acceptance
check, `getinfo` response, browser filter, master-list query and demo extension all
use this version. Vanilla CoD4, unmodified KisakCOD and cod4x1khz are incompatible.
Recordings use `.dm_10003`.

## Finding a server

The Local browser broadcasts `getinfo` to UDP ports 28960–28963. Its replies must
advertise protocol 10003. Direct connection and Favorites do not require a master
service; use `connect IP:PORT` in the game console. Both paths still require Steam
ownership verification and controller admission. Local discovery also depends on
the network/firewall allowing UDP broadcasts and replies.

Automatic Internet listings are **not provided** by this release. KisakCOD's
`SV_MasterHeartbeat` registration code is disabled upstream, and the legacy master
configuration points at Activision's service. Although the Internet-browser query
now requests protocol 10003, no compatible public master service has been deployed
or validated. Use Local, Favorites or a host's direct address. Do not assume that
opening a dedicated server registers it with the Internet browser.

## Command format and policy

Each user command explicitly serializes a controller metadata byte, including on
otherwise idle commands. It occupies existing padding in the 32-byte in-memory
command structure. Forward/right movement retains both signed eight-bit axes;
the original PC format's four directional bits lose analog precision.

| Bit | Meaning |
| --- | --- |
| 0 | Controller is connected |
| 1 | Controller supplied gameplay input |
| 2 | Keyboard/mouse gameplay was attempted |

Reserved bits and an active-but-disconnected controller report are invalid. The
metadata is a client report, not proof of physical hardware: adapters and modified
clients can impersonate controller input.

Authenticated clients without a connected controller remain in the loading state
until a fresh command reports one; they do not enter the match. During gameplay,
absent-controller or mixed-input commands retain the last accepted view/weapon and
have movement/actions neutralized. The server warns on the first prohibited
attempt and kicks after five seconds of continuing attempts. Resetting that window
requires 500 ms of reported clean input; a packet gap alone does not reset it.
Disconnecting a controller allows 30 seconds to reconnect. Internally created bots
are exempt through server-owned bot identity.

Policy time comes from the server clock. Active gameplay only processes newer
commands. Before admission, the client may have no snapshots and a frozen command
timestamp; each newly accepted network packet's latest command supplies its device
report. Duplicate/out-of-order network packets are discarded by the netchannel.
Map restarts preserve enforcement state; reusing `lastUsercmd` does not
advance or reset its timers. A fresh report after the deadline can trigger a kick.
Ordinary packet loss follows the engine's connection timeout handling.

## Automated coverage

`usercmd_codec` compiles production codec functions and checks all encoding
branches, chained commands, metadata-only changes, invalid flags and truncated
packets. `server_input` compiles production admission/enforcement functions against
an engine-service fixture and checks initial admission, server-clock timing,
reconnection, restart replay, bot exemption and failed Steam verification.
`input_policy` covers analog precision, warning/reset boundaries and FOV guards.
These tests do not replace a live two-machine connection/controller playtest.
