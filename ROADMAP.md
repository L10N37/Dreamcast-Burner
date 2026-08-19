# Retro Burner Roadmap

This file describes intended work only. Items are not considered supported until they are implemented, documented and physically validated where practical.

## 0.4.x validation / hardening

- Complete XGD3 testing on known-compatible Lite-On iHAS and cross-flash-compatible hardware with permanent C4EVA firmware where appropriate.
- Keep the existing native BurnerMAX path experimental until repeatable real burns prove it useful.
- Explore safe non-MTK/vendor-specific BurnerMAX probing on the ASUS SDRW-08U9M-U without assuming compatibility from model name alone.
- Complete PS2 DVD9, PS2 CD and Sega Saturn physical validation.
- Add a startup GitHub Releases update check and unobtrusive new-version pop-up, with a user-disable option.
- Add an advanced 32 KiB / 64 KiB recording-transfer selector and log the effective transfer size for every burn.
- Continue collecting structured drive/media/backend reports.

## More console workflows

Priority additions:

1. **Sega/Mega-CD** — common BIN/CUE-style workflows can reuse much of the current CD pipeline after layout validation.
2. **Original Xbox** — DVD workflow; research required for image preparation/layout and real-hardware validation.
3. **Neo Geo CD**.
4. **PC Engine CD / TurboGrafx-CD**.
5. **3DO**.
6. **NEC PC-FX**.
7. **Amiga CD32 / CDTV**.
8. **Philips CD-i**.
9. **Atari Jaguar CD**.
10. **FM Towns Marty**.

A console does not become “supported” just because its media is a CD. Mixed-mode layouts, pregaps, subchannel requirements, filesystem expectations and boot behaviour must be checked individually.

## Image preparation

- **CHD:** support `.chd` input by converting to a temporary recording layout via `chdman` or an equivalent properly licensed implementation; never modify the user's source image.
- **PS2 ESR:** integrate only from a suitable open-source implementation after licence/provenance review; retain upstream credits and notices.
- **PS2 FreeDVDBoot:** same rule—use an appropriately licensed implementation/data path and clearly distinguish preparation from the disc-recording engine.

## Platforms

- **Linux:** planned first non-Windows port.
- **macOS:** possible after Linux proves the cross-platform architecture and optical-device access path.
- **Windows 32-bit:** possible “retro PC” build if dependencies and toolchains remain maintainable.

## Project goal

Retro Burner should reduce the need to remember which legacy program handles which console, while still respecting the reality that optical recording is hardware- and media-dependent. Feature requests and good burn/coaster reports are part of that goal.
