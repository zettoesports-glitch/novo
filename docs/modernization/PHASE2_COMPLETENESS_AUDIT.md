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

`attach attempt -> GL diagnostics -> compatibility profile -> optional debug routing -> Diligent attach -> teardown barrier -> shutdown`

It rejects incomplete or reordered cycles. Each successful lifecycle must independently prove `profile=compatibility`; a valid first lifecycle cannot hide a later core-profile lifecycle.

### 2. Recreated WGL contexts

The source bridge intentionally allows a genuinely recreated WGL lifecycle after the previous lifecycle has shut down.

The first hardened parser incorrectly required exactly one attachment in the entire process lifetime. That contradicted the source design and could reject a valid fullscreen/window/context recreation.

The parser now accepts one or more **complete** lifecycle cycles and requires the previous shutdown to finish before the next attach attempt begins. A new attach attempt while a teardown barrier is active is rejected.

The source bridge was also hardened for Windows handle reuse:

- while the dying WGL context is still current, the tracked `HWND/HGLRC` remains blocked;
- after an actual WGL release/unbind is observed, the teardown generation is cleared;
- a newly created live pair may start the next lifecycle even if Windows later recycles the same numeric handle values;
- a failed/inactive initialization attempt is reset when its tracked window enters teardown, so stale attempted-state cannot block a later valid recreated context.

This makes the lifecycle generation boundary stronger than comparing raw handle values alone.

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
- rejection of an incomplete recreated lifecycle;
- rejection of recreated lifecycle evidence whose later cycle is not compatibility profile.

The workflow still performs pinned Diligent preparation, Release/x86 build, Debug/x86 build and output verification before parser validation.

### 6. Legacy OpenGL fallback must not inject GL errors

The bootstrap originally queried `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` before knowing whether the host context supported those enums. On an old OpenGL context that can create `GL_INVALID_ENUM` and contaminate the legacy renderer even though modern initialization correctly falls back.

The audited implementation now:

- parses the universally available `GL_VERSION` string first;
- queries `GL_MAJOR_VERSION` / `GL_MINOR_VERSION` only when the parsed context is OpenGL 3.x or newer;
- queries `GL_SHADING_LANGUAGE_VERSION` only for OpenGL 2.0 or newer;
- avoids reporting a successful modern shutdown after a capability/profile/backend attempt that never actually attached.

An unsupported host therefore falls back without the modern bootstrap deliberately leaving unsupported-enum GL errors behind.

### 7. Failed initialization state and teardown generations

A failed modern attempt still records the `HWND/HGLRC` pair so the bridge does not retry on every message. That state must not survive destruction of the tracked window.

The teardown bridge now resets attempted-but-inactive state for the tracked window. Successful attachments additionally arm the teardown barrier and release Diligent before legacy WGL destruction. This covers both sides:

- no repeated attach spam on the same live unsupported context;
- no stale failed-attempt identity after that context/window is destroyed.

## Source/bootstrap audit result

No additional repository-side blocker was found in the Phase 2 bootstrap after the corrections above:

- Main remains owner of `HWND`, `HDC`, `HGLRC` and legacy presentation;
- Diligent attaches to the active compatibility-profile WGL context;
- OpenGL < 4.6 and core-only contexts fall back safely;
- legacy OpenGL fallback avoids unsupported version/GLSL diagnostic queries;
- Diligent backend load/factory failure falls back safely;
- failed initialization tracking is cleared with the tracked window teardown;
- known post-attach WGL teardown messages are covered by the temporary bridge;
- the teardown generation handles both different context pairs and possible numeric handle recycling after a real WGL release;
- attachment-state guards prevent fallback attempts from masquerading as active modern state;
- resize and teardown are gated on a real active attachment where appropriate;
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

After the real GPU gate passes, reassess the temporary `WH_CALLWNDPROC` bridge. Static inspection shows that the preferred direct owners are already identifiable: attach after the real WGL context is initialized, resize in the window lifecycle, and shutdown before `KillGLWindow()` releases the context. Moving to those direct owners should happen only after runtime evidence proves the coexistence behavior that the temporary bridge is currently protecting.

Only after that Phase 2 boundary should the project activate the next implementation slice: concrete CPU/GPU contracts, Diligent resources, shared-HLSL model pipeline, pose conversion/Skeleton Texture and the first two-independent-instance BMD proof.
