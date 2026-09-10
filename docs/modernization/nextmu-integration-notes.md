# NextMU / Main 5.2 Integration Notes

This file is intentionally created as a working integration notebook for the modernization branch.

## Scope
- Compare Main 5.2 legacy render flow with the NextMU/reference material present in the repository.
- Extract reusable renderer concepts rather than copying backend-specific code blindly.
- Use OpenGL 4.6 as the only active backend in the first production phase.
- Preserve architectural extension points for Vulkan and DirectX 11.

## Integration principles
1. Keep game logic independent from graphics API calls.
2. Introduce explicit per-frame, per-object and per-material data.
3. Prevent cross-entity state leakage.
4. Keep legacy fallback available during staged migration.
5. Port shader contracts and rendering behavior before attempting aggressive optimization.
6. Validate visual parity path-by-path.

## What to extract from NextMU/reference implementation
- Renderer/device abstraction boundaries
- Shader program organization
- Vertex/index buffer ownership
- Texture and sampler binding model
- Material representation
- Camera/frame constant flow
- Per-object transform flow
- Blend/depth/cull state model
- Resource lifetime strategy
- Draw submission model
- Backend selection/factory structure

## What should not be copied blindly
- Backend-specific ownership assumptions
- Resource lifetime rules tied to another engine architecture
- Vulkan synchronization model into OpenGL code
- DirectX-specific constant buffer code into GLSL paths
- Object layouts that do not match the Main 5.2 BMD structures

## First parity target
The first complete target should be a single verified BMD/object path with:
- correct geometry
- correct animation state
- correct UVs/textures
- correct transform
- correct brightness/lighting
- correct alpha/blend/depth behavior
- no shared object state

Once stable, widen support to other entity classes and passes.
