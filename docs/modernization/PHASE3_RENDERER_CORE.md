# Phase 3 — Renderer Core and first BMD production draw

## State

Repository-side Phase 3 infrastructure is implemented and type-checked through the existing Main build path. The remaining boundary is production activation: a real BMD producer must propagate per-instance context into the shared BMD draw path and deliberately select the modern textured path, with legacy fallback on every unsupported or failed condition.

The deferred Phase 2 real Windows/GPU certification is still pending. Repository/build progress does not convert that runtime gate into a pass.

## Implemented renderer contracts

- `ModernRendererTypes.h` defines backend-neutral view/pass identities, the locked 40-byte `ModernBMDVertex` ABI, and aligned Frame/Instance/Material constant buffers.
- `IModernRendererBackendAdapter` is the game-side backend boundary. OpenGL 4.6 is the active modern target; Vulkan and Direct3D 11 remain reserved/unavailable.
- `CModernGPUBuffer` and `CModernConstantBuffer` provide Diligent buffer/update lifecycles.
- `CModernShaderManager`, `CModernTextureSamplerManager` and `CModernPipelineResourceCache` own shared-HLSL shader, texture/sampler and PSO/SRB caches.
- `CModernRendererCore::SubmitIndexed()` is implemented. A submission must provide explicit render target, dimensions, PSO/SRB, vertex/index buffers and index count. The draw boundary invalidates Diligent's cached GL state, sets render targets, establishes viewport, binds resources and issues `DrawIndexed`.
- `CModernBMDMeshCache` converts validated immutable BMD geometry into persistent Diligent vertex/index buffers keyed by asset identity + generation + mesh index.

## Implemented BMD pipeline

`ModernBMDPipeline.h` now contains the first shared-HLSL textured BMD pipeline:

- explicit input layout matching `ModernBMDVertex`;
- Frame/Instance/Material constant buffers;
- depth/blend/cull PSO state;
- mutable Skeleton Texture and diffuse texture SRV bindings;
- uint32 indexed submission construction;
- vertex skinning from quaternion + translation/uniform-scale bone texels;
- `BodyScale/BodyOrigin` applied after skinning for `BodyTransformSeparate` snapshots;
- initial alpha-test material flag in the pixel shader.

This is the first pipeline only. Chrome, metal, oil, wave, z-test, lightmap, projected shadow and other legacy/NextMU variants remain on legacy fallback until separate permutations are implemented and validated.

## Implemented pose/Skeleton Texture transport

`ModernSkeletonPose.h` and `ModernSkeletonTexture.h` implement the first production pose contract:

- legacy `BoneMatrix[bone][3][4]` is validated and converted to quaternion + translation/BoneScale;
- reflected/singular/non-finite matrices are rejected rather than normalized into apparently valid poses;
- the verified legacy row-major convention is preserved;
- `BodyTransformBaked` is rejected by the first modern path to prevent double application of `BodyScale/BodyOrigin`;
- each bone occupies two RGBA32F texels;
- the initial atlas is 512 texels wide with one pose snapshot per row and explicit linear offset/width/bone-count addressing.

## Implemented legacy coexistence bridges

The first production draw no longer needs to invent these pieces:

- `ModernOpenGLDefaultFramebuffer.h` exposes the legacy WGL default framebuffer to Diligent through a non-presenting OpenGL swap-chain facade. Main remains the only `SwapBuffers` owner.
- `ModernLegacyGLState.h` captures/restores the GL state needed around a modern draw, including framebuffer, viewport/scissor, program/VAO/buffers, textures/samplers, blend, depth, cull, stencil, alpha-test/fog and polygon state.
- `ModernLegacyTextureBridge.h` wraps Main-owned native GL textures with Diligent shader-resource views without transferring native texture ownership.
- `SubmitIndexed()` requires explicit production targets and viewport dimensions rather than assuming a Diligent-created swap chain.

These facilities are implemented, but they are not runtime-certified until a production BMD draw actually crosses raw GL -> Diligent -> raw GL on the target GPU.

## Per-instance propagation contract

`ModernBMDRenderContext.h` adds the missing identity bridge between high-level entity producers and the shared BMD asset renderer.

A `BMD` object is an asset and can be shared by multiple live entities, so instance identity, pose-atlas slot and generation must never be stored on the BMD itself. `CModernBMDRenderContextScope` snapshots a transient context and restores the previous context on destruction. Nested attachment/preview rendering therefore cannot leak the current Hero/player/object identity into subsequent draws.

The context carries:

- `ModernDrawIdentity` including instance slot + generation, view and pass;
- instance kind (Hero, remote player, NPC, monster, world object, preview or attachment);
- immutable asset ID + asset generation;
- per-instance Skeleton Texture slot;
- an explicit `AllowModernDraw` migration gate.

`ModernBMDRenderContextTests.cpp` covers nested Hero -> remote-player replacement/restoration, snapshot semantics and a later item-preview scope so state leakage is caught by the portable regression suite.

## Production activation audit

Fresh audit of `ZzzBMD.cpp` at the current branch shows no `Modern*` reference inside `BMD::RenderMesh()`. The legacy function still resolves material/texture state and issues the legacy client-array/VBO path. Therefore the modern renderer is **prepared but not yet selected by a production BMD draw**.

This distinction matters: source presence of a PSO, Skeleton Texture, default-framebuffer bridge or state guard is not proof that a live Hero/model traverses them.

The high-level Phase 1 map also remains authoritative: Hero and remote players share the character/BMD path, BotBuffer arrives as a player-like entity, attachments can nest, and item preview uses temporary objects. The first activation must therefore propagate an explicit scoped instance context from the producer instead of guessing identity inside `RenderMesh()` from model type or raw pointers.

## Build integration

Modern Phase 3 headers are type-checked through the existing `CShaderGL.h` / `CShaderGL.cpp` build path. No second Visual Studio project, OpenGL context or presentation owner is introduced.

Portable tests currently cover:

- BMD geometry conversion and uint32 expansion;
- skeleton pose packing/rejection rules;
- scoped BMD instance-context isolation.

Prior Windows/x86 CI passed the geometry/core Main Release+Debug build gate for commit `925bbb5956ac05eece159c1a7b0652b2d41dfbda`. Commits after that point still require their normal CI/build result; this document does not infer a pass from source inspection alone.

## Safety boundaries

Phase 3 still does **not** claim:

- Phase 2 target-GPU certification;
- a live migrated Hero/player/BotBuffer/NPC/monster;
- visual parity with legacy rendering;
- target-GPU proof of BMD cache reuse or Skeleton Texture uploads;
- raw-GL/Diligent state coexistence proof around a real model;
- Vulkan or Direct3D 11 activation.

Legacy rendering remains authoritative unless a deliberately supported modern production draw succeeds completely. Failure/unsupported state must fall through to the original legacy draw without double-rendering.

## Next implementation step

1. Install `CModernBMDRenderContextScope` at one deliberately selected high-level producer whose pose space is known to be `BodyTransformSeparate`; begin with a narrow/local-Hero-compatible path, not every `KIND_PLAYER`.
2. At the shared BMD boundary, accept only the first supported textured/material state. Build/reuse immutable geometry, snapshot/upload the current pose to the context's Skeleton slot, wrap the resolved legacy diffuse texture and initialize/reuse the first `CModernBMDPipeline`.
3. Initialize/resize `CModernOpenGLDefaultFramebuffer`, enter `CModernLegacyGLStateScope`, fill explicit target/viewport data and call `CModernRendererCore::SubmitIndexed()`.
4. Return success only after the indexed modern draw completes; otherwise execute the untouched legacy path. Never issue both paths for the same accepted draw.
5. Prove two independent instances of the same BMD keep separate pose slots/generations, then compare local Hero visual parity before expanding to remote player, BotBuffer, NPC and monster paths.
6. Run the deferred interactive Windows/OpenGL 4.6 Phase 2 GPU gate when target-GPU access is available.
