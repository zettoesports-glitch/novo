# Phase 3 — Renderer Core

## State

Repository-side implementation is in progress and the core infrastructure is now present in source. The Phase 2 real Windows/GPU certification is still pending because it cannot be run in the current environment. Proceeding with Phase 3 was explicitly authorized with that runtime gate deferred; this does not convert Phase 2 into a runtime-certified state.

## Implemented contracts

- `ModernRendererTypes.h` defines backend-neutral view/pass identities and constant-buffer transport structures.
- The initial BMD vertex contract is locked with compile-time offset/size checks: `Position` float3, `Normal` float3, `UV` float2, `PositionBone` uint16, `NormalBone` uint16 and `OriginalVertexId` uint32, total 40 bytes.
- Frame, instance and material constant structures are 16-byte aligned and contain no OpenGL handles.
- `IModernRendererBackendAdapter` is the game-side backend boundary.
- `CDiligentOpenGL46Adapter` consumes the already-attached Phase 2 `IRenderDevice`/`IDeviceContext`; it does not create another WGL context or presentation path.
- The backend selection path explicitly accepts OpenGL 4.6 now and leaves Vulkan/Direct3D 11 unavailable rather than silently falling back to another modern API.
- `CModernGPUBuffer` and `CModernConstantBuffer` provide backend-neutral buffer/update lifecycles over Diligent.
- `CModernShaderManager` provides cached shared-HLSL shader creation.
- `CModernTextureSamplerManager` owns texture SRVs and sampler cache entries.
- `CModernPipelineResourceCache` owns PSO/SRB cache entries.
- `ModernIndexedDrawSubmission` and `CModernRendererCore::SubmitIndexed()` define the indexed draw submission boundary and invalidate Diligent's OpenGL state cache at the raw-GL -> Diligent transition.

## Build integration

The new core is header/inl based and is included by the existing `CShaderGL.h` build path. This deliberately avoids adding a new project file or changing the legacy `Main.vcxproj` source list while the modern path is isolated. Runtime activation is not automatic yet; no production BMD draw is redirected by Phase 3.

Debug/Win32 is effectively C++17 through `Directory.Build.targets`, scoped to Main. Release/Win32 already uses C++17 in `Main.vcxproj`; DiligentCore keeps its upstream language settings.

## Safety boundaries

Phase 3 does **not**:

- create a second OpenGL context;
- create a Diligent swap chain;
- call a second `Present`/`SwapBuffers` path;
- redirect Hero/player/BotBuffer/NPC/monster rendering yet;
- upload Skeleton Texture/pose data yet;
- claim the Phase 2 GPU gate passed;
- activate Vulkan or Direct3D 11.

The legacy renderer therefore remains authoritative until the first deliberately migrated draw.

## Pending validation

- Windows/x86 Release and Debug compile against the pinned Diligent `v2.5.6` commit after these Phase 3 source changes.
- Real GPU runtime certification of Phase 2.
- First production HLSL PSO/SRB and constant-buffer binding.
- First persistent BMD vertex/index upload.
- Pose/Skeleton Texture conversion and two-independent-instance proof.

## Next implementation step

Build the first model pipeline on top of this core: create the BMD vertex/index GPU resources, compile the shared HLSL vertex/fragment pair, define the Diligent input layout/PSO/SRB contract, upload frame/instance/material constants and keep the legacy draw as fallback until visual/runtime comparison is available.

## Phase 3 continuation — persistent BMD geometry (2026-09-11)

Base reviewed: `238355b107b12f206cc65ca914197683456aff84`.

- `ModernBMDGeometry.h` adds a CPU-only, transactional triangle converter.
- `BMD::BuildModernMesh` instantiates it with the real `Mesh_t` and `NumBones`; the existing Main build therefore type-checks the actual adapter.
- The converter validates counts, pointers, indices, separate position/normal bones and finite coordinates. Non-triangular polygons explicitly return an error for legacy fallback.
- Corner deduplication uses (vertex, normal, UV), preserving seams, hard normals, winding, original wave vertex IDs and unmodified Main UVs.
- Output indices are uint32, including when seam expansion exceeds 65,535 vertices.
- `CModernBMDMeshCache` uploads immutable Diligent vertex/index buffers transactionally and reuses them by asset ID + asset generation + mesh index. No entity pose, alpha or light enters this asset cache.
- A failed upload does not register a partial mesh. Cache lookup pointers are borrowed until asset removal or core shutdown.
- `CModernGraphicsBootstrap::Shutdown` now shuts down the renderer core before releasing the Diligent context/device; the core clears its mesh cache with the other GPU caches.

### Usage contract for the next pipeline

1. Assign a stable asset ID and a new generation on reload. Do not use an object/entity ID or a recycled pointer as asset identity.
2. Call `BMD::BuildModernMesh(meshIndex, geometry, error)`; on false, keep the legacy path. A failed CPU conversion preserves the caller's prior output, so callers must check the returned bool and must not upload it as a new revision.
3. Initialize the core against the active bootstrap, then call `GetBMDMeshCache().Upload(device, key, geometry)`.
4. Reuse the cached buffers for instances of the same immutable asset revision. Change the key generation on source mutation and call `RemoveAsset` when unloading.
5. Supply cached buffers and `NumIndices` to a submission with **IndexType = Diligent::VT_UINT32**, offsets zero, FirstIndex zero and BaseVertex zero.
6. Bind a matching PSO/SRB, explicit render targets, viewport, per-instance pose/constants and texture before enabling a production draw.

This change implements the asset conversion/upload building block. It does **not** call it from a production BMD draw, create the first material PSO/SRB, convert/upload skeleton poses, or certify raw-GL/Diligent state restoration. The first pipeline remains the next item. In particular, `SubmitIndexed` invalidates cached state: a future pass must explicitly establish render targets and viewport after that invalidation before drawing. An attached device has no swapchain; do not assume dummy default-framebuffer proxies are safe without inspecting the pinned backend.

### Validation

Portable CPU regression tests in `tests/ModernBMDGeometryTests.cpp` cover shared corners, normal/UV seams, bone indices, invalid data, transactional rejection, triangle order and a 72,000-corner mesh with uint32 output. Local GCC C++17 build and CTest passed; AddressSanitizer/UndefinedBehaviorSanitizer passed with leak detection disabled because this environment's tracing prevents LeakSanitizer from running.

The existing Windows workflow now also builds/runs the CPU tests in x86. Main Release/Debug and Diligent GPU-resource creation still require their Windows/GPU gates. No local Windows or GPU success is implied by the CPU tests.
