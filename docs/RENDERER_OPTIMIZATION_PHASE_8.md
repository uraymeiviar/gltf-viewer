# Phase 8 — Optional Architectural Work

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-10"></a>

### 5.10 Phase 8 — Optional Architectural Work (R3–R4)

**Entry:** measured bottleneck remains after relevant earlier work; do not implement for completeness.

- [ ] **P8.1 — UBOs ([§3.10](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-10), [§3.17](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-17), R4).** After P4.5/P4.6, migrate camera/environment/light data one
  block at a time; evaluate skin storage separately after P4.7. Query limits, verify layouts and retain
  a large-skin fallback. Do not require SSBOs or raise the OpenGL 4.1 floor.
- [ ] **P8.2 — Dynamic debug mode ([§3.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-5), R4).** Compare first-switch shader compilation, permutation
  count and steady-state fragment cost. Validate every supported debug mode and multi-viewport mix;
  fewer variants may increase shader work and is not sufficient evidence by itself.
- [ ] **P8.3 — Instancing/static batching ([§3.9](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-9), R4).** After P4.6/P5.5, start with compatible opaque
  static geometry. Preserve transforms, bounds, transparency order and per-node picking identity.
- [ ] **P8.4 — Shader binary cache ([§4.4](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-4), R3).** Capability-gate binary support, include driver/program
  inputs in keys, handle stale/corrupt data, and always retain source compile fallback. Confirm the
  optional writable-cache policy before adding disk persistence; the executable must run without it.
- [ ] **P8.5 — CPU/hybrid picking ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7), R4).** Only if P5.1/P5.2 leave a measured problem. Define and
  test equivalence for alpha masks, skin/morph deformation, depth order and selection identity.

**Exit gate:** task-specific correctness suite and repeatable gains justify the extra architecture.
Reject or defer complexity whose benefit is not demonstrated on representative workloads.

---

