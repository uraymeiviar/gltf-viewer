# Phase A3 — Detail Maps, Glass and Windshields, Opacity Effects

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A1.2, A1.3. **Goal:** the surface-detail and glass shaders MSFS aircraft rely on (detail map: 11
project files; glass: 23; windshield: 16).

## Tasks

- [ ] **A3.1 — `ASOBO_material_detail_map` (R3).** Detail color, normal and metal-rough-AO textures tiled by
  `UVScale`, blended over the base by `blendThreshold` and an optional `blendMaskTexture`.
  - Derive the exact blend operators from `msfs_material_standard.py` and the shader graph.
  - Detail normals follow A1.2.
  - Tests:
    - a neutral detail (mid-grey color, flat normal, neutral MRAO) leaves the material unchanged;
    - the mask at 0 and at 1;
    - the tiling.
- [ ] **A3.2 — `ASOBO_material_glass_v2` (R3).** MSFS glass: confirm what `glassWidth` controls (thickness
  tint or refraction) in `msfs_material_glass.py`. Map onto the transmission/volume path where it applies;
  otherwise keep a blended path.
  - Covers the cockpit's single glass material and the exterior's three (`MAT_GLASS`, `_bluish`, `_red`).
- [ ] **A3.3 — `ASOBO_material_windshield_v3` (R4).** Detail roughness and opacity masks, micro-scratch normals,
  detail normal refraction, wiper mask and lines.
  - The wiper state is a viewer setting (default: parked).
  - Rain is not simulated (A7).
  - Confirm the `enabled` and `UVScale` keys the E195 data carries but the schema does not list.
  - Ground truth required: cockpit view through the windshield and exterior view of the cabin windshield.
- [ ] **A3.4 — `ASOBO_material_windshield` (v1) (R3).** Wiper mask with four wiper states; rain drop scale not
  applicable.
- [ ] **A3.5 — `ASOBO_material_fresnel_fade` and `ASOBO_material_ghost_effect` (R2).** View-angle-dependent opacity
  from (`fresnelFactor`, `fresnelOpacityOffset`) and (`bias`, `scale`, `power`). The formulas come from the
  exporter.
  - Tests: opacity at normal and grazing incidence matches the formula on a sphere.

**Exit gate:**
- Neutral-parameter tests are byte-identical to the base material.
- The E195 windshield and glass are reviewed against ground truth.
- The real-asset cost is measured.
