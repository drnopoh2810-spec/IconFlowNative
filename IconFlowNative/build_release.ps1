<#
.SYNOPSIS
    build_release.ps1 — Builds IconFlowNative.aex in Release x64.

.DESCRIPTION
    Invokes MSBuild (via Visual Studio 2022) to compile the plugin.
    Optionally runs unit tests and copies the output to After Effects.

    Usage:
        .\build_release.ps1
        .\build_release.ps1 -RunTests
        .\build_release.ps1 -InstallToAE -AePath "C:\Program Files\Adobe\Adobe After Effects 2024"
        .\build_release.ps1 -UseCMake

.NOTES
    Run configure_windows.ps1 first to validate your environment and
    generate the PiPL .rr resource.
#>

param(
    [string]$AeSdkRoot    = $env:AE_SDK_ROOT,
    [string]$Configuration = "Release",
    [string]$Platform      = "x64",
    [switch]$RunTests,
    [switch]$InstallToAE,
    [string]$AePath        = "",
    [switch]$UseCMake,
    [string]$BuildDir      = "build"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Helpers ───────────────────────────────────────────────────────────────────
function Write-Step { param([string]$msg) Write-Host "`n[STEP] $msg" -ForegroundColor Cyan }
function Write-Ok   { param([string]$msg) Write-Host "  [OK]  $msg" -ForegroundColor Green }
function Write-Warn { param([string]$msg) Write-Host "  [WRN] $msg" -ForegroundColor Yellow }
function Write-Fail { param([string]$msg)
    Write-Host "`n  [ERR] $msg" -ForegroundColor Red
    exit 1
}

$ScriptDir = $PSScriptRoot

# ── Banner ────────────────────────────────────────────────────────────────────
Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║  IconFlow Native Picker — Release Build ($Configuration | $Platform)  ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor White

# ── Validate AE SDK ───────────────────────────────────────────────────────────
if (-not $AeSdkRoot) {
    Write-Fail "AE_SDK_ROOT is not set. Run configure_windows.ps1 first."
}
if (-not (Test-Path "$AeSdkRoot\Headers\AE_Effect.h")) {
    Write-Fail "AE SDK not found at '$AeSdkRoot'. Check AE_SDK_ROOT."
}

# ── Ensure PiPL .rr is present ────────────────────────────────────────────────
Write-Step "Checking PiPL resource"

$rrFile = "$ScriptDir\src\IconFlowNative.rr"
if (-not (Test-Path $rrFile)) {
    Write-Warn "IconFlowNative.rr not found. Attempting to generate..."

    $piPlTool = "$AeSdkRoot\Resources\PiPLtool.exe"
    if (-not (Test-Path $piPlTool)) {
        $piPlTool = "$AeSdkRoot\Resources\windows\PiPLtool.exe"
    }
    if (-not (Test-Path $piPlTool)) {
        Write-Fail "PiPLtool.exe not found and IconFlowNative.rr is missing.`nRun configure_windows.ps1 first."
    }

    $rFile = "$ScriptDir\pipl\IconFlowNative.r"
    & $piPlTool $rFile $rrFile "-I$AeSdkRoot\Headers" "-I$AeSdkRoot\Resources"
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "PiPLtool failed. Cannot build without the PiPL resource."
    }
    Write-Ok "PiPL resource generated."
} else {
    Write-Ok "IconFlowNative.rr present."
}

# ── Choose build path (CMake or MSBuild direct) ───────────────────────────────
if ($UseCMake) {

    Write-Step "Building with CMake"
    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) { Write-Fail "cmake not in PATH." }

    $buildPath = Join-Path $ScriptDir $BuildDir

    if (-not (Test-Path "$buildPath\CMakeCache.txt")) {
        Write-Warn "CMake not configured. Running configure step..."
        & cmake -S $ScriptDir -B $buildPath `
            -G "Visual Studio 17 2022" -A x64 `
            -DAE_SDK_ROOT="$AeSdkRoot"
        if ($LASTEXITCODE -ne 0) { Write-Fail "CMake configure failed." }
    }

    & cmake --build $buildPath --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { Write-Fail "CMake build failed." }

    $outAex = "$buildPath\$Configuration\IconFlowNative.aex"

} else {

    # ── Locate MSBuild via vswhere ─────────────────────────────────────────────
    Write-Step "Locating MSBuild (Visual Studio 2022)"

    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        $vswhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
    }
    if (-not (Test-Path $vswhere)) {
        Write-Fail "vswhere.exe not found. Install Visual Studio 2022 first."
    }

    $vsInstall = & $vswhere -latest -version "[17.0,18.0)" -property installationPath 2>$null
    if (-not $vsInstall) {
        Write-Fail "Visual Studio 2022 not found. Install it with 'Desktop development with C++' workload."
    }

    $msbuild = Join-Path $vsInstall "MSBuild\Current\Bin\MSBuild.exe"
    if (-not (Test-Path $msbuild)) {
        Write-Fail "MSBuild.exe not found at '$msbuild'."
    }
    Write-Ok "MSBuild: $msbuild"

    # ── Run MSBuild ────────────────────────────────────────────────────────────
    Write-Step "Building IconFlowNative.vcxproj ($Configuration|$Platform)"

    $slnFile = "$ScriptDir\IconFlowNative.sln"
    $msbuildArgs = @(
        $slnFile,
        "/p:Configuration=$Configuration",
        "/p:Platform=$Platform",
        "/p:AE_SDK_ROOT=$AeSdkRoot",
        "/m",                            # multi-processor
        "/nologo",
        "/v:minimal"
    )

    Write-Host "  MSBuild $($msbuildArgs -join ' ')"
    & $msbuild @msbuildArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "MSBuild failed (exit $LASTEXITCODE). Check the errors above."
    }

    $outAex = "$ScriptDir\build\$Configuration\IconFlowNative.aex"
}

# ── Verify output ─────────────────────────────────────────────────────────────
Write-Step "Verifying output"

if (-not (Test-Path $outAex)) {
    Write-Fail "Output not found: $outAex`nBuild may have failed silently."
}

$aexItem = Get-Item $outAex
Write-Ok "Built: $outAex"
Write-Ok "Size:  $([Math]::Round($aexItem.Length / 1KB, 1)) KB"
Write-Ok "Date:  $($aexItem.LastWriteTime)"

# ── Run unit tests ────────────────────────────────────────────────────────────
if ($RunTests) {
    Write-Step "Running unit tests (ColorMathTests)"

    $testExe = if ($UseCMake) {
        "$ScriptDir\$BuildDir\$Configuration\ColorMathTests.exe"
    } else {
        "$ScriptDir\build\$Configuration\ColorMathTests.exe"
    }

    if (Test-Path $testExe) {
        & $testExe
        if ($LASTEXITCODE -ne 0) {
            Write-Fail "Unit tests failed (exit $LASTEXITCODE)."
        }
        Write-Ok "All unit tests passed."
    } else {
        Write-Warn "ColorMathTests.exe not found at '$testExe'. Build the tests project too."
    }
}

# ── Optional: install to After Effects ───────────────────────────────────────
if ($InstallToAE) {
    Write-Step "Installing IconFlowNative.aex to After Effects"

    # Auto-detect AE 2024 if path not specified.
    if (-not $AePath) {
        $candidates = @(
            "${env:ProgramFiles}\Adobe\Adobe After Effects 2024",
            "${env:ProgramFiles}\Adobe\Adobe After Effects 2023",
            "${env:ProgramFiles}\Adobe\Adobe After Effects 2022"
        )
        foreach ($c in $candidates) {
            if (Test-Path $c) { $AePath = $c; break }
        }
    }

    if (-not $AePath -or -not (Test-Path $AePath)) {
        Write-Fail "After Effects installation not found. Pass -AePath 'C:\Program Files\Adobe\Adobe After Effects 2024'."
    }

    $pluginsDir = "$AePath\Support Files\Plug-ins\IconFlow"
    if (-not (Test-Path $pluginsDir)) {
        New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
        Write-Ok "Created: $pluginsDir"
    }

    Copy-Item $outAex $pluginsDir -Force
    Write-Ok "Installed: $pluginsDir\IconFlowNative.aex"
    Write-Host ""
    Write-Host "  Restart After Effects and find the effect under:" -ForegroundColor White
    Write-Host "    Effect > IconFlow > IconFlow Native Picker" -ForegroundColor Yellow
}

# ── Done ──────────────────────────────────────────────────────────────────────
Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║  BUILD SUCCEEDED                                             ║
║                                                              ║
║  Output:  build\$Configuration\IconFlowNative.aex
║                                                              ║
║  Install the .aex to:                                        ║
║    %%PROGRAMFILES%%\Adobe\Adobe After Effects 2024\          ║
║        Support Files\Plug-ins\IconFlow\                      ║
║  or use: .\build_release.ps1 -InstallToAE -RunTests          ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Green
