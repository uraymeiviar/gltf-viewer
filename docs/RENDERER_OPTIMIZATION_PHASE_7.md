# Phase 7 — IBL and Render Quality

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-9"></a>

### 5.9 Phase 7 — IBL and Render-Quality Experiments (R3–R4)

**Entry:** Phase 0 plus a fixed quality reference suite. These are optional experiments, not automatic
follow-ons from cache work; each requires its own quality budget agreed before implementation.

- [ ] **P7.1 — IBL synchronization ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R3).** Evaluate removing the final blocking finish while
  preserving dependency ordering. Measure first presented usable frame and total filter completion,
  not only the apparent duration of the submitting function.
- [ ] **P7.2 — Prebaked default IBL ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R3).** Preserve filter parameters/formats initially; package
  through EmbeddedVFS. Test orientation, roughness mips and sheen. Retain custom HDR/EXR behavior.
- [ ] **P7.3 — IBL precision, dimensions and samples ([§3.12](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-12), R4).** Test half-float, cube/LUT resolution
  and sample count independently. Include bright HDR highlights, smooth metals, rough surfaces and sheen.
- [ ] **P7.4 — Grid algorithm ([§3.13](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-13), R4).** Compare grazing angles, zoom extremes, depth intersections
  and both projection types; record visual tradeoffs as well as fill cost.
- [ ] **P7.5 — MSAA/defaults/render scale ([§3.14](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-14), R4).** Separate allocation-format changes from changes
  to default samples/resolution. Validate depth/stencil requirements, thin edges, transparency, AA and
  picking coordinates. Lower quality is an explicit setting/product choice, not a transparent speedup.

**Exit gate:** quantified memory/startup/GPU benefit and accepted per-experiment image differences.
If no quality budget is agreed, retain the existing defaults and mark the experiment DEFERRED.

