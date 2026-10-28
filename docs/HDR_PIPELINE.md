# HDR Rendering Pipeline (`docs/HDR_PIPELINE.md`)

This document describes Glider's linear HDR rendering pipeline, physically based bloom, and auto-exposure architecture.

---

## 1. Per-Viewport Frame Order

Each open viewport in `Application::drawViewports` (`source/imgui/application/Viewports.cpp`) executes the following pipeline:

1. **Linear HDR Forward Scene (`vp->sceneFbo`, `GL_RGBA16F`, MSAA Off/2x/4x/8x)**:
   - All PBR materials (`pbr.frag`) and the background cubemap (`cubemap.frag`) compile with `LINEAR_OUTPUT=1` (`GltfResourceManager.cpp`, `GltfRenderLayer.cpp`).
   - Pass execution order (`RenderPassDefines.h`):
     1. `DecalPrepass` (depth `GL_DEPTH_COMPONENT32F` + geometry normal `G0`)
     2. `DecalAccumulation` (`ASOBO_material_geometry_decal` / `ASOBO_material_blend_gbuffer`)
     3. `Opaque` (skybox + opaque and `MASK` meshes)
     4. `Grid`
     5. `Transparent2` (non-glass `BLEND` draws when `asoboGlassCopy && !windshieldSsr`, so they appear in the refraction copy without duplicating drawables)
     6. `TransmissiveBlit` (captures the linear HDR `vp->sceneFbo` into `vp->transmissionTarget` (`GL_RGBA16F`) and regenerates its mip chain for rough refraction and SSR)
     7. `Transmissive` (`KHR_materials_transmission` and `ASOBO_material_glass_v2`)
     8. `Transparent` (`ASOBO_material_windshield_v3` and standard `BLEND` draws, blended in linear HDR space)
2. **MSAA Resolve (`Framebuffer::unbind`, `source/imgui/Framebuffer.cpp`)**:
   - Resolves multisampled `GL_RGBA16F` color and `GL_DEPTH24_STENCIL8` depth to single-sample textures before post-processing.
3. **HDR Temporal Anti-Aliasing (`AntiAliasing::render`, `source/imgui/AntiAliasing.cpp`)**:
   - Ping-pongs two `GL_RGBA16F` history buffers (`m_taaTexture[0..1]`).
   - Applies reversible Karis luminance weighting before the $3 \times 3$ neighborhood clamp and temporal blend, and inverts it afterwards so subpixel specular/emissive highlights never ghost or flicker:
     $$w(c) = \frac{c}{1 + \text{luma}(c)}, \qquad w^{-1}(c') = \frac{c'}{\max(1 - \text{luma}(c'),\, 10^{-4})}$$
4. **Auto-Exposure Metering (`AntiAliasing::updateAutoExposure`, `source/imgui/AntiAliasing.cpp`)**:
   - Downsamples the resolved HDR image into a $64 \times 64$ `GL_R32F` luminance target (`s_meterFS`).
   - Computes a centre-weighted ($\exp(-4(x^2 + y^2))$) log-luminance distribution and rejects the darkest $50\%$ and brightest $2\%$ of total weight:
     $$\log_2(L_{\text{avg}}) = \frac{\sum_{q \in [0.50, 0.98]} w_i \log_2(\max(L_i, 10^{-4}))}{\sum_{q \in [0.50, 0.98]} w_i}$$
   - Target exposure value (mapping $L_{\text{avg}}$ to middle grey $0.18$):
     $$\text{EV}_{\text{target}} = \text{clamp}\!\left(\text{EV}_{\text{comp}} - \log_2\!\left(\frac{L_{\text{avg}}}{0.18}\right),\, \text{EV}_{\min},\, \text{EV}_{\max}\right)$$
   - Adapts exponentially over frame delta time $\Delta t$ with separate speeds (`speedUp = 3.0 /s`, `speedDown = 1.5 /s`), snapping immediately on the first frame, model load, or camera jumps:
     $$\text{EV}(t + \Delta t) = \text{EV}(t) + (\text{EV}_{\text{target}} - \text{EV}(t))\left(1 - e^{-k \Delta t}\right)$$
5. **Physically Based Bloom (`AntiAliasing::renderBloom`, `source/imgui/AntiAliasing.cpp`)**:
   - Implements Jimenez (SIGGRAPH 2014, *Call of Duty: Advanced Warfare*) across a 5-level `GL_RGBA16F` canonical mip pyramid (`heights = 225, 112, 56, 28, 14`, widths scaled by viewport aspect ratio) so halo size in scene/FOV space is invariant across viewport resolutions (`800×450`, `1600×900`, `4K`):
     - **Downsample (`s_bloomDownsampleFS`)**: 13-tap bilinear filter (5 overlapping $2 \times 2$ box groups with weights $0.5, 0.125, 0.125, 0.125, 0.125$). The first pass applies a Karis weighted average against the second-brightest corner box to suppress isolated subpixel fireflies while preserving full unclamped HDR radiance on coherent emissive surfaces (`KHR_materials_emissive_strength` up to 1000).
     - **Upsample (`s_bloomUpsampleFS`)**: $3 \times 3$ tent filter ($\frac{1}{16}[1,2,1; 2,4,2; 1,2,1]$ scaled by `bloomRadius`) accumulated additively (`GL_ONE, GL_ONE`) from mip 4 up to mip 0. The chain therefore holds the sum of the five levels; the composite divides it by 5 (energy-conserving average), otherwise the intensity mix veils the whole frame.
6. **Post-Process Tone Mapping (`AntiAliasing::renderPostProcess`, `s_tonemapFS`)**:
   - Composites bloom in linear HDR space before tone mapping:
     $$C_{\text{hdr}} = \text{mix}(C_{\text{scene}},\, C_{\text{bloom}},\, \text{bloomIntensity})$$
   - Applies linear exposure $2^{\text{EV}}$, the selected tone mapper (**Khronos PBR Neutral**, **ACES Hill**, or **ACES Narkowicz**), and linear-to-sRGB ($\gamma = 1/2.2$) into `vp->overlayFbo` (`GL_RGBA8`).
   - Debug Render Modes (`--debug-view`) bypass bloom and tone mapping (`u_RawMode = 1`). The overlay debug modes (`ssr` and `ssrray`) use `u_RawMode = 2`: windshield diagnostic pixels carry an alpha tag (>1.5) to bypass tone mapping and bloom as raw values, while the shaded surroundings and background are tone mapped normally.
7. **Post-Tone-Map FXAA & Overlays (`Viewports.cpp`)**:
   - When Post AA is `FXAA`, runs FXAA on the LDR `vp->overlayFbo` and blits back.
   - Renders unjittered overlays (ground grid, wireframe, scene/node bounding boxes, coordinate gizmo) into `vp->overlayFbo` and resolves to the display texture for Dear ImGui. Both Wireframe shading mode and the Wireframe Overlay draw lines into `vp->overlayFbo` after tone mapping and FXAA, using unjittered projection:
     - They never feed into bloom, auto-exposure metering, or TAA history.
     - Depth testing against the resolved scene depth keeps hidden lines occluded.
     - Blend modes: Normal (alpha), Additive, Invert (mix(dst, 1-dst, alpha)), and XOR (logic op). Destination alpha is protected via glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE).

---

## 2. Defaults & CLI Flags

| Setting | Default | UI Location | CLI Flag |
|---|---|---|---|
| Exposure Mode | `Manual` | Scene Inspector → Environment | `--auto-exposure on\|off` |
| Manual Exposure | `0.0 EV` | Scene Inspector → Environment | `--exposure <ev>` |
| EV Compensation | `0.0 EV` | Scene Inspector → Environment | `--exposure-compensation <ev>` |
| Auto-Exposure Min / Max | `-6.0 / +6.0 EV` | Scene Inspector → Environment | — |
| Adapt Speed Up / Down | `3.0 / 1.5 /s` | Scene Inspector → Environment | — |
| Tone Mapping | `ACES Hill` | Scene Inspector → Environment | `--tone-mapping <neutral\|hill\|narkowicz>` |
| Bloom Enabled | `On` (`true`) | Scene Inspector → Environment | `--bloom on\|off` |
| Bloom Intensity | `0.04` | Scene Inspector → Environment | `--bloom-intensity <x>` |
| Bloom Radius | `1.0` | Scene Inspector → Environment | `--bloom-radius <x>` |
| MSFS Emissive Scale | `0.001` (1/1000) | Scene Inspector → Environment | `--msfs-emissive-scale <x>` |
| Emissive Day/Night | `Day` (`0`) | Scene Inspector → Environment | `--time-of-day day\|night\|auto` (the A6.1 clock is `--time-of-day-mode`) |

---

## 3. MSFS Emissive Brightness & Day/Night Scaling

MSFS glTF assets write `emissiveFactor = emissiveColor × emissiveScale` (exporter scale typically 1000, UI up to 200,000) and omit `KHR_materials_emissive_strength`. To preserve authored dynamic range and bloom cockpit displays without washing out to flat white:

1. **Unclamped authored factor**: The glTF loader stores the raw authored `emissiveFactor` (clamping only negative values to 0.0). Core glTF assets with factors in $[0, 1]$ remain byte-identical to baseline.
2. **MSFS emissive unit formula**: For assets declaring ASOBO extensions (`extensionsUsed` containing `ASOBO_*`) in viewports with ASOBO enabled:
   $$E = \text{emissiveFactor} \times \text{emissiveTexture} \times \text{msfsEmissiveScale} \times M_{\text{tod}}$$
   where:
   $$M_{\text{tod}} = \begin{cases} \text{emissiveNightMultiplier} & \text{if Time of Day is Night and } \text{ASOBO\_material\_emissive is applied} \\ \text{emissiveDayMultiplier} & \text{if Time of Day is Day and } \text{ASOBO\_material\_emissive is applied} \\ 1.0 & \text{otherwise} \end{cases}$$
3. **Decal emissive**: Decal accumulation (`decal_accum.frag`) applies the identical formula to `u_EmissiveFactor` before weighting by `wEmissive`.
4. **ASOBO disabled fallback**: If ASOBO extensions are undeclared or the viewport sets `--asobo off`, `emissiveFactor` is clamped to $[0, 1]$ and `msfsEmissiveScale` is bypassed, preserving standard glTF 2.0 behaviour and A/B comparison fidelity.
5. **KHR Extension Coexistence**: `KHR_materials_emissive_strength` continues to multiply emissive radiance for assets using the Khronos extension.
