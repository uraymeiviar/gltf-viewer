# Phase A3 — Sun, Shadows, Macro Lights, SSS

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A3.4 done (ahead of A3.1-A3.3).

**Entry:** A2.2, the deferred lighting pass. **Goal:** shadows from a sun (user priority 4), and the remaining
lighting features of user priority 5. The sun's direction is set by hand here and driven by time of day in A6.

## Tasks

- [ ] **A3.1 — Sun light (R3). Owner: Claude.** A directional light with direction (azimuth and elevation, set by
  hand), color and intensity, in Viewport Settings → *Sun*.
  - **Intensity and the environment map:** the HDR environment already contains a sun, so the sun's intensity and
    the IBL are balanced to avoid lighting it twice. Until A6 provides a matching sky, a documented rule is used:
    for example, dimming the IBL's direct-sun contribution, or a sun-free default HDR.
  - **Tests:** a Lambert sphere lit from a known direction.
- [ ] **A3.2 — Cascaded shadow maps (R4). Owner: Claude.** Shadows from the sun: cascades fitted to each
  viewport's frustum, and filtering (PCF), applied in the deferred lighting pass.
  - **`ASOBO_material_shadow_options`:** materials with `noCastShadow` are excluded from the shadow pass.
  - **Transparent and glass:** cast or not according to the A0.6 notes.
  - **Quality settings:** resolution, cascade count and filtering are user settings, as in Phase 7.
  - **Tests:**
    - a known occluder and receiver give a shadow edge at the expected position;
    - no acne or peter-panning on fixtures at grazing sun angles;
    - `noCastShadow` objects cast nothing;
    - stable under camera motion and TAA.
  - **Cost:** measured on the three-viewport setup. The shadow map is shared between viewports when the sun is
    the same.
- [ ] **A3.3 — `ASOBO_macro_light` (R3). Owner: Claude.** Light gizmos, and a real light contribution through the
  punctual-light path.
  - Animated by the animation clock: flashing (frequency, duration, phase) and rotation.
  - Day/night behavior follows the time-of-day setting.
  - Captures are deterministic with `--anim-time`.
- [x] **A3.4 — `ASOBO_material_SSS` (R4). Owner: Claude.** Subsurface-scattering approximation: wrap lighting and
  translucency tinted by `SSSColor`, with thickness from `opacityTexture`, per the A0.6 notes. Uses the sun and
  shadow data.
  - **Done 2026-10-28 (before A3.1/A3.2), support status Partial.** Semantics in
    [section 18](ASOBO_EXTENSIONS_SEMANTICS.md): `SSSColor` is RGBA in every exporter, file and SDK example (the
    schema's 2 items is wrong), default white; `opacityTexture` is "not currently used" per the SDK and absent from
    all 389 surveyed SSS materials.
  - **Shading** (`resources/shaders/asobo_sss.glsl`, OPAQUE/MASK only): direct lights get a wrapped diffuse
    `clamp((N.L + w) / (1 + w))`, `w = 0.5 SSSColor` per channel, plus back-lit translucency
    `0.5 SSSColor (1 - opacityTexture.r) clamp(-N.L)` with an N.V Fresnel; the IBL adds the same weight x
    `c_diff` x irradiance(-N) before AO. `front + back <= 1`. Black `SSSColor` adds exactly zero. No blur pass.
  - **Sun hook:** the direct-light term is `asoboSssDirectDiffuse(intensity, N.L, ...)`; A3.1's sun calls it like
    the punctual-light loop does. Shadows (A3.2) should attenuate only the front (wrapped) part; the transmitted
    part needs the shadow of the light's side, a decision for A3.2.
  - **Typed model:** `SSSColor` accepts 3 or 4 items (default `[1, 1, 1, 1]`); 2 items warn (`ASOBO_VALUE_RANGE`)
    and use white (`scripts/generate_asobo_specs.py` `ITEM_COUNTS`).
  - **Check:** `scripts/validate_asobo_sss.py` (quick regression, serial ASOBO list): black = core byte-identical
    (6 lighting setups); back-lit quad transmits 0.498 of the front-lit core response; light side unchanged; front-lit
    quad 0 levels; opacity white/black/grey/missing; side-lit wrap and orange tint; bounded by the front-lit
    maximum; IBL from behind; dither kept pixels exact; decal receiver; switches; baseline byte-identical.
  - **Review** (`build/review/sss/contact_sheet.png`, default IBL only, no punctual lights in these files): PMDG
    777 seat fur shells change in 4.6% of the frame (max 20 levels), gear handle knob 0.3% (max 25), MSFS 2020
    marshaller face/arms 4-7% (max 24, warmer skin on the sky-lit side), windsock fabric (orange `SSSColor`) 2.1%
    (max 65). With IBL only the effect is a mild glow on thin/convex parts; the wrap needs direct light (A3.1).

**Exit gate:**
- Shadow tests pass.
- `noCastShadow` is honored.
- Exterior and cockpit views are reviewed against the reference screenshots at the same sun direction.
- Cost and quality presets are recorded.
