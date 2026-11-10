<#
.SYNOPSIS
    Checks build prerequisites for DropIt on Windows and downloads JUCE.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1
    powershell -ExecutionPolicy Bypass -File scripts\setup.ps1 -JuceTag 8.0.9
#>
param(
    [string]$JuceTag = "8.0.9"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$ok = $true

function Write-Ok($msg)   { Write-Host "  [ OK ] $msg" -ForegroundColor Green }
function Write-Bad($msg)  { Write-Host "  [FAIL] $msg" -ForegroundColor Red }
function Write-Info($msg) { Write-Host "         $msg" -ForegroundColor DarkGray }

Write-Host "`nDropIt setup - checking prerequisites`n" -ForegroundColor Cyan

# --- Git -------------------------------------------------------------------
$git = Get-Command git -ErrorAction SilentlyContinue
if ($git) { Write-Ok ("git: " + (git --version)) }
else {
    $ok = $false
    Write-Bad "git not found"
    Write-Info "Install: winget install --id Git.Git -e"
}

# --- CMake -----------------------------------------------------------------
$cmake = Get-Command cmake -ErrorAction SilentlyContinue
if ($cmake) {
    $verLine = (cmake --version | Select-Object -First 1)
    $ver = [version](($verLine -replace '[^0-9.]', '').Trim('.'))
    if ($ver -ge [version]"3.22") { Write-Ok "cmake: $ver" }
    else {
        $ok = $false
        Write-Bad "cmake $ver is too old (need 3.22+)"
        Write-Info "Install: winget install --id Kitware.CMake -e"
    }
} else {
    $ok = $false
    Write-Bad "cmake not found"
    Write-Info "Install: winget install --id Kitware.CMake -e   (then restart the terminal)"
}

# --- Visual Studio C++ toolchain ------------------------------------------
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$vsPath = $null
if (Test-Path $vswhere) {
    $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
}
if ($vsPath) {
    $vsName = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property displayName
    Write-Ok "C++ toolchain: $vsName"
} else {
    $ok = $false
    Write-Bad "Visual Studio with the 'Desktop development with C++' workload was not found"
    Write-Info "Install Visual Studio 2022 Community or Build Tools and select 'Desktop development with C++':"
    Write-Info "  winget install --id Microsoft.VisualStudio.2022.BuildTools -e --override `"--add Microsoft.VisualStudio.Workload.VCTools --includeRecommended --passive`""
}

if (-not $ok) {
    Write-Host "`nInstall the missing tools above, open a NEW terminal, and run setup again.`n" -ForegroundColor Yellow
    exit 1
}

# --- JUCE ------------------------------------------------------------------
$juceDir = Join-Path $Root "external\JUCE"
if (Test-Path (Join-Path $juceDir "CMakeLists.txt")) {
    Write-Ok "JUCE already present at external\JUCE"
    Write-Info "(delete that folder and re-run setup to change version)"
} else {
    Write-Host "`nDownloading JUCE $JuceTag into external\JUCE ..." -ForegroundColor Cyan
    New-Item -ItemType Directory -Force (Join-Path $Root "external") | Out-Null
    git clone --depth 1 --branch $JuceTag https://github.com/juce-framework/JUCE.git $juceDir
    if ($LASTEXITCODE -ne 0) { Write-Bad "git clone failed"; exit 1 }
    Write-Ok "JUCE $JuceTag downloaded"
}

Write-Host "`nSetup complete. Next: powershell -ExecutionPolicy Bypass -File scripts\build.ps1`n" -ForegroundColor Green
