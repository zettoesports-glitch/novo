# MU Main 5.2 Renderer Modernization Roadmap

## Goal
Modernize the Main 5.2 rendering pipeline with OpenGL 4.6 as the first active backend, while structuring the engine so Vulkan and DirectX 11 can be implemented later without rewriting game-side rendering code.

## Current verified repository state
- Branch: `modernization`
- Main 5.2 root: `SRCMainGS/Source/Main5.2`
- Main 5.2 currently contains `Main.sln`, `dependencies`, and `source`.
- Renderer modernization work must be integrated inside the existing Main source tree rather than assuming a separate `resource` directory under Main5.2.

## Architecture target

### Backend-neutral layer
Introduce a renderer-facing abstraction used by game systems:
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

### OpenGL 4.6 backend
Implement first:
- Context validation and capability logging
- VAO/VBO/EBO lifecycle
- Shader compilation and program linking
- Uniform and UBO management
- Texture/sampler binding
- Blend/depth/cull state cache
- Indexed and non-indexed draw paths
- GPU error/debug logging

### Future backend slots
Prepare factories and interfaces for:
- OpenGL 4.6 — active implementation
- Vulkan — placeholder only
- DirectX 11 — placeholder only

Do not mix Vulkan/DX11 implementation into the first migration phase.

## Migration order
1. Inventory the legacy renderer entry points and all direct OpenGL state changes.
2. Identify BMD/model mesh upload and draw paths.
3. Identify texture/material/light/fog contracts used by legacy rendering.
4. Document shader inputs and outputs from the NextMU/reference material available in the repository.
5. Build the backend-neutral renderer layer.
6. Implement the OpenGL 4.6 backend.
7. Bridge BMD mesh data into persistent GPU resources.
8. Introduce modern object/material/frame parameter blocks.
9. Migrate the safest render path first.
10. Preserve legacy fallback for unsupported object classes while parity is validated.
11. Expand migration to remote players, bots, NPCs and monsters after instance-state correctness is proven.
12. Migrate terrain, effects, particles, UI and special passes separately.

## Compatibility strategy
During the transition, renderer selection must be explicit per render path. The legacy renderer remains available as a fallback until a migrated path has visual and behavioral parity.

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

Shader source should remain backend-agnostic at the contract level even though GLSL is the first implementation.

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
The next code slice should focus on source-level discovery and the BMD/model path:
1. Locate exact BMD render entry points.
2. Locate modern renderer files already present in the branch.
3. Trace one complete local-character draw from caller to mesh submission.
4. Trace one remote player/bot/NPC path for comparison.
5. Define the first stable `RenderObjectData` / `RenderMaterialData` contract.
6. Move only the verified-safe object path to the OpenGL 4.6 abstraction.

## Rule for the project
OpenGL 4.6 is the only backend that should receive production implementation during this phase. Vulkan and DirectX 11 must remain architectural extension points until the OpenGL renderer is complete and stable.
