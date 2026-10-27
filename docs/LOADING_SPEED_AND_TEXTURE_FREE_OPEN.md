# Loading speed and opening without textures

Follow-up measured on 2026-10-27, starting from `99de26c`, on Windows with an NVIDIA
GeForce RTX 5080. These are local measurements with
filesystem/driver caches warmed by sequential runs, not cold-start guarantees.

## Opening without textures

**File → Open without texture ...** opens a glTF/GLB with meshes, animations, skins,
material definitions and their scalar/vector parameters. Image payloads are intentionally
omitted: no image-file searches/reads, image decoding/resizing, inspector previews or
model-image GPU allocations. Texture and image metadata/references remain inspectable.
Generated tangents needed only by omitted normal maps are skipped too; authored attributes
remain intact. Environment maps and render targets still consume VRAM normally.

The viewport states “Opened without textures - material values only”, and Statistics has
an Image loading row. This choice belongs to the individual open operation. Changing
quality does not turn textures back on. Normal Open, Open Recent, drag/drop and File → Close
use the normal textured path. No preference or new settings file is created.

`--without-textures` provides the same behavior for startup and `--reload-model`. During a
normal loading overlay, File also offers the new option for the one-entry replacement
queue. Both path and texture mode are carried to the next outer frame, so opening from a
menu cannot re-enter ImGui or mutate a model owned by a worker.

GLB container/shared binary buffers must still be read for geometry, and can include
embedded image bytes. “No image reads” means no separate extraction/read/decode of image
payloads; it does not promise that those bytes are absent from a shared geometry buffer.
Base64 image URIs also remain as metadata strings in a parsed JSON document.
The zero-image counters below count image-owned allocations, not the container buffer.
Texture-driven masks, normals and colours naturally disappear; authored factor-based
opacity, emission and material behavior remain.

## What changed for loading speed

The progress implementation previously allowed redraws every 8 ms, sampled memory after
every redraw, and inherited the requested VSync interval for loading swaps. Presentation
now runs at most every 50 ms, OS memory sampling every 250 ms, and event polling remains
independent at 8 ms checkpoints. VSync is disabled for loading swaps and restored by the
load guard. Existing 8 MiB upload chunks and allocation checks remain unchanged. Driver
calls and individual codec calls are still indivisible.

That scheduling change alone did **not** produce a reliable full-texture speedup in the
cockpit measurements. An experiment increasing upload batches from 8 to 32 MiB also did
not improve throughput and was reverted.

Source inspection found MikkTSpace tangent generation inside synchronous renderer mesh
preparation. Normal opens now prepare those CPU vectors before GL upload with at most four
standard-library workers. A conservative estimate of 256 temporary bytes per vertex and
one eighth of available RAM further limits concurrency. Each primitive still uses the
same MikkTSpace function and normal-map UV-set key; only scheduling changes. The GL thread
uploads the prepared vectors and releases unused prepared data after scene synchronization.
This estimate is a scheduling heuristic, not an OS memory reservation. Existing load error
handling applies if an allocation fails. Texture-only refreshes do not regenerate tangents.

The progress overlay reports a separate Tangent generation stage. `loadDecodeMs` now includes
this CPU preparation, so it cannot be compared alone with its earlier value. Compare the
combined import/preparation + renderer synchronization + validation time instead.

Two related quality-transition defects surfaced in focused checks and were fixed:

- Commands no longer reject every mesh merely because no environment map exists when
  IBL is disabled. Low and early Auto can render their textured/untextured debug preview.
- A default filesystem Path prints as `.` and has no filesystem owner. Re-filtering now
  requires an existing EnvironmentMap instead of testing whether its printed path is empty.
  This prevents a Low → Ultra transition from dereferencing that uninitialized path.
- EnvironmentMap instances have a generation in cached command signatures. Reusing an
  allocation address after re-filtering no longer preserves stale sampler/uniform pointers.

Whole-model opens release the previous model's GPU image mapping and cached draw commands
before memory preflight. Reusing Image addresses and encoded-map version numbers across
files could otherwise keep a previous file's uploaded texels. The same-process replacement
check initially exposed a different PMDG GPU payload after E195; after this fix, fresh PMDG,
File → Open from the cube, and replacement of E195 all upload exactly **3,577,442,260 bytes**.
Texture-only quality refreshes retain their existing mapping and update it in place.

## E195 cockpit before and after

The same cockpit, search folders, deterministic legacy quality, VSync off, 1600×900 window
and frame-12 viewport capture were used. Before and after rows average two sequential runs.
The final executable was verified once more afterward: 4.804 s combined loading work and
5.406 s to first frame. Its viewport PNG is **byte-identical** to the before capture.

| CPU wall time | Before | After | Without textures (one run) |
|---|---:|---:|---:|
| Import + CPU preparation | 3.133 s | 4.160 s | 0.101 s |
| Renderer preparation + GPU submission | 2.344 s | 0.660 s | 0.165 s |
| Validation | 0.035 s | 0.034 s | 0.035 s |
| Combined loading work | **5.512 s** | **4.854 s** | **0.300 s** |
| Application startup to first frame | 6.113 s | 5.456 s | 0.923 s |
| Progress redraws | 257.5 | 69.5 | 5 |
| External peak process working set | 3,743 MiB | 3,445 MiB | 331 MiB |
| External sampled dedicated GPU peak | 3,059 MiB | 3,059 MiB | 681 MiB |

Combined textured loading improved about **12%**. Tangent work moved out of the renderer
column into the CPU-preparation column; the renderer-column reduction alone is not the
overall speedup. The final prepared-tangent stage took about 0.93 s. Image reading/decoding
still took about 3.1 s and remains the largest cost for the textured cockpit.

Artifacts: `build/review/loading/progress_before{,2}_e195.*`,
`progress_final{,2}_e195.*`, `progress_verified_e195.*`, and
`without_textures_final_e195.*`. `.external.json` contains the exact command and peak-memory
sampling details; `.json` contains all stages. Peak values above are maxima across each
two-run group, not averages. Timing intervals are CPU wall/submission time, not GPU
execution time. Stage sub-timers can overlap: mip work is nested in uploads and progress
presentation can occur during CPU/upload stages.

## PMDG 777-300ER cockpit

Input: `F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf`.
The file has **1,337 meshes, 205 materials, 363 images and 53,692,436 bytes of geometry
buffers**. All 363 images resolved with `--search-path ../texture*`; no error images were
substituted. There is no before-change PMDG measurement, so no before/after speedup is claimed.

| CPU wall time / allocation | Protected textured path | Full-resolution Ultra | Without textures |
|---|---:|---:|---:|
| Import + CPU preparation | 2.530 s | 1.723 s | 0.305 s |
| Image read/decode/resize sub-stage | 2.297 s | 1.477 s | ~0 s |
| Renderer preparation + GPU submission | 0.740 s | 0.945 s | 0.163 s |
| Validation | 0.131 s | 0.137 s | 0.132 s |
| Combined loading work | 3.401 s | 2.805 s | 0.600 s |
| Application startup to first frame | 4.023 s | 3.411 s | 1.200 s |
| Model texture GPU payload | 1,172 MiB | 3,412 MiB | **0** |
| External peak working set | 3,627 MiB | 3,677 MiB | **470 MiB** |
| External sampled dedicated GPU peak | 1,852 MiB | 4,149 MiB | 707 MiB |

Each PMDG row is one local run. The protected path chose a 1024-pixel maximum and reduced
104 images. Ultra retained source sizes with **zero reductions**. DDS headers show 95
2048² images, nine 4096² images, and smaller images; the full decoded RGBA base payload is
about 2.51 GiB. PMDG's authored tangents meant the generated-tangent stage had negligible
work here. Its remaining costs are image conversion and GPU upload.

The protected path was slower than Ultra in this run because CPU resizing adds work, even
though it saves GPU memory. This is an observed tradeoff, not evidence that memory budgets
should be disabled generally. The current DDS path decodes compressed payloads to CPU
pixels before upload. Native compressed DDS GPU upload is a promising further optimization,
but was not implemented here: format/sRGB/normal-map correctness needs separate validation.

The initial auto-fit capture is distant because this asset has broad overall bounds.
Additional `--dump-frame` captures use the existing PMDG overhead camera coordinates and
were visually inspected: `pmdg_ultra_close.png` and `pmdg_without_close.png`. The full-resolution
capture visibly contains its authored textures; these close-up dumps are visual checks,
not additional timing rows.

PMDG artifacts are `build/review/loading/pmdg_{textured,ultra,without}_PMDG773ER_VC.*`.
Exact commands and external peaks are in their `.external.json` files.

### Saved-settings and actual File menu checks

The maintainer reported an approximately 90-second load, so the isolated numbers were also
checked against a private, byte-for-byte snapshot of the executable's current `settings.ini`:
Ultra, `../texture*`, 2503×1287 window and VSync on. The original settings file was unchanged
(SHA-256 checked). Capture tooling normally suppresses profiles for compatibility, so these
checks explicitly request the saved profile to preserve its rendering/image settings.

The actual File menu was driven by the UI harness. `GLIDER_TEST_OPEN_FILE` supplies the
file-dialog result only when `--ui-script` is active; the real menu action and deferred
load queue still run. Ordinary sessions always use the native file dialog.

| Current build, full-resolution Ultra | Per-open load wall time | Open to first model frame ready | Model texture GPU payload |
|---|---:|---:|---:|
| Start with PMDG | 2.636 s | 2.832 s | 3,577,442,260 bytes |
| File → Open PMDG from bundled cube | 2.670 s | 2.857 s | 3,577,442,260 bytes |
| File → Open PMDG after E195 cockpit | 2.587 s | 2.698 s | 3,577,442,260 bytes |

All image paths resolved, `texturesIntentionallyOmitted` was false, and all three runs had
2,691,470,592 bytes of decoded image data. These are warmed local results, not a reproduction
of the reported 90 seconds. The maintainer subsequently confirmed that a copied executable
without its search-path settings explained the fast **untextured appearance** they observed.
That does not establish the cause of the earlier 90-second observation, and no 90-to-3-second
improvement is claimed.

Interactive loads now log two separate events: import completion with image/tangent/upload/UI
costs, and the first model frame ready with shader compile and program link costs. Statistics
and JSON expose `lastLoadWallMs` and `lastLoadToFirstModelFrameMs`; these remain meaningful
when File → Open replaces a scene. `totalStartupMs` continues to mean initial application
startup and must not be used as the latency of a later open. First-model-frame timing measures
CPU readiness before buffer swap, not GPU execution completion. Shader linking has its own
stage because drivers can do substantial compilation work during program linking.

Artifacts and exact commands: `build/review/loading/interactive/`. The earlier mismatched
replacement payload is preserved in `*.before_texture_release.json`. The standalone
`scripts/check_interactive_loading.py` reproduces this focused comparison without changing
the real settings file, purging caches or copying executables.

## Focused checks and other requested assets

`scripts/check_open_without_textures.py` passed: external/data-URI/GLB/missing-image cases,
material colour rendered, zero model image payloads/VRAM, queued texture-free open, normal
reopen restoring textures, and Low → Ultra keeping images omitted. The File menu was also
captured and inspected. Every reported automation GL-error total was zero. The new
`reloadwithouttextures` UI-script command exercises the same outer-frame queue as the menu.
The literal underscore probe-tag handling was corrected so tags such as
`file.open_without_textures` are not converted into spaces; legacy space aliases still work.
UI reload commands now preserve spaces in aircraft file paths.
These are focused checks; regression suites were **not run**, per the user's instruction.

Additional texture-free opens succeeded with all image-owned CPU/GPU payload counters zero:

| Asset | Import + CPU preparation | Renderer preparation/upload | Validation |
|---|---:|---:|---:|
| E195-Cabin-LOD00 | 0.219 s | 0.033 s | 0.019 s |
| E195-Exterior-Wings-LOD00 | 0.149 s | 0.098 s | 0.012 s |
| Generated 3.6M-vertex / sixteen-8K-image fixture | 1.483 s | 0.042 s | 0.001 s |

The cabin/wings inputs use the user-specified `msfs-projects` tree. Their reports are
`cabin_without_textures_e195.*` and `wings_without_textures_e195.*`; the heavy report is
`heavy_without_textures_heavy.*`. These rows have no matched new textured measurements.

## Reproduction and implementation map

```powershell
cmake --build build --config Release --target Glider
& C:\ProgramData\anaconda3\python.exe scripts/check_open_without_textures.py
& C:\ProgramData\anaconda3\python.exe scripts/check_interactive_loading.py
& C:\ProgramData\anaconda3\python.exe scripts/measure_loading.py --label cockpit --model e195
& C:\ProgramData\anaconda3\python.exe scripts/measure_loading.py --label cockpit_without --model e195 --without-textures
& C:\ProgramData\anaconda3\python.exe scripts/measure_loading.py --label pmdg_ultra --model-path 'F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf' --search-path '../texture*' --quality-preset ultra
& C:\ProgramData\anaconda3\python.exe scripts/measure_loading.py --label pmdg_without --model-path 'F:\msfs-addons\pmdg-aircraft-77w\SimObjects\Airplanes\PMDG 777-300ER\model.300ER\PMDG773ER_VC.gltf' --search-path '../texture*' --without-textures
```

- `source/imgui/application/Menus.cpp:642` — new File menu operation.
- `source/imgui/application/Scene.cpp:54` — per-open mode/queue; progress presentation
  scheduling at line 162; GPU image release at line 126; CPU preparation at line 332.
- `source/imgui/application/Frame.cpp:20` — quality refresh respects intentional omission.
- `OpenGL-Renderer/Tools/tools/gltf/source/Gltf.cpp:632` — skip image work, retain metadata.
- `OpenGL-Renderer/renderer/source/gltf/GltfRenderLayer.cpp:818` — skip model samplers;
  line 1500 skips generated tangents; line 2294 releases model image mappings; line 2303
  prepares normal-open CPU tangents in workers.
- `source/imgui/ElementInspector.cpp:162` — inspection cannot trigger omitted-image reads.
- `OpenGL-Renderer/renderer/source/CommandMapping.cpp:730` — no-environment rendering;
  line 91 includes EnvironmentMap generation in cache signatures.
- `source/imgui/RenderQuality.cpp:650` — refilter only a loaded environment.
- `source/imgui/Profiling.cpp` — omission, progress UI cost and CPU/GPU image payload counters.
- `source/imgui/StatisticsPanel.cpp:338` — visible omission/progress-cost state.

`startup.residentModelTextureBytes` counts mapped model-image payloads, excluding IBL,
render targets and driver padding. `encodedImageBytes` and `decodedImageBytes` count retained
image-owned CPU bytes. `loadingProgressUiMs` measures progress presentation, sampling and
swaps; ordinary event-only polling is outside that timer.
