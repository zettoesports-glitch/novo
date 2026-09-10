# Modernization Status

## Active target

Diligent integration for Main 5.2, using shared HLSL shaders and OpenGL 4.6 first. Vulkan and Direct3D 11 remain future active backends using the same contracts.

## Completed

- Phase 1 static source mapping against `13cfc7c3e5dab042e1c3e8f4184e18c26dd08eff`.
- Initial call paths: Hero, remote players, BotBuffer, NPC/monsters, world objects, equipment and inventory preview.
- Geometry, pose, instance, view, material and texture producer/consumer contracts.
- Snapshot/lifetime requirements and legacy render-state mapping.
- Roadmap aligned with the Diligent + HLSL decision.
- Phase 2 bootstrap contract documented: native-window/context ownership, resize, capability validation and single presentation.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).
Phase 2 contract: [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md).

## Phase 2 in progress

- Locate the real Main Windows creation/OpenGL context/resize/present integration points.
- Pin/verify Diligent dependency and shader-source revisions.
- Add the first renderer bootstrap and capability diagnostics without replacing legacy BMD drawing yet.
- Implement concrete CPU/GPU contracts only after the runtime/bootstrap boundary is stable.

## Not yet validated

No Diligent runtime has been validated by a Windows build/GPU run yet. No visual-parity test has been performed. All item/map/script/cloth variants are not claimed complete.

## Next action

Wire the Phase 2 runtime bootstrap to the actual Main window/context/present code, then validate OpenGL 4.6 capability, resize and exactly-one-present behavior. After that, implement the documented CPU/GPU contracts before the two-instance BMD proof.
