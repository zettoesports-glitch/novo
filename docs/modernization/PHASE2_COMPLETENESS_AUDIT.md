# Phase 2 Completeness Audit

Date: 2026-09-10  
Branch: `modernization`

This document is the corrective audit for the Phase 2 Diligent/OpenGL 4.6 bootstrap. When older wording conflicts with this file, this audit and `STATUS.md` are authoritative.

## Scope

The audit checked the implemented bootstrap rather than only the plan:

- `ModernGraphicsBootstrap.h/.inl`;
- the temporary `WH_CALLWNDPROC` lifecycle bridge in `CShaderGL.cpp`;
- `setup_diligent_opengl46.ps1`;
- `run_phase2_runtime_test.ps1`;
- `.github/workflows/phase2-win32-build.yml`;
- the mapped WGL lifecycle and legacy presentation paths in Main 5.2;
- modernization status/checklist documentation.

## Findings closed during the audit

### 1. Runtime evidence ordering

The earlier runtime parser could accept required markers without proving a valid lifecycle order.

The gate now validates complete ordered cycles:

`attach attempt -> GL diagnostics -> optional debug routing -> Diligent attach -> teardown barrier -> shutdown`

It rejects incomplete or reordered cycles.

### 2. Recreated WGL contexts

The source bridge intentionally allows a genuinely different `HWND/HGLRC` pair to start a fresh modern lifecycle after the previous lifecycle has shut down.

The first hardened parser incorrectly required exactly one attachment in the entire process lifetime. That contradicted the source design and could reject a valid fullscreen/window/context recreation.

The parser now accepts one or more **complete** lifecycle cycles and requires the previous shutdown to finish before the next attach attempt begins. A new attach attempt while a teardown barrier is active is rejected.

The source-level bridge remains responsible for the identity rule that prevents reattaching to the same tracked dying `HWND/HGLRC` pair.

### 3. Resize evidence

When `-RequireResize` is used, at least one resize marker is required and every recorded resize must occur between a successful attachment and its matching teardown barrier. A fallback-only initialization attempt cannot satisfy resize evidence.

### 4. Presentation wording corrected

The legacy Main does **not** have only one textual `SwapBuffers` call site.

The audited `ZzzScene.cpp` contains two legacy presentation call sites:

1. `LoadingScene(HDC)` flushes and swaps after rendering the loading scene.
2. The regular scene flow flushes and swaps when its render `Success` flag is true.

This does not create a second modern presentation owner. Phase 2 still creates no Diligent swap chain and calls no modern `Present`/`SwapBuffers`; presentation remains entirely application/legacy-owned.

The correct Phase 2 claim is therefore:

- **proven statically:** there is no Diligent/modern presentation path in the Phase 2 bootstrap;
- **not measured by the current log parser:** an exact per-frame count of legacy `SwapBuffers` calls.

The real GPU gate must verify visual/frame behavior, but it must not claim that the current log parser counts presentation calls.

### 5. CI parser coverage

The permanent Windows/x86 workflow now tests:

- a valid single lifecycle;
- valid complete lifecycle recreation;
- rejection of shutdown-before-teardown evidence;
- rejection of an attach attempt during teardown;
- rejection of an incomplete recreated lifecycle.

The workflow still performs pinned Diligent preparation, Release/x86 build, Debug/x86 build and output verification before parser validation.

## Source/bootstrap audit result

No additional repository-side blocker was found in the Phase 2 bootstrap after the corrections above:

- Main remains owner of `HWND`, `HDC`, `HGLRC` and legacy presentation;
- Diligent attaches to the active compatibility-profile WGL context;
- OpenGL < 4.6 and core-only contexts fall back safely;
- Diligent backend load/factory failure falls back safely;
- known post-attach WGL teardown messages are covered by the temporary bridge;
- attachment-state guards prevent fallback attempts from masquerading as active modern state;
- resize and teardown are gated on a real active attachment;
- Diligent state invalidation is available at raw-GL -> Diligent boundaries;
- persistent diagnostics and optional Diligent validation routing are implemented;
- generated dependencies/runtime evidence remain excluded from source control.

## What remains genuinely incomplete

Only work that requires the target interactive Windows/GPU environment remains before Phase 2 can be called runtime-certified:

- execute Main on a machine exposing OpenGL >= 4.6 compatibility profile;
- observe the real Diligent DLL/factory load and `AttachToActiveGLContext` result;
- confirm vendor/renderer/version/GLSL/profile evidence from the real driver;
- run with `MU_MODERN_GL_DEBUG=1` and verify validation/debug routing;
- exercise resize and, if available, a mode/context recreation;
- close through normal client lifecycle and verify ordered teardown/shutdown;
- visually confirm legacy scenes have no coexistence regression.

The current hosted Windows CI cannot substitute for that interactive GPU/WGL run.

## Post-GPU action

After the real GPU gate passes, reassess the temporary `WH_CALLWNDPROC` bridge. The preferred end state is direct calls in the already-mapped lifecycle owners when runtime evidence shows that doing so is safe.

Only after that Phase 2 boundary should the project activate the next implementation slice: concrete CPU/GPU contracts, Diligent resources, shared-HLSL model pipeline, pose conversion/Skeleton Texture and the first two-independent-instance BMD proof.
