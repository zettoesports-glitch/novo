# Phase 2 Runtime Discovery

Date: 2026-09-10
Branch: `modernization`

This document records the real Main 5.2 lifecycle points used by the Phase 2 OpenGL 4.6/Diligent bootstrap. Phase 2 is repository/build-complete, but it is not GPU runtime-certified until the target Windows/OpenGL validation gate in `PHASE2_OPENGL46_BOOTSTRAP.md` passes.

## Confirmed Main lifecycle

### Window ownership

`SRCMainGS/Source/Main5.2/source/WINHANDLE.cpp`

- `CWINHANDLE::Create(...)` creates the client `HWND`.
- the window class uses `CS_OWNDC`, keeping a stable device-context relationship suitable for the coexistence bridge.
- `CWINHANDLE::winLoop()` calls `Scene(g_hDC)` from the main/UI thread.

### OpenGL context ownership

`SRCMainGS/Source/Main5.2/source/Winmain.cpp`

`CreateOpenglWindow()` owns the legacy Win32/WGL initialization:

1. `GetDC()` stores `g_hDC`;
2. `ChoosePixelFormat` / `SetPixelFormat` configure a double-buffered surface;
3. `wglCreateContext()` stores `g_hRC`;
4. `wglMakeCurrent(g_hDC, g_hRC)` makes it current;
5. `glewInit()` initializes extension entry points.

The context is externally created from Diligent's point of view. Phase 2 uses `AttachToActiveGLContext`; it does not create a second OpenGL context or swap chain.

The current code still uses legacy `wglCreateContext()` rather than explicitly requesting a 4.6 context. The bootstrap validates the effective GL version and rejects core-only contexts before the Diligent attach. The real runtime gate requires OpenGL >= 4.6 with `profile=compatibility`, because the legacy coexistence path still depends on fixed-function/client-array behavior.

### Present ownership

`SRCMainGS/Source/Main5.2/source/ZzzScene.cpp`

The audited legacy source contains the legacy `SwapBuffers(hDC)` presentation call in the scene flow. No Phase 2 code calls `SwapBuffers`, and no Diligent swap chain is created. Presentation therefore remains application/legacy-owned during coexistence, with no second modern presentation path.

### Resize

`CWINHANDLE::WndProc` receives `WM_SIZE`. The temporary Phase 2 lifecycle bridge observes `WM_SIZE` on the same UI thread and calls `CModernGraphicsBootstrap::OnResize(width, height)` only when the tracked window has a successfully active modern attachment, the size is non-zero and the window is not minimized.

`OnResize()` records dimensions and a persistent runtime marker; it does not own a swap chain or recreate legacy framebuffer resources. Failed/legacy-only initialization attempts do not enter this modern resize path.

### Shutdown and WGL teardown audit

`KillGLWindow()` unbinds/deletes `g_hRC` and releases `g_hDC`.

The audit found all currently known `KillGLWindow()` call classes:

- error exits inside `CreateOpenglWindow()`; these are initialization-failure cleanup paths before a successful modern attach can exist;
- `WM_DESTROY` / `WM_CLOSE` cleanup in `CWINHANDLE::WndProc`;
- the exceptional `WM_USER_MEMORYHACK` path, which also calls `KillGLWindow()` directly.

The lifecycle bridge shuts Diligent down before `WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY` and `WM_USER_MEMORYHACK`, but only when the modern attachment is actually active for that window. This prevents failed/fallback initialization attempts from being treated as attached runtime state.

Commit `cf20045e70c888a14b2b3197663e095e51b60768` added the exceptional `WM_USER_MEMORYHACK` coverage. Commit `c564ef479b3727f1d50beb3760c1af047f138ad5` then added a teardown barrier keyed by the last attached `HWND/HGLRC`: after `Shutdown()` begins, later Win32 messages cannot reattach Diligent to the same WGL context while that context is being destroyed. A genuinely different window/context pair clears the barrier and can start a fresh attach attempt. Commit `ad6d6e213433fe5fdcb694d96ecd3745131e98c2` further gates teardown and resize forwarding on `IsActive()` so fallback-only attempts cannot arm the barrier or emit modern resize evidence.

`Shutdown()` flushes/releases the Diligent immediate context/device while the external WGL context is still alive, then records the shutdown marker.

## Coexistence bridge

The first runtime wiring is compiled through `CShaderGL.cpp`, which is already part of `Main.vcxproj`. When `MU_ENABLE_DILIGENT` is enabled, a `WH_CALLWNDPROC` hook observes the Main UI thread and attaches Diligent after the real legacy WGL context becomes current.

This is a temporary bootstrap bridge. It deliberately does not:

- subclass or replace the legacy WndProc;
- hook or replace `SwapBuffers`;
- create a second WGL context;
- create a Diligent swap chain;
- move BMD, terrain, effects or UI to the modern renderer.

After the real GPU gate is proven, initialization/resize/shutdown can be reassessed and normally moved directly into the already-mapped lifecycle owners without changing `CModernGraphicsBootstrap`'s public contract.

## Capability and diagnostics

`CModernGraphicsBootstrap::InitializeOpenGL46()`:

- verifies the supplied `HWND/HDC/HGLRC`;
- verifies the supplied WGL context/DC are current;
- reads/parses effective OpenGL major/minor;
- records vendor, renderer, OpenGL version, GLSL version and context profile;
- refuses modern activation below OpenGL 4.6;
- refuses modern activation for a core-only/non-compatibility profile, preserving the legacy renderer;
- loads the Diligent OpenGL backend through the official Win32 DLL loader;
- obtains `IEngineFactoryOpenGL`;
- registers `ModernDiligentMessageCallback` through `IEngineFactory::SetMessageCallback()`;
- enables Diligent validation when `MU_MODERN_GL_DEBUG=1`;
- attaches through `AttachToActiveGLContext`;
- records ownership as `Attached` after successful attachment.

The Main does not install a competing raw `glDebugMessageCallback`. Diligent owns KHR_debug when validation is enabled and routes messages through the factory callback.

Diagnostics go to both `OutputDebugStringA` and `ModernGraphics.log` beside `Main.exe`. The path is resolved from the executable location, so launcher/working-directory differences do not relocate Phase 2 evidence.

## Dependency/build state

DiligentCore is pinned in `DILIGENT_PIN.md` and prepared by:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

The script:

- verifies DiligentCore `v2.5.6` at commit `b036337d68be2353c9950a85929acf796b9a6d50`;
- initializes required recursive submodules;
- configures Win32 OpenGL-only with HLSL support;
- builds `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`;
- copies both modules into `Client_2`;
- can optionally build `Main.sln`.

`ModernGraphicsBootstrap.h` auto-enables `MU_ENABLE_DILIGENT` for MSVC/Win32 when the expected local Diligent OpenGL header exists. Debug maps Main's existing `DEBUG` configuration to Diligent's debug public definitions; Release selects the release backend. The backend is loaded dynamically, so Main does not require the Diligent OpenGL engine import library.

`source/Directory.Build.targets` scopes the required C++17 normalization to Main `Debug|Win32`; Release was already C++17.

## Windows/x86 build evidence

Earlier independent proof:

- Release/x86 run `34533022717`: pinned Diligent prepared, both backend DLLs produced, Main linked with 0 errors.
- Debug/x86 run `34533868904`: Main linked with 0 errors after the Main-only C++17 normalization.
- Combined baseline run `34538658227`: Release + Debug + output verification + synthetic runtime-evidence validation all passed.
- Lifecycle/runtime-gate audit run `34540959623`: Release + Debug + compatibility-profile synthetic evidence all passed after teardown/runtime-gate hardening.
- Compatibility-profile source gate `34542334616`: Release + Debug + output verification + synthetic runtime-evidence validation passed at `e6c0a678ecbb870262d62ed362902ce999adb06c`.

The teardown reattach barrier and active-attachment lifecycle gating were added after that baseline. Their latest combined Windows/x86 gate is tracked in `STATUS.md` and `IMPLEMENTATION_CHECKLIST.md` once the workflow completes.

## Reproducible GPU runtime evidence

`SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1` provides the real target-machine gate.

From `SRCMainGS/Source/Main5.2`:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

The real launch path verifies `Main.exe`, both Diligent backend DLLs, removes stale evidence, launches from `Client_2`, waits for normal close and requires exit code 0. It then validates:

- bootstrap attach attempt;
- OpenGL diagnostics;
- `profile=compatibility`;
- successful Diligent attach;
- validation/OpenGL debug routing when requested;
- resize marker when requested;
- teardown barrier armed before legacy WGL destruction;
- modern shutdown before legacy WGL teardown;
- absence of a `Legacy renderer remains active` fallback marker.

`-ValidateOnly` remains a parser/evidence-test mode for CI; it does not pretend to be a GPU run. Synthetic CI evidence now includes the teardown-barrier marker so the parser contract follows the hardened lifecycle.

## Remaining runtime gate

A real GPU-backed Main run must still prove:

- expected backend DLL/factory loads at runtime;
- effective OpenGL version is >= 4.6 and profile is compatibility;
- `AttachToActiveGLContext` succeeds against the real Main WGL context;
- Diligent validation/KHR_debug works without callback ownership conflict;
- resize is stable and only forwarded while the modern attachment is active;
- legacy presentation remains the only presentation path;
- teardown cannot reattach to the same dying `HWND/HGLRC`;
- shutdown/lifetime ordering is clean, including normal close and no WGL lifetime regression;
- legacy scenes render without regression.

Until those checks pass on the target Windows/GPU machine, Phase 2 is **repository/build-complete but not GPU runtime-certified**.