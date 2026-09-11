# Phase 2 Runtime Discovery

Date-base: 2026-09-10  
Branch: `modernization`

This document records the real Main 5.2 lifecycle points used by the Phase 2 OpenGL 4.6/Diligent bootstrap. Corrective details live in [PHASE2_COMPLETENESS_AUDIT.md](PHASE2_COMPLETENESS_AUDIT.md), and the latest build gate lives in [STATUS.md](STATUS.md).

Phase 2 is repository/build-complete only after its current Windows/x86 gate passes. It is not GPU runtime-certified until the target Windows/OpenGL validation gate passes.

## Confirmed Main lifecycle

### Window ownership

`SRCMainGS/Source/Main5.2/source/WINHANDLE.cpp`

- `CWINHANDLE::Create(...)` creates the client `HWND`.
- the window class uses `CS_OWNDC`;
- `CWINHANDLE::winLoop()` calls `Scene(g_hDC)` from the main/UI thread;
- `CWINHANDLE::WndProc` owns the mapped resize/close/destroy message paths.

### OpenGL context ownership

`SRCMainGS/Source/Main5.2/source/Winmain.cpp`

`CreateOpenglWindow()` owns the legacy Win32/WGL initialization:

1. `GetDC()` stores `g_hDC`;
2. `ChoosePixelFormat` / `SetPixelFormat` configure the double-buffered surface;
3. `wglCreateContext()` stores `g_hRC`;
4. `wglMakeCurrent(g_hDC, g_hRC)` makes it current;
5. `glewInit()` initializes extension entry points.

The context is externally created from Diligent's point of view. Phase 2 calls `AttachToActiveGLContext`; it does not create another OpenGL context or a Diligent window swap chain.

The legacy Main still uses `wglCreateContext()` rather than explicitly requesting 4.6. The bootstrap validates what the real driver returns. Modern activation requires OpenGL >= 4.6 and compatibility profile because coexistence still uses legacy fixed-function/client-array behavior.

### Presentation ownership

`SRCMainGS/Source/Main5.2/source/ZzzScene.cpp`

Static audit found two legacy `SwapBuffers(hDC)` call sites:

- loading-scene presentation;
- regular-scene presentation when that flow reports render success.

No Phase 2 code calls `SwapBuffers` and no Diligent swap chain/present path is created. Presentation therefore remains entirely legacy/application-owned.

The current evidence parser does not instrument or count legacy `SwapBuffers` calls per frame. Its proof boundary is lifecycle/ownership, not per-frame presentation counting.

### Resize

`CWINHANDLE::WndProc` receives `WM_SIZE`. The temporary Phase 2 bridge observes that message on the same UI thread and forwards non-zero, non-minimized dimensions only when a successful modern attachment is active for the tracked window.

`OnResize()` records dimensions/evidence only; it does not recreate a Diligent swap chain because no such swap chain exists in coexistence mode.

### Shutdown and WGL teardown

`KillGLWindow()` is the central owner that unbinds/deletes `g_hRC` and releases `g_hDC`.

The audit classified the known `KillGLWindow()` paths:

- cleanup inside failing `CreateOpenglWindow()` attempts — pre-modern-attach;
- normal `WM_CLOSE` / `WM_DESTROY` lifecycle;
- exceptional `WM_USER_MEMORYHACK` teardown path.

The temporary bridge observes `WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY` and `WM_USER_MEMORYHACK`.

For a successful modern attachment it:

1. arms the teardown generation barrier;
2. shuts Diligent down while the WGL context is still alive;
3. blocks reattach to the dying context;
4. clears the generation once a real WGL release is observed or a genuinely new live context generation appears.

The barrier does not rely forever on raw numeric handle inequality. After the old WGL generation has actually been released, Windows may recycle the same numeric `HWND/HGLRC` values and the new generation can still attach.

For an attempted-but-inactive bootstrap, teardown resets the tracked attempted state without writing a false successful-shutdown marker. This prevents an unsupported old context from leaving stale identity state that blocks a future recreated context.

## Temporary coexistence bridge

The first runtime wiring is compiled through `CShaderGL.cpp`, already part of `Main.vcxproj`. With `MU_ENABLE_DILIGENT`, a thread-local `WH_CALLWNDPROC` hook observes the Main UI lifecycle.

It deliberately does not:

- subclass/replace the legacy WndProc;
- replace/hook `SwapBuffers`;
- create a second WGL context;
- create a Diligent swap chain;
- migrate BMD, terrain, effects or UI.

It is intentionally temporary. Static inspection already identifies the preferred direct owners for a later cleanup: attach after the WGL initialization point, resize in the window lifecycle and shutdown immediately before the central WGL teardown. That replacement remains post-GPU-gate work so coexistence behavior is proven before moving the hooks.

## Capability and fallback diagnostics

`CModernGraphicsBootstrap::InitializeOpenGL46()`:

- verifies `HWND/HDC/HGLRC`;
- verifies that the supplied WGL context/DC are current;
- parses `GL_VERSION` before using newer version enums;
- queries `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` only for OpenGL 3.x+;
- queries GLSL version only for OpenGL 2.0+;
- records vendor, renderer, OpenGL version, GLSL when supported and context profile;
- refuses modern activation below OpenGL 4.6;
- refuses non-compatibility/core-only activation;
- loads the official Diligent OpenGL backend DLL;
- obtains `IEngineFactoryOpenGL`;
- installs the Diligent message callback;
- optionally enables Diligent validation through `MU_MODERN_GL_DEBUG=1`;
- calls `AttachToActiveGLContext`;
- records ownership as `Attached` only after success.

The old-OpenGL query ordering is deliberate: a fallback path must not inject `GL_INVALID_ENUM` merely by asking for enums unavailable on the host context.

The Main does not install a competing raw `glDebugMessageCallback`. Diligent remains owner of its validation/KHR_debug callback path.

Diagnostics are persisted in `ModernGraphics.log` beside `Main.exe` and also sent through `OutputDebugStringA`.

## Dependency/build state

DiligentCore is pinned in `DILIGENT_PIN.md` and prepared by:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

The setup verifies DiligentCore `v2.5.6` at commit `b036337d68be2353c9950a85929acf796b9a6d50`, initializes recursive submodules, configures Win32 OpenGL-only with HLSL support, builds `_32r` and `_32d` backend DLLs and copies them into `Client_2`.

`ModernGraphicsBootstrap.h` auto-enables the Diligent integration for MSVC/Win32 when the prepared header is present. Main Debug maps its existing `DEBUG` configuration to Diligent's debug public definitions. `source/Directory.Build.targets` keeps the necessary Debug/Win32 C++17 normalization scoped to Main rather than changing Diligent's upstream projects.

The authoritative latest Windows/x86 gate and commit are recorded only in `STATUS.md`.

## Reproducible runtime evidence

`SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1` is the target-machine gate.

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

The real path checks runtime prerequisites, removes stale evidence, launches `Main.exe`, requires a normal exit and validates complete ordered modern lifecycles.

For every successful lifecycle, evidence must show:

`attach attempt -> GL diagnostics -> compatibility profile -> optional debug routing -> attach -> teardown barrier -> shutdown`

The parser:

- supports multiple complete lifecycles for real WGL recreation;
- requires compatibility profile independently per successful lifecycle;
- rejects incomplete/reordered cycles;
- rejects attach beginning while the previous teardown is unfinished;
- requires resize evidence to live inside an active lifecycle when requested;
- rejects any logged modern fallback in a run intended to certify the modern path.

`-ValidateOnly` is a parser/evidence mode for CI and does not substitute for a GPU run.

## Remaining target-GPU gate

A real GPU-backed Main run must still prove:

- backend DLL/factory loading in the actual client environment;
- effective OpenGL >= 4.6 compatibility profile;
- successful `AttachToActiveGLContext` against the real Main context;
- Diligent validation/debug routing without callback ownership conflict;
- resize stability;
- context/mode recreation behavior where available;
- clean teardown/shutdown ordering;
- legacy presentation/rendering without coexistence regression.

Until those checks pass on the target Windows/GPU machine, Phase 2 is **repository/build-complete after its current CI gate, but not GPU runtime-certified**.
