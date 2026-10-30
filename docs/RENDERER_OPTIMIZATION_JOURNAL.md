# Renderer Performance Optimization Journal

**Application:** Glider · **Branch:** `msfs-gltf` · **Updated:** 2026-10-31

Phases 0–9 are complete. Phase 3 was committed as `3c86ff7`. Phase 4: P4.1 and P4.2 passed and were committed as `ebd4384`; P4.3 passed and was committed as `fab477f`; P4.4–P4.7 and a wireframe-overlay fix were committed as `0dd80f3`; the default environment switched in `ab8d174`. P4.6c (the full command-list cache) and the exit-gate review complete Phase 4 (`9e9fcbd`, `1a22976`). Phase 5 is complete (`110c786`): P5.1 on-demand picking, P5.2 shared 1×1 picking target, P5.3 cached scene redraw, P5.4 event-driven idle loop and P5.5 conservative frustum culling all passed, and the exit gate was reviewed ([§12.46](RENDERER_OPTIMIZATION_PHASE_5.md#section-12-46)). Phase 6 is complete (uncommitted): P6.1 validation source reuse and P6.2 bounded image decode passed, P6.3 background validation was deferred, and the exit gate was reviewed ([§12.50](RENDERER_OPTIMIZATION_PHASE_6.md#section-12-50)). Phase 7 is complete (uncommitted). P7.1, P7.3, P7.4 and the P7.5 render scale passed; every change that alters the image is a user setting under **Viewport Settings → Quality**, and the default High preset is byte-identical to before. P7.2 and the P7.5 depth-format change were deferred. The phase also fixed the UI freezing while the window is resized ([§12.56](RENDERER_OPTIMIZATION_PHASE_7.md#section-12-56)). Phase 7 was committed as `ff3f1c7`, and a transmission fix (glass never received its opaque-scene copy) as `51266d3`. Phase 8 is complete (uncommitted) ([§12.61](RENDERER_OPTIMIZATION_PHASE_8.md#section-12-61)): P8.5 hybrid picking, P8.1 uniform submission and uniform blocks, and P8.3 instancing passed with byte-identical images; P8.2 was deferred and P8.4 rejected. A follow-up for three viewports of the E195 cockpit ([§12.62](RENDERER_OPTIMIZATION_PHASE_8.md#section-12-62)) fixed the `settings.ini` lookup, added `ASOBO_material_invisible` support, shared the scene traversal across viewports and skipped redundant uniform and texture uploads: 8.72 → 4.67 ms per frame. Phase 9 is complete (uncommitted) ([§12.68](RENDERER_OPTIMIZATION_PHASE_9.md#section-12-68)): new `overlay.depth` / `post.process` stages and pass counters, C4 overlay-depth and C1 shadow-cascade reuse across TAA settle renders (three E195 viewports with the sky: CPU frame about 65 → 46 ms, draws 6105 → 2221), one startup environment load (C13: `--env` 2 → 1 loads, the procedural sky 1 → 0) and frame-value scene settings without a renderer sync (C2: `scene.dirtySync` 14 → 0 ms per edit); every image byte-identical to `69cf29b`, A/B switch `--disable-pass-cache`. Round 10 (uncommitted): per-viewport transmission copies (C6: 3 texture reallocations, 8.5 MB, per frame → 0; CPU frame 44 → 23 ms) and an asynchronous Auto-exposure meter readback (C3: `post.process` 7.8 → 0.8 ms against the same-binary sync arm; capture runs stay synchronous; `--exposure-readback`) ([§12.71](RENDERER_OPTIMIZATION_PHASE_9.md#section-12-71)), byte-identical to `1db382b`. Round 11 (uncommitted): `createScene` moves its result, `Defines` lookups no longer allocate and the material policy runs once per material per call (C5: `scene.create` 2.6 → 0.9 ms for three E195 viewports); the atmosphere transmittance and the time-of-day evaluation are cached on exact inputs (C7); C9 was measured without gain and left out ([§12.74](RENDERER_OPTIMIZATION_PHASE_9.md#section-12-74)). Byte-identical to `41df669`.
Task lists, dependencies, exit gates, and implementation evidence live in the phase files below. Update the relevant phase file when working on a task.

| Phase | Tasks and implementation history | Status |
|---|---|---|
| 0 | [Baseline and Instrumentation](RENDERER_OPTIMIZATION_PHASE_0.md) | Complete |
| 1 | [Local CPU Optimizations](RENDERER_OPTIMIZATION_PHASE_1.md) | Complete |
| 2 | [UI Caches and Visible Rows](RENDERER_OPTIMIZATION_PHASE_2.md) | Complete |
| 3 | [GL Object Lifetime and Binding Caches](RENDERER_OPTIMIZATION_PHASE_3.md) | Complete |
| 4 | [Invalidation and Persistent Scene Caches](RENDERER_OPTIMIZATION_PHASE_4.md) | Complete |
| 5 | [Skip Passes and Drawables](RENDERER_OPTIMIZATION_PHASE_5.md) | Complete |
| 6 | [Load Pipeline and Background Work](RENDERER_OPTIMIZATION_PHASE_6.md) | Complete |
| 7 | [IBL and Render Quality](RENDERER_OPTIMIZATION_PHASE_7.md) | Complete |
| 8 | [Optional Architectural Work](RENDERER_OPTIMIZATION_PHASE_8.md) | Complete |
| 9 | [Pass Caches, Startup Environment and Frame Values](RENDERER_OPTIMIZATION_PHASE_9.md) | Complete |

Shared documents:

- [Execution rules and validation protocol](RENDERER_OPTIMIZATION_VALIDATION.md) — risk model, focused testing, measurement guidance, and evidence template.
- [Reference audit and historical context](RENDERER_OPTIMIZATION_REFERENCE.md) — original findings, frame execution, open questions, appendices, and historical changelog.

Original section numbers are preserved, and section references link to their new locations. Historical records retain the claims and limitations recorded at the time; later review corrections take precedence.
