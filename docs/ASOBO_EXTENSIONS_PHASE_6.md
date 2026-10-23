# Phase A6 — Scene Objects and Animation

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A0.1, A0.2. Independent of the shading phases. **Goal:** make node-, mesh- and animation-level ASOBO
data visible and, where it drives motion, working.

## Tasks

- [ ] **A6.1 — `ASOBO_collision_object`, `ASOBO_gizmo_object`, `ASOBO_tags` (R1).** Draw the box, cylinder and
  sphere shapes at their node transform plus their local translation and rotation, as overlays like the
  bounding-box renderer.
  - Colored by type and tag.
  - Off by default (journal open question 2), switched per viewport.
  - Selectable in the hierarchy and inspector.
  - Tests: each shape and parameter on a fixture, including transforms under an animated parent.
- [ ] **A6.2 — `ASOBO_macro_light` (R3).** Show light gizmos (position, cone, color). Optionally act as punctual
  lights, using the existing `KHR_lights_punctual` path, with flashing (frequency, duration, phase) and rotation
  driven by the animation clock.
  - Day/night cycle follows the A1.6 setting.
  - Captures with `--anim-time` are deterministic.
- [ ] **A6.3 — `ASOBO_property_animation` (R3).** Animate the properties its channels target. Map the channels
  onto the existing `KHR_animation_pointer` support (`header/imgui/AnimationPlayer`, `KHRAnimationPointerTarget`).
  - Confirm the target path syntax with the exporter first.
  - The 207 project files contain no channels, so a fixture is required.
  - Tests: an animated material factor matches the equivalent `KHR_animation_pointer` animation frame by frame.
- [ ] **A6.4 — `ASOBO_animation_retargeting` and `ASOBO_unique_id` (R1).** Show the ids in the inspector and make
  the hierarchy filter search them. Retargeting between files is a feature decision for later; the id is
  metadata in a single-file viewer.
- [ ] **A6.5 — `ASOBO_softbody_mesh` (R1).** Inspector metadata (type `balloon`); recorded as *Metadata* in the
  support matrix.

**Exit gate:**
- Overlays are correct under hierarchy animation.
- Property animation matches its `KHR_animation_pointer` equivalent.
- With overlays off, output is byte-identical to the baseline.
