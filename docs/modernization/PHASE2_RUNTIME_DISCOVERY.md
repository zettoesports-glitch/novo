# Phase 2 Runtime Discovery

Date: 2026-09-10
Branch: `modernization`

This document records the real Main 5.2 lifecycle points used by the Phase 2 OpenGL 4.6/Diligent bootstrap. Phase 2 is still not runtime-complete until the Windows validation gate in `PHASE2_OPENGL46_BOOTSTRAP.md` passes.

## Confirmed Main lifecycle

### Window ownership

`SRCMainGS/Source/Main5.2/source/WINHANDLE.cpp`

- `CWINHANDLE::Create(...)` creates the client `HWND`.
- the window class uses `CS_OWNDC`, so the window keeps a stable device-context relationship suitable for the coexistence bridge.
- `CWINHANDLE::winLoop()` calls `Scene(g_hDC)` from the main/UI thread.

### OpenGL context ownership

`SRCMainGS/Source/Main5.2/source/Winmain.cpp`

`CreateOpenglWindow()` currently owns the legacy Win32/WGL initialization:

1. `GetDC()` stores `g_hDC`;
2. `ChoosePixelFormat` / `SetPixelFormat` configure a double-buffered window surface;
3. `wglCreateContext()` stores `g_hRC`;
4. `wglMakeCurrent(g_hDC, g_hRC)` makes it current;
5. `glewInit()` initializes extension entry points.

The context is therefore externally created from Diligent's point of view. Phase 2 uses `AttachToActiveGLContext`; it does not create a second OpenGL context or swap chain.

Important: the current code uses legacy `wglCreateContext()` rather than explicitly requesting a 4.6 context. The bootstrap therefore validates the **effective** GL version and remains inactive if the driver does not expose >= 4.6. Creating an explicit 4.6 compatibility context can be evaluated later, before asset creation, but must not invalidate legacy GL objects.

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

`LoadingScene(HDC)` also owns presentation while that scene is active. No Phase 2 code calls `SwapBuffers`, and no Diligent swap chain is created. Therefore there remains exactly one application-owned presentation path per rendered scene frame.

### Resize

`CWINHANDLE::WndProc` receives `WM_SIZE`. Before Phase 2 it only handled minimization state and did not notify a modern renderer.

The current Phase 2 lifecycle bridge observes `WM_SIZE` on the same UI thread and calls `CModernGraphicsBootstrap::OnResize(width, height)` for the attached window when the new size is non-zero and not minimized.

### Shutdown

`CWINHANDLE::WndProc` handles `WM_CLOSE` / `WM_DESTROY` and then calls `KillGLWindow()`. `KillGLWindow()` unbinds/deletes `g_hRC` and releases `g_hDC`.

The Phase 2 bridge observes `WM_CLOSE`, `WM_DESTROY` and `WM_NCDESTROY` before the legacy WndProc processes them and calls `CModernGraphicsBootstrap::Shutdown()`. This releases the Diligent immediate context/device while the external WGL context is still alive.

## Coexistence bridge

The first runtime wiring is compiled through `CShaderGL.cpp`, which is already part of `Main.vcxproj`. When `MU_ENABLE_DILIGENT` is enabled, a `WH_CALLWNDPROC` hook observes the Main UI thread and attaches Diligent after the real legacy WGL context becomes current.

This is a bootstrap bridge, not a replacement window system. It deliberately does **not**:

- subclass the window;
- replace the legacy WndProc;
- hook or replace `SwapBuffers`;
- create a second WGL context;
- create a Diligent swap chain;
- move BMD/terrain/effects/UI to the modern renderer yet.

The hook is still a **temporary Phase 2 bridge**. It has not been presented as the final window-system architecture. After the Windows gate is proven, initialization/resize/shutdown may be moved directly into `CreateOpenglWindow()`, `CWINHANDLE::WndProc` and the pre-`KillGLWindow()` path without changing `CModernGraphicsBootstrap`'s public contract.

## Capability and diagnostics

`CModernGraphicsBootstrap::InitializeOpenGL46()`:

- verifies the supplied `HWND/HDC/HGLRC` are valid;
- verifies the supplied WGL context/DC are current;
- reads/parses the effective OpenGL major/minor version;
- logs vendor, renderer, OpenGL version, GLSL version and context profile;
- refuses modern activation below OpenGL 4.6;
- loads the Diligent OpenGL backend through the official Win32 DLL loader;
- obtains `IEngineFactoryOpenGL` from the backend module;
- attaches Diligent through `AttachToActiveGLContext`;
- records ownership as `Attached` after successful attachment.

The existing `CErrorReport::WriteOpenGLInfo()` remains the application's persistent OpenGL information log. Phase 2's `OutputDebugStringA` diagnostics add the modern-backend decision/profile details without taking over the legacy logger.

## Dependency/build state

DiligentCore is pinned in `DILIGENT_PIN.md` and the repository now contains a reproducible setup path:

`SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`

The script is designed to:

- clone/verify DiligentCore `v2.5.6` at commit `b036337d68be2353c9950a85929acf796b9a6d50`;
- initialize the required recursive submodules;
- configure a Win32 OpenGL-only build with HLSL support;
- build `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`;
- copy both modules into `Client_2`;
- optionally build `Main.sln` through `-BuildMain`.

The generated Diligent checkout/build directories are ignored by `Main5.2/.gitignore`.

`ModernGraphicsBootstrap.h` now auto-enables `MU_ENABLE_DILIGENT` for MSVC/Win32 when the expected local Diligent OpenGL header exists. Main's `DEBUG` configuration is mapped to Diligent's public debug definitions so the official loader selects the debug backend; Release selects the release backend.

No Diligent engine import library is required by the Main for this Phase 2 path: the backend is loaded dynamically.

### Important validation boundary

The setup script and loader integration are **committed source/build infrastructure**, but this environment has not executed a Visual Studio Win32 build or a GPU run. Therefore none of the following may be marked runtime-proven yet.

## Remaining runtime gate

A real Windows Win32 build/run must still prove:

- the pinned setup completes successfully on the target development machine;
- both backend DLLs can be produced (or at minimum the configuration actually being tested);
- `Main.sln` compiles with the pinned Diligent headers;
- the expected backend DLL is found and its factory export loads;
- effective OpenGL version is >= 4.6 on the test system;
- `AttachToActiveGLContext` succeeds;
- resize remains stable;
- only the legacy `SwapBuffers` presents;
- shutdown has no lifetime/context errors;
- legacy scenes render without regression.

Until those checks pass, Phase 2 is source/build-path complete but **not runtime-certified**.
