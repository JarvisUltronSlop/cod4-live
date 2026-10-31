# Source provenance and notices

cod4-live is distributed under GNU GPL version 3; see the root LICENSE.

- KisakCOD source: https://github.com/SwagSoftware/KisakCOD at
  `8aadf94cc82795d6829ca06052996a5ec79001c3`. Original license and notices remain
  under `engine/`.
- IW4x controller components: https://github.com/iw4x/iw4x-client at
  `47c44fc0ce6f05172f34cc88c294121615df12e4`. Vendored portable controller code
  retains its attribution and GPL license; the CoD4 bridge is project-specific.
- Installer UI and transactional packaging adapted from
  https://github.com/JarvisUltronSlop/cod4x1khz at
  `59b5d59fefc6b89e251da5bdd1ccdc11dbf00098` under GPLv3.
- Microsoft D3DX runtime: Microsoft.DXSDK.D3DX 9.29.952.8, SHA-256 pinned in
  `upstream-lock.json`; its license accompanies packaged builds.
- Steamworks interfaces/runtime retain Valve's applicable SDK terms and notices.
  Build preparation restores the SDK binary matching the pinned engine headers.
- OpenAL Soft 1.25.2, LGPL: the existing KisakCOD audio backend replaces the
  incompatible retail Miles ABI. Preserve its license and corresponding source.
- Tracy v0.12.2 retains its upstream license and is fetched for build headers;
  instrumentation is disabled in the default build.

Call of Duty retail data and Bink remain their owners' property. Runtime
files are copied from the user's installed game, are not committed, and are not
embedded in the setup payload. No MW2 game art is required for controller prompts.
