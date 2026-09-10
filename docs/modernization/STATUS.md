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
- Diligent backend integration uses the official Win32 explicit-DLL model rather than statically linking the OpenGL engine into Main.
- Release/Debug backend names are aligned with the official loader: `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`.
- Main Debug's existing `DEBUG` macro is bridged to Diligent's debug public definitions so the correct backend suffix is selected.
- Reproducible dependency/build script exists at `SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`.
- Generated Diligent checkout/build folders are excluded through `SRCMainGS/Source/Main5.2/.gitignore`.
- Dependency-absent, unsupported-GL and backend-DLL-load failures retain the legacy renderer path.
- Windows/x86 compile gate exists at `.github/workflows/phase2-win32-build.yml`.
- GitHub Actions run `34533022717` successfully prepared the exact pinned Diligent checkout/submodules, built both `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`, compiled `Main.sln` as Release/x86 with C++17 and produced `Client_2/Main.exe` with 0 build errors.
- The previous Debug/x86 run `34529126049` proved the Diligent builds themselves were healthy but exposed a legacy Main project mismatch: Debug used C++14 while the vendored sol2 headers require C++17.
- `source/Directory.Build.targets` now scopes a C++17 override to `Debug|Win32` for Main only; Debug/x86 CI validation is being run separately so this fix is not treated as proven until that gate succeeds.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).
Phase 2 contract: [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md).
Runtime lifecycle map: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).
Dependency pin/setup model: [DILIGENT_PIN.md](DILIGENT_PIN.md).

## Phase 2 repository state

The Phase 2 bootstrap is represented in source and build infrastructure. The repository contains the lifecycle bridge, capability checks, explicit backend loader, exact dependency pin, reproducible setup/build script and a Windows/x86 CI compile gate.

The repository-side dependency/build-path gap is closed for Release/x86: the pinned Diligent OpenGL backend and the real Main compile successfully together. This does not yet certify runtime GPU behavior.

## Phase 2 still in progress — runtime validation boundary

Phase 2 is **not runtime-certified**. GitHub Actions proves compilation and generated outputs, but it does not launch the MU client in the real GPU/window environment required to validate the WGL/Diligent coexistence path.

Build validation state:

- Release/x86: **passed** in run `34533022717` (`Main.exe`, `_32r.dll`, `_32d.dll` produced; 0 errors).
- Debug/x86: previous run failed before the modern integration because Main was compiled as C++14; C++17 normalization is committed and the replacement Debug gate remains pending until its workflow succeeds.

Remaining runtime work:

- launch the client on a system exposing OpenGL >= 4.6;
- confirm the expected backend DLL/factory loads and `AttachToActiveGLContext` succeeds;
- capture the real vendor/renderer/version/GLSL/profile diagnostics;
- validate resize stability;
- prove there is still exactly one presentation owner (`SwapBuffers` legacy path);
- validate clean shutdown/lifetime ordering;
- verify legacy scenes render without regression.

The current `WH_CALLWNDPROC` integration remains intentionally a temporary coexistence bridge. After the runtime gate it should be reassessed and normally replaced by direct calls in the already-mapped lifecycle owners if no compatibility reason requires retaining it.

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

Finish the Debug/x86 compile gate, then run `Client_2/Main.exe` on a Windows machine with an OpenGL 4.6-capable GPU and capture the `[ModernGraphics]` diagnostics. Only after that runtime gate passes should Phase 2 be called runtime-complete and the project move into concrete CPU/GPU contracts, pose conversion, Skeleton Texture and the first two-instance modern BMD proof.
