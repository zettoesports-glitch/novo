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

function Get-LogIndices([string]$Text, [string]$Pattern) {
    $indices = @()
    $offset = 0

    while ($offset -lt $Text.Length) {
        $index = $Text.IndexOf($Pattern, $offset, [System.StringComparison]::Ordinal)
        if ($index -lt 0) {
            break
        }

        $indices += $index
        $offset = $index + $Pattern.Length
    }

    return @($indices)
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

if ($logText -match 'Legacy renderer remains active') {
    throw 'Phase 2 runtime log contains a modern-backend fallback. Inspect ModernGraphics.log before continuing.'
}

$attempts = Get-LogIndices $logText $attachAttemptMarker
$diagnostics = Get-LogIndices $logText $diagnosticsMarker
$attachments = Get-LogIndices $logText $attachedMarker
$teardowns = Get-LogIndices $logText $teardownMarker
$shutdowns = Get-LogIndices $logText $shutdownMarker
$debugEnables = Get-LogIndices $logText $debugMarker
$resizes = Get-LogIndices $logText $resizeMarker

$lifecycleCount = $attachments.Count
if ($lifecycleCount -lt 1) {
    throw 'Phase 2 runtime check failed: no successful modern lifecycle was recorded.'
}

# Each successful attachment must form one complete lifecycle. A legitimate
# HWND/HGLRC recreation may therefore produce more than one complete cycle in a
# single Main.exe run. What is forbidden is a partial/reordered cycle or a new
# attach attempt while the previous teardown is still in progress.
$requiredCounts = @{
    'attach attempts' = $attempts.Count
    'OpenGL diagnostics' = $diagnostics.Count
    'attachments' = $attachments.Count
    'teardown barriers' = $teardowns.Count
    'shutdowns' = $shutdowns.Count
}

foreach ($entry in $requiredCounts.GetEnumerator()) {
    if ($entry.Value -ne $lifecycleCount) {
        throw "Phase 2 runtime check failed: lifecycle count mismatch for $($entry.Key). Expected $lifecycleCount, found $($entry.Value)."
    }
}

if ($EnableGLDebug -and $debugEnables.Count -ne $lifecycleCount) {
    throw "Phase 2 runtime check failed: expected Diligent validation/debug routing for each lifecycle ($lifecycleCount), found $($debugEnables.Count)."
}

for ($i = 0; $i -lt $lifecycleCount; ++$i) {
    $number = $i + 1
    Assert-LogBefore $attempts[$i] "lifecycle $number attach attempt" $diagnostics[$i] "lifecycle $number OpenGL diagnostics"

    if ($EnableGLDebug) {
        Assert-LogBefore $diagnostics[$i] "lifecycle $number OpenGL diagnostics" $debugEnables[$i] "lifecycle $number debug routing enablement"
        Assert-LogBefore $debugEnables[$i] "lifecycle $number debug routing enablement" $attachments[$i] "lifecycle $number Diligent attachment"
    }
    else {
        Assert-LogBefore $diagnostics[$i] "lifecycle $number OpenGL diagnostics" $attachments[$i] "lifecycle $number Diligent attachment"
    }

    Assert-LogBefore $attachments[$i] "lifecycle $number Diligent attachment" $teardowns[$i] "lifecycle $number teardown barrier"
    Assert-LogBefore $teardowns[$i] "lifecycle $number teardown barrier" $shutdowns[$i] "lifecycle $number modern shutdown"

    if ($i + 1 -lt $lifecycleCount) {
        Assert-LogBefore $shutdowns[$i] "lifecycle $number modern shutdown" $attempts[$i + 1] "lifecycle $($number + 1) attach attempt"
    }
}

if ($RequireResize) {
    if ($resizes.Count -lt 1) {
        throw 'Phase 2 runtime check failed: -RequireResize was used but no modern resize observation was recorded.'
    }

    foreach ($resizeIndex in $resizes) {
        $belongsToActiveLifecycle = $false
        for ($i = 0; $i -lt $lifecycleCount; ++$i) {
            if ($resizeIndex -gt $attachments[$i] -and $resizeIndex -lt $teardowns[$i]) {
                $belongsToActiveLifecycle = $true
                break
            }
        }

        if (-not $belongsToActiveLifecycle) {
            throw 'Phase 2 runtime check failed: a resize marker was recorded outside an active modern attachment lifecycle.'
        }
    }

    Write-Host "[Phase2 Runtime] PASS: $($resizes.Count) resize observation(s) occurred only during active modern lifecycle(s)"
}

Write-Host "[Phase2 Runtime] PASS: $lifecycleCount complete modern lifecycle(s) recorded in valid order"
Write-Host '[Phase2 Runtime] PASS: no attach attempt occurred between a teardown barrier and its matching shutdown'
Write-Host '[Phase2 Runtime] NOTE: source-level HWND/HGLRC identity guards prevent same-context reattach; a later complete lifecycle is allowed for a genuinely recreated window/context pair.'
Write-Host '[Phase2 Runtime] NOTE: presentation ownership is proven by source architecture (no Diligent swap chain/present); this log parser does not count SwapBuffers calls per frame.'

Write-Host ''
if ($ValidateOnly) {
    Write-Host '[Phase2 Runtime] Evidence parser validation passed for the checks requested by this script.'
}
else {
    Write-Host '[Phase2 Runtime] GPU runtime gate passed for the checks requested by this script.'
}
Write-Host "[Phase2 Runtime] Evidence log: $logPath"
