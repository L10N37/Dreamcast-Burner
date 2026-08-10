# Third-Party Components

Dreamcast Burner contains original project code as well as third-party
components. The project MIT licence applies only to code original to
Dreamcast Burner.

## Dear ImGui

Used for the graphical user interface.

Source:
`external/imgui/`

Licence:
`licenses/DearImGui-LICENSE.txt` in packaged builds, or the upstream licence
stored with the source tree.

## CDIrip

Used to parse and extract DiscJuggler CDI images.

Source:
`external/cdirip/`

Licence:
`external/cdirip/LICENSE`
and packaged as `licenses/CDIrip-GPL-2.0.txt`.

## cdrecord / cdrtools

Used as the physical optical-disc recording backend.

Bundled backend for Dreamcast Burner 0.3.0:

`Cdrecord-ProDVD-ProBD-Clone 3.02a10 2021/07/23`

Relevant licence texts are stored under:

- `licenses/cdrecord-cddl.txt`
- `licenses/cdrecord-gpl3.txt`

## Cygwin Runtime

The currently bundled Windows cdrecord build uses `cygwin1.dll`.

Relevant licence information is stored under:

- `licenses/cygwin-GPL-2.0.txt`

## Redistribution

When redistributing the project or a binary package, retain the licence and
copyright information required by each third-party component.

Future single-EXE packaging does not remove third-party licence obligations;
the applicable notices must remain available to users even if helper binaries
are embedded as application resources.