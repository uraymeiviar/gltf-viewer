# Phase 8 — Optional Architectural Work

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete, uncommitted ([§12.61](#section-12-61)). Entry measurements are in [§12.57](#section-12-57). P8.5 PASS ([§12.58](#section-12-58)). P8.1 PASS for uniform submission, with skin storage REJECTED ([§12.59](#section-12-59)). P8.3 PASS ([§12.60](#section-12-60)). P8.2 DEFERRED and P8.4 REJECTED ([§12.57](#section-12-57)). A follow-up for the three-viewport E195 use case is in [§12.62](#section-12-62).

**Scope decision (user, 2026-10-23):** P8.1, P8.3 and P8.5 are implemented even where the representative assets show little gain, because some assets have thousands of objects. A change is kept unless it is shown to be slower or to change the image. P8.2 and P8.4 are not implemented ([§12.57](#section-12-57)).

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-10"></a>

### 5.10 Phase 8 — Optional Architectural Work (R3–R4)

**Entry:** measured bottleneck remains after relevant earlier work; do not implement for completeness.

- [x] **P8.1 — UBOs ([§3.10](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-10), [§3.17](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-17), R4).** After P4.5/P4.6, migrate camera/environment/light data one
  block at a time; evaluate skin storage separately after P4.7. Query limits, verify layouts and retain
  a large-skin fallback. Do not require SSBOs or raise the OpenGL 4.1 floor.
- [ ] **P8.2 — Dynamic debug mode ([§3.5](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-5), R4).** Compare first-switch shader compilation, permutation
  count and steady-state fragment cost. Validate every supported debug mode and multi-viewport mix;
  fewer variants may increase shader work and is not sufficient evidence by itself.
- [x] **P8.3 — Instancing/static batching ([§3.9](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-9), R4).** After P4.6/P5.5, start with compatible opaque
  static geometry. Preserve transforms, bounds, transparency order and per-node picking identity.
- [ ] **P8.4 — Shader binary cache ([§4.4](RENDERER_OPTIMIZATION_REFERENCE.md#section-4-4), R3).** Capability-gate binary support, include driver/program
  inputs in keys, handle stale/corrupt data, and always retain source compile fallback. Confirm the
  optional writable-cache policy before adding disk persistence; the executable must run without it.
- [x] **P8.5 — CPU/hybrid picking ([§3.7](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-7), R4).** Only if P5.1/P5.2 leave a measured problem. Define and
  test equivalence for alpha masks, skin/morph deformation, depth order and selection identity.

**Exit gate:** task-specific correctness suite and repeatable gains justify the extra architecture.
Reject or defer complexity whose benefit is not demonstrated on representative workloads.

---


---

## 12. Implementation Records

Common setup for every record below. Base: `51266d3` (Phase 7 plus the transmission fix). The baseline binary built from it is `build/p8/Glider_base.exe`. Test hardware is an NVIDIA RTX 5080 (driver 616.92) running OpenGL 4.1, with a 1280×800 window and the 734×696 viewport.

Large-drawable fixtures come from `scripts/generate_many_drawables_fixture.py` (grids of 40² = 1,600 and 100² = 10,000 quads that share four meshes and four materials). The real-asset control is the FSS E195 cockpit (`E195-Cockpit-LOD00.gltf`: 1,314 mesh nodes, 894 unique meshes, 32 materials, 14 skins). It is not part of the repository; harnesses skip it when it is absent.

<a id="section-12-57"></a>

### 12.57 Phase 8 Entry Measurements (2026-10-23)

Each task's premise was re-checked against the current source, and its bottleneck was measured before any change. Runs used `--disable-redraw-skip`, AA off and vsync off, with the median of 3 runs. Since P5.3 these costs apply only while the view changes (orbit, animation); idle frames redraw nothing.

**Cost of a redrawn frame:**

| Workload | Drawables | CPU frame | draw.submit | GPU |
|---|---:|---:|---:|---:|
| S0, S5, 2CylinderEngine, CesiumMilkTruck | 1–116 | 0.48–0.73 ms | ≤0.43 ms | ≤0.12 ms |
| E195 cockpit | 1,315 | 6.1 ms | 3.2 ms | 2.1 ms |
| 1,600 grid | 1,601 | 5.7 ms | 3.3 ms | 1.8 ms |
| 10,000 grid | 10,001 | 51 ms | 33 ms | 32 ms |

Each draw makes about 29 GL calls. About 12 of them re-upload per-frame values on every draw: camera (6), environment (5) and exposure (1). Every uniform and sampler is also looked up by name in the program's `std::map<std::string>` on every draw.

| Task | Premise today | Measured | Decision |
|---|---|---|---|
| P8.1 UBOs | Per-frame uniforms are uploaded per draw, with a string lookup per uniform | Up to the 33 ms submit at 10k drawables | Implement (scope decision) |
| P8.1 skin storage | Joint matrices are a float texture | 64 skins × 64 joints: GPU 0.11 ms for all draws; the 3.4 ms CPU cost is animation sampling and joint math, which storage does not change | REJECTED |
| P8.2 dynamic debug mode | Per-viewport define conflicts are gone: the P4.6c command cache is keyed by camera, so viewports in different modes keep separate entries | The first switch to a mode compiles new programs: 0.4 ms per program with the driver's shader cache warm, 11–18 ms cold (once per driver cache) | DEFERRED: a uniform-driven mode adds per-pixel cost to remove a one-time hitch |
| P8.3 instancing | Every primitive is its own draw | As P8.1. E195 has 894 unique meshes for 1,314 mesh nodes | Implement (scope decision) |
| P8.4 shader binary cache | — | The NVIDIA driver already caches binaries. Shader-compile frames cost 0.3–2.4 ms on real models and 30–50 ms for an 81-program fixture; an app cache cannot speed up the first, cold run | REJECTED |
| P8.5 hybrid picking | A click redraws every drawable into the 1×1 target | Click-frame picking: 0.08 ms (S5), 7.5 ms CPU + 4.1 ms GPU (1,600), 53 ms + 34 ms (10,000), 7.0 ms + 3.1 ms (E195) | Implement |

**Found while measuring:** transmissive materials never received their opaque-scene texture, which caused `GL_INVALID_OPERATION` on some scenes and wrong refraction on all of them. This was fixed separately in `51266d3`.

<a id="section-12-58"></a>

### 12.58 P8.5 — Hybrid Picking (2026-10-23)

**Status:** PASS.

#### 1. Change
- `GltfRenderLayer::cullToClip` removes from a drawable list the static primitives whose POSITION bounds lie entirely outside a clip volume. It uses the P5.5 test (`getStaticPositionBounds`, `boxOutsideClip`).
- On a click frame, `main.cpp` calls it with the picking projection (pixel window × scene projection, jittered under TAA) times the view matrix, before the 1×1 picking pass.
- **What is always kept:** skinned, morphed and bound-less primitives, plus drawables the reverse mapping does not know (cube map, transmission copies). The GPU still decides the pick among every primitive that can cover the pixel.
- **Why the result cannot change:** a primitive whose bounds miss the pixel's frustum can neither cover that pixel nor hide one that does.
- `--disable-pick-culling` restores the P5.2 path as a same-binary reference. The UI snapshot reports `pickCulled`.

#### 2. Hypothesis and thresholds (declared before the change)
- **Selection:** every click selects the same node, with the same camera and byte-identical screenshots, as the baseline and as `--disable-pick-culling`.
- **Cost:** click-frame picking CPU and GPU time fall on the large fixtures, with no regression beyond 5% and noise on a small control.

#### 3. Equivalence
`python scripts/validate_p8_5.py --only equivalence`: PASS. Every case ran on the baseline, on `--disable-pick-culling` and on the candidate.

| Case | Clicks | Drawables culled |
|---|---:|---:|
| 1,600 grid (with outer rows and columns) | 165 | 42,139 |
| 10,000 grid | 49 | 489,958 |
| Production fixture from behind, AA off / MSAA 4 / TAA, with 1-px edge steps | 108 each | 230–234 |
| Orthographic | 84 | 242 |
| Wireframe mode | 396 | 772 |
| AlphaBlendModeTest (textured masks) | 255 | 2,057 |
| Skinned Fox during playback | 112 | 0 (skinned: never culled) |
| Animated hierarchy, playback and wheel zoom | 67 | 261 |
| Two split viewports | 68 | 124 |
| E195 cockpit | 352 | 459,617 |

**Harness fix:** the P5.2 edge cases spaced clicks with wall-clock waits. Under TAA that made the screenshot differ between two runs of the same baseline binary, because the frame count between clicks varied. The P8.5 harness spaces them by 12 fixed frames instead, and two baseline runs are then byte-identical.

#### 4. Measurement
`python scripts/validate_p8_5.py --only benchmark`: 5 alternating pairs, 30 clicks per run, medians per run.

| Fixture | Picking CPU | GPU picking pass | Click-frame CPU |
|---|---|---|---|
| 1,600 grid | 7.53–8.19 → 0.41–0.78 ms (−91.4%, 5/5) | 3.91–4.91 → 0.004 ms (−99.9%) | 15.2–16.6 → 5.4–10.7 ms (−52.8%) |
| 10,000 grid | 50.8–54.0 → 4.7–5.0 ms (−90.6%, 5/5) | 33.7–36.0 → 0.004 ms (−100%) | 104–110 → 56–59 ms (−46.0%) |
| Production fixture (control) | 0.04–0.10 → 0.02–0.03 ms | 0.011 → 0.004 ms | within noise (+2.1%, noise 52%) |

**What the remaining cost is:** a click frame is now about the cost of an ordinary redrawn frame. The 5 ms of picking CPU at 10,000 drawables is the bounds test and the picking-layer build.

<a id="section-12-59"></a>

### 12.59 P8.1 — Uniform Submission and Uniform Blocks (2026-10-23)

**Status:** PASS for camera, environment and exposure data. Skin storage was REJECTED with the entry evidence ([§12.57](#section-12-57)). Lights stay individual uniforms: none of the measured assets has punctual lights.

Each substep was measured against the previous one with `python scripts/validate_p8_1.py --ref-exe <previous> --label <substep>`. The equivalence part compares viewport captures and UI-script results, which must be byte-identical:
- Ten models and fixtures in four AA modes.
- Wireframe, orthographic and forced double-sided views.
- Playback of Fox, the animated hierarchy and the pointer fixture.
- Two viewports and a click grid.

The benchmark runs 5 alternating pairs of 120 warmup and 300 measured frames, with every frame redrawn.

#### 1. P8.1a — Bindings resolved once
- **Before:** every uniform and sampler of every draw was looked up by name in the program's `std::map<std::string>`.
- **After:** `CommandMapping::makeCommand` resolves the bindings when it builds a command (the P4.6c cache keeps the command) and drops uniforms the program does not use. `CommandExecutor` uses the stored bindings, and commands built elsewhere still look the names up. Sampler positions are unchanged because they are texture units.
- **GL calls:** unchanged; uniforms the program did not use were never committed.

| Workload | draw.submit | CPU frame |
|---|---|---|
| 10,000 grid | 33.8–41.3 → 25.3–30.8 ms (−25.1%, 5/5) | −17.7% (5/5) |
| 1,600 grid | 3.1–4.6 → 2.7–3.2 ms (−15.2%, 4/5) | −8.6% |
| E195 | 2.9–4.6 → 1.9–3.2 ms (−29.5%, 4/5) | −3.5% |
| 2CylinderEngine | 0.31–0.35 → 0.18–0.23 ms (−31.4%, 5/5) | −7.9% |

Equivalence: 61/61 byte-identical.

#### 2. P8.1b — CameraBlock
- **Shaders:** `camera_block.glsl` declares a std140 `CameraBlock` (view, projection, view-projection, camera position) under the former uniform names. `pbr.frag`, `primitive.vert` and `cubemap.vert` include it at top level, because `ShaderCache` resolves includes only one level deep. Gizmo and grid shaders keep plain uniforms.
- **Programs:** `gl::Program` binds the block at `kCameraBlockBinding` (12) with `glUniformBlockBinding`, and leaves block members out of its default-uniform map.
- **Upload:** `CommandGenerator::bindFrameBlocks` copies the camera's own uniform bytes to their std140 offsets. It uploads only when they change, and binds the block. `Renderer::onDrawScenes` calls it right before running a generator's commands, so draws read the camera when they did before. Uploading in `update()` instead would leave a later draw with stale values, for example a redraw after the pick projection was restored.
- **Result:** 2 fewer GL calls per draw. Throughput was within noise everywhere (−1% to −4%, noise 5–15%), and the GPU pass timers were mixed: the 1,600 grid's median rose 1.64 → 2.05 ms with an unchanged p10, and E195 fell 2.0 → 1.25 ms. In these CPU-bound scenes the pass timers also count time the GPU waits for commands. The camera block was kept to be judged together with the environment block (next).

#### 3. P8.1c — EnvironmentBlock
- **Block:** `environment_block.glsl` holds the environment rotation (mat3 as three 16-byte columns), intensity, blur, mip count and exposure. It is included by `pbr.frag`, `picking.frag`, `cubemap.frag`, `cubemap.vert` and `gizmo_material.frag`.
- **Shared includes:** `textures.glsl`, `ibl.glsl` and `tonemapping.glsl` no longer declare these uniforms. The binding point is `kEnvironmentBlockBinding` (13).
- **New scenario flags** `--hdr-rotation <deg>` and `--exposure <ev>`: the default identity rotation cannot reveal a column-layout mistake. Against a P8.1a binary built with only these flags, captures at 73°/+1.3 EV and 200°/−0.7 EV are byte-identical on the cube, the production fixture and 2CylinderEngine.

**Camera and environment blocks together, against P8.1a:**

| Workload | GL calls/frame | GPU (median, 5 pairs) | Frame interval |
|---|---|---|---|
| 1,600 grid | 46,042 → 36,438 | −2.6% (5/5) | −6.2% (5/5) |
| 10,000 grid | 287,542 → 227,538 | −2.6% (5/5) | +1.7% (2/5, noise 13.8%) |
| E195 | 53,463 → 45,575 | −15.1% (5/5) | +0.3% (2/5, noise 16.5%) |
| 2CylinderEngine | 3,256 → 2,562 | −2.6% (5/5) | −13.7% (5/5) |

Equivalence: 61/61 byte-identical. No workload was slower beyond its noise, and GPU time improved in every pair, so both blocks are kept under the scope decision.

#### 4. Notes
- **Latent binding issue:** `Program` binds other uniform blocks at their block index without calling `glUniformBlockBinding`, which is only correct for block 0. No current shader uses another block. The per-frame blocks set their bindings explicitly.
- **Shader loader quirk:** while probing shaders, a `//` comment after code on the same line made rendering fail with nothing logged. The comment-only edits used in this phase avoid the pattern.

<a id="section-12-60"></a>

### 12.60 P8.3 — Instancing (2026-10-23)

**Status:** PASS for instancing. Static batching (merging different meshes into one buffer) was not implemented: instancing already removes the repeated draws, and batching would change buffer ownership, picking identity and bounds handling.

#### 1. Change
- **Grouping:** after `CommandMapping::update`, `mergeInstances` groups the opaque-pass draws that share material, program, camera, index buffer and range, vertex attributes (same buffers, offsets and formats), mesh defines, topology, culling, winding and polygon mode. A group of two or more becomes one `glDrawElementsInstanced`/`glDrawArraysInstanced` call.
- **What is never merged:**
  - skinned or morphed meshes, meshes with uniform blocks, non-`primitive.vert` meshes and transmissive materials;
  - the transparent pass, which needs back-to-front order;
  - picking copies, which have one material each;
  - transmission duplicates (pass `Opaque2`).
- **Per-instance data:** `primitive.vert` with `USE_INSTANCING` reads `a_instanceModelMatrix` and `a_instanceNormalMatrix` from a per-instance stream (divisor 1) in place of the uniforms. The stream holds the meshes' own `u_ModelMatrix` and `u_NormalMatrix` bytes, so transforms are bit-identical. The fragment shader reads `u_ModelMatrix` only for transmission, which is never merged.
- **Draw order:** the group is drawn at its first member's slot and sorted by its nearest member. Members keep their relative order within the instanced draw.
- **Caching:**
  - Each group keeps its instanced command, VAO and instance buffer. The command is rebuilt when its first member's command is rebuilt (tracked by a revision number), and the buffer is re-uploaded only when its contents change. Unused groups are evicted like command-cache entries.
  - Eligibility, the group key and the matrix uniform pointers are computed once, when a member's command is built. A first version recomputed them every frame and cost 3.6 ms of `commands.generate` at 10,000 drawables.
- **Plumbing:** `gl::GL` gains `drawArraysInstanced`, `drawElementsInstanced` and `vertexAttribDivisor` (OpenGL 3.3, within the 4.1 floor). `--disable-instancing` is the same-binary reference, and the UI snapshot reports `instancedDraws` and `instancedMembers`.

#### 2. Hypothesis and thresholds (declared before the change)
- **Images and selection:** byte-identical images and identical selections, against the P8.1c binary and against `--disable-instancing`.
- **Cost:** fewer draw calls and lower submission and GPU time on repeated-mesh workloads, with no regression beyond 5% and noise on any workload.

#### 3. Equivalence
- **`python scripts/validate_p8_3.py`: PASS (65 checks).** Each case ran on three binaries: P8.1c, the candidate with `--disable-instancing`, and the candidate. The `--disable-instancing` run merged nothing, and the candidate merged draws in every case except the production fixture, which has no repeated meshes.

| Case | Instanced draws / merged drawables (last frame) |
|---|---|
| 1,600 grid: AA off, TAA, orthographic, wireframe mode, wireframe overlay with forced double-sided | 3 / 1,200 |
| 1,600 grid close up (most culled) | 3 / 48 |
| 1,600 grid with one opaque node hidden | 3 / 1,199 |
| Leaf-animated and root-animated grids during playback, at two times and after stop | 3 / 1,200 |
| Click grid (selection) | 3 / 192 |
| Two split viewports | 3 / 840 |
| Reload to 2CylinderEngine and back | 3 / 1,200 |
| 2CylinderEngine | 19 / 100 |
| E195 cockpit with a click grid | 36 / 358 |

- **`python scripts/validate_p8_1.py --ref-exe build/p8/Glider_p81c.exe --label p8.3final`: PASS (61/61 byte-identical).**

#### 4. Measurement
`validate_p8_1.py` benchmark: 5 alternating pairs, every frame redrawn, against P8.1c.

| Workload | Draw calls | GL calls | Frame interval | GPU | draw.submit | commands.generate |
|---|---|---|---|---|---|---|
| 1,600 grid | 1,601 → 404 | 36,438 → 8,903 | 5.32–6.94 → 1.82–2.32 ms (−58.2%, 5/5) | 1.62–1.87 → 0.04 ms | −92.3% | +0.1 ms |
| 10,000 grid | 10,001 → 2,504 | 227,538 → 55,103 | 43.8–51.0 → 24.1–27.8 ms (−46.7%, 5/5) | 23.8–28.8 → 4.8–5.6 ms | −75.7% | 5.1–5.6 → 5.9–6.6 ms |
| E195 cockpit | 1,315 → 993 | 45,575 → 31,335 | 5.03–5.27 → 3.90–4.40 ms (−17.9%, 5/5) | 1.55–1.63 → 0.17 ms | −34.5% | +0.05 ms |
| 2CylinderEngine | 116 → 35 | 2,562 → 742 | 0.57–0.76 → 0.44–0.63 ms (−23.1%, 5/5) | 0.044 → 0.046 ms (+2 µs) | −22.8% | +0.009 ms |

**Where it costs more:** grouping adds a little to `commands.generate` (0.01–0.8 ms), and 2CylinderEngine's GPU time rose by 2 µs. Both are far smaller than the savings in the same frames, and every workload's frame interval improved in all pairs.

**What the remaining draws are:** the grids still draw their blended quads one by one, because the transparent pass must stay sorted.

<a id="section-12-61"></a>

### 12.61 Phase 8 Exit Gate (2026-10-23)

All runs used the final build (P8.5 + P8.1 + P8.3).

| Gate | Result |
|---|---|
| `validate_p8_5.py --only equivalence` | PASS |
| `validate_p8_1.py` (against P8.1c) and `validate_p8_3.py` | PASS |
| `validate_p4_exit_gate.py` | PASS |
| Revision, blend, AA-target and cache-invalidation self-checks | PASS (0 failures) |
| `validate_p5_1.py` | 4 FAIL, all expected, none from Phase 8 |

**The four `validate_p5_1.py` failures:**
- **What fails:** checks that screenshots are byte-identical to the pre-P5.1 binary.
- **Where the difference is:** only the production fixture's transmissive quad. The quad was dark grey before the transmission fix in `51266d3` and is now see-through.
- **Why Phase 8 is not the cause:** the same screenshots match the candidate's own `--always-pick` run exactly, and the failures appeared with `51266d3` alone, before any Phase 8 change.

**Summary of the phase:**

| Task | Outcome |
|---|---|
| P8.5 | A click frame costs about an ordinary redrawn frame, even at 10,000 drawables |
| P8.1 | Per-draw string lookups are gone, and camera, environment and exposure values are uploaded once per draw pass |
| P8.3 | Repeated meshes are drawn in one call |

Measured against the Phase 8 baseline, frames with every drawable redrawn are about 2.9× faster on the 1,600 grid and 1.8× faster on the 10,000 grid. E195 frames are about 20–25% faster. Every image is unchanged.

**Not done:**
- **P8.2** is deferred: its only cost is a one-time shader compile.
- **P8.4** is rejected: the driver already caches shaders.
- **Skin uniform blocks** are rejected: skin storage costs negligible GPU time.

<a id="section-12-62"></a>

### 12.62 Follow-up — Three-Viewport E195 Use Case (2026-10-23)

**Status:** PASS. Done after the Phase 8 commit `23f61c6`, for the user's typical setup: the E195 cockpit in three
TAA viewports with the environment map, at 1920×1080.

#### 1. Measurement before changes
- **The setup is CPU-bound.** With every frame redrawn, the frame took 8.9 ms of CPU, against about 4.7 ms of
  GPU pass time:

  | CPU stage | Time |
  |---|---|
  | `draw.submit` | 4.0 ms |
  | `scene.create` | 1.8 ms |
  | `commands.generate` | 1.1 ms |
  | UI submit (waiting on the GPU) | about 1 ms |

  Interacting in one viewport (the other two skip their redraw) took 4.2 ms.
- **448 collider primitives drawn for nothing.** Of the cockpit's 1,314 primitives, 448 use the materials
  `InvisibleCollider` and `COLLIDER`, which declare `ASOBO_material_invisible`.

#### 2. Changes
- **`settings.ini` location (bug fix).** Settings were read from the working directory before the executable's
  folder. A stale, git-ignored `settings.ini` in the repository root (search path `../texture` only) therefore
  shadowed the real one whenever Glider started from there, and the cockpit's textures in
  `../texture.E195.cockpit` were not found.
  - Settings now live in `%APPDATA%\Glider\settings.ini`, like the other persisted state.
  - On first use, the file next to the executable is copied there. The working directory is never read.
- **`ASOBO_material_invisible` support.** Primitives whose material declares the extension with `enabled` true
  (the schema default) are not rendered.
  - Turned on by the app setting *Show Invisible Materials (ASOBO_material_invisible)*, or with
    `--show-invisible-materials` or `vpsetting <id> invisible on`.
  - This is phase A1.1 of the [ASOBO extension journal](ASOBO_EXTENSIONS_JOURNAL.md).
- **Shared scene traversal.** `GltfRenderLayer::createScene` splits into a camera-independent traversal and
  per-viewport culling.
  - **What the traversal holds:** visible primitives with their meshes, materials, nodes and static bounds,
    plus lights.
  - **Reuse:** between `beginSceneFrame` and `endSceneFrame` (around the viewport loop), the viewports of one
    frame reuse a single traversal, keyed by scene, invisible setting and material variant. Outside a frame,
    `createScene` always traverses.
  - **Duplicate resets:** per-drawable state was reset twice per call; the redundant first pass was removed.
  - **Reference:** `--disable-shared-traversal` is the same-binary reference; the UI snapshot reports
    `traversalReuses`.
- **Redundant uploads skipped.**
  - **Uniforms:** every `gl::Uniform` write takes a new value from a global revision counter, so equal
    revisions mean equal bytes, even after a copy. Each program-side `UniformBinding` remembers the revision it
    last received and skips the `glUniform*` call when they match. Uniform values are program state.
  - **Samplers:** the sampler uniform's texture unit is remembered the same way.
  - **Texture units:** `CommandExecutor` remembers what each unit holds and skips the bind when nothing changed.
    That memory is cleared at every `Renderer::draw` and after external functions, so changes made outside the
    renderer are never missed.

#### 3. Validation
- **`python scripts/validate_p8_followup.py`: PASS.**
  - **Invisible materials:** hidden by default. The option (at startup or switched on at runtime) and
    `"enabled": false` both render byte-identically to `23f61c6`, and assets without the extension are
    unchanged. Checked on a generated grid and on the E195 cockpit.
  - **Shared traversal:** three-viewport scenes are byte-identical with and without it, including viewports
    with different invisible settings, and the traversal is reused (29–114 times per run).
- **Smoke test for the redundant-upload change:** six scenes under TAA were byte-identical to the build before it,
  with no GL errors.
- **Broad suites against `23f61c6`** (with `--show-invisible-materials`, so E195 stays comparable):
  `validate_p8_1.py` PASS and `validate_p8_3.py` PASS.

#### 4. Result
Three TAA viewports of E195 at 1920×1080, every frame redrawn, 3 rounds:

| Build | Frame interval (median) | Draws | GL calls |
|---|---|---|---|
| `23f61c6` | 8.72 ms | 2,831 | 88,701 |
| Final, default | 4.67 ms (−46%) | 1,485 | 14,512 |
| Final, invisible materials shown | 8.42 ms (−3.4%) | 2,831 | 25,511 |

**Where the gain comes from:**
- **Invisible materials:** most of it comes from not drawing the declared-invisible colliders, which are blended,
  unsortable geometry.
- **Same content:** with the colliders shown, the gain is 3.4% despite 71% fewer GL calls. The calls that are
  skipped are cheap.
- **Shared traversal:** on its own it moved `scene.create` only from 1.01 to 0.96 ms. The traversal is a small
  part of `scene.create`; most of it is per-viewport work on every drawable.
