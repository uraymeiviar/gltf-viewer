# Phase A0 — Foundations

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress. A0.1, A0.3 and A0.6 done (uncommitted); A0.2, A0.4 and A0.5 next; MSFS screenshots pending.

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
- [ ] **A0.2 — Support matrix and inspector badges (R1). Owner: GPT Astra.** One table in code lists every
  extension with its status: *Rendered*, *Partial*, *Viewer setting*, *Metadata*, *Not applicable*,
  *Not yet supported*. It drives:
  - the inspector badge next to each extension;
  - an info-level validation message listing declared extensions that Glider does not render.

  Include `ASOBO_unique_id`, `ASOBO_asset_optimized` and `MSFT_texture_dds`.
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
  - **Note (2026-10-24):** real-asset renders need their textures, which come from the texture search paths in
    `settings.ini`; `--isolated` ignores it. Add a `--search-path <dir>` flag so harness renders stay isolated
    and textured.
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
