# Phase A4 — Glass, Windshields, Glow, Opacity Effects

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A1.2 and A0.6 notes. A4 can start before A2/A3 because glass stays forward, but its final review
follows A3 (shadows) and A6 (sky). **Goal:** glass with refraction, reflection and blending (user priority 3), and
emissive glow.

## Tasks

- [ ] **A4.1 — `ASOBO_material_glass_v2` (R3). Owner: Claude.** MSFS glass mapped onto Glider's transmission path:
  refraction and reflection, with blending as MSFS does it. Glider's transmission path samples a mipmapped copy
  of the opaque scene (fixed in `51266d3`).
  - **Open:** what `glassWidth` controls (thickness, tint or refraction) comes from A0.6.
  - **Covers:** the cockpit's glass and the exterior's three materials (`MAT_GLASS`, `_bluish`, `_red`).
  - **Tests:** refraction through a slab of known width; tint; reflection at grazing angles.
- [ ] **A4.2 — `ASOBO_material_windshield_v3` and v1 (R4). Owner: Claude.**
  - Detail roughness and opacity masks, micro-scratch normals, detail-normal refraction.
  - Wiper mask, lines and state; the wiper state is a viewer setting, parked by default.
  - Confirm the `enabled` and `UVScale` keys the E195 data carries.
  - Rain is not simulated (A7).
  - **Reference views:** through the cockpit windshield, and the exterior cabin windshield.
- [ ] **A4.3 — Emissive glow (bloom) (R3). Owner: Claude.** A bloom post-process (threshold, downsample chain,
  intensity) so emissive panels and lights glow as in MSFS.
  - A user setting, on by default for ASOBO files only.
  - Applied before TAA and FXAA; the order is tested.
  - **Tests:** energy and threshold on a fixture of emissive patches; stable under TAA.
- [ ] **A4.4 — `ASOBO_material_fresnel_fade` and `ASOBO_material_ghost_effect` (R2). Owner: GPT Astra.**
  View-angle-dependent opacity, using the A0.6 formulas.
  - **Test:** opacity on a sphere at normal and grazing incidence matches the formula.

**Exit gate:**
- Glass and windshield are reviewed against the reference screenshots.
- Bloom on/off is verified, with files without ASOBO extensions byte-identical.
- Cost is measured.
