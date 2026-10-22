# Phase 6 — Load Pipeline and Background Work

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started. 

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-8"></a>

### 5.8 Phase 6 — Load Pipeline and Background Work (R2 then R3)

**Entry:** Phase 0 load-stage metrics. These tasks need not wait for Phase 5.

- [ ] **P6.1 — Validation source reuse ([§3.26](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-26), R2).** Preserve original bytes/container diagnostics and
  report target mapping; compare complete normalized reports and peak RAM on valid/malformed assets.
- [ ] **P6.2 — Bounded parallel image decode ([§3.16](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-16), R3).** Audit decoder/thread-safety and logging first.
  Bound concurrency by memory as well as CPU count; keep GL uploads on the context thread. Test all
  supported formats, failures/cancellation, reload and peak RAM. Use standard threads/a lightweight
  queue; do not assume `std::execution::par` avoids a TBB/backend dependency.
- [ ] **P6.3 — Background validation ([§3.26](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-26), R3).** Only if P6.1 still leaves meaningful UI blocking.
  Use immutable inputs, generation tokens and main-thread target resolution/publication. Test closing
  or replacing an asset before completion and stale results. Keep pending/failed status distinguishable
  from valid. Compare both responsiveness and total validation completion time.

**Exit gate:** same decoded/validated results, no stale publication or GL calls on workers, bounded
peak memory, and repeatable load/response improvements. More threads are not an acceptance criterion.

