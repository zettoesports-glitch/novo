# Implementation Checklist

Static discovery evidence: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). Checked discovery items mean source inspection, not runtime validation.

## Discovery
- [x] Locate legacy BMD/model render entry points
- [x] Locate existing BMD shader/VBO experiment (CShaderGL, Mesh_t, RenderVertexBuffer)
- [x] Locate GLSL/HLSL shader sources already committed
- [x] Locate NextMU/reference renderer and shader material
- [x] Trace initial local-player render path end-to-end (static)
- [x] Trace initial remote-player/BotBuffer/NPC render paths (static)

## Documented contracts
- [x] Initial producer/consumer map for geometry, pose, view, instance and material
- [x] Snapshot ownership rules for shared BMD state and temporary inventory objects
- [x] Diligent + shared HLSL architecture decision recorded
- [ ] Implement and verify concrete CPU/GPU layouts and pose conversion

## Renderer core
- [ ] Backend enum/factory
- [ ] Renderer device interface
- [ ] Buffer abstraction
- [ ] Texture abstraction
- [ ] Shader/program abstraction
- [ ] Render state cache
- [ ] Frame/Object/Material data contracts

## OpenGL 4.6
- [x] Define context/profile ownership and single-presentation rules (static design)
- [ ] Locate and wire real Main window/context/resize/present integration points
- [ ] Pin Diligent and shared shader revisions
- [ ] Context/capability validation
- [ ] Diligent buffer/texture lifecycle and transitions
- [ ] Diligent HLSL shader compilation diagnostics
- [ ] Diligent constant-buffer path
- [ ] Texture/sampler binding
- [ ] Depth/blend/cull state handling
- [ ] Draw submission
- [ ] Debug callback/logging

## BMD migration
- [x] Map legacy mesh fields and initial conversion rules (static)
- [ ] Define modern GPU vertex layout
- [ ] Persistent geometry upload
- [ ] Material bridge
- [ ] Per-object transform/animation state
- [ ] Local hero parity
- [ ] Remote player parity
- [ ] Bot parity
- [ ] NPC parity
- [ ] Monster parity

## Later passes
- [ ] Terrain
- [ ] Effects/particles
- [ ] Chrome/reflection/special materials
- [ ] UI/2D
- [ ] Vulkan backend
- [ ] DirectX 11 backend
