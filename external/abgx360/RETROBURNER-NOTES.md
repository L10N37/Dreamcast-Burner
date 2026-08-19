# Retro Burner ABGX360 build notes

Source snapshot:

https://github.com/BakasuraRCE/abgx360
commit c38475cc23f077ecfab72c1f881f50d82f28d50e

Retro Burner builds the Windows command-line abgx360.exe and invokes it as a
separate process. The ABGX360 source is not linked into Retro Burner.

For XGD3, Retro Burner runs AutoFix Level 3 against a temporary ISO copy:

  abgx360 -s --af3 -- <temporary-working-copy.iso>

and then performs a writes-disabled verification pass:

  abgx360 -s -w --af3 -- <temporary-working-copy.iso>

The selected original ISO is not modified.

Rebuild with:
  .\scripts\build-abgx360.ps1