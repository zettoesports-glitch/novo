# Modernization Status

## Active target

Diligent integration for Main 5.2, using shared HLSL shaders and OpenGL 4.6 first. Vulkan and Direct3D 11 remain future active backends using the same contracts.

## Completed

- Phase 1 static source mapping against `13cfc7c3e5dab042e1c3e8f4184e18c26dd08eff`.
- Initial call paths: Hero, remote players, BotBuffer, NPC/monsters, world objects, equipment and inventory preview.
- Geometry, pose, instance, view, material and texture producer/consumer contracts.
- Snapshot/lifetime requirements and legacy render-state mapping.
- Roadmap aligned with the Diligent + HLSL decision.
- Phase 2 ownership contract documented: legacy Main owns HWND/HDC/HGLRC and presentation; Diligent attaches to the active GL context.
- Real Main lifecycle located: `CreateOpenglWindow()`, `CWINHANDLE::WndProc`, `MainScene()` / `LoadingScene()` and `KillGLWindow()`.
- Phase 2 source bridge implemented behind `MU_ENABLE_DILIGENT`: context attach discovery, resize forwarding and pre-destroy shutdown without adding another present path.
- OpenGL 4.6 capability and vendor/renderer/version/GLSL/profile diagnostics implemented in `CModernGraphicsBootstrap`.
- DiligentCore pinned to official `v2.5.6` commit `b036337d68be2353c9950a85929acf796b9a6d50`; reference shader/resource snapshot pinned separately in `DILIGENT_PIN.md`.

Evidence and scope: [PHASE1_MAIN_RENDERER_MAPPING.md](PHASE1_MAIN_RENDERER_MAPPING.md).
Phase 2 contract: [PHASE2_OPENGL46_BOOTSTRAP.md](PHASE2_OPENGL46_BOOTSTRAP.md).
Runtime lifecycle map: [PHASE2_RUNTIME_DISCOVERY.md](PHASE2_RUNTIME_DISCOVERY.md).
Dependency pin: [DILIGENT_PIN.md](DILIGENT_PIN.md).

## Phase 2 in progress

The source-side lifecycle wiring is now present, but the runtime gate is still open because the pinned Diligent dependency has not yet been added to the Win32 project configuration.

Remaining Phase 2 work:

- make DiligentCore v2.5.6 headers/libraries available to `Main.vcxproj` Win32;
- enable `MU_ENABLE_DILIGENT` in a dedicated test configuration;
- build/run on Windows and capture the OpenGL 4.6/Diligent attach diagnostics;
- validate resize, exactly-one-present and shutdown lifetime behavior;
- verify that the legacy renderer still renders correctly during coexistence.

## Not yet validated

No Diligent runtime has yet been proven by a Windows build/GPU run from this branch. No visual-parity test has been performed. The lifecycle wiring is source-complete for the bootstrap but not runtime-certified. All item/map/script/cloth variants remain outside this Phase 2 gate.

## Next action

Wire the pinned Diligent v2.5.6 dependency into the Win32 Main project and run the Phase 2 validation gate. After that, implement concrete CPU/GPU layouts, constant buffers, pose conversion and the two-instance BMD proof before expanding modern rendering to Hero/remote/Bot/NPC/monster.