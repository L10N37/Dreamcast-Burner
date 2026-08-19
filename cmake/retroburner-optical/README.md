# RetroBurner Optical Engine - Modern Build

This directory is owned by RetroBurner.

Milestone A:
- CMake compiles the current cdrecord source set directly.
- Schilling RULES, SMakefile, smake, configure and makedepend are not invoked.
- Existing proven i686 support archives are temporarily linked as a bridge.

Milestone B:
- Build libscg, libscgcmd, librscg, libschily, libdeflt, libcdrdeflt and libedc_ecc with CMake.
- Replace the generated Schilling xconfig/rules dependency with RetroBurner feature checks.
- Add native Linux and macOS transport targets.

The cdrtools authorship/license remains preserved; RetroBurner owns this build and integration layer.
