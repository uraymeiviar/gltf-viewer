# MSFS 2024 Reference Views: FSS E-Jets E195

**Document:** Reference Views Specification (Task A0.3)  
**Parent Journal:** [ASOBO Extensions Journal](ASOBO_EXTENSIONS_JOURNAL.md) · [Phase A0 Foundations](ASOBO_EXTENSIONS_PHASE_0.md)  
**Target Aircraft:** FlightSim Studio Embraer E-Jets E195 (`fss-aircraft-e195`)  
**Resolution:** 16:9 (`1600x900` baseline render; 1080p / 1440p / 4K MSFS captures)  
**Status:** 11 Views Baseline Rendered & Verified  

---

## 1. Overview & Purpose

This document establishes the official visual reference views for the FlightSim Studio E-Jets (E195) asset suite. These views serve as the ground truth calibration standard across the implementation of ASOBO glTF extensions in Glider (Phases A1–A7).

Each view is defined with:
- **Exact Glider CLI invocation:** Reproducible camera parameters (`--camera-target`, `--camera-yaw`, `--camera-pitch`, `--camera-distance`, `--select-node`) configured without `--isolated`, ensuring runtime texture search paths in `%APPDATA%/Glider/settings.ini` are active.
- **MSFS 2024 capture conditions:** Precise cockpit/drone camera framing, time of day, weather, and aircraft power state for human artists capturing simulator ground truth.
- **Baseline Glider renders:** Pre-rendered 1600×900 test frames stored in `build/views/<name>.png`.
- **Extension focus checklist:** The exact PBR and ASOBO material behaviors under test.

---

## 2. Reference Views Matrix

The 11 reference views comprise 8 cockpit interior views and 3 exterior views.

| # | View Name | Model | Glider Flags (copy-pasteable) | MSFS 2024 Camera & State | Baseline Render |
|---|---|---|---|---|---|
| **01** | `cockpit_main_displays` | Cockpit | `--select-node SCREEN_EICAS --camera-yaw 180 --camera-pitch -8 --camera-distance 0.75` | Instrument Cam 1 (MIP); Noon; Avionics ON | `build/views/cockpit_main_displays.png` |
| **02** | `cockpit_pedestal_throttles` | Cockpit | `--camera-target 0,1.12,11.45 --camera-yaw 180 --camera-pitch -50 --camera-distance 0.45` | Instrument Cam 4 (Pedestal); Noon; Cold & Dark | `build/views/cockpit_pedestal_throttles.png` |
| **03** | `cockpit_overhead_panel` | Cockpit | `--camera-target 0,2.13,11.20 --camera-yaw 180 --camera-pitch -50 --camera-distance 0.55` | Instrument Cam 6 (Overhead); Noon; Battery ON | `build/views/cockpit_overhead_panel.png` |
| **04** | `cockpit_glareshield_mcp` | Cockpit | `--camera-target 0,1.66,11.73 --camera-yaw 180 --camera-pitch -20 --camera-distance 0.40` | Instrument Cam 2 (FGCP/MCP); Noon; Cold & Dark | `build/views/cockpit_glareshield_mcp.png` |
| **05** | `cockpit_pilot_yoke` | Cockpit | `--camera-target 0.535,1.33,11.65 --camera-yaw 180 --camera-pitch -10 --camera-distance 0.45` | Pilot Eyepoint forward-down; Noon; Cold & Dark | `build/views/cockpit_pilot_yoke.png` |
| **06** | `cockpit_windshield_inside` | Cockpit | `--select-node WINDSHIELD_FL --camera-yaw 180 --camera-pitch -5 --camera-distance 0.85` | Pilot Eyepoint forward-left; 10:00 AM; Clear Skies | `build/views/cockpit_windshield_inside.png` |
| **07** | `cockpit_hud_hgs` | Cockpit | `--camera-target 0.49,1.80,11.47 --camera-yaw 180 --camera-pitch -5 --camera-distance 0.38` | Pilot Eyepoint at HUD level; 14:00; Clear Skies | `build/views/cockpit_hud_hgs.png` |
| **08** | `cockpit_interior_shadows` *(needs re-framing: the camera is inside geometry)* | Cockpit | `--camera-target 0.82,1.25,11.55 --camera-yaw 140 --camera-pitch -20 --camera-distance 0.60` | Captain left console / tiller; 15:30; Low Sun | `build/views/cockpit_interior_shadows.png` |
| **09** | `exterior_fuselage_nose` | Exterior | `--camera-target 0.0,0.8,13.5 --camera-yaw -45 --camera-pitch -10 --camera-distance 4.8` | Drone Cam nose quarter; 11:00 AM; Clear Skies | `build/views/exterior_fuselage_nose.png` |
| **10** | `exterior_windshield_outside` | Exterior | `--camera-target 0.0,1.80,12.8 --camera-yaw -25 --camera-pitch -15 --camera-distance 2.0` | Drone Cam above nose looking windshield; 12:00 | `build/views/exterior_windshield_outside.png` |
| **11** | `exterior_cabin_windows` | Exterior | `--camera-target -1.45,1.60,11.80 --camera-yaw 90 --camera-pitch -10 --camera-distance 2.5` | Drone Cam left fuselage side; 10:00 AM; Clear Skies | `build/views/exterior_cabin_windows.png` |

---

## 3. Detailed View Specifications

### View 01: `cockpit_main_displays`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --select-node SCREEN_EICAS --camera-yaw 180 --camera-pitch -8 --camera-distance 0.75 --dump-frame build\views\cockpit_main_displays.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Emissive screen backlighting (the `$SCREEN_*` materials: core glTF emissive, with `ASOBO_material_disable_motion_blur` and `ASOBO_material_shadow_options`) and the display glass in front of them (`MAT_SCREEN_GLASS`: blended, `ASOBO_material_glass_v2`, `ASOBO_material_draw_order`).
  - Screen bezel buttons, rotary encoders, and surrounding panel bevels (normal maps).
  - Screen surface anti-reflective coating vs matte display bezel plastics (ORM).
- **MSFS Capture Setup:**
  - **Camera:** Quickview / Instrument View 1 (Main Instrument Panel), centered on EICAS.
  - **Time & Weather:** 12:00 Local, Clear Skies (minimizes outside glare on displays).
  - **Aircraft State:** Battery ON, APU / GPU running, Avionics ON with standard flight deck displays active.

---

### View 02: `cockpit_pedestal_throttles`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0,1.12,11.45 --camera-yaw 180 --camera-pitch -50 --camera-distance 0.45 --dump-frame build\views\cockpit_pedestal_throttles.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Center pedestal throttle quadrant levers, thrust reverser triggers, flap selector gate, and speed brake lever.
  - ORM roughness and metallic separation: matte painted metal throttle casing, chrome lever shafts, rubberized throttle grip.
  - Normal map panel grooves, screw heads, detent indicators, and switch guards.
- **MSFS Capture Setup:**
  - **Camera:** Instrument View 4 (Center Pedestal / Throttle Quadrant).
  - **Time & Weather:** 12:00 Local, Few Clouds.
  - **Aircraft State:** Cold & Dark (throttles at IDLE, flaps UP, speed brake RETRACTED).

---

### View 03: `cockpit_overhead_panel`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0,2.13,11.20 --camera-yaw 180 --camera-pitch -50 --camera-distance 0.55 --dump-frame build\views\cockpit_overhead_panel.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Overhead systems panel (electrical, hydraulics, fuel, APU, pneumatic).
  - Annunciator lights, pushbutton backlit text (`DECAL_LIT` material: blended, `ASOBO_material_geometry_decal`, `ASOBO_material_draw_order`).
  - Engine fire discharge handles with red protective guards.
  - Panel separation bevels, toggle switch normals, and knurled knobs.
- **MSFS Capture Setup:**
  - **Camera:** Instrument View 6 (Overhead Panel), looking straight upward.
  - **Time & Weather:** 12:00 Local, Clear Skies.
  - **Aircraft State:** Battery 1 & 2 ON, Emergency Lights ARMED, APU Master ON (illuminating annunciator legends).

---

### View 04: `cockpit_glareshield_mcp`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0,1.66,11.73 --camera-yaw 180 --camera-pitch -20 --camera-distance 0.40 --dump-frame build\views\cockpit_glareshield_mcp.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Flight Guidance Control Panel (FGCP / MCP) and master warning/caution switch indicators.
  - High-contrast material transition: ultra-matte leatherette glareshield eyebrow (`roughness ~ 0.9`) versus semi-gloss metallic guidance panel faceplate (`roughness ~ 0.35`, `metallic ~ 0.8`).
  - Baro/minimums rotary encoders, pushbuttons, and speed/heading selection windows.
- **MSFS Capture Setup:**
  - **Camera:** Instrument View 2 (Guidance Panel / Glareshield).
  - **Time & Weather:** 12:00 Local, Clear Skies.
  - **Aircraft State:** Cold & Dark.

---

### View 05: `cockpit_pilot_yoke`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0.535,1.33,11.65 --camera-yaw 180 --camera-pitch -10 --camera-distance 0.45 --dump-frame build\views\cockpit_pilot_yoke.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Embraer signature M-shaped control ram's horn yoke (`SEAT-YOKE` material).
  - Yoke center checklist placard decal (`ASOBO_material_geometry_decal` / `CHECKLIST` material).
  - Dual split pitch trim switches, autopilot disconnect button, push-to-talk trigger, and leather grip stitching normal map.
- **MSFS Capture Setup:**
  - **Camera:** Pilot seat eyepoint, angled slightly downward toward Captain yoke.
  - **Time & Weather:** 11:00 AM, Few Clouds.
  - **Aircraft State:** Cold & Dark (control column centered).

---

### View 06: `cockpit_windshield_inside`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --select-node WINDSHIELD_FL --camera-yaw 180 --camera-pitch -5 --camera-distance 0.85 --dump-frame build\views\cockpit_windshield_inside.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Pilot forward-left windshield pane (`WINDSHIELD_FL`, `windshield` / `MAT_SCREEN_GLASS` material).
  - Interior glass reflections, wiper sweep arc line, and glass tint / edge refraction.
  - Windshield structural post frame, de-ice sensor wire elements, and compass mounting.
- **MSFS Capture Setup:**
  - **Camera:** Pilot seat forward view looking through windshield toward the horizon.
  - **Time & Weather:** 10:00 AM, Clear Skies (direct sun on side windshield producing specular highlights).
  - **Aircraft State:** Cold & Dark.

---

### View 07: `cockpit_hud_hgs`
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0.49,1.80,11.47 --camera-yaw 180 --camera-pitch -5 --camera-distance 0.38 --dump-frame build\views\cockpit_hud_hgs.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Head-Up Guidance System (HGS) combiner glass (`HGS_PILOT_COMBINER`, `MAT_SCREEN_GLASS`).
  - Transparent combiner glass plate optical characteristics: specular reflection, Fresnel dropoff, and skybox transmission.
  - Overhead folding stowage mechanism, pivot joints, and "PUSH" release lever.
- **MSFS Capture Setup:**
  - **Camera:** Pilot eye position aligned with HUD combiner glass plane.
  - **Time & Weather:** 14:00 Local, Clear Skies.
  - **Aircraft State:** HUD combiner deployed down; Avionics OFF/ON.

---

### View 08: `cockpit_interior_shadows`

> **Review note (Claude, 2026-10-24):** this render shows the camera inside the console geometry with no
> recognisable subject. Re-frame it (for example from the captain's seat toward the left console) before the team
> captures the MSFS side.
- **Model:** `E195-Cockpit-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Cockpit-LOD00.gltf" --window-size 1600x900 --camera-target 0.82,1.25,11.55 --camera-yaw 140 --camera-pitch -20 --camera-distance 0.60 --dump-frame build\views\cockpit_interior_shadows.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Flight deck interior cavities, ambient occlusion, and directional shadowing.
  - Captain left side console, nosewheel steering tiller handle (`PILOT_TILLER`), steering disconnect switch.
  - Oxygen mask box door, circuit breaker panel side recess, and life vest storage decal placard.
- **MSFS Capture Setup:**
  - **Camera:** Free/drone camera positioned near center pedestal looking forward-left toward Captain side console.
  - **Time & Weather:** 16:00 Local, Low Sun Angle (casting deep shadows across cockpit console floor and side cavities).
  - **Aircraft State:** Cold & Dark.

---

### View 09: `exterior_fuselage_nose`
- **Model:** `E195-Exterior-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Exterior-LOD00.gltf" --window-size 1600x900 --camera-target 0.0,0.8,13.5 --camera-yaw -45 --camera-pitch -10 --camera-distance 4.8 --dump-frame build\views\exterior_fuselage_nose.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Nose radome cone contour, lightning diverter strips, and nose curvature reflection.
  - Metallic pitot tubes, angle-of-attack (AOA) vanes, and warning decals ("REMOVE BEFORE FLIGHT" red streamers).
  - Forward cockpit window frames, nose gear well doors, and fuselage skin rivet normal maps.
  - Gloss white painted fuselage (`Fuselage` material) vs chrome/metal sensor probes (ORM).
- **MSFS Capture Setup:**
  - **Camera:** Drone camera 3/4 front-left view of nose section.
  - **Time & Weather:** 11:00 AM, Clear Skies.
  - **Aircraft State:** Parked on apron, gear down, chocks/pins inserted.

---

### View 10: `exterior_windshield_outside`
- **Model:** `E195-Exterior-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Exterior-LOD00.gltf" --window-size 1600x900 --camera-target 0.0,1.80,12.8 --camera-yaw -25 --camera-pitch -15 --camera-distance 2.0 --dump-frame build\views\exterior_windshield_outside.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Exterior cockpit windshield glazing (`MAT_WINDSHIELD_CABIN` with `ASOBO_material_windshield_v3`).
  - Exterior glass environment reflection (skybox and ground reflections across curved glass surface).
  - Heavy mechanical windshield wiper arms, rubber wiper blades, and eyebrow rivet seams.
- **MSFS Capture Setup:**
  - **Camera:** Drone camera elevated above aircraft nose, aimed down-forward into cockpit windshield panes.
  - **Time & Weather:** 12:00 Local, Few Clouds (highlights environment reflections on glass).
  - **Aircraft State:** Cold & Dark.

---

### View 11: `exterior_cabin_windows`
- **Model:** `E195-Exterior-LOD00.gltf`
- **Glider Command:**
  ```cmd
  build\bin\Release\Glider.exe "F:\new-fss-aircraft-ejet\msfs24-projects\fss-aircraft-e195\SimObjects\Airplanes\E195\model\E195-Exterior-LOD00.gltf" --window-size 1600x900 --camera-target -1.45,1.60,11.80 --camera-yaw 90 --camera-pitch -10 --camera-distance 2.5 --dump-frame build\views\exterior_cabin_windows.png --exit-after-dump
  ```
- **Purpose & Focus:**
  - Passenger cabin window cutouts and acrylic panes (`MAT_WINDSHIELD_CABIN` / `WINDSHIELD_CABIN_FR`).
  - Rubber gasket seals surrounding passenger windows vs painted aluminum fuselage skin.
  - Fuselage skin lap joints, vertical rivet columns, and adjacent service decal placards.
- **MSFS Capture Setup:**
  - **Camera:** Drone camera positioned outside left passenger cabin windows, looking perpendicular to fuselage wall.
  - **Time & Weather:** 10:00 AM, Clear Skies.
  - **Aircraft State:** Parked on apron.

---

## 4. Coverage & Gap Analysis

The 11 reference views comprehensively cover primary cockpit avionics, controls, interior shading, glass surfaces, and forward exterior fuselage. The following asset features are **not covered by the initial 11 views** and are noted for Phase A2 and Phase A3 test fixtures:

1. **Wing Decals & Stencils (`E195-Exterior-Wings-LOD00.gltf`):**
   - No-step walkway boundary lines, flap degree alignment markings, and emergency exit pathway arrows on the upper wing surface (`Decal_sheet` material).
   - *Plan:* Add dedicated wing surface view in Phase A2 when validating `ASOBO_material_geometry_decal` and decal draw order offsets.
2. **Wing-on-Fuselage Cast Shadows:**
   - Sharp directional shadowing cast by the wing root and engine nacelle onto the fuselage underbelly during low sun angles.
   - *Plan:* Add exterior shadow view in Phase A3 when implementing the directional sun light and cascaded shadow maps.
3. **Main Landing Gear (MLG) Complex Mechanism:**
   - Multi-link trailing arm, brake assemblies, hydraulic flex hoses, and retraction actuators.
   - *Plan:* Add MLG bay inspection view during Phase A1/A2 ORM & normal validation.
4. **Vertical Stabilizer & Livery Decals:**
   - Tail fin airline livery logo, registration text decals, and vertical stabilizer logo floodlights (`LIGHTS` material).
   - *Plan:* Add tail fin view when testing multi-layer livery draw orders in Phase A2.

---

## 5. Capture Instructions for the Team

Follow these standardized steps when capturing ground-truth reference screenshots in Microsoft Flight Simulator 2024:

### 5.1 Environment & Simulation Settings
- **Simulator:** Microsoft Flight Simulator 2024.
- **Aircraft:** FlightSim Studio E-Jets 195 (LOD00).
- **Location:** Standard flat apron (e.g. EDDF or KDEN remote parking) with no surrounding building shadows obstructing the aircraft.
- **Graphics Settings:** Ultra Preset, Anti-Aliasing TAA (or DLAA), Motion Blur OFF, Lens Flare OFF, Depth of Field OFF.
- **Aspect Ratio & Resolution:** 16:9 aspect ratio mandatory (e.g. `1920x1080`, `2560x1440`, or `3840x2160`).

### 5.2 Capture Workflow
1. **Disable UI Overlays:**
   - In DevMode, disable the toolbar and all on-screen UI markers (`Debug` → `Hide UI` or press `Backspace` if mapped).
2. **Set Weather & Time:**
   - Open Weather menu: select **Clear Skies** preset (or custom with 0 clouds, 100km visibility).
   - Set Time of Day as specified in the reference table above.
3. **Position Camera:**
   - **Cockpit Views:** Use instrument quickviews or Drone Camera with coordinates locked to match the framing in `build/views/<name>.png`.
   - **Exterior Views:** Use Drone Camera in free mode with fixed focal length (approx 50mm / default zoom).
4. **Pause Simulator:**
   - Press `Active Pause` (or pause via DevMode) so animations and camera jitter are frozen.
5. **Save Screenshot:**
   - Capture in lossless PNG format.
   - Store files outside the git repository in the shared reference folder:
     ```text
     MSFS_Reference\<view_name>.png
     ```
     *(e.g., `MSFS_Reference\cockpit_main_displays.png`, `MSFS_Reference\exterior_fuselage_nose.png`).*

### 5.3 Review & Verification Checklist
When comparing a Glider render against an MSFS reference capture:
- [ ] **Framing Match:** Main subject occupies the same relative screen area and angle.
- [ ] **Normals & Bevels:** Panel seams, rivet dents, and knob ridges display matching light/shadow polarity.
- [ ] **ORM Roughness:** Matte surfaces (glareshield, tires) do not show spurious specular reflections; metallic surfaces (switches, struts) exhibit accurate Fresnel highlight roll-off.
- [ ] **Decal Layering:** Placards and stencils sit flush without Z-fighting or erroneous alpha clipping.
- [ ] **Glass Transparency:** Window glass is subtle with soft specular reflection rather than opaque black or completely invisible cutouts.
