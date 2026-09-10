# Phase 2 Runtime Discovery

Date: 2026-09-10
Branch: `modernization`

This document records the real Main 5.2 lifecycle points used by the Phase 2 OpenGL 4.6/Diligent bootstrap. Phase 2 is still not runtime-complete until the Windows GPU validation gate in `PHASE2_OPENGL46_BOOTSTRAP.md` passes.

## Confirmed Main lifecycle

### Window ownership

`SRCMainGS/Source/Main5.2/source/WINHANDLE.cpp`

- `CWINHANDLE::Create(...)` creates the client `HWND`.
- the window class uses `CS_OWNDC`, so the window keeps a stable device-context relationship suitable for the coexistence bridge.
- `CWINHANDLE::winLoop()` calls `Scene(g_hDC)` from the main/UI thread.

### OpenGL context ownership

`SRCMainGS/Source/Main5.2/source/Winmain.cpp`

`CreateOpenglWindow()` owns the legacy Win32/WGL initialization:

1. `GetDC()` stores `g_hDC`;
2. `ChoosePixelFormat` / `SetPixelFormat` configure a double-buffered window surface;
3. `wglCreateContext()` stores `g_hRC`;
4. `wglMakeCurrent(g_hDC, g_hRC)` makes it current;
5. `glewInit()` initializes extension entry points.

The context is therefore externally created from Diligent's point of view. Phase 2 uses `AttachToActiveGLContext`; it does not create a second OpenGL context or swap chain.

The current code still uses legacy `wglCreateContext()` rather than explicitly requesting a 4.6 context. The bootstrap validates the **effective** GL version and remains inactive if the driver does not expose >= 4.6. Explicit 4.6 compatibility-context creation can be evaluated later, before modern asset creation, only if it does not invalidate legacy GL objects.

### Present ownership

`SRCMainGS/Source/Main5.2/source/ZzzScene.cpp`

`MainScene(HDC)` owns the normal render-loop presentation:

```cpp
if (Success)
{
    glFlush();
    SwapBuffers(hDC);
}
```

`LoadingScene(HDC)` also owns presentation while that scene is active. No Phase 2 code calls `SwapBuffers`, and no Diligent swap chain is created. There remains exactly one application-owned presentation path per rendered scene frame.

### Resize

`CWINHANDLE::WndProc` receives `WM_SIZE`. The temporary Phase 2 lifecycle bridge observes `WM_SIZE` on the same UI thread and calls `CModernGraphicsBootstrap::OnResize(width, height)` for the attached window when the new size is non-zero and not minimized.

`OnResize()` currently records the dimensions and writes a persistent runtime marker; it does not own a swap chain or recreate legacy framebuffer resources.

### Shutdown

`CWINHANDLE::WndProc` handles `WM_CLOSE` / `WM_DESTROY` and then calls `KillGLWindow()`. `KillGLWindow()` unbinds/deletes `g_hRC` and releases `g_hDC`.

The Phase 2 bridge observes `WM_CLOSE`, `WM_DESTROY` and `WM_NCDESTROY` before the legacy WndProc processes them and calls `CModernGraphicsBootstrap::Shutdown()`. This flushes/releases the Diligent immediate context/device while the external WGL context is still alive and records a persistent shutdown marker.

## Coexistence bridge

The first runtime wiring is compiled through `CShaderGL.cpp`, which is already part of `Main.vcxproj`. When `MU_ENABLE_DILIGENT` is enabled, a `WH_CALLWNDPROC` hook observes the Main UI thread and attaches Diligent after the real legacy WGL context becomes current.

This is a bootstrap bridge, not a replacement window system. It deliberately does **not**:

- subclass the window;
- replace the legacy WndProc;
- hook or replace `SwapBuffers`;
- create a second WGL context;
- create a Diligent swap chain;
- move BMD/terrain/effects/UI to the modern renderer yet.

The hook remains a **temporary Phase 2 bridge**. After the Windows GPU gate is proven, initialization/resize/shutdown may be moved directly into `CreateOpenglWindow()`, `CWINHANDLE::WndProc` and the pre-`KillGLWindow()` path without changing `CModernGraphicsBootstrap`'s public contract.

A hook-installation failure is also persisted to `ModernGraphics.log`, so the runtime evidence path does not depend on an attached debugger.

## Capability and diagnostics

`CModernGraphicsBootstrap::InitializeOpenGL46()`:

- verifies the supplied `HWND/HDC/HGLRC` are valid;
- verifies the supplied WGL context/DC are current;
- reads/parses the effective OpenGL major/minor version;
- records vendor, renderer, OpenGL version, GLSL version and context profile;
- refuses modern activation below OpenGL 4.6;
- loads the Diligent OpenGL backend through the official Win32 DLL loader;
- obtains `IEngineFactoryOpenGL` from the backend module;
- registers `ModernDiligentMessageCallback` through `IEngineFactory::SetMessageCallback()` so Diligent diagnostics are persisted;
- when `MU_MODERN_GL_DEBUG=1`, enables `EngineGLCreateInfo.EnableValidation`, allowing the Diligent OpenGL backend to own KHR_debug and route those messages through the same factory callback;
- attaches Diligent through `AttachToActiveGLContext`;
- records ownership as `Attached` after successful attachment.

This avoids registering a competing raw `glDebugMessageCallback` in Main. Diligent v2.5.6 already registers its own OpenGL debug callback when validation is enabled, so factory-level message routing is the correct ownership boundary.

Diagnostics are sent both to `OutputDebugStringA` and to `ModernGraphics.log` beside `Main.exe`. The path is resolved from the executable location, so launcher/working-directory differences do not move the Phase 2 evidence file away from `Client_2`. This creates persistent evidence without requiring a debugger.

The existing `CErrorReport::WriteOpenGLInfo()` remains the application's original OpenGL information path; the Phase 2 log supplements rather than replaces it.

## Dependency/build state

DiligentCore is pinned in `DILIGENT_PIN.md` and the repository contains a reproducible setup path:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

The script:

- clones/verifies DiligentCore `v2.5.6` at commit `b036337d68be2353c9950a85929acf796b9a6d50`;
- initializes the required recursive submodules;
- configures a Win32 OpenGL-only build with HLSL support;
- builds `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`;
- copies both modules into `Client_2`;
- optionally builds `Main.sln` through `-BuildMain`.

The generated Diligent checkout/build directories are ignored by `Main5.2/.gitignore`.

`ModernGraphicsBootstrap.h` auto-enables `MU_ENABLE_DILIGENT` for MSVC/Win32 when the expected local Diligent OpenGL header exists. Main's `DEBUG` configuration is mapped to Diligent's public debug definitions so the official loader selects the debug backend; Release selects the release backend.

No Diligent engine import library is required by Main for this Phase 2 path: the backend is loaded dynamically.

`source/Directory.Build.targets` scopes a C++17 override to `Debug|Win32` for Main only. Release was already C++17. DiligentCore retains its upstream project settings.

## Windows/x86 compile evidence

### Release/x86

Workflow run `34533022717`, commit `b641263293ca1eebf5ed28e96cbfbd2643f1269b`:

- pinned Diligent checkout/submodules completed successfully;
- OpenGL-only Win32 configuration completed successfully;
- both `_32r` and `_32d` backend DLLs were built;
- `Main.sln` built as `Release|x86` using C++17;
- `CShaderGL.cpp` and the included Phase 2 bootstrap compiled successfully;
- `Client_2/Main.exe` linked successfully;
- final Main result: **0 errors**.

### Debug/x86

Workflow run `34533868904`, commit `51a21347b0f55a98957f85e53a6a5fde6bf7b2a4`:

- the same pinned Diligent preparation completed successfully;
- Main Debug/x86 compiled using the Main-only C++17 normalization;
- `Client_2/Main.exe` linked successfully;
- final Main result: **0 errors**.

The earlier Debug failure was caused by the legacy project using C++14 while its vendored sol2 headers already require C++17; it was not a Diligent attach failure.

### Permanent combined gate

`.github/workflows/phase2-win32-build.yml` builds Release/x86 and Debug/x86 sequentially in one Windows job, verifies `Main.exe`, `_32r.dll` and `_32d.dll`, and finally validates `run_phase2_runtime_test.ps1 -ValidateOnly` against synthetic Phase 2 evidence.

The final combined gate is workflow run `34538658227` at source/workflow commit `35cffa30b904071dcdf8a086a6ed5e03daca0342`. It completed successfully, including the PowerShell syntax preflight, pinned Diligent preparation, Release/x86 build/output verification, Debug/x86 build/output verification and synthetic runtime-evidence validation.

## Reproducible GPU runtime evidence

`SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1` turns the remaining manual GPU gate into a repeatable validation procedure.

From `SRCMainGS/Source/Main5.2`:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

The script verifies the executable and both backend DLLs, clears stale evidence, launches `Main.exe` from the `Client_2` working directory, waits for normal client shutdown and then checks the executable-relative `ModernGraphics.log` for:

- bootstrap attach attempt;
- OpenGL vendor/version/profile diagnostics;
- successful Diligent attach to the existing WGL context;
- Diligent validation/OpenGL debug routing when `-EnableGLDebug` is requested;
- resize marker when `-RequireResize` is requested;
- modern shutdown before legacy WGL teardown;
- absence of a `Legacy renderer remains active` fallback marker.

The script also supports `-ValidateOnly`. The Windows CI uses this mode with synthetic evidence after compiling both configurations. This validates the PowerShell parser/path/assertion logic without falsely treating hosted CI as a real GPU runtime test. The final combined run `34538658227` passed this validation.

The log is intentionally ignored by Git so local runtime evidence is not accidentally committed as a generated client file.

## Remaining runtime gate

A real GPU-backed Main run must still prove:

- expected backend DLL/factory loads at runtime;
- effective OpenGL version is >= 4.6 on the target system;
- `AttachToActiveGLContext` succeeds against the real Main WGL context;
- Diligent validation/KHR_debug operates without callback ownership conflict;
- resize remains stable;
- only the legacy `SwapBuffers` presents;
- shutdown has no lifetime/context errors;
- legacy scenes render without regression.

Until those runtime checks pass, Phase 2 is repository/build-path implemented and combined-gate build/evidence proven for both x86 configurations, but **not GPU runtime-certified**.
