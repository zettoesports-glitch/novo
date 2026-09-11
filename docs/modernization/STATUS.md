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
- verified row-major bone convention and explicit `BodyTransformSeparate` safety gate;
- attached-WGL default-framebuffer proxy, explicit render-target/viewport submission contract, raw-GL state save/restore scope and non-owning legacy GL texture wrapper;
- scoped per-instance BMD render context carrying instance slot+generation, asset revision, view/pass, Skeleton slot and migration gate, with nested/sibling restoration regression coverage.

The production shader applies skinning first and then `BodyScale/BodyOrigin`, matching the ordinary legacy `Transform(..., Translate=true)` path when the snapshot was produced without the body transform baked into the bone matrices.

## Phase 3 production activation boundary — still pending

A fresh source audit confirms that `BMD::RenderMesh()` in `ZzzBMD.cpp` still has no `Modern*` call. The modern pipeline, atlas, target bridge, texture interop and state guard therefore exist as reusable infrastructure but are not yet traversed by a live Hero/player/BotBuffer/NPC/monster BMD draw.

This is now the real repository-side frontier. The previous checklist wording that made render-target/state infrastructure look absent was stale and has been corrected.

The first live migration still requires:

- install `CModernBMDRenderContextScope` at one deliberately selected high-level producer and carry the entity identity to the shared BMD boundary;
- exercise BMD vertex/index cache and Skeleton Texture atlas on the attached Diligent device;
- resolve/wrap the effective legacy diffuse texture and accept only the first supported textured material state;
- initialize/reuse the first `CModernBMDPipeline`, bind explicit default-framebuffer targets/viewport and execute `CModernRendererCore::SubmitIndexed()` inside the GL state guard;
- fall back to the untouched legacy draw on every unsupported or failed condition, with no double draw;
- prove two independent live instances of the same BMD do not share pose/instance state;
- compare local Hero visual parity before expanding to remote player, BotBuffer, NPC or monster paths.

## Current validation evidence

Portable C++17 validation for the new scoped BMD context passes with warnings treated as errors. The repository regression suite now registers `ModernBMDRenderContextTests` alongside geometry and skeleton-pose tests.

The Windows/x86 workflow for commit `cb3040c093529695b6767a87d79a961ae7b6c071` (Main type-check of the scoped context) was still running at the last audit; no pass is inferred until GitHub reports a completed successful conclusion. Documentation-only commits after that point do not change the compiled source.

The deferred target-GPU Phase 2 certification remains independent of CI compile/test success.

## Verified legacy transform convention

The audit of the actual Main math/animation path established the boundary used by the modern pose contract:

- `VectorRotate(in, matrix, out)` evaluates `Dot(in, matrix[row])`, so the 3x3 bone basis is consumed row-major and must not be transposed during quaternion conversion.
- `R_ConcatTransforms` composes the same 3x4 affine representation used by `BMD::Animation`.
- `BMD::Transform` applies `BoneScale` during the bone transform and, when `Translate=true`, applies `BodyScale` and then adds `BodyOrigin`.
- `Calc_RenderObject` passes `!Translate` to `BMD::Animation`; therefore some legacy call paths can produce matrices where the body transform is already baked. Those matrices are intentionally not accepted by the first modern pose path yet.

This removes the matrix-transpose ambiguity and turns the body-transform distinction into an explicit fallback gate instead of an implicit assumption.

## Next action

1. Keep the Windows/x86 Release/Debug, geometry, skeleton-pose and instance-context regression gates green.
2. Wire one narrow `BodyTransformSeparate` producer into `CModernBMDRenderContextScope`; do not use `KIND_PLAYER` alone as a Hero discriminator.
3. Activate the first supported textured BMD draw through geometry cache + Skeleton Texture + legacy texture wrapper + explicit framebuffer/viewport + `SubmitIndexed()` + GL state scope.
4. Prove two independent instances and then local Hero parity before widening migration coverage.
5. Run the deferred interactive Windows/OpenGL 4.6 Phase 2 GPU certification when target-GPU access is available.
