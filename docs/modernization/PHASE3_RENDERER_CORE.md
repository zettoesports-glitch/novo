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
