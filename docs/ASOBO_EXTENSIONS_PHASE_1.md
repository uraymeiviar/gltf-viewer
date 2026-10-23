# Phase A1 — High-Usage, Low-Cost Semantics

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned. A1.1 implemented (uncommitted) before this journal existed.

**Entry:** A0.1, A0.4, A0.5. **Goal:** the extensions most MSFS assets declare, each a small and well-defined
change to existing shader inputs or draw state.

## Tasks

- [x] **A1.1 — `ASOBO_material_invisible` (R1).** Primitives whose material has the extension with `enabled`
  true (the schema default) are left out of the scene unless the viewport option *Show Invisible Materials* is on
  (`--show-invisible-materials`, `vpsetting <id> invisible on`).
  - Verified: an asset without the extension, `"enabled": false`, and the option switched on all render
    byte-identically to `23f61c6`.
  - E195 cockpit: 448 collider primitives hidden.
  - Harness: `scripts/validate_p8_followup.py`. Move to the A0 model when A0.1 lands.
- [ ] **A1.2 — `ASOBO_normal_map_convention` (R2).** Asset-level `DirectX` inverts the green channel of every
  normal map (and of normal maps added by later phases, such as detail and clear coat) before use. `OpenGL` and
  the absent case are unchanged.
  - Primary test (analytic): a DirectX-flagged map renders byte-identically to the same map with green
    pre-inverted and no extension.
  - Confirm with the exporter whether the convention also applies to `KHR_materials_clearcoat` normals and
    detail normals.
  - Used by 166 of 207 project files.
- [ ] **A1.3 — `ASOBO_material_UV_options` (R2).** UV tiling, offset and rotation, and clamping (`clampUVX/Y`,
  default true, which overrides the sampler's wrap mode).
  - Confirm with the exporter which textures the extension applies to, and how it composes with
    `KHR_texture_transform`.
  - Reuse the existing `HAS_*_UV_TRANSFORM` shader paths.
  - Tests: each property alone, the defaults, and the composition with `KHR_texture_transform`.
- [ ] **A1.4 — `ASOBO_material_draw_order` (R2).** `drawOrderOffset` changes the sort position of blended
  materials and decals within the transparent pass.
  - Confirm the exact ordering rule (an offset added to the distance key, or a priority level) in the
  exporter and SDK docs before coding.
  - Tests: overlapping coplanar blended quads with different offsets, and an unchanged order without the
    extension.
- [ ] **A1.5 — `ASOBO_occlusion_strength` and `ASOBO_extra_occlusion_texture` (R2).** Occlusion strength
  override; a second occlusion map multiplied in.
  - Confirm the combination rule and which UV set the second map uses.
- [ ] **A1.6 — `ASOBO_material_emissive` (R2).** Day/night emissive multipliers selected by a viewer setting
  (Viewport Settings → *MSFS Extensions* → *Time of day: Day / Night*; default Day, pending open question 1).
- [ ] **A1.7 — `ASOBO_material_alphamode_dither` (R3).** Dithered (screen-door) transparency. The material is
  drawn in the opaque pass with a per-pixel discard pattern. Optionally use alpha-to-coverage under MSAA, decided
  in the task.
  - Tests: the pattern is stable under TAA jitter and a different alpha changes the coverage, but there is no
    sort order and no blending.
- [ ] **A1.8 — `MSFT_texture_dds` (R2).** Use the DDS image the extension points to; the core `source` stays the
  fallback.
  - Today DDS files are found only by name substitution in the search paths, so this task makes the
    declared mapping authoritative.
  - Tests: an extension pointing at a DDS file next to a PNG with different content, and a missing DDS
    falling back.

**Exit gate:**
- Each task passes its harness and the A0.5 common checks.
- The E195 cockpit and exterior are reviewed visually with each extension on and off.
- The A1 cost on the real assets is measured (journal §2.1).
