# Phase A6 — Time of Day

[Journal index](ASOBO_EXTENSIONS_JOURNAL.md) · [Extension reference](ASOBO_EXTENSIONS_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Planned; not started.

**Entry:** A3.1 and A3.2 (sun and shadows). **Goal:** a dynamic, configurable time of day emulating MSFS (user
priority 6). The sun, sky, environment lighting, exposure and emissive day/night all follow one time-of-day
setting, and changing it is seamless.

## Tasks

- [ ] **A6.1 — Solar position (R1). Owner: Claude.** Sun direction from date, time, latitude, longitude and
  aircraft heading (a standard solar-position algorithm), driving the A3.1 sun.
  - **Settings:** Viewport Settings → *Time of Day*, with presets (dawn, noon, dusk, night) and a slider.
  - **Test:** sun azimuth and elevation against a published reference table.
- [ ] **A6.2 — Procedural sky (R4). Owner: Claude.** A physically based sky model (for example
  Hosek–Wilkie or a Bruneton-style precomputed atmosphere) rendered as the background and as the source of
  environment lighting, replacing the fixed HDR in time-of-day mode. The HDR stays available as an option.
  - **Tests:**
    - sky color and luminance against the model's reference values;
    - a smooth transition through sunset.
  - **Lower hemisphere:** the model's ground albedo comes from the A3.6 ground choice, so below the horizon the
    environment shows a lit ground instead of nothing; without a ground plane a neutral albedo is used.
- [ ] **A6.3 — Dynamic environment lighting (R4). Owner: Claude.** Re-filter the sky into the diffuse and specular
  environment cubemaps as time changes, without hitches. A full filter costs about 100–700 ms depending on the
  quality preset (Phase 7), so filtering is spread over frames and done at a reduced size while time is moving,
  then refined when it stops.
  - **Tests:**
    - no frame spike above a stated budget while time is moving;
    - the converged result matches a full filter.
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
  filter input so the sun is not counted twice (the A3.1 rule).
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
