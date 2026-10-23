# Phase A2 — Deferred Opaque Path and Decals

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A1.2, A1.4, and the A0.6 notes for decals. **Goal:** decals composite the way MSFS composites them.
`ASOBO_material_geometry_decal` is used by 144 of 207 project files: 7 cockpit materials and 4 exterior materials.

## Why deferred

A decal in MSFS replaces selected channels of the surface below by its blend factors: base color, metallic,
roughness, normal, emissive, occlusion. Lighting is computed afterwards.
- **Glider today:** a forward renderer. Every object is lit completely, and only the final color is stored.
- **What a forward decal can do:** blend its own lit color, so color decals (placards, text) can be emulated.
- **What it cannot do:** read the material underneath. That rules out decals that change only normal, roughness,
  metallic or occlusion: panel lines, rivets, grime. The E195 decal sheets combine small color factors (0.1)
  with large normal and occlusion factors (0.5).

The user chose a deferred path for opaque geometry, used only for files that declare ASOBO extensions. Every
other file keeps the forward path and stays byte-identical.

## Tasks

- [ ] **A2.1 — G-buffer and geometry pass (R4). Owner: Claude.** Per-viewport material targets, about four, for
  example:
  - base color and alpha;
  - packed normal;
  - occlusion/roughness/metallic;
  - emissive;
  - plus depth.

  A geometry variant of `pbr.frag` writes surface properties instead of lit color, reusing `material_info.glsl`
  and the texture code. OpenGL 4.1 multiple render targets are enough.
  - **Tests:**
    - each target against a forward debug view of the same channel;
    - resize and close/reopen of viewports;
    - memory accounting.
- [ ] **A2.2 — Lighting pass (R4). Owner: Claude.** A full-screen pass that reuses the BRDF, IBL and
  punctual-light code to light the G-buffer.
  - Transparent, transmissive and glass materials stay forward, drawn afterwards over the lit image with the same
    depth.
  - **Tests:** an opaque-only fixture lit deferred matches the forward render within a declared tolerance; the
    only expected differences are precision from target formats.
- [ ] **A2.3 — Anti-aliasing and the rest of the pipeline (R3). Owner: Claude.**
  - **TAA and FXAA** with the deferred path.
  - **MSAA:** expensive with a G-buffer, so it is either forward-only for ASOBO files or offered as a costly
    option; decided with measurements.
  - **Other pipeline parts:** picking (unchanged; it has its own pass), wireframe and debug modes, the
    transmission copy, the redraw-skip and caching logic from Phases 4–8.
  - **Tests:** the existing P8 suites on ASOBO and non-ASOBO files.
- [ ] **A2.4 — `ASOBO_material_geometry_decal` (R4). Owner: Claude.** Decals blend into the G-buffer.
  - Per-target blend factors (`glBlendFunci` and constant factors, or shader-written alpha per target) for:
    - base color, metallic, roughness, normal, emissive and occlusion;
    - `mode`;
    - `decalChan0–2`, interpreted for a viewer: SimObject on, scenery and terrain not applicable.
  - Ordering by A1.4.
  - **Tests:**
    - factor 0 leaves a channel unchanged;
    - factor 1 replaces only that channel;
    - overlapping decals follow draw order;
    - the E195 decal sheets match the reference screenshots.
- [ ] **A2.5 — `ASOBO_material_blend_gbuffer` (R3). Owner: Claude.** The same channel blending for non-decal
  geometry.
- [ ] **A2.6 — `ASOBO_material_environment_occluder`, `ASOBO_material_fake_terrain` (R1). Owner: Gemini Argon
  (classification), Claude (code if any).** Confirm their meaning, then record *Not applicable* or a viewer
  behavior.

**Exit gate:**
- Files without ASOBO extensions are byte-identical.
- The deferred-versus-forward tolerance is met on opaque-only fixtures.
- Decal tests pass.
- E195 is reviewed against the reference screenshots.
- Memory and frame cost per viewport are recorded on the three-viewport setup.
