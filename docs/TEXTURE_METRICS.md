# Texture Distortion and Texture Density

Two debug views that show how a glTF's UV mapping lies on its surface: **Texture Distortion** (`texturedistortion`) and
**Texture Density** (`texturedensity`). They port the **Heatmap** of the FSS Blender add-on
(`fss_blender_tools/uv_heatmap.py` in `msfs-blender-5-addons`, user page `docs/user/uv.md`), with its formulas,
thresholds and colours, so Glider and Blender agree on the same asset. Code: `source/imgui/TextureMetrics.cpp`
(a port of the add-on's functions: change them together), `uvHeatmapColor` in `resources/shaders/pbr.frag`. Check:
`scripts/validate_texture_metrics.py`.

## Using them

- The toolbar's shading pulldown, or the Render Mode combo in the viewport settings. Per viewport.
- CLI: `--debug-view texturedistortion [--texture-distortion-view both|shear|area]`, `--debug-view texturedensity`.
- Pipe: `set_viewport_setting render_mode texturedistortion`, `set_viewport_setting texture_distortion_view shear`,
  `get_state` `textureMetrics`.
- A legend in the viewport's lower left corner always shows the distribution chart and scale, and fits its size to what
  is open. Three collapsible sections, closed at first, per viewport and session only: **Statistics** (shares and, for
  density, the largest materials' pixels per metre), **Legend** (what the colours mean, and the warnings; its header
  counts them in amber while it is closed) and **Settings** (below the chart). Over the legend, the mouse reaches
  neither the camera nor picking. It is UI: `capture` and `--capture-viewport` read the framebuffer, without it.
- Settings (the legend's Settings section, and the Inspector below Render Mode, while a view shows): **Heatmap Opacity**,
  **UV Grid**, **Grid Cells**, **Cell Labels**, **Grid Opacity**, and **Distortion** in Texture Distortion. See
  [UV grid and cell labels](#uv-grid-and-cell-labels).

## What they measure

Every value is per triangle, in world space (node scale included).

| Measure | Definition | Scale |
|---|---|---|
| **Shear** | Conformal ratio sigma1 / sigma2 of the affine 3D-to-UV map. 1 keeps the face's shape; a uniform scale does not change it. | 1 neutral to 2+ blue |
| **Area** | (UV area / 3D area) of the face over the same ratio of its UV island, folded so that half and double read 2. | 1 neutral to 2+ green |
| **Density** | (UV area / 3D area) over the material's reference, read against the range 0.9 to 1.1: inside counts as 1, below is value / 0.9, above is value / 1.1. | log2: 0.5 red, 1 neutral, 2 blue |

- **Density compares areas, not lengths**, as the add-on does: 2.0 means twice the texels per square metre, so
  1.41 times the texels per metre.
- **Reference**: the area-weighted median of UV area / 3D area over every face of the scene that uses the material (one
  texture set per material), hidden nodes included, so isolating or hiding a part does not change the colours.
- **Island**: triangles sharing an edge with equal UVs at both ends; only an edge with exactly two faces joins them.
- **Texture Distortion's three views**: *both* (default): white none, blue shear, green area, cyan both, as a bilinear
  colour square; *shear*; *area*.
- **Error (magenta)**: `cross <= 1e-12 x longest edge squared`, or no UV area.
- **Surface colour**: the colour mixed over grey 0.8 at opacity 0.75 (the add-on's defaults), times the headlight
  `0.7 + 0.3 |n.z|` with the view-space face normal. The value goes to the screen as it is (no tone mapping), like the
  add-on's. Neutral facing the camera is 227 of 255.
- **Shares**: shear and area over 1.10 and 1.25, surface below and above the density range, faces without area, and
  islands whose mean area distortion exceeds 1.25 (Auto Seam's default Distortion Limit). Weighted by 3D area.

**Pixels per metre** (texel density in absolute units, which the add-on's heatmap does not show): for each material,
`sqrt(reference x texture width x texture height)`, in the legend (largest materials) and `get_state`. The colours do not
depend on it. Only a reference with a known texture size has one: not a material without a texture, nor a file opened
without textures.

## UV grid and cell labels

A grid in UV space, so stretched UVs show as stretched cells and a label tells where a part of the texture ends up. The
layout is the add-on's, the colours and the default strength are not (below). The two texture views have it in their
surface, on by default. **Every other view** has it as an overlay, off by default: `uv_grid_overlay`, `--uv-grid-overlay`,
the toolbar's checkerboard button (which switches the texture views' own grid while one shows) or **UV Grid Overlay** in
the Inspector. Cells, labels and grid opacity are shared.

| Setting | CLI | Values | Default |
|---|---|---|---|
| `texture_heatmap_opacity` | `--texture-heatmap-opacity` | heat colours over the grey surface, 0.3 to 1 | 0.75 |
| `texture_grid` | `--texture-grid` | on, off | on |
| `texture_grid_cells` | `--texture-grid-cells` | cells across the UV square: 8, 16, 32, 64 | 16 |
| `texture_grid_labels` | `--texture-grid-labels` | on, off | on |
| `texture_grid_opacity` | `--texture-grid-opacity` | lines and labels, 0 (none) to 1 (the add-on's strength) | 0.5 |
| `uv_grid_overlay` | `--uv-grid-overlay` | the overlay in other views: on, off | off |

- **Which UVs**: those the measures use: the density texture's UV set with its transforms. The renderer carries
  `TEXCOORD_0` and `TEXCOORD_1`; a material on a higher set has no grid. A primitive without the set has none either.
- **Labels**: column letters (A..Z, AA..) and the row number from 1. The cell counts in Blender's UVs, V up, so **A1 is the
  image's lower left** and a face has the same label in both tools. UVs outside 0 to 1 repeat the labels. A label sits in
  the cell's upper left, 0.26 of the cell high, and fades in from cells of 22 to 34 pixels on screen. Mirrored UVs show
  it mirrored.
- **Colours, Glider's own**, so that on a bent surface a label is not read as a line, nor one kind of line as the other:
  lines of constant U violet, lines of constant V orange (hues the heat scale does not use), labels one of four other
  dark colours by `(column + 2 x row) % 4`, so none of a cell's eight neighbours has its colour. The add-on draws all of them
  near black.
- **Font**: a 5 x 7 bitmap inside `pbr.frag`, as nothing may rely on a font file and no texture unit is spent. The add-on
  draws Blender's UI font, so the letters differ in shape, not in place.
- **Overlay**: `uv_grid.frag` draws the scene's surfaces once more after the view (and its wireframe), blended over the
  finished image with a small depth offset, so it does not change with lighting or tone mapping. Off, nothing runs and the
  rendering is byte-identical. It builds no measurement table: only per-mesh UV uniforms (`TextureMetrics::ensureUvMaps`).
  Like the wireframe overlay it does not read alpha, so it also shows across the holes of an alpha-masked (MASK) material.
  Its cost is one more pass over the scene.
- All of them are frame values: a change recompiles nothing, the table is not rebuilt, and the sliders run no renderer sync.
  Like the other view settings they are per viewport and session only.

## Where glTF forces a difference from Blender

| Point | Blender add-on | Glider |
|---|---|---|
| **Which UV set** | The object's active UV map. | The UV set of the material's *density texture*: the first present of base colour (or KHR_pbrSpecularGlossiness diffuse), normal, metallic-roughness, occlusion, emissive. With no texture: `TEXCOORD_0`. A primitive without that set is drawn "no data" grey. |
| **Texture transforms** | Mesh UVs only (a Mapping node is ignored). | Both applied, as the renderer does: `ASOBO_material_UV_options` tiling on UV set 0, then `KHR_texture_transform`. A non-uniform scale shows as shear, and every scale changes pixels per metre. A constant transform cancels in the area and density colours. |
| **Texture size** | Not used by the heatmap. The packer has its own target density (20.48 px/cm). | Read from the density texture's image, for pixels per metre only. |
| **Density marks and scene freedom** | Object custom properties: range = freedom x marks down the hierarchy. | Not in glTF. Every node uses the default freedom, 0.9 to 1.1. A marked object can read blue or red here and neutral in Blender. |
| **Scope** | Selected objects; shares over the selection. | The whole scene in every view; shares over the scene. |
| **Islands** | Blender's vertices and UVs. | glTF has no welded topology: a corner's exact position and its UV rounded to 1e-6 stand for Blender's vertex and UV, within one node (all its primitives, as one object). Two distinct Blender vertices that coincide with equal UVs join here. |
| **Triangles** | Blender's own tessellation. | The file's. An exported n-gon may be split differently. |
| **Pose** | Evaluated mesh at the current frame; **Refresh** after a change. | The pose when the table is built: node world matrices, and joints for a skinned mesh (the node's own transform does not apply). Files with animations are measured as Glider first shows them, not in the rest pose. Playback does not rebuild the table; choose the view again to measure the current pose. |
| **Morph targets** | Evaluated with shape keys. | The base shape (`morphedPrimitives` counts them). |
| **EXT_mesh_gpu_instancing** | Separate objects on import. | One draw: every instance shows the measures at the *first* instance's scale; its area counts once per instance. |
| **No UVs** | Skipped, listed in the legend. | "No data" grey, listed in the legend. Points and lines too. |
| **Invisible parts** | Shown when selected. | Follow the viewport's usual rules (`ASOBO_material_invisible` hides them). |

Not ported: the add-on's Visibility mode (reads a `.visibility` file beside the `.blend`).

Agreement on 2026-11-05: `validate_texture_metrics.py` checks the colours against hand-computed answers. A one-off run
of the add-on's own numpy functions (imported read-only with stubs) on the triangles of E195-Cockpit-LOD00 (957,396
triangles) and E195-Exterior-LOD00 (445,739), with the same transforms, against copies without animations: all shares
within 2e-7, the same island and over-limit counts, every material's reference within 1e-4. The add-on's drawing in a
running Blender was not compared, nor the grid against the add-on's on screen: its layout is ported from the add-on's
shader and checked against computed answers.

## How it works

- `TextureMetrics::ensure` runs from `Application::drawViewports` when a viewport selects a view: it walks the scene's
  nodes, reads POSITION and the UV set straight from the accessors (sparse, normalized, packaged half-float UVs; the
  index slice and base vertex of `ASOBO_primitive` come from the renderer mesh), in double precision, and fills one
  RGBA16F texture: per triangle shear, area, density and a degenerate flag, in draw order. About 0.4 s per million
  triangles on the main thread, reported as `buildMilliseconds`.
- `pbr.frag` compiled with `UV_HEATMAP` reads it with `gl_PrimitiveID` plus the per-mesh `u_UvHeatmapOffset`. The offset
  is a mesh uniform, so `CommandMapping::prepareInstancing` does not merge those draws (P8.3 would give all members the
  first one's offset).
- The grid is `uv_grid.glsl`, a premultiplied layer, used by `pbr.frag`'s `UV_HEATMAP` code and by `uv_grid.frag`. `u_UvHeatmapView` (opacity, cells, labels, grid opacity) is one
  `CommandGenerator` uniform written per viewport each frame; `u_UvHeatmapUvSet`, `u_UvHeatmapUvX` and `u_UvHeatmapUvY` (the
  UV's transforms as one affine map) are mesh uniforms set with the offset.
- **Off means off**: the define, its sampler and the table exist only in viewports showing a view. With none, the table
  is freed and the rendering is byte-identical.
- It rebuilds when the model, a node's transform, a material, an extension or the clip set changes. A view setting also
  syncs the renderer, but then only the offsets are set again. `builds` in `get_state` counts rebuilds.

## `get_state` `textureMetrics`

`built`, `builds`; once built: `triangles`, `degenerateTriangles`, `surfaceSquareMetres`, `shareDegenerate`,
`shear{over110,over125}`, `area{over110,over125}`, `density{under,over,rangeLow,rangeHigh}`,
`islands{count,overLimit,limit,overLimitShare}`, `skippedNoUvs` (first 32 nodes), `nonTrianglePrimitives`,
`truncatedTriangles` (over the largest table the GPU holds: 4096 x its texture limit), `morphedPrimitives`,
`buildMilliseconds`, and `materials[]`: `name`, `triangles`, `surfaceSquareMetres`, `referenceUvPerSquareMetre`,
`texture` (the slot), `uvSet`, `textureWidth`, `textureHeight`, `pixelsPerMetre`, largest surface first.
