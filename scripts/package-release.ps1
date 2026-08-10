$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = Join-Path $projectRoot "build\msvc-x64"
$distRoot = Join-Path $projectRoot "dist"
$packageRoot = Join-Path $distRoot "DreamcastBurner"
$zipPath = Join-Path $distRoot "DreamcastBurner-win64.zip"

& (Join-Path $PSScriptRoot "build.ps1") -Configuration Release

if (Test-Path $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

Push-Location $projectRoot
try {
    & cmake --install $buildRoot --config Release --prefix $packageRoot
    if ($LASTEXITCODE -ne 0) {
        throw "CMake install step failed."
    }
}
finally {
    Pop-Location
}

if (Test-Path $zipPath) {
    Remove-Item -LiteralPath $zipPath -Force
}
Compress-Archive -Path (Join-Path $packageRoot "*") -DestinationPath $zipPath

Write-Host ""
Write-Host "Packaged: $zipPath"
