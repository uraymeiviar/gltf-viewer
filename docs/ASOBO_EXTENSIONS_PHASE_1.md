# Phase A1 — Basic Materials and Draw Order

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned. A1.1 and A1.2 done (`00274ce`, `b61072c`); A1.4 done (uncommitted).

**Entry:** A0.1, A0.4, A0.5, and A0.6 notes for each task's extension. **Goal:** occlusion/roughness/metallic,
emissive and normals close to MSFS (user priority 1), and the explicit draw order artists export (priority 2).

## Tasks

- [x] **A1.1 — `ASOBO_material_invisible` (R1).** Materials that declare it with `enabled` true (the default) are
  not rendered unless the viewport's *Show Invisible Materials* is on.
  - Verified: byte-identical to `23f61c6` with the option on, and with `"enabled": false`.
  - Harness: `scripts/validate_p8_followup.py`.
  - Moves onto the A0.1 model.
- [x] **A1.2 — `ASOBO_normal_map_convention` (R2).** An asset-level `DirectX` convention inverts the green
  channel of every normal map before use, including normal maps added by later phases (detail, clear coat,
  windshield).
  - **Mechanism:** `sampleTangentSpaceNormal()` (`functions.glsl`) flips `texel.g` before decoding; the core,
    clear-coat and gizmo samples go through it. `u_AsoboNormalMapDirectX` is part of `EnvironmentBlock`, uploaded
    per viewport by `CommandGenerator` from `GltfRenderLayer`'s parsed asset extension combined with the
    viewport's `AsoboOptions`. The switch is already in the redraw key; the value is a frame-block upload, so
    the command and uniform caches stay valid.
  - **Harness:** `scripts/validate_asobo_normal.py` (quick tier). DirectX + M matches pre-inverted M' within 1
    LSB (max per-channel difference 1: the sampled UNORM value and `1.0 - g` round differently); with the switch
    off, and for `OpenGL`, a missing extension or an undeclared extension, the captures are byte-identical.
  - **Results:** `scripts/run_regression.py` PASS. E195 cockpit `SCREEN_EICAS` on/off reviewed: the panel bevels
    and knob rims read raised with the flip and inverted without it (subtle at the default exposure).
  - Every one of the 166 project files that declares it uses DirectX.
- [ ] **A1.3 — ORM fidelity check (R1). Owner: Claude.** MSFS's `_COMP` texture uses the same packing as glTF's
  occlusion/roughness/metallic texture (R occlusion, G roughness, B metallic). Confirm Glider reads it the same
  way, including:
  - the occlusion texture and its strength;
  - roughness and metallic factors;
  - sRGB versus linear decoding.

  Compare against the reference screenshots. Expected outcome: no code change, or a documented difference.
- [x] **A1.4 — `ASOBO_material_draw_order` (R2). Owner: Claude.**
  - **Rule (verified, SDK):** transparent objects sort back to front by the **mesh bounding-box centre**, not the
    pivot (MSFS SDK `flightsim-materials.md`, "Draw Order", line 371). Decals: the offset strictly specifies their
    order, higher drawn above/later, equal offsets keep back-to-front. Glass and windshield: `key = distance +
    offset`, drawn larger key first. Other material types ignore the value.
  - **Types:** decal = `ASOBO_material_geometry_decal` or `ASOBO_material_blend_gbuffer`; glass =
    `ASOBO_material_glass_v2` or `ASOBO_material_glass`; windshield = `ASOBO_material_windshield_v3` or
    `ASOBO_material_windshield`, all read from `renderer::Material::asobo`.
  - **Mechanism:** the offset is read once per material sync into `Material`; `createScene` sets the world
    bounding-box centre on `Mesh` and the per-viewport active flag when the asset declares the extension and
    `asoboOptions.applies(...)` is true. `CommandGenerator` copies class/offset into each `SortedCommand` and adds
    the glass offset to its key; `TransparentPass` draws decals first (offset ascending, then back to front) and
    every other blended draw by key. Covered transparent draws stop writing depth so their explicit order decides
    what is on top while they are forward blended; A2's deferred path replaces this. Files that do not declare the
    extension keep the previous pivot-distance order and depth writes byte for byte.
  - **Glass sign (inferred from the literal rule):** the SDK does not state which direction the offset moves a
    glass draw. The implementation uses the literal `key = distance + offset` with the existing larger-key-first
    transparent sort, so a positive glass offset acts like extra distance: it is drawn earlier, i.e. below other
    glass. Decals are the opposite convention: a higher offset is drawn later, above.
  - **Harness:** `scripts/validate_asobo_draworder.py` (quick tier, `run_regression.py`): a higher decal offset
    wins even when its quad is farther; equal offsets sort by the bounding-box centre (not the pivot); decals draw
    before glass; large/small glass offsets and windshield; an offset on a plain blended material is ignored with a
    byte-identical capture; switch off and an undeclared extension are byte-identical to the file without it.
  - **Results:** all harness checks PASS. `run_regression.py --extra=--asobo --extra=off` PASS. With ASOBO on, only
    the E195 cockpit cases differ: p8_1 (`e195` in all four AA modes), p8_3 (`e195`) and p8_5 (`e195`). E195
    `SCREEN_EICAS` on/off: the hazard-stripe decal placards around the pedestal knobs are fully visible with the
    switch on (mostly hidden without it), and a thin left screen-bezel line becomes visible; the EICAS glass itself
    looks the same in this dark frame.
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
