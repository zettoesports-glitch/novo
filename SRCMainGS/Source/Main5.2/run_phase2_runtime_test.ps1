param(
    [switch]$EnableGLDebug,
    [switch]$RequireResize,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$main52Root = Split-Path -Parent $MyInvocation.MyCommand.Path
$repoRoot = (Resolve-Path (Join-Path $main52Root '..\..\..')).Path
$clientRoot = Join-Path $repoRoot 'Client_2'
$mainExe = Join-Path $clientRoot 'Main.exe'
$releaseBackend = Join-Path $clientRoot 'GraphicsEngineOpenGL_32r.dll'
$debugBackend = Join-Path $clientRoot 'GraphicsEngineOpenGL_32d.dll'
$logPath = Join-Path $clientRoot 'ModernGraphics.log'

function Assert-Exists([string]$Path, [string]$Label) {
    if (-not (Test-Path $Path)) {
        throw "Phase 2 runtime prerequisite missing: $Label ($Path)"
    }
    Write-Host "[Phase2 Runtime] Found ${Label}: $Path"
}

function Assert-LogContains([string]$Text, [string]$Pattern, [string]$Label) {
    if ($Text -notmatch [regex]::Escape($Pattern)) {
        throw "Phase 2 runtime check failed: $Label. Expected log entry: $Pattern"
    }
    Write-Host "[Phase2 Runtime] PASS: $Label"
}

# -ValidateOnly is intentionally a parser/evidence validation mode. It must be
# usable by CI with synthetic evidence without coupling the parser test to the
# presence or layout of runtime binaries. The normal runtime path below still
# requires Main.exe and both Diligent OpenGL backend DLLs before launching.
if (-not $ValidateOnly) {
    Assert-Exists $mainExe 'Main.exe'
    Assert-Exists $releaseBackend 'GraphicsEngineOpenGL_32r.dll'
    Assert-Exists $debugBackend 'GraphicsEngineOpenGL_32d.dll'

    if (Test-Path $logPath) {
        Remove-Item $logPath -Force
    }

    $previousDebugSetting = $env:MU_MODERN_GL_DEBUG
    try {
        if ($EnableGLDebug) {
            $env:MU_MODERN_GL_DEBUG = '1'
            Write-Host '[Phase2 Runtime] MU_MODERN_GL_DEBUG=1 enables Diligent validation and routes its OpenGL/KHR_debug messages to ModernGraphics.log.'
        }
        else {
            Remove-Item Env:MU_MODERN_GL_DEBUG -ErrorAction SilentlyContinue
        }

        Write-Host '[Phase2 Runtime] Starting Main.exe.'
        Write-Host '[Phase2 Runtime] Let the client reach a rendered scene, resize the window at least once if -RequireResize was used, then close the client normally.'
        $process = Start-Process -FilePath $mainExe -WorkingDirectory $clientRoot -PassThru -Wait
        Write-Host "[Phase2 Runtime] Main.exe exited with code $($process.ExitCode)."

        if ($process.ExitCode -ne 0) {
            throw "Phase 2 runtime check failed: Main.exe did not close normally (exit code $($process.ExitCode))."
        }
    }
    finally {
        if ($null -eq $previousDebugSetting) {
            Remove-Item Env:MU_MODERN_GL_DEBUG -ErrorAction SilentlyContinue
        }
        else {
            $env:MU_MODERN_GL_DEBUG = $previousDebugSetting
        }
    }
}
else {
    Write-Host '[Phase2 Runtime] ValidateOnly: validating existing/synthetic ModernGraphics.log evidence without launching Main.exe or requiring runtime binaries.'
}

Assert-Exists $logPath 'ModernGraphics.log'
$logText = Get-Content $logPath -Raw

Assert-LogContains $logText '[ModernGraphics] Phase 2 OpenGL 4.6 attach attempt started.' 'bootstrap attach attempt reached'
Assert-LogContains $logText '[ModernGraphics] OpenGL vendor=' 'OpenGL diagnostics captured'
Assert-LogContains $logText 'profile=compatibility' 'OpenGL compatibility profile confirmed for legacy coexistence'
Assert-LogContains $logText '[ModernGraphics] Diligent attached to the existing OpenGL 4.6 context; legacy SwapBuffers remains authoritative.' 'Diligent attached to the existing WGL context'
Assert-LogContains $logText '[ModernGraphics] Shutdown completed before legacy WGL teardown.' 'Diligent shutdown completed before WGL teardown'

if ($RequireResize) {
    Assert-LogContains $logText '[ModernGraphics] Resize observed:' 'resize lifecycle reached modern bootstrap'
}

if ($EnableGLDebug) {
    Assert-LogContains $logText '[ModernGraphics] Diligent validation/OpenGL debug routing enabled by MU_MODERN_GL_DEBUG.' 'Diligent validation/OpenGL debug routing enabled'
}

if ($logText -match 'Legacy renderer remains active') {
    throw 'Phase 2 runtime log contains a modern-backend fallback. Inspect ModernGraphics.log before continuing.'
}

Write-Host ''
if ($ValidateOnly) {
    Write-Host '[Phase2 Runtime] Evidence parser validation passed for the checks requested by this script.'
}
else {
    Write-Host '[Phase2 Runtime] GPU runtime gate passed for the checks requested by this script.'
}
Write-Host "[Phase2 Runtime] Evidence log: $logPath"
