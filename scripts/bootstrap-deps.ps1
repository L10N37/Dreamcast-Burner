param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$externalRoot = Join-Path $projectRoot "external"
$imguiRoot = Join-Path $externalRoot "imgui"
$imguiTag = "v1.91.9b"

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw "Git was not found on PATH. Install Git for Windows and run this script again."
}

if (Test-Path (Join-Path $imguiRoot "imgui.cpp")) {
    if (-not $Force) {
        Write-Host "Dear ImGui is already available at external/imgui."
        exit 0
    }

    $expectedRoot = [System.IO.Path]::GetFullPath((Join-Path $projectRoot "external\imgui"))
    $resolvedRoot = [System.IO.Path]::GetFullPath($imguiRoot)
    if ($resolvedRoot -ne $expectedRoot) {
        throw "Refusing to replace an unexpected dependency directory."
    }
    Remove-Item -LiteralPath $imguiRoot -Recurse -Force
}

New-Item -ItemType Directory -Path $externalRoot -Force | Out-Null
Write-Host "Downloading Dear ImGui $imguiTag..."
& git clone --depth 1 --branch $imguiTag https://github.com/ocornut/imgui.git $imguiRoot
if ($LASTEXITCODE -ne 0) {
    throw "Dear ImGui download failed."
}

Write-Host "Dependency setup complete."

