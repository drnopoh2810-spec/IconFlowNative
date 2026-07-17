<#
.SYNOPSIS
    configure_windows.ps1 — First-time setup for IconFlow Native Picker.

.DESCRIPTION
    Validates the build environment, locates required tools, generates the
    PiPL .rr resource from the .r file, and optionally runs CMake configure.

    Prerequisites:
      - Visual Studio 2022 with "Desktop development with C++" workload
        (MSVC v143 compiler, Windows SDK)
      - AE SDK installed; AE_SDK_ROOT env var pointing to its root directory
        e.g. $env:AE_SDK_ROOT = "C:\Adobe\AfterEffectsSDK\2024"

    Usage:
        .\configure_windows.ps1
        .\configure_windows.ps1 -AeSdkRoot "D:\AE_SDK\2024"
        .\configure_windows.ps1 -UseCMake -BuildDir "build_cmake"
#>

param(
    [string]$AeSdkRoot   = $env:AE_SDK_ROOT,
    [switch]$UseCMake,
    [string]$BuildDir    = "build",
    [switch]$SkipTests
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Helpers ───────────────────────────────────────────────────────────────────
function Write-Step  { param([string]$msg) Write-Host "`n[STEP] $msg" -ForegroundColor Cyan }
function Write-Ok    { param([string]$msg) Write-Host "  [OK]  $msg" -ForegroundColor Green }
function Write-Warn  { param([string]$msg) Write-Host "  [WRN] $msg" -ForegroundColor Yellow }
function Write-Fail  { param([string]$msg)
    Write-Host "`n  [ERR] $msg" -ForegroundColor Red
    exit 1
}

# ── Banner ────────────────────────────────────────────────────────────────────
Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║      IconFlow Native Picker — Windows Build Configuration    ║
║      configure_windows.ps1                                   ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor White

# ── 1. Verify AE SDK root ─────────────────────────────────────────────────────
Write-Step "Checking AE SDK root"

if (-not $AeSdkRoot) {
    Write-Fail @"
AE_SDK_ROOT is not set.

  Option A: Set the environment variable permanently:
    [System.Environment]::SetEnvironmentVariable('AE_SDK_ROOT', 'C:\Adobe\AfterEffectsSDK\2024', 'User')

  Option B: Pass it to this script:
    .\configure_windows.ps1 -AeSdkRoot "C:\Adobe\AfterEffectsSDK\2024"

  Download the AE SDK from:
    https://developer.adobe.com/after-effects/
"@
}

if (-not (Test-Path "$AeSdkRoot\Headers\AE_Effect.h")) {
    Write-Fail @"
AE SDK headers not found at '$AeSdkRoot\Headers\AE_Effect.h'.
Check that AE_SDK_ROOT points to the correct SDK directory.
Expected structure:
    $AeSdkRoot\
        Headers\
            AE_Effect.h
            AE_EffectCB.h
            ...
        Resources\
            PiPLtool.exe
        Examples\
            ...
"@
}

Write-Ok "AE SDK found at: $AeSdkRoot"

# ── 2. Check required SDK files ───────────────────────────────────────────────
Write-Step "Checking required SDK headers"

$requiredHeaders = @(
    "Headers\AE_Effect.h",
    "Headers\AE_EffectCB.h",
    "Headers\AE_EffectCBSuites.h",
    "Headers\AE_Macros.h",
    "Headers\AEGP_SuiteHandler.h",
    "Headers\Smart_Utils.h",
    "Headers\Param_Utils.h"
)

$missing = @()
foreach ($h in $requiredHeaders) {
    $path = "$AeSdkRoot\$h"
    if (Test-Path $path) {
        Write-Ok $h
    } else {
        Write-Warn "MISSING: $h"
        $missing += $h
    }
}

if ($missing.Count -gt 0) {
    Write-Warn "$($missing.Count) SDK header(s) missing. The build may fail."
    Write-Warn "Ensure the AE SDK is fully extracted."
}

# ── 3. Locate PiPLtool ────────────────────────────────────────────────────────
Write-Step "Locating PiPLtool.exe"

$piPlTool = "$AeSdkRoot\Resources\PiPLtool.exe"
if (-not (Test-Path $piPlTool)) {
    # Try alternate path used by some SDK versions.
    $piPlTool = "$AeSdkRoot\Resources\windows\PiPLtool.exe"
}

if (-not (Test-Path $piPlTool)) {
    Write-Warn "PiPLtool.exe not found at '$AeSdkRoot\Resources\PiPLtool.exe'."
    Write-Warn "The .rr file cannot be auto-generated. You must generate it manually."
    Write-Warn "If you have a pre-generated IconFlowNative.rr, copy it to src\IconFlowNative.rr."
} else {
    Write-Ok "PiPLtool found: $piPlTool"

    # ── 4. Generate PiPL .rr resource ─────────────────────────────────────────
    Write-Step "Generating PiPL resource (pipl\IconFlowNative.r → src\IconFlowNative.rr)"

    $rFile  = "$PSScriptRoot\pipl\IconFlowNative.r"
    $rrFile = "$PSScriptRoot\src\IconFlowNative.rr"

    if (-not (Test-Path $rFile)) {
        Write-Fail "PiPL source not found: $rFile"
    }

    # PiPLtool requires SDK headers on the include path.
    # It is a command-line tool: PiPLtool.exe <input.r> <output.rr> [include_dirs...]
    $piPLArgs = @(
        $rFile,
        $rrFile,
        "-I$AeSdkRoot\Headers",
        "-I$AeSdkRoot\Resources"
    )

    Write-Host "  Running: $piPlTool $($piPLArgs -join ' ')"
    & $piPlTool @piPLArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "PiPLtool failed (exit $LASTEXITCODE). Check the error messages above."
    }
    Write-Ok "Generated: src\IconFlowNative.rr"
}

# ── 5. Locate Visual Studio / MSVC ───────────────────────────────────────────
Write-Step "Locating Visual Studio 2022"

$vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
if (-not (Test-Path $vswhere)) {
    $vswhere = "${env:ProgramFiles}\Microsoft Visual Studio\Installer\vswhere.exe"
}

if (Test-Path $vswhere) {
    $vsInstall = & $vswhere -latest -version "[17.0,18.0)" -property installationPath 2>$null
    if ($vsInstall) {
        Write-Ok "Visual Studio 2022 found: $vsInstall"
        # Verify MSVC v143 toolset
        $msvcPath = Join-Path $vsInstall "VC\Tools\MSVC"
        if (Test-Path $msvcPath) {
            $msvcVersions = Get-ChildItem $msvcPath -Directory | Select-Object -Last 1
            Write-Ok "MSVC toolset: $($msvcVersions.Name)"
        }
    } else {
        Write-Warn "Visual Studio 2022 not found via vswhere."
        Write-Warn "Install VS2022 with 'Desktop development with C++' workload."
    }
} else {
    Write-Warn "vswhere.exe not found. Cannot auto-detect Visual Studio."
    Write-Warn "Ensure VS2022 is installed with MSVC v143."
}

# ── 6. Optional: CMake configure ──────────────────────────────────────────────
if ($UseCMake) {
    Write-Step "Running CMake configure"

    $cmake = Get-Command cmake -ErrorAction SilentlyContinue
    if (-not $cmake) {
        Write-Fail "cmake not found in PATH. Install CMake from https://cmake.org/ or via VS Installer."
    }
    Write-Ok "CMake: $($cmake.Source)"

    $buildPath = Join-Path $PSScriptRoot $BuildDir

    $cmakeArgs = @(
        "-S", $PSScriptRoot,
        "-B", $buildPath,
        "-G", "Visual Studio 17 2022",
        "-A", "x64",
        "-DAE_SDK_ROOT=$AeSdkRoot"
    )

    Write-Host "  Running: cmake $($cmakeArgs -join ' ')"
    & cmake @cmakeArgs
    if ($LASTEXITCODE -ne 0) {
        Write-Fail "CMake configure failed (exit $LASTEXITCODE)."
    }

    Write-Ok "CMake project generated at: $buildPath"
    Write-Host ""
    Write-Host "  Build with:  cmake --build $buildPath --config Release" -ForegroundColor White
    Write-Host "  Or open:     $buildPath\IconFlowNative.sln" -ForegroundColor White
}

# ── 7. Summary ────────────────────────────────────────────────────────────────
Write-Host @"

╔══════════════════════════════════════════════════════════════╗
║  Configuration complete.                                     ║
║                                                              ║
║  Next steps:                                                 ║
║    1. Open IconFlowNative.sln in Visual Studio 2022          ║
║       or run: .\build_release.ps1                            ║
║    2. Build → Release | x64                                  ║
║    3. Copy build\Release\IconFlowNative.aex to:              ║
║       %%PROGRAMFILES%%\Adobe\Adobe After Effects 2024\       ║
║           Support Files\Plug-ins\IconFlow\                   ║
║    4. Launch After Effects → Effect → IconFlow →             ║
║       IconFlow Native Picker                                 ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan
