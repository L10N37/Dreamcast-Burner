# Changelog

All notable project changes are recorded here.

## [0.3.0] - 2026-08-10

### Added

- Reliable mapping between Windows optical drives and the actual device
  addresses reported by `cdrecord -scanbus`, improving operation with
  multiple burners connected at the same time.
- Always-visible in-application burn log with copy-to-clipboard support.
- Explicit documentation for both Data+Data and Audio+Data Dreamcast
  self-boot CDI layouts.

### Changed

- Updated the bundled recording backend from
  `Cdrecord-ProDVD-ProBD-Clone 2.01.01a36` to
  `Cdrecord-ProDVD-ProBD-Clone 3.02a10 2021/07/23`.
- CDI extraction now requests ISO conversion for data tracks without using
  CDIrip's `-cdrecord` preset, preventing the preset's `-cutall` behaviour
  from trimming the audio tracks.
- Drive selection now relies on cdrecord's own reported bus/target/LUN mapping
  rather than predicting its numbering solely from Windows adapter data.

### Fixed

- Audio+Data CDI session-one recording on tested hardware after correcting
  extraction behaviour and updating the recording backend.
- Incorrect burner selection when more than one optical writer is attached.
- Burn log visibility regressions introduced during backend testing.

### Validated

- Real CD-R Data+Data self-boot Dreamcast burn.
- Real CD-R multi-track Audio+Data self-boot Dreamcast burn.
- CDIrip output with uncut audio tracks and ISO-converted Mode2 data track.
- cdrecord 3.02a10 operation with multiple optical writers.

### Next

The next development milestone will become **Retro Burner** and is planned to:

- embed/manage the required helper components behind one distributable EXE
- present a clean application-level log rather than normal raw backend chatter
- add Dreamcast, PlayStation, PlayStation 2 and Sega Saturn media profiles
- add CD, DVD and supported dual-layer DVD workflows

No binary release is attached to this source checkpoint.

## [0.2.0]

- Initial native Windows Dreamcast Burner implementation.
- DiscJuggler CDI workflow.
- Drive/media detection.
- CDIrip extraction.
- cdrecord session sequencing.
- Burn progress, write-speed display and burn logging.