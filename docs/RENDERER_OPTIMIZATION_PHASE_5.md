# Phase 5 — Skip Passes and Drawables

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-7"></a>

### 5.7 Phase 5 — Skip Passes and Drawables (R3)

**Entry:** P4.1 and relevant invalidation tests. Each row is a separate change with a baseline fallback
for comparison; fewer draw calls alone do not prove correct visibility or selection.

- [ ] **P5.1 — On-demand full-size picking ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7), [§4.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-5)).** First preserve the existing picking projection
  and framebuffer size, changing only scheduling. Compare clicked IDs and click-to-selection latency
  at fixed coordinates during motion, animation, overlap, resize and multi-viewport use.
- [ ] **P5.2 — Shared small picking target ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7)).** Only after P5.1, introduce the 1×1 sub-frustum and
  remove per-viewport storage. Test edges, thin geometry, alpha masks, DPI and Y-coordinate conversion.
  Match the agreed picking behavior; do not assume a scissor rectangle alone changes the projection.
- [ ] **P5.3 — Cached scene redraw ([§3.11](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-11)).** After P4.3/P4.5/P4.6, skip scene passes while UI still
  renders. Check every dirty trigger, attached cameras, animations and TAA convergence/reset.
- [ ] **P5.4 — Event-driven idle loop ([§3.11](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-11)).** After P5.3, add event waits separately. Verify wakeup
  and response latency for mouse/keyboard, resize, file load, background completion and animation.
- [ ] **P5.5 — Conservative frustum culling ([§3.9](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-9)).** After P4.4, start with static primitives. Keep
  deformed/unsupported bounds on the uncullable path until conservative bounds are proven. Test
  near-plane edges, negative/non-uniform scale, perspective/orthographic views, picking and transmission.

**Exit gate:** unchanged output/selection on deterministic sequences, zero unintended picking passes
without requests, correct redraw wakeups, and measured CPU/GPU/idle benefits. Do not require a fixed
percentage of total draws saved: extra transmission/overlay passes change that denominator.

