# Implementation Checklist

Static discovery evidence: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). Checked discovery items mean source inspection, not GPU runtime validation. Phase 2 runtime notes: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).

## Discovery
- [x] Locate legacy BMD/model render entry points
- [x] Locate existing BMD shader/VBO experiment (`CShaderGL`, `Mesh_t`, `RenderVertexBuffer`)
- [x] Locate GLSL/HLSL shader sources already committed
- [x] Locate NextMU/reference renderer and shader material
- [x] Trace initial local-player render path end-to-end (static)
- [x] Trace initial remote-player/BotBuffer/NPC render paths (static)

## Documented contracts
- [x] Initial producer/consumer map for geometry, pose, view, instance and material
- [x] Snapshot ownership rules for shared BMD state and temporary inventory objects
- [x] Diligent + shared HLSL architecture decision recorded
- [ ] Implement and verify concrete CPU/GPU layouts and pose conversion

## OpenGL 4.6 — Phase 2 bootstrap
- [x] Define Win32/WGL ownership: Main owns `HWND/HDC/HGLRC`; Diligent attaches only
- [x] Preserve legacy presentation ownership; no Diligent swap chain or modern present path exists in Phase 2
- [x] Locate and wire real Main window/context/resize/teardown integration points
- [x] Pin DiligentCore and reference shader/resource revisions (`DILIGENT_PIN.md`)
- [x] Validate effective OpenGL >= 4.6 and record vendor/renderer/version/GLSL/profile
- [x] Require compatibility profile; reject core-only contexts with safe legacy fallback
- [x] Add reproducible pinned DiligentCore Win32/OpenGL/HLSL setup script
- [x] Use official Diligent explicit backend-DLL loading (`GraphicsEngineOpenGL_32r/32d.dll`)
- [x] Preserve dependency-absent, unsupported-GL, core-only-profile and DLL-load-failure legacy fallback
- [x] Align Main Debug definitions with Diligent debug backend selection
- [x] Normalize Main Debug/x86 to C++17 without changing DiligentCore project settings
- [x] Add Windows/x86 GitHub Actions compile gate
- [x] Build pinned Diligent OpenGL Release/Debug backend DLLs on Windows CI
- [x] Compile `Main.sln` Release/x86 with Diligent enabled
- [x] Compile `Main.sln` Debug/x86 with Diligent enabled
- [x] Cover known post-attach teardown messages (`WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY`, `WM_USER_MEMORYHACK`)
- [x] Confirm `KillGLWindow()` calls inside `CreateOpenglWindow()` are pre-attach failure cleanup paths
- [x] Prevent reattach to the same tracked `HWND/HGLRC` after teardown starts
- [x] Permit a genuinely recreated window/context pair to start a new complete lifecycle
- [x] Forward resize/teardown only while a real modern attachment is active
- [x] Add persistent `Client_2/ModernGraphics.log` lifecycle/capability evidence
- [x] Route Diligent validation/OpenGL debug messages through `IEngineFactory::SetMessageCallback()` when `MU_MODERN_GL_DEBUG=1`
- [x] Add reproducible local GPU validation script (`run_phase2_runtime_test.ps1`)
- [x] Require normal `Main.exe` exit and compatibility-profile evidence in the real GPU path
- [x] Validate lifecycle order: attach attempt -> diagnostics -> attach -> teardown barrier -> shutdown
- [x] Reject incomplete/reordered lifecycle evidence
- [x] Reject a new attach attempt while the previous teardown is still in progress
- [x] Accept multiple complete lifecycle cycles when WGL/window recreation legitimately occurs
- [x] Require resize evidence, when requested, to occur only inside an active modern lifecycle
- [x] Add positive and negative synthetic lifecycle tests to CI
- [x] Keep `-ValidateOnly` explicitly separate from real GPU certification
- [x] Clarify evidence boundary: source proves there is no modern present path; the current log parser does **not** count `SwapBuffers` calls per frame
- [ ] Pass the real Phase 2 Windows/GPU runtime gate: backend DLL/factory load, attach, GL >= 4.6 compatibility profile, debug routing, resize, clean teardown/lifetime and legacy visual regression check
- [ ] After the real GPU gate, reassess/replace the temporary `WH_CALLWNDPROC` bridge with direct lifecycle calls if no compatibility reason requires retaining it

## Renderer core — starts after the Phase 2 GPU gate
- [ ] Renderer device adapter/factory around Diligent
- [ ] Concrete CPU/GPU vertex layouts
- [ ] Frame/Object/Material constant-buffer contracts
- [ ] Buffer lifecycle
- [ ] Texture/sampler lifecycle
- [ ] Shared HLSL shader/program lifecycle
- [ ] Pipeline/render-state cache
- [ ] Resource binding and draw submission

## First modern draw
- [ ] Diligent buffer/texture transitions
- [ ] HLSL shader compilation diagnostics
- [ ] Constant-buffer upload/update path
- [ ] Texture/sampler binding
- [ ] Depth/blend/cull state handling
- [ ] Indexed/non-indexed draw submission

## BMD migration
- [x] Map legacy mesh fields and initial conversion rules (static)
- [ ] Define modern packed GPU vertex layout
- [ ] Persistent geometry upload
- [ ] Material bridge
- [ ] Pose conversion / Skeleton Texture upload and addressing
- [ ] Per-object transform/animation state
- [ ] Two independent instances of the same BMD without state leakage
- [ ] Local Hero parity
- [ ] Remote player parity
- [ ] BotBuffer parity
- [ ] NPC parity
- [ ] Monster parity

## Later passes
- [ ] Terrain renderer
- [ ] Effects/particles/joints
- [ ] Chrome/reflection/special-material variants
- [ ] UI/2D
- [ ] Vulkan backend activation
- [ ] DirectX 11 backend activation
