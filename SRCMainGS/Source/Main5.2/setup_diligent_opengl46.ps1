param(
    [switch]$ReleaseOnly,
    [switch]$BuildMain
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
        [Parameter(ValueFromRemainingArguments = $true)][string[]]$Arguments
    )

    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Program failed with exit code $LASTEXITCODE"
    }
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

New-Item -ItemType Directory -Path $DependenciesRoot -Force | Out-Null

if (-not (Test-Path (Join-Path $DiligentRoot '.git'))) {
    if (Test-Path $DiligentRoot) {
        throw "$DiligentRoot exists but is not a Git checkout. Remove or rename it before running this script."
    }

    Write-Host "[Phase2] Cloning DiligentCore $DiligentTag..."
    Invoke-Checked git clone --branch $DiligentTag --depth 1 --recursive --shallow-submodules $DiligentRepository $DiligentRoot
}

Write-Host '[Phase2] Verifying pinned DiligentCore revision...'
Invoke-Checked git -C $DiligentRoot fetch --depth 1 origin $DiligentCommit
Invoke-Checked git -C $DiligentRoot checkout --detach $DiligentCommit
Invoke-Checked git -C $DiligentRoot submodule sync --recursive
Invoke-Checked git -C $DiligentRoot submodule update --init --recursive --depth 1

$currentCommit = (& git -C $DiligentRoot rev-parse HEAD).Trim()
if ($LASTEXITCODE -ne 0 -or $currentCommit -ne $DiligentCommit) {
    throw "DiligentCore revision mismatch. Expected $DiligentCommit, got $currentCommit"
}

Write-Host '[Phase2] Configuring DiligentCore OpenGL-only Win32 build...'
Invoke-Checked cmake `
    -S $DiligentRoot `
    -B $BuildRoot `
    -A Win32 `
    -DDILIGENT_BUILD_TESTS=OFF `
    -DDILIGENT_NO_DIRECT3D11=ON `
    -DDILIGENT_NO_DIRECT3D12=ON `
    -DDILIGENT_NO_OPENGL=OFF `
    -DDILIGENT_NO_VULKAN=ON `
    -DDILIGENT_NO_WEBGPU=ON `
    -DDILIGENT_NO_ARCHIVER=ON `
    -DDILIGENT_NO_HLSL=OFF

Write-Host '[Phase2] Building Release OpenGL backend...'
Invoke-Checked cmake --build $BuildRoot --config Release --target Diligent-GraphicsEngineOpenGL-shared --parallel
Copy-BackendDll 'GraphicsEngineOpenGL_32r.dll'

if (-not $ReleaseOnly) {
    Write-Host '[Phase2] Building Debug OpenGL backend...'
    Invoke-Checked cmake --build $BuildRoot --config Debug --target Diligent-GraphicsEngineOpenGL-shared --parallel
    Copy-BackendDll 'GraphicsEngineOpenGL_32d.dll'
}

Write-Host "[Phase2] DiligentCore $DiligentTag is prepared at $DiligentRoot."
Write-Host '[Phase2] ModernGraphicsBootstrap.h will now enable MU_ENABLE_DILIGENT automatically for this checkout.'

if ($BuildMain) {
    if (-not (Get-Command msbuild -ErrorAction SilentlyContinue)) {
        throw 'msbuild.exe was not found in PATH. Run from a Visual Studio Developer PowerShell/Command Prompt or omit -BuildMain.'
    }

    Write-Host '[Phase2] Building Main Release|x86...'
    Invoke-Checked msbuild $SolutionPath /m /p:Configuration=Release /p:Platform=x86
}

Write-Host '[Phase2] Setup complete. Runtime/GPU validation must still be performed by launching Client_2/Main.exe on Windows.'
