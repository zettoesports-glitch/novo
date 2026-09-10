# Implementation Checklist

## Discovery
- [ ] Locate legacy BMD/model render entry points
- [ ] Locate all existing modern renderer files
- [ ] Locate GLSL/HLSL shader sources already committed
- [ ] Locate NextMU/reference renderer and shader material
- [ ] Trace local-player render path end-to-end
- [ ] Trace remote-player/bot/NPC render path end-to-end

## Renderer core
- [ ] Backend enum/factory
- [ ] Renderer device interface
- [ ] Buffer abstraction
- [ ] Texture abstraction
- [ ] Shader/program abstraction
- [ ] Render state cache
- [ ] Frame/Object/Material data contracts

## OpenGL 4.6
- [ ] Context/capability validation
- [ ] VAO/VBO/EBO lifecycle
- [ ] Shader compile/link diagnostics
- [ ] Uniform/UBO path
- [ ] Texture/sampler binding
- [ ] Depth/blend/cull state handling
- [ ] Draw submission
- [ ] Debug callback/logging

## BMD migration
- [ ] Map legacy mesh structures
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
