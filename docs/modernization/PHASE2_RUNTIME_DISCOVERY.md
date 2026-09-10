# Phase 2 Runtime Discovery

Date: 2026-09-10
Branch: `modernization`

This document records the real Main 5.2 lifecycle points used by the Phase 2 OpenGL 4.6/Diligent bootstrap. Phase 2 is still not runtime-complete until the Windows validation gate in `PHASE2_OPENGL46_BOOTSTRAP.md` passes.

## Confirmed Main lifecycle

### Window ownership

`SRCMainGS/Source/Main5.2/source/WINHANDLE.cpp`

- `CWINHANDLE::Create(...)` creates the client `HWND`.
- the window class uses `CS_OWNDC`, so the window keeps a stable device context relationship suitable for the coexistence bridge.
- `CWINHANDLE::winLoop()` calls `Scene(g_hDC)` from the main/UI thread.

### OpenGL context ownership

`SRCMainGS/Source/Main5.2/source/Winmain.cpp`

`CreateOpenglWindow()` currently owns the legacy Win32/WGL initialization:

1. `GetDC()` stores `g_hDC`;
2. `ChoosePixelFormat` / `SetPixelFormat` configure a double-buffered window surface;
3. `wglCreateContext()` stores `g_hRC`;
4. `wglMakeCurrent(g_hDC, g_hRC)` makes it current;
5. `glewInit()` initializes extension entry points.

The context is therefore externally created from Diligent's point of view. Phase 2 must use `AttachToActiveGLContext`; it must not create a second OpenGL context or swap chain.

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

The Phase 2 lifecycle bridge now observes `WM_SIZE` on the same UI thread and calls `CModernGraphicsBootstrap::OnResize(width, height)` for the attached window when the new size is non-zero and not minimized.

### Shutdown

`CWINHANDLE::WndProc` handles `WM_CLOSE` / `WM_DESTROY` and then calls `KillGLWindow()`. `KillGLWindow()` unbinds/deletes `g_hRC` and releases `g_hDC`.

The Phase 2 bridge observes `WM_CLOSE`, `WM_DESTROY` and `WM_NCDESTROY` **before** the legacy WndProc processes them and calls `CModernGraphicsBootstrap::Shutdown()`. This releases the Diligent immediate context/device while the external WGL context is still alive.

## Coexistence bridge

The first runtime wiring is compiled through `CShaderGL.cpp`, which is already part of `Main.vcxproj`. When `MU_ENABLE_DILIGENT` is enabled, a thread-local `WH_CALLWNDPROC` hook observes only the Main UI thread and attaches Diligent after the real legacy WGL context becomes current.

This is a bootstrap bridge, not a replacement window system. It deliberately does **not**:

- subclass the window;
- replace the legacy WndProc;
- hook or replace `SwapBuffers`;
- create a second WGL context;
- create a Diligent swap chain;
- move BMD/terrain/effects/UI to the modern renderer yet.

Once Diligent is vendored and the project wiring is stable, these lifecycle calls may be moved directly into the owning WinMain/WINHANDLE code without changing the `CModernGraphicsBootstrap` public contract.

## Capability and diagnostics

`CModernGraphicsBootstrap::InitializeOpenGL46()` now:

- verifies the supplied `HWND/HDC/HGLRC` are valid;
- verifies the supplied WGL context/DC are current;
- reads/parses the effective OpenGL major/minor version;
- logs vendor, renderer, OpenGL version, GLSL version and context profile;
- refuses modern activation below OpenGL 4.6;
- attaches Diligent only through `AttachToActiveGLContext`;
- records ownership as `Attached` after successful Diligent attachment.

The existing `CErrorReport::WriteOpenGLInfo()` remains the application's persistent OpenGL information log. Phase 2's `OutputDebugStringA` diagnostics add the modern-backend decision/profile details without taking over the legacy logger.

## Dependency state

DiligentCore is pinned in `DILIGENT_PIN.md`, but the dependency is not yet wired into `Main.vcxproj`. `MU_ENABLE_DILIGENT` therefore remains off by default and the current legacy build path remains unchanged.

## Remaining runtime gate

A real Windows Win32 build/run must still prove:

- pinned Diligent headers/libs compile and link;
- effective OpenGL version is >= 4.6 on the test system;
- the attach succeeds;
- resize remains stable;
- only the legacy `SwapBuffers` presents;
- shutdown has no lifetime/context errors;
- legacy scenes render without regression.