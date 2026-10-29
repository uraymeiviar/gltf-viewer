# Phase A0 — Foundations

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Tasks done (A0.1, A0.3, A0.6 in `b137320`; A0.2, A0.4, A0.5 uncommitted). Open: the MSFS screenshots and re-framing view 08.

**Entry:** none. **Goal:** every later phase has the following in place:
- extension data parsed into typed values with schema defaults;
- one place that records how each extension is supported;
- written semantics for each extension;
- fixtures and a harness template;
- a list of reference views for MSFS screenshots.

## Tasks

- [x] **A0.1 — Typed extension model (R2). Owner: Claude.** Parse the declared ASOBO extensions once at load into
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
  - **Done (2026-10-24, uncommitted):**
    - **Finding:** the schemas give a default for only 21 of their 133 properties (for example `drawOrderOffset`,
      `strength` and the emissive multipliers have none). The defaults come from the exporter (A0.6), added to the
      generator's `EXPORTER_DEFAULTS`; a property neither source defines stays unset unless the file sets it.
    - **Exporter versus schema (from A0.6, checked in the exporter source):**
      - The exporter writes `ASOBO_extra_occlusion` / `extraOcclusionTexture` (with `texCoord` 1), not the
        schema's `ASOBO_extra_occlusion_texture` / `extra_occlusion_texture`; both are modelled.
      - It spells `UVOffsetU/V`; the schema `UVOffSetU/V`. The property uses the exporter's name and accepts the
        schema's as an alias.
      - `clampUVX/Y`: the schema default is true, but Blender's is false and the exporter never writes false, so
        a missing key means false; the exporter default wins. The E195 cockpit tiles a material 2x without the
        key, which clamping would stretch.
      - Keys the exporter writes outside the schema are typed with its defaults: geometry decal `mode`,
        `decalChan0-2`, `normalOverrideFactor`, `blendSharpnessFactor`, `underClearcoat`, `blendMaskTexture`;
        windshield v3 `UVScale`, `ssrAttenuation`, `cubemapReflectionMasking`; parallax window `corridor`.
      - Ranges remain the schema's; the exporter's Blender UI limits are not enforced.
    - **Design:** a schema-driven table instead of one hand-written struct per extension.
      `scripts/generate_asobo_specs.py` generates `Tools/tools/gltf/source/extensions/AsoboExtensionSpecs.cpp`
      from the schemas (39 extensions, names from the file names because some titles are wrong), with a
      scope per extension and an `EXPORTER_DEFAULTS` table for A0.6; `--check` fails when the table drifts from
      the schemas. `gltf/extensions/AsoboExtensions.h` parses an object's extensions into typed values
      (boolean, number, string, number array, texture reference, JSON), with typed accessors, explicit-versus-
      default tracking and the unknown keys. Later phases read values through it.
    - **Rules as implemented:** `gltf::Gltf` now keeps every declared `extensionsUsed` name; undeclared ASOBO
      extensions are ignored (info `ASOBO_EXTENSION_UNDECLARED`). Out-of-range numbers are clamped
      (`ASOBO_VALUE_RANGE`), wrong types fall back to the default (`ASOBO_VALUE_TYPE`), an extension on the
      wrong kind of object is reported (`ASOBO_EXTENSION_SCOPE`), and an ASOBO extension without a schema is
      kept as metadata (`ASOBO_EXTENSION_NO_SCHEMA`). These appear in the Validation panel. A non-object
      extension value never reaches the parser: the glTF loader drops it and the schema validation reports it.
    - **Renderer:** `renderer::Material::asobo` holds the parsed set (shared, so the transmission path's
      per-frame material copies stay cheap); it is rebuilt on every renderer sync of the material, so reloads
      and edits refresh it. `ASOBO_material_invisible` now reads it, and is ignored when undeclared.
    - **Evidence:** `scripts/validate_asobo_model.py` (in the regression quick tier) reads the new
      `--asobo-report <out.json>` output for fixtures covering each rule and a `--reload-model`, and parses the E195
      cockpit and exterior: 2,446 and 312 declared extension instances, all parsed, with no warnings.
- [x] **A0.2 — Support matrix and inspector badges (R1). Owner: GPT Astra.** One table in code lists every
  extension with its status: *Rendered*, *Partial*, *Viewer setting*, *Metadata*, *Not applicable*,
  *Not yet supported*. It drives:
  - the inspector badge next to each extension;
  - an info-level validation message listing declared extensions that Glider does not render.

  Include `ASOBO_unique_id`, `ASOBO_asset_optimized` and `MSFT_texture_dds`.
  - **Done (2026-10-24):** `gltf/extensions/AsoboSupport.h` (`supportTable()`, `supportOf()`, `supportLabel()`; 41
    entries with their phase). Today: *Rendered* `ASOBO_material_invisible`; *Metadata* `unique_id`, `tags`,
    `asset_optimized`, `animation_retargeting`, `softbody_mesh`; *Not applicable* `disable_motion_blur`; the rest
    *Not yet supported*. Badges sit on each extension header's line (rows unchanged); the validator adds one
    `ASOBO_EXTENSIONS_NOT_RENDERED` info listing the declared extensions not rendered; `--asobo-report` includes
    the table. **Evidence:** `scripts/validate_asobo_support.py` (quick tier).
  - **Handoff:** GPT Astra (Codex, medium effort) in its own worktree, 6.5 min, 1.59M tokens (1.50M cached).
    Its sandbox blocked MSBuild (access to `AppData\Local\Microsoft SDKs`), so it could not build; it said so.
    Claude built and fixed it: `--validate-file` validated against an empty model, so every model-based check,
    including all ASOBO diagnostics, saw nothing there; it now loads the file first (and falls back to the empty
    model when the loader rejects it). Claude also updated a stale note, spaced the badge, and made the A0.4
    viewport switches come from the table (one per *Rendered* or *Partial* extension).
  - **Follow-up:** the extension property editor (`ExtensionInspector`) shows a property the file omits as 0
    from its own schema reader, while the model uses the exporter default (for example 1.0 for the decal blend
    factors). It should take its defaults from the A0.1 spec table.
- [x] **A0.3 — Reference views (R0). Owner: Gemini Argon (list), user's team (screenshots).** Produce:
  - A list of E195 cockpit and exterior views covering normals, ORM, emissive, decals, glass, windshield and
    shadows.
  - For each view: the Glider camera flags (`--camera-yaw/pitch/distance/target/fov`), and the MSFS camera,
    time of day and weather to use.

  The team captures the MSFS 2024 screenshots. Store them outside the repository; record their path in this file.
  - **Done (2026-10-24):** [`ASOBO_REFERENCE_VIEWS.md`](ASOBO_REFERENCE_VIEWS.md): 11 views (8 cockpit, 3 exterior)
    with copy-pasteable flags, MSFS camera, time and aircraft state, renders in `build/views/`, a gap list (wing
    decals, wing-on-fuselage shadow, landing gear, tail livery) and capture instructions. Review: view 01's flags
    reproduce its render exactly; two wrong extension names were corrected (`ASOBO_material_blend_additive`
    does not exist; the screens use core emissive and `glass_v2`); view 08 is framed inside geometry and must be
    re-framed. Gemini 3.8 Flash High (Argon was unavailable: wrong agy account), three runs because two
    multi-line `git grep` calls were denied: 2.5M tokens, 74.8M cache reads.
  - **Screenshots:** *path to be recorded when the team has captured them.*
- [x] **A0.4 — Extension switches (R1). Owner: Claude.** Now File > Settings > glTF Extensions (app setting, all viewports) (first built per viewport under Viewport Settings → *MSFS Extensions*): a
  master switch, plus one switch per rendered extension, all on by default. *Show Invisible Materials* moves
  here.
  - **Command line:** `--asobo off|on`, `--asobo-disable <name>`.
  - **Test:** with everything off, output is byte-identical to the build before this journal.
  - **Done (2026-10-24):** `renderer::AsoboOptions` (master switch plus a sorted list of disabled extensions)
    replaces `GltfRenderLayer::showInvisibleMaterials`; each viewport's `ViewportSettingsState::asobo` is applied
    before its scene is built, and the shared traversal compares it. The panel shows *Apply MSFS (ASOBO)
    Extensions* (kept on the row `validate_p4_exit_gate.py` clicks) and one switch per rendered extension
    (today *Hide Invisible Materials*; generated from the A0.2 support table once that lands).
    `--show-invisible-materials` and the UI-script key `invisible` now disable `ASOBO_material_invisible`; the
    key `asobo` sets the master switch. **Evidence:** `validate_asobo_common.py`: with `--asobo off`, and with
    only `--asobo-disable ASOBO_material_invisible`, the E195 cockpit and exterior reference views are
    byte-identical to `167a5e0` (the last build before this journal) run with `--show-invisible-materials`.
- [x] **A0.5 — Harness template (R0). Owner: Claude.** `scripts/validate_asobo_common.py` provides:
  - byte identity for files without the extension;
  - the "everything off" equivalence;
  - schema-default fixtures;
  - the real-asset list;
  - render helpers that apply the A0.3 camera flags.

  Wired into `scripts/run_regression.py`.
  - **Done (2026-10-24):** `scripts/validate_asobo_common.py`, in the regression quick tier. Later phases import
    `views()` (parsed from the A0.3 table), `render()` / `render_view()` (isolated viewport captures with a
    view's camera flags, textured through the new repeatable `--search-path <dir>`), `fixture()` (extensions
    injected into a geometry fixture) and `same()` / `check()` / `finish()`. Run alone it checks A0.4 and that
    `--search-path` gives a textured isolated render (0.0% missing-texture magenta). Binaries older than
    `--search-path` read it as a model path, so comparisons with them render untextured
    (`render_view(..., textured=False)`).
- [x] **A0.6 — Specification extraction (R0). Owner: DeepSeek.** For every extension in phases A1–A5, read
  the SDK Blender exporter (journal §1) and write the exact semantics into the reference doc:
  - formulas and blend operators;
  - which textures and UV sets are affected;
  - defaults the exporter writes;
  - keys outside the schema.

  Priority order: normal convention, occlusion strength / extra occlusion, emissive, draw order, geometry decal,
  glass v2, windshield v3, detail map, UV options, parallax window. Mark each statement with the source file and
  line.
  - **Done (2026-10-24):** `docs/ASOBO_EXTENSIONS_SEMANTICS.md`, from exporter 8.4.1 (SDK 1.8.1): all 18 A1, A2,
    A4 and A5 extensions, every statement marked verified or inferred with a source line, 74 open questions.
    Four are complete; the other 14 are partial because the exporter only writes values: the shading of glass,
    windshield, Fresnel fade, ghost, parallax interiors and the decal blend factors is implemented in the MSFS
    engine, not in the exporter, so those need the MSFS screenshots (A0.3). Claude checked the three findings
    that changed A0.1 against the exporter source. DeepSeek-V4.1-Flash (Max): 16 min, 266k input + 44.7M cache
    reads + 150k output tokens.

**Exit gate:**
- A0.1 parses every schema with correct defaults.
- With everything switched off, the output is byte-identical to the baseline.
- The A0.6 notes cover A1 and A2.
- The reference view list exists.
