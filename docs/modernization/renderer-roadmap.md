# MU Main 5.2 Renderer Modernization Roadmap

## Goal
Modernize the Main 5.2 rendering pipeline with OpenGL 4.6 as the first active backend, while structuring the engine so Vulkan and DirectX 11 can be implemented later without rewriting game-side rendering code.

## Current verified repository state
- Branch: `modernization`
- Main 5.2 root: `SRCMainGS/Source/Main5.2`
- Main 5.2 contains `Main.sln`, `dependencies`, and `source`.
- Renderer modernization work is integrated inside the existing Main source tree rather than assuming a separate `resource` directory under Main5.2.

## Phase 1 discovery

The static Main-to-renderer mapping is documented in [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). It identifies initial call paths, data producers, snapshot ownership, materials and the Diligent integration boundary. Phase 1 static discovery is complete.

## Phase 2 runtime bootstrap

Phase 2 is defined in [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md) and detailed in [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).

The repository-side bootstrap is implemented:

- legacy Main remains owner of `HWND`, `HDC`, `HGLRC` and `SwapBuffers`;
- Diligent attaches to the already-current WGL context with `AttachToActiveGLContext`;
- effective OpenGL >= 4.6 is required for modern activation;
- resize and pre-WGL-teardown shutdown are wired through a temporary `WH_CALLWNDPROC` coexistence bridge;
- the OpenGL backend is loaded through the official Diligent DLL loader;
- Release/x86 and Debug/x86 have both compiled successfully against the pinned Diligent setup;
- runtime decisions are persisted to `Client_2/ModernGraphics.log`;
- optional KHR_debug diagnostics are available through `MU_MODERN_GL_DEBUG=1`;
- `run_phase2_runtime_test.ps1` provides a reproducible local GPU validation gate.

The only bootstrap certification that cannot be provided by hosted CI is the real interactive GPU/WGL run. Phase 2 must not be called runtime-complete until that gate passes.

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
The implementation sequence is:
- pin Diligent and reference shader/resource revisions;
- establish/validate the OpenGL 4.6 runtime boundary and explicit presentation ownership;
- prove the attached-context lifecycle on the target GPU;
- create buffers/textures through Diligent and compile shared HLSL through its shader interface;
- define constant buffers, pipeline state objects and shader resource bindings;
- submit indexed/non-indexed draws and resource transitions through Diligent;
- log shader/resource failures with source, entity and pass identifiers.

The current coexistence mode validates the effective version of the context returned by legacy `wglCreateContext()`. An explicit 4.6 compatibility-context creation path may be introduced later if target-driver validation shows it is required and legacy GL behavior remains intact.

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
5. Establish Diligent/OpenGL 4.6 bootstrap, ownership, reproducible Win32 build and runtime evidence path. **Repository/build implementation complete; target-GPU validation pending.**
6. Implement concrete CPU/GPU contracts and pose conversion.
7. Bridge BMD mesh data into persistent GPU resources.
8. Introduce modern object/material/frame parameter blocks.
9. Migrate the safest render path first.
10. Preserve legacy fallback for unsupported object classes while parity is validated.
11. Expand migration to remote players, bots, NPCs and monsters after instance-state correctness is proven.
12. Migrate terrain, effects, particles, UI and special passes separately.

## Compatibility strategy
During the transition, renderer selection must be explicit per render path. The legacy Main currently owns a compatibility-sensitive WGL context and fixed-function/client-array paths, so no Core-only migration can be assumed safe. Diligent attaches to the host-owned context and invalidates its cached GL state at raw-GL -> Diligent boundaries. Presentation remains owned by the legacy scene code exactly once per frame.

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
- OpenGL version/vendor/renderer log — **implemented in Phase 2; runtime evidence pending**
- shader compile/link errors with source name
- resource creation/destruction counters
- draw-call counters
- fallback reason logging
- entity/object identifier in renderer diagnostics
- optional GL debug callback — **implemented in Phase 2; runtime evidence pending**

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
Use the completed Phase 1 map and repository-side Phase 2 bootstrap as input:
1. Finish the permanent combined Release+Debug x86 CI gate against the latest bootstrap source.
2. Run `run_phase2_runtime_test.ps1 -EnableGLDebug -RequireResize` on the target Windows/OpenGL 4.6 GPU and validate `ModernGraphics.log`.
3. After the GPU gate, implement concrete CPU/GPU contracts and pose conversion.
4. Add persistent BMD geometry buffers, Frame/Object/Material constant buffers and the shared-HLSL model pipeline.
5. Render one BMD with two independent instances.
6. Compare Hero, remote player/BotBuffer and inventory-preview behavior before expanding coverage.

## Rule for the project
OpenGL 4.6 is the only backend that should receive production implementation during this phase. Vulkan and DirectX 11 must remain architectural extension points until the OpenGL renderer is complete and stable.