# ASOBO Extension Semantics (from the MSFS 2024 Blender exporter)

Exporter **8.4.1** (`__init__.py:26`), SDK **1.8.1** build **2603316** (`C:\MSFS 2024 Flighting SDK\version.txt:1-2`), addon folder timestamps 2026-10-08; schemas read from `resources/schemas/asobo/` (identical to the SDK `Schemas/` files).

**Conventions.** **V** = verified by reading the code, schema, or asset; **I** = inferred by combining verified pieces. `SDK` means `C:\MSFS 2024 Flighting SDK\Tools\Blender\addons\io_scene_gltf2_msfs_2024\`; paths without a prefix are relative to it. `repo:` is relative to the Glider repository. `asset:` refers to read-only checks against the E195 cockpit/exterior/cabin `.gltf` files under `F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\`; the cited material or file name identifies the exact file. **Coverage** is *complete* when the source answers every requested category (residual questions are engine-side only), and *partial* when the exporter does not define a requested meaning; the missing piece is always listed under **Open questions**. Line numbers are from the installed files.

**Cross-cutting facts (apply to every section).**

- **V** All extension writes in this document are gated by `enable_msfs_extension` (default `true`). With it off, the add-on's export hooks return immediately and no ASOBO extension is written (`io/exp/gltf_hooks.py:98-102`, `io/exp/gltf_hooks.py:196-207`, `io/exp/export_settings.py:179-184`).
- **V** For every material, `AsoboMaterialCommon.to_extension` runs first and rewrites the core PBR block from the MSFS material panel: `pbrMetallicRoughness.baseColorFactor`, `metallicFactor`, `roughnessFactor`, `normalTexture.scale`, `emissiveFactor`, `alphaMode`, `alphaCutoff` (only for `MASK`), and `doubleSided` (`io/com/extensions/material/asobo_material_common.py:220-338`, called by `io/com/msfs_material_extensions.py:101-107`). The extension writers run after it and only add their own dictionaries.
- **V** `emissiveFactor` is written as `emissiveColor * emissiveScale`, and `KHR_materials_emissive_strength` is explicitly removed (`io/com/extensions/material/asobo_material_common.py:293-302`, `:337-338`). The exporter therefore writes factors above 1 even though core glTF's schema caps `emissiveFactor` at 1; MSFS uses the larger value. Real cockpit data contains `[3000,3000,3000]` (`asset`, materials such as `$PFDSCREEN_L`).
- **V** Each ASOBO texture is exported through a temporary image node wired to a Principled Base Color socket; the resulting texture info has `tex_coord = None` and no mapping node, so the glTF has no `texCoord` key (defaults to TEXCOORD_0) and no `KHR_texture_transform` (`io/com/msfs_material_utils.py:113-202`, especially `:199-201`). The one deliberate exception is extra occlusion, which forces `texCoord = 1` (`io/com/extensions/material/asobo_extra_occlusion.py:61-62`).
- **V** When MSFS extensions are enabled, the exporter forces `export_tangents=False` (`io/exp/multi_export.py:116`, repeated for the other exporter branches). Normal maps therefore have to be evaluated with a generated tangent basis; the exporter does not store one.
- **V** In the Blender preview graph, the standard path is `baseColor = baseColorFactor.rgb * baseColorTexture.rgb * vertexColor.rgb`, `alpha = alphaFactor * baseColorTexture.a * detailColorTexture.a * vertexColor.a`, `metallic = metallicFactor * OMR.b`, `roughness = roughnessFactor * OMR.g`, and `occlusion = OMR.r` (`blender/material/msfs_material.py:1384-1402`, `:1491-1492`, `:1596-1669`, `:2221-2223`, `:810-811`). The alpha `MASK` test is `alpha > cutoff`, not `>=` (`blender/material/msfs_material.py:1596-1669`). The graph is a Blender preview approximation; where it disagrees with the exported numbers, the exported numbers are what MSFS consumes.

## 1. `ASOBO_normal_map_convention`

**Coverage: complete.**

**Written when**

- **V** Unconditionally on the asset, whenever the MSFS export hook is enabled: `gltf_asset.extensions["ASOBO_normal_map_convention"] = {"tangent_space_convention": "DirectX"}`. There is no material, texture, or Blender setting that changes or omits it (`io/exp/gltf_hooks.py:104-124`).
- **V** It is written even if the asset contains no normal maps, and it cannot be exported as `OpenGL` by this exporter.

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `tangent_space_convention` | string, hard-coded `"DirectX"`; no range | none | Schema allows `DirectX`, `OpenGL`, or any string, with no default (`repo:resources/schemas/asobo/ASOBO_normal_map_convention/gltf.ASOBO_normal_map_convention.schema.json:6-20`) | `io/exp/gltf_hooks.py:120-124` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| Core `normalTexture` | RGB; G is DirectX-convention | TEXCOORD_0 | no | `io/com/extensions/material/asobo_material_common.py:274-282`; `blender/material/msfs_material.py:2391-2432` | V |
| `ASOBO_material_detail_map.detailNormalTexture` | RGB; G is DirectX-convention | TEXCOORD_0; shader uses detail UV | no | `blender/material/msfs_material.py:2410-2437` | V |
| Normals in clearcoat/windshield/other extensions | RGB; presumed same asset-level convention | TEXCOORD_0 unless stated | no | `io/exp/gltf_hooks.py:120-124` (asset-level; no per-texture override in the exporter) | I |

**Shading meaning**

- **V** For a DirectX tangent-space map, the Blender graph flips G before decoding: the Y curve maps `0 -> 1` and `1 -> 0` (`blender/material/msfs_material.py:2392-2401`, `:2410-2419`). The equivalent expression is `G_use = 1 - G_tex`, then `N_tangent = normalize(2 * (R, G_use, B) - 1)`.
- **I** The asset-level statement means every normal map in the asset is DirectX, including maps referenced by later ASOBO extensions; the graph only demonstrates it for the core and detail maps.
- **I** It does not modify the glTF core `normalTexture.scale`; that remains a separate scalar (`io/com/extensions/material/asobo_material_common.py:314-318`).

**Interactions**

- **V** Tangents are not exported, so the viewer must generate a tangent basis before applying the green flip (`io/exp/multi_export.py:116`).
- **V** The separate `ASOBO_material_flip_back_face` extension (not in this task) flips back-face normals for double-sided materials; it is unrelated to this green-channel convention (`io/com/extensions/material/asobo_material_flip_back_face.py:49-68`).
- **V** No effect on base color, ORM, occlusion, or emissive.

**Open questions**

1. Does MSFS apply the asset convention to every normal map (detail, clearcoat, windshield, iridescent, etc.), or only to the core `normalTexture`? The asset-level wording says every map, but the exporter only shows the flip for core and detail.
2. What tangent basis does MSFS use when TANGENT is absent: MikkTSpace, screen-space derivatives, or per-triangle? The exporter only disables tangents.
3. What should a viewer do for a missing, `"OpenGL"`, or unknown `tangent_space_convention` value? The exporter never produces those cases.

## 2. `ASOBO_occlusion_strength`

**Coverage: partial** (the exporter writes the value but does not use it in any shader graph).

**Written when**

- **V** When the material's `msfs_occlusion_strength` differs from its default `1.0`; the writer adds `{"strength": value}` and inserts the extension (`io/com/extensions/material/asobo_occlusion_strength.py:44-59`, generic non-default rule at `io/com/msfs_material_utils.py:380-387`). At exactly `1.0` the extension is omitted.
- **V** Any material type that exposes `OCCLUSIONSTRENGTH` can emit it: standard, clearcoat, anisotropic, glass, ghost, hair, decals, fresnel, parallax, SSS, sail, propeller, porthole, tire, tree, vegetation, windshield, fake terrain (`blender/utils/msfs_material_utils.py:54`; attribute lists in the corresponding `blender/material/*.py`).
- **V** For windshield materials the same property is labelled **Reflection Mask Strength** and is exported under the same `ASOBO_occlusion_strength`/`strength` names (`blender/utils/msfs_material_utils.py:179`; `blender/material/msfs_material_windshield.py:186-191`; `io/com/extensions/material/asobo_occlusion_strength.py:44-59`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `strength` | number, default `1.0`, Blender UI range `0.0..2.0` | none | Schema has `strength` as a bare number, no default (`repo:resources/schemas/asobo/ASOBO_occlusion_strength/gltf.ASOBO_occlusion_strength.schema.json:6-11`) | default/range: `blender/utils/msfs_material_utils.py:54`, `io/com/extensions/material/asobo_occlusion_strength.py:62-69` | V |

**Textures**

None. It is a scalar only.

**Shading meaning**

- **V** The value is never wired into the Blender shader graph: `force_update_nodes` does not set an occlusion-strength node and the standard material only draws the UI slider (`blender/material/msfs_material.py:907-939`; `blender/material/msfs_material_standard.py:178-182`). No `OCCLUSIONSTRENGTH` reference exists in `blender/material/*.py` beyond attribute lists and panels.
- **V** The core `occlusionTexture` is created from the OMR texture index without a core `strength` (`io/com/extensions/material/asobo_material_common.py:241-246`, `:229-246`), so this extension is the only strength channel the exporter writes.
- **I** The name and the core glTF analogue imply a modulation of the occlusion darkening, e.g. `ao_used = 1 - strength * (1 - ao)`. This is **not** stated by the exporter; the exact operator is unknown.

**Interactions**

- **V** With `ASOBO_extra_occlusion_texture`: the preview graph multiplies the two AO maps before applying them to base color; `strength` is not part of that multiplication (`blender/material/msfs_material.py:250-268`, `:2641-2655`).
- **V** With core glTF: this extension is used instead of `occlusionTexture.strength`; the exporter does not write the latter.
- **V** For windshields it is a reflection-mask strength, not ambient occlusion. The same JSON key changes meaning by material type.

**Open questions**

1. What is the exact operator and clamping of `strength`?
2. Does it replace a missing core `occlusionTexture.strength`, multiply it, or act only on the extra occlusion map?
3. How is it combined with the `ASOBO_extra_occlusion_texture` channel (before or after the product)?
4. For windshields, how does `strength` modulate the reflection mask (the schema says nothing and there is no shader implementation)?

## 3. `ASOBO_extra_occlusion_texture`

**Coverage: partial** (the exporter's data and preview combination are defined; the canonical extension name and the strength interaction are not).

**Written when**

- **V** When the material's `msfs_occlusion_uv2` image pointer is not `None`; the writer creates a texture info, forces `texCoord = 1`, and inserts the extension (`io/com/extensions/material/asobo_extra_occlusion.py:46-68`, especially `:51-62`). If the image is cleared, the extension is omitted.
- **V** Any material type that exposes `OCCLUSIONUV2` can emit it (standard, fresnel, glass, ghost, fake terrain, geo decal, parallax, windshield, etc.); there is no material-type check in the writer.

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `extraOcclusionTexture` | textureInfo object, no default; optional | none; `texCoord` forced to `1` | **Mismatch:** schema says extension `ASOBO_extra_occlusion_texture` and key `extra_occlusion_texture` (`repo:resources/schemas/asobo/ASOBO_extra_occlusion_texture/gltf.ASOBO_extra_occlusion_texture.schema.json:6-11`); exporter uses `ASOBO_extra_occlusion` and `extraOcclusionTexture` | `io/com/extensions/material/asobo_extra_occlusion.py:27`, `:61-67`; `io/exp/texture_lib/gltf_material.py:285-297` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| `extraOcclusionTexture` | R (occlusion); G/B/A unused | TEXCOORD_1 (`texCoord = 1`) | no | `io/com/extensions/material/asobo_extra_occlusion.py:61-62`; channel label `blender/utils/msfs_material_utils.py:210`; texture-lib slot `MTL_BITMAP_OCCLUSION` at `io/exp/texture_lib/bitmap_config.py:22` | V |
| Channel R specifically | R | UV1 | no | `blender/material/msfs_material.py:2647-2649` (AO UV2 separated and output 0 used) | V |

**Shading meaning**

- **V** In the Blender preview, the OMR AO and the extra AO are multiplied: `ao_combined = OMR.r * extraOcclusionTexture.r` (`blender/material/msfs_material.py:2647-2652`), then the base color is multiplied by that product (`:2651-2655`). The full node also mixes through a shadow mask, which is a preview device, not an exported value.
- **V** `ASOBO_occlusion_strength` is not applied in this preview graph.
- **I** The engine likely uses the same product; the texture-lib maps it to the single `MTL_BITMAP_OCCLUSION` slot and the exporter forces UV1, which matches a second AO map.

**Interactions**

- **V** Uses TEXCOORD_1 despite `ASOBO_material_UV_options` being a single transform for the main UV set; the UV2 map is explicit in the graph and is not transformed by the UV-options group (`blender/material/msfs_material.py:1862-1867`, `:1902`).
- **V** With `ASOBO_occlusion_strength`: both AO maps are combined first; strength has no preview effect.
- **V** With windshields: the same attribute/key is used for the reflection mask (`REFLECTIONMASKTEXTURE`), still UV1 (`blender/utils/msfs_material_utils.py:241`).

**Open questions**

1. Which name does the MSFS 2024 runtime actually read: the schema's `ASOBO_extra_occlusion_texture`/`extra_occlusion_texture`, or the exporter's `ASOBO_extra_occlusion`/`extraOcclusionTexture`? The exporter and its texture library use the short name; the schema uses the long one.
2. Is only R read, or does MSFS use G/B/A for anything? The property label says "Occlusion (UV2)", the texture-lib slot is `MTL_BITMAP_OCCLUSION`, and the preview uses R.
3. What is the exact combination with `ASOBO_occlusion_strength` and the core OMR AO (product, minimum, or strength applied per map)?
4. Does the extra map respect its own UV transform, or only the fixed UV1 set?

## 4. `ASOBO_material_emissive` (and standard-material emissive)

**Coverage: partial** (the exported values and Blender graph are defined; the requested day/night behaviour and units are not).

**Written when**

- **V** When `emissiveDayMultiplier != 1.0` or `emissiveNightMultiplier != 1.0`; each non-default key is added and the extension is inserted if the result is non-empty (`io/com/extensions/material/asobo_material_emissive.py:49-65`; non-default rule `io/com/msfs_material_utils.py:380-387`). With both multipliers at their defaults (`1.0`), the extension is omitted.
- **V** It is available on material types that expose the two multipliers (standard, anisotropic, clearcoat, fake terrain, geo decals, fresnel, glass, ghost, hair, parallax, porthole, propeller, sail, SSS, tire, tree, vegetation, windshield); invisible and environment occluder do not expose them.
- **V** Independently of this extension, every material gets a core `emissiveFactor` from `emissiveColor * emissiveScale`. The default scale is `1000.0`; the UI allows up to `200000.0` (`io/com/extensions/material/asobo_material_common.py:293-302`, `:456-467`; `blender/utils/msfs_material_utils.py:53`, `:58-60`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `emissiveDayMultiplier` | number, default `1.0`, range `0.0..100000.0` | none | Schema has the key as a bare number, no default (`repo:resources/schemas/asobo/ASOBO_material_emissive/gltf.ASOBO_material_emissive.schema.json:6-15`) | `io/com/extensions/material/asobo_material_emissive.py:26-29`, `:67-73` | V |
| `emissiveNightMultiplier` | number, default `1.0`, range `0.0..100000.0` | none | same | `io/com/extensions/material/asobo_material_emissive.py:26-29`, `:75-80` | V |
| Core `emissiveFactor` (not part of this extension) | RGB array, computed as `emissiveColor * emissiveScale`; no clamp in the exporter | `emissiveColor` is linear `[0,1]`, `emissiveScale` `[0,200000]`, so the product can be far above 1 | Core glTF field; the exporter removes `KHR_materials_emissive_strength` instead of using it | `io/com/extensions/material/asobo_material_common.py:293-302`, `:337-338`; `blender/utils/msfs_material_utils.py:53` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| Core `emissiveTexture` | RGB; A is not described/used | TEXCOORD_0 | no | `io/com/extensions/material/asobo_material_common.py:248-263`; label "Emissive Texture (RGB)" at `blender/utils/msfs_material_utils.py:203` | V |
| Blender preview emissive | Color (RGB) times texture, times scale | main UV | no | `blender/material/msfs_material.py:2226-2302` | V |

**Shading meaning**

- **V** Exported data: `emissiveFactor = emissiveColor * emissiveScale`, and the core material references the emissive texture.
- **I** The resulting MSFS emissive radiance is `E = emissiveFactor * emissiveTexture.rgb`, with no KHR emissive-strength multiplier.
- **V** Blender preview: the emissive group computes `scale * (emissiveColor * emissiveTexture.rgb)` and feeds it to Principled **Emission Color**, while `emissiveScale` is also fed to **Emission Strength** (`blender/material/msfs_material.py:2226-2302`, `:834-839`). Blender consequently previews `scale^2 * color * texture`, unlike the exported single-scale factor. Treat this as a preview artefact, not glTF semantics.
- **I** Day/night: the only sensible reading of the two extension keys, and the project plan, is `E *= isNight ? emissiveNightMultiplier : emissiveDayMultiplier`; the exporter itself never evaluates day/night and the schema has no description. The multipliers are absent at `1.0`, so absent means no change.
- **V** There is no "emissive mode" property in exporter 8.4.1. The related control is the separate `ASOBO_material_day_night_switch`: when its Blender property is `true`, the UI description says the emissive is related to the in-game day/night cycle; when `false`, the emissive is always on (`io/com/extensions/material/asobo_material_day_night_switch.py:44-61`).
- **V** On import, the exporter's reverse helper splits an `emissiveFactor` into `scale = max(r,g,b)` and a normalized color, confirming the color-times-scale convention (`io/com/extensions/material/asobo_material_common.py:32-64`). This is destructive and cannot recover the original pair.

**Interactions**

- **V** `KHR_materials_emissive_strength` is removed, so a viewer that only understands core glTF must not clamp `emissiveFactor` to 1 and must not expect the KHR strength extension.
- **V** The day/night switch is a separate extension (`ASOBO_material_day_night_switch`) and is not part of `ASOBO_material_emissive`; the exporter writes it only when the Blender property is true (`io/com/extensions/material/asobo_material_day_night_switch.py:42-52`).
- **V** Emissive does not change alpha mode, double-sidedness, draw order, or the base PBR factors.
- **V** No bloom/glow parameters are exported by this add-on; bloom is a renderer decision.

**Open questions**

1. What unit is `emissiveFactor` in MSFS: nits, cd/m2, or an arbitrary HDR scalar? The exporter states no unit; the default scale of 1000 and real values of 3000 show only magnitude.
2. How exactly do the day/night multipliers apply: multiply `emissiveFactor`, multiply the final emissive radiance, or replace it? At what time-of-day value does the switch happen, and is there interpolation?
3. Is `emissiveDayMultiplier` used during day and `emissiveNightMultiplier` at night, or are they two ends of a continuous blend? The names and defaults are all the source provides.
4. How does `ASOBO_material_day_night_switch` interact with the multipliers? Its description distinguishes "related to the cycle" from "always on", but gives no formula.
5. Is the double application of `emissiveScale` in the Blender graph intentional (preview compensation) or a bug? Only the exported `emissiveFactor` is authoritative for MSFS.

## 5. `ASOBO_material_draw_order`

**Coverage: partial** (the value is defined; the sorting rule is not).

**Written when**

- **V** When `msfs_draw_order_offset != 0`; the writer adds `{"drawOrderOffset": value}` and inserts the extension (`io/com/extensions/material/asobo_material_draw_order.py:44-59`; non-default rule `io/com/msfs_material_utils.py:380-387`). At `0` it is omitted.
- **V** Available on most material types (standard, clearcoat, anisotropic, fresnel, fake terrain, all three geo decals, ghost, glass, hair, parallax, propeller, porthole, SSS, sail, tire, vegetation, windshield, tree); invisible and environment occluder do not expose it (`blender/utils/msfs_material_utils.py:63` and the attribute lists in `blender/material/*.py`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `drawOrderOffset` | integer, default `0`, range `-999..999` | none; written as a JSON number | Schema says `number`, no default or range (`repo:resources/schemas/asobo/ASOBO_material_draw_order/gltf.ASOBO_material_draw_order.schema.json:6-11`) | `io/com/extensions/material/asobo_material_draw_order.py:61-68` | V |

**Textures**

None.

**Shading meaning**

- **V** There is no shading formula; it is a sorting key. The Blender UI describes it as "Draw Order Offset, to manually sort decals draw order for example" (`io/com/extensions/material/asobo_material_draw_order.py:63-65`).
- **V** The exporter writes the offset and does not sort primitives by it; no other exporter file reads `drawOrderOffset` (only the writer and the property definition reference it).
- **I** The viewer should apply it before the normal blended-material distance sort, as a priority/offset, but the sign and tie-break are not in the source.

**Interactions**

- **V** It is intended for decals and blended materials, but is available on opaque material types too; the source does not restrict it to `BLEND`.
- **V** Used together with `ASOBO_material_geometry_decal` in the E195 data (offsets `3..30`; e.g. `DECAL_LIT` has `10`, `MAT_SCREEN_GLASS` has `25`).
- **V** It does not change `alphaMode`, depth testing, or double-sidedness.

**Open questions**

1. Does a larger offset draw earlier or later? The name "offset" and the positive E195 values are not enough to determine the direction.
2. Is the offset applied per material, per primitive, or per node, and how are ties broken (distance, material index, stable submission order)?
3. Does it affect only blended/transparent draws, or also opaque and mask draws? The source places it on every material type.
4. Is the offset added to the distance sort key, multiplied, or used as a separate bucket before sorting?

## 6. `ASOBO_material_alphamode_dither`

**Coverage: partial** (the flag and alpha source are defined; the dither pattern is not).

**Written when**

- **V** Exactly when the material's MSFS alpha mode is `"DITHER"`; the writer inserts `{"enabled": true}` (`io/com/extensions/material/asobo_material_alphamode_dither.py:40-50`). Any other alpha mode omits the extension.
- **V** The same MSFS alpha mode is copied verbatim into the core `alphaMode` (`io/com/extensions/material/asobo_material_common.py:321-324`). The exporter's enum includes `DITHER` alongside `OPAQUE`, `MASK`, and `BLEND` (`io/com/extensions/material/asobo_material_common.py:469-499`).
- **V** In Blender the mode is previewed as hashed/blended transparency because Eevee has no dither mode: `HASHED` + `DITHERED` on Blender 4.2+, or `BLEND` on older versions (`blender/material/msfs_material.py:354-362`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `enabled` | boolean; the exporter only writes `true` | none | Schema has `enabled`, default `true` (`repo:resources/schemas/asobo/ASOBO_material_alphamode_dither/gltf.ASOBO_material_alphamode_dither.schema.json:6-11`) | `io/com/extensions/material/asobo_material_alphamode_dither.py:43-50` | V |

**Textures**

None. The alpha source is the standard alpha: `baseColorFactor.a`, base/detail texture alpha, and vertex alpha (`blender/material/msfs_material.py:1596-1669`).

**Shading meaning**

- **V** The Blender property description calls it "The rendered output is blend with dithering dot pattern" (`io/com/extensions/material/asobo_material_common.py:492-495`).
- **I** That implies the layer is rendered with alpha coverage rather than sorted blending.
- **I** A screen-door/ordered-dither interpretation is `if (bayer(x,y) >= alpha) discard;` with the surviving fragment opaque; the source does not define the matrix, scale, or stochastic pattern.
- **V** The alpha value is the standard base alpha (`alphaFactor * baseColorTexture.a * detailColorTexture.a * vertexColor.a`), not a separate dither mask (`blender/material/msfs_material.py:1596-1669`).

**Interactions**

- **V** Core `alphaMode` is assigned the string `"DITHER"` (`io/com/extensions/material/asobo_material_common.py:321-324`).
- **I** The Khronos serializer writes that string through without remapping it, so such a file is not valid glTF 2.0 (read from the bundled Blender 4.5 `C:\Program Files\Blender Foundation\Blender 4.5\4.5\scripts\addons_core\io_scene_gltf2\io\com\gltf2_io.py:796-836`); there is no DITHER sample in the E195 data to confirm the runtime behaviour.
- **V** No `alphaCutoff` is written for DITHER because the cutoff is only written for `MASK` (`io/com/extensions/material/asobo_material_common.py:326-330`).
- **I** Dithering belongs in the opaque pass: no sorting and no blending, with depth writes on. The exporter does not encode pass selection.

**Open questions**

1. What is the exact dither pattern: 4x4/8x8 Bayer, blue noise, hash, and at what screen-space scale?
2. Should a conformant viewer map core `"DITHER"` to `BLEND` (or `MASK`) internally, or is the raw string intentional for MSFS?
3. Is the dither stable under TAA jitter, and does it write depth / cast shadows?
4. What does `"enabled": false` mean? The writer never emits false; the importer ignores the value and simply sets the Blender mode to DITHER (`io/com/extensions/material/asobo_material_alphamode_dither.py:24-37`).

## 7. `ASOBO_material_geometry_decal`

**Coverage: partial** (mode/channels/texture and alpha are defined; the per-channel blend operator is not implemented in the source).

**Written when**

- **V** When the material's `msfs_decal_mode` is non-empty. It is set to `"default"` by the Decal material type, `"frosted"` by Geo Decal Frosted, and `"blendMasked"` by Geo Decal BlendMasked (`blender/material/msfs_material_geo_decal.py:105-108`; `blender/material/msfs_material_geo_decal_frosted.py:107-108`; `blender/material/msfs_material_geo_decal_blendmasked.py:110-113`). With an empty mode the writer returns before inserting anything (`io/com/extensions/material/asobo_material_geometry_decal.py:103-116`).
- **V** For `"default"` it additionally forces all three `decalChan*` keys to their current values, including defaults (`io/com/extensions/material/asobo_material_geometry_decal.py:125-132`). For `"frosted"`/`"blendMasked"` channels are written only when non-default.
- **V** All six blend factors, `normalOverrideFactor`, `blendSharpnessFactor`, and `underClearcoat` are written only when different from their defaults (`io/com/extensions/material/asobo_material_geometry_decal.py:30-40`, `:118-123`; generic rule `io/com/msfs_material_utils.py:380-387`).
- **V** `blendMaskTexture` is written only when the separate `msfs_decal_blend_mask_texture` image is set and non-default (`io/com/extensions/material/asobo_material_geometry_decal.py:134-139`).
- **V** The schema's `enabled` key is never written by this exporter (`io/com/extensions/material/asobo_material_geometry_decal.py:104-146`).
- **I** Presence of the extension is therefore the enable signal in practice, not a JSON boolean.
- **V** Legacy files with `ASOBO_material_blend_gbuffer` are read as a decal with mode `"default"` (`io/com/extensions/material/asobo_material_geometry_decal.py:57-64`).

**Properties**

Schema file for this table: `repo:resources/schemas/asobo/ASOBO_material_geometry_decal/gltf.ASOBO_material_geometry_decal.schema.json:6-31`. Its title still says `ASOBO_material_blend_gbuffer` (`:3`), another sign that the two schemas were copied from one another.

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `mode` | string, `""` default; values `"default"`, `"frosted"`, `"blendMasked"` | none | **Not in schema** | `io/com/extensions/material/asobo_material_geometry_decal.py:107-116`; type defaults `blender/material/msfs_material_geo_decal.py:105-108`, `blender/material/msfs_material_geo_decal_frosted.py:107-108`, `blender/material/msfs_material_geo_decal_blendmasked.py:110-113` | V |
| `baseColorBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it (`repo:resources/schemas/asobo/ASOBO_material_geometry_decal/gltf.ASOBO_material_geometry_decal.schema.json:11-13`) | `blender/utils/msfs_material_utils.py:88`; `io/com/extensions/material/asobo_material_geometry_decal.py:150-155` | V |
| `metallicBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:89`; `:157-162` | V |
| `roughnessBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:90`; `:164-169` | V |
| `normalBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:91`; `:171-176` | V |
| `emissiveBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:92`; `:178-183` | V |
| `occlusionBlendFactor` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:93`; `:185-190` | V |
| `normalOverrideFactor` | number, `1.0`, `0.0..1.0` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:94`; `:192-197` | V |
| `blendSharpnessFactor` | number, `0.0`, `0.0..1.0` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:95`; `:199-205` | V |
| `underClearcoat` | boolean, `true` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:99`; `:207-210` | V |
| `decalChan0` | boolean, `true` (Scenery) | none | **Not in schema** | `blender/utils/msfs_material_utils.py:100`; `:217-221` | V |
| `decalChan1` | boolean, `false` (Terrain) | none | **Not in schema** | `blender/utils/msfs_material_utils.py:101`; `:222-226` | V |
| `decalChan2` | boolean, `true` (SimObject) | none | **Not in schema** | `blender/utils/msfs_material_utils.py:102`; `:227-231` | V |
| `blendMaskTexture` | textureInfo, optional | none | **Not in schema** | `blender/utils/msfs_material_utils.py:212`; `io/com/extensions/material/asobo_material_geometry_decal.py:134-139` | V |
| `enabled` | boolean, schema default `true` | none | Schema has it, but **the writer never emits it** | `repo:resources/schemas/asobo/ASOBO_material_geometry_decal/gltf.ASOBO_material_geometry_decal.schema.json:7-10`; absence in `io/com/extensions/material/asobo_material_geometry_decal.py:104-146` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| Core `baseColorTexture`, `metallicRoughnessTexture`, `normalTexture`, `emissiveTexture`, `occlusionTexture` | standard PBR channels | TEXCOORD_0 | no | `io/com/extensions/material/asobo_material_common.py:220-284` | V |
| `blendMaskTexture` | R in the `blendMasked` preview; G/B/A unused | TEXCOORD_0 | no | `blender/material/msfs_material_geo_decal_blendmasked.py:326-343`; `:159-167` (image node uses the UV group's main output at `:390-396`) | V |
| Texture-lib role | mapped to `MTL_BITMAP_BLENDMASK` | — | — | `io/exp/texture_lib/gltf_material.py:327-339`; `io/exp/texture_lib/bitmap_config.py:10` | V |

The extension itself carries **no** decal color, normal, emissive, or OMR texture. Those are the material's core textures; the extension only says how to blend them.

**Shading meaning**

- **V** The six per-channel blend factors are exposed in the Blender UI but are **not wired into any node graph**. The standard decal's custom tree only adds an alpha multiply; the blend-masked tree only adds a mask/alpha ramp. There is no reference to `BASECOLORBLENDFACTOR`, `METALLICBLENDFACTOR`, etc. in any shader construction beyond the attribute list and panel (`blender/material/msfs_material_geo_decal.py:75-86`, `:114-232`; `blender/material/msfs_material_geo_decal_blendmasked.py:80-102`, `:149-366`).
- **V** For the default `msfs_decal` type, the core alpha is the standard alpha multiplied by vertex alpha: `alpha = alphaFactor * baseColorTexture.a * detailColorTexture.a * vertexColor.a^2` (the alpha group already multiplies vertex alpha once at `blender/material/msfs_material.py:1581-1586`, then the decal group multiplies by vertex alpha again at `blender/material/msfs_material_geo_decal.py:157-180`, `:199-210`).
- **V** For `blendMasked`, the preview alpha is `alpha_std * linearstep(vA * (1 - threshold); low = clamp(R - (1 - sharpness), 0, 1); high = clamp(R + (1 - sharpness), 0, 1))`, where `R` is the blend-mask red channel, `sharpness` is the Blender value plus `1e-6`, and `alpha_std` already contains the vertex-alpha multiply (`blender/material/msfs_material_geo_decal_blendmasked.py:263-344`, `:407-415`). The threshold is a Blender-only property and is not exported.
- **V** `frosted` has a separate preview tree with frost-tint and freeze nodes (`blender/material/msfs_material_geo_decal_frosted.py:119-500`), but the extension carries no frost-specific keys; the engine-side effect of `mode: "frosted"` is not in the exporter.
- **I** The intended per-channel operation is a lerp from the background channel to the decal channel by the matching factor, because that is what a G-buffer decal blend means and what the factor names imply. **The source does not contain that expression**, so it must not be treated as verified.
- **I** `decalChan0/1/2` are the Scenery/Terrain/SimObject target channels (property names at `blender/utils/msfs_material_utils.py:100-102`). Their effect on the blend is engine-side; the source only writes the booleans.

**Interactions**

- **V** All three decal types set the MSFS alpha mode to `BLEND` (`blender/material/msfs_material_geo_decal.py:107`; `blender/material/msfs_material_geo_decal_frosted.py:107`; `blender/material/msfs_material_geo_decal_blendmasked.py:112`). The core `alphaMode` follows (`io/com/extensions/material/asobo_material_common.py:321-324`).
- **V** Draw order is the intended sorting mechanism for overlapping decals; the extension and draw order are separate (`io/com/extensions/material/asobo_material_draw_order.py:61-65`).
- **V** Double-sidedness is the core `doubleSided` value; back-face normal flipping is a separate extension (`io/com/extensions/material/asobo_material_flip_back_face.py:49-68`).
- **V** `blendMaskTexture` exists in both `ASOBO_material_geometry_decal` and `ASOBO_material_detail_map`, under the same key name but fed by different Blender image properties (`blender/utils/msfs_material_utils.py:209`, `:212`). Do not confuse them.
- **V** A decal material also exposes `OCCLUSIONUV2` and can emit the extra-occlusion extension (`ASOBO_extra_occlusion` in exporter 8.4.1) in addition to the decal extension (`blender/material/msfs_material_geo_decal.py:96`).

**Open questions**

1. What is the exact per-channel blend operator: `dst = mix(dst, decal, f)`, `dst = decal * f + dst * (1 - f)`, additive, or a G-buffer blend-factor equation? The source has no implementation.
2. How do `decalChan0/1/2` affect the blend? Are they target-channel masks (only blend if the underlying scene is that channel), or merely metadata? The exporter only writes them.
3. What does `underClearcoat` change: render the decal into a separate layer below clearcoat, or force a specific pass? The source only stores the boolean.
4. What do `normalOverrideFactor` and `blendSharpnessFactor` mean in the engine? The UI labels ("Normal Mode Tangent/Override", "Decal Blend Mask Sharpness") and defaults are the only evidence.
5. How are missing `decalChan*` keys interpreted (the 33 real decals with no `mode` key, and the non-default modes that can omit channels)? Are the Blender property defaults `T/F/T` the engine defaults?
6. Is `"enabled": false` meaningful? The schema defines it, the writer never emits it, and the importer ignores it.
7. Why does the geometry-decal schema not contain `mode`, `decalChan*`, `normalOverrideFactor`, `blendSharpnessFactor`, `underClearcoat`, or `blendMaskTexture` even though the exporter writes them?
8. What does `mode: "frosted"` change in the engine, and do the frost/freeze values in the Blender tree correspond to any exported parameter? They are not exported.

## 8. `ASOBO_material_blend_gbuffer`

**Coverage: partial** (legacy, no writer).

**Written when**

- **V** Never by exporter 8.4.1. The material-extension list contains `AsoboMaterialGeometryDecalExtension` but no blend-gbuffer writer (`io/com/msfs_material_extensions.py:54-88`). The only occurrence of the string is the legacy read fallback in the geometry-decal importer (`io/com/extensions/material/asobo_material_geometry_decal.py:57-64`).
- **V** On import, a file containing `ASOBO_material_blend_gbuffer` is converted to the decal material type with `mode = "default"`; its blend factors are then read from the same dictionary (`io/com/extensions/material/asobo_material_geometry_decal.py:57-64`, `:89-94`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `enabled` | boolean, schema default `true` | none | Schema has it | `repo:resources/schemas/asobo/ASOBO_material_blend_gbuffer/gltf.ASOBO_material_blend_gbuffer.schema.json:6-10`; importer ignores it at `io/com/extensions/material/asobo_material_geometry_decal.py:57-64` | V |
| `baseColorBlendFactor`, `metallicBlendFactor`, `roughnessBlendFactor`, `normalBlendFactor`, `emissiveBlendFactor`, `occlusionBlendFactor` | number, default `1.0` in the Blender geometry-decal properties; schema has no defaults | none | Schema has all six, identical to the geometry-decal schema | `repo:resources/schemas/asobo/ASOBO_material_blend_gbuffer/gltf.ASOBO_material_blend_gbuffer.schema.json:11-28`; Blender defaults `blender/utils/msfs_material_utils.py:88-93` | V |

**Textures**

None in the extension. The legacy material uses the core PBR textures; the exporter does not add a blend mask for this legacy name.

**Shading meaning**

- **V** The exporter defines no formula for this name. Because the importer maps it to the geometry-decal type and mode `"default"`, the safest verified statement is: **treat its presence as `ASOBO_material_geometry_decal` with `mode = "default"` and no decal channels**.
- **I** The name and the six factors indicate the same per-channel G-buffer blend as the geometry decal, applied to non-decal geometry. The geometry-decal section's unknown operator applies here too.
- **V** The schema has the same property set as the geometry-decal schema (both 32 lines, with the title copied as `ASOBO_material_blend_gbuffer`; `repo:resources/schemas/asobo/ASOBO_material_blend_gbuffer/gltf.ASOBO_material_blend_gbuffer.schema.json:1-32`; `repo:resources/schemas/asobo/ASOBO_material_geometry_decal/gltf.ASOBO_material_geometry_decal.schema.json:1-32`).

**Interactions**

- **V** It is the legacy spelling that the 2024 exporter reads for backward compatibility, not one it writes. New exports use `ASOBO_material_geometry_decal`.
- **I** When the imported material is used as a decal in Blender, the decal material type sets its preview alpha to `BLEND`; the JSON's core `alphaMode` is whatever the file itself contains.
- **V** It does not carry `mode`, `decalChan*`, or a blend-mask texture; the importer synthesizes mode `"default"`.

**Open questions**

1. Which older exporter version wrote `ASOBO_material_blend_gbuffer`, and was `enabled: false` ever emitted?
2. Is `enabled` a real enable/disable switch in MSFS, or only a schema placeholder? The 2024 importer ignores it.
3. Is its channel-blend operator the same as the current geometry decal's? No writer or shader graph in this SDK implements either operator.

## 9. `ASOBO_material_environment_occluder` and `ASOBO_material_fake_terrain`

**Coverage: partial** for both (the exporter writes only an enable flag; it defines no shading).

### 9a. `ASOBO_material_environment_occluder`

**Written when**

- **V** When `msfs_material_type == "msfs_environment_occluder"`; it writes `{"enabled": true}` and nothing else (`io/com/extensions/material/asobo_material_environment_occluder.py:44-60`). Other material types omit it.
- **V** The material type sets the Blender preview alpha mode to `BLEND` and `msfs_no_cast_shadow` to `true` (`blender/material/msfs_material_environment_occluder.py:37-40`). The latter is exported by the separate `ASOBO_material_shadow_options` writer (`io/com/extensions/material/asobo_material_shadow_options.py:45-60`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `enabled` | boolean; exporter writes `true` only | none | Schema has `enabled`, default `true` (`repo:resources/schemas/asobo/ASOBO_material_environment_occluder/gltf.ASOBO_material_environment_occluder.schema.json:6-11`) | `io/com/extensions/material/asobo_material_environment_occluder.py:53-59` | V |

**Textures**

None in the extension.

**Shading meaning**

- **V** The exporter defines no special node graph: the material uses the standard tree (`blender/material/msfs_material_environment_occluder.py:34-35`). Only base color is exposed, all other textures/properties are reset to defaults (`:24-26`, `:28-32`).
- **V** The only exported signals are the enable flag, core base color/alpha (BLEND), and the no-cast-shadow extension. The source does not state what the material occludes: environment reflection, rain, GI, or something else.
- **I** The name and the BLEND/no-shadow defaults suggest a transparent volume that blocks the environment probe/reflections behind it, but this is not verified.

**Interactions**

- **V** Core alpha mode is `BLEND`, and `ASOBO_material_shadow_options` carries `noCastShadow: true`.
- **V** It is not the same extension as `ASOBO_material_invisible`; it is still rendered.
- **V** No draw-order offset is exposed by this material type.

**Open questions**

1. What exactly is occluded (reflection probe/environment map, sky occlusion, rain, GI), and does it require a specific mesh volume or just a surface?
2. Does the engine require the BLEND alpha and no-cast-shadow settings, or are they only Blender preview conveniences?
3. Does it interact with `ASOBO_material_invisible` or the render order?

### 9b. `ASOBO_material_fake_terrain`

**Written when**

- **V** When `msfs_material_type == "msfs_fake_terrain"`; it writes `{"enabled": true}` and nothing else (`io/com/extensions/material/asobo_material_fake_terrain.py:44-60`). Other material types omit it.
- **V** It exposes the standard PBR texture/property set and uses the standard tree without a custom branch (`blender/material/msfs_material_fake_terrain.py:25-68`, `:76-77`). Its core alpha mode stays at the default `OPAQUE`.

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `enabled` | boolean; exporter writes `true` only | none | Schema has `enabled`, default `true` (`repo:resources/schemas/asobo/ASOBO_material_fake_terrain/gltf.ASOBO_material_fake_terrain.schema.json:6-11`) | `io/com/extensions/material/asobo_material_fake_terrain.py:53-59` | V |

**Textures**

None in the extension; the core PBR textures apply normally.

**Shading meaning**

- **V** The exporter defines no formula and no special shader tree. The extension only flags the material as fake terrain.
- **V** The core material is standard PBR: base color, OMR, normal, emissive, detail, blend mask, and UV2 occlusion.
- **I** The engine likely uses the flag to render the mesh with terrain shading/LOD rules (e.g. no object-scale UVs, terrain lighting), but the source does not define that.

**Interactions**

- **V** Core alpha is OPAQUE by default. Like the standard material it exposes draw order and no-cast-shadow, so those separate extensions can be emitted (`blender/material/msfs_material_fake_terrain.py:33-37`).
- **V** It can emit `ASOBO_material_detail_map` and `ASOBO_extra_occlusion_texture` because it exposes those texture slots.

**Open questions**

1. What does "fake terrain" change in MSFS: lighting model, UV scale, shadow reception, LOD, collision, or only editor metadata?
2. Does the renderer need a special terrain shader, or is the extension informational?
3. Does it require alpha testing/masking for terrain edges? The exporter does not set it.

## 10. `ASOBO_material_invisible`

**Coverage: complete** for the exported flag; hidden-pass details are engine-side.

**Written when**

- **V** When `msfs_material_type == "msfs_invisible"`; it writes `{"enabled": true}` and nothing else (`io/com/extensions/material/asobo_material_invisible.py:44-60`). Other material types omit it.
- **V** The material type exposes only base color plus the collision flags (`blender/material/msfs_material_invisible.py:30-36`). Its `custom_shader_tree` is a no-op, and it only changes the Blender preview blend mode (`:41-59`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `enabled` | boolean; exporter writes `true` only | none | Schema has `enabled`, default `true` (`repo:resources/schemas/asobo/ASOBO_material_invisible/gltf.ASOBO_material_invisible.schema.json:6-11`) | `io/com/extensions/material/asobo_material_invisible.py:53-59` | V |

**Textures**

None in the extension; the material type exposes no base-color/OMR/normal/emissive texture slots.

**Shading meaning**

- **I** The exporter defines no visibility formula; the name and the project's A1.1 behavior are the meaning: a material carrying this extension with `enabled` true is not rendered.
- **V** The MSFS alpha-mode property is not in the type's attribute list, so the base `set_default_properties` resets it to `OPAQUE` (`blender/material/msfs_material.py:890-905`); `switch_to_blend_render` only changes Blender's Eevee blend method (`blender/material/msfs_material.py:336-340`).
- **I** The exported core `alphaMode` is therefore `OPAQUE`, while the extension is what hides the material.
- **V** The material's core `baseColorFactor` and `doubleSided` are still written by the common block; they are irrelevant when the material is skipped.

**Interactions**

- **I** It is distinct from alpha `BLEND`/`MASK`: hiding is not transparency.
- **I** The viewer's *Show Invisible Materials* switch is a viewer-side override; the glTF only carries the flag (`repo:docs/ASOBO_EXTENSIONS_PHASE_1.md:12-16`).
- **V** It does not write a draw-order offset or no-cast-shadow flag by itself.

**Open questions**

1. What does `"enabled": false` mean? The writer never emits false, and the importer sets the invisible type on presence without checking the value (`io/com/extensions/material/asobo_material_invisible.py:27-42`).
2. Is an invisible material skipped only in the color pass, or also in shadows, picking, reflections, and collision/debug passes?
3. Should its primitives still affect bounds, draw-order slots, or hierarchy visibility?

## 11. `ASOBO_material_glass_v2`

**Coverage: partial** (`glassWidth` is defined; its physical meaning and the glass shading are not).

**Written when**

- **V** When `msfs_material_type == "msfs_glass"`; it always writes `glassWidth`, even at the default `0.0` (`io/com/extensions/material/asobo_material_glass.py:65-91`; property at `:93-99`). Other material types omit it.
- **V** The material type sets the MSFS alpha mode to `BLEND` and uses the standard shader tree without a glass-specific branch (`blender/material/msfs_material_glass.py:78-83`).
- **V** Only v2 exists in this exporter. There is no `ASOBO_material_glass` (v1) writer or reader in 8.4.1; the extension name is hard-coded to `ASOBO_material_glass_v2` (`io/com/extensions/material/asobo_material_glass.py:27`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `glassWidth` | number, default `0.0`, Blender UI range `0.0..1000.0` | export divides by `100.0`; import multiplies by `100.0` | Schema has `glassWidth` as a bare number (`repo:resources/schemas/asobo/ASOBO_material_glass_v2/gltf.ASOBO_material_glass_v2.schema.json:6-10`) | writer `io/com/extensions/material/asobo_material_glass.py:77-87`; import `io/com/extensions/material/asobo_material_glass.py:53-63`; UI `blender/utils/msfs_material_utils.py:154`, `io/com/extensions/material/asobo_material_glass.py:93-99` | V |

The conversion comment says "meters to mm" on import, but the factor is `100`, not `1000`; the Blender UI label is "(mm)". The exact unit is unresolved (open question 1). The schema's title also still says `ASOBO_material_glass`, even though the folder/file and exporter name are v2 (`repo:resources/schemas/asobo/ASOBO_material_glass_v2/gltf.ASOBO_material_glass_v2.schema.json:3`).

**Textures**

None in the extension. The glass material uses the standard core base color/OMR/normal/emissive/detail/blend-mask/UV2 slots (`blender/material/msfs_material_glass.py:61-69`).

**Shading meaning**

- **V** The exporter writes no glass-specific nodes, no IOR, no transmission, no reflection color, and no refraction scale. The Blender preview is the standard PBR tree (`blender/material/msfs_material_glass.py:82-83`).
- **I** `glassWidth` almost certainly controls physical thickness/refraction in the MSFS glass shader, because the UI is labelled "Glass Width (mm)" and the material is a glass type. The source does not define the formula, the index of refraction, or how thickness affects tint/reflection.
- **V** No `KHR_materials_transmission`, `KHR_materials_volume`, or `KHR_materials_ior` is written by this extension.

**Interactions**

- **V** Core alpha mode is `BLEND`.
- **I** Sorting and draw order therefore matter for glass.
- **V** The standard PBR factors and textures still apply; the MSFS material type adds only `glassWidth`.
- **V** `ASOBO_material_draw_order` is available and commonly used for glass (E195 `MAT_SCREEN_GLASS` has offset `25`).

**Open questions**

1. What unit does the exported `glassWidth` use? Export divides by 100; the UI says mm; a 100x factor corresponds to cm if the glTF unit is metres.
2. What is the glass shading model: refraction through a slab of `glassWidth`, tint, Fresnel reflection, roughness, and how does it use the standard OMR/base-color maps?
3. Is there a glass v1 in MSFS 2024 assets, and if so what are its keys? This exporter and the repo schema only cover v2; the project note says MSFS 2020 had a v1 predecessor.

## 12. `ASOBO_material_windshield_v3` (and earlier versions)

**Coverage: partial** (all written keys/textures are defined; no windshield shading is implemented in the Blender graph).

**Written when**

- **V** When `msfs_material_type == "msfs_windshield"`; the extension is **always** inserted. It contains every non-default parameter and every non-default texture; if all are default, it falls back to `{"enabled": true}` (`io/com/extensions/material/asobo_material_windshield.py:102-147`, fallback `:140-142`). Other material types omit it.
- **V** The material type sets alpha mode `BLEND` and uses the standard tree; `set_wiper_mask_tex` is a stub that returns `NotImplementedError` (`blender/material/msfs_material_windshield.py:104-113`).
- **V** The exporter implements v3 only. It has no reader or writer for `ASOBO_material_windshield` (v1); the repo has a v1 schema but the 2024 add-on hard-codes `ASOBO_material_windshield_v3` (`io/com/extensions/material/asobo_material_windshield.py:28`). There is no v2 in the repo schema set.

**Properties**

Schema file for this table: `repo:resources/schemas/asobo/ASOBO_material_windshield_v3/gltf.ASOBO_material_windshield_v3.schema.json:7-67`.

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `detail1Rough` | number, `0.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:168`; range `io/com/extensions/material/asobo_material_windshield.py:152-157` | V |
| `detail2Rough` | number, `0.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:169`; `:159-164` | V |
| `detail1Opacity` | number, `0.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:170`; `:166-171` | V |
| `detail2Opacity` | number, `0.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:171`; `:173-178` | V |
| `microScratchesTiling` | number, `1.0`, `0.0..1000.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:172`; `:180-185` | V |
| `microScratchesStrength` | number, `1.0`, `0.0..100.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:173`; `:187-192` | V |
| `detailNormalRefractScale` | number, `1.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:174`; `:194-199` | V |
| `wiperLines` | boolean, `false` | none | Schema has it | `blender/utils/msfs_material_utils.py:175`; `:201-204` | V |
| `wiperLinesTiling` | number, `1.0`, `0.0..100.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:176`; `:213-218` | V |
| `wiperLinesStrength` | number, `1.0`, `0.0..10.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:177`; `:206-211` | V |
| `wiper1State` | number, `0.0`, `0.0..1.0` | none | Schema has it | `blender/utils/msfs_material_utils.py:178`; `:220-225` | V |
| `UVScale` | number, `1.0`, `0.01..100.0` | none | **Not in schema**; same key name and same underlying Blender property as the detail-map UV scale; here it tiles the windshield detail normal/layers | `blender/utils/msfs_material_utils.py:82`; writer parameter `io/com/extensions/material/asobo_material_windshield.py:42`; range `io/com/extensions/material/asobo_material_detail_map.py:120-127` | V |
| `ssrAttenuation` | number, `1.0`, `0.0..1.0` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:181`; `io/com/extensions/material/asobo_material_windshield.py:227-232` | V |
| `cubemapReflectionMasking` | boolean, `false` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:180`; `:234-237` | V |
| `enabled` | boolean; only emitted as the empty-result fallback | none | **Not in schema** | `io/com/extensions/material/asobo_material_windshield.py:140-147`; asset `MAT_WINDSHIELD_CABIN` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| `wiperMaskTexture` | RGBA: R = animation In, G = animation Out, B = wiper length, A = wiper mask | TEXCOORD_0 | no | schema `repo:resources/schemas/asobo/ASOBO_material_windshield_v3/gltf.ASOBO_material_windshield_v3.schema.json:53-56`; writer `io/com/extensions/material/asobo_material_windshield.py:47-53` | V |
| `windshieldDetailNormalTexture` | RGB normal; `windshieldDetailNormalTexture.scale` is assigned from `DETAILNORMALSCALE` but the object is a plain TextureInfo, so `scale` is not serialized | TEXCOORD_0; UVScale intended | no | `io/com/extensions/material/asobo_material_windshield.py:47-53`, `:130-138`; `io/com/msfs_material_utils.py:173-201`; schema `:57-60` | V for assignment, I for dropped scale (see open question 3) |
| `scratchesNormalTexture` | RGB normal | TEXCOORD_0 | no | schema `:61-64`; writer `:47-53` | V |
| `windshieldInsectsTexture` | albedo, RGBA (property name "Insects Albedo (RGBA)") | TEXCOORD_0 | no | `blender/utils/msfs_material_utils.py:235`; writer `:47-53` | V (channel name), I (sampling) |
| `windshieldInsectsMaskTexture` | A mask (property name "Insects Mask (A)") | TEXCOORD_0 | no | `blender/utils/msfs_material_utils.py:236`; writer `:47-53` | V (channel name), I (sampling) |
| Core `baseColorTexture`, `emissiveTexture` (secondary details), `detailColorTexture` (details 1/icing/details 2), `detailMetalRoughAOTexture`, `detailNormalTexture` (icing), and `ASOBO_extra_occlusion.extraOcclusionTexture` (reflection mask, UV2) | standard / detail channels | UV0, except reflection mask UV1 | no | `blender/material/msfs_material_windshield.py:79-92`; `blender/utils/msfs_material_utils.py:232-241`; `io/com/extensions/material/asobo_extra_occlusion.py:61-62` | V |

**Shading meaning**

- **V** The Blender graph is only the standard PBR graph plus aliases: the "reflection roughness/metallic" texture is the same `msfs_occlusion_metallic_roughness_texture`, "secondary details" is the same `msfs_emissive_texture`, "details 1/icing/details 2" is the same `msfs_detail_color_texture`, "details reflection" is the same detail OMR, "icing normal" is the same detail normal, and "reflection mask" is the same `msfs_occlusion_uv2` (`blender/material/msfs_material_windshield.py:79-92`; `blender/utils/msfs_material_utils.py:232-241`). No windshield-specific combine nodes exist.
- **V** Because the standard detail writer runs for every material, a windshield with those slots also emits `ASOBO_material_detail_map`, with the same `UVScale` value. The E195 cockpit windshield has both extensions, each with `UVScale: 5` (`asset`; `io/com/extensions/material/asobo_material_detail_map.py:80-116`; `io/com/extensions/material/asobo_material_windshield.py:42`).
- **V** `ASOBO_occlusion_strength` is used for the windshield's **reflection mask strength**, but the JSON name stays `strength` (`blender/utils/msfs_material_utils.py:179`; `blender/material/msfs_material_windshield.py:186-191`; `io/com/extensions/material/asobo_occlusion_strength.py:44-59`).
- **V** No formula for detail roughness/opacity, wiper lines, micro-scratches, refraction, SSR attenuation, or insect layers exists in the Blender graph. The schema descriptions define the intended roles but not the math.
- **V** The schema says `wiperMaskTexture` is mandatory when `wiperLines` is true, and that `scratchesNormalTexture` is sampled multiple times at different rotations aligned to the U axis (`repo:resources/schemas/asobo/ASOBO_material_windshield_v3/gltf.ASOBO_material_windshield_v3.schema.json:36-43`, `:61-64`). The exporter enforces neither rule.
- **V** The v1 schema (if encountered) only has `rainDropScale`, `wiper1State..wiper4State`, and `wiperMaskTexture` (`repo:resources/schemas/asobo/ASOBO_material_windshield/gltf.ASOBO_material_windshield.schema.json:6-24`); this exporter does not read or write it.

**Interactions**

- **V** Alpha mode is `BLEND`; draw order is exposed and used.
- **V** `ASOBO_material_UV_options` applies to the main UV set and therefore to the core/detail textures; the windshield-specific textures are not in the Blender UV group.
- **V** `ASOBO_material_detail_map` is emitted alongside it when detail textures are set; do not apply both `UVScale` values twice.
- **V** Normal maps use the asset-level DirectX convention and the generated tangent basis.

**Open questions**

1. What are the exact windshield shading equations for detail 1/2 roughness and opacity, icing, micro-scratches, refraction, wiper lines, SSR attenuation, and cubemap reflection masking? The exporter only writes the parameters.
2. What does `UVScale` mean here: the detail-normal tiling, the detail-map tiling, or both? It is duplicated into `ASOBO_material_detail_map` in real data.
3. Is the missing `scale` on the detail normal (`windshieldDetailNormalTexture`) and on `detailNormalTexture` a bug? The writer assigns it, but the texture-info object has no serialized `scale` field; the E195 detail normal has only `{"index":20}`.
4. Does `"enabled": true` mean "use the default windshield shader" and is it required when other keys are present? The writer only emits it as the empty-result fallback.
5. Which earlier windshield versions exist in assets: v1 only, or also a v2? This exporter only handles v3, and the repo only has v1 + v3 schemas.
6. Are the schema-missing keys (`UVScale`, `ssrAttenuation`, `cubemapReflectionMasking`, `windshieldInsectsTexture`, `windshieldInsectsMaskTexture`) accepted by the runtime? The exporter writes them; the schema does not describe them.

## 13. `ASOBO_material_fresnel_fade` and `ASOBO_material_ghost_effect`

**Coverage: partial** for both (properties are defined; no shader formula is implemented).

### 13a. `ASOBO_material_fresnel_fade`

**Written when**

- **V** When `msfs_material_type == "msfs_fresnel_fade"`; it always writes both parameters, even at defaults (`io/com/extensions/material/asobo_material_fresnel_fade.py:58-83`, `with_default_value=True` at `:70-76`). Other types omit it.
- **V** The material sets alpha mode `BLEND` and uses the standard tree (`blender/material/msfs_material_fresnel_fade.py:75-80`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `fresnelFactor` | number, `1.0`, `0.001..100.0` | none | Schema has it, no default (`repo:resources/schemas/asobo/ASOBO_material_fresnel_fade/gltf.ASOBO_material_fresnel_fade.schema.json:6-9`) | `blender/utils/msfs_material_utils.py:141`; `io/com/extensions/material/asobo_material_fresnel_fade.py:85-91` | V |
| `fresnelOpacityOffset` | number, `1.0`, `-1.0..1.0` | none | Schema has it, no default (`:10-12`) | `blender/utils/msfs_material_utils.py:142`; `:93-98` | V |

**Textures**

None in the extension. The core base color/OMR/normal/emissive/UV2 slots apply.

**Shading meaning**

- **V** No formula is implemented. The material's custom tree is exactly the standard tree (`blender/material/msfs_material_fresnel_fade.py:75-76`), and neither property is referenced in any node construction (only the attribute list and UI panel).
- **I** The names imply a view-angle opacity ramp: a Fresnel term scaled by `fresnelFactor`, plus an opacity offset. The exact Fresnel exponent, base reflectivity, and sign of the offset are not in the source.

**Interactions**

- **V** Alpha mode `BLEND`; draw order is available.
- **V** The normal map still follows the asset-level DirectX convention and the generated tangent basis.

**Open questions**

1. What is the exact Fresnel formula and the role/sign of `fresnelOpacityOffset`? The properties are never consumed by a shader in the exporter.
2. Is `fresnelFactor` a multiplier, an exponent, or an IOR-like term, and how does it combine with the base alpha and the base color?

### 13b. `ASOBO_material_ghost_effect`

**Written when**

- **V** When `msfs_material_type == "msfs_ghost"`; it always writes all three parameters, even at defaults (`io/com/extensions/material/asobo_material_ghost_effect.py:59-84`, `with_default_value=True`). Other types omit it.
- **V** The material sets alpha mode `BLEND` and `msfs_no_cast_shadow = true`, and uses the standard tree (`blender/material/msfs_material_ghost.py:79-85`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `bias` | number, `0.0`, `0.0..1.0` | none | Schema has it (`repo:resources/schemas/asobo/ASOBO_material_ghost_effect/gltf.ASOBO_material_ghost_effect.schema.json:6-9`) | `blender/utils/msfs_material_utils.py:118`; `io/com/extensions/material/asobo_material_ghost_effect.py:86-92` | V |
| `scale` | number, `1.0`, `0.0..1.0` | none | Schema has it (`:10-12`) | `blender/utils/msfs_material_utils.py:119`; `:94-99` | V |
| `power` | number, `1.0`, `0.001..64.0` | none | Schema has it (`:13-15`) | `blender/utils/msfs_material_utils.py:120`; `:101-106` | V |

**Textures**

None in the extension; core PBR textures apply.

**Shading meaning**

- **V** No formula is implemented. The custom tree is the standard tree (`blender/material/msfs_material_ghost.py:84-85`); the three values only appear in the attribute list and panel.
- **I** The names (`bias`, `scale`, `power`) and the material name suggest a Fresnel-like ghost/force-field opacity: something like `alpha = bias + scale * pow(fresnel, power)`. This is a guess from names, not source; it is recorded as an open question, not semantics.

**Interactions**

- **V** Alpha mode `BLEND`; `ASOBO_material_shadow_options` carries `noCastShadow: true`; draw order is available.
- **V** Normal maps still use the asset-level DirectX convention.

**Open questions**

1. What is the actual ghost opacity formula, and what do `bias`, `scale`, and `power` multiply/exponentiate?
2. Is the effect driven by the view angle, the normal, the base alpha, or a texture (there is no texture slot in the extension)?
3. Does the ghost material render in a special pass (e.g. after opaque, no depth write, no shadows) beyond the alpha BLEND and no-cast-shadow defaults?

## 14. `ASOBO_material_detail_map`

**Coverage: complete** for the written data and the preview graph; the engine may differ in the fine details recorded below.

**Written when**

- **V** When at least one of the four detail textures is set: `detailColorTexture`, `detailMetalRoughAOTexture`, `detailNormalTexture`, `blendMaskTexture` (`io/com/extensions/material/asobo_material_detail_map.py:33-38`, `:84-94`). If no texture is present, the writer returns before adding the parameters, so `UVScale`/`blendThreshold` alone do **not** create the extension (`:93-94`, `:105-116`).
- **V** Once a texture exists, `UVScale` and `blendThreshold` are added only when non-default (`:105-110`; default `1.0` / `0.001` at `blender/utils/msfs_material_utils.py:82-83`).
- **V** It is not restricted to a material type: any material with those four image properties can emit it, including the windshield (see section 12).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `UVScale` | number, `1.0`, `0.01..100.0` | none; used as a UV multiplier | Schema has it (`repo:resources/schemas/asobo/ASOBO_material_detail_map/gltf.ASOBO_material_detail_map.schema.json:6-9`) | `blender/utils/msfs_material_utils.py:82`; `io/com/extensions/material/asobo_material_detail_map.py:118-127` | V |
| `blendThreshold` | number, `0.001`, `0.001..1.0` | none; used as the half-width of the mask edge | Schema has it (`:10-12`) | `blender/utils/msfs_material_utils.py:83`; `:129-136` | V |
| `detailColorTexture` | textureInfo, optional | none | Schema has it (`:13-15`) | `io/com/extensions/material/asobo_material_detail_map.py:33-38` | V |
| `detailMetalRoughAOTexture` | textureInfo, optional | none | Schema has it (`:19-21`) | `:33-38` | V |
| `detailNormalTexture` | textureInfo (schema says normalTextureInfo), optional; `scale` intended | none | Schema has it (`:16-18`) | `:33-38`, `:96-104` | V for key, I/V for dropped scale |
| `blendMaskTexture` | textureInfo, optional | none | Schema has it (`:22-24`) | `:33-38` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| `detailColorTexture` | RGB color; A is a mask (the graph multiplies alpha) | TEXCOORD_0 with detail UV = transformed UV0 * UVScale * object attribute `scale` | no | `blender/utils/msfs_material_utils.py:205`; `blender/material/msfs_material.py:1896-1901`, `:1753-1868` | V |
| `detailMetalRoughAOTexture` | R occlusion, G roughness, B metallic | same detail UV | no | `blender/utils/msfs_material_utils.py:206`; `blender/material/msfs_material.py:2000-2133` | V |
| `detailNormalTexture` | RGB normal | same detail UV | no | `blender/utils/msfs_material_utils.py:207`; `blender/material/msfs_material.py:2410-2437` | V |
| `blendMaskTexture` | R (the preview separates and uses output 0) | main transformed UV0 (not detail-scaled) | no | `blender/material/msfs_material.py:1273-1287`; `blender/material/msfs_material.py:174-202` | V |

**Shading meaning**

Let `B = baseColorFactor.rgb * baseColorTexture.rgb` (before vertex color), `D = detailColorTexture.rgb`, `aD = detailColorTexture.a`, `vA = vertexColor.a`, `ORM = (OMR.r, OMR.g, OMR.b)`, `dORM = (detailMetalRoughAOTexture.r, .g, .b)`, `m = blendMaskTexture.r`, `t = blendThreshold`.

- **V Detail mode (no `blendMaskTexture`).** The mask is vertex alpha. Base color: `smooth = smoothstep(clamp(sqrt(B * D), 0, 1))` per channel, then `C = mix(B, smooth, aD * vA) * vertexColor.rgb` (`blender/material/msfs_material.py:1415-1492`; the `smoothstep` is a MapRange node with vector smoothstep at `:1430-1438`). This is the literal graph; the `sqrt` + `smoothstep` is not a standard detail-map operator.
- **V Detail mode, OMR.** `d = (2 * dORM - 1) * vA` (via the `*2 - 1` unpack at `blender/utils/msfs_material_nodes_library.py:265-320` and the mask mix at `blender/material/msfs_material.py:2014-2132`), then
  `ao = clamp(ORM.r + d.r, 0, 1)`, `rough = clamp(ORM.g + d.g * roughnessScale, 0, 1)`, `metal = clamp(ORM.b + d.b * metallicScale, 0, 1)`
  (`blender/material/msfs_material.py:2088-2132`; the metallic/roughness scales come from the OMR group inputs at `:1924-1937`).
- **V Detail mode, normal.** `N = N_base + mix((0,0,1), N_detail, vA)` (no normalization in the graph) (`blender/material/msfs_material.py:2450-2489`). This is a preview construction and can be non-neutral at `vA = 0`.
- **V Blend mode (`blendMaskTexture` present).** `low = clamp(m - t, 0, 1)`, `high = clamp(m + t, 0, 1)`, `coverage = saturate((vA - low) / (high - low))` (`blender/utils/msfs_material_nodes_library.py:20-107`; links `blender/material/msfs_material.py:1272-1287`). Then the channel mixes are written **detail first, base second**: `C = mix(D, B, coverage) * vertexColor.rgb`, `ORM = mix(dORM, ORM, coverage)` (then roughness/metallic scaling), `N = mix(N_detail, N_base, coverage)` (`blender/material/msfs_material.py:1505-1533`, `:2145-2198`, `:2502-2526`). So `coverage = 1` keeps the base, and `coverage = 0` applies the detail layer, which is the opposite parameter order from the detail-mode mix.
- **V** `blendThreshold` is the half-width of the edge around the blend-mask red channel, not a weight. At `t = 0.001` the edge is almost a hard threshold. In detail mode it has no effect.
- **V** The detail extension adds no separate alpha operator; the standard alpha group already multiplies `detailColorTexture.a`: `alpha = alphaFactor * baseColorTexture.a * detailColorTexture.a * vertexColor.a`. If the core alpha mode is `MASK`, the graph tests `alpha > cutoff` (`blender/material/msfs_material.py:1596-1669`).
- **I** The engine may use cleaner standard operators. The source only contains the preview graph above, so these expressions are the best available evidence but not an MSFS shader dump.

**Interactions**

- **V** `ASOBO_material_UV_options` applies to the main UV before the detail scale; the detail textures then multiply by `UVScale` and the object attribute `scale` (`blender/material/msfs_material.py:1896-1901`). The blend mask uses the main UV, not the detail UV.
- **V** Detail normals follow the asset-level DirectX convention (section 1).
- **V** `detailNormalTexture.scale` is assigned from `DETAILNORMALSCALE`, but the texture object is a plain `TextureInfo`; its `scale` field is not serialized. The E195 `windshield` detail normal is `{"index":20}` with no scale (`io/com/extensions/material/asobo_material_detail_map.py:96-104`; `io/com/msfs_material_utils.py:173-201`; `asset`). The Blender preview still uses the scale as the Normal Map node strength (`blender/material/msfs_material.py:280-307`).
- **V** The `blendMaskTexture` key also exists in `ASOBO_material_geometry_decal` (different property, same JSON key). Do not merge them blindly.
- **V** A windshield can emit this extension as well as `ASOBO_material_windshield_v3`; the detail textures are shared.

**Open questions**

1. What is the object attribute `scale` in the detail-UV path? It is read as an `OBJECT` attribute and multiplied with `UVScale` (`blender/material/msfs_material.py:1839-1901`), but the exporter does not create or document it.
2. Is the missing serialized `scale` on `detailNormalTexture` a bug, and how should a viewer handle the intended `DETAILNORMALSCALE` value?
3. Is the `sqrt` + `smoothstep` detail-color formula the actual MSFS operator, or only a Blender preview approximation? It is unusual and can darken/shift colors.
4. Is the reversed operand order in blend mode (`mix(detail, base, coverage)`) intentional, and how should `coverage` be applied to each channel (base color, OMR, normal) in the engine?
5. Does the engine use only R from `blendMaskTexture`, or G/B/A for additional masks? The preview uses R.
6. Is the added detail normal normalized in the engine? The preview adds two tangent-space vectors without normalization.

## 15. `ASOBO_material_UV_options`

**Coverage: complete** for the exported values and the Blender UV graph; the clamp default and V-centering are open.

**Written when**

- **V** When any of the seven UV properties differs from its Blender default; the non-default keys are added and the extension is inserted (`io/com/extensions/material/asobo_material_uv_options.py:57-73`; generic rule `io/com/msfs_material_utils.py:380-387`). With all defaults the extension is omitted.
- **V** It is not material-type restricted; any material can expose the properties. It is one transform for the material, not per texture.

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `clampUVX` | boolean, Blender default `false`; no range | none | Schema has it, **default `true`** (`repo:resources/schemas/asobo/ASOBO_material_UV_options/gltf.ASOBO_material_UV_options.schema.json:7-10`) | `io/com/extensions/material/asobo_material_uv_options.py:76-79`; schema `:7-10` | V |
| `clampUVY` | boolean, Blender default `false` | none | Schema has it, **default `true`** (`:11-14`) | `:81-84` | V |
| `UVOffsetU` | number, `0.0`, `-10.0..10.0` | none (degrees/units written as-is) | **Key mismatch:** schema spells `UVOffSetU` (`:15-17`); exporter writes `UVOffsetU` (`blender/utils/msfs_material_utils.py:74`) | `:86-95` | V |
| `UVOffsetV` | number, `0.0`, `-10.0..10.0` | none | **Key mismatch:** schema spells `UVOffSetV` (`:18-20`) | `:97-106` | V |
| `UVTilingU` | number, `1.0`, `-10.0..100.0` | none | Schema matches (`:21-23`) | `:108-117` | V |
| `UVTilingV` | number, `1.0`, `-10.0..100.0` | none | Schema matches (`:24-26`) | `:119-128` | V |
| `UVRotation` | number, `0.0`, `-360.0..360.0` (degrees) | degrees; the Blender graph converts to radians (`blender/material/msfs_material.py:1869-1875`) | Schema matches (`:27-29`) | `:130-139`; `blender/utils/msfs_material_utils.py:78` | V |

**Textures**

The extension has no texture references. It applies to the material's main UV set and, through the detail path, to the detail textures. It does **not** apply to the UV2 occlusion map.

**Shading meaning**

- **V** The Blender graph is: `p = uv0 - (0.5, 0.5, 0)`; `p = Rotate_Z(p, radians(UVRotation))`; `u = p.x * UVTilingU + UVOffsetU`; `v = p.y * UVTilingV - UVOffsetV`; `uv_main = (u + 0.5, v - 0.5)` (`blender/material/msfs_material.py:1753-1901`; the rotation and offsets are linked at `:1871-1894`; the final add is `(0.5, -0.5)` at `:1829-1836`).
- **V** At defaults (`rotation 0`, `tiling 1`, `offset 0`) this evaluates to `uv_main = (u, v - 1)`, not the identity. The V half-texel/unit centering is therefore suspect; see open question 3.
- **I** The intended transform is the standard one: `uv' = (uv - 0.5) * tiling`, rotate, `+ 0.5`, with an offset added in UV units. The exporter's V sign is the unresolved part.
- **V** `clampUVX/Y` is not used in the node graph at all; the graph only transforms coordinates.
- **I** Because the schema default (`true`) and the Blender default (`false`) disagree, and the writer only emits non-default `true` values, an omitted clamp key is ambiguous.

**Interactions**

- **V** Detail textures inherit this transform and then multiply by `UVScale` and the object attribute `scale` (`blender/material/msfs_material.py:1896-1901`).
- **V** The UV2 occlusion map is sampled from the separate UV2 map node and is **not** transformed by this extension (`blender/material/msfs_material.py:1862-1867`, `:1902`).
- **V** No `KHR_texture_transform` is written; the core textures have no `texCoord` key and no transform. This extension is a parallel, non-KHR mechanism (`io/com/msfs_material_utils.py:199-201`).
- **V** Real E195 data uses only `UVTilingU`/`UVTilingV` (125 x 125) and no offsets, clamps, or rotation (`asset`, `LCD_DETAIL`).

**Open questions**

1. What is the effective default of `clampUVX/Y` when the keys are absent: the schema's `true` or the Blender property's `false`? The exporter cannot write `false` because false is its default, so absence is the only representation of both.
2. Which spelling does the runtime read: `UVOffsetU/V` (exporter) or `UVOffSetU/V` (schema)? The E195 data has no offset keys to disambiguate.
3. Is the `(0.5, -0.5)` final add intentional (a V-flip convention) or a bug? At defaults it shifts V by minus one.
4. How does this transform compose with a `KHR_texture_transform` that might already exist on a texture? This exporter removes/does not write KHR transforms, but imported assets may have both.
5. Are `clampUVX/Y` sampler wrap modes, shader-side `clamp()` on the final UV, or only export hints? The node graph does not implement clamping.
6. Does the object attribute `scale` participate in the main UV transform or only the detail UV? The graph applies it only to detail UV.

## 16. `ASOBO_material_parallax_window`

**Coverage: partial** (keys and texture are defined; the interior-mapping math is not implemented in the exporter).

**Written when**

- **V** When `msfs_material_type == "msfs_parallax_window"`; it always writes **all five** parameters with `with_default_value=True`, plus `behindWindowMapTexture` if set (`io/com/extensions/material/asobo_material_parallax_window.py:69-100`, `:81-87`). Other material types omit it.
- **V** The material sets alpha mode `MASK` and uses the standard tree plus an incomplete parallax helper (`blender/material/msfs_material_parallax.py:87-93`).

**Properties**

| Key | Type, default, range | Conversion | Schema | Source | St. |
|---|---|---|---|---|---|
| `roomSizeXScale` | number, `0.5`, `0.01..10.0` | none | Schema has it (`repo:resources/schemas/asobo/ASOBO_material_parallax_window/gltf.ASOBO_material_parallax_window.schema.json:10-12`) | `blender/utils/msfs_material_utils.py:146`; `io/com/extensions/material/asobo_material_parallax_window.py:104-109` | V |
| `roomSizeYScale` | number, `0.5`, `0.01..10.0` | none | Schema has it (`:13-15`) | `blender/utils/msfs_material_utils.py:147`; `:111-116` | V |
| `parallaxScale` | number, `0.5`, `0.01..1.0` | none | Schema has it under `parallaxScale` (`:7-9`) | `blender/utils/msfs_material_utils.py:148`; `:118-123` | V |
| `roomNumberXY` | number/integer-like, `5`, `1..16` | none | Schema has it (`:16-18`) | `blender/utils/msfs_material_utils.py:149`; `:125-130` | V |
| `corridor` | boolean, `false` | none | **Not in schema** | `blender/utils/msfs_material_utils.py:150`; `:132-135` | V |
| `behindWindowMapTexture` | textureInfo, optional | none | Schema has it (`:19-21`) | `io/com/extensions/material/asobo_material_parallax_window.py:89-94` | V |

**Textures**

| Slot | Channels | UV set | KHR texture transform | Source | St. |
|---|---|---|---|---|---|
| `behindWindowMapTexture` | RGB color; A likely coverage (UI label "Behind Glass Color (RGB) Alpha (A)") | TEXCOORD_0 | no | `blender/material/msfs_material_parallax.py:321`; writer `io/com/extensions/material/asobo_material_parallax_window.py:89-94`; sampling `blender/material/msfs_material_parallax.py:104-126` | V for RGB/A label, I for sampling |

**Shading meaning**

- **V** No interior-mapping formula is implemented. `parallax_shader_tree` creates a `behindGlassTex` node and a MixRGB node, links the texture into input 2 of the mix, and never links the mix output to the Principled BSDF (`blender/material/msfs_material_parallax.py:91-140`). `set_detail_color_tex` has a commented-out TODO for the output link (`:128-140`).
- **I** The parameter names and the project plan describe interior mapping: for each fragment, cast the view ray through the window plane, intersect a grid of rooms (`roomNumberXY`) with room sizes (`roomSizeXScale`, `roomSizeYScale`, `parallaxScale`), and sample `behindWindowMapTexture`. The exact ray/room math, coordinate space, and handling of `corridor` are not in the source.
- **V** The extension does not modify the core base color/OMR/normal/emissive values.
- **I** It is intended to add a behind-window layer on top of the standard PBR material.

**Interactions**

- **V** Alpha mode is `MASK`, so an alpha cutoff applies; the extension does not write its own cutoff.
- **V** Draw order is available; the standard PBR textures and UV options still apply to the front glass.
- **V** UV2 occlusion is exposed by the material type but is not part of this extension.
- **V** Real E195 lavatory windows use all five parameters and `corridor: false`; the extension is present without a behind-window texture in that sample (`asset`, E195 cabin lavatory files).

**Open questions**

1. What is the exact interior-mapping equation: ray direction, plane intersection, room-grid indexing, per-room UV, and whether `parallaxScale` is depth or offset?
2. What do `roomNumberXY`, `roomSizeXScale`, and `roomSizeYScale` mean together (grid counts vs physical room sizes), and how does `corridor` change the layout?
3. Which channels of `behindWindowMapTexture` are used (RGB only, A as mask, or depth)?
4. Is the extension expected to work with alpha `MASK`, and does the room layer write depth or only shade the window surface?
