# Loading and adaptive quality

Glider keeps the window responsive while importing assets and protects texture allocations
with a memory preflight. Auto quality is enabled by default for interactive use. It changes
effective rendering settings, while the configured user preferences remain separate.

## Ownership and progress

`Application::loadGltfModel` owns a load to completion. Parsing, file resolution, image
preparation/decoding and accessor deinterleaving run in an `std::async` worker. Image
decoding uses a pool of at most sixteen standard-library workers; the existing weighted
decode lease bounds estimated temporary allocations. Validation runs in another joined
future after renderer synchronization. Workers never access ImGui or issue GL commands.
Console writers are serialized, and the loading UI does not read the model or console.

The main thread services GLFW and renders an ImGui progress overlay while waiting for the
worker. Loading owns a separate ImGui context/font atlas and GL backend; GLFW callbacks are
temporarily attached to that context and restored afterward. Loading text therefore cannot
change the main UI's glyph-atlas packing or docking work-area insets. Input keys are cleared
on return so a key released during loading cannot remain held in the main UI.
The overlay shows the model path, stage, completed/total items, percentage and elapsed
time. File → Open / Ctrl+O remains available, placing the latest request in a one-entry queue.
Regular menu requests are also queued until the next outer frame. This prevents re-entering
an unfinished ImGui frame or mutating a model still owned by a worker. Closing the window
joins outstanding CPU work before releasing the GL context; a codec call is not forcibly
interrupted. Resize callbacks cannot recursively enter the load.

GL uploads stay on the context-owning main thread. Texture rows and large buffers transfer
in chunks of at most 8 MiB. At complete work-item/chunk boundaries a soft 8 ms budget yields
to another progress frame. GL allocation and individual driver/compiler calls are indivisible,
so this is a cooperative budget, not a guarantee of an 8 ms maximum frame interval. The
progress renderer preserves the GL framebuffer/viewport, and ImGui restores its render state.
Loading frames are excluded from ordinary capture frame counts and UI-script execution.

## Memory preflight and failure handling

On Windows, `MemoryInfo.h` dynamically loads the OS DXGI API, chooses the largest hardware
adapter matching the OpenGL vendor, and queries `IDXGIAdapter3::QueryVideoMemoryInfo` for
the process-local budget and usage. Same-vendor multi-GPU configurations are a limitation:
OpenGL 4.1 does not provide a portable DXGI adapter LUID. If DXGI is unavailable, Glider
tries `GL_NVX_gpu_memory_info`, then `GL_ATI_meminfo`. Otherwise GPU capacity is explicitly
unknown. `GlobalMemoryStatusEx` supplies available system RAM.

Images first retain their encoded data. Header preflight estimates a full RGBA mip chain,
or the actual compressed KTX2 payload. The texture budget is the smaller of 70% of available
VRAM and one third of available RAM. If VRAM is unknown, only the RAM budget is used. The
reserve covers geometry, render targets and driver overhead. A common power-of-two maximum
dimension is reduced until the estimate fits (down to 64). An Unlimited preset
(`textureMaxSize = 0`, used by Ultra and Auto's target) skips this estimate-based reduction
entirely, so textures upload at authored resolution and only the per-texture allocation
fallback still applies. PNG/JPEG and other supported STB
8/16-bit images are resized on CPU before copying into the resident Image object. Aspect
ratio is preserved. Dimensions and image URIs are logged; source files are unchanged.

The temporary decode budget defaults to at most 4 GiB and one quarter of free RAM (32 MiB
floor); `--decode-budget-mib` retains its existing explicit override. A large individual
decode can run alone under the weighted lease. The estimate is a planning bound, not an OS
reservation: images used in both colour spaces may need an additional GPU copy. DDS and
STB-decoded images are resized before the resident copy is made; compressed KTX2 uploads trim
larger authored mip levels when a level at or below the cap exists. A compressed chain without
a small enough authored level cannot guarantee the cap (see the follow-up section). Actual GL
allocation
errors are still checked. A failed texture allocation gets a complete 1×1 texture; failed mip
generation retains the base level. Buffer/allocation exceptions stop the load safely and
replace the partial scene with an empty scene. A lost driver context can require closing the
window. No application can guarantee recovery from OS process termination or a driver crash.

## Texture-only refresh

Changing the texture maximum of the active preset re-decodes and re-uploads images without
reopening the model: selection, animation, camera and edited material values survive. The
last presented frame stays on screen and a progress strip is drawn along the bottom edge,
where the status bar sits, instead of switching to the full loading overlay. The full overlay
is reserved for a fresh glTF load. The strip reports the same stage/percentage information
and disappears when the upload finishes. Texture-only budgets count the current resident GL
texture payload as reclaimable, so a refresh does not artificially shrink the configured
High/Ultra memory target.

## Adaptive controller

Viewport Settings → Quality has an Auto quality checkbox and status. The viewport also
shows the active status; every level/status change is logged. The portable `settings.ini`
(`[RenderQuality]`) contains the user's target and the Auto quality switch, never the
temporary automatic reductions.

| Level | Effective additions toward configured quality |
| --- | --- |
| 0 | IBL contribution off; filter allocation 256 cube / 32 irradiance / 256 LUT / half float / at most 256 samples; MSAA off; FXAA; SSR and bloom off; render scale at most 75% |
| 1 | Enable the small IBL contribution |
| 2 | Raise IBL cube to at most 512 |
| 3 | Restore configured IBL filter fields |
| 4 | Restore configured AA and bloom |
| 5 | Enable configured SSR with Low quality |
| 6 | Restore configured SSR quality and render scale |

At least twelve rendered samples and a 400 ms window are required per decision. CPU wall
time and nonblocking GL timestamp samples bound frame time; idle event waits and capture
readback/encoding are excluded. Under 28 ms permits another step. At 28–50 ms the controller
holds; over 50 ms steps back down, with a five-second retry delay. Less than 256 MiB free
VRAM, a process budget at or below 1 GiB, or less than 512 MiB free RAM limits the controller.
The next IBL step also reserves its incremental estimated allocation plus 128 MiB before
raising filter sizes. Periodic short sampling bursts keep monitoring a stationary model without invalidating the
scene's topology or repeatedly uploading geometry. TAA can settle between real changes.
When memory limits the target but at least 128 MiB VRAM remains, level 1 retains the already
allocated small IBL contribution so assets without punctual lights stay visible.

Explicit CLI IBL, render-scale, AA, SSR and bloom flags pin their group. UI edits to AA/SSR,
IBL controls or render scale are detected/pinned; changing the requested bloom state also
pins bloom. Pinning is session-wide for the group. Auto quality off immediately restores
the configured effective settings. Texture memory protection is independent: freeing memory
and reloading restores authored resolution when it fits. Enabling Auto quality from its UI
checkbox also works in a session initially suppressed by automation.

`--isolated`, `--no-persist`, window/viewport captures, `--fixed-frame-time`, and UI scripts
suppress adaptation unless `--auto-quality on` is explicit. This suppression is not saved
over the user's preference. Informational loading telemetry stays in stderr/profile JSON
instead of changing the old status-bar Info count during isolated deterministic captures.

## Optimizations and instrumentation

The old loader already decoded images in parallel, but blocked the main thread while waiting.
This change retains parallel work, bounds worker count, and removes an encoded-data copy in
the new preparation path. Texture upload already generated mips; material setup no longer
generates them a second time. `updateScene` also consumes the accessor upload already done
by the immediately preceding `updateGltf`, instead of uploading every accessor twice.
Standalone `updateScene` still refreshes buffers.

`--profile-report` adds `startup.loadStagesMs` (JSON conversion, buffer read, image read/decode,
buffer/texture upload, mip submission, validation and renderer shader compile), texture
budget/estimate/maximum/reductions, progress frame count, sampled process-local DXGI peak
usage, and adaptive status/effective settings. The existing environment-init/filter and
first-frame fields remain available. Stage timings are CPU wall/submission times; mip
generation is asynchronous and its timer is not a GPU execution measurement. Upload timers
include cooperative progress rendering where it occurs inside an upload. Stage sums are
not additive: mip submission is nested in upload and driver work may complete in later calls.
DXGI and the external Windows PDH sampler measure different accounting scopes; neither is a
GPU memory allocation trace.

## Editable profiles and telemetry follow-up

See [Quality presets, Settings and runtime telemetry](QUALITY_PRESETS_AND_TELEMETRY.md) for the newer viewport selector, editable preset definitions, live texture resizing, Settings tabs, wildcard directories and memory/timing displays. [The continuation journal](QUALITY_CONTROLS_HANDOFF.md) records which follow-up checks have completed. The original measurements and validation results predate that UI follow-up. At the user's request, do not automatically run regression suites for the follow-up.

## Reproducible checks

Use the configured Release executable directly. Do not copy or rename it or the baseline.

```powershell
& C:\ProgramData\anaconda3\python.exe scripts/make_heavy_fixture.py
& C:\ProgramData\anaconda3\python.exe scripts/validate_loading.py --measure --cabin --wings
& C:\ProgramData\anaconda3\python.exe scripts/validate_hdr.py
& C:\ProgramData\anaconda3\python.exe scripts/validate_asobo_glass.py
& C:\ProgramData\anaconda3\python.exe scripts/validate_asobo_decal.py
& C:\ProgramData\anaconda3\python.exe scripts/validate_settings_storage.py
```

The loading suite compares isolated viewport PNG bytes with `build/baselines/3176a16.exe`,
checks the real 3.6M-vertex/sixteen-8K-image fixture, adaptive ramp-up, a 1 GiB reported VRAM
cap, texture/buffer allocation fallback branches, queueing during decode, explicit override
pinning, and optionally the maintainer's cabin/wings models. All assets/logs/reports are in
`build/review/loading/`; the fixture is in `build/review/heavy/`. `measure_loading.py` also
samples peak process working set and Windows PDH dedicated GPU usage for unmodified baseline
executables. Run timing comparisons sequentially, with no other harnesses active.
