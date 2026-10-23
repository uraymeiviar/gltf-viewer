# Phase A0 — Foundations

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** none. **Goal:** every later phase reads typed, schema-defaulted extension data, reports its status in
one place, and has fixtures, harness conventions and ground truth to test against.

## Tasks

- [ ] **A0.1 — Typed extension model (R2).** Parse the declared ASOBO extensions once at load into typed structs
  with schema defaults, attached to the renderer objects that need them (asset, material, node, mesh, animation).
  Today they are kept as JSON strings in `GltfObject::m_unknownExtensions`, and the one rendered extension
  (`ASOBO_material_invisible`) parses its string in `GltfRenderLayer`.
  - Extensions that are present but not listed in `extensionsUsed` are ignored.
  - Unknown keys are kept (for example `decalChan0–2`, `mode`).
  - Values out of the schema's range are clamped and reported as validation warnings.
  - Material reload (`updateRendererobjectFromGltf`) and file reload rebuild the parsed values.
  - Move `ASOBO_material_invisible` onto the model. Tests: parsing of every schema in `resources/schemas/asobo`
    with defaults, an undeclared extension, malformed values and unknown keys; reload.
- [ ] **A0.2 — Support matrix (R1).** One table in code listing every extension with its status: *Rendered*,
  *Partial*, *Viewer setting*, *Metadata*, *Not applicable*, *Not yet supported*.
  - It drives an inspector badge next to each extension.
  - It drives an info-level validation message listing the declared extensions that Glider does not render.
  - It drives the reference doc's *Current* column.
  - Non-schema extensions seen in real data (`ASOBO_unique_id`, `ASOBO_asset_optimized`, `MSFT_texture_dds`)
    are included.
- [ ] **A0.3 — Fixtures and ground truth (R0).**
  - A shared generator library for ASOBO fixtures, `scripts/asobo_fixture_lib.py` (a sphere or quad grid, one
    material per property combination, textures written as small PNGs).
  - A regression list of real-asset views: E195 cockpit and exterior, using absolute-path copies as in
    `validate_p8_followup.py`.
  - A request to the user for MSFS 2024 reference screenshots of those views.
- [ ] **A0.4 — Per-viewport extension switches (R1).** Viewport Settings → *MSFS Extensions*: a master switch plus
  one switch per rendered extension, all on by default. *Show Invisible Materials* moves here.
  - Command-line equivalents for tests: `--asobo off|on`, `--asobo-disable <name>`.
  - With everything off, every asset renders as plain core glTF, byte-identical to a build without ASOBO support.
- [ ] **A0.5 — Harness template (R0).** `scripts/validate_asobo_common.py` with the shared checks from journal
  §2.1:
  - byte identity for assets without the extension;
  - an "all off" equivalence;
  - schema-default fixtures;
  - the real-asset regression list.

**Exit gate:**
- A0.1 parses every schema with correct defaults.
- Turning everything off is byte-identical to the baseline.
- The support matrix shows every declared extension in the E195 cockpit and exterior.
