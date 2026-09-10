# Renderer Modernization

Working documentation for the `modernization` branch.

- [Phase 1: Main → renderer mapping](PHASE1_MAIN_RENDERER_MAPPING.md): source call paths, instance ownership, CPU/GPU contracts and material/state mapping.
- [Renderer roadmap](renderer-roadmap.md): Diligent integration and migration sequence.
- [NextMU integration notes](nextmu-integration-notes.md): reference material.
- [Implementation checklist](IMPLEMENTATION_CHECKLIST.md): discovery versus implementation gates.
- [Status](STATUS.md): completed work and next step.

Architecture: **Diligent + shared HLSL**. First production backend: **OpenGL 4.6**. Vulkan and Direct3D 11 share the contracts; activation and validation remain pending.
