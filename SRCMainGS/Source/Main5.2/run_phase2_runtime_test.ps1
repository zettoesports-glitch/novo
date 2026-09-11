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

function Get-LogIndex([string]$Text, [string]$Pattern, [string]$Label) {
    $index = $Text.IndexOf($Pattern, [System.StringComparison]::Ordinal)
    if ($index -lt 0) {
        throw "Phase 2 runtime check failed: $Label. Expected log entry: $Pattern"
    }
    return $index
}

function Assert-LogOccursOnce([string]$Text, [string]$Pattern, [string]$Label) {
    $count = [regex]::Matches($Text, [regex]::Escape($Pattern)).Count
    if ($count -ne 1) {
        throw "Phase 2 runtime check failed: $Label. Expected exactly one occurrence of '$Pattern', found $count."
    }
    Write-Host "[Phase2 Runtime] PASS: $Label"
}

function Assert-LogBefore(
    [int]$EarlierIndex,
    [string]$EarlierLabel,
    [int]$LaterIndex,
    [string]$LaterLabel
) {
    if ($EarlierIndex -ge $LaterIndex) {
        throw "Phase 2 runtime check failed: expected '$EarlierLabel' before '$LaterLabel'."
    }
    Write-Host "[Phase2 Runtime] PASS: ${EarlierLabel} precedes ${LaterLabel}"
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

$attachAttemptMarker = '[ModernGraphics] Phase 2 OpenGL 4.6 attach attempt started.'
$diagnosticsMarker = '[ModernGraphics] OpenGL vendor='
$attachedMarker = '[ModernGraphics] Diligent attached to the existing OpenGL 4.6 context; legacy SwapBuffers remains authoritative.'
$resizeMarker = '[ModernGraphics] Resize observed:'
$debugMarker = '[ModernGraphics] Diligent validation/OpenGL debug routing enabled by MU_MODERN_GL_DEBUG.'
$teardownMarker = '[ModernGraphics] Teardown barrier armed for the attached WGL context.'
$shutdownMarker = '[ModernGraphics] Shutdown completed before legacy WGL teardown.'

Assert-LogContains $logText $attachAttemptMarker 'bootstrap attach attempt reached'
Assert-LogContains $logText $diagnosticsMarker 'OpenGL diagnostics captured'
Assert-LogContains $logText 'profile=compatibility' 'OpenGL compatibility profile confirmed for legacy coexistence'
Assert-LogContains $logText $attachedMarker 'Diligent attached to the existing WGL context'
Assert-LogContains $logText $teardownMarker 'reattach barrier armed before WGL teardown'
Assert-LogContains $logText $shutdownMarker 'Diligent shutdown completed before WGL teardown'

# A successful Phase 2 session must have one attachment and one teardown/shutdown
# sequence. Merely finding the strings is insufficient: stale evidence or a
# reattach race could otherwise make a broken lifecycle look healthy.
Assert-LogOccursOnce $logText $attachedMarker 'exactly one successful Diligent attachment recorded'
Assert-LogOccursOnce $logText $teardownMarker 'exactly one teardown barrier recorded'
Assert-LogOccursOnce $logText $shutdownMarker 'exactly one modern shutdown recorded'

$attachAttemptIndex = Get-LogIndex $logText $attachAttemptMarker 'bootstrap attach attempt reached'
$diagnosticsIndex = Get-LogIndex $logText $diagnosticsMarker 'OpenGL diagnostics captured'
$attachedIndex = Get-LogIndex $logText $attachedMarker 'Diligent attachment recorded'
$teardownIndex = Get-LogIndex $logText $teardownMarker 'teardown barrier recorded'
$shutdownIndex = Get-LogIndex $logText $shutdownMarker 'shutdown recorded'

Assert-LogBefore $attachAttemptIndex 'attach attempt' $diagnosticsIndex 'OpenGL diagnostics'
Assert-LogBefore $diagnosticsIndex 'OpenGL diagnostics' $attachedIndex 'Diligent attachment'
Assert-LogBefore $attachedIndex 'Diligent attachment' $teardownIndex 'teardown barrier'
Assert-LogBefore $teardownIndex 'teardown barrier' $shutdownIndex 'modern shutdown'

if ($RequireResize) {
    Assert-LogContains $logText $resizeMarker 'resize lifecycle reached modern bootstrap'
    $resizeIndex = Get-LogIndex $logText $resizeMarker 'resize lifecycle reached modern bootstrap'
    Assert-LogBefore $attachedIndex 'Diligent attachment' $resizeIndex 'resize observation'
    Assert-LogBefore $resizeIndex 'resize observation' $teardownIndex 'teardown barrier'
}

if ($EnableGLDebug) {
    Assert-LogContains $logText $debugMarker 'Diligent validation/OpenGL debug routing enabled'
    $debugIndex = Get-LogIndex $logText $debugMarker 'Diligent validation/OpenGL debug routing enabled'
    Assert-LogBefore $diagnosticsIndex 'OpenGL diagnostics' $debugIndex 'debug routing enablement'
    Assert-LogBefore $debugIndex 'debug routing enablement' $attachedIndex 'Diligent attachment'
}

if ($logText -match 'Legacy renderer remains active') {
    throw 'Phase 2 runtime log contains a modern-backend fallback. Inspect ModernGraphics.log before continuing.'
}

# Once the teardown barrier is armed, the same session must never attempt to
# attach again. This makes the anti-reattach requirement observable instead of
# relying only on source inspection.
$postTeardownText = $logText.Substring($teardownIndex + $teardownMarker.Length)
if ($postTeardownText -match [regex]::Escape($attachAttemptMarker) -or
    $postTeardownText -match [regex]::Escape($attachedMarker)) {
    throw 'Phase 2 runtime check failed: a modern attach/attach-attempt was recorded after the teardown barrier.'
}
Write-Host '[Phase2 Runtime] PASS: no modern reattach occurred after the teardown barrier'

Write-Host ''
if ($ValidateOnly) {
    Write-Host '[Phase2 Runtime] Evidence parser validation passed for the checks requested by this script.'
}
else {
    Write-Host '[Phase2 Runtime] GPU runtime gate passed for the checks requested by this script.'
}
Write-Host "[Phase2 Runtime] Evidence log: $logPath"
