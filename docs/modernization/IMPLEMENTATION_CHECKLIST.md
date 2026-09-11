# Implementation Checklist

Static discovery evidence: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). Checked discovery items mean source inspection, not GPU runtime validation. Phase 2 runtime notes: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md). Corrective audit: [PHASE2_COMPLETENESS_AUDIT.md](PHASE2_COMPLETENESS_AUDIT.md). Phase 3 core: [PHASE3_RENDERER_CORE.md](PHASE3_RENDERER_CORE.md).

> Runtime gate note: the real Phase 2 Windows/GPU test remains pending. Repository-side Phase 3 work is allowed to continue while that test is unavailable. Checked Phase 3 items therefore mean implemented source/build contracts, not target-GPU certification.

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
- [x] Define concrete initial CPU/GPU vertex + constant-buffer layouts
- [x] Implement pose conversion / Skeleton Texture contract in Main source
- [x] Verify legacy row-major `vec34_t`/quaternion convention against `VectorRotate`/`QuaternionMatrix`
- [x] Make body-transform pose space explicit and reject body-baked matrices from the first modern shader path

## OpenGL 4.6 — Phase 2 bootstrap
- [x] Define Win32/WGL ownership: Main owns `HWND/HDC/HGLRC`; Diligent attaches only
- [x] Preserve legacy presentation ownership; no Diligent swap chain or modern present path exists in Phase 2
- [x] Audit the two legacy `SwapBuffers(hDC)` call sites and avoid claiming that the log parser counts them per frame
- [x] Locate and wire real Main window/context/resize/teardown integration points
- [x] Pin DiligentCore and reference shader/resource revisions (`DILIGENT_PIN.md`)
- [x] Validate effective OpenGL >= 4.6 and record vendor/renderer/version/GLSL/profile
- [x] Require compatibility profile; reject core-only contexts with safe legacy fallback
- [x] Avoid unsupported GL version/GLSL diagnostic queries on old contexts so fallback does not deliberately inject `GL_INVALID_ENUM`
- [x] Add reproducible pinned DiligentCore Win32/OpenGL/HLSL setup script
- [x] Use official Diligent explicit backend-DLL loading (`GraphicsEngineOpenGL_32r/32d.dll`)
- [x] Preserve dependency-absent, unsupported-GL, core-only-profile and DLL-load-failure legacy fallback
- [x] Do not emit a successful modern-shutdown marker after an initialization attempt that never attached
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
- [x] Clear the teardown generation after real WGL release so recycled numeric handles can represent a new context generation
- [x] Clear attempted-but-inactive bootstrap state when its tracked window is destroyed
- [x] Forward resize only while a real modern attachment is active
- [x] Add persistent `Client_2/ModernGraphics.log` lifecycle/capability evidence
- [x] Route Diligent validation/OpenGL debug messages through `IEngineFactory::SetMessageCallback()` when `MU_MODERN_GL_DEBUG=1`
- [x] Add reproducible local GPU validation script (`run_phase2_runtime_test.ps1`)
- [x] Require normal `Main.exe` exit and compatibility-profile evidence in the real GPU path
- [x] Validate lifecycle order: attach attempt -> diagnostics -> compatibility profile -> optional debug routing -> attach -> teardown barrier -> shutdown
- [x] Require compatibility-profile evidence independently for every successful recreated lifecycle
- [x] Reject incomplete/reordered lifecycle evidence
- [x] Reject a new attach attempt while the previous teardown is still in progress
- [x] Accept multiple complete lifecycle cycles when WGL/window recreation legitimately occurs
- [x] Require resize evidence, when requested, to occur only inside an active modern lifecycle
- [x] Add positive and negative synthetic lifecycle tests to CI, including mixed compatibility/core recreation
- [x] Keep `-ValidateOnly` explicitly separate from real GPU certification
- [x] Clarify evidence boundary: source proves there is no modern present path; the current log parser does **not** count `SwapBuffers` calls per frame
- [ ] Pass the real Phase 2 Windows/GPU runtime gate: backend DLL/factory load, attach, GL >= 4.6 compatibility profile, debug routing, resize, clean teardown/lifetime and legacy visual regression check
- [ ] After the real GPU gate, reassess/replace the temporary `WH_CALLWNDPROC` bridge with direct lifecycle calls if no compatibility reason requires retaining it

## Renderer core — Phase 3 repository implementation
- [x] Define backend adapter interface around Diligent
- [x] Keep backend selection explicit: OpenGL 4.6 active path; Vulkan/D3D11 reserved and unavailable for now
- [x] Lock the initial 40-byte BMD CPU/GPU vertex contract with compile-time offset/size checks
- [x] Define Frame/Instance/Material constant-buffer contracts with 16-byte alignment
- [x] Implement generic vertex/index GPU-buffer lifecycle wrapper
- [x] Implement constant-buffer lifecycle/update wrapper
- [x] Implement shared HLSL shader cache/creation manager
- [x] Implement texture SRV + sampler lifecycle/cache
- [x] Implement PSO/SRB resource cache
- [x] Implement indexed draw submission boundary
- [x] Keep renderer-core code independent of raw OpenGL calls
- [x] Compile the core through the existing Main build path without adding another project/presentation owner
- [x] Validate Phase 3 core + geometry changes in Windows/x86 Release and Debug CI/build ([925bbb5 evidence](https://github.com/zettoesports-glitch/novo/actions/runs/34615937928))
- [ ] Activate the core from the first migrated production draw

## First modern draw
- [x] Implement validated BMD conversion and immutable vertex/index upload/cache API
- [ ] Exercise vertex/index creation on a real Diligent device from the first production BMD path
- [x] Add first shared-HLSL production BMD VS/PS pipeline contract
- [x] Create/update Frame/Instance/Material constant-buffer path
- [x] Bind Skeleton Texture + diffuse texture/sampler resources through SRB
- [x] Define depth/blend/cull PSO state
- [x] Define Diligent input layout matching `ModernBMDVertex`
- [ ] Bind explicit production render targets and viewport for the attached-no-swapchain path
- [ ] Issue first indexed production draw through `CModernRendererCore::SubmitIndexed()`
- [ ] Validate both raw-GL -> Diligent and Diligent -> legacy-GL state coexistence around the first production modern draw

## BMD migration
- [x] Map legacy mesh fields and initial conversion rules (static)
- [x] Define initial 40-byte modern BMD vertex layout
- [x] Implement persistent geometry upload/cache by asset generation
- [x] Run portable CPU geometry regression tests (seams, bones, malformed data, uint32 expansion)
- [ ] Validate actual geometry upload and reuse on the target GPU
- [ ] Material bridge beyond the first textured pipeline
- [x] Implement pose conversion to quaternion + translation/BoneScale texels
- [x] Implement Skeleton Texture atlas upload/addressing contract (2 float4 texels per bone)
- [x] Validate independent CPU pose snapshots and reject non-finite/degenerate/reflected rotation data
- [x] Guard against double `BodyScale/BodyOrigin` by rejecting `BodyTransformBaked` snapshots from the first modern pipeline
- [ ] Route per-object transform/animation state into a production modern draw
- [ ] Two independent live instances of the same BMD without state leakage
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
