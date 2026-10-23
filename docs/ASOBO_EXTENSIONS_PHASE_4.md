# Phase A4 — Translucency and Special Surfaces

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A0. **Goal:** MSFS shading models that have no glTF core or KHR equivalent.

## Tasks

- [ ] **A4.1 — `ASOBO_material_sail` (R3).** Back-face lighting and translucency controlled by
  `sailLightAbsorption` (0 lets the light through, 1 is opaque). The schema requires double-sided materials:
  report a validation warning when the material is not double-sided.
- [ ] **A4.2 — `ASOBO_material_SSS` (R4).** A subsurface-scattering approximation (wrap lighting and
  translucency tinted by `SSSColor`, thickness from `opacityTexture`). Confirm the model in `msfs_material_sss.py`.
- [ ] **A4.3 — `ASOBO_material_parallax_window` (R4).** Interior mapping:
  - a ray from the eye through the window plane is intersected with a virtual room grid (`roomNumberXY`,
    `roomSizeX/YScale`, `parallaxScale`);
  - the result samples `behindWindowMapTexture`.

  Used by 12 project files. Tests:
  - the room grid stays fixed relative to the surface as the camera moves;
  - the result is stable under TAA.
- [ ] **A4.4 — `ASOBO_material_wind_flex` (R3).** Wind-animated detail normal (`windFlexNormal`). Needs a viewer
  wind setting and the animation clock. Off (static) when animation is paused, so captures stay deterministic.

**Exit gate:**
- Each task passes its geometric tests (sail light transmission, parallax ray-room intersection) and the common
  checks.
- Ground-truth review is recorded where an asset is available.
