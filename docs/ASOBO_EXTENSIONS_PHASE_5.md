# Phase A5 — Decals and G-Buffer Blending

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A1.4 (draw order). **Goal:** correct decal compositing.
- **Usage:** `ASOBO_material_geometry_decal` is the second most used material extension (486 objects in 144
  of 207 project files; 7 cockpit and 4 exterior materials).
- **Current behavior:** Glider draws decals as ordinary blended materials. Their own lighting covers the surface
  below, instead of changing only selected channels of it.

## Background

MSFS composites decals into its deferred material buffer. A decal replaces each channel of the surface below
(base color, metallic, roughness, normal, emissive, occlusion) by its blend factor, and the lighting is computed
once afterwards.

Glider is a forward renderer. Each object is shaded completely, and the only stored output is the final color
plus depth. There is no material buffer (G-buffer).

## Tasks

- [ ] **A5.0 — Design spike (R0, no shipped code).** Prototype both options on the E195 decal sheets and record
  the image difference, cost and memory of each:
  1. **Forward approximation:** the decal is shaded with its own material and alpha-blended per channel, with
     factors applied as alpha. It cannot read the surface's material, so channels the decal does not cover can
     only be preserved by not drawing them.
  2. **Material buffer for opaque geometry:**
     - the opaque pass writes normal, base color, metallic/roughness/occlusion and emissive into extra targets;
     - decals blend into those targets with per-channel factors (separate blend state per target);
     - a full-screen pass lights the result;
     - transparent and transmissive materials stay forward.

     Cost: extra full-size targets per viewport (about 4 × RGBA8/RGBA16F), a lighting pass, harder MSAA
     (per-sample lighting or resolve-before-light), and a possible TAA interaction.

  Output: a recommendation and the measured numbers, for the user to decide (journal open question 3).
- [ ] **A5.1 — `ASOBO_material_geometry_decal` (R4).** Implement the chosen design:
  - every blend factor and its schema default;
  - `mode`;
  - the `decalChan0–2` channel flags, interpreted for a viewer, which has scenery and SimObject but no terrain.

  Tests:
  - factor 0 leaves the surface unchanged;
  - factor 1 replaces that channel only;
  - draw order between overlapping decals (A1.4).
- [ ] **A5.2 — `ASOBO_material_blend_gbuffer` (R4).** The same channel blending for non-decal geometry, on the
  same mechanism.
- [ ] **A5.3 — `ASOBO_material_environment_occluder` and `ASOBO_material_fake_terrain` (R2).** Confirm their
  meaning first (`asobo_material_environment_occluder.py`, `asobo_material_fake_terrain.py`).
  - Expected: simulator compositing hints. These map to *Not applicable* or a viewer setting rather than
    visible changes.
  - Record the decision in the support matrix.

**Exit gate:**
- The user approves the A5.0 decision.
- Decal tests pass.
- Assets without decals stay byte-identical. If a material buffer is added, they still render through the
  forward path, or the change is proven byte-identical for them.
- The memory and frame cost per viewport are recorded.
