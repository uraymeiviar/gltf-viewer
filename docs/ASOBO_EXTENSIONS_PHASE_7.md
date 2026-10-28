# Phase A7 — Remaining Extensions

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A0. Tasks proceed when they touch no shader code another phase is changing. **Goal:** support or
classify every remaining extension, so the support matrix has no unknown entry.

## Tasks

- [ ] **A7.1 — Layered BRDF features (R3–R4). Owner: Claude.**
  - `ASOBO_material_clear_coat_v2` and v1: map onto the `KHR_materials_clearcoat` path, with the color/roughness
    texture, tilings, inverse roughness and the v1 dirt mask.
  - `ASOBO_material_iridescent`: map onto `KHR_materials_iridescence`.
  - `ASOBO_material_pearlescent`: a view-dependent hue shift (25 project files use it).
  - [x] `ASOBO_material_anisotropic_v2` and v1: an anisotropic GGX lobe. **Done (v2), 2026-10-28:** v2 maps onto
    the new `KHR_materials_anisotropy` lobe (Sample-Viewer anisotropic GGX for punctual lights, bent-normal IBL):
    direction = RG x 2 - 1 in tangent space (0.5/0.5 = tangent), roughness X = core roughness along it, Y = texture B
    across it; X == Y is byte-identical to the isotropic material. Generated tangents also serve anisotropic
    materials. Per-viewport switch; `KHR_materials_anisotropy` has the Inspector display switch. v1 is deferred: its
    packing is not established (semantics section 17). Check: `scripts/validate_asobo_anisotropy.py` (quick
    regression).
  - **Tests:** each must equal its KHR counterpart at neutral settings.
- [ ] **A7.2 — `ASOBO_material_sail` (R3). Owner: Claude.** Back-face lighting and translucency by
  `sailLightAbsorption`. Warn when the material is not double-sided.
- [ ] **A7.3 — `ASOBO_property_animation` (R3). Owner: Claude, low priority.** Map its channels onto the existing
  `KHR_animation_pointer` support. The team does not use it, so it needs a synthetic fixture.
  - **Test:** frame-by-frame equality with the equivalent `KHR_animation_pointer` animation.
- [ ] **A7.4 — Collision and gizmo overlays (R1). Owner: GPT Astra.** For `ASOBO_collision_object`,
  `ASOBO_gizmo_object` and `ASOBO_tags`: draw the box, cylinder and sphere shapes as overlays, following the
  bounding-box renderer, colored by type and tag.
  - Off by default; switched per viewport.
  - **Test:** correct under animated parents.
- [ ] **A7.5 — Metadata (R1). Owner: GPT Astra.** `ASOBO_unique_id` and `ASOBO_animation_retargeting` ids in the
  inspector and hierarchy search; `ASOBO_softbody_mesh` and `ASOBO_asset_optimized` shown as metadata.
- [ ] **A7.6 — Simulator-only classification (R0). Owner: Gemini Argon.** Record a decision for each:
  - `ASOBO_material_disable_motion_blur`: not applicable, since Glider has no motion blur.
  - `ASOBO_material_rain_options`: not applicable, or a later wet-surface setting.
  - `ASOBO_material_antialiasing_options`: applies to TAA; implemented as A7.7.
  - `ASOBO_material_environment_occluder`, `ASOBO_material_fake_terrain`: with A2.6.
- [ ] **A7.7 — `ASOBO_material_antialiasing_options` (R3). Owner: Claude.** `responsiveAA` reduces TAA history for
  the material: a responsiveness mask written in the scene pass and read by the TAA resolve.

**Exit gate:** every extension in the reference has a support-matrix status other than *Not yet supported*, or an
explicit deferral with its reason.
