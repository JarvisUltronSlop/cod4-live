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
- IW4x controller artwork: https://github.com/iw4x/iw4x-rawfiles at
  `5009e6d54dab90f6196a8518b9a4d084bed73e2b`, whose GPLv3 license is retained.
  Only the controller image set is imported. Original IWI8 files are retained in
  `vendor/iw4x-controller-images`; the IWI6 container conversion preserves encoded
  pixel data. Eight unused malformed big variants are excluded from the runtime.
  The installed image manifest records source and output hashes. No MW2 maps,
  fastfiles or executables are included.
- IW3 Bot Warfare by INeedGames/INeedBots:
  https://github.com/ineedbots/iw3_bot_warfare at
  `901895a10dee921142cf4228c467f99013b5b6c4`. Upstream permits reuse and modification
  with attribution; its README and credit notice accompany the script files.
- Microsoft D3DX runtime: Microsoft.DXSDK.D3DX 9.29.952.8, SHA-256 pinned in
  `upstream-lock.json`; its license and third-party NOTICE accompany packaged builds.
- Steamworks interfaces/runtime retain Valve's applicable SDK terms and notices.
  Build preparation restores the SDK binary matching the pinned engine headers.
- OpenAL Soft 1.25.2, LGPL: the existing KisakCOD audio backend replaces the
  incompatible retail Miles ABI. Preserve its license and corresponding source.
  Packaging creates `OpenAL-Soft-source.zip`; distribute it with
  `cod4-live-source.zip` and the matching installer. The full project source and
  build scripts allow rebuilding and relinking against a modified audio library.
- Tracy v0.12.2 retains its upstream license and is fetched for build headers;
  instrumentation is disabled in the default build.
- The executable branding is an orange adaptation of the classic Call of Duty 4
  numeral mark, requested for this project. Call of Duty branding remains
  Activision's property; it is not relicensed by the code's GPL notice. The
  recolored icon source and palette-conversion notes are in `native/branding/`.

Call of Duty retail data and Bink remain their owners' property. Runtime
files are copied from the user's installed game, are not committed, and are not
embedded in the setup payload. Controller prompts use the narrowly imported IW4x
artwork described above; a Modern Warfare 2 installation is not required. The
explicit branding adaptation above is the only retail-mark-based executable art.
