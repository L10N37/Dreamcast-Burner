# Third-Party Software and Assets

Retro Burner original code is MIT licensed. **That does not relicense any third-party code, executable or asset.** This inventory must match the final release package exactly.

## Dear ImGui

- Purpose: GUI framework/backends.
- Upstream: `ocornut/imgui`.
- Licence: MIT.
- Release requirement: retain the MIT copyright/licence notice.
- Credit: Omar Cornut and contributors.

## CDIrip

- Purpose: Dreamcast DiscJuggler CDI extraction.
- Upstream lineage: CDIrip by DeXT / Lawrence Williams; maintained source snapshot carries GPL-2.0 terms.
- Licence: GPL-2.0.
- Vendored source: `external/cdirip/`.
- Release requirement: retain GPL-2.0 text and make corresponding source for the shipped modified executable available. Modified source files should carry a prominent Retro Burner modification/date notice while retaining upstream notices.

## RetroBeam / SchilyTools cdrtools components

- Purpose: main optical recording engine and Windows SPTI transport base.
- Pinned source: SchilyTools tag `2021-09-18`, commit `90e8f68220698ce0dc132a9f7e7e25f0b9382f64`.
- Applicable linked components in the current bridge include `cdrecord`, `libscg`, `libscgcmd`, `librscg`, `libschily`, `libedc`, `libcdrdeflt` and `libdeflt`.
- Licence for those components in the vendored SchilyTools `COPYING` inventory: CDDL.
- Vendored source: `external/schilytools/`.
- Release requirement: preserve applicable CDDL headers/text and corresponding source/modifications. Do not describe the RetroBeam-linked subset as GPL-3 merely because other programs in the broader SchilyTools distribution use different licences.

## dvd+rw-tools / growisofs / dvd+rw-mediainfo

- Purpose: DVD media interrogation plus optional DVD recording backend.
- Windows source snapshot: `external/dvd-rw-tools-windows/`.
- Licence: GPL-2.0 in the vendored/upstream Windows port.
- Release requirement: retain GPL-2.0 notice and corresponding source for the executable(s) actually distributed.
- Credits: Andy Polyakov/dvd+rw-tools contributors and Windows-port contributors.

## ABGX360

- Purpose: Xbox 360 ISO verification and preparation used by the XGD3 workflow.
- Integration: Retro Burner invokes `abgx360.exe` as a separate helper process; ABGX360 source is not linked into the Retro Burner executable.
- Vendored source snapshot: `external/abgx360/`, from the BakasuraRCE community repository at commit `c38475cc23f077ecfab72c1f881f50d82f28d50e`.
- Helper: `tools/abgx360.exe`, rebuilt from the retained source snapshot.
- Credits: Seacrest for the original ABGX360 work, plus Hadzz, BakasuraRCE and later community contributors who kept the tool/database ecosystem usable.
- Release requirement: retain the corresponding source/provenance and all applicable third-party notices for the helper and its bundled dependencies.

## Native BurnerMAX interoperability code

- Purpose: capability/signature/capacity probing and payload interoperability in Retro Burner-owned Windows code.
- The original C4E `BurnerMax.exe` is not distributed.
- Credits/provenance: C4EVA, Team Jungle and Team Xecuter historical BurnerMAX work/research.
- Release requirement: keep `licenses/BurnerMAX-NOTICE.txt` accurate and avoid implying endorsement or authorship by those projects.

## Mixkit completion sound

- Purpose: success/completion sound.
- Current notice: `licenses/Mixkit-Sound-Effects-NOTICE.txt`.
- Release requirement: retain the source/licence provenance used when the asset was downloaded. If there is any doubt about application redistribution terms, replace it with a project-owned/original sound before release rather than carrying ambiguity.

## Project artwork and screenshots

Before release, verify that every asset under `assets/` and `Images/` is project-created or has documented permission/licensing. Console/platform names and logos may also be trademarks; credits do not imply endorsement.

## Release audit checklist

- [ ] Final helper list matches `CMakeLists.txt` / `RetroBurner.rc` resources.
- [ ] Every helper has a verified upstream licence or explicit redistribution permission.
- [ ] Required licence/copyright notices are embedded/packaged and readable.
- [ ] GPL/CDDL corresponding source/modifications for shipped helpers are present or otherwise supplied as required.
- [ ] CDIrip modified files have prominent change/date notices.
- [ ] RetroBeam-linked Schily components are described using their actual applicable CDDL terms.
- [ ] Asset/sound provenance is verified.
- [ ] Credits list upstream authors/contributors without implying endorsement.

This review is a practical release-engineering audit, not legal advice.
