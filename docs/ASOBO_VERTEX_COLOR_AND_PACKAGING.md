# ASOBO Vertex Colours And MSFS Packaging

Read-only investigation of (a) how the MSFS 2024 Blender exporter writes vertex
colours and what each material does with them, and (b) what the MSFS package tool
changes in a glTF. The investigation itself changed no source code; the runtime
rule derived from it is in §2.9.

**Marking.** **[V]** = verified in code or measured from the files named here.
**[I]** = inferred from those facts. Path prefixes used below:

- `EXP/` = `H:\git-repos\msfs-blender-5-addons\io_scene_gltf2_msfs_fss\`
- `BLENDER/` = `C:\Program Files\Blender Foundation\Blender 5.2\5.2\scripts\addons_core\io_scene_gltf2\`
- `SDK/` = `F:\new-fss-aircraft-ejet\doc\msfs-sdk\pages\`
- `PROJ/` = `F:\new-fss-aircraft-ejet\msfs24-projects\`

**Method.** **[V]** The measuring script is
`build/vertex_packaging/compare_packaging.py`; its raw output is
`build/vertex_packaging/measurements.json` and `measurements.txt`. It opens every
model file read-only and writes only to `build\vertex_packaging\`. The two pairs
are:

- `PROJ/fss-aircraft-e190/SimObjects/Airplanes/E190/model/E190-Exterior-LOD00.gltf`
  vs `PROJ/Packages/fss-aircraft-e190/SimObjects/Airplanes/E190/model/E190-Exterior-LOD00.gltf`
  (E190 exterior, 141 primitives).
- `PROJ/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf`
  vs `PROJ/Packages/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf`
  (E195 cockpit, 894 primitives).

Both source files name generator `Khronos glTF Blender I/O v5.2.40 and Asobo
Studio MSFS2024 Blender I/O v8.4.1 with Blender v5.2.2` **[V]**.

---

# Part 1 — Exporter vertex colours

## 1.1 What is written as `COLOR_0`

| Question | Answer | Basis |
|---|---|---|
| Blender data used | The active **render** colour attribute (`mesh.color_attributes.render_color_index`) when MSFS exports colours; default name `Color`; `COLOR_0` is the glTF name. | [V] `BLENDER/blender/exp/primitive_extract.py:531-535,598-603`; `EXP/io/exp/multi_export.py:279`; `EXP/blender/utils/msfs_constants.py:7-12` |
| Domain / type conversion | The active attribute is renamed `Color` and converted to `BYTE_COLOR` / `CORNER` if needed before the join/export step. | [V] `EXP/io/exp/multi_export.py:763-781` |
| glTF type | `VEC4`, component type `UNSIGNED_SHORT` (5123), `normalized: true`; MSFS forces alpha, so the `BYTE_COLOR`/`VEC4` branch is used. | [V] `BLENDER/blender/exp/primitive_extract.py:1317-1324`; `BLENDER/blender/com/conversion.py:88-102`; `BLENDER/blender/exp/primitive_attributes.py:183-211`; source accessors measured by the script |
| Linear or sRGB | The exported values are linear. `foreach_get("color")` supplies linear floats and the exporter explicitly says “colors are already linear, no need to switch color space”. The E195 windshield values corroborate this: G `0.07421988` and B `0.01937896` are the linearised sRGB bytes 77/255 and 38/255 (`0.07421357`, `0.01938236`). | [V] `BLENDER/blender/exp/primitive_extract.py:1288,1356-1360`; measured source values and sRGB arithmetic |
| White default | New/converted meshes with no colour attribute get a white `Color` `BYTE_COLOR`/`CORNER` layer (Blender 5.2 headless test confirms new `BYTE_COLOR` data is `(1,1,1,1)`). It exists so an empty Vertex Color node does not output black. | [V] `EXP/blender/utils/msfs_mesh_utils.py:22-34`; `EXP/blender/msfs_handlers.py:25-60`; `EXP/blender/material/msfs_material.py` uses an unset `layer_name` (`EXP/blender/utils/msfs_material_nodes_utils.py:327-350`) |
| Is the white layer kept? | No, not if it is uniformly white: `clean_color_attributes` removes `COLOR_0` when the active render colour is all 1.0, and removes every other `COLOR_n` in MSFS 2024 mode. | [V] `EXP/io/com/msfs_gltf_mesh_utils.py:11-28`; hook `EXP/io/exp/gltf_hooks.py:290-293` |
| Multiple colour attributes | Only `COLOR_0` survives; `COLOR_1..n` are popped. | [V] `EXP/io/com/msfs_gltf_mesh_utils.py:21-28` |
| Blender fake-white path | The built-in exporter can synthesise a white `UNSIGNED_BYTE` `COLOR_0` when `export_all_vertex_colors` is on, but MSFS sets it false. The package-tool white (Part 2) is a different path. | [V] `BLENDER/blender/exp/primitive_extract.py:817-850,1216-1231,1328-1350`; `EXP/io/exp/multi_export.py:348` |

**[V]** MSFS 2020 target scenes skip the default-colour handler
(`EXP/blender/msfs_handlers.py:39-43`), so the white default is a MSFS 2024
authoring behaviour.

Consequences in the measured source files: the E190 exterior has `COLOR_0` on
only **2 of 141** primitives; the E195 cockpit on **7 of 894**. The exporter
therefore does not write white to every primitive **[V]** (script layout counts).
Those two/seven carry non-white data, so the white-default removal did not apply
to them.

## 1.2 Per-material channel meaning

**[I]** The material modules build a Blender shader preview (the exported glTF
does not contain these nodes). All modules except `Invisible` call
`default_shader_tree()` — for example glass
`EXP/blender/material/msfs_material_glass.py:82-83`, clearcoat `:156-157`,
porthole `:76-77`, anisotropic `:90-91`, hair `:78-79`, SSS `:83-84`, sail
`:79-80`, tree `:81-82`, vegetation `:81-82`, tire `:85-86`, fake terrain
`:76-77`, ghost `:84-85`, fresnel fade `:75-76`, environment occluder `:34-35`,
parallax `:91-92`, standard `EXP/blender/material/msfs_material_standard.py:91-92`
**[V]**. That shared tree creates Base Color, Alpha and Blend Mask groups
(`EXP/blender/material/msfs_material.py:681-747`). In those groups:

- VC **RGB** is a plain multiplier on the base/detail colour
  (`msfs_material.py:1392-1398,1491-1492,1532-1533,1540-1542`) **[V]**.
- VC **alpha** multiplies alpha and is also the detail/blend selector
  (`msfs_material.py:1221-1222,1283-1287,1589-1594,1662-1669`) **[V]**.

This is a Blender preview and a way for the exporter to detect VC usage; the
glTF contains only `COLOR_0` plus the material extension, not these node values
**[V]**. Runtime channel meaning therefore comes from the MSFS shader /
extension, not from the node graph **[I]**.

| Material / extension | Blender node-graph use | Documented / runtime meaning | Basis |
|---|---|---|---|
| Standard (`msfs_standard`) | RGB × base colour; A × alpha; A drives detail mask | A: detail-map impact, 0 = none, 1 = fully active (and with a blend mask, 0 = blend map, 1 = first map). RGB: plain tint in the node graph, not documented by the SDK. | [V] node links; SDK `SDK/models-and-textures/textures/materials/flightsim-material-textures.md:46,55,64,73`, `SDK/models-and-textures/modeling/aircraft/cockpit/textures/decals-and-detail-maps.md:53-58`. RGB runtime tint is **[I]**. |
| Detail map (`ASOBO_material_detail_map`) | Shared blend-mask group: A selects detail vs blend; detail mask = A × base alpha | A: detail blend/impact; with `blendMaskTexture`, A = 0 shows the blend map, A = 1 the first map, threshold controls sharpness. | [V] `EXP/blender/material/msfs_material.py:1187-1287,1360-1542`; SDK `flightsim-material-textures.md:46,55,64,73` |
| Windshield v3 (`ASOBO_material_windshield_v3`; material type `msfs_windshield`) | Generic RGB/A only; no windshield-specific VC nodes. The module itself carries `# TODO - Add new windshield shader`. | **R = scratches mask, B = fingerprints mask, A = icing mask; G = mask for the Secondary Details texture alpha.** The G *texture* channel is the icing mask, not vertex G. | [V] `EXP/blender/material/msfs_material_windshield.py:108-112`; SDK `flightsim-material-textures.md:170,176`, `SDK/models-and-textures/modeling/aircraft/airframe/windshield-and-windows.md:47-54,62-68` |
| Older windshield (`ASOBO_material_windshield`) | Not emitted by the 2024 exporter; appears only in the `.fs20-backup`. The MSFS 2020 graph has a `toggleVertexBlendMapMask` that uses VC alpha as the blend-map selector. | No 2024 SDK per-channel table for the old extension was found; likely the same alpha-driven blend/icing role, but not independently verified. | [V] `PROJ/fss-aircraft-e195/SimObjects/Airplanes/E195/model/E195-Cockpit-LOD00.gltf.fs20-backup` material 26; `EXP/_msfs2020/blender/msfs_material_function.py:1069-1080`; **[I]** for runtime equivalence |
| Glass (`msfs_glass`) | Generic RGB/A | SDK lists glass under the standard texture set; no dedicated VC channel documented. Generic tint/alpha is therefore the only defensible reading. | [V] `msfs_material_glass.py:82-83`; SDK `flightsim-material-textures.md:13`; **[I]** runtime tint/alpha |
| Geometry decal (`msfs_geo_decal`) | A × decal alpha | The node graph makes VC alpha an extra opacity factor. The SDK documents decal opacity from the albedo texture alpha, not from VC. | [V] `EXP/blender/material/msfs_material_geo_decal.py:161-180`; SDK `decals-and-detail-maps.md:18-19`; runtime A modulation is **[I]** |
| GeoDecal BlendMasked | A × (1 − threshold) feeds the blend mask | A selects the blend map; 1 = first map, 0 = blend map. | [V] `EXP/blender/material/msfs_material_geo_decal_blendmasked.py:330-344`; SDK `flightsim-material-textures.md:363` |
| GeoDecal Frosted (airframe ice/frost) | A × alpha, then inverted; A controls frost coverage | Paint vertex alpha; more alpha = more icing/frost. | [V] `EXP/blender/material/msfs_material_geo_decal_frosted.py:218-235,434-436`; SDK `SDK/models-and-textures/modeling/aircraft/airframe/frost-and-ice.md:33`, `windshield-and-windows.md:62-68` |
| Soft body (`ASOBO_softbody_mesh`) | No VC nodes. The object boolean becomes a mesh extension `{"type":"balloon"}`. | SDK describes a deformable balloon canopy only; it documents no vertex-colour weights. | [V] `EXP/io/com/extensions/object/asobo_softbody_mesh.py:21,41-58`; `EXP/blender/msfs_properties.py:28`; SDK `SDK/models-and-textures/plugins/blender-plugin/blender-plugin-properties.md:77`, `SDK/models-and-textures/plugins/gltf-schemas.md:542-550`. **No soft-body weight channel found [I]**. |
| Invisible (`msfs_invisible`) | No default shader tree at all. | No VC meaning. | [V] `EXP/blender/material/msfs_material_invisible.py:57-59` |
| All other types (clearcoat, porthole, parallax, anisotropic, hair, SSS, sail, propeller, tree, vegetation, tire, fake terrain, ghost, fresnel fade, environment occluder) | `default_shader_tree()` only. | No dedicated VC channel in code or SDK; only the generic RGB multiplier / A alpha-detail behaviour. | [V] module calls above; **[I]** for runtime generality |

**Windshield correction.** The brief's “R scratches, G icing, B fingerprints” is
not what the 2024 SDK says. R and B are the scratch/fingerprint masks, the icing
mask is the **G channel of the Details 1/2 texture**, and it is masked by
**vertex alpha**. Vertex **G** instead masks the Secondary Details texture alpha
**[V]** (`flightsim-material-textures.md:170,176`).

## 1.3 What decides the meaning

- **[V]** The Blender material type is an enum (`msfs_standard`, `msfs_windshield`,
  `msfs_glass`, `msfs_geo_decal_frosted`, …)
  (`EXP/blender/utils/msfs_material_utils.py:9-26`). That enum selects the node
  tree (`EXP/blender/material/msfs_material_properties_update.py:52-98`) and the
  exported extension (e.g. `EXP/io/com/extensions/material/asobo_material_windshield.py:26,104-110`).
- **[V]** Names are labels, not semantics: E190 material 1
  `MAT_WINDSHIELD_CABIN` and E195 cockpit material 10 `windshield` both carry
  `ASOBO_material_windshield_v3` (measured in the source/pair JSON); the default
  colour layer is named `Color`, but export selection uses the active/render
  index, not the name.
- **[V]** Texture presence changes the mode with no per-vertex flag: a
  `blendMaskTexture` switches the shared tree from detail mode to blend mode
  (`EXP/blender/material/msfs_material.py:152-172`).
- **[V]** `COLOR_0` itself has no semantic flag. The same four channels mean
  different things under `ASOBO_material_windshield_v3`, a standard detail map,
  a GeoDecal Frosted material, or a plain standard material.
- **[I]** A renderer must key the channel interpretation off the material
  extension/shader (and texture presence), not off the attribute name or the
  Blender node graph.

## 1.4 E195 cockpit `windshield` — requested data note

**[V]** In the current source and package E195 cockpit glTFs, material index 10
`windshield` declares `ASOBO_material_windshield_v3` (with
`ASOBO_material_detail_map` and `ASOBO_material_shadow_options`). The
`.gltf.fs20-backup` declares the legacy `ASOBO_material_windshield` instead
(material 26). This is the measured data (`measurements.json`,
`windshield.source_material` / `package_material`).

**[V]** The brief records that the E195 cockpit `windshield` material declares
no windshield extension, believed lost when converting an MSFS 2020 Blender
file. That state is not present in the exported glTFs measured here; among the E195
cockpit files inspected, the `.fs20-backup` is the only variant with a different
extension (the legacy one). This is reported as data about these files, not as
a rule **[V]**. Whether an older
Blender source state really lost the extension is outside what the glTF can
show **[I]**.

---

# Part 2 — What MSFS packaging changes

## 2.1 Pairs, matching and coverage

**[V]** Both pairs keep the same node, mesh and primitive counts and the same
names/order. Node objects are equal after 1e-6 float rounding; the JSON text
differs in key order and float re-serialisation (e.g. `14.046731948852539` →
`14.04673194885254`) **[V]** (`node_equality` in `measurements.json`).

**[V]** Vertex matching used exact float32 POSITION equality, with a 2 mm
tolerance only for float-rounded positions. Decode errors below are computed on
exact matches only. JOINTS/WEIGHTS were paired by UV because skinned package
positions are baked to another space (§2.6).

**[V]** Matching coverage:

| Pair | Package vertices | Exact position | Near (≤2 mm) | Unmatched |
|---|---:|---:|---:|---:|
| E190 exterior | 383,253 | 323,867 (84.5%) | 19,075 | 40,311 (10.5%) |
| E195 cockpit | 705,125 | 699,869 (99.3%) | 150 | 5,106 (0.7%) |

**[V]** The E190 unmatched set is concentrated in eight primitives (the two
`Barier_ribbon` primitives, the skinned `GPU_LINE` and `use_materials=1.001`,
the two `MLG_XXX`, and the two `GEAR BRISTLES`); E195's 5,106 are 23 primitives,
mostly small screens/colliders. They are excluded from the maximum-error claims;
the Barier/skinned anomalies are described in §2.6. Near matches reached a
maximum position error of `0.001694` (E190) and `0.000702` (E195) **[V]**.

## 2.2 Attribute layout before and after

**[V]** All package vertex attributes are interleaved: 36-byte stride for static
meshes, 48-byte stride for skinned meshes. Source accessors are tightly packed
(no `byteStride`); package accessors share a small number of buffer views.

**[V]** Layout:

| Attribute | Source (both pairs) | Package, 36-byte static | Package, 48-byte skinned |
|---|---|---|---|
| `POSITION` | `FLOAT` `VEC3`, unnormalised | `FLOAT` `VEC3` @ 0 | `FLOAT` `VEC3` @ 0 |
| `NORMAL` | `FLOAT` `VEC3`, unnormalised | `BYTE` `VEC4` @ 16: xyz signed/127, w = 127 | same |
| `TANGENT` | **absent** | `BYTE` `VEC4` @ 12: xyz signed/127, w = ±127 | same |
| `TEXCOORD_0` | `FLOAT` `VEC2` | `SHORT` `VEC2` @ 20: two IEEE half-floats | same |
| `TEXCOORD_1` | `FLOAT` `VEC2`, only 4/141 (E190) or 12/894 (E195) primitives | `SHORT` `VEC2` @ 24: half-floats, added to all primitives | same |
| `COLOR_0` | `UNSIGNED_SHORT` `VEC4`, **normalized**, 2/141 or 7/894 primitives | `UNSIGNED_SHORT` `VEC4` @ 28, **not normalized**, four half-floats; 139/141 or 888/894 primitives | `BYTE` `VEC4` @ 44, not normalized, unsigned RGBA8 /255; 2/141 or 6/894 primitives |
| `JOINTS_0` | `UNSIGNED_BYTE` `VEC4` | — | `UNSIGNED_SHORT` `VEC4` @ 28 |
| `WEIGHTS_0` | `FLOAT` `VEC4` | — | `UNSIGNED_SHORT` `VEC4` @ 36, **normalized**, /65535 |

**[V]** The 36-byte layout is exactly `0 POSITION, 12 TANGENT, 16 NORMAL,
20 TEXCOORD_0, 24 TEXCOORD_1, 28 COLOR_0, 36 end`; the 48-byte layout inserts
`28 JOINTS_0, 36 WEIGHTS_0, 44 COLOR_0, 48 end`. Measured from the package
accessors and buffer views (`measurements.json`, `layouts.package`).

**[V]** `TANGENT` is added to every package primitive (the source files have
none; `export_tangents` defaults false in
`EXP/io/exp/export_settings.py:365-369`), `TEXCOORD_1` is added to all
primitives, and `COLOR_0` is added to all primitives. When the source has no
`TEXCOORD_1`, the packaged UV1 equals UV0:
`322,706 / 322,706` exact-paired E190 vertices and `697,191 / 697,191` E195
vertices (within 1e-6). **[V]**

**[V]** Accessor counts rise while buffer-view counts fall because of
interleaving: E190 `758 → 1,176` accessors, `758 → 196` buffer views; E195
`3,472 → 6,901`, `3,472 → 635`.

## 2.3 Decoding and measured errors

**[V]** Decode rules that reproduce the source values are:

- `NORMAL` / `TANGENT`: signed byte `b / 127.0`; ignore the fourth normal byte
  (always 127) and read the tangent fourth byte as ±1.
- `TEXCOORD_n`: reinterpret the 16-bit pattern as an IEEE binary16 half-float,
  not as a normalized integer.
- `COLOR_0` 16-bit: reinterpret as four half-floats. `COLOR_0` 8-bit: unsigned
  byte `/255.0` (the accessor says signed `BYTE`, but the values are RGBA8).
- `WEIGHTS_0`: unsigned short `/65535.0`.
- `POSITION`: float32 as-is.

**[V]** Measured decoding errors:

| Attribute | Decode | Max error, E190 / E195 | Mean error, E190 / E195 | Basis |
|---|---|---:|---:|---|
| `POSITION` | float32 | `0 / 0` on exact matches | `0 / 0` | [V] script |
| `NORMAL` xyz | signed byte /127 | `0.012603 / 0.009743` (component) | `0.005073 / 0.005228` | [V] script; `/128` gives `0.017296 / 0.016572`, so `/127` is the better decode |
| `NORMAL` length | sqrt(x²+y²+z²) | max deviation `0.013483 / 0.013200` | `0.004822 / 0.005169` | [V] script |
| `TANGENT` | signed byte /127 | no source tangents; unit-length max deviation `0.012729 / 0.013232` | `0.004524 / 0.003238` | [V] source has no TANGENT; package adds it |
| `TEXCOORD_0` | IEEE half | `0.039246 / 0.0011297` | `9.819e-5 / 8.515e-5` | [V] script; integer normalization instead gives max `149.22 / 0.9999`, so half is correct |
| `TEXCOORD_1` | IEEE half | `0.000244 / 0.000467` | `7.463e-5 / 8.629e-5` | [V] script |
| `COLOR_0` 16-bit | four half-floats | `0 / 0.0002432` | `0 / 1.979e-5` | [V] script; integer /65535 gives max `0.7656` |
| `COLOR_0` 8-bit | unsigned byte /255 | all white, no source colors | — | [V] `MESH_YOKE_BASE`, etc. |
| `JOINTS_0` | unsigned short, direct int | values are remapped; no 1:1 source error | — | [V] layout; see §2.6 |
| `WEIGHTS_0` | unsigned short /65535 | weight-sum max `3.046e-5 / 4.580e-5` | `9.982e-6 / 6.578e-6` | [V] script; per-component comparison not used because joint slots differ |

**[V]** The measured `NORMAL` error is larger than pure 1/254 rounding; **[I]**
the packager appears to regenerate/split normals as well as quantise them. The
`/127` rule still reproduces the values to ~0.013 component / ~0.013 length in
the worst case, and `/128` is clearly worse **[V]**. Tangent w was `+1` 115,733 and `-1`
227,209 times on E190 exact pairs; `+1` 423,233 and `-1` 276,636 on E195 **[V]**.

**[V]** The normal/tangent/UV/color accessors are all `normalized: false` (or
omit the flag). A spec-only glTF reader would decode them as integers, not unit
vectors/half-floats. This is the practical reason a generic viewer cannot render
packaged MSFS vertex data without the `ASOBO_asset_optimized` convention **[I]**.

## 2.4 Painted colours and added white

**[V]** E190 exterior: the only two source `COLOR_0` primitives are black/white
GEAR masks; the package preserves them exactly (maximum channel error `0`).

**[V]** E195 cockpit: six of the eight `WINDSHIELD_FRONT.*` primitives carry
source `COLOR_0`; the other two (`.006`, `.008`) do not. The six painted
primitives have R `1.0`, G `0.0742198825`, B `0.0193789578` and A `0.0..1.0`
with 25, 30, 42, 42, 30 and 30 distinct A values. The package decodes to R `1.0`,
G `0.074219`, B `0.019379`, the same distinct A counts and the same 0..1 range.
Maximum channel errors are `R 0`, `G 1.13e-6`, `B 2.96e-7`, `A 2.43e-4` **[V]**
(`measurements.json`, `windshield`).

**[V]** The two unpainted windshield primitives (`.006`, `.008`) get a white
`COLOR_0` (`1,1,1,1`). The Button primitive (`Mesh.010`) is not white: it has its
own source colour (0.60383 grey) and is preserved; it is the seventh source
`COLOR_0` primitive (six windshields + `Mesh.010`). The remaining 887 E195
primitives and 139 E190 primitives have no source `COLOR_0` and all receive
white **[V]**.

| Pair | Source primitives without `COLOR_0` | Package added white | 16-bit half white | 8-bit RGBA8 white |
|---|---:|---:|---:|---:|
| E190 exterior | 139 | 139 | 137 | 2 (skinned stride 48) |
| E195 cockpit | 887 | 887 | 881 | 6 (skinned stride 48) |

Examples: the eight-bit white is on the skinned `GPU_LINE`/`use_materials`
(E190) and `MESH_YOKE_BASE`/`MESH_CHECKLIST_YOKE`/`C3_001.*`/`Plane.029`/
`Plane.047` (E195); the two white windshield primitives `.006`/`.008` are
16-bit half **[V]**.

**[V]** Every package primitive has `COLOR_0` (141/141, 894/894). The packager
chooses 16-bit half for the 36-byte layout and 8-bit RGBA8 for the 48-byte
skinned layout. The 8-bit accessor says `BYTE`, but the bytes are `0xFF` white,
so it must be read unsigned **[V]** (bytes measured; unsigned interpretation
**[I]** from the white result).

## 2.5 Images, textures, extensions

**[V]** Image counts are unchanged (22 E190, 24 E195), but every referenced file
is converted:

- Source: `mimeType: "image/png"`, `uri: "Name.png"`.
- Package: no `mimeType`; `uri: "NAME.PNG.KTX2"` (uppercase basename,
  `.PNG.KTX2` suffix); `extras: "ASOBO_image_converted_meta"`.
- All 22/24 referenced KTX2 files were found in the package's `texture.*`
  folders, each with a `.KTX2.json` sidecar **[V]**.
- Source PNG totals `301,579,176` / `508,953,682` bytes; package KTX2 totals
  `280,333,528` / `625,753,608` bytes (E190 / E195).
- KTX2 `vkFormat` counts: E190 `133` (BC1_RGBA_UNORM) 9, `142` (BC5_SNORM) 7,
  `145` (BC7_UNORM) 6; E195 `133` 3, `142` 6, `145` 15 **[V]**.
- The extension is named `MSFT_texture_dds` but the files are `.KTX2`; this is
  data, not a claim that the extension name matches the container **[V]**.

**[V]** Texture records change from `{sampler: 0, source: n}` to
`{extensions: {"MSFT_texture_dds": {"source": n}}}`: the core `source` and
`sampler` links are removed. The `samplers` array still has one entry; the
package adds explicit `wrapS`/`wrapT` `10497` (REPEAT), while the source sampler
has only `magFilter`/`minFilter` **[V]**.

**[V]** `extensionsUsed` gains `MSFT_texture_dds` and `ASOBO_asset_optimized`;
nothing is removed. `extensionsRequired` goes from `[]` to
`["MSFT_texture_dds"]`. The asset-level
`ASOBO_asset_optimized` object is `MajorVersion 4`, `MinorVersion 8`, with
`UseCheckerboardMaterialForMissingTextures: true` and
`UseOnlyFilenameForImageURI: true`; it also carries the asset bounding box
(`measurements.json`, `extensions.asset_optimized`).

## 2.6 Counts, indices and other changes

**[V]** Counts:

| Count | E190 source → package | E195 source → package |
|---|---|---|
| Nodes / meshes / primitives | 277/140/141 → same | 2,381/894/894 → same |
| Materials | 15 → 15 | 32 → 33 |
| Accessors | 758 → 1,176 | 3,472 → 6,901 |
| Buffer views | 758 → 196 | 3,472 → 635 |
| Images / textures / samplers | 22/22/1 → same | 24/24/1 → same |
| Skins / animations / scenes | 3/18/1 → same | 14/503/1 → same |
| glTF JSON bytes | 334,182 → 327,766 | 1,753,347 → 1,896,935 |
| BIN bytes | 15,159,400 → 16,588,468 (+9.4%) | 29,409,056 → 32,807,004 (+11.6%) |

**[V]** The E195 material increase is `glTFValidator_Added_Default_Material`
(index 32), assigned to the one source primitive that had no material
(`Cube.097`); the package/validator step leaves no materialless primitive.
E190 has no such addition **[V]**.

**[V]** Indices keep their component types: E190 `140 × UNSIGNED_SHORT + 1 ×
UNSIGNED_INT`, E195 `893 × UNSIGNED_SHORT + 1 × UNSIGNED_INT`; all primitives are
mode 4 and indexed. E195 index count/triangles are unchanged (`2,463,810` /
`821,270`). E190 changes from `1,323,342` to `1,401,366` indices (`441,114` →
`467,122` triangles). The entire increase is one anomaly:

- `Barier_ribbon` source: primitive 0 material `Belt`, 74 vertices, 210 indices;
  primitive 1 material `Pole`, 21,674 vertices, 77,814 indices.
- Package: both primitives have 21,748 vertices and 78,024 indices; materials are
  `Pole` and `Belt`. The 210-index `Belt` geometry is not preserved; the Pole
  geometry is duplicated into both slots **[V]** (`indices.per_primitive_changes`).
  This looks like a packager bug, not a rule **[I]**.

**[V]** Every package primitive gains `extras.ASOBO_primitive` (`PrimitiveCount`,
`UV2Length`, `UVLength`, `VertexCount`, `VertexType: "VTX"`,
`VertexVersion: 2`); the source has none (0 → 141/894).

**[V]** The inspected package materials are re-serialised with default-valued
fields stripped: `doubleSided: false`, `emissiveFactor: [0,0,0]`, `extras: {}`,
`metallicFactor: 1`, `roughnessFactor: 1`, `normalTexture.scale: 1`; extension
defaults such as `ASOBO_material_windshield_v3.enabled: true` and
`ASOBO_material_glass_v2.glassWidth: 0` become `{}`. These are glTF default
values, so the change is serialisation, not semantics **[V]** for the fields,
**[I]** for “semantics unchanged”.

**[V]** Skinned position data is rebaked: the E190 `GPU_LINE`/`use_materials`
and E195 skinned primitives have source positions in local/bind space and
package positions in another space (e.g. source GPU_LINE z range `-0.044..4.812`
becomes `9.886..14.742`, shifted by the node translation). This is why exact
POSITION matching fails for those primitives; JOINTS/WEIGHTS were paired by UV
instead. The exact transform/bind convention is unresolved (Open questions 3–4).

**[V]** For non-skinned primitives, `POSITION` min/max are unchanged apart from
`-0.0` vs `0.0`; package `NORMAL`, `TANGENT`, `TEXCOORD` and `COLOR_0`
accessors have no min/max. Animations, skins and scenes keep their counts.

## 2.7 Triangle winding

**[V]** The packager also reverses the triangle winding. `build/review/winding.py`
compares each indexed triangle's geometric normal (`cross(p1-p0, p2-p0)`) with
the sum of its three vertex normals, over every indexed mode-4 primitive that
has `NORMAL`:

| Pair | Source: triangles agreeing with normals | Package: triangles agreeing with normals |
|---|---:|---:|
| E195 cockpit | 100% of 821,087 | 0% of 821,134 |
| E190 exterior | 100% of 441,099 | 0% of 467,103 |

The source files are counter-clockwise (glTF's front-face convention); every
packaged triangle is clockwise (the Direct3D convention). The declaration that
selects the packaged layout, `ASOBO_asset_optimized`, therefore also selects the
clockwise front face. The viewer combines that with the glTF mirrored-node rule
(a negative-determinant world matrix reverses the winding), so the two flips
cancel for a mirrored packaged node. Files without the declaration keep the
counter-clockwise convention byte for byte.

## 2.8 Textures, ORM and tangents (A1.10/A1.11)

**[V] ORM occlusion.** An audit over all 17 packaged/source glTF pairs under
`msfs24-projects/Packages/**` found only one occlusion change: in 61 materials
the source had `occlusionTexture` and `metallicRoughnessTexture` pointing at the
same texture record (same index, UV set and transform), and the package dropped
`occlusionTexture`. 92 materials had neither slot, and there was no material with
a different occlusion slot, an added slot, or a dropped slot that did not also
match the metallic-roughness record. Every one of the 17 files declares
`ASOBO_asset_optimized`. The runtime rule is therefore: for a declared file, a
material with `metallicRoughnessTexture` and no `occlusionTexture` reads occlusion
from the metallic-roughness texture's R channel with strength 1. The audit script
and log are `build/review/orm_audit.py` / `build/review/orm_audit.txt`.

**[V] COMP channel layout and values.** `COCKPIT_STD_COMP` is a BC7_UNORM KTX2
(vkFormat 145, 8192², 14 mips). Decoding mip 0 and comparing every eighth pixel
with the source `Cockpit_STD_COMP.png` gives identical G and B (mean absolute
difference 0.290 / 0.233 of 255; means 158.14 / 47.85 against 158.14 / 47.81), so
roughness (G) and metallic (B) are unchanged. R differs: source mean 68.95,
package mean 117.16, and the package is the square root of the source
(`R_pkg = R_src^0.49994`; mean absolute residual 0.00175, i.e. below half an
8-bit step). This is a package-side gamma transform of the AO channel, not a
Glider read: the same KTX2 data decoded by DirectXTex and by Glider's own
`decodeKTX2Preview` shows it. A1.12 decodes `R²` at sampling time; G/B are
still identity.

**[V] NORM data and mips.** `COCKPIT_STD_NORM.PNG.KTX2` is BC5_SNORM (vkFormat
142, 8192², 14 mips). Its mip-0 R/G standard deviation is 22.23 / 21.98 against
7.19 / 6.55 in the source PNG. Re-encoding the source PNG to BC5_SNORM with
DirectXTex gives 7.43 / 6.79 (mean absolute error 0.70), so the raw package
values are not the source values; the square-root encoding in §2.9 explains the
larger stored standard deviation. All 14 mip levels are present and uploaded;
lower levels average the variation down (R std 16.72, 12.49, 9.58, 7.59, 6.18,
5.20 at levels 1..6). Before A1.12 the Shading Normal view was grainy at camera
distance 0.75 and smooth at 2.0, the expected high-frequency content filtered by
the mip chain rather than missing mips. After the signed-square decode the
source and packaged Shading Normal views match (correlation 0.9954, high-pass
detail ratio 0.96). Geometry Normal is smooth at both distances, so the 8-bit
packaged vertex normals are not the cause.

**[I] Sampler/mip setup.** `decodeKTX()` copies levels in level order and
`Texture::writeCompressed()` uploads them with `glCompressedTexImage2D`, sets
`GL_TEXTURE_MAX_LEVEL = levelCount - 1` and leaves the glTF sampler's
`LINEAR_MIPMAP_LINEAR` (9987) in place; a primitive without a sampler gets the
same default in `m_samplerStateMapping`. The far-view smoothing above is the
runtime evidence that the mips are used.

**[V] Packaged TANGENT.** The package adds `TANGENT` to every primitive
(44.4% of the 705,053 checked vertices have w = -1). Running the vendored
MikkTSpace code over the package's own decoded positions/normals/UVs and
comparing the result with the stored tangent gives, over the 371 normal-mapped
primitives: mean tangent dot 0.70, w sign agreement 69.3%, and only 149 of 371
primitives above a 0.99 mean dot. The mismatch is not a winding error (both
stored windings give the same tangent direction); on `Plane.103` (mesh 495) the
stored tangents match the per-triangle `dP/du` with dot 1.0 on every checked
triangle while MikkTSpace's smoothed per-vertex tangent points elsewhere
**[I]**, i.e. the exporter appears to store a per-face rather than MikkTSpace
tangent on these atlas/decal meshes. Glider therefore leaves stored tangents
alone and uses MikkTSpace only for primitives that do not have them.

## 2.9 Normal XY and COMP R square-root encoding (A1.12)

**[V] Rule.** All 17 packaged/source glTF pairs declare
`ASOBO_asset_optimized`. In a declared file the package tool also changes the
texture storage:

- BC5 SNORM normal maps store each signed tangent-space X/Y component as
  `p = sign(x) * sqrt(|x|)`; Z is not stored and is rebuilt as
  `sqrt(max(0, 1 - x*x - y*y))`.
- COMP (ORM) textures store the linear AO channel as `R = sqrt(source R)`.
  Roughness (G) and metallic (B) are unchanged.
- Albedo, emissive and every other colour channel are unchanged.

Files without the declaration keep the plain glTF values.

**[V] Method.** `build/review/sqrt_audit.py` decodes mip 0 of every packaged
KTX2, decodes the matching source PNG (same image list entry; all are 8-bit),
and fits `p = s^a` by log-log regression per channel. It reports the exponent,
the fixed candidate (`identity`, `sqrt`, `signed sqrt`, `square`, `constant`),
and the RMSE of that candidate. The full per-channel table is
`build/review/sqrt_audit.csv`; the summary is `build/review/sqrt_audit.txt`.
Normal-map RMSE excludes source values below `0.02` in signed units: an 8-bit
normal rounds a zero component to ±0.5 LSB and the square root amplifies that
floor, so those texels cannot distinguish the transform from a different
source rounding **[I]**.

### Normal maps (BC5 SNORM)

All 21 distinct normal-map textures fit `signed sqrt` in both X and Y (42
channel pairs; 41 package/source occurrences per channel because some
textures are shared by several glTFs). The fitted exponent range is `0.5013`
to `0.5181`. No normal map is identity, square, or another transform.
Low-amplitude maps whose near-zero source values made a raw fit ambiguous all
still have a 0.50 exponent and fit `signed sqrt` once the quantisation floor
is excluded.

| Texture | Format | X/R | Y/G |
|---|---|---|---|
| `190-Fuselage_NORM.png` | BC5_SNORM | signed_sqrt (0.0325) | signed_sqrt (0.0267) |
| `Bagage_NORM.png` | BC5_SNORM | signed_sqrt (0.0224) | signed_sqrt (0.0208) |
| `Button_NORM.png` | BC5_SNORM | signed_sqrt (0.0343) | signed_sqrt (0.0337) |
| `Cockpit_STD_NORM.png` | BC5_SNORM | signed_sqrt (0.0212) | signed_sqrt (0.0213) |
| `DECAL_LIT_NORM.png` | BC5_SNORM | signed_sqrt (0.0322) | signed_sqrt (0.0335) |
| `FSS-Belt_NORM.png` | BC5_SNORM | signed_sqrt (0.0540) | signed_sqrt (0.0459) |
| `FSS-Pole_NORM.png` | BC5_SNORM | signed_sqrt (0.0278) | signed_sqrt (0.0277) |
| `FSS_Stair_NORM.png` | BC5_SNORM | signed_sqrt (0.0255) | signed_sqrt (0.0188) |
| `Frost_Detail_norm.png` | BC5_SNORM | signed_sqrt (0.0516) | signed_sqrt (0.0516) |
| `Fuselage_NORM.png` | BC5_SNORM | signed_sqrt (0.0173) | signed_sqrt (0.0152) |
| `Galley_NORM.png` | BC5_SNORM | signed_sqrt (0.0300) | signed_sqrt (0.0288) |
| `LG_NORM.png` | BC5_SNORM | signed_sqrt (0.0136) | signed_sqrt (0.0137) |
| `Lights_NORM.png` | BC5_SNORM | signed_sqrt (0.0133) | signed_sqrt (0.0140) |
| `Rear_cockpit_NORM.png` | BC5_SNORM | signed_sqrt (0.0456) | signed_sqrt (0.0452) |
| `SEAT_ECO_NORM.png` | BC5_SNORM | signed_sqrt (0.0250) | signed_sqrt (0.0248) |
| `SeatNYoke_NORM.png` | BC5_SNORM | signed_sqrt (0.0243) | signed_sqrt (0.0248) |
| `Skin_NORM.png` | BC5_SNORM | signed_sqrt (0.0168) | signed_sqrt (0.0201) |
| `Wing_engine_NORM.png` | BC5_SNORM | signed_sqrt (0.0276) | signed_sqrt (0.0275) |
| `decals_bolt_NORM.png` | BC5_SNORM | signed_sqrt (0.0208) | signed_sqrt (0.0206) |
| `ground_objects_NORM.png` | BC5_SNORM | signed_sqrt (0.0235) | signed_sqrt (0.0262) |
| `seat_vip_NORM.png` | BC5_SNORM | signed_sqrt (0.0169) | signed_sqrt (0.0178) |

**[V]** There are no clearcoat normal textures in the audited packaged
materials. If a declared file has one, it uses the same BC5 path in the shader.

### COMP / ORM textures

All COMP R channels with AO variation that are used in a material fit `sqrt`:
19 distinct textures, 37 package/source occurrences (exponent `0.4916` to
`0.5050`, RMSE `0.0008` to `0.0138`). G is roughness and B is metallic; both
fit identity. The exceptions are not contradictions for rendered occlusion:

- `GLASS_COMP.png` and `Frost_Detail_comp.png` are not referenced by any
  material in the audited files and are stored as identity (`R` exponents
  `1.0092` and `0.9692`). The packager therefore did not apply the AO
  transform to unused COMP images.
- `FSS-Belt_COMP.png` has only three R levels near 1.0 (`0.988` to `1.0`);
  R, G and B are classified constant. Its AO is effectively 1 either way.

| Texture | Format | R | G | B |
|---|---|---|---|---|
| `190-Fuselage_COMP.png` | BC7 | sqrt (0.0040) | identity (0.0043) | identity (0.0026) |
| `Bagage_COMP.png` | BC1_RGBA | sqrt (0.0095) | identity (0.0093) | identity (0.0132) |
| `Button_COMP.png` | BC7 | sqrt (0.0029) | identity (0.0019) | identity (0.0046) |
| `Cockpit_STD_COMP.png` | BC7 | sqrt (0.0032) | identity (0.0031) | identity (0.0031) |
| `FSS-Belt_COMP.png` | BC1_RGBA | constant (0.0000) | constant (0.0000) | constant (0.0000) |
| `FSS-Pole_COMP.png` | BC1_RGBA | sqrt (0.0095) | identity (0.0130) | constant (0.0000) |
| `FSS_Stair_COMP.png` | BC1_RGBA | sqrt (0.0138) | identity (0.0129) | identity (0.0113) |
| `Frost_Detail_comp.png` | BC1_RGBA | identity (0.0461) | identity (0.0644) | constant (0.0000) |
| `Fuselage_COMP.png` | BC7 | sqrt (0.0031) | identity (0.0037) | identity (0.0021) |
| `GLASS_COMP.png` | BC7 | identity (0.0058) | identity (0.0072) | identity (0.0064) |
| `Galley_COMP.png` | BC7 | sqrt (0.0027) | identity (0.0020) | identity (0.0025) |
| `LG_COMP.png` | BC7 | sqrt (0.0076) | identity (0.0077) | identity (0.0151) |
| `Lights_COMP.png` | BC1_RGBA | sqrt (0.0086) | identity (0.0029) | identity (0.0158) |
| `Rear_cockpit_COMP.png` | BC7 | sqrt (0.0029) | identity (0.0022) | identity (0.0025) |
| `SCREEN_GLASS_COMP.png` | BC7 | sqrt (0.0027) | identity (0.0032) | identity (0.0036) |
| `SEAT_ECO_COMP.png` | BC7 | sqrt (0.0059) | identity (0.0065) | identity (0.0092) |
| `SeatNYoke_COMP.png` | BC7 | sqrt (0.0034) | identity (0.0032) | identity (0.0042) |
| `Skin_COMP.png` | BC7 | sqrt (0.0026) | identity (0.0019) | identity (0.0022) |
| `Wing_engine_COMP.png` | BC7 | sqrt (0.0046) | identity (0.0052) | identity (0.0047) |
| `decals_bolt_COMP.png` | BC7 | sqrt (0.0008) | identity (0.0013) | identity (0.0032) |
| `ground_objects_COMP.png` | BC1_RGBA | sqrt (0.0111) | identity (0.0112) | identity (0.0119) |
| `seat_vip_COMP.png` | BC7 | sqrt (0.0034) | identity (0.0031) | identity (0.0054) |

**[V] Explicit occlusion.** The A1.10 audit found 61 source `occlusionTexture`
records that were identical to `metallicRoughnessTexture` and were dropped by
the package; 92 materials had neither. No packaged material has an explicit
`occlusionTexture`, so the rendered path is always the fallback COMP. Decoding
an explicit slot the same way in a declared file is **inferred** from the
file-level convention, not exercised by these 17 pairs.

### Albedo and emissive

| Kind | Textures | Channels | Transform | Fit RMSE range |
|---|---:|---|---|---|
| Albedo | 26 | RGB | identity | 0.0007-0.0128 |
| Emissive | 6 | RGB | identity | 0.0041-0.0168 |

No albedo or emissive channel shows a square-root transform. The small
residual is BC1/BC3/BC7 block compression and, for `Wing_engine_ALBD`/`NORM`,
the source's RGBA/alpha layout.

---

# Open questions

1. **No formal `ASOBO_asset_optimized` schema.** The viewer's
   `resources/schemas/asobo` has no schema for it; `docs/ASOBO_EXTENSIONS_REFERENCE.md:60`
   calls it schema-less. Is the half-float/signed-byte/8-bit layout specified
   anywhere, or only encoded in the packager?
2. **`Barier_ribbon` duplication/loss.** Why does the packager give both
   primitives the Pole geometry and drop the 210-index Belt geometry? Bug,
   primitive reorder, or a material-slot merge?
3. **Skinned position space.** Which transform is baked into skinned POSITION
   (node global, skin bind, inverse bind, or a pose), and why are only some
   meshes affected?
4. **JOINTS/WEIGHTS remap.** Weight sums are preserved to ~4.6e-5, but package
   joint indices are not byte-identical to the source and per-component weights
   were not comparable. Is this a reskin, a joint remap, or just re-quantisation?
5. **E195 `windshield` extension.** Current source/package files declare
   `ASOBO_material_windshield_v3`; the `.fs20-backup` declares the legacy
   `ASOBO_material_windshield`. Was the brief's “no windshield extension” state
   a Blender-side loss that no longer exists in these exports?
6. **`clean_color_attributes` vs active colour.** The cleanup checks
   `render_color_index` (`EXP/io/com/msfs_gltf_mesh_utils.py:16`) while
   `multi_export` renames the `active_color` attribute
   (`EXP/io/exp/multi_export.py:763-781`). If the two Blender indices differ,
   the wrong attribute may be kept or removed.
7. **Windshield node graph vs runtime.** The v3 module only builds the generic
   RGB/A tree and carries `# TODO - Add new windshield shader`. Is the generic
   tree intentionally preview-only, or does it mislead artists about runtime
   R/G/B/A masks?
8. **Soft-body weights.** No vertex-colour weight path exists in the exporter or
   in the SDK pages searched; the extension is only `{"type":"balloon"}`. Does
   MSFS use `COLOR_0` for cloth/soft-body weights outside the documented path?
