# Modernization Status

## Active target

Diligent integration for Main 5.2, using shared HLSL shaders and OpenGL 4.6 first. Vulkan and Direct3D 11 remain future active backends using the same contracts.

## Completed

- Phase 1 static source mapping against `13cfc7c3e5dab042e1c3e8f4184e18c26dd08eff`.
- Initial call paths: Hero, remote players, BotBuffer, NPC/monsters, world objects, equipment and inventory preview.
- Geometry, pose, instance, view, material and texture producer/consumer contracts.
- Snapshot/lifetime requirements and legacy render-state mapping.
- Roadmap aligned with the Diligent + HLSL decision.
- Phase 2 ownership contract documented: legacy Main owns HWND/HDC/HGLRC and presentation; Diligent attaches to the active GL context.
- Real Main lifecycle located: `CreateOpenglWindow()`, `CWINHANDLE::WndProc`, `MainScene()` / `LoadingScene()` and `KillGLWindow()`.
- Phase 2 source bridge implemented: context attach discovery, resize forwarding and pre-destroy shutdown without adding another present path.
- OpenGL 4.6 capability and vendor/renderer/version/GLSL/profile diagnostics implemented in `CModernGraphicsBootstrap`.
- DiligentCore pinned to official `v2.5.6` commit `b036337d68be2353c9950a85929acf796b9a6d50`; reference shader/resource snapshot pinned separately in `DILIGENT_PIN.md`.
- Diligent backend integration changed to the official Win32 explicit-DLL model rather than statically linking the OpenGL engine into Main.
- Release/Debug backend names are aligned with the official loader: `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`.
- Main Debug's existing `DEBUG` macro is bridged to Diligent's debug public definitions so the correct backend suffix is selected.
- Reproducible dependency/build script added at `SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`.
- Generated Diligent checkout/build folders are excluded through `SRCMainGS/Source/Main5.2/.gitignore`.
- Dependency-absent, unsupported-GL and backend-DLL-load failures retain the legacy renderer path.
- Windows/x86 compile gate added at `.github/workflows/phase2-win32-build.yml`; it prepares the pinned Diligent backend and builds Main Debug/x86 when GitHub Actions executes it.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).
Phase 2 contract: [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md).
Runtime lifecycle map: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).
Dependency pin/setup model: [DILIGENT_PIN.md](DILIGENT_PIN.md).

## Phase 2 repository state

The Phase 2 bootstrap is represented in source and build infrastructure. The repository contains the lifecycle bridge, capability checks, explicit backend loader, exact dependency pin, reproducible setup/build script and a Windows/x86 CI compile gate.

This means the previous repository-side dependency wiring gap has been closed without adding a hard Diligent `.lib` dependency to `Main.vcxproj`.

## Phase 2 still in progress — validation boundary

Phase 2 is **not runtime-certified**. This environment has not run a GPU-backed Main instance. At the time of this status update, the newly added GitHub Actions workflow has not yet produced a recorded workflow run, so CI compilation is also not marked as passed.

Remaining validation work:

- obtain a successful Windows/x86 CI or local build using `setup_diligent_opengl46.ps1`;
- verify the exact pinned checkout/submodules configure successfully;
- produce the expected OpenGL backend DLL for the configuration under test;
- compile `Main.sln` Win32/x86 with the Diligent headers detected;
- launch the client on a system exposing OpenGL >= 4.6;
- confirm the DLL/factory loads and `AttachToActiveGLContext` succeeds;
- validate resize stability;
- prove there is still exactly one presentation owner (`SwapBuffers` legacy path);
- validate clean shutdown/lifetime ordering;
- verify legacy scenes render without regression.

The current `WH_CALLWNDPROC` integration remains intentionally a temporary coexistence bridge. It is source-complete for the bootstrap, but after the runtime gate it should be reassessed and normally replaced by direct calls in the already-mapped lifecycle owners if no compatibility reason requires retaining it.

## Not yet implemented

The following work is downstream of the Phase 2 bootstrap/runtime gate and must not be confused with bootstrap completion:

- concrete CPU/GPU vertex and constant-buffer layouts;
- Diligent buffer/texture resource lifecycle;
- shared HLSL production shader compilation path;
- Skeleton Texture upload/addressing in Main;
- material/state binding and draw submission;
- two-independent-instance BMD proof;
- Hero/remote/Bot/NPC/monster modern-render parity;
- terrain/effects/UI migration;
- Vulkan and Direct3D 11 activation.

## Next action

Prefer the repository CI gate if GitHub Actions is enabled. For local Windows validation, run from `SRCMainGS/Source/Main5.2` in a Visual Studio developer environment:

```powershell
powershell -ExecutionPolicy Bypass -File .\setup_diligent_opengl46.ps1 -BuildMain
```

Then launch `Client_2/Main.exe` and capture the `[ModernGraphics]` diagnostics. Once the Phase 2 runtime gate passes, start the concrete CPU/GPU data contracts and the two-instance BMD proof.
