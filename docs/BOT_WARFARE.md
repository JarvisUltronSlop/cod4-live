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
1 is easiest and 7 hardest. The native host menu saves these settings before a
match and applies live changes while hosting. Remote clients cannot change a
server's bot settings through this menu. Server owners can set these dvars in
their server configuration.

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

Bot identity is derived from the server's `NA_BOT` address type; no client-provided
flag grants this exemption from Steam/controller checks.
