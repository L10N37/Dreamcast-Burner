![Retro Burner](Images/dreamcastburner.png)

# Retro Burner

Retro Burner is a native Windows optical-disc burning frontend for classic game consoles. It combines console-aware image validation, optical-drive/media detection and several proven open-source command-line backends behind one Windows GUI.

The Release build is designed to ship as a **single `RetroBurner.exe`**. Helper executables and required licence notices are embedded as Windows resources and extracted to a private temporary directory only while Retro Burner is running.

**Current development version: 0.4.0**

## Supported console profiles

| Console | Image format | Recording path | Current status |
| --- | --- | --- | --- |
| Dreamcast | CDI | CDIrip + cdrecord | Physically tested |
| PlayStation | BIN/CUE | cdrecord CUE/SAO | Physically tested |
| PlayStation 2 CD | BIN/CUE or ISO | cdrecord | Implemented |
| PlayStation 2 DVD | ISO, DVD5/DVD9 | growisofs | DVD5 physically tested; DVD9 implemented |
| Sega Saturn | BIN/CUE | cdrecord CUE/SAO | Implemented |
| Xbox 360 XGD2 | ISO to DVD+R DL | growisofs | Implemented |
| Xbox 360 XGD3 | ISO to DVD+R DL | ABGX360 + native BurnerMAX + growisofs | BurnerMAX hardware path physically tested; full XGD3 burn pending |

Retro Burner is intended for images and backups that you are legally entitled to use.

## Highlights

- Native C++20 Windows application using Dear ImGui and DirectX 11.
- One console/profile selector instead of separate burning applications.
- Automatic optical writer, firmware and inserted-media detection.
- Real write-speed choices reported by the drive/media where available.
- Read-only media preflight before writing.
- Live percentage, speed, remaining time and buffer reporting.
- Static disc artwork while idle; animated disc during writes.
- Automatic disc eject after a successful burn.
- Success sound after a completed burn.
- User-facing log filters known harmless Windows/cdrecord privilege chatter while preserving meaningful errors.
- Single-EXE packaging with embedded helper tools.

## Console workflows

### Dreamcast CDI

Dreamcast DiscJuggler images are extracted with CDIrip and recorded with cdrecord. Retro Burner supports the tested self-boot layouts used by this project, including Data+Data and Audio+Data multi-session images.

Important implementation detail: Retro Burner asks CDIrip for ISO conversion but does not use CDIrip's `-cdrecord` preset, because that preset also enables track cutting that can damage audio tracks.

### PlayStation, PlayStation 2 CD and Sega Saturn

BIN/CUE images use cdrecord's native CUE/SAO path. PlayStation BIN/CUE burning has been physically tested with a real disc.

### PlayStation 2 DVD

PS2 DVD ISO images use the Windows port of `growisofs` together with `dvd+rw-mediainfo` for media interrogation.

- DVD5 images can use supported single-layer recordable DVD media.
- Larger DVD9 images require compatible dual-layer recordable media.
- DVD write progress includes percentage, current speed, remaining time, read buffer and drive buffer.

A PS2 DVD-R produced by Retro Burner has been tested successfully on real PlayStation 2 hardware.

### Xbox 360 XGD2

XGD2 images are written to DVD+R DL using growisofs with the standard XGD2 layer break:

```text
1913760
```

### Xbox 360 XGD3

XGD3 requires both a correctly prepared image and the expanded writable capacity normally associated with BurnerMAX-capable hardware/firmware.

Retro Burner performs the full prerequisite chain before allowing the real write:

```text
Selected XGD3 ISO
      |
      v
Create temporary working copy beside the source ISO
      |
      v
ABGX360 AutoFix Level 3 (--af3)
      |
      v
ABGX360 read-only verification pass
      |
      v
Blank DVD+R DL preflight
      |
      v
Native BurnerMAX test / payload injection
      |
      v
Verify expanded writable capacity
      |
      v
growisofs with XGD3 layer break 2133520
      |
      v
Eject + success sound
```

The **original Xbox 360 ISO is never modified**. ABGX360 operates on a temporary copy and Retro Burner deletes that temporary working directory when the job ends.

ABGX360 may require network access for verification data. If AutoFix cannot complete, topology data remains invalid/missing, or BurnerMAX capacity verification fails, Retro Burner stops before the XGD3 disc write.

The XGD3 layer break used by Retro Burner is:

```text
2133520
```

## Native BurnerMAX

Retro Burner contains its own native BurnerMAX compatibility module. The original C4E `BurnerMax.exe` is **not bundled, embedded or executed**.

The module uses Windows SCSI pass-through to probe compatible MediaTek-style vendor command paths, locate the expected DVD+R DL capacity/layer-capacity register signatures, apply the payload only after both signatures are found, then verify the expanded capacity before enabling an XGD3 write.

The payload is volatile. Retro Burner therefore repeats the capacity/payload preflight for each newly inserted blank DVD+R DL rather than permanently trusting a drive model.

### Hardware verified during development

**Supported and physically verified:**

```text
HL-DT-ST DVDRAM GP60NB50
Firmware PE00
Connection: USB
Native BurnerMAX path: F1
```

![BurnerMAX supported on HL-DT-ST GP60NB50](Images/burnerMaxSupported.png)

**Safely rejected as unsupported in the tested firmware:**

```text
ASUS SDRW-08U9M-U
Firmware B201
Connection: USB
```

![BurnerMAX unsupported on ASUS SDRW-08U9M-U](Images/burnerMaxNotSupported.png)

A drive appearing in this section is not a permanent whitelist. Retro Burner still performs the live capacity and payload checks each time.

## Single-EXE architecture

The Release executable embeds the helper programs required for the selected workflow, currently including:

- CDIrip
- cdrecord
- Cygwin runtime required by the bundled cdrecord build
- growisofs
- dvd+rw-mediainfo
- ABGX360 CLI

At runtime these are extracted under a private directory similar to:

```text
%TEMP%\RetroBurner\<process-id>\
```

They are launched without visible console windows and removed on normal application exit.

Embedding a helper does not change its licence. See `THIRD_PARTY.md`, `licenses/` and the vendored corresponding source under `external/`.

## Open-source recording components

Retro Burner coordinates existing open-source tools rather than pretending that every recording engine was written by this project.

- **Dear ImGui** - GUI framework, MIT.
- **CDIrip** - DiscJuggler CDI extraction, GPL-2.0.
- **cdrecord / cdrtools** - CD recording backend, under the licence texts supplied with the bundled build.
- **Cygwin runtime** - runtime used by the bundled Windows cdrecord build.
- **dvd+rw-tools** - `growisofs` and `dvd+rw-mediainfo`, GPL-2.0. Retro Burner uses a Windows x64 build with small handle-width fixes documented in the vendored source tree.
- **ABGX360** - Xbox 360 verification/AutoFix CLI, GPL-2.0, invoked as a separate extracted process for XGD3 preparation.
- **Mixkit sound effect** - completion sound, used under the Mixkit Sound Effects Free License.

The original Retro Burner code remains under the project's MIT licence. Third-party components retain their own terms.

## Building from source

### Requirements

- Windows 10 or Windows 11
- Visual Studio 2022 with Desktop development with C++
- CMake 3.24 or newer
- PowerShell
- Git
- MSYS2 for rebuilding the bundled DVD and ABGX360 CLI tools

Retro Burner uses C++20.

### Main application

If Dear ImGui has not been bootstrapped:

```powershell
.\scripts\bootstrap-deps.ps1
```

Build Release:

```powershell
.\build-release.bat
```

### Rebuilding ABGX360

The corresponding ABGX360 source used for the embedded CLI is kept under:

```text
external/abgx360/
```

Rebuild it with:

```powershell
.\scripts\build-abgx360.ps1
```

The resulting executable is copied to:

```text
tools/abgx360.exe
```

### Rebuilding growisofs / dvd+rw-mediainfo

The corresponding patched Windows source is kept under:

```text
external/dvd-rw-tools-windows/
```

Rebuild the static helpers with:

```powershell
.\scripts\build-dvd-rw-tools.ps1
```

## Project layout

```text
RetroBurner/
|-- Images/                         README/release screenshots
|-- assets/                         embedded console art and sound
|-- src/                            Retro Burner C++ source
|-- external/
|   |-- imgui/
|   |-- cdirip/
|   |-- abgx360/                    corresponding ABGX360 source
|   `-- dvd-rw-tools-windows/       corresponding patched DVD-tool source
|-- tools/                          helper binaries embedded at build time
|-- licenses/                       third-party licence texts/notices
|-- scripts/                        build/bootstrap helpers
|-- CMakeLists.txt
|-- README.md
|-- THIRD_PARTY.md
|-- CHANGELOG.md
`-- LICENSE
```

## Troubleshooting

### XGD3 says BurnerMAX is unsupported

That result means Retro Burner could not safely use the expected vendor-command path on the selected drive/firmware, or the required expanded capacity could not be verified. Do not force the write. Try a known-compatible burner/firmware.

XGD2 does not require the expanded XGD3 capacity.

### XGD3 stops during ABGX360

Open the burn log. Retro Burner deliberately blocks the write if ABGX360 AutoFix fails or if the read-only follow-up does not report an XGD3 topology-data check.

The original ISO is unaffected because AutoFix runs on a temporary working copy.

### A CD burn shows harmless Windows privilege warnings in raw cdrecord output

The Cygwin cdrecord build can emit Unix-style privilege warnings on Windows even when direct optical access works correctly. Retro Burner's normal log filters the known harmless variants while leaving real drive/media failures visible.

### Burn reaches 100% and then fails

100% only means track/image transfer reached the end. Optical fixation/finalization can still fail. Retro Burner trusts the final backend exit result before announcing success.

## Licensing

Original Retro Burner source code is released under the **MIT License**; see `LICENSE`.

That MIT grant applies only to original Retro Burner code. It does not relicense third-party programs or assets bundled with the application. Full details and corresponding-source locations are in:

```text
THIRD_PARTY.md
licenses/
external/
```

## Disclaimer

Retro Burner is an independent community project. It is not affiliated with, endorsed by, sponsored by, or produced by Sega, Sony, Microsoft or the publishers/developers of supported games.

Console names and trademarks belong to their respective owners. No BIOS files, console firmware, game data or copyrighted game images are included.

Use Retro Burner only with software and disc images that you have the legal right to use. Online-service policies and local law remain the user's responsibility.

## Credits

Thanks to the authors and contributors behind Dear ImGui, CDIrip, cdrtools/cdrecord, Cygwin, dvd+rw-tools, ABGX360 and the original BurnerMAX research/tooling that made interoperable XGD3 capacity work possible.
