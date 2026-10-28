# Phase A6 — Time of Day

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A6.1, A6.2 and A6.3 done.

**Entry:** A3.1 and A3.2 (sun and shadows). **Goal:** a dynamic, configurable time of day emulating MSFS (user
priority 6). The sun, sky, environment lighting, exposure and emissive day/night all follow one time-of-day
setting, and changing it is seamless.

## Tasks

- [x] **A6.1 — Solar position (R1). Owner: Claude.** Sun direction from date, time, latitude, longitude and
  aircraft heading (a standard solar-position algorithm), driving the A3.1 sun (its azimuth/elevation convention
  and units are in [Phase 3, A3.1](ASOBO_EXTENSIONS_PHASE_3.md): azimuth from +Z towards +X around +Y).
  - **Settings:** ~~Viewport Settings~~ Scene Inspector → *Sun & Time of Day* → *Time of Day* (scene-level, like the sun:
    one clock per glTF scene, shared by every viewport), with the presets Dawn, Morning, Noon, Afternoon, Dusk,
    Night, a time slider and Play (time scale).
  - **Test:** sun azimuth and elevation against a published reference table.
  - **Done 2026-10-28.** Code: `header/imgui/SolarPosition.h` (algorithms, header-only), `header/imgui/TimeOfDay.h`
    (state, presets, clock, parsing; `SceneEnvironmentState::clock`), `Application::effectiveSun` /
    `timeOfDayStateJson` (`source/imgui/application/Commands.cpp`), `SceneInspector::renderTimeOfDay`.
    - **Sun source:** *Manual* (A3.1 as before, the default) or *Time of day* (`time_of_day_mode on`). In Time of
      day mode the sun is on (whatever `sun` says), its direction is the solar position through the heading,
      colour and intensity stay the Manual sun's (`sun_color`, `sun_intensity`; the HDR sun mode is not used), and
      the intensity is multiplied by a smoothstep fade of the lighting elevation from -1 to +1 degrees (the sun
      is disabled for lighting, shadows included, once the fade is 0). The HDR's own sun is removed from the IBL
      and the background in Time of day mode as with any sun (one re-filter when the mode is switched). A6.2 and
      A6.6 will add twilight colour and the moon on top. Mode off changes nothing (byte-identical to the previous
      commit, also with every clock setting changed).
    - **Clock:** local civil time at the location: date (`YYYY-MM-DD`, 1900..2100), time (`HH:MM[:SS]`, fractional
      hours accepted), UTC offset in hours (UTC = time - offset; no daylight-saving or time-zone table), latitude
      (north +), longitude (east +). Defaults, so that nothing needs setting: Seattle-Tacoma airport (47.45 N,
      122.31 W), 2026-04-15, 12:00, UTC-7 (PDT), heading 0. Presets are *resolved lazily* from the current date and
      place (they follow a date, latitude, longitude or offset change); editing the time or starting Play turns the
      preset into plain time. Dawn and Dusk: the morning and evening crossings of +3 degrees of geometric
      elevation; Noon: the solar transit; Night: solar midnight; Morning and Afternoon: halfway between
      dawn/noon and noon/dusk. Where the sun never reaches +3 degrees (polar night) or never goes below it, dawn
      and dusk fall back to noon -/+ 5 h. Play advances the clock by `time_scale` simulated seconds per real
      second (default 600), rolling the date past midnight; it is session state and not saved.
    - **Heading and north:** `heading` is the true compass direction the model's nose points to (0 = north,
      90 = east). **MSFS-exported aircraft have their nose on glTF +Z and their left side on +X** (verified on the
      E195 exterior: nose gear at z = +9.67, rudder at z = -21.08, left elevator at x = +2.69, right at -2.69;
      `scripts/validate_time_of_day.py` repeats the check from the file). Seen from above the nose is +Z and the
      left wing +X, so a compass bearing B (clockwise from north) is at A3.1 azimuth `heading - B` (mod 360): a
      sun straight ahead is +Z (azimuth 0), a sun to the right of the nose is -X (azimuth 270). With heading 0 the
      aircraft's nose points north, so a noon sun (south) is behind it (azimuth 180) and +90 degrees of heading
      turns the A3.1 azimuth by +90.
    - **Solar position:** NOAA solar calculator equations (Meeus ch. 25, low precision: mean longitude and
      anomaly, equation of centre, apparent longitude with the nutation/aberration term, obliquity with its
      nutation term) and Meeus ch. 12 sidereal time. Against ERFA (IAU 2006/2000A) the error is below 0.012
      degrees over 2000-2060. Refraction (`sun_refraction`, default on): the NOAA piecewise fit (about 1010 hPa,
      10 C), applied to the lighting elevation (`lightElevation`); the geometric elevation is reported as well.
      Sunrise/sunset: geometric elevation -0.833 degrees, bisected on a 10-minute scan (within 2 s of ERFA);
      transit and solar midnight from the hour angle; polar day/night reported.
    - **Moon (state only, not rendered until A6.6):** Schlyter's low-precision elements (12 longitude, 5 latitude
      and 2 distance terms), topocentric (diurnal parallax) azimuth and elevation, ecliptic and equatorial
      coordinates, distance, elongation, phase angle, `illuminatedFraction` (0 new .. 1 full), `phase` (cycle
      fraction: 0 new, 0.25 first quarter, 0.5 full, 0.75 last quarter) and age. Against ERFA moon98 the geocentric
      error is below 0.09 degrees (2000-2060); Meeus example 47.a within 0.04 degrees.
    - **Day/night link:** `time_of_day` (the ASOBO_material_emissive switch) gained *auto*: night while the sun's
      lighting elevation is below `night_threshold` (default -6 degrees, civil twilight), day otherwise, read from
      the clock whether or not the sun follows it. The emissive path takes an integer, so it switches at the
      threshold (no blend); A6.5 replaces it. Day and night still force the multiplier.
    - **Names:** `--time-of-day day|night|auto` is and stays the emissive switch; the clock is `--time-of-day-mode`.
    - **Settings / CLI (registry keys, `set_environment`, `list_settings`):** `time_of_day_mode`
      (`--time-of-day-mode on|off`), `date` (`--date YYYY-MM-DD`), `time` (`--time HH:MM[:SS]`), `utc_offset`
      (`--utc-offset`), `latitude`, `longitude`, `heading`, `time_preset`
      (`--time-preset dawn|morning|noon|afternoon|dusk|night`), `sun_refraction`, `night_threshold`,
      `time_playing` (`--time-play`), `time_scale`. The registry gained a `string` setting type for the date and
      time. `get_state` and `--profile-report` carry a `timeOfDay` object: the clock, UTC, the sun (true azimuth,
      geometric/apparent/lighting elevation, hour angle, RA/Dec, fade, A3.1 azimuth and direction), the day's
      events (sunrise, sunset, transit, solar midnight, polar flags), the moon, and the day/night state.
  - **Check:** `scripts/validate_time_of_day.py` (quick regression, serial list). Reference data: the NREL SPA test
    case (Reda and Andreas, NREL/TP-560-34302: 2003-10-17 12:30:30 UTC-7, 39.742476 N 105.1786 W: azimuth
    194.340241, elevation 39.872046) is the one published sun table row; the other twelve rows (high latitude,
    southern hemisphere, equator, polar day and night, 1999 and 2050) and the sunrise/sunset times were generated
    once with pyerfa, an independent implementation (they are not a published table); Meeus examples 47.a/48.a and
    the 2024 new, first-quarter and full moons (USNO/NASA times) for the moon. 13 sun cases within 0.05 degrees
    (worst 0.010), 6 sunrise/sunset times within 60 s (worst 1.5 s), moon within 1 degree (worst 0.07 for the
    topocentric cases, 0.03 for Meeus) and phase within 0.05; heading, fade (monotonic across -1..+1, rendered
    value = reported fade, steepest rise 0.75 per degree), automatic day/night flips (state and render path),
    registry/CLI, play, and mode off byte-identical to the baseline. Review captures:
    `scripts/review_time_of_day.py` (E195 on the ground plane at the six presets).
- [x] **A6.2 — Procedural sky (R4). Owner: Claude.** A physically based sky model (for example
  Hosek–Wilkie or a Bruneton-style precomputed atmosphere) rendered as the background and as the source of
  environment lighting, replacing the fixed HDR in time-of-day mode. The HDR stays available as an option.
  - **Tests:**
    - sky color and luminance against the model's reference values;
    - a smooth transition through sunset.
  - **Lower hemisphere:** the model's ground albedo comes from the A3.6 ground choice, so below the horizon the
    environment shows a lit ground instead of nothing; without a ground plane a neutral albedo is used. A3.6 left the
    value ready: `ViewportInstance::groundAlbedo` (`header/imgui/ViewportInstance.h`, with `groundPlaneActive`; the
    plane's average albedo, `GroundSettings::averageAlbedo()`), refreshed every render of a viewport; take it from
    the active viewport while `groundPlaneActive`, and keep the HDR's own lower half otherwise (also in `get_state`
    `ground.albedo`).
  - **Done 2026-10-28.** Code: `header/imgui/SkyAtmosphere.h` (parameters, units, CPU transmittance),
    `header/imgui/SkyEnvironment.h` (LUTs, source cube, time-sliced IBL; header-only), `resources/shaders/sky_common.glsl`
    (sky-view LUT mapping and lookup), `resources/shaders/sky_passes.frag` (the four passes), `cubemap.frag`
    (`PROCEDURAL_SKY` background), `Application::updateSkyEnvironment` / `effectiveSun` / `skyStateJson`
    (`source/imgui/application/Commands.cpp`), `SceneInspector::renderSky`.
    - **Model:** Hillaire 2020 ("A Scalable and Production Ready Sky and Atmosphere Rendering Technique", EGSR):
      transmittance LUT 256 x 64 (Bruneton 2017 parameterisation, 128 steps), multiple-scattering LUT 32 x 32 (Psi_ms:
      isotropic second order over 64 directions x 32 steps, geometric series 1 / (1 - f_ms), ground included),
      sky-view LUT 192 x 108 (Hillaire's non-linear horizon mapping, 48 quadratic steps, single scattering with the
      Rayleigh and Cornette-Shanks Mie phases plus Psi_ms, energy-conserving integration, a Lambert ground where the
      ray ends on it). Earth (Bruneton 2017 / Hillaire 2020): radii 6360 / 6460 km; Rayleigh (5.802, 13.558, 33.1) e-3
      /km, 8 km; Mie scattering 3.996e-3, extinction 4.440e-3 /km, 1.2 km, g 0.8; ozone (0.650, 1.881, 0.085) e-3 /km
      in a 10..40 km tent. Observer 50 m above the ground, world +Y its zenith, the sun white at the top of the
      atmosphere. Fragment passes (`#version 410`), so the sky works on every 4.1+ context: no compute, no
      unavailable state. LUTs are RGBA32F; transmittance and multiple scattering are rebuilt only when the atmosphere
      parameters change, the sky view when the sun moves. No aerial perspective on the model (the A3.6 plane keeps
      its own haze).
    - **Units:** one constant, `sky::kLuxPerEnvironmentUnit`: 1 environment unit = 12,800 lux (radiance x sr). The
      top-of-atmosphere sun (128 klux) is 10 units; at the ground the sun gives 8.86 at 30 degrees, 9.32 at 60 (the
      A3.1 default is 8), a white Lambert surface facing it reflects about 2.8; the clear sky is 0.16 at the zenith
      with the sun 60 degrees up (2.1 kcd/m^2) and 0.6-0.9 near the horizon. The default exposure (EV 0, ACES Hill)
      shows noon readable (review captures); A6.4 refines exposure.
    - **Environment source** (scene-level, `environment hdr|sky`, Scene Inspector → *Sky & Atmosphere*, a header after
      Sun & Time of Day, collapsed by default so the existing controls keep their places): *HDR* (default) is the
      panorama, byte-identical to before also with every sky setting changed. *Procedural sky*: the background is the
      sky-view LUT drawn per pixel (it follows the sun in the same frame), the environment rotation/intensity/blur
      are identity, the HDR is released (its sun detection and removal do not run), and the A3.1 sun is always on:
      direction = the A6.1 clock's sun (else the Manual angles), illuminance = 10 x the transmittance towards it
      (CPU, 512-step quadrature, the same atmosphere) x the A6.1 fade, so it reddens and dims near the horizon and is
      off below it; `sun_color`, `sun_intensity` and `sun_mode` do not apply. Hooks: `cubemap.frag` marks where A6.7
      adds the sun disk (background only, after filtering) and A6.6 the moon and stars.
    - **Parameters:** `sky_mie` (Mie density x, 1 = clear; 0..20), `sky_ozone` (on/off), `sky_ground_albedo` (grey
      override 0..1; -1 = automatic: the active viewport's A3.6 plane albedo while it draws the plane, else 0.3, Earth's
      mean albedo). `get_state` / profile report `sky`: parameters, effective ground albedo and its source, sun
      direction / transmittance / illuminance, refinement state, GPU statistics, memory.
  - **Check:** `scripts/validate_sky.py` (quick regression, serial list). Reference: `scripts/sky_reference.py`, an
    independent float64 numpy implementation of the same model (direct transmittance quadrature along every ray,
    2000-step single scattering per direction, Psi_ms at 64 x 64 from 256 directions; nothing shared with the GPU
    LUTs), values frozen in the script (`--regenerate-references`). Background radiance at 9 directions for a 60 and
    a 5 degree sun: worst luminance error 1.8%, worst channel 1.9% (tolerance 4% / 5%); sun transmittance at 6
    elevations within 0.005% (tolerance 0.5%) and the renderer's sun = 10 x transmittance; noon zenith bluer than the
    horizon (blue share 0.59 vs 0.42); horizon towards the sun red/blue 0.51 at 60 degrees, 11.5 at 2 degrees;
    zenith and two horizon directions fall monotonically for sun elevations 10 .. -6 degrees; Mie 4 brightens and
    whitens the zenith, ozone keeps the twilight zenith blue, the automatic ground albedo equals the A3.6 plane's;
    HDR source byte-identical to the baseline (HelloCube default and with the sky settings, two E195 ASOBO views).
  - **Deviations:** the sky is selected by `environment sky` rather than tied to time-of-day mode (either sun
    source works with it); the source/atmosphere controls are in a new *Sky & Atmosphere* header (not inside the
    Environment header) so existing UI probes keep their positions; the lower hemisphere is the model's lit ground
    (sun only: the ground receives no sky irradiance in Hillaire's model, so it is a little darker than a full
    radiosity solution); no aerial perspective.
- [x] **A6.3 — Dynamic environment lighting (R4). Owner: Claude.** Re-filter the sky into the diffuse and specular
  environment cubemaps as time changes, without hitches. A full filter costs about 100–700 ms depending on the
  quality preset (Phase 7), so filtering is spread over frames and done at a reduced size while time is moving,
  then refined when it stops.
  - **Tests:**
    - no frame spike above a stated budget while time is moving;
    - the converged result matches a full filter.
  - **Done 2026-10-28.** `header/imgui/SkyEnvironment.h`.
    - **Pipeline:** the sky-view LUT is rendered into a source cube (`PASS_CUBE`, the IBL sampler's face orientation),
      mipmapped, and filtered by `ibl_filtering.frag` compiled with ShaderCache's prefix and driven with
      `IBLSampler::applyFilter`'s uniforms: Lambertian, GGX mips 0..levels and Charlie sheen mips, plus the GGX/Charlie
      BRDF LUTs once (512 samples). The renderer's `EnvironmentMap` wraps non-owned handles
      (`GltfRenderLayer::setExternalEnvironment`); a swap only re-points its three cube samplers and mip counts
      (`EnvironmentMap::retargetCubes`), so no command cache is rebuilt. IBL lighting, reflections, the SSR fallback,
      sky portals and the A3.6 plane's haze all read those cubes and so follow the sky.
    - **Double buffering:** outputs are written to a set that is not displayed and swapped in only when every pass of
      its job is complete (`incompleteSwaps` counts violations: 0). Sets: one at the configured quality, two fast.
    - **While the sky changes** (time slider, play, sun, parameters): a fast job (cube 128, irradiance 32, GGX 128 /
      Lambertian 256 / sheen 32 samples, or less when the preset is smaller) renders its source cube from the current
      LUT at once, then runs its passes in row bands (scissor) within the frame budget; when complete it is swapped
      in and the next fast job starts from the then-current sky. GGX mip 0 uses one sample (every importance sample is
      the normal at roughness 0, so the configured count gives the same value).
    - **When the sky is still** (10 frames) and the front set is fast: a refine job at the quality preset's sizes and
      samples (the source cube's faces and mip chain are work items too) runs the same way and is swapped in when
      complete; a sky change aborts it. Activation and a quality change filter synchronously once (like an HDR load).
      `sky_time_slicing false` filters every change synchronously before the frame (the reference path).
    - **Budget:** each piece of work is followed by a GL_TIMESTAMP query; per kind of work (pass, target size,
      source size) the largest of the last 8 measured costs per sample-texel plans the next frames, an unmeasured
      kind is assumed 8 x the dearest measured one (it starts with a thin band). Aim `sky_budget_ms` = 2.5 ms of
      extra GPU time per frame; stated limit 4 ms. The quality preset's sizes and samples are the refined target;
      the fast set is a temporary state the UI shows (*Updating / Reduced, refining soon / Refining n% / Converged*).
  - **Check (validate_sky.py):** while time plays (1200 x, 240 frames, High preset): every time-sliced frame below
    4 ms (max 2.2-2.4 ms, mean 0.8-1.6 ms over runs on this machine), 45-206 complete fast jobs swapped, no
    refinement while moving; after stopping, refined to 1024 px in 176-240 frames, max 2.7-2.9 ms per frame including
    the refinement; white Lambert sphere
    (probes at normals the sun cannot reach) against the reference irradiance: 0.3% / 0.3% (60 degree sun, away /
    down), 1.0% / 7.9% (5 degree sun; the down-facing value is a few hundredths and the filter's mip-lod sampling blurs
    the bright horizon into it; tolerance 10%); converged time-sliced cubes equal a synchronous full filter exactly
    (difference 0); E195 at dusk with the sky, time of day, shadows and the ground plane: finite, zero GL errors.
  - **Costs:** (GPU, this machine, measured with the timestamps above) sky-view LUT 0.02 ms per sun change;
    transmittance + multiple-scattering LUTs 0.64 ms per parameter change; a fast job's source cube 0.16 ms; a fast job
    completes in 1-4 frames (100-200 swaps in 240 played frames); refinement at High (cube 1024, irradiance 1024, 2048
    Lambertian / 1024 GGX samples) 239 ms of GPU work over about 180-250 frames (about 3-4 s at 60 fps), at
    Performance (512 / 32 / 256 samples) 13 ms over 55 frames; activation / quality change: one synchronous filter
    (about 10 ms of CPU to queue it, the GPU cost of an HDR filter). Memory (cubes, LUTs, the fast source cube): 411 MB
    at High (RGBA32F), 40 MB at Performance; the configured-size source cube exists only while a refinement runs.
  - **Review:** `build/review/sky/contact_sheet.png` (`scripts/review_sky.py`): E195 on the concrete plane at dawn
    (warm low sun, orange horizon, long shadow), noon and afternoon (blue sky, white horizon, sharp shadows, model
    readable at EV 0), sunset and dusk (dark at EV 0; with auto exposure an orange horizon under a pink-purple belt and
    a purple-lit aircraft at sunset, only a red horizon glow at dusk), the DA62 cockpit at noon (blue sky through the
    windows, interior lit) and sunset (sky gradient in the windows, interior dark; readable with auto exposure), the
    sky alone towards the setting sun at EV 0 and +4 (red-orange glow at the horizon fading upwards). Exposure for
    twilight is A6.4's.
  - **Deviations:** the budget is a GPU-time aim measured per kind of work, not a fixed number of faces per frame;
    the configured-quality refinement takes a few seconds at High (it is limited to the same per-frame budget).
- [ ] **A6.4 — Exposure and tone mapping (R3). Owner: Claude.** Exposure that keeps noon, dusk and night readable,
  either an MSFS-like auto exposure (eye adaptation) or presets per time of day, and the tone-mapping curve
  closest to MSFS. Both are judged against the reference screenshots.
- [ ] **A6.5 — Emissive day/night and lights (R2). Owner: Claude.** The A1.6 day/night switch and the A3.3 macro
  lights' day/night cycle follow the time of day.
- [ ] **A6.6 — Night sky (R2). Owner: Claude.** Moon (position and phase from the A6.1 date and location, lit
  disk, and a dim directional light through the A3.1 sun path) and stars (a star-field cubemap or procedural
  stars, rotated by sidereal time). Without them night is black plus emissives.
  - **Tests:** moon position and phase against a published ephemeris; night exposure keeps the moonlit model
    readable (A6.4).
- [ ] **A6.7 — Sun disk and glare (R2). Owner: Claude.** A sun disk with limb darkening in the sky, so it appears
  in the background, in sky reflections and in SSR on the windshield; and a glare/bloom response through the
  existing bloom chain (optionally a lens-flare or starburst, user-selectable). The disk is excluded from the IBL
  filter input so the sun is not counted twice (the A3.1 rule: while the sun is on, the HDR's own sun region is
  clamped out of the panorama before filtering, `IBLSampler` `EnvironmentSun`; a sky model should add its disk
  only to the background, after filtering).
  - **Tests:** the disk is at the A6.1 sun direction; bloom off removes the glare.
- [ ] **A6.8 — Ocean (R3, optional, later). Owner: Claude.** An FFT ocean (Tessendorf) as the A3.6 *Ocean* ground
  choice, for seaplanes, carrier aircraft and showcase views. Not required for the exit gate.
  - **Simulation:** 3–4 cascades at 256², a modern directional spectrum (JONSWAP / Horvath 2015), foam from the
    displacement Jacobian.
  - **Looping precomputed variant (default):** choose a repeat period (for example 30 s), compute the
    displacement and normal maps for that period once with GL 4.3+ compute, and store them in a texture array, so
    the per-frame cost is texture sampling only. Changing wind or sea state recomputes in the background, spread
    over frames like A6.3. Live per-frame FFT is an option on capable GPUs. Below GL 4.3 the ocean is
    unavailable (the choice is disabled).
  - **Rendering:** a projected-grid or clipmap mesh to the horizon; sky and SSR reflections; the ocean's average
    colour feeds the lower hemisphere of the environment lighting (as A3.6's ground albedo); stable under TAA;
    correct depth where floats and hulls meet the water.
  - **Settings:** wind speed and direction, sea state, quality (cascades, resolution), loop period.
  - **Tests:** the loop is seamless (frame 0 equals frame N); deterministic captures with `--anim-time`; the
    per-frame cost of the looping variant is measured.

**Exit gate:**
- Moving time from dawn to night is seamless (frame budget met).
- Reference screenshots at noon, sunset and night (moonlit) are reviewed, including the sun disk and glare.
- Files without ASOBO extensions are unchanged unless time-of-day mode is explicitly enabled for them.
