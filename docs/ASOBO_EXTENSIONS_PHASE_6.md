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

**Exit gate:**
- Moving time from dawn to night is seamless (frame budget met).
- Reference screenshots at noon, sunset and night are reviewed.
- Files without ASOBO extensions are unchanged unless time-of-day mode is explicitly enabled for them.
