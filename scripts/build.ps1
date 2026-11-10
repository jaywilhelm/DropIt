<#
.SYNOPSIS
    Configures and builds DropIt (VST3 + Standalone) on Windows.

.PARAMETER Config
    Debug or Release (default Release).

.PARAMETER Install
    Copy DropIt.vst3 into a VST3 folder after building. Uses
    C:\Program Files\Common Files\VST3 if writable (run as Administrator),
    otherwise the per-user folder %LOCALAPPDATA%\Programs\Common\VST3.

.PARAMETER Clean
    Delete the build folder first.

.PARAMETER Generator
    Optional CMake generator, e.g. "Visual Studio 17 2022" or "Ninja".

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Config Debug -Clean
    powershell -ExecutionPolicy Bypass -File scripts\build.ps1 -Install
#>
param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",
    [switch]$Install,
    [switch]$Clean,
    [string]$Generator = ""
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build"

if ($Clean -and (Test-Path $BuildDir)) {
    Write-Host "Removing $BuildDir" -ForegroundColor Yellow
    Remove-Item -Recurse -Force $BuildDir
}

if (-not (Test-Path (Join-Path $Root "external\JUCE\CMakeLists.txt"))) {
    Write-Host "external\JUCE not found - CMake will download JUCE automatically (or run scripts\setup.ps1 first)." -ForegroundColor DarkYellow
}

# --- Configure ---------------------------------------------------------------
$configureArgs = @("-S", $Root, "-B", $BuildDir)
if ($Generator) {
    $configureArgs += @("-G", $Generator)
    if ($Generator -like "Visual Studio*") { $configureArgs += @("-A", "x64") }
    if ($Generator -like "Ninja*") { $configureArgs += "-DCMAKE_BUILD_TYPE=$Config" }
} elseif (-not (Test-Path (Join-Path $BuildDir "CMakeCache.txt"))) {
    # Default: newest Visual Studio CMake knows about, 64-bit.
    $configureArgs += @("-A", "x64")
}

Write-Host "`n> cmake $($configureArgs -join ' ')`n" -ForegroundColor Cyan
cmake @configureArgs
if ($LASTEXITCODE -ne 0) { Write-Host "CMake configure failed." -ForegroundColor Red; exit 1 }

# --- Build -------------------------------------------------------------------
Write-Host "`n> cmake --build build --config $Config --parallel`n" -ForegroundColor Cyan
cmake --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) { Write-Host "Build failed." -ForegroundColor Red; exit 1 }

$artefacts = Join-Path $BuildDir "DropIt_artefacts\$Config"
$vst3 = Join-Path $artefacts "VST3\DropIt.vst3"
$exe  = Join-Path $artefacts "Standalone\DropIt.exe"

Write-Host "`nBuild succeeded:" -ForegroundColor Green
Write-Host "  VST3       : $vst3"
Write-Host "  Standalone : $exe"

# --- Install -----------------------------------------------------------------
if ($Install) {
    $systemDir = Join-Path $env:CommonProgramFiles "VST3"
    $userDir   = Join-Path $env:LOCALAPPDATA "Programs\Common\VST3"
    $dest = $null

    try {
        New-Item -ItemType Directory -Force $systemDir | Out-Null
        $probe = Join-Path $systemDir ".dropit_write_test"
        Set-Content -Path $probe -Value "x" -ErrorAction Stop
        Remove-Item $probe -Force
        $dest = $systemDir
    } catch {
        Write-Host "No write access to $systemDir (run as Administrator to install system-wide)." -ForegroundColor DarkYellow
        New-Item -ItemType Directory -Force $userDir | Out-Null
        $dest = $userDir
    }

    $target = Join-Path $dest "DropIt.vst3"
    if (Test-Path $target) { Remove-Item -Recurse -Force $target }
    Copy-Item -Recurse $vst3 $target
    Write-Host "`nInstalled to $target" -ForegroundColor Green
    Write-Host "Rescan plugins in your DAW to pick it up."
}
