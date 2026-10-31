# Validation record

Tested on Windows on October 31, 2026 with Visual Studio 2022, Win32 Release,
classic KisakCOD simulation and DX9. This is an initial development build; the
remaining acceptance cases below are not claimed as passed.

## Build and automated checks

- Full multiplayer client and dedicated server compile and link successfully.
- Five engine CTest suites pass: controller-math, input_policy, usercmd_codec,
  server_input, and steam_auth. The separate installer suite also passes.
- Controller tests exercise the vendored IW4x calculations, radial deadzones,
  stick layouts, inversion, slowdown, relative-motion rotation, short-tap
  retention, command-source filtering, and controller menu-focus preservation.
  Constant-turn integration is checked at 30/60/144/250/1000 Hz with acceleration
  disabled; this does not validate every accelerated aiming trace.
- Protocol tests compile production serialization code, cover all signed-byte
  axis pairs, both encoding branches, metadata-only changes, chained commands,
  malformed flags, and truncation. Server tests compile production enforcement
  and cover fresh-command timing, resets, disconnects, frozen pre-admission
  timestamps, replay/restart behavior, bot exemptions and ownership rejection.
- Installer tests cover separate destinations, payload hashes, cancellation,
  repair, rollback, user configuration preservation and unowned-file protection.

## Live evidence

- The real app-7940 Steam API accepted a directly owned ticket through the
  asynchronous GameServer callback and revoked it after cancellation. Anonymous
  dedicated authentication and destination-bound client tickets passed.
- A compiled dedicated server loaded mp_crash. UDP probes passed nonce exchange,
  rejection before verification, authenticated admission, cancellation disconnect,
  corrupted/full-size-invalid-ticket rejection, and reconnect after cleanup.
- The client launched from outside its installation directory, authenticated to
  the local server, loaded Crash and reached team selection with PlayStation
  prompts. Its profile and assets remained in the standalone runtime directory.
- A native USB DualSense produced 2,523 decoded states during a 30-second check
  through the production driver stack. Both sticks and triggers reached their
  full ranges, all 16 normal mapped buttons were observed, and idle returned to
  zero. Exactly one HID device and no virtual XInput device were used. A 250 ms
  vibration command was submitted; physical vibration is not yet confirmed.
- Disabling native HID by selecting Steam Input mode without a virtual XInput
  device triggered the live server's controller-disconnected timeout.
- The settings UI was visually checked, including FOV endpoints 65 and 90 and
  backend selection. A physical menu test exposed stationary mouse events
  overwriting controller focus; that path is fixed and has a regression test.
- After the fix, the user confirmed that DualSense menu selection and gameplay
  both work, including spawning and stick movement/look.
- Valve's dedicated runtime helper downloaded app 1007 directly from Valve,
  checked its signatures and x86 DLLs, and passed anonymous server API login
  while loading the downloaded local steamclient.dll.

## Acceptance still required

- Broaden physical checks to short taps, held actions, scoreboard, all class/team
  menus, location selection, hotplug and physical vibration.
- Xbox/XInput hardware, PlayStation Bluetooth, and Steam Input virtual-gamepad
  testing require those devices/configurations. Native USB decoding alone does
  not establish their end-to-end behavior.
- Live enemy/teammate and occlusion scenarios, every weapon class, hip/ADS/melee,
  FOV endpoints and multiple render rates need a playtest. Unit calculations do
  not establish that the full aiming feel is finished.
- Held keyboard/mouse and simultaneous stick attempts, clean resets, chat and
  console exemptions, packet loss/reordering, demo playback, and map-change
  behavior have automated/source coverage but need a broader live multiplayer
  acceptance session.
- No non-owning or Family Sharing Steam account was available. Rejection is
  tested in the ownership state machine, not with a second live account. Steam
  outage behavior and a completely clean dedicated host also need testing.
- No public master service is deployed. Use direct addresses, Favorites or LAN;
  see NETWORK.md. No two-machine WAN match has been validated.

Input metadata is a policy report from the client, not physical hardware
attestation. Modified clients and mouse-to-gamepad adapters can misreport it.
