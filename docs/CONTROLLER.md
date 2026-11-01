# Controller support

Open **CONTROLLER** from the native main, Options, or pause menus. The **F7**
shortcut and `controller_settings` command open the same native menu. These are
normal CoD4 menu items, sliders, selection lists, and menu stacks, including mouse
selection and keyboard navigation. There is no separate drawn settings overlay.
Controller pages use the stock Video Settings frame, highlighted rows and a
category sidebar for General, Look / Sensitivity, Sticks / Triggers, Menu Scroll,
and Button Mapping.

The D-pad and either stick navigate. A / Cross and Start / Options select;
B / Circle and Back / Share return. Y / Triangle invokes the native F1 action and
X / Square invokes F2, matching IW4x. Menu scrolling uses IW4x's configurable first
repeat, repeat interval, and acceleration. Keyboard and mouse remain available in
menus, chat, and the console.

L1/R1 (LB/RB) move focus to the previous/next control, even when a list or slider
consumes the directional buttons. D-pad Up/Down browse the focused map/server
list; A/Cross confirms its highlighted row. Left/Right adjust graphics choices,
toggles and sliders without requiring the mouse to hover over them. A setting
with only one available choice keeps focus when adjusted. Disabled and hidden
controls cannot be changed. Shoulders also leave name/password text editing.

The main menu places **Controller** between Controls and Options in its native
list. **Start New Server** includes a **MAP** choice below Bot Difficulty;
Left/Right cycles maps for the selected mode and updates the preview immediately.
This row and the map browser share the same selection, so either can be used
before selecting Start. Left/Right also changes Game Mode.

The default gameplay layout is A / Cross to jump or stand, B / Circle for stance,
X / Square for use and reload, Y / Triangle to change weapon, right trigger to fire,
left trigger to aim, shoulders for grenades, left-stick click for sprint/hold breath,
and right-stick click for melee. The D-pad uses CoD4's four action slots; View / Share
holds the scoreboard and Menu / Options opens the pause menu.

## First launch and local settings

A fresh client creates and selects a local `cod4-live` profile. The initial player
name comes from the signed-in Steam account, with `cod4-live` as the fallback.
Existing profiles and chosen player names are retained. The normal player/profile
menus remain available for changes.

By default, profiles and configuration live under
`<cod4-live install>/players/profiles/`, beside the standalone client. The launch
working directory does not redirect them into vanilla CoD4. Explicit `fs_basepath`
and `fs_homepath` overrides remain available; profiles are searched, created, and
deleted only under `fs_homepath`. Hardware/config updates keep user settings and
do not open Windows reset dialogs. Unrecognized modern CPU/GPU presets keep the
current defaults instead of terminating startup.

## Compatibility

- **Native** (`gpad_backend 0`): IW4x's XInput and PlayStation HID drivers, including
  DualShock 4, DualSense, and DualSense Edge over their supported USB/Bluetooth paths.
- **Steam Input** (`gpad_backend 1`): only XInput is read. Use Steam's **Gamepad**
  template so the virtual device emits controller axes and buttons. HID is disabled
  in this mode to avoid processing both the physical PlayStation device and its
  virtual Xbox output. Keyboard/mouse mappings remain keyboard/mouse input.

Changing **Input Backend** only selects a pending value. Choose **Apply Input
Backend**, then confirm **Apply** to test it. Cancel leaves the active backend
unchanged. The new backend is saved only after it detects a controller. If none
is detected within 15 seconds, the previous backend is restored automatically;
quitting during the test also leaves the saved backend unchanged. When using
Steam Input, launch through Steam with a Gamepad layout before applying it.

The driver automatically selects one controller, preferring a native HID device
over XInput in native mode. XInput slots are rediscovered periodically, including
virtual devices that do not generate a hardware notification. Stale samples and
disconnects release held controls. Switching backends, losing focus, and opening
menus also release held gameplay controls. Device discovery and freshness checks
continue while the window is unfocused, so unplugging or losing a device still
starts the server's 30-second reconnect grace. Background input is not dispatched
to menus or gameplay, does not count as a keyboard/mouse attempt, and cannot
produce held-button actions when focus returns.

## Settings and bindings

The native Controller menu exposes all eight IW4x button profiles: Default,
Tactical, Lefty, Nomad, and the flipped version of each. Their config names are
`buttons_default`, `buttons_tactical`, `buttons_lefty`, `buttons_nomad`, and the
corresponding `_alt` names. All four stick profiles are available: Default,
Southpaw, Legacy, and Legacy southpaw (`thumbstick_default`,
`thumbstick_southpaw`, `thumbstick_legacy`, `thumbstick_legacysouthpaw`).

**Look / Sensitivity** offers IW4x's ten presets, with multipliers
`0.6, 1.0, 1.4, 1.8, 2.0, 2.2, 2.6, 3.0, 3.5, 4.0`, plus a custom `0.05-5.0`
slider, ADS multiplier, and inversion. **Sticks / Triggers** exposes inner/outer
stick deadzones, anti-deadzone, menu stick threshold/hysteresis, and trigger
threshold/hysteresis. The effective outer deadzone is limited so it cannot
eliminate the usable stick range. **Menu Scroll** provides the four repeat timing
controls. **Button Mapping** displays all 16 current bindings with their
device glyphs. Backend, prompt family, vibration, and capped field-of-view scale
are on the main Controller page. Base FOV stays 65; scale cannot exceed 90/65.

The two glyph families use all 32 button materials named by IW4x's controller
mapping, including face buttons, shoulders, triggers, both stick clicks,
Start/Back, and each D-pad direction. Automatic prompts follow the active native
device; PlayStation and Xbox overrides are available. Native menu footers and
in-game binding prompts use inline image glyphs. The art comes from pinned
`iw4x/iw4x-rawfiles`; its GPL license, source paths, hashes, and image conversion
notes ship in `notices/`. No MW2 executable or compiled retail asset bundle is
included. Code-owned glyph textures use an isolated `images/live_controller/`
path and are released/recreated with the renderer.

Gameplay binding prompts resolve the current physical button, including custom
assignments and all eight layouts. The shared lookup covers pickup/use, reload,
jump/mantle and killcam actions, ADS, hold breath, stance, melee, scoreboard and
action slots. It recognizes combined use/reload and sprint/breath bindings, while
preferring an exact custom action binding. An unbound controller action remains
unbound instead of showing a keyboard key. A connected controller keeps gameplay
prompts active while chat or console input changes the last input; actual menus
still follow the last input device.

The reload warning includes its current binding glyph. The stock end-of-game and
quick-message exit prompts use the menu Back button (B / Circle), and the airstrike
location prompt uses its confirmation button (A / Cross). These context controls follow their
actual menu/location behavior rather than the gameplay Fire or Pause binding.
Localized wording and color codes are preserved. Escape prompts also recognize
the localized Escape key name; translations without a matching action token
receive the glyph as a prefix.

Bindings use persistent CoD key bindings with `BUTTON_A`, `BUTTON_B`, `BUTTON_X`,
`BUTTON_Y`, `BUTTON_LSHLDR`, `BUTTON_RSHLDR`, `BUTTON_LTRIG`, `BUTTON_RTRIG`,
`BUTTON_LSTICK`, `BUTTON_RSTICK`, `BUTTON_START`, `BUTTON_BACK`, `DPAD_UP`,
`DPAD_DOWN`, `DPAD_LEFT`, and `DPAD_RIGHT`.
Use the `bind` console command for custom assignments. For example, `bind BUTTON_RSTICK "+melee"` assigns right-stick click.
Selecting a different layout replaces those assignments. Existing custom assignments are retained on
startup. Gameplay custom binds remain subject to controller provenance checks.

## Aim and source accounting

The adapter uses the pinned IW4x deadzone, stick mapping, response-curve evaluator,
turn integration, slowdown, and rotational-assist math. Both engine adapters read
the host game's graph 3 with scaled knot values; cod4-live uses CoD4's
`aim_assist/view_input_3.graph` from the owned game assets. The pinned IW4x rawfiles
repository does not publish a separate replacement graph. Exact equality with
MW2's retail graph knots is not claimed. CoD4 supplies its existing
weapon ranges, ADS and FOV scaling, target bounds, enemy/team checks, visibility
traces, and melee behavior. Slowdown defaults are 0.4 at hip and 0.5 at ADS;
rotational strength is 0.6 and requires look/strafe deflection.

ADS snap is a fixed cod4-live addition, enabled for every controller player at the
user's request. Pressing the mapped ADS control (normally LT/L2) chooses the
nearest-crosshair eligible enemy in CoD4's native auto-aim region, then converges
for at most 200 ms with gain 40. Convergence is stable across frame rates. The
chosen aim bone is freshly checked against solid geometry and effect visibility
each frame. Snap requires visibility of at least 0.2, matching the crosshair
feedback threshold rather than the weaker native target-acquisition threshold.
Teammates, dead players, hidden targets, offhands, non-ADS weapons,
and lock-on-only weapons are excluded. Positive weapon `autoAimRange` is used
when present. Retail PC infantry weapons set it to zero, so those use their own
`aimAssistRangeAds` instead; no global range increase is applied.

Holding ADS never acquires another target. Release/repress permits a new snap;
losing visibility, going out of range, changing weapon, or losing eligibility
cancels the current snap. A blocked keyboard/mouse command, menu, focus loss,
frozen player state or disconnect cancels snap and requires ADS release before
it can activate again. Corrections use the current processed command angles,
converted to world space with the player's spawn/view delta and the same
kick/offset added during command serialization.
Rotation is not added on a snap frame, and native melee still runs only once.
The PC base has no auto-aim updater to reuse, so exact closed-source console
behavior is not claimed. `aim_autoaim_enabled` reports the fixed enabled state.

Gameplay aim tuning, target radius, and melee defaults are
read-only and reject non-default values even through DevGUI; the response-curve
editor is not registered. Debug display options remain separate. Lock-on-only
weapons are excluded; free-fire weapons use their own range and ammunition rules.
The mouse aim updater is bypassed for controller commands,
so melee and assist are not applied twice. Mixed-input commands disable assistance.

`gpad_in_use` tracks the last menu input for prompts. Gameplay provenance is
separate: each command reports connection, controller activity, and attempted
keyboard/mouse gameplay. Physical gameplay input is blocked before it can change
the command. Console/config action commands are also rejected unless dispatched
by a controller binding, including startup configs. Chat, voice, scoreboard,
settings, and console operations remain usable. Button releases use their actual
release time; brief taps are retained until a command has represented them.

This is client-reported provenance, not hardware attestation. A modified client
or a device that presents mouse-derived input as a controller can misreport it.

## Validation

For a bounded hardware check without launching the game, explicitly build
`cmake --build build --config Release --target controller-probe`, then run
`build\Release\controller-probe.exe --seconds 20`. It uses the same native
discovery, drivers, calibration, and deadzones as the client, prints decoded
axes/triggers/buttons, and summarizes observed movement. `--steam` selects XInput
only. Optional `--rumble` submits one low-strength 250 ms pulse; physical vibration
must be confirmed separately. Close the game during this check to avoid competing
device output. The probe injects no input and is excluded from normal builds and
the installer.

`controller-math` checks radial deadzones, constant turn integration (acceleration
disabled) at 30/60/144/250/1000 Hz,
slowdown interpolation, relative-motion rotation, inversion, stick mapping,
gameplay command classification, short-tap retention, native menu button parity,
all eight button profiles, parsing/length validation for all 32 inline glyphs,
and presence-versus-focus behavior through the server's disconnect grace. The
production ADS-snap state machine is also tested for press/release, held-input
suppression after resets, weapon changes, target loss, visibility/team/range
rejection, short-angle wrapping, the time bound and convergence across frame
rates. Those deterministic visibility inputs do not substitute for in-game
geometry/smoke testing. The integration and native drivers compile into the
Windows client.

`binding_prompts` compiles the production controller resolver, central CoD4
binding lookup and localized formatter, device-family selection, and named-glyph
encoding. It checks the eight real layouts, custom binding precedence, combined
action aliases, unbound and keyboard fallback, Xbox/PlayStation overrides,
two-binding output bounds, gameplay/menu/scoreboard/demo states, and localized
reload/end-of-game/quick-message/airstrike text adaptation. These checks do not
replace visual and physical confirmation of every gameplay prompt.

On 2026-10-31 a physical DualSense over USB was tested with the production driver
stack for 30 seconds: 2,523 sampled states decoded, both sticks and both triggers
reached full range, all 16 normal mapped buttons were observed, and released
controls returned to zero. There were no driver warnings/errors. A 250 ms rumble
pulse was submitted; physical vibration was not confirmed by that capture.
The user then confirmed that menus and gameplay work with this DualSense in
native mode after the stationary-mouse focus fix. That verifies the basic native
menu-to-match flow; it does not establish quantitative weapon-specific aim
assistance behavior or confirm vibration.

Remaining hardware acceptance includes Xbox, DualShock 4, Bluetooth, Steam Input,
alternate layouts, ADS/melee/killstreak targeting, slow walk/diagonals, live
weapon-specific slowdown/rotation and visibility cases, rumble, hotplug, and
focus loss/recovery. Command-driven debugger checks also completed an 18-second sequence that opens
Controller settings, restarts the renderer, and reopens settings, plus a 28-second
isolated listen-server sequence that loads `mp_crash`, opens settings, disconnects,
reloads the main menu, and reopens settings. There were no fatal exceptions,
missing glyph warnings, or native focus-script errors. These checks validate
startup/reload paths, not the visual appearance of the pages.

The expanded native menu build and thirteen engine test suites passed. Later visual
checks verified the general Controller page, all 16 PlayStation mapping glyphs,
and the Controller main-menu entry. The user subsequently confirmed that DualSense
navigation works in the new Controller menus and both Bot Warfare settings can
be adjusted on the earlier separate page. Bot Count and Bot Difficulty now use
inline native rows in Start New Server, immediately below Maximum Players; the
user also confirmed these inline controls work with the DualSense.
The remaining acceptance checks are tracked in VALIDATION.md.

The user subsequently confirmed the focused one-bot Shipment ADS-snap test and,
after the prompt fixes, the Square glyphs and actions for weapon pickup, reload
and skipping killcam. These checks used the native USB DualSense backend.
