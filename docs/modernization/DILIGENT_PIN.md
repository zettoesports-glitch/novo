# Diligent / shader revision pin — Phase 2

Date: 2026-09-10
Target branch: `modernization`

## DiligentCore

The Phase 2 bootstrap is pinned to the official stable DiligentCore release below:

- repository: `DiligentGraphics/DiligentCore`
- tag: `v2.5.6`
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`
- OpenGL integration API: `IEngineFactoryOpenGL::AttachToActiveGLContext`
- Win32 backend loading API: `LoadGraphicsEngineOpenGL()` / `GetEngineFactoryOpenGL`

This pin is for reproducible Main 5.2 integration. It is **not** a claim that NextMU used this exact DiligentCore revision.

Do not replace this pin with a moving `master`/`main` reference during Phase 2. A later revision must be introduced as an explicit dependency upgrade and revalidated on Win32.

## Integration model used by Main 5.2

Main does not statically link the Diligent OpenGL engine into `Main.exe` during the coexistence phase.

Instead:

1. the pinned DiligentCore source checkout supplies the public C++ interface headers;
2. `ENGINE_DLL=1` enables Diligent's official Win32 explicit backend loader;
3. Release loads `GraphicsEngineOpenGL_32r.dll`;
4. Debug loads `GraphicsEngineOpenGL_32d.dll`;
5. the loaded factory attaches to the already-current legacy WGL context through `AttachToActiveGLContext`;
6. Main continues to own `HWND`, `HDC`, `HGLRC` and `SwapBuffers`.

This mirrors the backend-module shape observed in the reference package while keeping the Main renderer architecture backend-neutral for later Vulkan and Direct3D 11 work.

No Diligent import library is added to the legacy `Main.vcxproj` for this Phase 2 path. Failure to find/load the backend DLL leaves the modern renderer inactive and preserves the legacy renderer.

## Reproducible local setup

The repository contains:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

The script:

- clones DiligentCore recursively;
- checks out exactly `b036337d68be2353c9950a85929acf796b9a6d50`;
- synchronizes/initializes the required submodules;
- configures a Win32 OpenGL-only build with HLSL support enabled;
- builds the official shared OpenGL backend for Release and Debug;
- copies `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll` to `Client_2`;
- can optionally build `Main.sln` with `-BuildMain`.

Example from the `Main5.2` directory in a Visual Studio Developer PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File .\setup_diligent_opengl46.ps1 -BuildMain
```

The generated checkout/build folders are intentionally ignored:

- `dependencies/DiligentCore/`
- `dependencies/_diligent_build/`

The third-party source/binaries are therefore reproducible from the pin and are not silently committed as generated repository state.

## Activation rule

`ModernGraphicsBootstrap.h` checks for the pinned dependency layout under `dependencies/DiligentCore`.

- If the Diligent OpenGL header is absent, `MU_ENABLE_DILIGENT` remains undefined and the legacy build path remains active.
- If the header is present, MSVC/Win32 automatically enables the Phase 2 Diligent path for the bootstrap translation unit.
- If the matching backend DLL is missing at runtime, the explicit loader fails safely and the legacy renderer remains active.

This makes dependency preparation explicit while avoiding a permanent manual project macro that could accidentally break machines that have not prepared Diligent yet.

## Reference shader/resource snapshot

The reference shader contracts used by the modernization work are pinned to the repository snapshot:

- repository: `zettoesports-glitch/novo`
- baseline commit: `ceadb719f47bc781f789fe56d2e0eff2c4c8012b`
- reference package: `Resources_DE_2024-01-25`

This snapshot is the evidence baseline for the NextMU model/world/effect shader contracts already mapped in `ReverseEngineering/NextMU/`. The production shared-HLSL shader package for Main 5.2 is a later implementation task; this pin prevents the reference inputs from drifting while that bridge is built.

## Validation boundary

The setup/build path is now represented in the repository, but Phase 2 is **not runtime-certified** until it is executed on Windows and the OpenGL 4.6 attach/resize/present/shutdown gate passes.
