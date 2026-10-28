# ASOBO Extension Reference

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md)

Inventory taken 2026-10-23. Schemas: `resources/schemas/asobo/` (38 extensions, identical to the MSFS 2024 SDK
`Schemas` folder; the MSFS 2020 SDK additionally has `ASOBO_material_glass`, the predecessor of `glass_v2`).
Usage: all 207 `.gltf` files under `F:\new-fss-aircraft-ejet\msfs24-projects` (FSS E195 projects).

**Current Glider handling** values:
- *Inspector*: shown in the Material/Element Inspector's extension sections (MSFS journal milestone 2).
- *Rendered*: the extension changes rendering.
- *Ignored*: the asset renders from core glTF data.

---

## 1. The Two Reference Assets

| | `E195-Cockpit-LOD00.gltf` | `E195-Exterior-LOD00.gltf` |
|---|---|---|
| Nodes / meshes / materials | 2,381 / 894 / 32 | 294 / 157 / 17 |
| `extensionsRequired` | none | none |
| `ASOBO_normal_map_convention` | asset: `{"tangent_space_convention": "DirectX"}` | asset: same |
| `ASOBO_unique_id` | 2,381 nodes | 294 nodes (e.g. `{"id": "AFT_DOOR"}`) |
| `ASOBO_material_invisible` | 2 materials (`InvisibleCollider`, `COLLIDER`; 448 primitives) | — |
| `ASOBO_tags` | 2 materials | — |
| `ASOBO_material_shadow_options` | 25 materials | 6 materials |
| `ASOBO_material_geometry_decal` | 7 materials | 4 materials (`Decal_sheet*`, `Bold`) |
| `ASOBO_material_draw_order` | 7 materials (offsets 5–25) | 3 materials (e.g. 21) |
| `ASOBO_material_disable_motion_blur` | 17 materials | — |
| `ASOBO_material_glass_v2` | 1 material (`glassWidth` 0) | 3 materials (`MAT_GLASS`, `_bluish`, `_red`) |
| `ASOBO_material_windshield_v3` | 1 material (`{"UVScale": 5}`) | 1 material (`MAT_WINDSHIELD_CABIN`, `{"enabled": true}`) |
| `ASOBO_material_UV_options` | 1 material (tiling 125 × 125) | — |
| `ASOBO_material_detail_map` | 1 material (color, MRAO and normal detail, `UVScale` 5, `blendThreshold` 0.1) | — |

**Keys outside the schema found in real data** (must be tolerated, and interpreted where the exporter defines them):

| Extension | Keys | Meaning (exporter) |
|---|---|---|
| `ASOBO_material_geometry_decal` | `mode` (`"default"`), `decalChan0`, `decalChan1`, `decalChan2` | Decal target channels: scenery, terrain, SimObject (`asobo_material_common.py`) |
| `ASOBO_material_windshield_v3` | `enabled`, `UVScale` | To be confirmed in A3 |

---

## 2. Usage Across 207 Project Files

| Extension | Objects | Files |
|---|---:|---:|
| `ASOBO_unique_id` | 60,833 | 166 |
| `ASOBO_normal_map_convention` (always `DirectX`) | 207 | 166 |
| `ASOBO_material_shadow_options` | 571 | 138 |
| `ASOBO_material_geometry_decal` | 486 | 144 |
| `ASOBO_material_draw_order` | 461 | 143 |
| `ASOBO_material_disable_motion_blur` | 136 | 6 |
| `ASOBO_material_glass_v2` | 39 | 23 |
| `ASOBO_material_pearlescent` | 34 | 25 |
| `ASOBO_tags` | 32 | 12 |
| `ASOBO_material_invisible` | 25 | 13 |
| `ASOBO_material_windshield_v3` | 22 | 16 |
| `MSFT_texture_dds` | 179 | 14 |
| `ASOBO_asset_optimized` (no schema) | 17 | 14 |
| `ASOBO_material_detail_map` | 15 | 11 |
| `ASOBO_material_parallax_window` | 12 | 12 |
| `ASOBO_material_UV_options` | 9 | 7 |
| `ASOBO_material_environment_occluder` | 8 | 6 |

No file uses the remaining 21 schema extensions, but they are still in scope: other MSFS assets do use them.

---

## 3. All Extensions

**Phase** is the phase in [the journal](ASOBO_EXTENSIONS_JOURNAL.md) that owns the extension (revised order, 2026-10-23). **Viewer meaning** is
the planned behavior. Exact semantics are confirmed per task against the sources in journal §1.

### 3.1 Asset and texture level

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_normal_map_convention` | `tangent_space_convention`: `DirectX` \| `OpenGL` | Every normal map in the asset uses that convention. DirectX inverts green relative to glTF's OpenGL convention. | Ignored | A1 |
| `MSFT_texture_dds` (no ASOBO schema) | texture `source` → DDS image | Use the DDS image as the texture source (the core `source` is the fallback). | Ignored as an extension; DDS files are found only through search-path name matching | A1 |
| `ASOBO_asset_optimized` (no schema) | — | Exporter metadata | Inspector | A7 |

### 3.2 Material: core shading parameters

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_invisible` | `enabled` (default true) | Material not rendered | **Rendered** (hidden; per-viewport *Show Invisible Materials*) | A1.1 |
| `ASOBO_material_UV_options` | `clampUVX/Y` (default true), `UVOffSetU/V`, `UVTilingU/V`, `UVRotation` | UV transform and clamping for the material's textures | Ignored | A5 |
| `ASOBO_occlusion_strength` | `strength` | Ambient occlusion strength | Ignored | A1 |
| `ASOBO_extra_occlusion_texture` | `extra_occlusion_texture` (textureInfo) | Second occlusion map | Ignored | A1 |
| `ASOBO_material_draw_order` | `drawOrderOffset` | Sorting offset for blended materials and decals | Ignored | A1 |
| `ASOBO_material_alphamode_dither` | `enabled` (default true) | Dithered (screen-door) alpha instead of blending | **Rendered** (OPAQUE or core `"DITHER"`: discard where alpha <= 4x4 Bayer threshold, offset per TAA frame; picking and decal prepass match) | A1.7 |
| `ASOBO_material_emissive` | `emissiveDayMultiplier`, `emissiveNightMultiplier` | Emissive scale by time of day; a viewer day/night setting | Ignored | A1 (day/night: A6) |

### 3.3 Material: layered BRDF features

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_clear_coat` | `dirtTexture` | Clear coat with a dirt mask (v1) | Ignored | A7 |
| `ASOBO_material_clear_coat_v2` | `clearcoatColorRoughnessTexture` (required), `clearcoatNormalTexture`, roughness/normal factors and tilings, `clearcoatInverseRoughness`, `clearcoatBaseRoughness` | Colored clear coat layer; can reuse the `KHR_materials_clearcoat` shader path | Ignored | A7 |
| `ASOBO_material_iridescent` | `iridescentThicknessTexture`, min/max thickness (10–400 nm), `iridescentBrightness` | Thin-film interference; can reuse the `KHR_materials_iridescence` path | Ignored | A7 |
| `ASOBO_material_pearlescent` | `pearlShift`, `pearlRange`, `pearlBrightness` | View-angle-dependent hue shift | Ignored | A7 |
| `ASOBO_material_anisotropic` | `anisotropicTexture` | Anisotropic specular (v1) | Ignored (deferred: packing not established, semantics section 17) | A7.1 |
| `ASOBO_material_anisotropic_v2` | `anisoDirectionRoughnessTexture` (RG direction, B roughness Y) | Anisotropic GGX along a tangent-space direction | **Rendered** (`KHR_materials_anisotropy` lobe: direction RG x 2 - 1, 0.5/0.5 = tangent; roughness X = core roughness along it, Y = B across it; without the texture isotropic) | A7.1 |

### 3.4 Material: detail, glass, opacity effects

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_detail_map` | `UVScale`, `blendThreshold`, `detailColorTexture`, `detailNormalTexture`, `detailMetalRoughAOTexture`, `blendMaskTexture` | Tiled detail layers blended over the base material | **Rendered** (detail colour/ORM/normal, `UVScale` + viewer-side `UVOffset`, vertex-alpha or blend-mask mode; object-attribute `scale` ignored) | A5.1 |
| `ASOBO_material_glass_v2` | `glassWidth` | MSFS glass shader | Ignored | A4 |
| `ASOBO_material_windshield` | `rainDropScale`, `wiper1–4State`, `wiperMaskTexture` | Windshield (v1) | **Partial** (reflection + SSR, details 1/2, detail reflection, secondary details, icing; wipers/rain ignored) | A4 |
| `ASOBO_material_windshield_v3` | detail roughness/opacity, micro-scratches, `detailNormalRefractScale`, wiper lines and state, three textures | Windshield with scratches, detail reflection, micro-scratches, icing | **Partial** (reflection + SSR, details 1/2, detail reflection, micro-scratches, secondary details, icing; wipers/rain/insects ignored; refraction not applicable) | A4 |
| `ASOBO_material_fresnel_fade` | `fresnelFactor`, `fresnelOpacityOffset` | View-angle-dependent opacity | **Rendered** (BLEND alpha x `clamp(pow(N.V, fresnelFactor) - fresnelOpacityOffset, 0, 1)`, inferred; OPAQUE/MASK unchanged) | A4.4 |
| `ASOBO_material_ghost_effect` | `bias`, `scale`, `power` | Fresnel-style ghost transparency | **Rendered** (BLEND alpha x `clamp(bias + scale * pow(1 - N.V, power), 0, 1)`, inferred; OPAQUE/MASK unchanged) | A4.4 |

### 3.5 Material: translucency and special surfaces

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_SSS` | `SSSColor` (RGBA in every file and exporter; schema says 2 items), `opacityTexture` | Subsurface scattering approximation | **Partial** (OPAQUE/MASK: wrapped diffuse tinted by `SSSColor` and back-lit translucency, IBL from -N and lights behind, x `SSSColor` x base colour x `1 - opacityTexture.r`; default white; formulas inferred, no blur, no sun/shadows yet) | A3.4 |
| `ASOBO_material_sail` | `sailLightAbsorption` (0–1, requires double-sided) | Back-face lighting and translucency | Ignored | A7 |
| `ASOBO_material_parallax_window` | `parallaxScale`, `roomSizeX/YScale`, `roomNumberXY`, `behindWindowMapTexture` | Interior mapping behind a flat window | Ignored | A5 |
| `ASOBO_material_wind_flex` | `windFlexNormal` | Wind-animated detail normal; needs a viewer time/wind setting | Ignored | A5 |

### 3.6 Material: decals and simulator compositing

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_geometry_decal` | `enabled`, base color/metallic/roughness/normal/emissive/occlusion blend factors (+ `mode`, `decalChan0–2` in data) | Blends selected material channels onto the surface below | Ignored (drawn as a plain blended material) | A2 |
| `ASOBO_material_blend_gbuffer` | same blend factors | Same channel blending, for non-decal geometry | Ignored | A2 |
| `ASOBO_material_environment_occluder` | `enabled` | Occludes the environment (to be confirmed: reflection/rain masking) | Ignored | A2 |
| `ASOBO_material_fake_terrain` | `enabled` | Renders as terrain in the simulator | Ignored | A2 |

### 3.7 Simulator-only material options

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_material_shadow_options` | `noCastShadow` | Excluded from sun shadow casting once shadows exist (A3.2) | Ignored | A3 |
| `ASOBO_material_disable_motion_blur` | `enabled` | Glider has no motion blur: not applicable | Ignored | A7 |
| `ASOBO_material_antialiasing_options` | `responsiveAA` | Reduce TAA history for the material; applicable to Glider's TAA | Ignored | A7 |
| `ASOBO_material_rain_options` | `rainDropScale`, `rainDropSide` | Rain on the surface; no weather in Glider | Ignored | A7 |

### 3.8 Node, mesh and animation

| Extension | Schema properties | Viewer meaning | Current | Phase |
|---|---|---|---|---|
| `ASOBO_collision_object` | `collision_objects[]`: type (box, cylinder, sphere), translation, rotation, params | Collision shapes; optional wireframe overlay | Inspector | A7 |
| `ASOBO_gizmo_object` | `gizmo_objects[]`: type, translation, rotation, params | Gizmo shapes; optional overlay | Inspector | A7 |
| `ASOBO_tags` (schema inside the gizmo folder) | `tags[]` | Collision and gizmo categories (e.g. collision) | Inspector | A7 |
| `ASOBO_macro_light` (deprecated MSFS 2020 name; the 2024 exporter only imports it, as a street light) | color, intensity, cone angle, symmetry, flashing frequency/duration/phase, rotation speed, day/night cycle | Light gizmo, optionally a punctual light with animated flashing | Inspector | A3.3, deferred |
| `ASOBO_sky_portal` (no schema shipped) | `shape_type` (1 = disc), `source_radius` (cm), `inner_cone_angle`, `outer_cone_angle` | Window area light carrying sky/sun light into the cabin; gizmo | Not listed | A3.7 |
| `ASOBO_street_light` (no schema shipped) | macro_light parameters plus `random_phase`, `flare_enabled`, `flare_only` | MSFS 2024 successor of `ASOBO_macro_light` (scenery, vehicles) | Not listed | deferred |
| `ASOBO_advanced_light` (no schema shipped) | shape (point, sphere, disc), source radius, inner/outer cone, colour, intensity, interior/exterior channels | Punctual or area light | Not listed | deferred |
| `ASOBO_softbody_mesh` | `type` (`balloon`) | Metadata (physics) | Inspector | A7 |
| `ASOBO_property_animation` | `channels[]` (sampler, target) | Animates material or other properties; map onto the existing `KHR_animation_pointer` support | Inspector | A7 (low priority) |
| `ASOBO_animation_retargeting` | `id` | Node identity for retargeting animations between files | Inspector | A7 |
| `ASOBO_unique_id` (no schema) | `id` | Node identity; show in the inspector and search | Inspector | A7 |
