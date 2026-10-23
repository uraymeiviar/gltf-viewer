# Phase A0 — Foundations

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** none. **Goal:** every later phase has the following in place:
- extension data parsed into typed values with schema defaults;
- one place that records how each extension is supported;
- written semantics for each extension;
- fixtures and a harness template;
- a list of reference views for MSFS screenshots.

## Tasks

- [ ] **A0.1 — Typed extension model (R2). Owner: Claude.** Parse the declared ASOBO extensions once at load into
  typed structs with schema defaults. Attach them to the renderer objects that need them: asset, material, node,
  mesh, animation.
  - **Today:** extensions are kept as JSON strings (`GltfObject::m_unknownExtensions`), and
    `ASOBO_material_invisible` is parsed in `GltfRenderLayer`.
  - **Rules:**
    - extensions not listed in `extensionsUsed` are ignored;
    - unknown keys are kept, for example `decalChan0–2` and `mode`;
    - out-of-range values are clamped and reported as validation warnings;
    - a material reload or file reload rebuilds the values.
  - `ASOBO_material_invisible` moves onto this model.
  - **Tests:** every schema in `resources/schemas/asobo` with its defaults, an undeclared extension, malformed
    values, unknown keys, and reload.
- [ ] **A0.2 — Support matrix and inspector badges (R1). Owner: GPT Astra.** One table in code lists every
  extension with its status: *Rendered*, *Partial*, *Viewer setting*, *Metadata*, *Not applicable*,
  *Not yet supported*. It drives:
  - the inspector badge next to each extension;
  - an info-level validation message listing declared extensions that Glider does not render.

  Include `ASOBO_unique_id`, `ASOBO_asset_optimized` and `MSFT_texture_dds`.
- [ ] **A0.3 — Reference views (R0). Owner: Gemini Argon (list), user's team (screenshots).** Produce:
  - A list of E195 cockpit and exterior views covering normals, ORM, emissive, decals, glass, windshield and
    shadows.
  - For each view: the Glider camera flags (`--camera-yaw/pitch/distance/target/fov`), and the MSFS camera,
    time of day and weather to use.

  The team captures the MSFS 2024 screenshots. Store them outside the repository; record their path in this file.
- [ ] **A0.4 — Per-viewport extension switches (R1). Owner: Claude.** Viewport Settings → *MSFS Extensions*: a
  master switch, plus one switch per rendered extension, all on by default. *Show Invisible Materials* moves
  here.
  - **Command line:** `--asobo off|on`, `--asobo-disable <name>`.
  - **Test:** with everything off, output is byte-identical to the build before this journal.
- [ ] **A0.5 — Harness template (R0). Owner: Claude.** `scripts/validate_asobo_common.py` provides:
  - byte identity for files without the extension;
  - the "everything off" equivalence;
  - schema-default fixtures;
  - the real-asset list;
  - render helpers that apply the A0.3 camera flags.

  Wired into `scripts/run_regression.py`.
- [ ] **A0.6 — Specification extraction (R0). Owner: DeepSeek.** For every extension in phases A1–A5, read
  the SDK Blender exporter (journal §1) and write the exact semantics into the reference doc:
  - formulas and blend operators;
  - which textures and UV sets are affected;
  - defaults the exporter writes;
  - keys outside the schema.

  Priority order: normal convention, occlusion strength / extra occlusion, emissive, draw order, geometry decal,
  glass v2, windshield v3, detail map, UV options, parallax window. Mark each statement with the source file and
  line.

**Exit gate:**
- A0.1 parses every schema with correct defaults.
- With everything switched off, the output is byte-identical to the baseline.
- The A0.6 notes cover A1 and A2.
- The reference view list exists.
