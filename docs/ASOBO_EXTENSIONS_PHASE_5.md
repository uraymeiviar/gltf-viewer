# Phase A5 — Detail Map, UV Options, Parallax Window

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** A5.1 done (2026-10-27); A5.2 done (2026-10-28); A5.3–A5.4 planned.

**Entry:** A2.1, the geometry pass. Material additions land in the shared material code, so they work in both
the deferred and forward paths. **Goal:** user priority 5.

## Tasks

- [x] **A5.1 — `ASOBO_material_detail_map` (R3). Owner: DeepSeek (started 2026-10-27), reviewed by Claude.**
  - **What it does:** detail colour, normal and metal-rough-AO textures, tiled by `UVScale` over TEXCOORD_0 and
    blended into the base material, either by vertex alpha (detail mode) or by `blendMaskTexture` with
    `blendThreshold` (blend mode). The operators are in
    [semantics §14](ASOBO_EXTENSIONS_SEMANTICS.md#14-asobo_material_detail_map) (exporter preview graph, A0.6).
  - **Reference assets (2026-10-27).** Both are MSFS 2020 packaged cockpits (`ASOBO_asset_optimized` 4.x, DDS,
    `ASOBO_normal_map_convention` DirectX, babylon.js 3ds Max exporter):

    | Asset | Materials with detail_map | Slots used | Notes |
    | --- | --- | --- | --- |
    | `F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf` | 16 of 205 (33 primitives) | normal only on most (`UVScale` 40–100, one `scale` 0.22); colour + MRAO + normal on `Seats FABRIC` (40) and `FrictionTape` (20, MRAO + normal) | the main test asset |
    | `F:\msfs-addons\pmdg-aircraft-739\SimObjects\Airplanes\PMDG 737-900\model.BW\PMDG_737-900NG_VC.gltf` | 1 of 253 | `windshield_int`: colour + normal (`scale` 0.5), `UVScale` 7.5, `blendThreshold` 0.1 | windshield, `extras.ASOBO_material_code: "Windshield"` |

    The 777 also has a `windshield_int` (`UVScale` 5, `blendThreshold` 0.1). No reference uses `UVOffset` or
    `blendMaskTexture`, so blend mode is covered by fixtures only. The E195 cockpit (MSFS 2024) has one
    detail-map material, the windshield.
  - **Decisions for the implementation:**
    - Detail mode (no `blendMaskTexture`), with `vA` = vertex alpha (1 without `COLOR_0`):
      colour `mix(B, smoothstep(clamp(sqrt(B * D), 0, 1)), aD * vA)`; ORM
      `clamp(ORM + (2 * dORM - 1) * vA, 0, 1)` (offsets added unscaled to the factor-scaled base, as the
      exporter graph does; corrected in review); alpha also
      multiplies `aD`. These are the literal exporter formulas (open question 3 in §14); keep them in one
      function so a later correction is one edit.
    - Detail normal: blend with the base tangent-space normal by a standard normalised operator (whiteout:
      `normalize(vec3(nb.xy + nd.xy, nb.z * nd.z))`, detail strength = `detailNormalTexture.scale` when present,
      weighted by `vA`). The preview graph's unnormalised sum is not a usable shading normal. Detail normals are
      decoded exactly like the base normal map (A1.2 DirectX convention, A1.12 packaged decode with the same
      gating as the base normal, signed BC5 DDS since `8e932ca`).
    - Blend mode: `coverage = saturate((vA - (m - t)) / (2t))` with `m = blendMaskTexture.r` on the main UV, then
      `mix(detail, base, coverage)` per channel (detail first, as in §14).
    - `UVOffset` adds to the detail UV after `UVScale`. The undocumented object `scale` attribute (§14 open
      question 1) is ignored. When a material also has `ASOBO_material_windshield_v3`, `UVScale` is applied once.
    - Each texture is optional; with no detail texture the extension is inert. A display switch in File > Settings > glTF Extensions (app setting, all viewports)
      like the other extensions; support-table entry `Rendered`/`Partial` with the limits stated.
  - **Tests:**
    - a neutral detail (colour where `smoothstep(sqrt(B*D))` = B is not neutral, so use `aD = 0`; flat normal;
      MRAO 0.5) leaves the material byte-identical;
    - mask at 0 and at 1, and `blendThreshold` edge width;
    - tiling (`UVScale`) and `UVOffset`;
    - detail normal strength and the DirectX flip;
    - 777 seat fabric and walls before/after crops; files without the extension byte-identical.
  - **Result (2026-10-27).** `resources/shaders/asobo_detail_map.glsl` implements the operators in one place:
    `getAsoboDetailUV` (`v_texcoord_0 * UVScale + UVOffset`), `getAsoboDetailWeight` (the shared mask: vA in
    detail mode, `1 - coverage` in blend mode), `applyAsoboDetailColor`, `applyAsoboDetailOrm`,
    `applyAsoboDetailOcclusion` and `applyAsoboDetailNormal`. The material builds
    `ASOBO_MATERIAL_DETAIL_MAP` (+ `ASOBO_DETAIL_BLEND_MASK`) and four samplers only when a detail layer exists,
    and the viewport switch is the frame-block `u_AsoboDetailMapEnabled`, so materials of other files compile
    unchanged. `material_info.glsl` calls the operators from `getNormalInfo`/`getBaseColor`/
    `getMetallicRoughnessInfo`; `pbr.frag`, `decal_accum.frag` and `decal_prepass.frag` include the file, so the
    detail layer also reaches the decal accumulation path (fixture check). Support table: `Rendered`.
    `scripts/validate_asobo_detail_map.py` passes 24 checks (neutral, colour, ORM/AO values, UVScale/UVOffset,
    normal scale/flip/neutral, both mask modes and the threshold edge, detail alpha, inert extension,
    switch-off byte identity, windshield `UVScale` once, decal accumulation).
  - **Review (2026-10-27).** Crops in `build/review/detailmap/`: `777_seat_*`, `777_wall_*`, `777_tape_*`,
    `777_ws737_*`, `777_ws777_*`, `e195_ws_*` (before `8e932ca` / after / extension switched off, plus
    `*_detail_zoom.png` and `*_comparison.png`). The seat fabric, wall detail normal and friction-tape layers are
    clearly visible; the PMDG windshields are 1 %-opaque legacy glass (737: a ≤1 LSB tint over the glass, 777: a
    glare-shield/lower-pane band), and the E195 windshield (windshield_v3 + detail map) gains a visible
    rain/stipple layer.
    HelloCube and `E195-Exterior-LOD00` stay byte-identical to `8e932ca`; the E195 cockpit (which does declare the
    extension on its windshield) is byte-identical to `8e932ca` with the switch off and differs only by the detail
    layer with it on. Frame cost on the 777 seat view (TAA, 43 draws, median of 3): 1.2572 ms with the layer vs
    1.2495 ms switched off — within run-to-run noise.
  - **Deviations / limits.** `UVOffset` is a viewer-side key (no schema or exporter 8.4.1 property); it accepts a
    number for both axes or `[u, v]`. Detail textures always use TEXCOORD_0 and ignore their `texCoord` and
    `KHR_texture_transform`. The detail AO offset requires a core `occlusionTexture` (the exporter's AO path is the
    material occlusion). Detail-mode normals use the normalised whiteout, blend mode lerps as the exporter graph
    does; `detailNormalTexture.scale` is honoured although exporter 8.4.1 drops it. The object attribute `scale`
    is ignored. A4's windshield paths do not consume their own `UVScale`, so the detail `UVScale` is applied once.
    ORM (corrected in review): the exporter graph scales the base OMR sample by the factors and adds the detail
    offset unscaled (semantics §14 note), so `applyAsoboDetailOrm` adds the offsets unscaled to the factor-scaled
    base. The plan's offset scaling only differed for `carpetVC` (777) and the E195 `windshield`.
    **Windshields excluded (corrected in review, 2026-10-27):** on windshield v3, windshield v1 and material code
    `Windshield` the same slots are the windshield layers, not a detail map (MSFS SDK "FlightSim Material
    Textures"): detail colour = masks (R details 1, G icing, B details 2), detail OMR = detail reflection, detail
    normal = icing normal. Rendering them as a detail map drew the frost/icing pattern on clear glass, so
    `GltfRenderLayer` never compiles the A5.1 variant for windshield materials; their layers belong to A4.
    The E195 windshield view is byte-identical to `8e932ca` again; the fixture checks all three windshield kinds.
- [x] **A5.2 — `ASOBO_material_UV_options` (R2), 2026-10-28.** Material variant plus a
  switch in File > Settings > glTF Extensions (app setting, all viewports); existing Inspector edits refresh the variant. No new flag. Empty/default parameters keep the old variant.
  - **Research / inferred formula:** local 2020 Materials / UV Options and official 2024 FlightSim Materials /
    UV Options define the controls but omit pivot/order/sign. Use
    `uvMain = R(radians(rotation)) * ((uv0 - 0.5) * tiling) + 0.5 + offset`.
    Semantics §15 records sources, directions, Blender discrepancies and all six question resolutions/limits.
  - **Scope:** core base colour, COMP/metallic-roughness, normal, emissive, UV0 occlusion, generic detail
    layers (transformed UV × UVScale) and blend mask (transformed main UV). KHR follows ASOBO; clamp follows
    KHR. UV1 (including KHR overrides), UV2 extra occlusion and windshield-only layers are untouched.
    False/absent clamp leaves sampler wrapping alone (inferred from E195/DA62/PMDG and exporter defaults);
    true clamps each axis to edge texel centres. `clampUVZ` is ignored and remains visible/editable.
    Both offset spellings work; exporter spelling wins and conflicts log once per process.
  - **Checks:** Release build, `validate_asobo_uv_options.py`, `validate_asobo_support.py`,
    `validate_asobo_detail_map.py`, and `validate_asobo_common.py --baseline-exe build/baselines/d0761e7.exe`
    pass. CPU-baked references check tiling, U/V offset, rotation, order and aliases; independent sampler
    references check each clamp axis. Checks cover core slots, UV1, KHR/detail/blend-mask composition,
    windshield exclusion, Inspector edit refresh and both switches. Empty/defaults and HelloCube/non-ASOBO
    captures are byte-identical (the latter against d0761e7). Zero GL errors in the new capture suites.
    The new check is in the serial ASOBO list; the regression runner was not run or its thresholds changed.
  - **Review:** `scripts/review_asobo_uv_options.py` creates baseline/current captures and a contact sheet
    under `build/review/uv_options/`, using read-only source assets and absolute-URI copies under build.
    The contact sheet uses display gain 8× on the dark LCD crop only; raw PNGs are unmodified.
    E195 LCD micro-close-up shows the fine RGB lattice after 125× tiling (before: broad subpixel edges).
    Dust redistributes its mottled pattern at 1.5×. Tyre V offset 1 is visually unchanged under repeat;
    PMDG fuselage is visually stable with only small edge differences. The source SDK tyre has a
    pre-existing skin deformation in both binaries; an additional `da62_tyre_static` build-only copy
    removes that node’s skin binding to review its real rest-mesh texture. Raw source captures are kept.
    All texture URIs resolved; all ten captures report zero GL errors. Changed pixel counts (of 822,528):
    e195_lcd 205289, da62_tyre 39, da62_tyre_static 100, da62_dust 250267, pmdg_fuselage 777.
  - **Deviations / limits:** the surveyed E195 `asset` material is absent in current model files;
    `LCD_DETAIL` is present at 125 × 125. The SDK Nose attachment has a Dust decal shell without its
    receiver, so the Dust review disables geometry-decal processing identically in both binaries.
    The base-level clamp inset approximates per-mip edge clamp with minified linear filtering; nearest
    edge stretching is exact. Rotation/order/sign are inferred, not simulator parity. No benchmark or
    performance improvement is claimed. No simulator config files were read and no SDK/add-on files written.
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
