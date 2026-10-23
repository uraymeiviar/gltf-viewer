# Phase A3 — Sun, Shadows, Macro Lights, SSS

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

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
- [ ] **A3.4 — `ASOBO_material_SSS` (R4). Owner: Claude.** Subsurface-scattering approximation: wrap lighting and
  translucency tinted by `SSSColor`, with thickness from `opacityTexture`, per the A0.6 notes. Uses the sun and
  shadow data.

**Exit gate:**
- Shadow tests pass.
- `noCastShadow` is honored.
- Exterior and cockpit views are reviewed against the reference screenshots at the same sun direction.
- Cost and quality presets are recorded.
