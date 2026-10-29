# Command-Line Arguments & Automation (Glider)

Complete reference moved from AGENTS.md §4.2. Related flags retain their descriptions and defaults together. Start with [AGENTS.md](../AGENTS.md); use [LIVE_SCRIPT_CONTROL.md](LIVE_SCRIPT_CONTROL.md) for script syntax and [AGENT_CONTROL_PLAN.md](AGENT_CONTROL_PLAN.md) for protocol and registry details.

`Glider.exe` supports several CLI flags useful for testing and headless image verification:

## Loading, selection & camera

- `[model.gltf / model.glb]`: Path to glTF or GLB file to open on startup (defaults to embedded `HelloCube.glb`).

- `--without-textures`: Open the startup model (and `--reload-model`, if supplied) with geometry and material parameters, intentionally skipping image reads, decoding, previews, GPU image uploads and generated normal-map tangents. Equivalent to **File → Open without texture ...**. Texture references remain inspectable; quality changes keep them omitted. A normal File → Open / Open Recent loads textures again. This is a per-open choice, not a saved preference. GLB/shared buffers still have to be read for geometry. See `docs/LOADING_SPEED_AND_TEXTURE_FREE_OPEN.md`.

- `--isolate-node <name>`: Isolate a node (without descendants), select it and frame the camera on startup.

- `--select-node <name>`: Automatically selects a node by name and frames camera on startup.

- `--frame-node <name|#index>`: Frame a node on startup using the same bounds and distance logic as **Object -> Focus (`F`)**. Selects the node. Node lookup matches exact, case-insensitive, unique substring, or `#index` (`--select-node`/`--isolate-node` keep exact names). Nodes with empty geometry bounds frame their world position at a default distance without error.

- `--frame-view <front|back|left|right|top|bottom|iso>`: Preset viewing direction for `--frame-node`.

- `--frame-margin <x>`: Distance multiplier for `--frame-node` (default 1.0; 2.0 frames twice as far away).

- `--search-path <dir>` (repeatable): extra texture folder for this run, relative to the model; works with `--isolated`, which ignores `settings.ini`. In **File → Settings → Search Paths**, folders may use case-insensitive wildcards: `../texture*` matches sibling texture folders, `../*` searches immediate sibling folders, and `../**` searches recursively. Recursive expansion skips symlinks/junctions; reload the model to rescan newly created folders.

- Search Paths starts with the ordinary, removable `../texture*` list item when `[SearchPaths]` is absent. Removing every item persists `[SearchPaths] count=0`, so the default stays removed after restarting. Existing saved lists are authoritative; restoring the default-only list removes the override section, consistent with saving only non-default values. The model's own folder remains the separate implicit lookup location.

## AA, quality & viewport rendering

- `--aa <none|msaa2|msaa4|msaa8|fxaa|taa>`: Legacy AA preset of the renderer-wide quality, setting both MSAA and Post AA (e.g. msaa4 = 4x + None, taa = Off + TAA, none = Off + None). Registry `aa` (renderer; also a compatibility viewport key).

- `--msaa <0|2|4|8>` and `--post-aa <none|fxaa|taa>`: Renderer-wide geometry and post AA (every viewport; settings scopes), applied after `--aa` regardless of argument order. Default MSAA Off + TAA; a new viewport uses the global AA. Viewport Settings → Quality → Anti-Aliasing; registry `msaa`, `post_aa` (renderer).

- `--debug-view <name>`: Selects a viewport Render Mode at startup, using the View menu's names with spaces,
  underscores and hyphens ignored (`occlusion`, `roughness`, `shadingnormal`, `geometrybitangent`,
  `normaltexture`, `decalprepassdepth`, `decalprepassnormal`, `ssr`, `ssrray`, ...).
  SSR shows hit U/V in red/green and confidence in blue (black = miss); SSR Ray shows the view-space
  reflected direction mapped to RGB (magenta = rejected physical-pane hemisphere). Unknown names log the value and leave the
  default shaded view.

- `--select-viewport <id>`: Selects a viewport to inspect its settings.

- `--quality <high|balanced|performance>`: Render-quality preset for this run (default High, the original quality). Finer overrides: `--ibl-size`, `--ibl-irradiance-size`, `--ibl-lut-size`, `--ibl-precision <32|16>`, `--ibl-samples`, `--grid-mode <ray|plane>`, `--render-scale`. The UI equivalent is **File → Settings → Quality** (preset, AA, ground grid, render scale, effect quality, the IBL filter sizes and samples, sun shadows; the Scene Inspector shows an IBL filter summary while IBL is enabled); all renderer-wide and saved in the portable `settings.ini` (`[RenderQuality]`; `%APPDATA%/Glider/settings.ini` on fallback).

- `--auto-quality <on|off>`: Auto quality defaults to on for interactive sessions and is saved by **File → Settings → Quality → Auto quality**. It starts with cheap effective settings and steps toward the configured choices using measured CPU/GPU frame time and RAM/VRAM headroom. The configured choices are never replaced by the automatic values. Explicit CLI IBL/scale/AA/SSR/bloom overrides and manual UI edits pin their setting group. `--isolated`, `--no-persist`, `--dump-frame`, `--capture-viewport`, `--fixed-frame-time`, and `--ui-script` disable adaptation unless `--auto-quality on` is explicit. Off preserves the configured rendering path; loading memory protection remains active. See `docs/LOADING_AND_ADAPTIVE_QUALITY.md`.

- `--quality-preset <auto|low|med|mid|high|ultra>`: select the editable renderer-wide preset. Interactive startup defaults to Auto, which adapts toward Ultra. The viewport dropdown has Auto/Low/Mid/High/Ultra; **File → Settings → Quality → Presets** edits and resets the four definitions (the Current Settings sub-tab holds the former Viewport Settings Quality tab). They are saved in the portable `settings.ini` (`[QualityProfiles]`; same fallback and isolated rules as other settings). Texture-cap changes refresh images without reopening the model. Legacy `--quality` retains its existing meanings. See `docs/QUALITY_PRESETS_AND_TELEMETRY.md`.

- `--show-telemetry`: show status-bar VRAM usage/budget and FPS in deterministic tooling runs. Normal interactive runs show these automatically. Existing isolated/capture harnesses keep their previous chrome unless this flag or `--quality-preset` explicitly requests the new controls. Clicking either readout opens and focuses the Statistics panel.

- `--render-mode <shaded|wireframe>`: viewport shading mode for captures and profiling.

- `--wireframe-overlay`: draw mesh edges over the shaded viewport.

- `--wireframe-color <r,g,b[,a]>`: wireframe line RGB colour (0..1) with optional opacity (default 1,1,1,1).

- `--wireframe-blend <normal|additive|invert|xor>`: blend mode for wireframe lines drawn into the LDR overlay target (default normal).

- `--disable-wireframe-cache`: same-binary A/B reference; copy wireframe materials per drawable on every redraw instead of reusing per-viewport variants. `wireframeMaterialCopies` in profile frame counters reports actual copies.

## Capture & diagnostics

- `--report-node <name|#index>`: Repeatable. Adds `nodes` array to `--profile-report` with `index`, `name`, `visible`, `screenRect` `[x0, y0, x1, y1]` in capture-image coordinates, `onScreen`, `clipped`, `behindCamera`, and world-space `center`.

- `--report-pick <x,y>`: Repeatable. Adds `picks` array to `--profile-report` with GPU-picked node, mesh, primitive, and material under pixel `(x, y)` in capture-image coordinates (or `null` if background).

- `--dump-frame <output.jpg>`: Renders 8 warm-up frames and dumps the window framebuffer to disk as `.jpg` or `.png`.

- `--decal-prepass-check <out.json>`: A2.1 diagnostic. After the warm-up frames it reads the decal prepass
  depth/G0 and the forward depth/Geometry Normal, writes the maximum differences and viewport/buffer sizes,
  then exits. It forces a single-sample target and redraws every frame. Files without a decal material report
  `decalBuffer.active: false` instead of failing.

- `--dump-next-frame <output.png>`: With `--dump-frame`, also dump the immediately following window frame (overlay steadiness diagnostic); `--exit-after-dump` waits for both.

- `--exit-after-dump`: Exits the application immediately after dumping the frame.

- `--capture-loading <window.png>`: Save one loading-progress window frame during image decoding. Ordinary capture/dump frame numbers count model frames, excluding loading frames. `--profile-report` now also includes `startup.loadStagesMs`, progress-frame count, texture estimates/reductions, sampled DXGI GPU usage, and adaptive status/effective settings.

- `--clear-color <r,g,b>`: scene clear colour for this run; disables the background skybox (capture/test hook).

- `--shadows on|off`, `--shadow-resolution <1024|2048|4096>`, `--shadow-cascades <2..4>`, `--shadow-distance <m>`,
  `--shadow-filter <hard|pcf|soft>`, `--shadow-split-lambda <0..1>`, `--shadow-normal-bias <texels>`,
  `--shadow-slope-bias <x>`, `--shadow-constant-bias <x>`: A3.2 cascaded shadow maps of the sun (drawn only while
  the sun is on; default on, 2048 x 3 cascades, 200 m, PCF, lambda 0.75, biases 1 / 2 / 1). `--shadows` / `shadows`
  switch the scene's shadows (environment; Scene Inspector → Shadows, Reflections & AO; a viewport overrides them with
  `viewport_shadows`; not saved). The rest is renderer-wide quality (File → Settings → Quality → Current Settings → Sun Shadows, with
  the presets' "Allow sun shadows" gate, registry `shadows_allowed`, `[RenderQuality] shadows=`; the quality fields
  are part of the four presets in `[QualityProfiles]`, all of them in `[RenderQuality]`; Auto starts at 1024 x 2 and
  steps up to the configured values); registry keys `shadow_resolution`, `shadow_cascades`, `shadow_distance`,
  `shadow_filter`, `shadow_split_lambda`, `shadow_normal_bias`, `shadow_slope_bias`, `shadow_constant_bias`,
  `shadows_allowed` (saved preferences: `set_persist(true)` over the pipe). Each viewport keeps its own map, fitted to its frustum (size² x cascades x
  4 bytes: 48 MiB at the default), released while the sun or its shadows are off. `--debug-view shadowcascades`
  and `shadowvisibility` show the cascades and the visibility term; `sun.shadows` in `get_state` and
  `--profile-report` lists each viewport's cascades. Sun off and shadows off are byte-identical to before. See
  `docs/ASOBO_EXTENSIONS_PHASE_3.md` (A3.2).

- `--compute-selfcheck <out.json>`: GL 4.6 smoke test. On a 4.3+ context it dispatches a `#version 430` compute shader that writes a known pattern into an `r32ui` image with `imageStore`, reads it back and reports PASS/FAIL (exit code 0/1); below 4.3 it reports "skipped" and succeeds. Registered in `scripts/run_regression.py`.

- `--validate-file <gltf> --validation-report <out.json>`: validate headlessly and exit; files the loader accepts are validated with their model, so the model-based checks (including ASOBO) run.

- `--disable-shared-traversal`: Every viewport traverses the scene itself (A/B reference).

- `--disable-pick-culling`, `--disable-instancing`: Same-binary A/B references for the Phase 8 hybrid picking and instancing (see `docs/RENDERER_OPTIMIZATION_PHASE_8.md`).

## Agent control & automation

- `--ui-script-follow <file>`: Watches `<file>` for appended commands, executes them between frames on the main thread, and writes structured JSONL results to `<file>.out.jsonl`. Wakes at least every 250 ms when idle. Enforces non-persistent isolated mode and displays `Agent script: <file>` in the status bar. See [`docs/LIVE_SCRIPT_CONTROL.md`](LIVE_SCRIPT_CONTROL.md). Script lines run through the shared command registry, so every registry command (below) also works in `--ui-script` files.

- `--control-pipe`: Opens the agent control pipe `\\.\pipe\glider-<pid>` (current user only, local only, one client at a time; also **File → Settings → Agent Control**, session only). Line-delimited JSON `{"id","cmd","args"}`; `describe` lists the commands, which come from the same registry as the CLI flags and `--ui-script` (`header/imgui/CommandRegistry.h`, `source/imgui/application/Commands.cpp`). The status bar shows "Agent connected" while a client is attached; pipe commands never write `settings.ini` unless the client calls `set_persist(true)`. `get_state` carries `gltfExtensions`; the extension keys are renderer-scope and saved only with `set_persist(true)`. Clients: `scripts/glider_mcp.py` (dependency-free MCP stdio server; `--pid`, `--launch`, `--exe`) and `scripts/glider_ctl.py`. Runtime-safe viewport/environment flags (`--ssr`, `--asobo-disable`, `--exposure`, `--wireframe-*`, `--debug-view`, `--set-asobo-param`, `--select-node`, ...) apply through the registry's settings, so a pipe `set_viewport_setting` / `set_environment` / `set_material_param` renders exactly like the flag; a new setting is added by registering it once (`docs/AGENT_CONTROL_PLAN.md` §7.2). Readback: `probe` (linear HDR before tone mapping, LDR, depth, world position, pick) and `probe_region`. Checked by `scripts/validate_agent_control.py`.

- `--agent-guide [<out.md>]` / `--agent-guide --format skill <dir>`: Writes the embedded agent guide (`resources/agent/GLIDER_AGENT_GUIDE.md`) followed by the command and settings tables generated from the registry, then exits without opening a window (no path: redirected stdout or the parent console). `skill` writes `<dir>/glider-control/SKILL.md`.

## Environment, sky, sun & time

- `--ambient-color <r,g,b>`, `--ambient-intensity <cd/m²>`: A3.8 Manual ambient colour (linear RGB) and luminance (0..10000 cd/m², default 3). Select Manual with `--ambient-mode manual`; legacy `--ambient on` also chooses Manual when no mode was specified. The uniform diffuse/specular/sheen/clearcoat and ground terms, authored AO/SSAO, pre-exposure, units and glTF independence are unchanged. A3.10 adds the source-dependent enable default and Auto fill below. Registry `ambient_color`, `ambient_intensity`; state/profile `ambient`; not persisted.

- `--sun-source <off|hdr|manual|clock>` (A3.10), registry `sun_source`: **Sun Source** in Scene Inspector → Sun & Time of Day.
  HDR offers Off / From HDR / Manual / Time of Day; the sky offers Manual / Time of Day (atmospheric colour and
  intensity). Each source retains its own selection: until explicitly chosen for the sky, HDR Clock maps to Clock,
  every other HDR choice maps to Manual; switching back restores the HDR choice. From HDR stays locked to the
  panorama even while the clock runs. `sun.source` in `get_state` / profile reports lists requested/effective/HDR
  choices and valid options. `--sun on|off` selects the stored manual/HDR mode or Off; `--sun-mode manual|hdr` changes
  that stored mode and selects it if the HDR sun was enabled (including Clock); mode alone leaves an off sun off.
  `--time-of-day-mode on` selects Clock (both environment choices); off restores the legacy enabled/manual/HDR choice
  and selects Manual for the sky. Later source-selecting flags win. Selecting Manual/From HDR/Off does not stop the
  independent clock's moon/stars or emissive evaluation. The ambiguous HDR Clock + From HDR combination therefore
  follows the last selection. Viewer settings only, never part of the glTF.
  Legacy `sun` and `sun_mode` update the retained HDR choice; an explicit sky selection is kept. The legacy
  `time_of_day_mode` selects both environments; `sun_source` selects only the current environment.

- `--ambient <auto|on|off>`, `--ambient-mode <auto|manual>`, `--ambient-fill <k>` (A3.10, default k = 0.05): enabled automatically with
  the procedural sky and disabled with HDR. The unspecified intensity mode resolves to Auto with sky / Manual with
  HDR. Legacy `--ambient on` chooses Manual if no mode was specified. Auto uses white `L = k E_out / pi` (environment
  radiance units; E_out horizontal illuminance, 1 unit = 12,800 lux; L cd/m² = L units × 12,800). Sky E_out is the
  Curve's sun + sky + moon + glow; HDR E_out is its filtered Lambertian cube at +Y × pi × HDR intensity plus the
  effective sun's horizontal illuminance (absolute HDR scale is conventional). The per-fragment Auto term is weighted
  by `1 - diffuse sky visibility` when A3.9 is active; uniform without a volume. The open ground plane receives no
  Auto fill while the volume is active. Manual retains its uniform colour and cd/m² value (`--ambient-color`,
  `--ambient-intensity`), byte-identically. Registry `ambient`, `ambient_mode`, `ambient_fill`; state/profile `ambient`
  reports setting/mode/fill, outdoor illuminance (units/lux), intensity (cd/m²/units), radiance, visibility weighting.
  No persistence. See Phase 3 A3.10 and `scripts/validate_sun_source.py`, `validate_ambient.py`.

- `--sun on|off`, `--sun-mode manual|hdr`, `--sun-dir <az,el>`, `--sun-color <r,g,b>`, `--sun-intensity <x>`,
  `--sun-angular-radius <deg>`: A3.1 sun (scene environment; Scene Inspector → Sun & Time of Day → Sun; the same keys
  `sun`, `sun_mode`, `sun_dir`, `sun_color`, `sun_intensity`, `sun_angular_radius` through `set_environment`).
  Directional light through the punctual-light BRDF. Azimuth around +Y from +Z towards +X, elevation above the
  horizon; intensity is illuminance in the HDR's radiance units x sr (a white Lambert surface facing the sun
  reflects colour x intensity / pi). `hdr` mode takes the HDR's brightest region (direction after
  `--hdr-rotation`, colour, integrated illuminance x HDR intensity). While the sun is on, the HDR's own sun is
  removed from the IBL and background (one re-filter); off (default) is byte-identical. `get_state` and
  `--profile-report` include a `sun` object. See `docs/ASOBO_EXTENSIONS_PHASE_3.md` (A3.1).

- `--time-of-day <day|night|auto>`: The `ASOBO_material_emissive` day/night switch (day=0 [default], night=1, auto): `emissiveDayMultiplier` during day, `emissiveNightMultiplier` at night; auto is night while the sun's elevation (from the time-of-day clock below, whether or not the sun follows it) is under `--night-threshold` (default -6 deg). This is **not** the clock. Also editable in Scene Inspector → Environment (Emissive Day/Night).

- `--time-of-day-mode <on|off>`, `--date YYYY-MM-DD`, `--time HH:MM[:SS]`, `--utc-offset <hours>`, `--latitude <deg>`, `--longitude <deg>`, `--heading <deg>`, `--time-preset <dawn|morning|noon|afternoon|dusk|night>`, `--sun-refraction <on|off>`, `--night-threshold <deg>`, `--time-play <on|off>`, `--time-scale <x>` (A6.1): the scene-level solar clock (Scene Inspector → Sun & Time of Day → Time of Day). Mode off (default) restores the legacy sun choice; on selects the Time of Day sun source, following the solar position at the local date/time (UTC = time - offset) and place, through `--heading` (the true direction the model's nose points to; MSFS aircraft have the nose on glTF +Z and the left side on +X, so a bearing B is at A3.1 azimuth heading - B). Defaults: Seattle-Tacoma (47.45, -122.31), 2026-04-15, 12:00, UTC-7, heading 0. Presets follow the date and place (dawn/dusk: sun 3 deg up, noon: transit, night: solar midnight). The sun fades out over -1..+1 deg of elevation. `get_state`/`--profile-report` report the computed sun, moon, sunrise/sunset in `timeOfDay`. See `docs/ASOBO_EXTENSIONS_PHASE_6.md` A6.1.

- `--environment <hdr|sky>`, `--sky-mie <x>`, `--sky-ozone <on|off>`, `--sky-ground-albedo <x|auto>`, `--sky-time-slicing <on|off>`, `--sky-budget-ms <ms>` (A6.2/A6.3): the scene's environment source (Scene Inspector → Sky & Atmosphere). `hdr` (default) is the panorama, unchanged. `sky` is a procedural atmosphere (Hillaire 2020 transmittance / multiple-scattering / sky-view LUTs, Bruneton 2017 Earth parameters; `header/imgui/SkyAtmosphere.h`, `header/imgui/SkyEnvironment.h`, `resources/shaders/sky_passes.frag`): the background is drawn from the sky-view LUT, the IBL cubes (Lambertian, GGX, sheen) are filtered from the sky with the IBL sampler's shader, and the sun is always on with its colour and illuminance from the atmosphere transmittance (direction: Sun Source Time of Day or Manual `--sun-dir`; HDR rotation/intensity/blur and `--sun-color/--sun-intensity/--sun-mode` do not apply). Units: 1 environment unit = 12,800 lux (the top-of-atmosphere sun = 10). `--sky-mie` multiplies the aerosol density (1 = clear), `--sky-ground-albedo` overrides the lower hemisphere's grey albedo (`auto` = the plane/ocean albedo of the lowest-id open viewport that draws one in its shaded view, never the focused one, else 0.3). When the sky changes, the cubes are re-filtered at 128 px spread over frames (about 2.5 ms GPU per frame, stated limit 4 ms; `--sky-budget-ms`) and swapped in only when complete, then refined to the quality preset's sizes once the sky is still; `--sky-time-slicing off` filters every change synchronously (reference path). `get_state` / `--profile-report` carry a `sky` object (parameters, sun transmittance, refinement state, GPU time per frame). Validation: `scripts/validate_sky.py`; review: `scripts/review_sky.py`. See `docs/ASOBO_EXTENSIONS_PHASE_6.md` A6.2/A6.3.

- `--sun-disk <on|off>`, `--sun-disk-scale <x>`, `--sun-disk-limb-darkening <on|off>` (A6.7, scene environment; Scene Inspector → Sun Disk): while the sun is on (either environment source) the background shows its disk at the sun direction (default on): angular radius `--sun-angular-radius` x scale (0..20), mean radiance E / (pi sin^2 R) (E the A3.1 sun illuminance; in the procedural sky the transmittance colour), Hestroffer & Magnan limb darkening mu^alpha normalised to that mean, an antialiased edge (`header/imgui/SunDisk.h`, `cubemap.frag` `SUN_DISK`). Drawn in the visible background only, never into an IBL filter input (the directional light is the sun's lighting). The real sun's radiance exceeds the RGBA16F scene target, so the drawn disk is scaled (hue kept) until its brightest channel is 32000 (`get_state` / `--profile-report` `sun.disk`: `visible`, `angularRadius`, `meanRadiance`, `centreRadiance`, `rangeScale`). The disk, the procedural sky and the HDR background all show the true per-pixel view direction: `cubemap.vert` projects the background cube exactly with a perspective camera (no clip-w offset; the Khronos sample viewer's w + 0.1 drew the HDR about 7% towards the image centre until the background-projection fix), the orthographic path keeps the old projection; a perspective background writes depth exactly 1 (`scripts/validate_background_projection.py`). Mirrors and windshields show the sun through the A3.1 specular highlight; the glare is the bloom (bloom off: none). `--lens-flare <off|on|ghosts|streak|both>` and `--lens-flare-intensity <x>` (per viewport, Viewport Settings → Post; default off): ghosts, a halo and/or an anamorphic streak from the bloom mips (`AntiAliasing::renderLensFlare`), only from light above 4 in exposed units, added through the bloom mix, so bloom off removes it. Registry keys `sun_disk`, `sun_disk_scale`, `sun_disk_limb_darkening`, `lens_flare`, `lens_flare_intensity`. Validation: `scripts/validate_sun_disk.py`; review: `scripts/review_sun_disk.py`. See `docs/ASOBO_EXTENSIONS_PHASE_6.md` A6.7.

- `--moon <on|off>`, `--moonlight-scale <x>`, `--stars <on|off>`, `--star-density <x>`, `--sky-airglow <x>` (A6.6, procedural sky; moon and stars need the A6.1 clock): moon disk at the A6.1 position with its phase (Lommel-Seeliger, Krisciunas-Schaefer phase law, albedo 0.12: full moon about 0.25 lux), the moonlight as the one shadowed directional light while brighter than the sun; procedural stars (to magnitude 6.5) rotated by local sidereal time, fading in below -6 deg; a night sky glow (airglow, 2e-4 cd/m^2 at the zenith, in the sky LUT and its IBL). `--time-of-day auto` now blends the emissive multipliers over twilight (A6.5). Validation: `scripts/validate_night.py`; review: `scripts/review_night.py`.

## Exposure & tone mapping

- `--hdr-rotation <deg>`, `--exposure <ev>`, `--tone-mapping <neutral|hill|narkowicz>`: Environment rotation,
  exposure and tone mapping for this run. Environment settings belong to the glTF scene (one set per scene, shared
  by every viewport) and are edited in the Inspector when the scene row is selected (`SceneInspector`).

- `--exposure-mode <default|manual|auto|curve>`, `--exposure-min-ev100 <ev>`, `--exposure-max-ev100 <ev>`, `--exposure-metering <average|highlight>` (A6.4, A6.4b): the scene's exposure. `default` is Manual with the HDR (unchanged) and `auto` with the procedural sky (A6.4b; `curve` before): metered eye adaptation in EV100 (limits default -12..18; Glider EV = 13.38 - EV100 + a key curve that keeps night darker than day, + `--exposure-compensation`); with the HDR, `auto` is the previous auto exposure. `--exposure-metering highlight` (default) takes the darker of that average and a highlight meter (the brightest channel at the 99.7th percentile of the image area, surfaces only: scene depth < 1, so the sky, moon and stars do not count; exposed to 1.05 since A6.4c, was 0.8: the A3.9 sky occlusion darkened the cockpit interior, so the noon screen contrast and panel needed about +0.4 EV), so lit cockpit screens are not clipped; `average` is the A6.4 meter alone. Auto adapts at 3 /s towards brighter scenes and 1.5 /s towards darker ones; capture runs (`--fixed-frame-time`, `--dump-frame`, `--capture-viewport`, `--capture-first-frame`, `--ui-script`) snap it to the meter every frame, so pipe settings and CLI flags give the same image. `curve`: an exposure from the time of day (horizontal illuminance of sun + clear sky + moon + night glow, incident meter). With the sky in auto/curve the scene target is pre-exposed (`u_PreExposure`: 2^curve EV in `curve`, 2^round(adapted EV) in `auto`; readers divide it out; probes report linear radiance); `get_state` `night.exposure.viewports[].meter` reports both meters. `--tone-mapping default` (the new default) = ACES Hill with the HDR, PBR Neutral with the sky. `--night-vision <0..1>` (default 0.75): per-pixel rod-vision shift (desaturation to blue-grey below 3 cd/m^2) in the physical modes. `get_state` / `--profile-report` `night`. See `docs/ASOBO_EXTENSIONS_PHASE_6.md` A6.4.

## Ground & ocean

> **Flagged scope ambiguity:** the ocean entry below calls its flags per viewport and not persisted, but `--ocean-quality` is the renderer-wide exception. `source/imgui/RenderQuality.cpp:697` resolves it into each viewport, and `:866` serializes the profile value. Read [SETTINGS_SCOPES.md](SETTINGS_SCOPES.md) for ownership; the original entry is retained for review.

- `--hide-grid`: hide the active viewport's ground grid for review captures.

- `--ground <grid|plane>`: what the active viewport draws on the ground (A3.6): the editor grid (default, unchanged) or an infinite ground plane in the linear HDR scene target (ray intersection like `--grid-mode ray`, writes depth, lit by the IBL and by the sun with the A3.2 shadows, fogged into the environment's horizon; shaded views only). `--ground-height <model-base|zero|number>` (default model-base: the scene's lowest point from the glTF node transforms, visible nodes only, ASOBO invisible/occluder materials ignored, never moved by animation; a number is a custom height in scene units), `--ground-preset <concrete|grass|sand|grey>` (sets colour, roughness, detail and slab joints), `--ground-color <r,g,b>` (linear albedo, also the average albedo A6.2 reads), `--ground-roughness <0..1>`, `--ground-detail <0..1>`, `--ground-detail-scale <units>`, `--ground-joints <units>` (slab joint spacing, 0 = none), `--ground-haze <units>` (fog visibility distance, 0 = automatic 0.9 x far plane), `--ground-grid <on|off>` (draw the grid on the plane, at its height). Later flags override earlier ones (a preset after a colour replaces the colour). `--ground ocean` selects the A6.8 ocean (below; needs OpenGL 4.3, otherwise it logs one warning and keeps the current ground). Registry keys: `ground`, `ground_height_mode`, `ground_height`, `ground_preset`, `ground_color`, `ground_roughness`, `ground_detail`, `ground_detail_scale`, `ground_joint_spacing`, `ground_haze`, `ground_grid`; `get_state` carries a `ground` object.

- `--ocean-wind-speed <0.5..40 m/s>`, `--ocean-wind-dir <0..360 deg>` (the direction the wind comes from; the sun's azimuth convention), `--ocean-fetch <1000..1e7 m>`, `--ocean-choppiness <0..2>`, `--ocean-swell <0..1>`, `--ocean-foam <0..1>` (0.5 = the whitecap coverage observed at the wind speed), `--ocean-color <r,g,b>` (linear water body colour 0..1; default 0.0035,0.022,0.035), `--ocean-quality <low|medium|high>` (renderer-wide quality), `--ocean-mode <loop|live>`, `--ocean-period <5..120 s>` (looping mode), `--ocean-time <s>` (pins the ocean time for captures; -1 follows the clock), `--ocean-play <on|off>` (A6.8, with `--ground ocean`): per viewport, not persisted; registry keys `ocean_wind_speed`, `ocean_wind_dir`, `ocean_fetch`, `ocean_choppiness`, `ocean_swell`, `ocean_foam`, `ocean_color`, `ocean_quality`, `ocean_mode`, `ocean_period`, `ocean_time`, `ocean_play` (CLI = pipe; out-of-range values are refused). `get_state` and `--profile-report` carry `ground.ocean` (settings, cascades, memory, precompute progress and slices, live GPU ms per frame, foam parameters; the report only while the ground is the ocean). Checked by `scripts/validate_ocean.py`; review captures by `scripts/review_ocean.py`.

## MSFS / ASOBO & scene effects

- `--decal-tolerance <mm>`: Scene decal depth tolerance along the receiver normal, 0–10 mm (default 1.2 mm); applies only to the decal buffer. Scene Inspector → Shadows, Reflections & AO (shown for files with decals); registry `decal_tolerance_mm` (environment).

- `--ssr <on|off>`: the scene's windshield-v3 screen-space reflections (default on; registry `ssr`, environment). A viewport overrides it with `viewport_ssr scene|off|on`; the viewport key `ssr` sets that override.

- `--ssr-quality <low|medium|high>`: renderer-wide 256/512/1024 screen-space sample budgets, maximum pixel stride 2/1/1, with five refinement steps (default medium). File → Settings → Quality → Current Settings → Effects.

- `--interior-sky-reflection <0..1>`: the scene's windshield IBL fallback strength inside the combined environment-occluder bounds; without occluders it applies to every windshield (default 0.5). SSR and this strength are in Scene Inspector → Shadows, Reflections & AO.

- `--windshield-icing <0..1>`: the scene's icing state (0.0 = clean [default], 1.0 = fully iced): windshield icing coverage and the coverage of `frosted` geometry decals. Scene Inspector → Shadows, Reflections & AO → Aircraft state.

- `--sky-portal-strength <0..4>` and `--show-sky-portals`: A3.7 `ASOBO_sky_portal` window light (default 0.35, added as (1 - occlusion) x strength to OPAQUE/MASK materials of files with portals) and the portal gizmos (UI overlay; a selected portal node always shows its own). The strength is a scene setting (Scene Inspector → Shadows, Reflections & AO), the gizmos a viewport helper. `--asobo-disable ASOBO_sky_portal` switches the light off (File → Settings → glTF Extensions, app setting). The gizmo switch is in Viewport Settings (glTF Extensions block, shown for files with portals). See `docs/ASOBO_EXTENSIONS_PHASE_3.md`, A3.7.

- `--ssao <on|off>`: the scene's screen-space ambient occlusion of the sky light (A3.5, default off; Auto quality never changes it; a viewport overrides it with `viewport_ssao`). GTAO from the A2.1 depth/normal prepass (the decal buffer when decals are active, otherwise the viewport's own prepass target), computed before the opaque pass with a depth-aware 5 x 5 denoise (and a depth-aware upsample at half resolution); `pbr.frag` multiplies only the IBL diffuse (multi-bounce) and, through Lagarde's specular occlusion, the IBL specular, sheen and clearcoat of OPAQUE/MASK surfaces, as `min(authored occlusion, SSAO)`; the sun, punctual lights, emissive, BLEND/transmission, the background and the ground plane are unchanged. `--ssao-radius <x>` (world units, default 0.5), `--ssao-intensity <0..4>` (exponent, default 1), `--ssao-quality <low|medium|high>` (2 x 4, 3 x 6, 4 x 8 samples per side), `--ssao-resolution <full|half>`; debug view `ssao` (raw visibility). Scene Inspector → Shadows, Reflections & AO (on, radius, intensity: scene) and File → Settings → Quality (quality, resolution: renderer-wide); registry keys `ssao`, `ssao_radius`, `ssao_intensity` (environment), `ssao_quality`, `ssao_resolution` (renderer); `get_state` / `--profile-report` carry an `ssao` object (per viewport: active, target size, memory, GPU time). Fragment shaders only (`#version 410`): works on every 4.1+ context. See `docs/ASOBO_EXTENSIONS_PHASE_3.md` A3.5.

- `--sky-occlusion <auto|on|off>`, `--sky-occlusion-quality <low|medium|high>`, `--sky-occlusion-occluders <on|off>` (A3.9, scene environment; Scene Inspector → Sky Occlusion, after Ambient Light; the bake quality is renderer-wide, File → Settings → Quality → Current Settings → Effects): sky occlusion inside the model. A sky-visibility volume over the model's bounds (cubic cells, at most 64 / 96 / 128 per axis plus 2 padding cells; per cell the visible fraction `a` and mean visible direction `m` of 64 / 128 / 256 Fibonacci directions, RGBA16F, `a = -1` inside solids) is baked from orthographic depth maps of the casters (`ShadowCasterLayer` rules without `noCastShadow`, plus `ASOBO_material_environment_occluder` primitives unless `--sky-occlusion-occluders off`; glass/BLEND/decals/invisible do not block) and applied in `pbr.frag` (`sky_occlusion.glsl`, define `SKY_OCCLUSION` per viewport) to the IBL terms only: diffuse by the cosine-weighted visibility (L1 and visibility-cone readings blended), specular/sheen/clearcoat by a lobe-cone visibility, as a product with authored AO / decal AO / SSAO; A3.7 portals add back `(1 - occlusion × visibility)`. The sun, punctual lights, emissive, the A3.8 ambient light, the background and the ground plane are unchanged. Default `auto`: on with the procedural sky, off with the HDR (HDR renders byte-identical). The bake is time-sliced (4 / 8 / 8 frames), rebakes on model, visibility, transform, material, extension, isolation, quality or occluder changes (never per frame, animation ignored), keeps the previous complete volume until the new one is done, and is skipped without IBL (Low preset). Debug view `skyvisibility`; registry keys `sky_occlusion`, `sky_occlusion_quality`, `sky_occlusion_occluders`; `get_state` / `--profile-report` carry a `skyOcclusion` object (volume dims/cell/memory, bake frames/CPU/GPU ms; `upToDate`, `noCasters`: a key with nothing to bake, e.g. an isolated glass part, has no volume and is not re-traversed per frame; `sceneBuilds`). Validation: `scripts/validate_sky_occlusion.py`; review: `scripts/review_sky_occlusion.py`. See `docs/ASOBO_EXTENSIONS_PHASE_3.md` A3.9.

- `--show-invisible-materials`: Also draw materials marked invisible by `ASOBO_material_invisible` and environment occluders (`ASOBO_material_environment_occluder`); both are hidden by default. An app setting for every viewport; this run only.

- `--asobo off|on`, `--asobo-disable <extension>` (repeatable): glTF extensions Glider applies in every viewport: an app setting (File → Settings → glTF Extensions, `settings.ini` `[GltfExtensions]`, saved when edited in the UI). The flags apply for this run only and are never saved. All on by default; `--asobo off` renders the file as if it had no ASOBO extensions. Every `Rendered`/`Partial` row of the support table with a switch can be disabled, e.g. `--asobo-disable ASOBO_material_geometry_decal` or `--asobo-disable ASOBO_material_detail_map`.

- `--msfs-emissive-scale <x>`: explicit scene/session MSFS emissive scale override for **both** environment sources. Without an override: HDR = `1/1000` (unchanged), procedural sky = `1/12800` (ASOBO emissive in cd/m², A1.6b). For assets declaring `ASOBO_*` with ASOBO/MSFS emissive enabled, scales authored factor × texture × KHR emissive strength × ASOBO day/night multiplier; authored values are never changed. ASOBO/MSFS emissive off keeps the existing [0,1] factor clamp. Non-ASOBO glTF, including KHR emissive_strength, stays unchanged with either source. No reliable glTF-only 2020/2024 unit discriminator exists: legacy ASOBO files also use nits with the sky and may need an override. Scene Inspector → Environment shows the effective scale (same function as renderer/registry); editing overrides both sources, right-click → Use source default restores automatic selection. `get_environment msfs_emissive_scale` and `get_state.msfsEmissive.effectiveScale` report the effective value; the latter also carries `overridden` / nullable `overrideScale`. Validation: `scripts/validate_emissive_units.py`.

- `--msfs-emissive on|off`: Same-binary A/B switch; `off` renders ASOBO assets with the clamped core emissive factor (the behaviour before the MSFS mapping). Default on.

- `--set-asobo-param <material> <extension> <param> <value>` (repeatable): edits a material's ASOBO extension parameter in memory after loading (by material name or index), using the same code path as the Inspector UI (`ExtensionInspector::setMaterialAsoboParameter` + `GltfRenderLayer::updateMaterialStates`).

- `--asobo-report <out.json>`: After loading (and any `--reload-model`), write the typed ASOBO extension model and its diagnostics, then exit (`scripts/validate_asobo_model.py`). The model lives in `gltf/extensions/AsoboExtensions.h`; its table is generated by `scripts/generate_asobo_specs.py` from `resources/schemas/asobo` plus the exporter data in `docs/ASOBO_EXTENSIONS_SEMANTICS.md`.

## Test hooks & environment variables

- Loading diagnostics also include `lastLoadWallMs`, `lastLoadToFirstModelFrameMs`, `loadingProgressUiMs`, image CPU/GPU payload bytes and `texturesIntentionallyOmitted`. `Shader link` and `Tangent generation` are separate stages. Normal interactive opens log import completion and first model frame readiness separately. `GLIDER_TEST_OPEN_FILE=<path>` supplies the native-open result only while a UI script is active, for focused menu checks; it has no effect in ordinary sessions.

- `GLIDER_TEST_VRAM_MB=<MiB>` (environment): caps the reported process-local video-memory budget for constrained tests. `GLIDER_TEST_QUEUE_LOAD=<model>` queues one replacement during image decoding. `GLIDER_TEST_TEXTURE_ALLOCATION_FAILURE=1` injects one texture-allocation fallback branch without inserting a GL error; `GLIDER_TEST_LOAD_FAILURE=upload` exercises safe failure of model upload. These hooks do not change persistent preferences.

- `GLIDER_NO_FOCUS=1` (environment): shows and renders the window normally but creates it without keyboard focus (`GLFW_FOCUSED` / `GLFW_FOCUS_ON_SHOW` false); unset behaviour is unchanged.

## Invocation example

Example test command:
```cmd
cmd /c "build\bin\Release\Glider.exe --dump-frame frame.jpg --exit-after-dump"
```
*(Note: Because `Glider.exe` is a Win32 subsystem executable, wrap in `cmd /c` when invoking from PowerShell to wait for process exit synchronously).*
