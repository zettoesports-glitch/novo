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

Repository-side Phase 3 work is allowed to proceed while this GPU test is unavailable. That does not mark the Phase 2 GPU checkbox as passed.

The existing validation command remains:

```powershell
powershell -ExecutionPolicy Bypass -File .\run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize
```

## Phase 3 renderer core — repository implementation present

Phase 3 has moved beyond the generic renderer core and now contains the first BMD production-pipeline building blocks. See [PHASE3_RENDERER_CORE.md](PHASE3_RENDERER_CORE.md).

Implemented repository-side contracts now include:

- backend-neutral Frame/Instance/Material constants and the locked 40-byte `ModernBMDVertex` ABI;
- explicit Diligent backend adapter boundary with OpenGL 4.6 active and Vulkan/D3D11 reserved;
- generic GPU/constant-buffer wrappers, shared-HLSL shader cache, texture/sampler ownership, PSO/SRB cache and indexed submission boundary;
- validated CPU BMD conversion plus immutable vertex/index GPU mesh cache keyed by asset revision;
- first shared-HLSL textured BMD pipeline with Frame/Instance/Material constant buffers, explicit input layout, depth/blend/cull PSO state, Skeleton Texture SRV and diffuse texture/sampler binding;
- NextMU-style Skeleton Texture atlas upload path using two `float4` texels per bone and per-pose addressing;
- legacy `BoneMatrix[200][3][4]` to quaternion + translation/BoneScale packing, with finite-data, singular/reflected-basis and allocation failure rejection;
- verified row-major bone convention: legacy `VectorRotate` consumes matrix rows, matching the current matrix-to-quaternion conversion and shared-HLSL rotation path;
- explicit pose-space safety: the first modern BMD shader accepts only poses whose `BodyScale/BodyOrigin` remain separate per-instance data. Poses with the body transform already baked into `BoneMatrix` are rejected for legacy fallback, preventing double application.

The production shader still applies skinning first and then `BodyScale/BodyOrigin`, matching the ordinary legacy `Transform(..., Translate=true)` path when the snapshot was produced without the body transform baked into the bone matrices.

## Phase 3 validation boundary — still pending

The modern components above are source/build contracts, not proof of a live migrated model. The legacy renderer remains authoritative because no Hero/player/BotBuffer/NPC/monster production draw has been redirected yet.

Still required before calling the first BMD migration complete:

- exercise the BMD vertex/index cache and Skeleton Texture atlas on the real attached Diligent device from a production model path;
- bind explicit render targets/viewport and execute the first indexed `CModernRendererCore::SubmitIndexed()` draw;
- verify raw-GL -> Diligent -> legacy-GL state coexistence around that draw;
- prove two independent live instances of the same BMD do not share pose/instance state;
- compare the local Hero against the legacy renderer before expanding to remote player, BotBuffer, NPC or monster paths;
- run the deferred target-GPU Phase 2 certification.

## Verified legacy transform convention

The audit of the actual Main math/animation path established the boundary used by the modern pose contract:

- `VectorRotate(in, matrix, out)` evaluates `Dot(in, matrix[row])`, so the 3x3 bone basis is consumed row-major and must not be transposed during quaternion conversion.
- `R_ConcatTransforms` composes the same 3x4 affine representation used by `BMD::Animation`.
- `BMD::Transform` applies `BoneScale` during the bone transform and, when `Translate=true`, applies `BodyScale` and then adds `BodyOrigin`.
- `Calc_RenderObject` passes `!Translate` to `BMD::Animation`; therefore some legacy call paths can produce matrices where the body transform is already baked. Those matrices are intentionally not accepted by the first modern pose path yet.

This removes the matrix-transpose ambiguity and turns the body-transform distinction into an explicit fallback gate instead of an implicit assumption.

## Next action

1. Preserve the current Windows/x86 Release/Debug, geometry and skeleton-pose regression gates.
2. Wire one deliberately selected production BMD path into the existing geometry cache + Skeleton Texture atlas + `CModernBMDPipeline`, with explicit render targets/viewport and legacy fallback on every unsupported/failed condition.
3. Start with a path whose pose is known to use `BodyTransformSeparate`; do not normalize body-baked poses silently.
4. Prove two independent instances, then validate local Hero parity before enabling remote player/BotBuffer/NPC/monster migration.
5. When target-GPU access is available, execute the deferred Phase 2 runtime certification before calling the OpenGL coexistence boundary runtime-complete.
