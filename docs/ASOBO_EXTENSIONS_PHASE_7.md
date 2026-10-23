# Phase A7 — Simulator-Only Features

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A0.2. **Goal:** decide, for every extension that depends on a simulator system Glider does not have,
whether it maps to a viewer feature, becomes a viewer setting, or is recorded as not applicable, and make that
decision visible in the support matrix.

## Tasks

- [ ] **A7.1 — `ASOBO_material_shadow_options` (R0 now; R3 if shadows are added).** Glider has no shadow maps, so
  `noCastShadow` is *Not applicable*. It is used by 138 project files. If shadow maps are ever added, as a
  separate renderer feature, this extension excludes the material from shadow casting.
- [ ] **A7.2 — `ASOBO_material_disable_motion_blur` (R0).** Glider has no motion blur: *Not applicable*.
- [ ] **A7.3 — `ASOBO_material_antialiasing_options` (R3).** `responsiveAA` applies to Glider's TAA. Materials with
  it get a reduced history weight (a responsiveness mask written during the scene pass and read by the TAA
  resolve).
  - Tests:
    - a moving responsive object leaves less trailing than a non-responsive one;
    - static results are unchanged.
- [ ] **A7.4 — `ASOBO_material_rain_options` (R1).** No weather in Glider. *Not applicable* by default; a later
  optional *Wet surface* viewer setting could use `rainDropScale` and `rainDropSide`, decided with the user.

**Exit gate:** every A7 extension has a recorded decision, shown in the support matrix and inspector badge, with
the reasoning in this file.
