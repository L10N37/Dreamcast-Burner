param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'external\abgx360\windows\abgx360'
$output = Join-Path $root 'tools\abgx360.exe'
$bash = 'C:\msys64\usr\bin\bash.exe'
$gcc = 'C:\msys64\mingw32\bin\gcc.exe'

if (-not (Test-Path -LiteralPath $source)) {
    throw "ABGX360 corresponding source is missing: $source"
}
if (-not (Test-Path -LiteralPath $bash)) {
    throw 'MSYS2 is required at C:\msys64 to rebuild ABGX360.'
}

if (-not (Test-Path -LiteralPath $gcc)) {
    Write-Host 'Installing the MSYS2 32-bit MinGW GCC toolchain required by the upstream ABGX360 Windows libraries...'
    & $bash -lc 'pacman -S --needed --noconfirm mingw-w64-i686-gcc make'
    if ($LASTEXITCODE -ne 0) {
        throw "MSYS2 could not install the ABGX360 build toolchain (exit $LASTEXITCODE)."
    }
}

$env:CHERE_INVOKING = 'yes'
$env:MSYSTEM = 'MINGW32'

Write-Host "Building ABGX360 CLI from: $source"

Push-Location -LiteralPath $source
try {
    & $bash -lc 'make clean && make'
    $buildExit = $LASTEXITCODE
}
finally {
    Pop-Location
}

if ($buildExit -ne 0) {
    throw "ABGX360 build failed (exit $buildExit)."
}

$built = Join-Path $source 'abgx360.exe'
if (-not (Test-Path -LiteralPath $built)) {
    throw 'ABGX360 build completed without producing abgx360.exe.'
}

New-Item -ItemType Directory -Force -Path (Split-Path -Parent $output) | Out-Null
Copy-Item -LiteralPath $built -Destination $output -Force
Write-Host "Built ABGX360 CLI: $output"

Push-Location -LiteralPath $source
try {
    & $bash -lc 'make clean'
    $cleanExit = $LASTEXITCODE
}
finally {
    Pop-Location
}
if ($cleanExit -ne 0) {
    Write-Warning 'ABGX360 built successfully, but make clean returned a non-zero exit code.'
}
