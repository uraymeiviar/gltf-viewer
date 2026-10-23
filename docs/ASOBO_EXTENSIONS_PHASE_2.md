# Phase A2 — Layered BRDF Features

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A1.2 (normal convention, needed by the extra normal maps). **Goal:** MSFS material layers that map
onto shading features `pbr.frag` already has (KHR clear coat, iridescence) or that are self-contained additions.

## Tasks

- [ ] **A2.1 — `ASOBO_material_clear_coat_v2` (R3).** Map onto the `KHR_materials_clearcoat` path:
  - the color and roughness texture (the color tints the layer: confirm in `msfs_material_clearcoat.py`);
  - the clear coat normal texture with its own tiling;
  - roughness and normal factors;
  - `clearcoatInverseRoughness` with `clearcoatBaseRoughness`.

  Tests:
  - a v2 material with white color and no textures matches the equivalent `KHR_materials_clearcoat` material;
  - each property alone.
- [ ] **A2.2 — `ASOBO_material_clear_coat` (v1) (R3).** Clear coat with a dirt mask (`dirtTexture`); confirm how
  the mask attenuates the layer.
- [ ] **A2.3 — `ASOBO_material_iridescent` (R3).** Map onto the `KHR_materials_iridescence` path: the thickness
  texture and range, plus `iridescentBrightness`. Confirm the IOR and factor MSFS assumes.
- [ ] **A2.4 — `ASOBO_material_pearlescent` (R3).** A view-angle-dependent hue shift (`pearlShift`, `pearlRange`,
  `pearlBrightness`). Derive the formula from the exporter's shader graph; ground-truth screenshots are required
  (used by 25 project files).
- [ ] **A2.5 — Anisotropic GGX (`ASOBO_material_anisotropic_v2`, then `ASOBO_material_anisotropic`) (R4).**
  - Add an anisotropic specular lobe and IBL approximation driven by the direction and roughness-Y texture.
  - Implement it as an anisotropy core usable by a future `KHR_materials_anisotropy` too.
  - Tests:
    - zero anisotropy equals the isotropic result;
    - highlight orientation follows the direction map.

**Exit gate:**
- Each extension passes its analytic or equivalence tests and the common checks.
- The visual review against ground truth is recorded.
- The shader-variant count and compile time are measured (each feature is a define, so programs without it
  are unchanged).
