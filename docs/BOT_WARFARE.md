# Bot Warfare integration

The upstream source is [INeedBots / IW3 Bot Warfare](https://github.com/ineedbots/iw3_bot_warfare),
pinned to `901895a10dee921142cf4228c467f99013b5b6c4` in `upstream-lock.json`.
The author's README grants permission to use, host, modify and merge this code with
credit. The complete upstream README, including that permission and its credits,
ships in `notices/BotWarfare-UPSTREAM-README.md`. Credit: INeedGames / INeedBots,
PeZBot team, Ability, Salvation and the CoD4x team. These files are not relicensed
as part of the engine.

Only GSC source and waypoint CSV data are included. Retail game scripts, maps,
models, audio and fastfiles still come from the user's Steam installation.
Waypoint data covers the 21 retail/DLC map variants supplied by upstream.

## Settings

`live_botCount` selects the desired number of bots, default 0. The actual count is
limited to `sv_maxclients - max(human clients, 1)`. The engine also checks this at
bot allocation, so script commands cannot fill the reserved player slot. A joining
human can occupy that slot; additional humans require a free slot. The Bot Warfare
manager removes excess bots when the human population or selected count changes.

`live_botDifficulty` selects upstream skill 0–7, default 3. Zero randomizes skill;
1 is easiest and 7 hardest. Start New Server contains native BOTS and BOT
DIFFICULTY value controls immediately below Maximum Players. Left/right on the
D-pad, either stick, or keyboard adjusts a focused value; Cross/A or Enter also
cycles forward. The gray label shows the available bot maximum. A one-player
capacity leaves zero bot slots; increase Maximum Players before adding bots.
There is no separate Bot Warfare settings page or main-menu entry. Remote clients
cannot change a server's bot settings through these controls. Server owners can
also set the dvars in their configuration or console, including while hosting.

The adapter maintains `bots_manage_fill` in bots-only mode, enables removal of
excess bots, and maps difficulty to `bots_skill`. Bot Warfare's scripted admin
menu and first-player-as-host privilege are disabled in favor of the native host
menu. Its waypoint editor and chat module are not started.

## Engine bridge

Classic KisakCOD lacks CoD4x's bot input builtins and automatic `scripts/mp`
loader. cod4-live explicitly loads `_cod4live.gsc`, initializes its builtin table,
then starts Bot Warfare after the stock gametype startup callback. Only the
`maps/mp/bots/` namespace uses packaged source ahead of fastfiles; stock assets
retain their normal resolution.

The native adapter supplies actions, signed movement, melee parameters, bot
identity and bounded read-only `scriptdata/` files. It uses the analog movement
already calculated by Bot Warfare's GSC, preserving the player's view-relative
movement convention. Server-created bot commands use server time and native
movement/weapon processing. Weapon and offhand selections are intercepted because
bots do not have a network client to consume weapon-selection server commands.
Input state and file handles reset when scripts unload or a bot slot is reused.
Damage from a projectile whose owner has disconnected is attributed to the world
before invoking the stock player-damage callback, matching the engine's existing
behavior for non-player targets.

Two local changes in `_bot_internal.gsc` reorder pure visibility predicates:
FOV/memory and smoke eligibility run before bone traces, and failed sight traces
skip construction of detailed bullet-trace results. The target rules, bone order,
reaction times and 50 ms AI update interval are unchanged.

Bot identity is derived from the server's `NA_BOT` address type; no client-provided
flag grants this exemption from Steam/controller checks.

## Multiplayer script capacity

The native Host Game Maximum Players control selects 1-64 players using the
controller's left/right inputs. It sets `sv_maxclients`; Bot Warfare follows that
pending capacity before hosting and the allocated capacity during a match.
`sv_maxclients` defaults to 32 and supports up to 64 independently of the legacy
`ui_maxclients` value, which mirrors the allocated capacity when a server starts.
A 64-slot server can request 63 bots while reserving one human slot. No
player/network structures or simulation timing were enlarged or changed.

Bot Warfare exceeded the original child-variable pool around 38 bots on `mp_crash`.
The multiplayer VM now has 262,141 usable child-variable slots and 65,533 usable
parent-object slots. Child hash, sibling and free-list indices use 32 bits; parent
IDs and thread-stack IDs remain unsigned 16-bit values. The VM keeps 3,200 slots
of emergency headroom in each pool. The single-player layout is unchanged.

The server's script HUD pool grows from 1,024 to 8,192 objects to accommodate
per-viewer claymore/headicon objects at high player counts. Script HUD IDs remain
16-bit, and each player's transmitted HUD still contains the original 31 current
and 31 archival elements. At the stock two-mine and 32-player team limits, mine
headicons can require 4,096 objects before bombsquad and ordinary HUD elements.
The single-player HUD pool remains 256 objects.

Object array keys occupy the expanded 16-bit object-ID range. Internal notify and
stack keys are `0x1FFFE` and `0x1FFFF`; integer array keys therefore accept
`-0x7E0000` through `0x7FFFFF` in multiplayer. Arrays retain a 65,535-element bound,
and object reference counts retain a 16-bit bound; exceeding either fails before
mutating the allocator. This prevents counter wraparound in the larger pool.

`script_variables` tests compile the production allocator and hash functions,
fill and recycle both pools, maintain 120,000 simultaneous hashed children,
cross the former 16-bit child and signed-16-bit parent boundaries, and exercise
object/integer key separation, overflow rejection, animation-tree lookup reuse,
and full HUD allocation/recycling. `script_references` tests the production
string/vector reference increments, including atomic packed-string updates and
overflow rejection. The optional manual
`tests/bot_runtime_probe.py` checks a running local server's bot count, movement,
difficulty, fast/full restarts, removal and slot reuse.
