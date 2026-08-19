param()

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path $root 'external\dvd-rw-tools-windows'
$tools = Join-Path $root 'tools'
$bash = 'C:\msys64\usr\bin\bash.exe'
$gcc = 'C:\msys64\ucrt64\bin\gcc.exe'
$gxx = 'C:\msys64\ucrt64\bin\g++.exe'

foreach ($path in @($source, $bash, $gcc, $gxx)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Required dvd+rw-tools build dependency is missing: $path"
    }
}

$env:CHERE_INVOKING = 'yes'
$env:MSYSTEM = 'UCRT64'

Write-Host "Building dvd+rw-tools from: $source"

Push-Location -LiteralPath $source
try {
    & $bash -lc 'rm -f growisofs.o growisofs_mmc.o growisofs-static.exe dvd+rw-mediainfo-static.exe'
    if ($LASTEXITCODE -ne 0) {
        throw "dvd+rw-tools cleanup failed (exit $LASTEXITCODE)."
    }

    & $bash -lc 'gcc -O2 -c growisofs.c -o growisofs.o'
    if ($LASTEXITCODE -ne 0) {
        throw "growisofs.c compile failed (exit $LASTEXITCODE)."
    }

    & $bash -lc 'g++ -O2 -c growisofs_mmc.cpp -o growisofs_mmc.o'
    if ($LASTEXITCODE -ne 0) {
        throw "growisofs_mmc.cpp compile failed (exit $LASTEXITCODE)."
    }

    & $bash -lc 'g++ -O2 -s -static -static-libgcc -static-libstdc++ -o growisofs-static.exe growisofs.o growisofs_mmc.o'
    if ($LASTEXITCODE -ne 0) {
        throw "growisofs static link failed (exit $LASTEXITCODE)."
    }

    & $bash -lc 'g++ -O2 -s -static -static-libgcc -static-libstdc++ -o dvd+rw-mediainfo-static.exe dvd+rw-mediainfo.cpp'
    if ($LASTEXITCODE -ne 0) {
        throw "dvd+rw-mediainfo static build failed (exit $LASTEXITCODE)."
    }
}
finally {
    Pop-Location
}

$grow = Join-Path $source 'growisofs-static.exe'
$info = Join-Path $source 'dvd+rw-mediainfo-static.exe'

foreach ($path in @($grow, $info)) {
    if (-not (Test-Path -LiteralPath $path)) {
        throw "Expected dvd+rw-tools output is missing: $path"
    }
}

New-Item -ItemType Directory -Force -Path $tools | Out-Null
Copy-Item -LiteralPath $grow -Destination (Join-Path $tools 'growisofs.exe') -Force
Copy-Item -LiteralPath $info -Destination (Join-Path $tools 'dvd+rw-mediainfo.exe') -Force

Remove-Item -LiteralPath (Join-Path $source 'growisofs.o') -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath (Join-Path $source 'growisofs_mmc.o') -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $grow -Force -ErrorAction SilentlyContinue
Remove-Item -LiteralPath $info -Force -ErrorAction SilentlyContinue

Write-Host "Built growisofs.exe and dvd+rw-mediainfo.exe in $tools"
