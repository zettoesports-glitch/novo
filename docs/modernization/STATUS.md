# Modernization Status

## Active target

Diligent integration for Main 5.2, using shared HLSL shaders and OpenGL 4.6 first. Vulkan and Direct3D 11 remain future active backends using the same contracts.

## Completed

- Phase 1 static source mapping against `13cfc7c3e5dab042e1c3e8f4184e18c26dd08eff`.
- Initial call paths: Hero, remote players, BotBuffer, NPC/monsters, world objects, equipment and inventory preview.
- Geometry, pose, instance, view, material and texture producer/consumer contracts.
- Snapshot/lifetime requirements and legacy render-state mapping.
- Roadmap aligned with the Diligent + HLSL decision.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).

## Phase 2 in progress

- Pin/verify Diligent dependency and shader-source revisions.
- Define Windows OpenGL 4.6 context/profile ownership and single-presentation model.
- Add the first renderer bootstrap and capability diagnostics without replacing legacy BMD drawing yet.
- Implement concrete CPU/GPU contracts only after the runtime/bootstrap boundary is stable.

## Not yet validated

No Diligent runtime integrated or enabled by this documentation change. No Windows build, GPU execution, shader compilation or visual-parity test performed. All item/map/script/cloth variants are not claimed complete.

## Next action

Implement Phase 2 runtime bootstrap on `modernization`: dependency pinning, OpenGL 4.6 capability validation, device/context/swapchain ownership, resize path and single presentation. Then implement the documented CPU/GPU contracts before the two-instance BMD proof.
