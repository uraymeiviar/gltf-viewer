# ASOBO (MSFS) glTF Extension Support Journal

**Application:** Glider · **Branch:** `msfs-gltf` · **Created:** 2026-10-23 · **Revised:** 2026-10-23 · **Status:** Planning

## Purpose

Glider is used in-house as an **MSFS asset tester**. After an artist exports a model from Blender, they inspect the
glTF in Glider instead of running a slow test in MSFS 2024. Glider therefore has to render and behave as close to
MSFS as practical, while still opening any glTF.

**Decisions (user, 2026-10-23):**

1. **Fidelity over conformance.** For files that declare ASOBO extensions, the target is MSFS's appearance. Exact
   identity is impossible, so the target is close enough that an artist catches the same mistakes in Glider as in
   MSFS. glTF-conformant appearance is not a constraint.
   - Separate rendering profiles (glTF reference / MSFS) are parked for later.
   - Files without ASOBO extensions keep rendering exactly as today, byte-identical (§2.1).
2. **Only the glTF is read.** No MSFS XML (model behaviors), `model.cfg`, LOD sets or `systems.cfg`.
3. **Animations already work.** Artists check that every frame of each exported clip is correct, and Glider plays
   clips end to end. `ASOBO_property_animation` is low priority: the team does not use it, so it cannot be tested
   on real data.
4. **LOD is out of scope** for now (it involves multiple glTF files).
5. **Priorities:**
   1. Basic materials close to MSFS: occlusion/roughness/metallic (ORM), emissive, normals.
   2. Draw order (materials carry explicit offsets) and visibility.
   3. Glass (refraction, reflection, blending) and decals.
   4. Shadows with a sun, whose direction and position later come from time of day.
   5. Detail map, UV repeat, parallax window.
   6. A dynamic, configurable time of day emulating MSFS.
6. **Decals use a deferred path** (G-buffer for opaque geometry); forward emulation cannot reproduce
   normal-, roughness- or occlusion-only decals ([Phase A2](ASOBO_EXTENSIONS_PHASE_2.md)).
7. **Ground truth:** the team can capture MSFS 2024 reference screenshots.

## Phases

| Phase | Scope | Status |
|---|---|---|
| A0 | [Foundations: typed extension model, support matrix, fixtures, harness, reference screenshots](ASOBO_EXTENSIONS_PHASE_0.md) | Planned |
| A1 | [Basic materials and draw order: normal convention, ORM/occlusion, emissive, draw order, dither, DDS](ASOBO_EXTENSIONS_PHASE_1.md) | Planned (A1.1 invisible materials: done, `00274ce`) |
| A2 | [Deferred opaque path and decals](ASOBO_EXTENSIONS_PHASE_2.md) | Planned |
| A3 | [Sun, shadows, macro lights, SSS](ASOBO_EXTENSIONS_PHASE_3.md) | Planned |
| A4 | [Glass, windshields, glow (bloom), fresnel and ghost opacity](ASOBO_EXTENSIONS_PHASE_4.md) | Planned |
| A5 | [Detail map, UV options, parallax window, wind flex](ASOBO_EXTENSIONS_PHASE_5.md) | Planned |
| A6 | [Time of day: sky, dynamic environment lighting, exposure](ASOBO_EXTENSIONS_PHASE_6.md) | Planned |
| A7 | [Remaining extensions: layered BRDFs, overlays, metadata, simulator-only](ASOBO_EXTENSIONS_PHASE_7.md) | Planned |

Shared documents:

- [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md): every extension, its schema, its usage across the FSS
  E195 projects, current handling and owning phase.
- [Renderer validation rules](RENDERER_OPTIMIZATION_VALIDATION.md): risk model R0–R4, evidence template §6.6, and
  the PASS / FAIL / INCONCLUSIVE / NOT RUN / DEFERRED vocabulary. These apply here, with the additions in §2.
- Regression tooling: `scripts/save_baseline.py`, `scripts/run_regression.py` (`AGENTS.md` §4.5).

---

## 1. Sources of Truth

Before implementing an extension, confirm what it means, in this order:

1. **The schema** in `resources/schemas/asobo/<extension>/` (identical to the MSFS 2024 SDK `Schemas`).
2. **The MSFS 2024 Blender exporter**,
   `C:\MSFS 2024 Flighting SDK\Tools\Blender\addons\io_scene_gltf2_msfs_2024\`:
   - `io/com/extensions/material/*.py`: how each property is written;
   - `blender/material/*.py`: the shader graph each MSFS material type uses.

   Example: `decalChan0/1/2` are the scenery, terrain and SimObject decal channels, which the schema does not
   list.
3. **MSFS SDK documentation.**
4. **MSFS reference screenshots** (§2.3).

Each task records ambiguities and the interpretation chosen in its evidence record. These spec extractions are the
main work handed to DeepSeek ([§4](#4-work-allocation)).

---

## 2. Rules

The renderer journal's rules apply:
- one task and one mechanism at a time;
- hypothesis and thresholds declared before editing;
- a baseline binary;
- no commit without the user's instruction.

In addition:

### 2.1 Correctness gates

- **Files without ASOBO extensions:** byte-identical to the saved baseline (`scripts/run_regression.py`) in every
  task. New paths, including the deferred path in A2, are used only for files that declare ASOBO extensions.
- **Extension declared:** the rendering is expected to change. It is judged by:
  - analytic and fixture tests of the math (each task lists them);
  - side-by-side review against MSFS reference screenshots (§2.3).
- **Per-extension switch:** each rendered extension has a viewport switch (A0.4). With everything off, a file
  renders as it did before this journal.
- **Performance:** measured on the three-viewport E195 setup (`validate_p8_followup.py --bench`) for every phase
  that adds GPU work. There is no silent quality reduction; any expensive feature gets a quality setting.

### 2.2 Fixtures

- **Per task:** each task adds a fixture generator `scripts/generate_asobo_<name>_fixture.py` and checks in
  `scripts/validate_asobo_<task>.py`, built on the A0.5 template.
- **Real assets:** the FSS E195 projects, 207 glTF files, are regression and review inputs. They are not
  committed; harnesses skip them when absent.

### 2.3 Reference screenshots

The team captures MSFS 2024 screenshots of a fixed list of E195 cockpit and exterior views, with fixed time of
day and weather (list produced in A0.3). Glider renders the same views with matching camera flags. Reviews
record *matches* or *differs because …*. They are judgments, not pixel gates.

---

## 3. Phase Overview

```
A0 ── A1 ── A2 (deferred + decals) ── A3 (sun + shadows) ── A6 (time of day)
        └── A4 (glass, bloom)  ── A5 (detail, UV, parallax)
A7 (remaining) proceeds in parallel where it touches no shared shader code.
```

- **Why A2 comes before shadows (A3):** the sun and shadow lookups are cheaper and simpler in a deferred lighting
  pass.
- **Why A6 comes last:** it replaces the fixed HDR environment with a sky driven by the A3 sun. Tuning A4/A5
  materials before A6 is acceptable because A6 changes lighting, not material math. The reference screenshots
  are rechecked after A6.

---

## 4. Work Allocation

Four agents share the work. GPT Astra (Codex), Gemini Argon (via the Antigravity CLI, `agy`) and DeepSeek
(DeepSeek-V4.1-Flash on Max effort, via the DeepSeek Harness CLI, `dsh`) have small quotas, so they get bounded tasks with a clear specification and acceptance check. Claude keeps the architecture
and the renderer-critical paths, and reviews and integrates every handoff.

| Task | Owner | Why |
|---|---|---|
| **Spec extraction** (A0.6): read the SDK Blender exporter and write precise semantics for A1–A5 extensions | **DeepSeek** | Reading a large Python codebase and shader graphs; produces text, no code risk. Moved from Argon, whose weekly quota is the smallest (2026-10-24) |
| A0.3 reference-view list and camera flags for MSFS screenshots | **Gemini Argon** | Small, documentation-only |
| A0.1 typed extension model, A0.4 switches, A0.5 harness template | Claude | Shared foundation every later task builds on |
| A0.2 support matrix and inspector badges | **GPT Astra** | Self-contained UI work against the A0.1 interface |
| Fixture generators (`scripts/generate_asobo_*_fixture.py`) for A1 | **GPT Astra** | Pure Python with clear schema-driven specs |
| A1.2 normal convention, A1.4 draw order, A1.5 occlusion, A1.6 emissive | Claude | Shader and sort changes on the hot path |
| A1.7 alpha dither, A1.8 `MSFT_texture_dds` | **GPT Astra** | Small, well-bounded, testable in isolation |
| A2 deferred path and decals; A3 sun and shadows; A4 glass and bloom; A6 time of day | Claude | Architecture, performance and image correctness |
| A5.1 detail map, A5.2 UV options | **GPT Astra**, after A2 lands | Material-shader additions with neutral-parameter tests; reviewed by Claude |
| A7.4 collision and gizmo overlays | **GPT Astra** | Follows the existing bounding-box overlay pattern |
| A7 classification of simulator-only extensions | **Gemini Argon** | Research and documentation |

**Handoff protocol.** Each handoff brief names the task ID, the files to touch, the spec section to follow and the
acceptance check: the task's harness plus `scripts/run_regression.py` against the saved baseline. Delegated agents
do not commit. Claude reviews the diff, runs the checks and records the result in the phase file. If Astra's or
Argon's quota runs out, the task returns to Claude unchanged.

---

## 5. Status Log

| Date | Change |
|---|---|
| 2026-10-23 | Journal created (planning only): inventory of 38 schemas and of usage across 207 E195 project files. `ASOBO_material_invisible` implemented earlier, during the multi-viewport follow-up (`00274ce`); recorded as A1.1. |
| 2026-10-23 | Revised after the user's decisions (Purpose): MSFS fidelity is the target, only the glTF is read, animation and LOD are out of scope, and decals use a deferred path. Phases reordered; work allocation added (§4). |
| 2026-10-24 | A0 started. A0.1 typed extension model (Claude) and A0.6 exporter semantics (DeepSeek, `ASOBO_EXTENSIONS_SEMANTICS.md`) done; A0.3 reference views running on Gemini Flash. Details in [Phase A0](ASOBO_EXTENSIONS_PHASE_0.md). |
| 2026-10-24 | A0 tasks done: A0.2 support matrix (GPT Astra), A0.3 views (Gemini Flash), A0.4 viewport switches and A0.5 harness with `--search-path` (Claude). The MSFS screenshots are still to come. The regression runner now runs the ASOBO harnesses one at a time after the parallel batch, because their E195 renders upset `validate_p8_5.py`'s edge clicks. |
