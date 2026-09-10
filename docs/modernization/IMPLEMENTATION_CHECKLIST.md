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

## Renderer core — downstream of the Phase 2 runtime gate
- [ ] Backend enum/factory (backend enum exists; generic backend factory remains a later renderer-core task)
- [ ] Renderer device interface
- [ ] Buffer abstraction
- [ ] Texture abstraction
- [ ] Shader/program abstraction
- [ ] Render state cache
- [ ] Frame/Object/Material data contracts

## OpenGL 4.6 — Phase 2 bootstrap
- [x] Define context/profile ownership and single-presentation rules (static design)
- [x] Locate and wire real Main window/context/resize/present integration points
- [x] Pin Diligent and reference shader revisions (`DILIGENT_PIN.md`)
- [x] Context/capability validation implemented (effective >= 4.6, diagnostics; target-GPU runtime pending)
- [x] Reject OpenGL core-only contexts before Diligent attach; require compatibility profile for legacy coexistence
- [x] Add reproducible pinned DiligentCore Win32/OpenGL/HLSL setup script
- [x] Add official Diligent explicit backend-DLL loading (`GraphicsEngineOpenGL_32r/32d.dll`)
- [x] Preserve dependency-absent, unsupported-GL, core-only-profile and DLL-load-failure legacy fallback
- [x] Align Main Debug definitions with the Diligent debug backend selection
- [x] Normalize Main Debug/x86 to C++17 without modifying DiligentCore project settings
- [x] Add GitHub Actions Windows/x86 compile gate (`.github/workflows/phase2-win32-build.yml`)
- [x] Execute pinned Diligent setup successfully on Windows and produce both `_32r` and `_32d`
- [x] Compile `Main.sln` Release/x86 with the pinned Diligent headers enabled
- [x] Compile `Main.sln` Debug/x86 with C++17 and the pinned Diligent headers enabled
- [x] Preserve a single legacy `SwapBuffers` presentation owner; no modern swap chain/present added
- [x] Cover all currently known post-attach WGL teardown messages (`WM_CLOSE`, `WM_DESTROY`, `WM_NCDESTROY`, `WM_USER_MEMORYHACK`) before `KillGLWindow()`
- [x] Confirm `KillGLWindow()` calls inside `CreateOpenglWindow()` are pre-attach initialization-failure cleanup paths
- [x] Add persistent `Client_2/ModernGraphics.log` evidence for attach/capability/resize/shutdown and bridge failures
- [x] Route Diligent validation and OpenGL/KHR_debug through `IEngineFactory::SetMessageCallback()` when `MU_MODERN_GL_DEBUG=1`, without installing a competing raw GL callback in Main
- [x] Add reproducible local GPU validation script (`run_phase2_runtime_test.ps1`)
- [x] Require compatibility profile evidence and normal `Main.exe` exit in the real GPU validation path
- [x] Add CI `-ValidateOnly` coverage for the runtime-evidence parser using compatibility-profile synthetic evidence
- [x] Re-audit lifecycle/runtime gate after `WM_USER_MEMORYHACK` hardening (run `34540959623`, source commit `d24e117270113918d877e17abd4609cad236d92e`)
- [x] Pass the latest source-affecting combined Release+Debug x86 gate after enforcing compatibility profile in the bootstrap (run `34542334616`, source commit `e6c0a678ecbb870262d62ed362902ce999adb06c`)
- [ ] Pass the real Phase 2 Windows/GPU runtime gate: DLL load/factory, attach, GL >= 4.6 compatibility profile, validation/debug routing, resize, single-present behavior, clean shutdown and legacy regression check
- [ ] After the real runtime gate, replace/reassess the temporary `WH_CALLWNDPROC` coexistence bridge with direct lifecycle calls if validation shows no need to retain the bridge

## First modern draw — starts only after the Phase 2 GPU gate
- [ ] Diligent buffer/texture lifecycle and transitions
- [ ] Diligent HLSL shader compilation diagnostics
- [ ] Diligent constant-buffer path
- [ ] Texture/sampler binding
- [ ] Depth/blend/cull state handling
- [ ] Draw submission

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