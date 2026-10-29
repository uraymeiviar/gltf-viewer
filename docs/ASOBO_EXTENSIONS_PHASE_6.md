# Phase A6 — Time of Day

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** In progress: A6.1-A6.7 done (A6.8 optional, later).

**A3.10 update:** the historical Sun / Sun Mode / Sun Follows Time of Day controls and their precedence described
below have been replaced by one source-specific **Sun Source** selector. HDR From HDR is locked to the panorama;
Manual and Time of Day are the sky's choices. Clock evaluation, exposure and atmosphere units are retained. Auto
interior ambient now uses the same Curve horizontal illuminance. See [Phase 3 A3.10](ASOBO_EXTENSIONS_PHASE_3.md).

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
      override 0..1; -1 = automatic: the A3.6 plane (or A6.8 ocean) albedo of the lowest-id open viewport that draws one in its shaded view (focus never changes it), else 0.3, Earth's
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
- [x] **A6.4 — Exposure and tone mapping (R3). Owner: Claude.** Exposure that keeps noon, dusk and night readable,
  either an MSFS-like auto exposure (eye adaptation) or presets per time of day, and the tone-mapping curve
  closest to MSFS. Both are judged against the reference screenshots.
  - **Done 2026-10-29.** `header/imgui/NightSky.h` (constants, key curve, sky illuminance table), `Application::exposurePlan`
    (`Commands.cpp`), `AntiAliasing::updateAutoExposure` (physical branch), pre-exposure in `pbr.frag`, `ground.frag`,
    `cubemap.frag` (`environment_block.glsl` `u_PreExposure`).
    - **Modes** (`exposure_mode`, Scene Inspector → Environment → Exposure Mode): *Manual* (the EV, unchanged), *Auto (eye
      adaptation)*, *Time-of-day curve*, and *default*: Manual with the HDR source (byte-identical to before), the curve
      with the procedural sky (Auto since A6.4b, below). `auto_exposure on/off` = auto / manual. With the HDR, Auto is the previous metered auto
      exposure and the curve acts as Manual (an HDR panorama has no time of day).
    - **Units:** saturation-based exposure at ISO 100 (Lagarde & de Rousiers 2014) H = 1 / (1.2 x 2^EV100) per cd/m^2;
      with A6.2's 1 unit = 12,800 cd/m^2 this is **Glider EV = 13.38 - EV100** (EV 0 = EV100 13.4). Auto meters the
      scene (the existing 64 x 64 centre-weighted log average with outlier rejection) as a reflected meter, EV100 =
      log2(L x 100 / 12.5); the curve is an incident meter on the horizontal illuminance, EV100 = log2(E x 100 / 250),
      E = sun (direct) + clear sky (table integrated from the A6.2 model; below -10 deg the rendered sky's own values) +
      moonlight + night glow, plus 2 x the sky part (the sky in view) and -0.75 EV (calibrated against Auto).
      **Limits** EV100 -12 (starlight, about 0.001 lux) .. 18 (`exposure_min_ev100` / `exposure_max_ev100`): Glider EV
      about -3 .. +23. **Key/compensation** (shared by both modes): +0.6 EV in daylight (EV100 >= 11: an 18% grey at
      0.18), +0.4 at 8, -0.2 at 4, -0.8 at 0, -1.3 at -4, -1.6 at -8, -1.8 at -12 (linear between): night is shown
      darker than day, monotonic; `exposure_compensation` is added. Auto adapts with the existing speeds.
    - **Pre-exposure:** night radiance (1e-8 .. 1e-5 units) is below the half-float scene target's normal range, so in the
      physical modes the scene's linear outputs are written x 2^curveEV (pbr, ground plane, sky background; the
      transmission/SSR copies divide it out) and the post exposure is 2^EV / pre-exposure; the meter, `probe` and
      `probe_region` remove it (radiance in environment units). Pre-exposure 1 = the previous shaders exactly.
    - **Tone mapper:** default for the procedural sky **Khronos PBR Neutral** (`tone_mapping default`: ACES Hill with
      the HDR, unchanged). Comparison (scripts/review_night.py, E195 sunset frame, 828 sky pixels near the set sun,
      exposed linear colour vs displayed): PBR Neutral hue shift mean 0.6 deg / 95th percentile 1.7 deg, saturation
      kept 1.07; ACES Hill 1.7 / 5.4 deg, 0.91; ACES Narkowicz 3.4 / 10.2 deg, 0.61 (yellows the orange horizon, washes
      the sky out). *Inferred:* no MSFS reference screenshots were available; the choice is for plausible sunset hues.
    - **Night vision** (`night_vision`, default 0.75, physical modes only): per pixel, below 3 cd/m^2 (full at 0.01)
      the colour moves towards its luminance with a blue tint (Jensen et al. 2000); bright pixels keep their colour.
  - **Check:** `scripts/validate_night.py` (serial list): E195 exterior on the plane, Auto and Curve, mean displayed luma
    in 0.05..0.60 at noon (0.34), sunset (0.33), dusk -6.4 deg (0.21), moonless night (0.08) and full moon (0.10);
    evening sweep sunset - 30 min .. + 2.5 h in 5-minute steps: largest step 0.023 (curve) / 0.018 (auto), limit 0.03;
    curve darkens monotonically (largest rise 0.004, the night plateau; limit 0.01).
  - **Deviation / finding:** the A6.2 sky had kinks in its twilight brightness every ~3.7 degrees of sun elevation (the
    32 x 32 multiple-scattering LUT was filtered linearly where Psi_ms falls by orders of magnitude): `sky_passes.frag`
    now interpolates it bilinearly in log space (validate_sky still passes, references within tolerance). Interiors at
    night (MSFS-scaled emissive panels) need Auto: the curve is the exterior's and overexposes cockpits.
  - **A6.4b (2026-10-29), deviation: Auto is the procedural sky's default, with highlight metering.** Review of the
    A6.4 commit: with the sky at night the curve blew the E195 cockpit out, and Auto clipped its screens (and those of
    the DA62, metered EV 21.5 from the dark sky and stars through the windows).
    - `exposure_mode default` = **Auto** with the procedural sky (Manual with the HDR, byte-identical); Curve and Manual
      stay selectable. `exposure_metering average|highlight` (default highlight; Scene Inspector → Metering, after the
      exposure controls).
    - **Highlight meter** (`NightSky.h`, `AntiAliasing::updateAutoExposure`): the meter pass also writes the brightest
      channel of its four taps per 64 x 64 cell; cells showing the background (scene depth = 1, a nearest-sampled 64 x 64
      blit of the scene depth; single-sample targets only, with MSAA every cell counts) count as black, so the sky's
      glow round a low sun, the moon and the stars may compress in the shoulder as before. The value at the **99.7th
      percentile** of the image area (minimum bright area 0.3%, about 12 cells) is exposed to **1.05** (A6.4c; 0.8 in
      A6.4b; PBR Neutral's shoulder starts at 0.76; displayed about 0.94 sRGB), plus `exposure_compensation`; no key curve (a display limit).
      The exposure is the **darker** of that and the average meter (unchanged: trimmed centre-weighted log average, EV100,
      key curve, compensation, EV100 limits). Single LEDs, the moon and stars do not move it (measured below).
    - **Pre-exposure follows Auto:** `2^round(adapted EV)` (a power of two: exact, so the image does not depend on which
      one was used) instead of the curve's; with the curve's (2^20.6 at night) the 3-5 unit MSFS-scaled screens were
      clipped at the half-float target's 13,000 limit before the meter saw them. Interactively it changes when the
      adapted EV is more than 1 EV away (hysteresis).
    - **Adaptation:** with the sky the two speeds are read as the scene getting brighter (3 /s, light adaptation) and
      darker (1.5 /s); the sliders are labelled *Adapt to Brighter / Darker*. The HDR's Auto is unchanged.
    - **Determinism:** capture runs (`--fixed-frame-time`, `--dump-frame`, `--capture-viewport`, `--capture-first-frame`,
      `--ui-script`) snap the sky's Auto to its meter every frame and set the pre-exposure from the last frame only, so
      pipe and CLI give byte-identical images (validate_sky's pipe = CLI and validate_night's Auto pipe = CLI pass).
    - **A6.4c (target 0.8 -> 1.05):** A3.9 (sky occlusion) darkens the sunlit-cockpit interior, so the sunlit white
      `$DRM_UI_SCREEN` surface, not the panel, limits the noon exposure and the panel median fell to 0.035 (< 0.04) with
      screen-to-bezel contrast 0.285-0.291 (< 0.3). Raising the target to 1.05 (+0.39 EV at noon: -1.01 -> -0.62) passes
      both; the percentile (0.997 -> 0.999) does not help (the sunlit area is larger than 0.1%). Night screens' 99th
      percentile 0.941 (limit 0.95). The numbers below are the A6.4b ones at 0.8.
    - **Measured** (validate_night, 1280 x 720, no AA): E195 cockpit main displays at night EV -1.91 (average meter 1.56,
      highlight -1.91), screens' 99th percentile 0.871-0.882, 6.5% of the panel outside the screens lit above 0.1;
      E195 windshield view at noon EV -2.56, screens 0.702, 0.37% of the image clipped, panel median 0.059; DA62 at night
      EV -2.64 (average 7.2), screens 0.875. Exterior Auto unchanged from A6.4 (noon 0.343, sunset 0.295, dusk 0.202,
      night 0.114, full moon 0.093; sweep largest step 0.0185). Star field on/off 0.004 EV, moon in view on/off 0.000 EV,
      a single LED on a surface 0.002 EV.
    - **Not met / finding:** the MSFS-scaled displays are flat emissive 3000-5000 (A1.6: x 0.001 = 3-5 units =
      38,000-64,000 cd/m^2 in the sky's units, 50-100 x a real avionics display), 28 EV above the night sky. Keeping them
      unclipped leaves the DA62 interior and its stars black (the stars need about EV 18+). `exposure_metering average`
      gives the A6.4 look (530 stars detected, screens clipped). The E195/DA62 screens have no content in the glTF
      (render targets), so "content contrast" is the screen against its bezel. A physically calibrated MSFS emissive
      scale for the sky's units would ease this; not changed here (A1.6 setting).
  - **A1.6b (2026-10-29):** the preceding A6.4b measurements describe the old 1/1000 mapping. The sky now uses
    ASOBO cd/m² / 12800 by default (E195 3000 → 0.234375 units; DA62 5000 → 0.390625, 12.8 times dimmer).
    HDR, core glTF and KHR emissive_strength on non-ASOBO files stay unchanged. Explicit MSFS scale overrides both
    sources (A1.6). The highlight meter, key curve, adaptation, tone mapping and bloom are unchanged. With an
    emissive-limited night interior the expected Auto shift is +log2(12.8) = +3.678 EV, so displayed screens keep
    their highlight limit while real ambient light becomes more visible. Curve remains an exterior incident meter
    and can still clip cockpit screens. Global exposure cannot span a 5000 cd/m² display and a starlit sky at once.
    Measured with `validate_night` (1280 x 720, no AA): E195 night **EV +1.771**, DA62 night **+1.034**, screen
    99th percentile 0.875 (below 0.95) and contrast above 0.83; E195 noon **EV -1.283**, panel median 0.098,
    exterior clipped 0.37%. The noon screens are dimmer: 99th percentile 0.274 (MFD/PFD) / 0.665 (EICAS), with
    screen-to-bezel contrast **0.179-0.196**. Three existing contrast assertions fail their unchanged 0.3 threshold;
    every screen still passes the not-clipped limit. The physical conversion is retained; neither authored values,
    exposure nor those thresholds are adjusted to recover the old over-bright displays.
    Updated measured exposures and captures: `emissive-units.report.md` in the maintainer's codex-runs folder;
    `scripts/review_emissive_units.py` writes `build/review/emissive_units/contact_sheet.png`.
- [x] **A6.5 — Emissive day/night and lights (R2). Owner: Claude.** The A1.6 day/night switch and the A3.3 macro
  lights' day/night cycle follow the time of day.
  - **Done 2026-10-29.** Automatic day/night now blends: night weight w = smoothstep from threshold + 3 to threshold - 3
    degrees of lighting elevation (0.5 at the threshold, where the reported state flips), multiplier = mix(day, night,
    w) (`u_TimeOfDay` 2 + `u_EmissiveNightBlend`, `pbr.frag` and `decal_accum.frag`); outside the blend the integer
    Day / Night path is used, so the A6.1 checks (byte-identical to forced Day / Night) hold. `timeOfDay.dayNight.nightBlend`.
    Check: an emissive quad over a 90-minute sweep matches day x (1 - w) + night x w within 0.06%, one state flip.
  - **Not done:** `ASOBO_macro_light` lights are not rendered (A3.3 deferred), so they have no day/night cycle.
- [x] **A6.6 — Night sky (R2). Owner: Claude.** Moon (position and phase from the A6.1 date and location, lit
  disk, and a dim directional light through the A3.1 sun path) and stars (a star-field cubemap or procedural
  stars, rotated by sidereal time). Without them night is black plus emissives.
  - **Tests:** moon position and phase against a published ephemeris; night exposure keeps the moonlit model
    readable (A6.4).
  - **Done 2026-10-29.** Procedural sky with the A6.1 clock (without the clock there is no moon and no stars).
    `night::evaluateMoon`, `worldToEquatorial`, `cubemap.frag` (`MOON_DISK`, `STARS`), `sky_passes.frag` (glow),
    `Application::effectiveDirectionalLight`, Scene Inspector → *Night Sky* (collapsed, after Sun Disk).
    - **Moon disk** at the A6.1 topocentric moon direction (refraction per `sun_refraction`), angular radius from its
      distance, drawn with the true view direction (not cubemap.vert's). Shading: Lommel-Seeliger mu0 / (mu0 + mu) of
      the hemisphere lit from the true sun-from-moon vector, so phase and bright-limb orientation follow from geometry.
      Illuminance (top of atmosphere) = p x E_sun x (R/d)^2 x Krisciunas-Schaefer phase law, p = 0.12, E_sun = 10 units:
      **full moon 0.234 lux at phase angle 7.7 deg** (0.31 at 0), x the atmosphere transmittance towards it; the disk
      is normalised to integrate to it. Background only; the moonlit sky (scattered moonlight) is not modelled.
    - **Moonlight:** the A3.1 directional light (same BRDF path, angular radius of the moon) with the A3.2 shadows,
      **one shadowed directional light at a time: the brighter of sun and moon** (the sun by day; once it has set the
      moon; at the switch both are 1e-4 of the sky). Colour: white sunlight x transmittance, not tinted; the bluish
      look of moonlit scenes comes from the tone stage (*night vision*, A6.4), stated choice. `moonlight_scale`.
    - **Stars:** hash-based, no catalogue: a 6 x 256 x 256 cube grid on the celestial sphere, star probability per cell
      proportional to its solid angle (about 8,900 stars x `star_density` down to magnitude 6.5), magnitudes from
      N(<m) ~ 10^(0.5 m), illuminance 2.54e-6 lux x 10^(-0.4 m), hashed colour temperature, a Gaussian about a pixel
      wide integrating to the illuminance, extinction by Kasten-Young airmass with the zenith optical depth of the
      A6.2 atmosphere, no twinkle. Rotated world -> equatorial from the clock's local sidereal time, latitude and heading.
      Fade: smoothstep from sun -6 deg (0) to -12 (1). Not in the IBL; their integrated light is part of the glow.
    - **Night sky glow:** 2e-4 cd/m^2 at the zenith (airglow + integrated starlight, slightly green-white), van Rhijn
      factor of a 90 km layer, achromatic extinction; inside the sky-view LUT, so it lights the scene through the IBL
      (about 0.001 lux) and the horizon is never black (`sky_airglow`).
  - **Check (validate_night.py):** full moon centre 0.9 px from the projected A6.1 direction (limit 1), centre radiance
    = K/2 within 0.7%; lit fraction from the image 1.001 / 0.817 / 0.401 vs A6.1 0.995 / 0.809 / 0.398 (limit 0.1),
    lit side towards the sun within 0.1 deg; white Lambert sphere: fitted light direction 1.5 deg from the moon, strength
    +4.3% (IBL glow included; limit 10%); shadow map rendered for the moon at night and the sun at noon, the sphere's
    moon shadow on the ground; 767 / 800 stars in a 60 deg night view, none at noon, field turns -15.0 deg about the
    reported celestial pole in 1 h (expected -15.04).
  - **Deviations:** moon and stars need the clock; no moonlit sky glow, no earthshine, no lunar surface detail (the
    disk is uniform albedo); HDR source: no moon/stars/glow. With 16-bit IBL presets (Balanced, Performance) the night
    glow underflows the IBL cubes (High uses 32-bit).
- [x] **A6.7 — Sun disk and glare (R2). Owner: Claude.** A sun disk with limb darkening in the sky, so it appears
  in the background, in sky reflections and in SSR on the windshield; and a glare/bloom response through the
  existing bloom chain (optionally a lens-flare or starburst, user-selectable). The disk is excluded from the IBL
  filter input so the sun is not counted twice (the A3.1 rule: while the sun is on, the HDR's own sun region is
  clamped out of the panorama before filtering, `IBLSampler` `EnvironmentSun`; a sky model should add its disk
  only to the background, after filtering).
  - **Tests:** the disk is at the A6.1 sun direction; bloom off removes the glare.
  - **Done 2026-10-29.** `header/imgui/SunDisk.h`, `resources/shaders/cubemap.frag` (`SUN_DISK`),
    `AntiAliasing::renderLensFlare`, `Application::sunDiskEvaluation`, `SceneInspector::renderSunDisk`.
    - **Disk:** drawn by the background shader only (`SUN_DISK` define, absent while the sun is off or the disk
      disabled, so those images are unchanged), in both environment sources, at the A3.1 sun direction (in the
      procedural sky and with the clock: the A6.1 sun). Angular radius R = `sun_angular_radius` x `sun_disk_scale`;
      mean radiance E / (pi sin^2 R), E the A3.1 sun illuminance the lighting uses (procedural sky: 10 x the
      transmittance towards the sun, so the disk has the transmittance colour; HDR: the Manual or From HDR sun, which
      replaces the HDR's removed sun). Limb darkening: Hestroffer & Magnan (1998) I(mu) / I(1) = mu^alpha, alpha =
      (0.397, 0.503, 0.652) at 680 / 550 / 440 nm (as in Hillaire 2016), normalised by (alpha + 2) / 2 so the disk
      still integrates to E. Antialiased edge: coverage over the pixel's angular footprint (`fwidth`), so it is about
      one pixel wide and the same for every TAA jitter.
    - **Not in the IBL:** the background shader is no filter input (the HDR is filtered from the panorama after the
      A3.1 sun removal, the sky from `sky_passes.frag` `PASS_CUBE`), so the directional light alone carries the sun's
      lighting. Switching the disk changes only its own pixels and never re-filters the sky.
    - **Half-float range:** the real sun's mean radiance (1.2e5..1.5e5 for E = 8..10 at 0.27 degrees) exceeds the
      RGBA16F scene target, so the drawn colour is scaled (hue kept) until its brightest channel at the centre is
      32000 (`rangeScale` in `get_state` / profile `sun.disk`); half the range stays free for blending.
    - **True view direction:** the disk and the procedural sky (A6.2) use the direction the perspective camera really
      sees at each fragment, so the disk is at the projected sun position (0.0-0.1 px measured), in line with the
      geometry, the ground plane, shadows and highlights. `cubemap.vert` (from the Khronos sample viewer) first
      projected the cube with w + 0.1, which drew the HDR background up to about 9% (direction and camera dependent)
      towards the image centre, and the disk and sky carried a matching ray correction. Since the background-projection
      fix (2026-10-30, maintainer-approved image change) a perspective camera projects the cube exactly (no clip-w
      offset, `pos.xyww` at the far plane) and `trueWorldDirection` is the cube position, so the HDR background lands
      on its world directions too; the orthographic path keeps the old projection and correction (byte-identical).
      `cubemap.frag` also writes depth 1 for a perspective camera: the rasterized depth of the large, clipped cube
      triangles could round one 24-bit step below 1, so background cells counted as surfaces in the A6.4b highlight
      meter (already up to 5% of a view before, up to 32% with the exact projection, which darkened the dusk sweep).
      `scripts/validate_background_projection.py`: panorama markers within 0.77 px (mean 0.09 px) of their projected
      directions (500 measurements, 30 / 60 / 90 deg FoV, two aspects, AA none and TAA; the old projection misses by
      up to 52 px), the disk within 0.2 px with the HDR and the sky, no cube seams or holes, no background cell below
      the far plane.
    - **Reflections:** mirror-like materials and windshields show the sun through the A3.1 highlight (GGX widened by
      tan(R) / 2): it integrates to E and peaks at E / (pi tan^2 R), the disk's mean radiance (within cos^2 R), and its
      energy-equivalent size is the disk's reflection (validated on a mirror sphere). SSR misses fall back to the
      environment cube, which has no disk; the analytic highlight is added independently of SSR, so glass never
      loses the sun. The disk is not drawn into the cubes for reflections (it would count the sun twice).
    - **Glare:** the disk's HDR radiance goes through the existing Jimenez bloom chain (no change to it); bloom off
      leaves a sharp edge. **Lens effect** (per viewport, Viewport Settings → *Post*, a new tab after Quality;
      `lens_flare off|ghosts|streak|both`, default off, `lens_flare_intensity`): one pass between the bloom
      downsample and upsample reads the downsampled levels 1 and 3 and adds into level 0: five ghosts mirrored
      through the image centre with a small chromatic split, a halo ring, and/or a 49-tap horizontal anamorphic
      streak; only light above 4 in exposed units takes part (the composite's bloom mix carries it, so bloom off
      removes it and an image without such light is byte-identical). The program is compiled on first use.
    - **Settings:** environment `sun_disk` (default on), `sun_disk_scale` (0..20, default 1; the disk keeps E, so a
      larger disk is dimmer; the A3.1 highlight is not scaled), `sun_disk_limb_darkening` (default on); viewport
      `lens_flare`, `lens_flare_intensity` (0..10). CLI `--sun-disk`, `--sun-disk-scale`, `--sun-disk-limb-darkening`,
      `--lens-flare`, `--lens-flare-intensity`. UI: Scene Inspector → *Sun Disk* (a header after Sky & Atmosphere,
      collapsed by default).
  - **Check:** `scripts/validate_sun_disk.py` (quick regression, serial list); review `scripts/review_sun_disk.py`
    (`build/review/sundisk/`). Sun off with the HDR source (with every disk / lens setting changed) byte-identical
    to the baseline (HelloCube, two E195 ASOBO views), and sun on with `--sun-disk off` too; HDR disk (black
    panorama, R 1 degree, two directions and scale 2): centre 0.03-0.11 px from the projected sun, diameter within
    0.02 px of 2R projected, interior radiance E / (pi sin^2 R) within 0.04%; limb darkening: centre = mean x
    (alpha + 2) / 2 within 0.1%, profile at rho 0.5 / 0.8 = mu^alpha within 0.2%; procedural sky with the A6.1 clock:
    centre 0.00 px from the clock's sun, diameter exact, colour = the sun's transmittance colour; Lambert sphere
    identical with the disk on and off (HDR and sky, no re-filter or swap), only pixels inside the disk change;
    mirror sphere (R 2 and 4 degrees): highlight energy-equivalent diameter -1.1 / -1.2% from the disk's ray-traced
    reflection, peak -1.3 / -0.3% from E / (pi tan^2 R); bloom off: black beyond 2 px of the edge; lens effect off,
    at intensity 0 or with bloom off byte-identical to the effect-free render, on adds light only (ghosts opposite,
    the streak horizontal), byte-identical when nothing exceeds its threshold; registry, refused values, pipe = CLI.
  - **Deviations:** the drawn disk is limited to 32000 per channel (half-float range); the size multiplier keeps the
    illuminance (a larger disk is dimmer) and does not widen the highlight; the HDR background keeps
    `cubemap.vert`'s projection (changing it would change every HDR image); `validate_sun.py`'s check that the HDR's
    own sun is removed from the background now passes `--sun-disk off` (the drawn disk is there by design) and a new
    check confirms the drawn disk replaces it.
- [x] **A6.8 — Ocean (R3, optional, later). Owner: Claude.** An FFT ocean (Tessendorf) as the A3.6 *Ocean* ground
  choice, for seaplanes, carrier aircraft and showcase views. Not required for the exit gate. Done in two parts (A6.8a
  core, A6.8b settings, live mode, calibration; 2026-10-29).
  - **Simulation:** 2 / 3 / 4 cascades (Low / Medium / High) at 128² / 256² / 256², a JONSWAP spectrum with the Horvath
    2015 directional spread, foam from the displacement Jacobian.
  - **Looping precomputed variant (default):** the surface repeats exactly every loop period (default 30 s); the
    frames are computed once with GL 4.3+ compute into a texture array, so the per-frame cost is texture sampling only.
    Changing wind or sea state recomputes in the background, spread over frames like A6.3. **Live** mode runs the FFT
    every frame (512² at High). Below GL 4.3 the ocean is unavailable (the choice is disabled).
  - **Rendering:** a camera-centred polar grid to the horizon; sky / IBL reflections (no ocean SSR); the ocean's average
    colour feeds the lower hemisphere of the environment lighting (as A3.6's ground albedo); stable under TAA; correct
    depth where floats and hulls meet the water.
  - **Settings:** wind speed and direction, fetch, choppiness, swell, foam amount, water colour, quality, loop period,
    mode (Looping / Live), play / pause.
  - **Tests:** the loop is seamless; deterministic captures; the per-frame cost of both modes is measured
    (`scripts/validate_ocean.py`).
  - **Design (as built).**
    - **Spectrum and cascades** (`header/imgui/OceanSimulation.h`, `source/imgui/OceanSimulation.cpp`,
      `resources/shaders/ocean_{spectrum,fft,finalize,foam,mip}.comp`): JONSWAP (fetch-limited alpha and peak frequency,
      the fetch clamped at full development, g F / U² <= 2.2e4) x the Horvath 2015 spread (Donelan-Banner / Mitsuyasu
      exponent plus the swell term). The wave-number range [k_peak / 8, 2 pi / 4 cm) is cut into log-equal bands, one per
      cascade, ratio B = min(N / 8, (F / 7.5)², range^(1/C)) (the frame term only when looping); each patch puts its band
      top at 4 texels per wavelength. h0 is made on the CPU from hashed Box-Muller numbers (deterministic, independent of
      the C++ library) and uploaded as RGBA32F. Per frame on the GPU: spectrum, row and column radix-2 inverse FFT in
      shared memory (one complex transform carries h + i Dx, a second Dz), finalize into (lambda Dx, h, lambda Dz, J), then
      for a loop two foam turns per cascade (min(J, J_prev + 0.6 / s x dt)) and a compute mip chain.
    - **Loop:** every cascade's dispersion is quantised to multiples of 2 pi m / T (m per cascade so its lowest band
      frequency is >= 1.25 quanta), so the set repeats after T exactly (time t and t + T are byte-identical for exactly
      representable times); F frames of T / m (32 / 48 / 64) are interpolated at render time. Memory (array + mips +
      scratch): Low 11.9 MiB, Medium 101.0 MiB, High 175.7 MiB (4 x 256² x 64 frames; a precomputed 512² loop would need
      0.5 to 1 GB, so 512² is live only). Synchronous set (the first set, capture runs): 6.8 / 18.1 / 25.0 ms wall (the CPU
      spectrum dominates), 2.9 / 3.3 / 6.7 ms GPU. Later changes recompute in the background in timestamp-timed slices
      (1, 2, 4 ... units within 3 ms; Medium: 8 frames, 4.3 ms GPU in total, largest slice 1.43 ms); the old array stays
      until the swap. Per-frame cost of drawing the loop (Opaque pass against the plane, ocean filling the frame): 0.060 /
      0.108 / 0.114 ms.
    - **Live:** one layer per cascade (2 x 128², 3 x 256², 4 x 512²), recomputed every frame at time t with the exact
      dispersion (the phase in double precision, `glUniform1d`), foam accumulating in J itself; the first step after a rebuild,
      a seek back or a pause over 0.5 s pre-rolls 1.5 s of foam history (15 steps of 0.1 s), so a pinned time shows the foam a
      running clock has. Measured GPU time per frame (all cascades: spectrum, two FFT passes, finalize, mips): 0.052 / 0.089
      / 0.285 ms (RTX-class GPU under VM load; maxima 0.8 / 1.0 / 4.7 ms are first-step outliers), memory 1.1 / 6.0 / 30.7
      MiB. A setting or mode change builds the live set at once (the CPU spectrum: 3.6 / 12.8 / 64 to 123 ms wall); a mode
      change drops the old set (the programs and scratch are shared).
    - **Mesh** (`ocean.vert`, `GltfResourceManager::initOcean`): a camera-centred polar grid of 256 x 192, radius s (e^(vL) - 1),
      s = half the camera height above the sea (at least 0.5 m), reaching 2000 heights and at least 100 km. Chosen over a
      projected grid because turning the camera never moves a vertex. The grid is centred on the camera in X/Z, so a
      camera move slides the vertices over the waves; displacement is therefore sampled one mip level coarser than the
      ring spacing, which keeps waves shorter than about four vertices out of the geometry (they would change with the vertex
      positions). Five camera positions 0.2 m apart at a fixed ocean time (grazing 6 degrees, `review_ocean.py`
      `swim_strip.png`) differ frame to frame by 3.22 / 3.24 / 3.27 / 3.32 levels: even steps, no swimming visible;
      the pixel normals are sampled from the field and never swim.
    - **Shading** (`ocean.frag`): per-pixel normals from the displaced field at the footprint's mip per cascade; the slope
      variance the mips and the finest cascade lose is added to the GGX roughness (Cox-Munk total 0.003 + 5.12e-3 U). Fresnel
      (IOR 1.33, F0 0.02) between the sky / IBL reflection (the split-sum LUT; a reflection below the horizon is mirrored
      above it) and the water body; the A3.1 sun's GGX highlight with the A3.2 shadow visibility. Under a uniform sky of
      radiance L a calm sea at nadir reads L (0.02 + 0.98 colour) within 0.4% per channel. Reflections are sky / IBL only:
      **no ocean SSR** (the SSR path belongs to the windshield pass, after the opaque pass; the ocean is an Opaque-pass draw,
      so it would need its own copy of the opaque scene: not clean, dropped).
    - **Shadow on the water:** the shadow removes the sun's direct specular and two thirds of the sun-driven body light; the
      sky reflection and the sky-lit body light are untouched. The remaining third stands for light that entered the water
      outside the shadow and diffused sideways (diffusion lengths of metres to tens of metres, more than a hull's shadow
      is wide). The body is lighter at crests and darker in troughs (+-35% at Hs). Measured lit / shadowed radiance
      (sun 55 degrees, procedural sky, 6 m/s, glitter-free sample): 1.88; with the sun-driven part removed completely it
      is about 2.5 (the A6.8a look the review found too dark).
    - **Foam:** whitecaps where the summed Jacobian J = 1 + sum over the cascades of (J_c - 1) is low (the per-cascade test
      of A6.8a never saw a fold: each cascade's J varies by about 0.07, the sum by 0.07 to 0.085; the first-order estimate
      lambda sqrt(S) is about 2.2 times too wide because of the 4-texel central differences and the persistence). The threshold sits z
      standard deviations below the mean (-0.03), z chosen so the coverage is the whitecap fraction of Monahan and
      O'Muircheartaigh (1980), W = 3.84e-6 U^3.41 (0.02% at 3 m/s, 1% at 10, 10% at 20, 42% at 30), times 4 x amount² (amount 0.5 =
      the observed fraction), trimmed by a measured gain of 0.8; sigma narrows with the slope variance the pixel footprint still
      resolves, so the coverage holds at every distance. Edge width 0.7 sigma; the threshold is perturbed by noise fixed to
      the undisplaced water coordinate (lace; foam moves with the water, not the crests). Measured whitecap fraction (uniform
      sky, 120 m of sea, Medium): 0.00 / 0.89 / 11.7% at 3 / 10 / 20 m/s (30 m view: 0.00 / 0.64 / 2.98%); live with a pinned time
      0.70% / 8.9% at 10 / 20 m/s.
    - **Integration:** Opaque pass like the plane; removed before picking and never in the overlay depth (not pickable); the
      shadow cascades cover it; its average albedo (0.066 + colour) feeds A6.2; shaded views only (debug views and wireframe show
      the sky). TAA: the plane's reprojection (no motion vectors); the animated surface relies on the neighbourhood clamp.
    - **Model interiors:** with the A3.9 volume (default with the procedural sky) water where < 5% of the sky is visible is
      discarded; without it, while the camera is inside the model's bounds, water inside the bounds is discarded (limit: from a
      cockpit the water under the wings / between floats inside the bounding box is hidden too).
    - **Time:** per-viewport clock advanced by the frame time while playing; capture runs move it only by `--fixed-frame-time`
      steps; `--ocean-time` pins it; `--ocean-play off` holds it.
  - **Interface:** `--ground ocean`; `--ocean-wind-speed`, `--ocean-wind-dir` (the direction the wind comes FROM, the sun's
    azimuth convention), `--ocean-fetch`, `--ocean-choppiness`, `--ocean-swell`, `--ocean-foam`, `--ocean-color`,
    `--ocean-quality`, `--ocean-mode <loop|live>`, `--ocean-period`, `--ocean-time`, `--ocean-play`, all through the registry
    (CLI = pipe; out-of-range values refused); the settings UI in Viewport Settings → Camera & Helpers → Ground (unique IDs,
    checked with a UI script in Ocean and Plane mode: no ImGui ID conflict on any control); `get_state` and `--profile-report`
    carry `ground.ocean` (settings, cascades, memory, precompute progress and slices, live GPU ms, foam parameters).
  - **Checks:** `scripts/validate_ocean.py` (registered in `run_regression.py`): Grid and Plane byte-identical to the baseline,
    loop seam (0 s and 30 s byte-identical; 7.25 s and 37.25 s), deterministic captures (loop and live), CLI = pipe for every
    flag, refused values, GL 4.3 gate (test hook), hull depth and pick, closed-box interior, uniform-sky energy, foam against the
    wind, shadow ratio, UI ID conflicts, GPU cost. Review: `scripts/review_ocean.py` (`build/review/ocean/contact_sheet.png`,
    `seam_strip.png`, `swim_strip.png`).
  - **Deviations:** High (looping) is 4 x 256², not 512² (memory); no ocean SSR; the shadow keeps a third of the sun-driven body light
    (lateral light diffusion in the water); the foam threshold is calibrated against measurements, not derived (the first-order
    Jacobian statistics were 2.2 times too wide); the seam check compares 7.25 s and 37.25 s (exactly representable) instead
    of 7.3 s and 37.3 s (4e-6 s apart as floats: one pixel of 450,000 differed by 2 levels); a pinned live time pre-rolls its
    foam history instead of starting without it.

**Exit gate:**
- Moving time from dawn to night is seamless (frame budget met).
- Reference screenshots at noon, sunset and night (moonlit) are reviewed, including the sun disk and glare.
- Files without ASOBO extensions are unchanged unless time-of-day mode is explicitly enabled for them.
