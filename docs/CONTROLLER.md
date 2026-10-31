# Controller support

Open **Controller settings** with **Y / Triangle** while a menu is open, **F7**, or
the `controller_settings` console command. Use the D-pad or left stick to navigate,
A / Cross to select, and B / Circle to return. Keyboard and mouse remain available
in menus, chat, and the console.

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

The driver automatically selects one controller, preferring a native HID device
over XInput in native mode. XInput slots are rediscovered periodically, including
virtual devices that do not generate a hardware notification. Stale samples and
disconnects release held controls. Switching backends, losing focus, and opening
menus also release held gameplay controls.

## Settings and bindings

The menu offers default, tactical, lefty, nomad, default flipped, and tactical
flipped button layouts (`buttons_default`, `buttons_tactical`, `buttons_lefty`,
`buttons_nomad`, `buttons_default_alt`, `buttons_tactical_alt`); standard, southpaw,
legacy, and legacy southpaw sticks (`thumbstick_default`, `thumbstick_southpaw`,
`thumbstick_legacy`, `thumbstick_legacysouthpaw`);
look and ADS sensitivity; inverted look; inner/outer stick deadzones; trigger
threshold; automatic/Xbox/PlayStation prompts; vibration; and field of view.

Bindings use persistent CoD key bindings with `BUTTON_A`, `BUTTON_B`, `BUTTON_X`,
`BUTTON_Y`, `BUTTON_LSHLDR`, `BUTTON_RSHLDR`, `BUTTON_LTRIG`, `BUTTON_RTRIG`,
`BUTTON_LSTICK`, `BUTTON_RSTICK`, `BUTTON_START`, `BUTTON_BACK`, `DPAD_UP`,
`DPAD_DOWN`, `DPAD_LEFT`, and `DPAD_RIGHT`.
Use the `bind` console command for custom assignments. For example, `bind BUTTON_RSTICK "+melee"` assigns right-stick click.
Selecting a different layout replaces those assignments. Existing custom assignments are retained on
startup. Prompts are readable button names; no MW2 UI art is included.

## Aim and source accounting

The adapter uses the pinned IW4x deadzone, stick mapping, response curve, turn
integration, slowdown, and rotational-assist math. CoD4 supplies its existing
weapon ranges, ADS and FOV scaling, target bounds, enemy/team checks, visibility
traces, and melee behavior. Slowdown defaults are 0.4 at hip and 0.5 at ADS;
rotational strength is 0.6 and requires look/strafe deflection. It does not snap to
targets when ADS starts. Gameplay aim tuning, target radius, and melee defaults are
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
gameplay command classification, and short-tap retention. The integration and
native drivers compile into the Windows client.

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
focus loss/recovery. The final native client/server build and all five automated
tests passed after the focus and fixed-tuning changes.
