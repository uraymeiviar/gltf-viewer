# Phase A1 — Basic Materials and Draw Order

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned. A1.1 done (`00274ce`).

**Entry:** A0.1, A0.4, A0.5, and A0.6 notes for each task's extension. **Goal:** occlusion/roughness/metallic,
emissive and normals close to MSFS (user priority 1), and the explicit draw order artists export (priority 2).

## Tasks

- [x] **A1.1 — `ASOBO_material_invisible` (R1).** Materials that declare it with `enabled` true (the default) are
  not rendered unless the viewport's *Show Invisible Materials* is on.
  - Verified: byte-identical to `23f61c6` with the option on, and with `"enabled": false`.
  - Harness: `scripts/validate_p8_followup.py`.
  - Moves onto the A0.1 model.
- [ ] **A1.2 — `ASOBO_normal_map_convention` (R2). Owner: Claude.** An asset-level `DirectX` convention inverts
  the green channel of every normal map before use, including normal maps added by later phases (detail, clear
  coat, windshield).
  - **Test (analytic):** a DirectX-flagged map renders byte-identically to the same map with green pre-inverted
    and no extension.
  - Every one of the 166 project files that declares it uses DirectX.
- [ ] **A1.3 — ORM fidelity check (R1). Owner: Claude.** MSFS's `_COMP` texture uses the same packing as glTF's
  occlusion/roughness/metallic texture (R occlusion, G roughness, B metallic). Confirm Glider reads it the same
  way, including:
  - the occlusion texture and its strength;
  - roughness and metallic factors;
  - sRGB versus linear decoding.

  Compare against the reference screenshots. Expected outcome: no code change, or a documented difference.
- [ ] **A1.4 — `ASOBO_material_draw_order` (R2). Owner: Claude.** `drawOrderOffset` sets the order of blended
  materials and decals.
  - Implement the ordering rule from A0.6, for example a priority applied before the distance sort.
  - **Tests:**
    - overlapping coplanar blended quads with different offsets;
    - unchanged order without the extension.
  - Used by 143 project files.
- [ ] **A1.5 — `ASOBO_occlusion_strength` and `ASOBO_extra_occlusion_texture` (R2). Owner: Claude.** An occlusion
  strength override, and a second occlusion map combined by the A0.6 rule.
- [ ] **A1.6 — Emissive (R2). Owner: Claude.**
  - `ASOBO_material_emissive` day/night multipliers. Day is used until A6 adds time of day; the viewer setting
    *Time of day: Day / Night* exists from this task on.
  - MSFS emissive intensity convention: confirm how the exporter folds emissive strength into `emissiveFactor`
    (`asobo_material_common.py::_extract_emissive_values`).

  Glow (bloom) is A4.3.
- [ ] **A1.7 — `ASOBO_material_alphamode_dither` (R3). Owner: GPT Astra.** Dithered (screen-door) transparency in
  the opaque pass, using a per-pixel discard pattern.
  - **Tests:**
    - the pattern is stable under TAA jitter;
    - coverage scales with alpha;
    - no sorting and no blending.
- [ ] **A1.8 — `MSFT_texture_dds` (R2). Owner: GPT Astra.** Use the DDS image the extension points to, with the
  core `source` as fallback.
  - **Tests:**
    - a DDS next to a PNG with different content renders the DDS;
    - a missing DDS falls back to the PNG.

**Exit gate:**
- Each task passes its harness and `run_regression.py`.
- E195 cockpit and exterior reviewed against the reference screenshots with each extension on and off.
- Cost measured on the three-viewport setup.
