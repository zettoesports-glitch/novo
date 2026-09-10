param(
    [switch]$BuildMain,
    [ValidateSet('Debug', 'Release')]
    [string]$MainConfiguration = 'Debug',
    [string]$CMakeGenerator = ''
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

$DiligentRepository = 'https://github.com/DiligentGraphics/DiligentCore.git'
$DiligentTag = 'v2.5.6'
$DiligentCommit = 'b036337d68be2353c9950a85929acf796b9a6d50'

$MainRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$DependenciesRoot = Join-Path $MainRoot 'dependencies'
$DiligentRoot = Join-Path $DependenciesRoot 'DiligentCore'
$BuildRoot = Join-Path $DependenciesRoot '_diligent_build'
$ClientRoot = [System.IO.Path]::GetFullPath((Join-Path $MainRoot '..\..\..\Client_2'))
$SolutionPath = Join-Path $MainRoot 'Main.sln'

function Invoke-Checked {
    param(
        [Parameter(Mandatory = $true)][string]$Program,
        [Parameter(Mandatory = $true)][string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
}

function Resolve-CMakeVisualStudioGenerator {
    param([string]$RequestedGenerator)

    if (-not [string]::IsNullOrWhiteSpace($RequestedGenerator)) {
        return $RequestedGenerator
    }

    $cmakeHelp = (& cmake --help 2>&1) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw 'Unable to query CMake generators.'
    }

    # Prefer the newest generator supported by the installed CMake/Visual Studio,
    # but keep VS2022 compatibility for existing Main 5.2 development machines.
    $candidates = @(
        'Visual Studio 18 2026',
        'Visual Studio 17 2022'
    )

    foreach ($candidate in $candidates) {
        if ($cmakeHelp -match [regex]::Escape($candidate)) {
            return $candidate
        }
    }

    throw 'No supported Visual Studio CMake generator found. Install Visual Studio 2022/2026 C++ tools or pass -CMakeGenerator explicitly.'
}

function Copy-BackendDll {
    param([Parameter(Mandatory = $true)][string]$FileName)

    $candidate = Get-ChildItem -Path $BuildRoot -Filter $FileName -File -Recurse | Select-Object -First 1
    if ($null -eq $candidate) {
        throw "Unable to find $FileName under $BuildRoot"
    }

    New-Item -ItemType Directory -Path $ClientRoot -Force | Out-Null
    Copy-Item -Path $candidate.FullName -Destination (Join-Path $ClientRoot $FileName) -Force
    Write-Host "[Phase2] Copied $FileName to Client_2."
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'git.exe was not found in PATH.'
}
if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw 'cmake.exe was not found in PATH.'
}

$ResolvedCMakeGenerator = Resolve-CMakeVisualStudioGenerator -RequestedGenerator $CMakeGenerator
Write-Host "[Phase2] Using CMake generator: $ResolvedCMakeGenerator"

New-Item -ItemType Directory -Path $DependenciesRoot -Force | Out-Null

if (-not (Test-Path (Join-Path $DiligentRoot '.git'))) {
    if (Test-Path $DiligentRoot) {
        throw "$DiligentRoot exists but is not a Git checkout. Remove or rename it before running this script."
    }

    Write-Host "[Phase2] Cloning DiligentCore $DiligentTag..."
    Invoke-Checked -Program 'git' -Arguments @(
        '-c', 'core.longpaths=true',
        'clone', '--branch', $DiligentTag, '--depth', '1', '--recursive',
        '--shallow-submodules', $DiligentRepository, $DiligentRoot
    )
}

Write-Host '[Phase2] Verifying pinned DiligentCore revision...'
Invoke-Checked -Program 'git' -Arguments @('-c', 'core.longpaths=true', '-C', $DiligentRoot, 'fetch', '--depth', '1', 'origin', $DiligentCommit)
Invoke-Checked -Program 'git' -Arguments @('-c', 'core.longpaths=true', '-C', $DiligentRoot, 'checkout', '--detach', $DiligentCommit)
Invoke-Checked -Program 'git' -Arguments @('-c', 'core.longpaths=true', '-C', $DiligentRoot, 'submodule', 'sync', '--recursive')
Invoke-Checked -Program 'git' -Arguments @('-c', 'core.longpaths=true', '-C', $DiligentRoot, 'submodule', 'update', '--init', '--recursive', '--depth', '1')

$currentCommit = (& git -c core.longpaths=true -C $DiligentRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $currentCommit -ne $DiligentCommit) {
    throw "DiligentCore revision mismatch. Expected $DiligentCommit, got $currentCommit"
}

Write-Host '[Phase2] Configuring DiligentCore OpenGL-only Win32 build...'
Invoke-Checked -Program 'cmake' -Arguments @(
    '-S', $DiligentRoot,
    '-B', $BuildRoot,
    '-G', $ResolvedCMakeGenerator,
    '-A', 'Win32',
    '-DDILIGENT_BUILD_TESTS=OFF',
    '-DDILIGENT_NO_DIRECT3D11=ON',
    '-DDILIGENT_NO_DIRECT3D12=ON',
    '-DDILIGENT_NO_OPENGL=OFF',
    '-DDILIGENT_NO_VULKAN=ON',
    '-DDILIGENT_NO_WEBGPU=ON',
    '-DDILIGENT_NO_ARCHIVER=ON',
    '-DDILIGENT_NO_HLSL=OFF'
)

Write-Host '[Phase2] Building Release OpenGL backend...'
Invoke-Checked -Program 'cmake' -Arguments @(
    '--build', $BuildRoot, '--config', 'Release',
    '--target', 'Diligent-GraphicsEngineOpenGL-shared', '--parallel'
)
Copy-BackendDll -FileName 'GraphicsEngineOpenGL_32r.dll'

Write-Host '[Phase2] Building Debug OpenGL backend...'
Invoke-Checked -Program 'cmake' -Arguments @(
    '--build', $BuildRoot, '--config', 'Debug',
    '--target', 'Diligent-GraphicsEngineOpenGL-shared', '--parallel'
)
Copy-BackendDll -FileName 'GraphicsEngineOpenGL_32d.dll'

Write-Host "[Phase2] DiligentCore $DiligentTag is prepared at $DiligentRoot."
Write-Host '[Phase2] ModernGraphicsBootstrap.h will now enable MU_ENABLE_DILIGENT automatically for this checkout.'

if ($BuildMain) {
    if (-not (Get-Command msbuild -ErrorAction SilentlyContinue)) {
        throw 'msbuild.exe was not found in PATH. Run from a Visual Studio Developer PowerShell/Command Prompt or omit -BuildMain.'
    }

    Write-Host "[Phase2] Building Main $MainConfiguration|x86..."
    Invoke-Checked -Program 'msbuild' -Arguments @(
        $SolutionPath, '/m', "/p:Configuration=$MainConfiguration", '/p:Platform=x86'
    )
}

Write-Host '[Phase2] Setup complete. Runtime/GPU validation must still be performed by launching Client_2/Main.exe on Windows.'
