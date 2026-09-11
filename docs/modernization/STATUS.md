# Modernization Status

## Active target

Diligent integration for Main 5.2, using shared HLSL shaders and OpenGL 4.6 first. Vulkan and Direct3D 11 remain future active backends using the same game-side contracts.

## Phase 1 — complete

Static discovery/mapping is complete for the initial Hero, remote player, BotBuffer, NPC/monster, world-object, equipment and inventory-preview render paths. Geometry, pose, instance, view, material, texture, snapshot/lifetime and legacy render-state contracts were recorded in [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).

## Phase 2 repository/build state — complete

The repository contains the OpenGL 4.6 coexistence bootstrap, real Main lifecycle bridge, compatibility-profile checks, persistent `ModernGraphics.log` diagnostics, explicit Diligent OpenGL backend-DLL loading, pinned DiligentCore setup, validation/debug routing, local GPU validation script and Windows/x86 CI build/evidence gate.

Ownership remains unchanged: legacy Main owns `HWND/HDC/HGLRC` and the only presentation path; Diligent attaches to the active WGL context and does not create another swap chain or `Present`/`SwapBuffers` owner.

DiligentCore is pinned to official `v2.5.6` commit `b036337d68be2353c9950a85929acf796b9a6d50`. Release/Win32 uses C++17 directly in `Main.vcxproj`; Debug/Win32 is effectively normalized to C++17 by the Main-scoped `Directory.Build.targets`, leaving DiligentCore project language settings untouched.

Prior Phase 2 Windows gates successfully built the pinned Release/Debug OpenGL backend DLLs and Main Release/Debug x86 configurations, validated the lifecycle evidence parser, and published a target-GPU runtime bundle. Detailed bootstrap/runtime evidence remains in [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md), [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md) and [PHASE2_COMPLETENESS_AUDIT.md](PHASE2_COMPLETENESS_AUDIT.md).

## Phase 2 GPU runtime boundary — still pending

Phase 2 is **not runtime-certified**. The real interactive Windows/OpenGL 4.6 compatibility-profile test is still required to prove backend load/attach, debug routing, resize stability, single presentation ownership, clean shutdown/lifetime and visual legacy regression behavior on the target GPU.

The user explicitly authorized repository-side Phase 3 work to proceed while this GPU test is unavailable. That authorization does not mark the Phase 2 GPU checkbox as passed.

The existing validation command remains:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

## Phase 3 renderer core — repository implementation present

Phase 3 core infrastructure is now represented in source. See [PHASE3_RENDERER_CORE.md](PHASE3_RENDERER_CORE.md).

Implemented repository-side contracts:

- `ModernRendererTypes.h` defines backend-neutral render view/pass identity plus Frame/Instance/Material constant structures.
- The initial BMD GPU vertex contract is locked to 40 bytes with compile-time offset/size checks: Position, Normal, UV, PositionBone, NormalBone and OriginalVertexId.
- `IModernRendererBackendAdapter` defines the renderer/backend boundary.
- `CDiligentOpenGL46Adapter` consumes the Phase 2 Diligent device/context. Vulkan and Direct3D 11 are explicit future selections and are not silently activated.
- `CModernGPUBuffer` and `CModernConstantBuffer` provide resource/update wrappers.
- `CModernShaderManager` provides cached shared-HLSL shader creation.
- `CModernTextureSamplerManager` provides texture-SRV and sampler lifecycle/cache.
- `CModernPipelineResourceCache` provides PSO/SRB cache ownership.
- `ModernIndexedDrawSubmission` plus `CModernRendererCore::SubmitIndexed()` define the indexed draw boundary and preserve the raw-GL -> Diligent cache invalidation contract.
- The core is compiled through the existing `CShaderGL.h` path without adding another Visual Studio project source item or presentation owner.

Runtime activation remains intentionally gated: Phase 3 does not yet redirect a production BMD draw, upload a Skeleton Texture, or replace any Hero/player/Bot/NPC/monster render path. The legacy renderer therefore remains authoritative.

## Phase 3 validation still pending

- Windows/x86 Release compile after the new Phase 3 source changes.
- Windows/x86 Debug compile after the new Phase 3 source changes.
- First production HLSL PSO/SRB/input-layout creation.
- First persistent BMD vertex/index upload.
- First production indexed modern draw and raw-GL/Diligent coexistence check.
- Pose/Skeleton Texture conversion and addressing.
- Two-independent-instance BMD proof before broad player/NPC migration.

## Phase 3 continuation — BMD geometry

The source now has a validated CPU BMD converter, a real `BMD::BuildModernMesh` entry point and an immutable vertex/index cache keyed by asset revision. The bootstrap releases the core caches before the attached Diligent context/device. Portable geometry tests passed locally and were added to the Windows x86 workflow. Details and the caller contract are in [PHASE3_RENDERER_CORE.md](PHASE3_RENDERER_CORE.md).

This is an asset-preparation building block. No production BMD draw has been redirected, and PSO/SRB, explicit render-target/view binding, pose upload and target-GPU parity remain pending. Windows compilation of this continuation must be checked separately.

## Next action

1. Run/inspect the Windows/x86 compile gate for the Phase 3 source changes.
2. Build the first BMD model pipeline on the new core: persistent vertex/index buffers, HLSL VS/PS, input layout, PSO/SRB, Frame/Instance/Material constant buffers and texture/sampler binding.
3. Keep the legacy draw as fallback and do not expand to remote players/BotBuffer/NPC/monster until the two-independent-instance proof is stable.
4. When access to the target GPU is available, execute the deferred Phase 2 runtime certification before calling the OpenGL coexistence boundary runtime-complete.
