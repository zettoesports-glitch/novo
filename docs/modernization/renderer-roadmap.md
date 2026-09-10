# MU Main 5.2 Renderer Modernization Roadmap

## Goal
Modernize the Main 5.2 rendering pipeline with OpenGL 4.6 as the first active backend, while structuring the engine so Vulkan and DirectX 11 can be implemented later without rewriting game-side rendering code.

## Current verified repository state
- Branch: `modernization`
- Main 5.2 root: `SRCMainGS/Source/Main5.2`
- Main 5.2 currently contains `Main.sln`, `dependencies`, and `source`.
- Renderer modernization work must be integrated inside the existing Main source tree rather than assuming a separate `resource` directory under Main5.2.

## Phase 1 discovery

The static Main-to-renderer mapping is documented in [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). It identifies initial call paths, data producers, snapshot ownership, materials and the Diligent integration boundary. Runtime and GPU validation remain pending.

## Phase 2 runtime bootstrap

Phase 2 is defined in [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md). It establishes the runtime boundary for Diligent/OpenGL 4.6: native-window ownership, context/profile requirements, resize ownership, exactly one presentation per frame, capability diagnostics and coexistence rules with the legacy renderer. The implementation must bind these contracts to the real Main window/context/present code before any BMD path is migrated.

## Architecture target

### Backend-neutral layer
Use Diligent as the graphics abstraction, with shared HLSL shaders and a Main-facing adapter for existing game data. OpenGL is the first production backend; Vulkan and Direct3D 11 use the same Diligent resource and draw contracts.

Expose game-facing contracts for:
- Renderer backend selection enum
- Renderer device/interface
- Buffer abstraction
- Texture abstraction
- Shader/program abstraction
- Pipeline/render-state abstraction
- Frame/object/material constant data
- Resource lifetime manager
- Render command submission interface

The game should not directly depend on OpenGL calls after a render path has been migrated.

### Diligent / OpenGL 4.6 integration
Implement first:
- Pin Diligent and submodule revisions, build architecture and runtime dependencies.
- Establish explicit OpenGL 4.6 context creation and capability validation; selecting the GL backend alone does not guarantee 4.6.
- Define host window, render target, resize and presentation ownership.
- Create buffers/textures through Diligent; compile HLSL through its shader interface.
- Define constant buffers, pipeline state objects and shader resource bindings.
- Submit indexed/non-indexed draws and resource transitions through Diligent.
- Log shader/resource failures with source, entity and pass identifiers.

### Future backend integration
Prepare typed factories, conditional builds and explicit availability checks for:
- OpenGL 4.6 — active implementation
- Vulkan — shared contracts, activation and validation pending
- DirectX 11 — shared contracts, activation and validation pending

Do not mix Vulkan/DX11 implementation into the first migration phase.

## Migration order
1. Inventory the legacy renderer entry points and all direct OpenGL state changes. **Completed in Phase 1.**
2. Identify BMD/model mesh upload and draw paths. **Completed statically in Phase 1.**
3. Identify texture/material/light/fog contracts used by legacy rendering. **Completed initially in Phase 1.**
4. Document shader inputs and outputs from the NextMU/reference material available in the repository. **Completed initially in Phase 1.**
5. Establish Diligent/OpenGL 4.6 runtime bootstrap and ownership. **Phase 2 active.**
6. Implement concrete CPU/GPU contracts and pose conversion.
7. Bridge BMD mesh data into persistent GPU resources.
8. Introduce modern object/material/frame parameter blocks.
9. Migrate the safest render path first.
10. Preserve legacy fallback for unsupported object classes while parity is validated.
11. Expand migration to remote players, bots, NPCs and monsters after instance-state correctness is proven.
12. Migrate terrain, effects, particles, UI and special passes separately.

## Compatibility strategy
During the transition, renderer selection must be explicit per render path. A temporary legacy path requires a validated context/profile and explicit state boundaries. Legacy calls cannot execute inside a Core-only context or a Vulkan/D3D11 renderer. Validate a host-owned OpenGL 4.6 compatibility context with Diligent attachment, or use an isolated Core harness until coexistence is proven. Define who presents the frame exactly once.

The modern renderer must never reuse object-instance state from another entity. Per-object matrices, material state, animation state, lighting and texture bindings must be supplied explicitly for every draw.

## Shader plan
Create a shader manager with named programs and stable contracts.

Initial shader families:
- BMD/model vertex + fragment
- alpha-tested model
- emissive/bright model
- chrome/reflection compatible path
- terrain
- effect/particle
- UI/2D

Common data blocks should include, where applicable:
- camera/view/projection
- model/world transform
- normal transform
- lighting
- fog
- material parameters
- texture flags
- render flags

Use shared HLSL source through Diligent. Document stage inputs, macros, constant-buffer layouts, resource names and coordinate/color conventions per permutation. Preserve the distinction between 2024 Resources and later NextMU source.

## BMD modernization
The BMD path is the critical bridge between legacy game data and modern rendering.

Required work:
- Map legacy vertex/normal/UV/index structures.
- Define modern packed GPU vertex layouts.
- Upload static mesh data once where possible.
- Separate per-mesh material metadata from geometry.
- Avoid rebuilding buffers every frame.
- Preserve animation/skinning behavior before optimizing it.
- Introduce explicit mesh/material/object handles.

## Diagnostics required before broad rollout
- OpenGL version/vendor/renderer log
- shader compile/link errors with source name
- resource creation/destruction counters
- draw-call counters
- fallback reason logging
- entity/object identifier in renderer diagnostics
- optional GL debug callback in development builds

## Validation gates
A render path is considered migrated only when:
- Geometry matches legacy output.
- Animation state is correct per object.
- Textures and UVs match.
- Brightness/tonality is acceptable.
- Blend/depth/cull behavior matches expected behavior.
- Multiple simultaneous entities do not share state incorrectly.
- No new crash appears during map change or resource destruction.

## Next implementation slice
Use the completed static phase-1 map as input:
1. Pin Diligent and shader versions.
2. Locate and wire the real Windows window/context/resize/present integration points.
3. Validate the Windows OpenGL 4.6 context/profile and presentation model in a narrow prototype.
4. Implement and verify the documented CPU/GPU contracts and pose conversion.
5. Render one BMD with two independent instances.
6. Compare Hero, remote player/BotBuffer and inventory-preview behavior before expanding coverage.

## Rule for the project
OpenGL 4.6 is the only backend that should receive production implementation during this phase. Vulkan and DirectX 11 must remain architectural extension points until the OpenGL renderer is complete and stable.
