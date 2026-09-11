# Renderer Modernization

Working documentation for the `modernization` branch.

- [Phase 1: Main → renderer mapping](PHASE1_MAIN_RENDERER_MAPPING.md): source call paths, instance ownership, CPU/GPU contracts and material/state mapping.
- [Phase 2: OpenGL 4.6 bootstrap](PHASE2_OPENGL46_BOOTSTRAP.md): Diligent runtime boundary, context/profile ownership, resize, presentation-ownership rules, build evidence and GPU validation procedure.
- [Phase 2 runtime discovery](PHASE2_RUNTIME_DISCOVERY.md): confirmed Main window/WGL/present/shutdown lifecycle and coexistence wiring.
- [Phase 2 completeness audit](PHASE2_COMPLETENESS_AUDIT.md): corrective audit of lifecycle ordering, WGL recreation, resize evidence and the exact boundary of presentation evidence. This file plus `STATUS.md` is authoritative when older wording conflicts.
- [Diligent pin](DILIGENT_PIN.md): exact dependency revision and reproducible setup model.
- [Renderer roadmap](renderer-roadmap.md): Diligent integration and migration sequence.
- [NextMU integration notes](nextmu-integration-notes.md): reference material.
- [Implementation checklist](IMPLEMENTATION_CHECKLIST.md): discovery, build, runtime and implementation gates.
- [Status](STATUS.md): completed work and next step.

Runtime helper: `SRCMainGS/Source/Main5.2/run_phase2_runtime_test.ps1` launches/validates the Phase 2 GPU gate and checks the persistent `Client_2/ModernGraphics.log` evidence. The parser validates lifecycle ordering and complete context-recreation cycles; it does not count legacy `SwapBuffers` calls per frame.

Architecture: **Diligent + shared HLSL**. First production backend: **OpenGL 4.6**. Vulkan and Direct3D 11 share the contracts; activation and validation remain pending.
