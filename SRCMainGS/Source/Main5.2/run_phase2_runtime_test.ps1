param(
    [switch]$EnableGLDebug,
    [switch]$RequireResize,
    [switch]$ValidateOnly
)

$ErrorActionPreference = 'Stop'

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
    Write-Host "[Phase2 Runtime] Found $Label: $Path"
}

function Assert-LogContains([string]$Text, [string]$Pattern, [string]$Label) {
    if ($Text -notmatch [regex]::Escape($Pattern)) {
        throw "Phase 2 runtime check failed: $Label. Expected log entry: $Pattern"
    }
    Write-Host "[Phase2 Runtime] PASS: $Label"
}

Assert-Exists $mainExe 'Main.exe'
Assert-Exists $releaseBackend 'GraphicsEngineOpenGL_32r.dll'
Assert-Exists $debugBackend 'GraphicsEngineOpenGL_32d.dll'

if (-not $ValidateOnly) {
    if (Test-Path $logPath) {
        Remove-Item $logPath -Force
    }

    $previousDebugSetting = $env:MU_MODERN_GL_DEBUG
    try {
        if ($EnableGLDebug) {
            $env:MU_MODERN_GL_DEBUG = '1'
            Write-Host '[Phase2 Runtime] MU_MODERN_GL_DEBUG=1 enabled for this client run.'
        }
        else {
            Remove-Item Env:MU_MODERN_GL_DEBUG -ErrorAction SilentlyContinue
        }

        Write-Host '[Phase2 Runtime] Starting Main.exe.'
        Write-Host '[Phase2 Runtime] Let the client reach a rendered scene, resize the window at least once if -RequireResize was used, then close the client normally.'
        $process = Start-Process -FilePath $mainExe -WorkingDirectory $clientRoot -PassThru -Wait
        Write-Host "[Phase2 Runtime] Main.exe exited with code $($process.ExitCode)."
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

Assert-Exists $logPath 'ModernGraphics.log'
$logText = Get-Content $logPath -Raw

Assert-LogContains $logText '[ModernGraphics] Phase 2 OpenGL 4.6 attach attempt started.' 'bootstrap attach attempt reached'
Assert-LogContains $logText '[ModernGraphics] OpenGL vendor=' 'OpenGL diagnostics captured'
Assert-LogContains $logText '[ModernGraphics] Diligent attached to the existing OpenGL 4.6 context; legacy SwapBuffers remains authoritative.' 'Diligent attached to the existing WGL context'
Assert-LogContains $logText '[ModernGraphics] Shutdown completed before legacy WGL teardown.' 'Diligent shutdown completed before WGL teardown'

if ($RequireResize) {
    Assert-LogContains $logText '[ModernGraphics] Resize observed:' 'resize lifecycle reached modern bootstrap'
}

if ($EnableGLDebug) {
    Assert-LogContains $logText '[ModernGraphics] OpenGL debug callback enabled by MU_MODERN_GL_DEBUG.' 'OpenGL debug callback enabled'
}

if ($logText -match 'Legacy renderer remains active') {
    throw 'Phase 2 runtime log contains a modern-backend fallback. Inspect ModernGraphics.log before continuing.'
}

Write-Host ''
Write-Host '[Phase2 Runtime] GPU runtime gate passed for the checks requested by this script.'
Write-Host "[Phase2 Runtime] Evidence log: $logPath"