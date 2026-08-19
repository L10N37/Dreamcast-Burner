# Retro Burner 0.4.0 — Release Notes Draft

**Status:** release candidate / not yet pushed or released  
**Target version:** 0.4.0  
**Public predecessor:** 0.3.0

0.4.0 is the first release that turns the original Dreamcast-focused burner into the broader **Retro Burner** application. It is intentionally a large version jump in functionality rather than a small point update.

## Headline changes

- Retro Burner rebrand/generalisation and console-profile UI.
- Dreamcast CDI, PlayStation BIN/CUE, PS2 CD/DVD, Sega Saturn, Xbox 360 XGD2 and experimental XGD3 workflows.
- RetroBeam native Windows optical-recording backend based on the pinned SchilyTools/cdrecord/libscg source, with Retro Burner SPTI/FIFO/diagnostic integration.
- Optional growisofs backend retained for supported DVD workflows.
- DVD media interrogation, drive/media write-speed selection, Advanced Settings, auto-eject and completion sound.
- New graphical burn monitoring with overall progress plus live **Buffer** and **Device Buffer** bars.
- Unified backend reporting: RetroBeam FIFO/read and drive-buffer data, and growisofs `RBU`/`UBU`, are presented through the same graphical monitoring UI; growisofs also reports parsed speed, remaining time and burn-phase status.
- Native BurnerMAX interoperability/probing work and XGD3 preparation/preflight workflow.
- Expanded third-party source, licence and provenance documentation.

## XGD3 test status — important

XGD3 must be described as **experimental and end-to-end untested** in 0.4.0 unless a successful real burn is completed before release.

The development GP60NB50/PE00 drive did successfully accept the BurnerMAX payload and expose the expected expanded-capacity state. The actual XGD3 burn did not proceed. The drive was then rendered unusable while manually experimenting with firmware during follow-up research, so it could no longer be used to finish the validation. This was separate from Retro Burner's normal recording path.

Compatible Lite-On iHAS and cross-flash-compatible burners intended for permanent C4EVA firmware are expected for continued testing.

## Backend wording

Do not describe RetroBeam, growisofs or any other backend as universally “better”. Development results are specific to a combination of writer, firmware, USB/SATA bridge, recordable-media MID/batch, selected speed and even the target console's optical pickup.

RetroBeam is the default; growisofs remains a deliberate optional choice for supported DVD profiles.

## Known release-validation gaps

- XGD3 complete burn + console verification.
- PS2 DVD9 physical burn/boot validation.
- Final PS2 CD and Sega Saturn physical regression burns.
- Final third-party licence/notice inventory must match the binaries and source snapshots included in the release package.

## Planned after 0.4.0

- Original Xbox and Sega/Mega-CD profiles.
- CHD conversion/input support for CD-based systems.
- PS2 ESR patching and FreeDVDBoot preparation from properly licensed open-source sources.
- Experimental non-MTK BurnerMAX testing.
- GitHub Releases new-version check/pop-up at startup.
- Advanced 32 KiB / 64 KiB transfer-size selection.
- Linux port, with possible macOS and 32-bit Windows retro-PC builds later.
- More CD-based systems after format research and physical validation.

## Release gate

Before a public push/release:

- prune build/diagnostic/firmware-research artefacts;
- correct the local Git remote from the old Dreamcast-Burner URL to `L10N37/Retro-Burner`;
- remove user-specific hard-coded source paths;
- fix stale DreamcastBurner package names;
- reconcile README/CHANGELOG wording with the actual selectable growisofs backend;
- finish the third-party licence/notice audit;
- build from a clean tree and run smoke tests;
- review `git status`, `git diff --check` and the final commit contents before pushing.
