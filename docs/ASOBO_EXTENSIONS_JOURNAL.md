# ASOBO (MSFS) glTF Extension Support Journal

**Application:** Glider · **Branch:** `msfs-gltf` · **Created:** 2026-10-23 · **Status:** Planning

Glider is a glTF 2.0 viewer, not an MSFS renderer. The rule this plan follows:

> When an asset **declares** an extension (`extensionsUsed` or `extensionsRequired`) and the repository has its
> schema, Glider supports the extension with the meaning its specification gives it. An extension that is not
> declared, or that Glider does not support yet, is ignored, and the asset renders from its core glTF data, as
> the glTF specification requires. Rendering that depends on simulator systems Glider does not have (weather,
> time of day, shadows, motion blur, terrain, scenery) is mapped to the closest viewer equivalent, offered as a
> viewer setting, or recorded as not applicable, never silently approximated.

This journal plans support for every ASOBO extension in `resources/schemas/asobo/` (the 38 schemas of the MSFS
2024 SDK) plus the non-schema extensions found in real MSFS assets. Work is split into phases like the renderer
optimization journal ([`RENDERER_OPTIMIZATION_JOURNAL.md`](RENDERER_OPTIMIZATION_JOURNAL.md)).

| Phase | Scope | Status |
|---|---|---|
| A0 | [Foundations: parsed extension model, support matrix, fixtures, ground truth](ASOBO_EXTENSIONS_PHASE_0.md) | Planned |
| A1 | [High-usage, low-cost semantics: normal convention, UV options, draw order, occlusion, emissive, dither, DDS](ASOBO_EXTENSIONS_PHASE_1.md) | Planned (A1.1 `ASOBO_material_invisible` implemented, uncommitted) |
| A2 | [Layered BRDF features on existing KHR paths: clear coat, iridescence, pearlescent, anisotropy](ASOBO_EXTENSIONS_PHASE_2.md) | Planned |
| A3 | [Detail maps, glass and windshields, fresnel/ghost opacity](ASOBO_EXTENSIONS_PHASE_3.md) | Planned |
| A4 | [Translucency and special surfaces: SSS, sail, parallax window, wind flex](ASOBO_EXTENSIONS_PHASE_4.md) | Planned |
| A5 | [Decals and G-buffer blending: geometry decal, blend gbuffer, environment occluder, fake terrain](ASOBO_EXTENSIONS_PHASE_5.md) | Planned |
| A6 | [Scene objects and animation: collision, gizmos, macro lights, softbody, property animation, retargeting](ASOBO_EXTENSIONS_PHASE_6.md) | Planned |
| A7 | [Simulator-only features: shadows, motion blur, responsive AA, rain](ASOBO_EXTENSIONS_PHASE_7.md) | Planned |

Shared documents:

- [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md): every extension, its schema properties, usage across the
  FSS E195 projects, current Glider handling and the phase that owns it.
- [Renderer validation rules](RENDERER_OPTIMIZATION_VALIDATION.md): the risk model (R0–R4), the evidence template
  (§6.6) and the PASS / FAIL / INCONCLUSIVE / NOT RUN / DEFERRED vocabulary. This journal reuses them, with the
  additions in §2 below.
- [MSFS glTF journal](MSFS_GLTF_JOURNAL.md): the earlier inspector and validator work on this branch.

---

## 1. Sources of Truth

Each task must confirm the exact meaning of an extension before implementation, in this order:

1. **The schema** in `resources/schemas/asobo/<extension>/` (identical to `C:\MSFS 2024 Flighting SDK\Schemas`).
   Property names, types, defaults and ranges come from here.
2. **The MSFS 2024 Blender exporter** in
   `C:\MSFS 2024 Flighting SDK\Tools\Blender\addons\io_scene_gltf2_msfs_2024\`:
   `io/com/extensions/material/*.py` (how each property is read and written) and `blender/material/*.py` (the
   shader graph each MSFS material type uses). Example: `decalChan0/1/2` are the scenery, terrain and SimObject
   decal channels (`asobo_material_common.py`), keys the schema does not list.
3. **MSFS SDK documentation** (`C:\MSFS 2024 Flighting SDK` and the online SDK reference) for behavior the
   exporter does not encode.
4. **Ground-truth images** from MSFS itself (§2.3), when a visual comparison is needed.

Where these sources disagree or are silent, the task records the ambiguity and the chosen interpretation in its
evidence record, and asks before shipping an interpretation that changes the image.

---

## 2. Rules for This Journal

The renderer journal's execution rules apply (one task and one mechanism at a time, declare hypothesis and
thresholds before editing, baseline binary, focused validation, no commit without the user's instruction).
In addition:

### 2.1 Correctness gates

- **Assets without the extension:** byte-identical viewport captures to the baseline binary, in all AA modes.
  Supporting an extension must never change an asset that does not declare it.
- **Extension present but not declared:** treated as not declared (ignored), and tested.
- **Defaults:** a property that is absent takes its schema default; tested with a fixture that omits it.
- **Per-extension A/B:** every implemented extension can be switched off at runtime (the viewport's
  *MSFS Extensions* settings, A0.4) so its effect can be inspected. Switched off, the output is byte-identical to
  the baseline.
- **Performance:** no regression beyond noise on assets without the extension; for assets with it, the cost is
  measured and recorded (the renderer journal's timing procedure, §6.3).

### 2.2 Fixtures

Every task adds a deterministic fixture generator, `scripts/generate_asobo_<name>_fixture.py`, that exercises the
extension's properties, defaults and edge values, and a harness, `scripts/validate_asobo_<task>.py`. Where an
analytic equivalence exists it is the primary test. For example, a DirectX-convention normal map must render
byte-identically to the same map with its green channel pre-inverted and no extension.

Real assets (the FSS E195 projects under `F:\new-fss-aircraft-ejet\msfs24-projects`, 207 glTF files) are used as
regression and visual-review inputs. They are not committed; harnesses skip them when absent.

### 2.3 Ground truth

Synthetic fixtures prove the math, not the look. For extensions whose meaning is visual (decals, windshield,
pearlescent, parallax), A0.3 collects reference screenshots of known assets and views from MSFS 2024, captured
by the user. Comparisons against MSFS are reviewed by eye and recorded as *matches / differs because …*; they
are never pixel gates, because MSFS lighting, exposure and post-processing differ from Glider's.

---

## 3. Phase Overview and Dependencies

```
A0 Foundations ──┬── A1 High-usage semantics ──┬── A2 Layered BRDF ── A3 Detail/glass ── A4 Special surfaces
                 │                             └── A5 Decals / G-buffer (design spike first)
                 ├── A6 Scene objects & animation (independent of shading phases)
                 └── A7 Simulator-only features (documentation and viewer settings)
```

- A0 is a prerequisite for all other phases: the typed extension model (A0.1) replaces ad-hoc JSON parsing, and
  the support matrix (A0.2) is where every later task records its status.
- A1 comes first among the rendering phases. It covers the extensions used by the most files (normal convention:
  166 of 207 files; draw order and decals: 143–144 files) at the lowest cost.
- A5 (decals) needs a design decision between forward approximation and a material buffer (G-buffer). Glider's
  renderer is forward-only today. A5.0 is a spike that produces that decision with measurements before any
  decal code.
- A6 and A7 do not touch the PBR shader and can proceed in parallel with A2–A5.

---

## 4. Open Questions for the User

1. **Default of the MSFS-only appearance settings.** Emissive day/night multipliers (A1.6) and wiper state or
   rain (A3, A7) need a viewer setting with a default. Proposed: day, wipers parked, dry.
2. **Collision and gizmo visualization (A6.1).** Off by default and shown as overlays when enabled?
3. **Decal architecture (A5).** Acceptable to add a material buffer (G-buffer) for opaque geometry if the spike
   shows the forward approximation is visibly wrong? It costs memory per viewport and complicates MSAA.
4. **Ground truth.** Can you capture MSFS 2024 screenshots for a short list of E195 views (A0.3)?
5. **MSFT_texture_dds** is used by 14 of the 207 files. It is a Microsoft extension, not ASOBO. Include it in
   A1 as proposed?

---

## 5. Status Log

| Date | Change |
|---|---|
| 2026-10-23 | Journal created (planning only). Inventory of all 38 schemas and of extension usage across 207 E195 project files ([reference](ASOBO_EXTENSIONS_REFERENCE.md)). `ASOBO_material_invisible` was implemented before this journal, during the multi-viewport performance follow-up, and is recorded as A1.1. |
