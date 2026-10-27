# Phase A2 — Decal Buffer and Decals

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Surface-data study](ASOBO_GBUFFER_STUDY.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Design approved 2026-10-25; A2.1 (buffer and prepass) and A2.2 (decal pass and receiver application) done, A2.3 next.

**Entry:** A1.2, A1.4, the A0.6 notes and the [surface-data study](ASOBO_GBUFFER_STUDY.md). **Goal:** decals
composite the way MSFS composites them. `ASOBO_material_geometry_decal` is used by 144 of 207 project files: 7 cockpit materials and 4 exterior materials.

## 1. What has to be reproduced

From the SDK (verified; study §1 and §4):
- A decal changes six properties of the surface below it, each by its own factor `f`, before lighting:
  - base colour, metallic, roughness, normal and emissive: `p = lerp(p_below, p_decal, f)`;
  - occlusion: `O = lerp(O_below, O_decal * O_below, f) = O_below * lerp(1, O_decal, f)`.
- Decals are drawn after the opaque pass and before transparents, in draw order (A1.4): a higher
  `drawOrderOffset` lands on top. Successive decals do not commute.
- Decals land only on five receiver types: Standard, ClearCoat, Anisotropic, FakeTerrain and SSS, and only on
  OPAQUE surfaces (MSFS draws MASK and BLEND in its transparent pass). Glass and windshields never receive decals.
- `normalOverrideFactor` picks the base normal of the decal: the receiver's mesh normal at 0, the decal mesh normal
  at 1. It is separate from `normalBlendFactor`.
- `underClearcoat` (exporter default true): the decal changes the base layer under the coat.

## 2. Design: decal buffer, forward lighting

The earlier plan was a full deferred opaque path (G-buffer + deferred lighting pass). The study changes that:
since decals only modify surface properties *before* lighting, and only on opaque receivers, Glider can collect
the decals in screen space first and let the existing forward shader apply them while it shades. Nothing is lit
twice, and no lighting code is duplicated.

Per viewport, only when that viewport has decals to draw (the file has decal materials, ASOBO is on and the
`ASOBO_material_geometry_decal` switch is on):

1. **Prepass.** Draw all opaque and MASK geometry, except decals, into a decal-buffer depth target plus one colour
   target holding the world-space geometric normal (`RGB10_A2`, encoded `n*0.5+0.5`). MASK materials alpha-test.
   Use the same camera matrices as the forward pass (TAA jitter included). Model it on `PickingRenderLayer`
   (own shader, own pass, shares the traversal and draw lists).
2. **Decal pass.** Draw the decal draws in A1.4 order, depth test `LEQUAL` against the prepass depth, no depth
   writes. The decal shader evaluates the decal material (base colour, alpha, ORM, normal, emissive) and its
   weight per property, `w_p = f_p * coverage` (coverage = base-colour alpha × mode mask; open question 1), and
   accumulates into the accumulation targets below with hardware blending. It may sample the prepass normal (not
   attached in this pass) for `normalOverrideFactor`.
3. **Forward opaque pass**, as today. Receiver shaders read the accumulation targets at `gl_FragCoord` and apply
   them to their own properties before lighting: `p = V_p + T_p * p_receiver`, and AO multiplied by the decal AO.
   Non-receivers ignore the buffer.
4. **Transparent pass**, as today. Decal draws are no longer in it for that viewport.

### Accumulation (order-correct with hardware blending)

For a chain of decals with weights `w_i`, `lerp` composes as `p = V + T * p_below` with `V' = d·w + V·(1−w)` and
`T' = T·(1−w)`. Each target stores `V` in colour and `T` in its last channel; the shader writes `(d·w, …, w)` and
the blend is `glBlendFunci(i, GL_ONE, GL_ONE_MINUS_SRC_ALPHA)` (colour) with `T` carried by the same rule (the
shader writes 0 into the `T` channel). Cleared to `V = 0`, `T = 1`. Occlusion uses a multiply blend
(`GL_DST_COLOR, GL_ZERO`), cleared to 1, with the shader writing `lerp(1, O_decal, w_O)`. Every property has its
own weight, so each blended group gets its own target (OpenGL 4.1 per-buffer blending, at most 8 targets).

| Target | Format | Contents | Clear |
|---|---|---|---|
| depth | `DEPTH_COMPONENT32F` | prepass depth | 1 |
| G0 | `RGB10_A2` | receiver geometric normal (prepass only) | — |
| A0 | `RGBA8` | base colour `V.rgb`, `T` | (0,0,0,1) |
| A1 | `RGBA16F` | normal `V.xyz` (world), `T` | (0,0,0,1) |
| A2 | `RGBA16F` | emissive `V.rgb` (HDR), `T` | (0,0,0,1) |
| A3 | `RG8` | metallic `V`, `T` | (0,1) |
| A4 | `RG8` | roughness `V`, `T` | (0,1) |
| A5 | `R8` | occlusion product | 1 |

About 33 bytes per pixel, about 68 MB at 1920×1080 (render-scale resolution). The viewports render one after
another, so **one** set is shared, sized to the largest viewport that needs it, created on first use and released
when the model is unloaded or no viewport uses decals. Formats are an implementation choice: start here, and raise
A0 to `RGBA16F` or A3–A5 to 16-bit only if the tests show banding. Not a user setting.

### Receiver side (`pbr.frag`)

- Variant define `ASOBO_DECAL_RECEIVER`, compiled only for OPAQUE receiver materials of files that contain decal
  materials, so files without decals compile exactly the shaders they do today.
- A per-viewport uniform in `EnvironmentBlock` (like A1.2's `u_AsoboNormalMapDirectX`) turns the read on, so the
  command cache is not split per viewport. With it off, or where `T == 1` for a property, the receiver keeps its
  own value unchanged (an explicit branch, not `normalize(0 + 1·n)`), so pixels without decals stay
  byte-identical.
- Injection points: base colour before `MaterialInfo.baseColor` is set; metallic and roughness before `c_diff`/`f0`
  are derived (inside or right after `getMetallicRoughnessInfo`); normal before the `NdotV` terms; emissive where
  `f_emissive` is formed; decal AO multiplies diffuse, specular, sheen and clear-coat IBL terms with strength 1,
  also when the receiver has no occlusion map.
- Clear coat, sheen, anisotropy, SSS and every KHR feature keep working, because the forward shader still does all
  the lighting. `underClearcoat = true` therefore works by construction. `underClearcoat = false` (over the coat) is
  open question 3; until answered it behaves like `true`, and the support table says so.
- Receiver classes: opaque materials with no ASOBO material type, `clear_coat`/`clear_coat_v2`, `anisotropic`/v2,
  `fake_terrain`, `SSS`. Everything else ignores the buffer. FakeTerrain is shaded as standard PBR (no terrain
  renderer).

### Decal modes and related extensions

- `mode = default`: the rules above.
- `mode = blendMasked`: coverage × the mask ramp (`blendSharpnessFactor`). The SDK and the Blender preview disagree on
  its direction (study §4.7, open question 2): follow the SDK (smaller = sharper) and keep it one function.
- `mode = frosted`: coverage × vertex alpha (frost coverage). The frost look itself (textures, melt) belongs with
  icing in A6; A2 only blends.
- `ASOBO_material_blend_gbuffer`: the exporter imports it as a default-mode decal; same path, same switch logic.
- `decalChan0–2` (scenery, terrain, SimObject targets): Glider shows a SimObject, so only the SimObject channel
  applies; record the others as not applicable.

### What stays as it is

- Files without decal materials, ASOBO off, or the decal switch off: today's path (A1.4 draws decals as blended
  forward draws). This keeps the regression baselines.
- Picking (own pass), the transmission copy, transparents, glass and windshields.
- **MSAA:** the decal buffer is always single-sample at render resolution; the MSAA forward pass reads it at the
  pixel centre. Decals sit inside their receivers, so the only error is at silhouettes crossing a decal edge.
  Measured, not assumed (A2.5).
- TAA and FXAA: no change (the prepass uses the jittered matrices of the frame).
- Debug views (base colour, normal, metallic, …) show the properties after decals, which makes them the first test
  tool.

### Why not a full deferred path

- Deferred lighting would have to re-implement every forward lighting branch (KHR clear coat, sheen, iridescence,
  ASOBO anisotropy, SSS, A3 shadows) for decal pixels, or lose them.
- MSFS's own G-buffer layout is not documented (study §3), so there is nothing to match exactly anyway.
- Cost here: one depth+normal prepass and a small decal pass, only in viewports that show decals.
- The journal's argument that shadows are cheaper deferred (A3) no longer applies: A3 adds the shadow lookup to the
  forward shader, where every material benefits.
- If a later phase needs screen-space data (windshield SSR in A4, velocity for TAA), the prepass is the place to add
  it.

## 3. Tasks

Run one at a time; each lands, is reviewed and committed before the next. Owners per the worker roster
(DeepSeek for build-heavy work, GPT Sol/Astra for bounded work, Gemini Flash for documentation).

- [x] **A2.1 — Decal buffer and prepass. Done 2026-10-25. Owner: DeepSeek.**
  - Shared targets as in the table, lazy creation, resize, release; memory reported in the Statistics panel.
  - Prepass layer (model: `PickingRenderLayer`), opaque + MASK, decals excluded, jittered camera.
  - Wired only for viewports with active decals; nothing else changes yet.
  - **Tests:** prepass depth equals the forward depth on an opaque fixture (read back, exact); G0 normals within
    1/512 of the forward geometric-normal debug view; viewports resized, closed and reopened; memory counted once
    for three viewports; full regression byte-identical (no viewport uses decals in the suites).
  - **Results:** shared buffer is 33 B/px: 47,520,000 B (45.32 MiB) at 1600x900 and 68,428,800 B (65.26 MiB) at
    1920x1080. Synthetic fixture: 33,752 receiver pixels, prepass depth max |diff| 0 (all exact), G0 encoded max
    diff 0 (byte-identical to the forward Geometry Normal view), MASK plane included, two instances of one mesh
    in the instancing path, decal material excluded. Resize/open/close of three viewports: 0 GL errors, one
    shared 579x796 buffer (15,209,172 B), released when the last viewport closes. E195 cockpit and HelloCube
    dumps are byte-identical to `build/baselines/8d58094.exe`. E195 `DecalPrepass` GPU time: median 0.085 ms
    (min 0.084, max 0.566) over 53 samples at 734x696 render resolution. New diagnostics:
    `--decal-prepass-check` and the "Decal prepass depth"/"Decal prepass normal" Render Modes.
- [x] **A2.2 — Decal pass and receiver application. Done 2026-10-25. Owner: DeepSeek.**
  - Decal shader variant, blend states, A1.4 ordering, decal draws removed from that viewport's transparent list.
  - `pbr.frag` receiver variant and the per-viewport uniform; receiver classes.
  - **Tests (synthetic fixtures, `scripts/validate_asobo_decal.py`):**
    - factor 0 for a property leaves it unchanged (byte-identical to no decal);
    - factor 1 replaces only that property (debug views per property);
    - occlusion multiplies; two overlapping decals with weights 0.5 follow draw order (0.25 vs 0.5 example of
      study §4.4);
    - non-receivers (MASK, glass, a material with an unsupported type) are untouched;
    - decal switch off renders byte-identical to the A1.4 baseline;
    - pixels outside decal coverage are byte-identical between switch on and off.
  - **Results:** new `DecalAccumulationLayer` + `DecalAccumulationPass` run between `DecalPrepass` and `Opaque`;
    decals use `decal_accum.frag` (same `material_info.glsl` sampling, per-property `w = factor × base-colour
    alpha`) with per-target `glBlendFunci`/`glBlendFuncSeparatei` over blending (A0-A4, `T' = T·(1-w)`) and
    multiply (A5). `pbr.frag` gains `ASOBO_DECAL_RECEIVER` for OPAQUE standard/clear-coat/anisotropic/fake-terrain/
    SSS materials of files with decals; a per-viewport `u_AsoboDecalReceiver` guards A0-A5 reads at `gl_FragCoord`.
    Decals also get a `(-1, -1)` polygon offset in the A1.4 forward fallback. Synthetic fixture: all six factor-0
    views byte-identical to no decal; every factor-1 property changes only its own view; metallic 1.0, roughness
    0.1, normal (0.6,0,0.8), emissive (0.8,0.4,0.1) and occlusion 128×128 land exactly (occlusion `0.25`, no-map
    decal AO `0.5`); overlapping 0.5 decals give 0.25 (1 then 0) and 0.5 reversed; MASK and glass base-colour
    views are byte-identical with and without the decal; normalOverrideFactor 0 uses G0. E195 cockpit: the MCP
    "YD"/"APP" legends are clean in both paths (accumulation and A1.4 fallback); the screenshots are
    `build/review/a22_eicas_side_by_side.png` and `build/review/a22_mcp_side_by_side.png`. Accumulation respects
    the authored per-property factors, so A1.4 fallback and A2.2 differ where a decal declares only some properties
    (for example `LCD_DETAIL`, which leaves metallic/roughness/emissive to the receiver).
  - **A2.2 fixes (2026-10-25, decal depth / edit cost).**
    - **Measured depth gap.** Python over the E195 cockpit glTF, world space, every `DECAL_LIT`
      vertex (33,703) against the nearest OPAQUE/MASK non-decal receiver (BVH, ray along
      `±normal`, facing-filtered closest-point fallback): min `-0.2419 m`, median `+0.000498 m`,
      max `+0.1987 m`, mean `-0.00004 m`; 3,359 vertices (9.97 %) lie behind the receiver.
      The extremes are decal vertices whose normal ray leaves its panel and reaches unrelated
      geometry (compass / yoke); at the MCP the relevant text decals (`DECAL_AFCS_*`,
      `DECAL_SPEEDM_*`, `DECAL_VERTG_*`, DCP) sit 0.03–0.9 mm **behind** their button faces,
      while the button meshes (`B_GS_*_LIGHT`) sit +0.27 mm in front. The old LEQUAL test
      rejected the text decals, which is the partly missing "A/T / VNAV / APP / BANK".
    - **Tolerant buffer test.** The accumulation pass disables `GL_DEPTH_TEST`; `decal_accum.frag`
      samples the prepass depth at `gl_FragCoord`, linearizes both depths through the z row of
      the frame's projection matrix (perspective, infinite perspective and orthographic) and
      converts axial depth separation to ray distance for perspective, then projects it onto
      the receiver geometric normal from G0: `gapNormal = gapAlongRay × max(abs(dot(viewDir, n)), 0.05)`.
      Orthographic rays use the view axis. Discard when `gapNormal > t_abs + t_rel × distance`.
      Defaults: `t_abs = 0.0012 m` (1.2 mm), `t_rel = 0.0002` (0.02 %, depth precision margin).
      `t_abs` is per viewport in `EnvironmentBlock`, editable at 0–10 mm in Viewport Settings
      or through `--decal-tolerance <mm>`; command caches remain independent of it. The accumulation framebuffer no
      longer attaches the depth texture (it is sampled instead), and it gets no polygon offset:
      an offset would corrupt `gl_FragCoord.z` before the comparison.
    - **Forward fallback.** The A1.4 path keeps a polygon offset (`factor -1`), but its units are
      now a physical 2 mm bias converted per draw from the frame's near/far, projection and
      view distance (24-bit forward depth): ~30 units on the synthetic orthographic fixture and
      a few thousand at close cockpit distances. The old `(-1, -1)` is sub-micrometre at those
      distances. The bias is active when the viewport applies the decal extension or, in the
      fallback, when it applies `ASOBO_material_draw_order`; `--asobo off` keeps the old path.
    - **Tests:** `scripts/validate_asobo_decal.py` verifies 1 mm gaps visible and 2 mm gaps hidden
      at 0�, 45� and 70� in both perspective and orthographic projection; `--decal-tolerance 3`
      restores the 2 mm case at every angle. The 2 cm occluder and A1.4 fallback checks remain.
      E195 MCP and left audio panel review captures: `build/review/tol_*.png`.
    - **Material edit cost.** `updateGltf` re-uploaded every accessor (58.8 MB/frame on the E195
      cockpit) and `updateScene` rebuilt the meshes and re-ran MikkTSpace. MikkTSpace tangents
      are now cached per primitive and normal-map UV set for the loaded model (cleared on model
      load, regenerated only on a UV-set change), and a material-only `SceneChange::Material`
      sync calls `updateMaterialStates` (materials/uniforms/samplers) instead of
      updateGltf+updateScene. E195, emissive of `DECAL_LIT` changed every frame, 111 measured
      frames: CPU frame median 20.40 ms → 1.70 ms; dirty sync 17.68 ms → 0.64 ms; buffer uploads
      6945 / 58.82 MB per frame → 1 / 288 B (skin joints); textures 0 in both. The two final
      frames are byte-identical (`build/review/a22fix_edit_before.png` /
      `a22fix_edit_after.png`). Repro: `--material-edit-emissive DECAL_LIT --profile-report ...`,
      with `--force-full-sync` as the before reference.
- [ ] **A2.3 — Modes and legacy decals. Owner: GPT Sol.** `blendMasked` ramp, `frosted` coverage,
  `decalChan0–2`, support-table notes. `ASOBO_material_blend_gbuffer` and `normalOverrideFactor` already use the
  A2.2 default path; A2.3 verifies their equivalence.
  **Tests:** fixtures per mode; ramp endpoints; blend_gbuffer renders like the equivalent default decal.
  - **Samples (survey 2026-10-28):** the MSFS 2024 SDK DA62 exterior is the only source of both modes, in
    source and packaged form (`DA62\PackageSources\...\attachments\asobo\` and `DA62\Packages\...`):
    `Asset_Exterior_Nose_Default|Long`, `Part_Exterior_Airframe`, `Part_Exterior_Engine_L|R`, `Part_Exterior_Tail`,
    `Part_Exterior_Wing_L|R`. 47 `blendMasked` materials (e.g. `Insects`: `blendMaskTexture`, `blendSharpnessFactor`
    1.0, `normalOverrideFactor` 0, `underClearcoat` false) and 50 `frosted` (e.g. `Frost`: base colour/metallic/
    roughness factors 0.99, normal 0.81, emissive/occlusion 0), all `alphaMode: BLEND`, per tree. One `frosted` decal
    in the FSS E175 project. Shared textures: `DA62\Sources\Art\Max\textures` as a search path. Some attachments
    hold decals without their receiver surface (A5.2 review: the Nose file); review views need a file that holds
    both.
  - **Semantics:** §7 of the semantics doc (the `blendMasked` preview ramp is verified from the exporter graph;
    `frosted` has no exported frost keys; the per-channel blend operator is inferred). Brief:
    `C:\Users\uray\.claude\codex-runs\gltf-viewer\decal-modes.brief.md`.
- [x] **A2.4 — Environment occluder and fake terrain. Done 2026-10-25 (occluder hidden; fake terrain not yet recorded).** Was: Owner: GPT Astra (small).** Per study §4a, neither has
  anything to render in a viewer:
  - `ASOBO_material_environment_occluder` only hides MSFS weather and vegetation inside the cabin, which Glider does
    not draw. It is in every cabin LOD00 file (E170, E175, E190, E190F, E195, E195F), and today Glider draws it as a
    shell around the cabin. Treat it like `ASOBO_material_invisible`: hidden, shown with
    `--show-invisible-materials`; support *Rendered* ("hidden: nothing to occlude in a viewer").
  - `ASOBO_material_fake_terrain`: Glider has no terrain, and no project file uses it. Support *Not applicable*; a
    material that declares it is drawn as plain glTF (and still receives decals, as a standard material).
- [ ] **A2.5 — Review against the project files. Owner: Claude (review), DeepSeek (measurements).**
  - E195 and E190 cockpit and exterior decal sheets: compare with A1.4 and with the MSFS reference screenshots
    when they exist.
  - MSAA, TAA, FXAA and render scale with decals.
  - Frame cost and memory on the three-viewport setup, decals on vs off.

**Exit gate:**
- Files without decals, ASOBO off, or the decal switch off are byte-identical to the baseline.
- Pixels without decal coverage are byte-identical between decals on and off.
- The decal fixture tests pass.
- E195 reviewed (against the reference screenshots when available).
- Memory and frame cost recorded.

## 4. Open questions carried from the study

Numbers refer to [study §5](ASOBO_GBUFFER_STUDY.md). They do not block the design; each has a single place in code:

- **1 — exact weight:** `w_p = f_p × coverage` in the decal shader.
- **2 — blendMasked direction:** the ramp function.
- **3 — over-coat decals:** the receiver injection.
- **5 — ties and depth bias:** the A1.4 sort and the decal depth state.

The A0.7 test scene with MSFS screenshots answers them.
