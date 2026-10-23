# Phase A5 — Detail Map, UV Options, Parallax Window

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A2.1, the geometry pass. Material additions land in the shared material code, so they work in both
the deferred and forward paths. **Goal:** user priority 5.

## Tasks

- [ ] **A5.1 — `ASOBO_material_detail_map` (R3). Owner: GPT Astra, reviewed by Claude.**
  - **What it does:** detail color, normal and metal-rough-AO textures, tiled by `UVScale` and blended by
    `blendThreshold` and an optional `blendMaskTexture`, using the A0.6 operators.
  - **Normal convention:** detail normals follow A1.2.
  - **Tests:**
    - a neutral detail (mid-grey, flat normal, neutral MRAO) leaves the material unchanged;
    - mask at 0 and at 1;
    - tiling.
  - Used by 11 project files, one of them the E195 cockpit.
- [ ] **A5.2 — `ASOBO_material_UV_options` (R2). Owner: GPT Astra, reviewed by Claude.** UV tiling, offset,
  rotation and clamping (`clampUVX/Y`, default true, overrides the sampler wrap).
  - Which textures it applies to, and how it composes with `KHR_texture_transform`, come from A0.6.
  - **Tests:** each property alone; defaults; composition.
  - E195: one material, tiled 125 × 125.
- [ ] **A5.3 — `ASOBO_material_parallax_window` (R4). Owner: Claude.** Interior mapping: an eye ray through the
  window plane is intersected with a room grid (`roomNumberXY`, `roomSizeX/YScale`, `parallaxScale`) and samples
  `behindWindowMapTexture`.
  - **Tests:**
    - the room grid stays fixed to the surface as the camera moves;
    - stable under TAA.
  - Used by 12 project files.
- [ ] **A5.4 — `ASOBO_material_wind_flex` (R3). Owner: Claude.** Wind-animated detail normal, driven by the
  animation clock; static when animation is paused, so captures stay deterministic.

**Exit gate:**
- Neutral-parameter tests are byte-identical to the base material.
- Real-asset review is done.
- Cost is measured.
