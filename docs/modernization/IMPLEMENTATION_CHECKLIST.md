# Implementation Checklist

Static discovery evidence: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md). Checked discovery items mean source inspection, not runtime validation. Phase 2 runtime notes: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).

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
- [ ] Backend enum/factory (backend enum exists; generic backend factory remains a later renderer-core task)
- [ ] Renderer device interface
- [ ] Buffer abstraction
- [ ] Texture abstraction
- [ ] Shader/program abstraction
- [ ] Render state cache
- [ ] Frame/Object/Material data contracts

## OpenGL 4.6 — Phase 2 bootstrap
- [x] Define context/profile ownership and single-presentation rules (static design)
- [x] Locate and wire real Main window/context/resize/present integration points (source bridge implemented; Windows runtime pending)
- [x] Pin Diligent and reference shader revisions (`DILIGENT_PIN.md`)
- [x] Context/capability validation implemented (effective >= 4.6, diagnostics; Windows runtime pending)
- [x] Add reproducible pinned DiligentCore Win32/OpenGL/HLSL setup script
- [x] Add official Diligent explicit backend-DLL loading (`GraphicsEngineOpenGL_32r/32d.dll`)
- [x] Preserve dependency-absent and DLL-load-failure legacy fallback
- [x] Align Main Debug definitions with the Diligent debug backend selection
- [x] Normalize Main Debug/x86 to C++17 without modifying DiligentCore project settings
- [x] Add GitHub Actions Windows/x86 compile gate (`.github/workflows/phase2-win32-build.yml`)
- [x] Execute pinned Diligent setup successfully on Windows (runs `34533022717` and `34533868904`; both `_32r` and `_32d` produced)
- [x] Compile `Main.sln` Release/x86 with the pinned Diligent headers enabled (run `34533022717`, 0 errors)
- [x] Compile `Main.sln` Debug/x86 with C++17 and the pinned Diligent headers enabled (run `34533868904`, 0 errors)
- [x] Observe successful Phase 2 Windows/x86 CI runs for both Main configurations independently
- [ ] Observe the permanent combined Release+Debug x86 workflow gate succeed after consolidating both configurations into one CI job
- [ ] Diligent buffer/texture lifecycle and transitions
- [ ] Diligent HLSL shader compilation diagnostics
- [ ] Diligent constant-buffer path
- [ ] Texture/sampler binding
- [ ] Depth/blend/cull state handling
- [ ] Draw submission
- [ ] OpenGL debug callback/logging (basic vendor/renderer/version/profile diagnostics already implemented)
- [ ] Pass Phase 2 Windows runtime gate: DLL load/factory, attach, GL >= 4.6, resize, single present, clean shutdown, legacy regression check
- [ ] After runtime gate, replace the temporary `WH_CALLWNDPROC` coexistence bridge with direct lifecycle calls if validation shows no need to retain the bridge

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
