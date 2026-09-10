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
- Persistent runtime evidence is written to `Client_2/ModernGraphics.log` beside `Main.exe`, independent of the launcher's working directory, in addition to `OutputDebugStringA`.
- `MU_MODERN_GL_DEBUG=1` enables Diligent validation; the Diligent factory message callback persists Diligent/OpenGL diagnostic messages without installing a competing raw `glDebugMessageCallback` in Main.
- DiligentCore pinned to official `v2.5.6` commit `b036337d68be2353c9950a85929acf796b9a6d50`; reference shader/resource snapshot pinned separately in `DILIGENT_PIN.md`.
- Diligent backend integration uses the official Win32 explicit-DLL model rather than statically linking the OpenGL engine into Main.
- Release/Debug backend names are aligned with the official loader: `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`.
- Main Debug's existing `DEBUG` macro is bridged to Diligent's debug public definitions so the correct backend suffix is selected.
- Reproducible dependency/build script exists at `SRCMainGS/Source/Main5.2/setup_diligent_opengl46.ps1`.
- Reproducible local GPU validation script exists at `SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1`.
- `-ValidateOnly` validates synthetic/existing evidence without requiring runtime binaries; the real launch path still verifies `Main.exe` and both backend DLLs.
- Generated Diligent checkout/build folders are excluded through `SRCMainGS/Source/Main5.2/.gitignore`; runtime evidence `Client_2/ModernGraphics.log` is ignored at repository root.
- Dependency-absent, unsupported-GL and backend-DLL-load failures retain the legacy renderer path.
- Windows/x86 compile gate exists at `.github/workflows/phase2-win32-build.yml`; it performs an early PowerShell syntax preflight and validates the runtime-evidence parser with synthetic log data after both builds.
- GitHub Actions run `34533022717` successfully prepared the exact pinned Diligent checkout/submodules, built both `GraphicsEngineOpenGL_32r.dll` and `GraphicsEngineOpenGL_32d.dll`, compiled `Main.sln` as Release/x86 with C++17 and produced `Client_2/Main.exe` with 0 build errors.
- GitHub Actions run `34533868904` successfully compiled `Main.sln` as Debug/x86 after the Main-only C++17 normalization, with 0 build errors.
- Final combined GitHub Actions run `34538658227` at source/workflow commit `35cffa30b904071dcdf8a086a6ed5e03daca0342` passed the PowerShell preflight, pinned Diligent preparation, Release/x86 build/output verification, Debug/x86 build/output verification and synthetic runtime-evidence validation.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).
Phase 2 contract: [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md).
Runtime lifecycle map: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).
Dependency pin/setup model: [DILIGENT_PIN.md](DILIGENT_PIN.md).

## Phase 2 repository/build state — complete

The Phase 2 bootstrap is represented in source and build infrastructure. The repository contains the lifecycle bridge, capability checks, explicit backend loader, exact dependency pin, reproducible setup/build script, persistent runtime diagnostics, Diligent-owned validation/debug routing, a reproducible GPU validation script and a Windows/x86 CI compile/evidence gate.

The final combined Release+Debug x86 gate passed in workflow run `34538658227`. Repository-side source/build/synthetic-evidence work for the Phase 2 bootstrap is therefore closed.

## Phase 2 still in progress — GPU runtime boundary

Phase 2 is **not runtime-certified**. GitHub Actions proves compilation, generated outputs and evidence-parser behavior, but it does not launch the MU client in the real interactive GPU/window environment required to validate WGL/Diligent coexistence.

Build validation state:

- Release/x86: **passed** independently in run `34533022717` and again in the final combined run `34538658227`.
- Debug/x86: **passed** independently in run `34533868904` and again in the final combined run `34538658227`.
- PowerShell syntax preflight: **passed** in run `34538658227`.
- Combined permanent Release+Debug x86 + synthetic runtime-evidence validation gate: **passed** in run `34538658227`.

Remaining GPU runtime work:

- launch the client on a Windows system exposing OpenGL >= 4.6;
- confirm the expected backend DLL/factory loads and `AttachToActiveGLContext` succeeds;
- capture vendor/renderer/version/GLSL/profile/backend evidence in `Client_2/ModernGraphics.log`;
- with `MU_MODERN_GL_DEBUG=1`, confirm Diligent validation/OpenGL debug routing activates without callback ownership conflicts;
- validate resize stability;
- prove there is still exactly one presentation owner (`SwapBuffers` legacy path);
- validate clean shutdown/lifetime ordering;
- verify legacy scenes render without regression.

The local validation command from `SRCMainGS/Source/Main5.2` is:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

The script starts the client, waits for a normal close, then validates the required attach/diagnostic/debug-routing/resize/shutdown log markers and rejects any modern-backend fallback recorded in the log.

The current `WH_CALLWNDPROC` integration remains intentionally a temporary coexistence bridge. After the GPU runtime gate it should be reassessed and normally replaced by direct calls in the already-mapped lifecycle owners if no compatibility reason requires retaining it.

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

1. Execute the local GPU runtime gate with `run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize` on the target Windows/OpenGL 4.6 machine and retain `Client_2/ModernGraphics.log` as evidence.
2. If the GPU gate passes, replace/reassess the temporary `WH_CALLWNDPROC` bridge using the already-mapped direct lifecycle owners.
3. Only after that GPU gate should Phase 2 be called runtime-complete and work move into concrete CPU/GPU contracts, pose conversion, Skeleton Texture and the first two-instance modern BMD proof.
