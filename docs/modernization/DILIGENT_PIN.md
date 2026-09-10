# Diligent / shader revision pin — Phase 2

Date: 2026-09-10
Target branch: `modernization`

## DiligentCore

The Phase 2 bootstrap is pinned to the official stable DiligentCore release below:

- repository: `DiligentGraphics/DiligentCore`
- tag: `v2.5.6`
- commit: `b036337d68be2353c9950a85929acf796b9a6d50`
- OpenGL integration API used: `IEngineFactoryOpenGL::AttachToActiveGLContext`

This pin is for reproducible Main 5.2 integration. It is **not** a claim that NextMU used this exact DiligentCore revision.

Do not replace this pin with a moving `master`/`main` reference during Phase 2. A later revision must be introduced as an explicit dependency upgrade and revalidated on Win32.

## Reference shader/resource snapshot

The reference shader contracts used by the modernization work are pinned to the repository snapshot:

- repository: `zettoesports-glitch/novo`
- baseline commit: `ceadb719f47bc781f789fe56d2e0eff2c4c8012b`
- reference package: `Resources_DE_2024-01-25`

This snapshot is the evidence baseline for the NextMU model/world/effect shader contracts already mapped in `ReverseEngineering/NextMU/`. The production shared-HLSL shader package for Main 5.2 is still a later implementation task; this pin prevents the reference inputs from drifting while that bridge is built.

## Build activation rule

`MU_ENABLE_DILIGENT` must remain disabled until the pinned Diligent headers/libraries are available to the Win32 `Main.vcxproj`. The legacy build therefore remains the safe default. When the dependency is wired, enabling this macro activates the Phase 2 runtime bridge and OpenGL 4.6/Diligent attach path.