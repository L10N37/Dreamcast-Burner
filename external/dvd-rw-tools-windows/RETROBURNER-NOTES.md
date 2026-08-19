# Retro Burner dvd+rw-tools Windows build notes

Upstream:
  https://github.com/vt-idiot/dvd-rw-tools-windows
  commit 4e8fa78f006a24884eecb3a148dc05f19c319a77

The corresponding source snapshot contains these x64 Windows handle-width
changes used by Retro Burner's static helper build:

growisofs.c
  ioctl_fd: long -> LONG_PTR

growisofs_mmc.cpp
  ioctl_fd: long -> LONG_PTR
  fumount(int) -> fumount(LONG_PTR)

transport.hxx
  Windows umount(int) -> umount(LONG_PTR)
  Non-Windows umount implementations are intentionally unchanged.

Rebuild with:
  .\scripts\build-dvd-rw-tools.ps1