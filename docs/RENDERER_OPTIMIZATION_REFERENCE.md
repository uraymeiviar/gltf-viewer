# Renderer Optimization — Reference Audit and History

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

Original section numbers are retained for traceability. Audit observations and older changelog entries describe their original baseline; consult phase records for subsequent changes.

<a id="section-1"></a>

## 1. Executive Summary

This journal records every performance opportunity found during a full audit of the render loop, the
command-generation pipeline, the GL abstraction layer, GPU picking, anti-aliasing and the IBL pipeline.

Static inspection identifies substantial potential CPU overhead; whether a scene is CPU- or GPU-bound
must be established with [§6](RENDERER_OPTIMIZATION_VALIDATION.md#section-6) measurements. A large amount of
redundant work is executed **per drawable, per viewport, per frame**:

* The GL error state is queried on the order of **50–100+ times per drawable per frame**.
* A **new VAO is generated and deleted for every drawable every frame**.
* Shader permutations, `Defines` maps, uniform/sampler pointer lists and picking materials are
  **rebuilt from scratch every frame**.
* The **entire scene is rasterized a second time** for picking on every frame, even though the result
  is only read when the user clicks.
* The IBL pipeline alone reserves roughly **~550 MB of VRAM** (1024² RGBA32F cube maps + LUTs) and
  performs a blocking `glFinish()` during a multi-second startup filter.

None of the findings require a renderer rewrite. The first phase is a set of small, local changes with
low visual risk. Later phases restructure how commands and scenes are cached.

### Priority Summary

This is an opportunity inventory, not implementation order. Follow the risk-ordered task IDs and
dependency gates in [§5](RENDERER_OPTIMIZATION_VALIDATION.md#section-5); the preliminary risk labels below do not override those gates.

| # | Opportunity | Primary location | Cost type | Expected gain | Effort | Risk |
| :-- | :--- | :--- | :--- | :--- | :--- | :--- |
| 1 | Gate `checkGLError` out of hot paths | `gl/Uniform.cpp`, `gl/Sampler.cpp`, `gl/Texture.cpp`, `CommandExecutor.cpp` | CPU / driver sync | **Very high** | S | Low |
| 2 | Stop querying max anisotropy per bind; use sampler objects | `gl/Sampler.cpp` | CPU / driver | High | S | Low |
| 3 | Shadow-cache redundant GL state per draw | `CommandExecutor.cpp` | CPU / driver | Medium–High | S | Low |
| 4 | Cache VAOs per `(mesh, program)` | `CommandMapping.cpp:169` | CPU / driver | **Very high** | M | Low |
| 5 | Cache command descriptors, `Defines`, program selection | `CommandMapping.cpp` | CPU / allocations | **Very high** | L | Medium |
| 6 | Render GPU picking on demand (or 1×1) | `main.cpp`, `PickingRenderLayer.cpp` | GPU + CPU | **Very high** | M | Medium |
| 7 | Split `isSceneDirty` into granular dirty flags | `main.cpp:1629`, `GltfRenderLayer.cpp` | CPU / hitching | High | M | Medium |
| 8 | Skip redraw when nothing changed | `main.cpp` main loop | GPU + CPU | High (idle) | M | Medium |
| 9 | Frustum culling + instancing | `GltfRenderLayer::createScene` | GPU draw count | High on large scenes | L | Medium |
| 10 | Move camera/lights/env uniforms into UBOs | `CommandMapping.cpp`, shaders | CPU | Medium | L | Medium |
| 11 | IBL: half-float, smaller cubes, async/prebaked | `IBLSampler.cpp` | VRAM + startup | **Very high (light)** | M | Low |
| 12 | `Mapping::update` O(n²) → O(n) | `Mapping.h:78` | CPU / hitching | Medium–High | S | Low |
| 13 | Grid fill-rate and MSAA bandwidth | `GridRenderer.cpp`, `Framebuffer.cpp` | GPU | Low–Medium | S/M | Low |
| 14 | Animation playback decoupling & clip filtering | `main.cpp:1625`, `GltfRenderLayer.cpp:1530` | CPU / hitching | **Very high (anim)** | S | Low |
| 15 | Multi-threaded parallel texture decoding | `Image.cpp`, `imageformats` | CPU / load time | **Very high (load)** | M | Low |
| 16 | Skinning joint vector allocation & UBO migration | `Skin.cpp:93-116` | CPU / allocations | Medium–High | S | Low |
| 17 | Selected node OBB subtree caching | `main.cpp:213-232` | CPU / traversal | Medium | S | Low |
| 18 | ImGui hierarchy panel virtualization & label caching | `SceneHierarchyPanel.cpp` | CPU / UI frame | Medium–High | S | Low |
| 19 | Allocate only the active AA mode's targets ([§3.20](#section-3-20)) | `AntiAliasing.cpp` | VRAM / resize | Predictable memory saving | S | Low |
| 20 | Cache Statistics aggregates and file metadata ([§3.21](#section-3-21)) | `StatisticsPanel.cpp` | CPU / filesystem | Scene-dependent | S | Low |
| 21 | Cache and clip resource/validation lists ([§3.22](#section-3-22)) | `ResourcesPanel.cpp`, `ValidationPanel.cpp` | CPU / UI frame | High on long lists | M | Medium |
| 22 | Incremental log counters and visible-row rendering ([§3.23](#section-3-23)) | `StatusBar.cpp`, `main.cpp` | CPU / UI frame | Log-volume-dependent | M | Medium |
| 23 | Index animation conflicts ([§3.24](#section-3-24)) | `AnimationPlayer.cpp`, `StatusBar.cpp` | CPU / popup / load | High with many clips | M | Medium |
| 24 | Propagate transform and skin revisions ([§3.25](#section-3-25)) | `TransformHierarchyNode.cpp`, `Mesh.cpp`, `Skin.cpp` | CPU / uploads | High on mostly static hierarchies | L | Medium |
| 25 | Avoid validator's second whole-file load ([§3.26](#section-3-26)) | `GltfValidator.cpp`, `main.cpp` | I/O / peak RAM / load | Asset-size-dependent | M | Medium |
| 26 | Remove duplicate initial scene synchronization ([§3.27](#section-3-27)) | `main.cpp::loadGltfModel` | CPU / load | Scene-dependent | M | Medium |

Legend: Effort S ≈ <1 day, M ≈ 1–3 days, L ≈ >3 days. Expected gain is relative and must be confirmed
with the measurement protocol in [§6](RENDERER_OPTIMIZATION_VALIDATION.md#section-6).

---

<a id="section-2"></a>

## 2. How a Frame Currently Executes

Understanding the per-frame cost requires following one frame through the application:

```text
main.cpp main loop (1286–2471)
│
├─ glfwPollEvents / delta time / animationPlayer.update() / turntable
│
├─ ImGui NewFrame + DockSpace + menus + shortcuts
│
├─ if (isSceneDirty)                       ← any UI change, including selection
│    ├─ gltfLayer->updateGltf(model)       ← rescans all materials + textures
│    ├─ gltfLayer->updateScene(model, scene)
│    └─ gltfLayer->applyAnimationTime()
│
└─ for each open viewport
     ├─ resize sceneFbo / pickingFbo if needed
     ├─ bind + clear pickingFbo              (every frame)
     ├─ bind + clear sceneFbo
     ├─ resolve camera / projection / TAA jitter
     ├─ loop all materials → rewrite defines / face culling
     ├─ loop all meshes    → recompute winding via map lookup + defines
     ├─ gltfLayer->createScene()             ← rebuilds drawable list + reverse map
     ├─ pickingLayer.applyLayer(scene)       ← NEW Material + RenderPass per drawable
     ├─ commandGenerator->update(scene)
     │    └─ CommandMapping::makeCommand()   ← per drawable: Defines, shader, VAO, vectors
     ├─ renderer.draw()
     │    └─ CommandExecutor per command     ← uniforms, samplers, GL error checks
     ├─ optional wireframe overlay           ← second createScene() + second draw
     ├─ gridRenderer.draw()
     ├─ bbox / OBB renderers
     ├─ sceneFbo.unbind() (MSAA resolve)
     ├─ antiAliasing.render() (FXAA/TAA full-screen)
     ├─ ImGui::Image(scene texture)
     ├─ compass gizmo / selection picking (on click)
     └─ coordinate gizmo
     │
├─ panels, status bar, ImGui::Render()
├─ glClear(window) + ImGui draw data
└─ glfwSwapBuffers()
```

Two structural observations follow from this flow:

1. **Almost everything is rebuilt every frame**, even when the scene, materials and settings have not
   changed. The only per-frame *necessary* work is: camera matrices, animation sampling, viewport
   input, and the actual draw submission.
2. **Work scales with drawables × viewports × frames**, and picking doubles the drawable count. A model
   with 2,000 primitives and two open viewports performs command generation and draw setup for
   ~8,000 drawables per frame.

---

<a id="section-3"></a>

## 3. Detailed Findings

<a id="section-3-1"></a>

### 3.1 Hot-Path `glGetError()` Calls

**Observation**

`checkGLError()` ultimately calls `GL::getError()` (`glGetError`), which can force a driver
round-trip and pipeline serialization on many drivers. It is invoked in the innermost loops:

| File | Line | Frequency |
| :--- | :--- | :--- |
| `renderer/source/gl/Uniform.cpp` | 98 | **Once per uniform commit** |
| `renderer/source/gl/Sampler.cpp` | 46 | Once per sampler commit |
| `renderer/source/gl/Texture.cpp` | 53 | Once per texture bind (also inside every sampler commit) |
| `renderer/source/gl/UniformBlock.cpp` | 24 | Once per uniform-block bind |
| `renderer/source/CommandExecutor.cpp` | 92, 108 | Before and after every draw |
| `renderer/source/CommandExecutor.cpp` | 140, 153, 165, 172 | Vertex array, blend, culling, polygon mode |

**Why it costs**

`CommandMapping` appends roughly 25–35 uniforms and 10–18 samplers to every draw command
(`CommandMapping.cpp:188-249`). Each uniform triggers `checkGLError`; each sampler triggers
`Texture::bind` (`checkGLError`) **and** `Sampler::commit` (`checkGLError`). The executor adds ~5 more
per draw. A typical drawable therefore issues:

```text
~30 uniforms          × 1  = 30 glGetError
~15 samplers          × 2  = 30 glGetError
draw + state setup    × 5  =  5 glGetError
------------------------------------------
                         ≈ 65 glGetError per drawable
```

With picking enabled this happens **twice** → ~130 per drawable per frame. For 1,000 drawables that is
**~130,000 `glGetError` calls per frame**. Even on drivers where `glGetError` is inexpensive, this is
pure overhead; on drivers where it flushes, it defeats batching entirely.

**Proposed change**

* Introduce a compile-time or runtime flag (`GLIDER_GL_DEBUG_CHECKS`, default off in Release).
* Keep checks at coarse boundaries only: after resource creation (shader compile, texture upload,
  FBO setup, IBL filter) and optionally once per frame.
* Remove calls from `Uniform::commit`, `Sampler::commit`, `Texture::bind`, `UniformBlock::commit` and
  the per-draw executor path.

Sketch:

```cpp
#if defined(GLIDER_GL_DEBUG_CHECKS)
    #define GLIDER_CHECK_GL(gl, msg) ::ux3d::renderer::gl::checkGLError((gl), (msg))
#else
    #define GLIDER_CHECK_GL(gl, msg) ((void)0)
#endif
```

**Expected impact / risk**

Very high on scenes with many drawables. Low risk: errors are still caught in Debug builds and at
resource-creation boundaries. This should be the **first change** because it makes every later
measurement less noisy.

---

<a id="section-3-2"></a>

### 3.2 Per-Bind Sampler Work and Anisotropy Query

**Observation** — `renderer/source/gl/Sampler.cpp:15-47`

Every sampler commit currently performs:

```cpp
gl->activeTexture(GL_TEXTURE0 + textureUnit);
texture->bind();                                   // includes checkGLError

gl->texParameteri(target, GL_TEXTURE_MIN_FILTER, ...);
gl->texParameteri(target, GL_TEXTURE_MAG_FILTER, ...);
gl->texParameteri(target, GL_TEXTURE_WRAP_S, ...);
gl->texParameteri(target, GL_TEXTURE_WRAP_T, ...);
gl->texParameteri(target, GL_TEXTURE_WRAP_R, ...);

if (gl->hasExtension("GL_EXT_texture_filter_anisotropic")) {
    GLfloat maxAnisotropy;
    gl->getFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAnisotropy);   // synchronous query
    gl->texParameterf(target, GL_TEXTURE_MAX_ANISOTROPY_EXT, maxAnisotropy);
}

gl->uniform1i(binding.location, textureUnit);
checkGLError(gl, "Failed to commit sampler");
```

`hasExtension` constructs a `std::string` and performs an `unordered_set` lookup on every call
(`source/imgui/GladGL.cpp:10-25`), and the max-anisotropy value is a driver query repeated for every
texture of every drawable.

**Why it costs**

Per sampler: 1 string construction + hash lookup + 1 driver query + 6–7 `texParameteri` calls, most of
which set values that are already set on the texture object. With ~15 samplers × 2,000 drawables this
is tens of thousands of redundant driver calls per frame.

**Proposed change**

1. Query max anisotropy **once** during `CommandExecutor::init()` (or GL init) and cache it.
2. Prefer GL sampler objects: create one `glSampler` per glTF `Sampler` and bind with
   `glBindSampler(unit, sampler)`; remove the five `texParameteri` calls from the draw path.
3. If sampler objects are too invasive for the first pass, cache per-texture “params applied” state and
   skip repeated `texParameteri` calls.

**Expected impact / risk**

High. Low risk if implemented with sampler objects carefully (GL 4.1 supports them). Sampler objects
also make the sampler-state mapping (`m_samplerStateMapping`) more faithful to glTF.

---

<a id="section-3-3"></a>

### 3.3 Redundant GL State in the Executor

**Observation** — `renderer/source/CommandExecutor.cpp`

* `activateProgram()` calls `Program::use()` unconditionally (`:192-201`, `gl/Program.cpp:139-142`),
  even when the same program was already active.
* `setupPolygonMode()` (`:168-173`) calls `glPolygonMode` on every draw.
* `setupFrontFace()` (`:175-178`) calls `glFrontFace` on every draw.
* `setupCulling()` (`:156-166`) enables/disables `GL_CULL_FACE` and sets `glCullFace` on every draw.
* `resetState()` only invalidates the vertex array (`:20-23`), not the other cached state.

**Proposed change**

Add a small shadow state in `CommandExecutor` (active program, polygon mode, front face, cull mode,
depth test, blend state) and only issue a GL call when the desired value differs. Reset the cache at
the start of each renderer pass (or after external-function commands, since ImGui, grid, bbox and AA
passes mutate GL state outside the executor).

**Expected impact / risk**

Medium–High CPU reduction; trivial to implement but requires discipline around external-function
commands that change GL state behind the executor's back. The safest pattern is to invalidate the whole
shadow cache whenever an `ExternalFunction` command runs.

---

<a id="section-3-4"></a>

### 3.4 Per-Frame VAO Creation

**Observation** — `renderer/source/CommandMapping.cpp:169-172`

```cpp
if (program != command->program || true) { // ToDo: optimize in the future to not recreate vertexLayout every frame
    command->program = program;
    command->vertexLayout = program->createVertexLayout(mesh->attributes);
}
```

The `|| true` disables the cache entirely. `Program::createVertexLayout` allocates a `VertexLayout`,
which calls `glGenVertexArrays`, binds it, configures every attribute and then returns a
`std::unique_ptr` (`gl/VertexLayout.cpp:6-15`). The pointer is stored in `Command::vertexLayout`
(`renderer/include/renderer/Command.h:75`) and destroyed when the per-frame command vector is rebuilt,
which calls `glDeleteVertexArrays`.

**Why it costs**

One VAO create + attribute setup + delete **per drawable per frame**. This is one of the most expensive
redundant operations in the pipeline: VAO creation is driver-object allocation, and the attribute setup
issues one `glVertexAttribPointer` per attribute.

**Proposed change**

Cache `std::shared_ptr<gl::VertexLayout>` keyed by `(const Mesh*, const gl::Program*)`. Reuse it across
frames. Invalidate a mesh's cached layouts when its attributes are recreated (`Mesh` recreation or
`updateMeshes`). Because `Command` already stores a `shared_ptr`, ownership can be shared between the
cache and the command.

Sketch:

```cpp
struct VertexLayoutKey {
    const Mesh* mesh;
    const gl::Program* program;
    bool operator==(const VertexLayoutKey&) const = default;
};

std::unordered_map<VertexLayoutKey, std::shared_ptr<gl::VertexLayout>, VertexLayoutKeyHash>
    m_vertexLayoutCache;
```

> [!TIP]
> **VAO Independence from Shader Permutations**: In Glider's GLSL pipeline, vertex attribute locations
> are explicitly declared with fixed indices in `pbr.vert` (`layout(location = 0)` for POSITION, 1 for
> NORMAL, 2 for TANGENT, 3 for TEXCOORD_0, 4 for TEXCOORD_1, 5 for JOINTS_0, 6 for WEIGHTS_0, 7 for
> COLOR_0). Because attribute bindings do not change across material permutations, a VAO does not
> need to be keyed by `(Mesh*, Program*)`—it can be cached **directly per `Mesh`** (or stored as a
> `VertexLayout` member directly on `ux3d::renderer::Mesh`). This eliminates the tuple map lookup
> entirely and guarantees exactly one VAO per primitive.

Clear the cache whenever meshes are destroyed/recreated (e.g. in `updateMeshes` or the mesh mapping's
destroy callback).

**Expected impact / risk**

Very high. Low risk, but the cache lifetime must respect `gltfLayer->updateScene()` rebuilding meshes.
Add a generation counter to avoid stale VAOs if a `Mesh*` address is reused.

---

<a id="section-3-5"></a>

### 3.5 Per-Frame Command, `Defines` and Shader Rebuild

**Observation** — `renderer/source/CommandMapping.cpp:33-251`

`CommandMapping::update()` calls `makeCommand()` for **every drawable, every viewport, every frame**.
Each call:

1. Constructs a `CmdDesc` and a fresh `Command` (with empty uniform/sampler vectors).
2. Builds a `Defines` object with ~40 entries (`:50-92`), copies mesh and material defines into it
   (`:92-93`), then adds/removes extension, tone-mapping, IBL and punctual-light defines (`:95-157`).
   `Defines` is an `unordered_map<std::string,int>` (`renderer/include/renderer/Defines.h:38`), so this
   is dozens of string keys and hash-map insertions per drawable.
3. Hashes the defines (`Defines::hash`, `source/Defines.cpp:43-50`) and looks up / compiles the shader
   variant (`ShaderCache::selectShader`, `gl/ShaderCache.cpp:47-182`).
4. Pushes ~25–35 uniform pointers and ~10–18 sampler pointers into vectors that were not reserved
   (`:188-249`), causing repeated reallocations.
5. Sorts commands per pass (`CommandGenerator::emitCommands`, `CommandGenerator.cpp:36-48`).

**Why it costs**

The shader *selection* is cached (good), but the **inputs to selection are rebuilt** every frame. For
1,000 drawables this is ~40,000 map insertions, ~2,000 hashes, ~50,000 pointer pushes and thousands of
heap reallocations per frame, before any GL work happens. The picking layer duplicates this work for a
second copy of every drawable.

**Proposed change**

Cache a compiled `CachedDraw` per drawable, keyed by `(Mesh*, Material*)`, containing:

* the selected `gl::Program*` and cached `VertexLayout`,
* the static uniform and sampler pointer lists,
* the pass assignment,
* a hash/revision of the defines-relevant settings.

Rebuild the cache only when one of these changes:

* scene structural version (`updateGltf` / `updateScene`),
* render mode / face-culling / winding settings,
* IBL / punctual-light / tone-mapping settings,
* material or mesh was modified.

Per frame, only update transform and camera-dependent uniforms, then sort and submit. This is the
single largest CPU-side refactor but also the largest sustained gain.

> [!IMPORTANT]
> **Multi-Viewport Concurrency & Shading Mode as a Uniform**:
> Glider supports multiple concurrent viewports (e.g. Viewport 1 in *Default Shaded*, Viewport 2 in
> *Wireframe* or *Normal* debug mode). In the current code, `main.cpp:2006-2034` imperatively mutates
> `renMat->defines.define("DEBUG", mode)` across all 2,000+ materials sequentially per viewport.
> If compiled draw commands and defines are cached globally without accounting for viewports,
> Viewport 2 will overwrite or conflict with Viewport 1's shader defines.
>
> **The Key Solution**: Convert `DEBUG` shading mode from a compile-time preprocessor definition
> (`#define DEBUG <mode>`) into a **dynamic fragment uniform (`u_debugMode`)** in `pbr.frag`.
> This achieves three critical benefits:
> 1. Eliminates the per-viewport material define rewrite loops in `main.cpp`.
> 2. Avoids compiling 30+ separate shader permutations for the various debug modes.
> 3. Allows multiple viewports with different shading modes to share the exact same compiled shaders
>    and cached draw commands simultaneously.

**Expected impact / risk**

Very high. Medium risk: the cache must be invalidated correctly. A coarse but safe first iteration is
to cache only the `Defines`/program/VAO portion and rebuild the cheap pointer lists each frame.

---

<a id="section-3-6"></a>

### 3.6 Per-Frame Material and Mesh Loops in `main.cpp`

**Observation** — `source/imgui/main.cpp:2006-2034`

Every frame, for every open viewport:

```cpp
for (auto& [gltfMat, renMat] : gltfLayer->m_materialMapping.map) {
    renMat->isDoubleSided = vp->settings.forceDoubleSided
        ? true : (gltfMat ? gltfMat->isDoubleSidedOrDefault() : false);
    renMat->defines.define("DEBUG", static_cast<int>(mode));
}

for (auto& [gltfTuple, renMesh] : gltfLayer->m_meshMapping.map) {
    auto nodeIt = gltfLayer->m_transformHierarchyMapping.map.find(gltfNode);
    float det = ux3d::math::determinant(nodeIt->second->getWorldTransform());
    renMesh->frontFace = ...;
    renMesh->polygonMode = ...;
    renMesh->defines.define("DEBUG", ...);
}
```

This is redundant with the work in `createScene` (`GltfRenderLayer.cpp:1687-1716`), which rewrites the
same defines, and it performs an `unordered_map` lookup per mesh to recompute the winding from the
world-matrix determinant.

**Proposed change**

Move this to a settings-change handler (or fold it into the command-cache revision bump). Winding only
needs recomputation when the node transform or `invertWinding` changes, not every frame.

**Expected impact / risk**

Medium–High on large scenes. Low risk.

---

<a id="section-3-7"></a>

### 3.7 GPU Picking Every Frame

**Observation**

* `source/imgui/main.cpp:1914-1932`: the picking FBO is resized to the full viewport and cleared every
  frame.
* `source/imgui/main.cpp:2058`: `pickingLayer.applyLayer(std::move(scene))` appends a picking copy of
  every drawable to the scene.
* `renderer/source/PickingRenderLayer.cpp:33-91`: every call destroys all picking materials and
  render-pass copies, then allocates **one new `Material` per drawable** and one new `RenderPass` per
  unique pass, including copies of uniforms and samplers.
* The result is only consumed on left-click (`main.cpp:2269-2275`).

**Why it costs**

On the GPU the scene is rasterized twice (color+depth) every frame; the picking buffer also carries a
full-resolution RGBA8 color target plus a D24S8 depth/stencil renderbuffer:

| Viewport | Picking color | Picking depth | Total |
| :--- | ---: | ---: | ---: |
| 1920×1080 | 8.3 MB | 8.3 MB | **16.6 MB** |
| 2560×1440 | 14.7 MB | 14.7 MB | **29.5 MB** |
| 3840×2160 | 33.2 MB | 33.2 MB | **66.4 MB** |

On the CPU, the material/pass churn adds a full second set of allocations and shader selections every
frame.

**Proposed change (in order of preference)**

1. **On-demand picking**: set a `pickingRequested` flag on left-click, render the picking pass only in
   that frame (or the next frame), then read the 1×1 pixel. Store the pending click position so the
   selection is applied immediately after the picking render. Camera cannot move between click and
   read in a way that matters for a click.
2. **Scissored 1×1 picking**: keep the picking pass but enable `GL_SCISSOR_TEST` with a 1×1 rectangle
   and render with a projection offset so only the clicked pixel is shaded. Vertex cost remains, but
   fill and bandwidth collapse.
3. **CPU ray picking**: cast a ray through the click, test primitive bounding boxes, then triangles.
   Eliminates the GPU pass and the extra FBO entirely; requires a BVH for large meshes.
4. **Half-resolution picking** as a compromise: quarter the pixel cost, then read the scaled coordinate.

> [!TIP]
> **Single Shared 1×1 Picking FBO for Multi-Viewport**:
> In the current implementation, every open viewport allocates a full-screen `pickingFbo` (16.6 MB at
> 1080p, up to 66.4 MB at 4K). With 2 or 3 viewports open, this reserves 50–200 MB of VRAM for buffers
> that are only read on a click.
> When moving to on-demand picking:
> 1. Allocate a **single, shared 1×1 pixel FBO** across the entire application.
> 2. When the user clicks inside any viewport, render a 1×1 scissored sub-frustum using that
>    viewport's camera into the shared 1×1 FBO.
> 3. Read back the single pixel immediately with `glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, pixel)`.
> This eliminates all per-viewport picking FBO memory allocations and reduces GPU rasterization to 1 pixel.

A hybrid is attractive: CPU bounds test to find candidate primitives, GPU 1×1 picking only if needed.

**Expected impact / risk**

Very high GPU win (removes ~50% of draw calls and the extra FBO). Medium risk: selection timing must be
handled carefully so a click still selects in the same perceived interaction.

---

<a id="section-3-8"></a>

### 3.8 Dirty-Flag Granularity and O(n²) Mapping Updates

**Observation**

`isSceneDirty` is a single boolean set by many unrelated actions, including node selection
(`main.cpp:2289`), viewport settings (`:1839`, `:1856`, `:1883`) and animation playback (`:1625`).
When set, the frame performs (`main.cpp:1629-1635`):

```cpp
gltfLayer->updateGltf(gltfModel);      // rescans all materials, textures, accessors, samplers
gltfLayer->updateScene(gltfModel, *activeScene);
gltfLayer->applyAnimationTime(...);
```

`updateGltf` (`GltfRenderLayer.cpp:1414-1528`) rebuilds the full image set from all materials, updates
texture/buffer/sampler mappings and material uniforms. `updateScene` (`:1394-1412`) clears and rebuilds
the transform hierarchy (`:1832-1850`) and remaps meshes, lights, cameras, skins and animations.

`Mapping::update` (`renderer/include/renderer/Mapping.h:78-124`) collects all current keys into a linked
list and, for every incoming property, performs a **linear search** in that list. This is O(n²) per
update; with 5,000 nodes it is millions of comparisons — a visible hitch on every selection.

**Proposed change**

* Replace `isSceneDirty` with flags:
  * `structuralDirty` → `updateGltf` (meshes, materials, textures changed),
  * `transformDirty` → `updateScene` transform hierarchy / skins,
  * `animationDirty` → `applyAnimationTime`,
  * `selectionDirty` → UI only (no renderer re-sync).
* Make node selection set only `selectionDirty`.
* Rewrite `Mapping::update`'s erase bookkeeping with an `unordered_set<KEY>` instead of a linked list +
  linear find. The algorithm stays the same, but becomes O(n).

Sketch:

```cpp
std::unordered_set<KEY, core::Hash<KEY>> alive;
alive.reserve(map.size());
for (const auto& [key, _] : map) alive.insert(key);

for (const auto& property : gltfProperties) {
    KEY key = getKey(property);
    alive.erase(key);
    // create / recreate / update as before
}

for (const KEY& key : alive) {
    onDestroyMapping(key, map[key]);
    map.erase(key);
}
```

> [!NOTE]
> **Immediate Quick Win — Node Selection Trigger**:
> In `main.cpp:2289`, clicking a node in the 3D viewport or Scene Hierarchy tree currently sets
> `isSceneDirty = true`. This triggers an immediate, full `updateGltf(model)` (rescanning all
> materials and textures) and `updateScene(model, scene)` (rebuilding the transform hierarchy).
> However, selecting a node does not modify any glTF node or transform—it only updates the UI
> inspector, oriented bounding box (OBB), and local coordinate gizmo rendered in `main.cpp`.
> **Simply removing `isSceneDirty = true` from node selection** eliminates the per-click UI
> freeze immediately with zero risk, before full dirty-flag refactoring in Phase 2.

**Expected impact / risk**

High for interactive use on large models (removes per-click hitches). Low–Medium risk.

---

<a id="section-3-9"></a>

### 3.9 No Frustum Culling, No Instancing

**Observation**

`GltfRenderLayer::createScene` (`GltfRenderLayer.cpp:1627-1695`) adds every visible primitive to
`scene.drawables` unconditionally. There is no frustum, distance or occlusion culling anywhere in the
renderer (confirmed by a repository-wide search). Every primitive is also submitted individually; there
is no instancing or static batching.

**Why it costs**

Off-screen geometry still pays vertex processing, command generation and draw-call overhead. In
architectural / aircraft / city scenes, the majority of primitives are usually outside the view.

**Proposed change**

1. Compute a world-space bounding sphere per drawable from the accessor `POSITION` min/max (already
   available on `gltf::Accessor`, and the validator already expects min/max).
2. Build a frustum from the active view-projection matrix and reject spheres outside it in
   `createScene` or in `CommandMapping`.
3. Optionally add coarse screen-size / distance culling for sub-pixel primitives.
4. Later: group drawables by `(mesh, material)` and use instancing (`glDrawElementsInstanced`) with
   per-instance transforms, or merge static primitives that share a material.

**Expected impact / risk**

High on large scenes, near-zero on single-object showcase models. Medium risk: bounds must handle
skinning, morph targets and non-uniform node scales conservatively (expand bounds or disable culling
for those primitives).

---

<a id="section-3-10"></a>

### 3.10 Redundant Uniform Uploads / Missing UBOs

**Observation** — `CommandMapping.cpp:212-249`

Every draw command receives its own pointers to:

* 6 camera uniforms (view, projection, view-projection, camera position, ortho scaling, viewport),
* 4–5 environment uniforms (rotation, intensity, blur, mip counts),
* every punctual light uniform,
* exposure,
* all material and mesh uniforms.

`CommandExecutor::draw` commits all of them (`:62-72`) with individual `glUniform*` calls and a string
lookup per uniform (`Program::commitUniform`, `gl/Program.cpp:90-97`).

**Why it costs**

Camera, environment and light values are **identical for every drawable in a pass**. Uploading them
per drawable multiplies uniform traffic by the drawable count and prevents the driver from batching.
String-keyed location lookup per commit adds CPU overhead.

**Proposed change**

1. Group per-frame data into a single UBO (`CameraBlock`, `EnvironmentBlock`, `LightBlock`) and bind it
   once per pass.
2. Cache uniform locations (already partially done via `UniformBinding`) and bypass the string map in
   the hot path.
3. For material uniforms, keep per-material uploads but only when values change.

**Expected impact / risk**

Medium CPU, potentially medium GPU on draw-call-bound drivers. Larger refactor because shaders must be
updated to read UBOs. Best done after the command cache ([§3.5](#section-3-5)).

---

<a id="section-3-11"></a>

### 3.11 Idle Redraw at V-Sync

**Observation** — `main.cpp:1286-2471`

The scene, picking, AA, grid, gizmos and UI are redrawn every frame, throttled only by
`glfwSwapInterval(1)` (`:991`). `frameSceneDirty` exists but is used **only** by TAA
(`main.cpp:2124-2128`). A static model with no animation still consumes a full 60 FPS of GPU/CPU time.

**Proposed change**

Track a per-viewport “needs redraw” condition:

```cpp
bool cameraChanged = (currentViewProj != lastViewProj);
bool needsRedraw =
    frameSceneDirty ||
    cameraChanged ||
    animationPlayer.isPlaying() ||
    viewportResized ||
    aaMode == TAA && taaNeedsMoreSamples;
```

When false, skip scene render and keep displaying the last `sceneFbo` texture (ImGui still composes the
UI). In fully idle state, use `glfwWaitEventsTimeout(0.25)` instead of a busy loop. TAA can mark
convergence and stop accumulating after N frames.

**Expected impact / risk**

High for battery/thermals and idle GPU load; no change to interaction FPS. Medium risk: missed dirty
signals cause stale views. Introduce a conservative catch-all (e.g. any ImGui input event) during
development.

---

<a id="section-3-12"></a>

### 3.12 IBL Memory and Startup Cost

**Observation**

* `GltfRenderLayer.h:121` defaults `iblMaxResolution = 1024`.
* `IBLSampler.h:11-14` defaults `textureSize = 1024`, `lutResolution = 1024`, `lowestMipLevel = 4`.
* `IBLSampler.cpp:64-77` creates **four** cube maps (source panorama cube, Lambertian irradiance, GGX
  specular, Charlie sheen) plus **three** 1024² LUTs.
* `IBLSampler.cpp:190,238` uses `GL_RGBA32F` for all of them.
* `IBLSampler.cpp:138-172` runs `panoramaToCubeMap`, two LUT passes, three convolution passes and a
  blocking `m_gl->finish()`. The GGX pass uses **512 samples per texel** (`:275`).

**Memory estimate at current defaults**

| Resource | Format | Size |
| :--- | :--- | ---: |
| Panorama cubemap (with mips) | RGBA32F 1024² × 6 | ≈ 134 MB |
| Lambertian irradiance (no mips) | RGBA32F 1024² × 6 | ≈ 101 MB |
| GGX specular (with mips) | RGBA32F 1024² × 6 | ≈ 134 MB |
| Charlie sheen (with mips) | RGBA32F 1024² × 6 | ≈ 134 MB |
| 3 × LUTs | RGBA32F 1024² | ≈ 50 MB |
| **Total** | | **≈ 553 MB** |

This is a large fixed VRAM cost for a “lightweight” viewer, and it is paid even for a simple cube model.

**Proposed change**

1. Switch IBL targets to `GL_RGBA16F`. IBL is a low-dynamic-range convolution; half-float is visually
   sufficient and halves memory and bandwidth.
2. Use **512²** (or 256²) cube maps for specular/GGX, **32–64²** for Lambertian irradiance. LUTs can be
   256² without visible loss.
3. Reduce GGX sample count (512 → 128–256) or use importance-sampled mip chains; quality difference is
   small at typical roughness.
4. Remove the blocking `glFinish()` in production; rely on fences or just continue (the first frame may
   block naturally).
5. Best case: pre-filter the embedded `footprint_court.hdr` offline and embed the resulting cube maps
   and LUTs in the resource pack, eliminating startup filtering entirely.

**Projected memory with RGBA16F + 512 cube + 256 LUTs**

| Resource | Approx. size |
| :--- | ---: |
| Specular GGX (mips) | ≈ 16.8 MB |
| Lambertian (64², no mips) | ≈ 0.2 MB |
| Sheen (mips) | ≈ 16.8 MB |
| Source cube (mips, optional, can be released) | ≈ 16.8 MB |
| 3 × LUTs 256² | ≈ 1.5 MB |
| **Total** | **≈ 52 MB** |

Roughly a **10× reduction**, with faster startup and no visible loss for a viewer.

> [!NOTE]
> **Custom OpenEXR (.exr) & HDR Maps**:
> Glider supports user-loaded custom `.hdr` and `.exr` files (including large 4K/8K panoramas) via
> the Viewport Inspector and CLI `--env` argument. The 10× memory reduction (`GL_RGBA16F`, 512²
> specular cubemap, 64² irradiance, 256² LUT) is critical for custom environments: filtering an
> 8K EXR at 512² RGBA16F completes in a fraction of the time and prevents GPU out-of-memory crashes.
> For the default embedded `footprint_court.hdr`, pre-filtering offline and packaging the compressed
> cubemap mips and LUTs directly into `EmbeddedVFS` will reduce cold startup time to near-zero.

**Expected impact / risk**

Very high for “lighter” and startup. Low risk, but requires visual comparison of the reference frame
(especially rough metals and sheen) before accepting.

---

<a id="section-3-13"></a>

### 3.13 Grid Fill Rate

**Observation** — `source/imgui/GridRenderer.cpp:35-114,198-233`

The grid is a full-screen triangle strip. The fragment shader:

* computes `gl_FragDepth` manually (disables early-Z for the whole quad),
* uses `discard` for rays parallel to the plane / out of range / empty pixels,
* computes both 1 m and 0.1 m grids with derivatives,
* blends over the scene.

**Why it costs**

At 4K, a full-screen shader with manual depth writes and multiple discards is a measurable fill-rate
cost, especially on integrated GPUs. It runs every frame even when nothing changes.

**Proposed change**

* Skip the minor 0.1 m grid beyond a distance threshold on the CPU or via a uniform (fewer texture
  fetches/derivatives is not the issue; discards and depth writes are).
* Consider rendering the grid into a smaller offscreen buffer or using a finite quad centered on the
  camera with analytic fading.
* At minimum, include the grid in the idle-skip logic ([§3.11](#section-3-11)).

**Expected impact / risk**

Low–Medium; mainly relevant at high resolutions and on integrated GPUs. Low risk.

---

<a id="section-3-14"></a>

### 3.14 MSAA Buffer Cost

**Observation** — `source/imgui/Framebuffer.cpp:96-188`

The scene FBO always allocates a single-sample resolve texture plus, for MSAA modes, a multisampled
color renderbuffer and a multisampled D24S8 depth renderbuffer, then resolves with `glBlitFramebuffer`
every frame (`:179-188`). `AntiAliasing::getSampleCount` allows 2×/4×/8×
(`source/imgui/AntiAliasing.cpp:264-272`).

At 4K with 8× MSAA the memory and resolve bandwidth are substantial (color ~265 MB, depth ~265 MB,
before the resolve target). Defaulting to 4× or offering render-scale would make the viewer
significantly lighter on mid-range GPUs.

**Proposed change**

* Keep MSAA but default to 4×.
* Add an optional render-resolution scale (0.5–1.0) for weak GPUs; the AA pass already exists.
* Consider dropping the depth/stencil renderbuffer to depth-only if stencil is unused.
* Alternatively, rely on FXAA/TAA at native resolution for lower-memory AA.

**Expected impact / risk**

Medium for VRAM and bandwidth at high resolution/AA. Low risk (quality trade-off is user-visible and
configurable).

---

<a id="section-3-15"></a>

### 3.15 Animation Playback Scene Teardown & Over-Evaluation

**Observation** — `source/imgui/main.cpp:1625-1635`, `GltfRenderLayer.cpp:1530-1543`

When an animation is playing:

```cpp
if (animationPlayer.isPlaying()) {
    isSceneDirty = true;
}

if (isSceneDirty && gltfLayer != nullptr) {
    gltfLayer->updateGltf(gltfModel);      // rescans all materials, textures, accessors
    if (activeScene != nullptr) {
        gltfLayer->updateScene(gltfModel, *activeScene); // tears down & rebuilds transform hierarchy
    }
    gltfLayer->applyAnimationTime(animationPlayer.getCurrentTime());
}
```

And in `GltfRenderLayer::applyAnimationTime`:

```cpp
if (animationTime.has_value()) {
    for (animation::Animation& anim : m_animations) {
        anim.updateChannels(*animationTime);
    }
}
```

**Why it costs**

1. Setting `isSceneDirty = true` on every animation tick causes `updateGltf` (scanning all materials,
   textures, and samplers into `std::set`) and `updateScene` (rebuilding the entire node transform
   hierarchy for all 2,000+ nodes) to execute **on every single frame**. Ordinary animation changes
   transforms and morph weights; `KHR_animation_pointer` can also change material and other properties.
   These need targeted updates rather than unconditional structural synchronization.
2. `applyAnimationTime` visits every entry in `m_animations`, but inactive clips already skip channel
   creation in `GltfRenderLayer.cpp:1278-1366`. The earlier claim that all inactive channels are
   interpolated was overstated. An active-clip list can avoid empty visits, but profile this separately
   from the much larger scene rebuild and transform/skin work.

**Proposed change**

* Decouple animation playback from `isSceneDirty`. Create an `animationDirty` flag that only evaluates
  active channel curves and marks node transforms as dirty, bypassing `updateGltf` and `updateScene`
  entirely.
* In `applyAnimationTime`, only evaluate the active/playing animation clip(s) selected in `AnimationPlayer`.

**Expected impact / risk**

Very high for animated models and cockpits. Eliminates major frame drops and hitching during animation
playback. Low risk.

---

<a id="section-3-16"></a>

### 3.16 Multi-Threaded Parallel Texture Decoding on File Load

**Observation** — `OpenGL-Renderer/Tools/tools/gltf/source/Image.cpp:163-250`

During model loading, textures (PNG, JPEG, WebP, KTX2, OpenEXR) are decoded sequentially one-by-one on the
main thread:

```cpp
if (core::Failed(imageformats::decodeImage(rawData, m_data))) { ... }
```

**Why it costs**

Decompressing 30–100 high-resolution textures (e.g., 2K/4K PNGs, WebPs, or EXRs) sequentially on a single
CPU core blocks the application for multiple seconds upon opening a model.

**Proposed change**

Image decompression from an in-memory encoded byte buffer into raw RGBA pixel data is an embarrassingly
parallel, CPU-bound operation with **zero OpenGL context dependency**.
* Parallelize texture decoding across CPU worker threads using C++20 `std::for_each(std::execution::par, ...)`
  or a lightweight task loop.
* Once all raw pixel buffers are decoded on background threads, upload the resulting pixel buffers to OpenGL
  textures serially or via PBOs on the render thread.

**Expected impact / risk**

3×–6× faster model loading on multi-core CPUs. Low risk.

---

<a id="section-3-17"></a>

### 3.17 Skinning Joint Vector Allocations & UBO Migration

**Observation** — `OpenGL-Renderer/renderer/source/Skin.cpp:93-116`

`Skin::computeJoints()` executes every frame for every skinned character/mesh:

```cpp
auto width = math::ceil(math::sqrt(m_joints.size() * 8));
core::Vector<float> textureData;
textureData.resize(width * width * 4); // heap allocation every frame!
...
m_jointsTexture->write(width, width, gl::Format::RGBA, gl::ComponentType::Float, textureData.data(), gl::InternalFormat::RGBA32F);
```

**Why it costs**

1. Heap-allocating `core::Vector<float>` on every frame per skin causes memory fragmentation and cache
   thrashing.
2. Uploading joint matrices as a dynamic 2D float texture (`GL_RGBA32F`) forces texture sampling in the
   vertex shader.

**Proposed change**

* Preallocate `textureData` as a persistent capacity buffer inside `Skin`, eliminating per-frame heap
  allocations.
* Evaluate a UBO against the existing texture path, using queried block-size limits and a texture
  fallback for larger skins. The current representation stores **two mat4 values per joint**
  (joint and normal matrices), so 64 joints require 8,192 bytes before any additional data.
  SSBOs must not become a requirement of the application's OpenGL 4.1 baseline.

**Expected impact / risk**

Medium–High CPU reduction for skinned animated models. Low risk.

---

<a id="section-3-18"></a>

### 3.18 Selected Node OBB Subtree Caching

**Observation** — `source/imgui/main.cpp:213-232, 2111`

`getNodeOBB()` is called every frame for every active viewport when `showSelectedBoundingBox` is enabled:

```cpp
if (vp->settings.showSelectedBoundingBox && selection.type == SelectionType::Node) {
    NodeOBB obb = getNodeOBB(selection.selectedNode, gltfLayer.get());
    ...
}
```

`getNodeOBB()` recursively traverses the selected node's entire subtree (`accumulateNodeOBB`), iterating all
child nodes, primitive meshes, and accessor min/max bounds, computing 8 corner transforms per primitive.

**Why it costs**

When the user selects a root node or a large assembly (such as an aircraft cockpit with 2,400 child nodes),
`getNodeOBB` re-traverses thousands of nodes and accessors on every frame.

**Proposed change**

Cache the computed `NodeOBB` on the selection state. Only recompute when the selection changes or node
world transforms change.

**Expected impact / risk**

Eliminates repetitive CPU subtree traversal on complex models. Low risk.

---

<a id="section-3-19"></a>

### 3.19 ImGui Hierarchy Panel Virtualization & Label Formatting

**Observation** — `source/imgui/SceneHierarchyPanel.cpp:273-276, 319, 345`

For every node in the hierarchy, `formatLabel()` executes string allocations and formatting on every frame:

```cpp
std::string label = formatLabel(ICON_MDI_SOURCE_BRANCH, "Node", node->m_ID, name, opts);
bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(static_cast<intptr_t>(node->m_ID)), flags, "%s", label.c_str());
```

In models with 2,500+ nodes, thousands of string allocations and ImGui ID hashes occur every frame even
for nodes that are scrolled out of the visible panel.

**Proposed change**

* Use stack-allocated formatting buffers (`snprintf`) instead of `std::string` concatenation in `formatLabel`.
* Use `ImGui::IsRectVisible()` or clipping heuristics to avoid full label formatting and widget submission
  for items that are outside the current scroll viewport.

**Expected impact / risk**

Smooth 60+ FPS UI interaction when navigating large node trees. Low risk.

---

<a id="section-3-20"></a>

### 3.20 Allocate AA Targets by Active Mode

**Evidence** — `source/imgui/AntiAliasing.cpp`, `ensureBuffers()` (line 306) and `render()`.
The first FXAA or TAA render allocates all three full-resolution RGBA8 textures: one FXAA
target and two TAA history targets. None/MSAA bypass allocation, but switching to those modes
does not release previously allocated post-process targets.

**Future tasks**

- [ ] Split allocation by mode: FXAA needs one target; TAA needs two. Decide whether to release
  inactive targets immediately or retain them under a bounded reuse policy.
- [ ] Reset TAA history when its targets are recreated, and preserve valid output texture lifetimes
  until ImGui has consumed the current frame.
- [ ] Measure allocations during resizing and mode switches. At 3840×2160, each RGBA8 texture
  represents about 31.6 MiB of pixel storage: FXAA can avoid about 63.3 MiB and TAA about
  31.6 MiB per viewport, excluding driver overhead.

**Acceptance / risk** — Compare all AA modes across resize, viewport close/reopen, and mode
switches; verify stable memory and no stale history. Low risk. Distinct from MSAA sample reduction
in [§3.14](#section-3-14): this removes unused targets without reducing rendering quality.

---

<a id="section-3-21"></a>

### 3.21 Cache Statistics and File Metadata

**Evidence** — `source/imgui/StatisticsPanel.cpp:46-101`, `StatisticsPanel::render()`.
Each render traverses meshes/primitives/attributes, buffers and images, then calls
`std::filesystem::file_size`. These values normally remain unchanged between frames.

**Future tasks**

- [ ] Cache aggregate counts, byte estimates and formatted display strings by model revision.
- [ ] Query file metadata on load/reload or explicit refresh, outside the panel's frame loop;
  retain error handling and distinguish embedded assets from disk files.
- [ ] Keep clipboard output and displayed values backed by the same cached snapshot.

**Acceptance / risk** — With Statistics visible on a large model, confirm zero repeated filesystem
queries and aggregate traversals until invalidation. Compare cached values after opening a different
asset and reloading. Low risk; improves panel CPU cost rather than GPU rendering.

---

<a id="section-3-22"></a>

### 3.22 Cache Filtering and Clip Resource / Validation Rows

**Evidence** — `source/imgui/ResourcesPanel.cpp:155-232` counts search matches by formatting and
lowercasing labels, then repeats that work when rendering open categories.
`source/imgui/ValidationPanel.cpp:179-218` visits every issue and creates lowercase copies of code,
message and pointer during every frame with an active search.

**Future tasks**

- [ ] Cache normalized searchable strings and matching row indices by model/report revision,
  search text, severity toggles and label display options.
- [ ] Render only visible matching rows using a clipper where row height is fixed; provide
  appropriate height bookkeeping for wrapped or otherwise variable-height content.
- [ ] Preserve stable IDs, source issue indices, selection, double-click focus and full report copy.

**Acceptance / risk** — Benchmark scrolling and typing with thousands of resources/issues;
verify exact match counts, scroll extents and selection after filtering or replacing the model.
Medium risk. This complements [§3.19](#section-3-19)'s hierarchical tree work; these are separate flat-list paths.

---

<a id="section-3-23"></a>

### 3.23 Incremental Logger Counters and Row Clipping

**Evidence** — `source/imgui/main.cpp:102-106` appends logs to a vector; the Logger renders every
entry at line 2421. `source/imgui/StatusBar.cpp::render()` rescans all log entries for severity counts,
including while the Logger is closed. Log storage has no cap in this append path.

**Future tasks**

- [ ] Maintain severity counters on append and clear rather than scanning the log each frame.
- [ ] Clip visible log content, accounting for multiline entries and wrapped text; invalidate cached
  heights when panel width/font/wrapping changes.
- [ ] Evaluate an explicit retention limit or ring buffer for long sessions, with a visible truncation
  indication and defined Copy behavior. Treat retention as a product decision, not silent data loss.

**Acceptance / risk** — Test large logs with Logger open/closed, Clear, Copy, wrapping and auto-scroll.
Counts must remain exact and UI time should track visible content rather than total retained entries.
Medium risk for variable-height scrolling; counter caching alone is low risk.

---

<a id="section-3-24"></a>

### 3.24 Index Animation Target Conflicts

**Evidence** — `source/imgui/AnimationPlayer.cpp:30-89,115-141`.
`checkConflict()` compares every channel pair and constructs target-path strings. Active-conflict
queries scan other clips; the animation UI and `source/imgui/StatusBar.cpp` popup repeat those
queries for inactive rows, along with `areAllPossibleAnimationsSelected()`.

**Future tasks**

- [ ] Build each clip's unique target keys once per model/channel revision, preserving both ordinary
  node/path targets and `KHR_animation_pointer` object/property targets.
- [ ] Maintain active-target membership counts or a cached conflict index when the active set changes.
  Reuse it for row enablement and the "Select all non-conflicting" result.
- [ ] Preserve the current deterministic clip selection order and existing conflict semantics.

**Acceptance / risk** — Compare results against the current algorithm for disjoint clips, duplicate
targets, pointer channels and repeated toggles; measure popup and initial selection time on
hundreds of clips. Medium risk. Separate from channel sampling and playback rebuilds in [§3.15](#section-3-15).

---

<a id="section-3-25"></a>

### 3.25 Dirty Subtrees, Normal Matrices and Skin Uploads

**Evidence** — `OpenGL-Renderer/renderer/source/TransformHierarchy.cpp::apply()` traverses every root.
`TransformHierarchyNode.cpp:126-167` recomputes world transforms and updates attached meshes,
cameras and lights on each traversal. `Mesh.cpp:52-57` computes an inverse-transpose normal matrix
on every transform update. `GltfRenderLayer.cpp::applyAnimationTime()` then calls `computeJoints()`
for every skin, whose implementation recomputes matrices and uploads the joint texture.

**Future tasks**

- [ ] Introduce local/world and descendant-dirty revisions so static branches can be skipped while
  ancestor changes still propagate to every affected descendant.
- [ ] Recompute model/normal uniforms only when their effective transform changes; share a normal
  matrix across primitives with the same effective transform where applicable.
- [ ] Track morph-weight dirtiness independently from transform dirtiness, preserving animated-weight
  precedence and `nukeScale` / `nukeRotation` behavior.
- [ ] Recompute/upload a skin only when a joint, root transform, inverse bind data or binding changes.

**Acceptance / risk** — Instrument visited nodes, matrix inversions and skin uploads. Animate one
branch in a large static hierarchy, then test parent motion, non-uniform scales, morph-only clips,
scrubbing and multiple viewports. Frame comparisons must match. Medium risk, large effort:
this is a second-stage optimization after [§3.8](#section-3-8)/[§3.15](#section-3-15), not a replacement for correct invalidation.
It complements [§3.17](#section-3-17) by reducing update frequency rather than only changing storage.

---

<a id="section-3-26"></a>

### 3.26 Reuse Load Input for Validation

**Evidence** — `source/imgui/main.cpp::loadGltfModel()` invokes validation synchronously after load.
`source/imgui/GltfValidator.cpp:222-325` loads the entire GLB again into `binaryData`, copies its
JSON chunk into a string, and parses it; plain glTF text is also loaded and parsed again.
The whole-file GLB buffer is additional transient memory even when filesystem reads hit the OS cache.

**Future tasks**

- [ ] Measure parsing, schema validation and semantic checks separately before choosing concurrency.
- [ ] Share an immutable source snapshot / parsed JSON and original container metadata with validation,
  or read only required GLB ranges through a suitable filesystem abstraction.
- [ ] Preserve original-file diagnostics: validating a reserialized model can hide malformed source
  data and is not an equivalent replacement.
- [ ] If validation remains a first-frame bottleneck, evaluate a worker using immutable data and a
  model generation token; discard stale results and resolve issue targets safely on the UI thread.

**Acceptance / risk** — Measure first usable frame, validation completion time, bytes read and peak
RAM on large GLBs. Compare report contents and target selection, including malformed inputs and
rapid file replacement. Medium risk. Keep validation enabled and use `StdFileSystem` / `EmbeddedVFS`;
background validation is a separate responsiveness option, not a guaranteed total-time reduction.

---

<a id="section-3-27"></a>

### 3.27 Avoid Duplicate Initial Scene Synchronization

**Evidence** — `source/imgui/main.cpp::loadGltfModel()` calls `updateGltf`, `updateScene` and
`applyAnimationTime(0)`, followed by `AnimationPlayer::setModel()` (which also applies animation time).
It then sets `sceneDirty = true`, making the main loop eligible to repeat full synchronization
before rendering. This is separate from repeated rebuilds during continuous playback.

**Future tasks**

- [ ] Trace and count synchronization calls from load through the first presented frame, including
  animation initialization, viewport camera attachment and settings restoration.
- [ ] Choose one owner for initial renderer synchronization, or clear only dirty revisions already
  consumed; retain subsequent changes made by initialization callbacks.
- [ ] Consolidate repeated initial animation application where no observable intermediate state is needed.

**Acceptance / risk** — Open/reload static and animated assets with multiple viewports and attached
cameras. Confirm the expected initial pose, framing and validation targets while reducing redundant
sync calls. Medium risk; depends on [§3.8](#section-3-8) and should not be implemented as an unconditional dirty-flag reset.

---

<a id="section-4"></a>

## 4. Additional Observations (Lower Priority)

<a id="section-4-1"></a>

### 4.1 `createScene` and Reverse Mapping Rebuilt Per Frame

`source/imgui/main.cpp:2036` calls `gltfLayer->createScene(activeScene)` every frame. The function:

* allocates a recursive `std::function` (`GltfRenderLayer.cpp:1634`),
* traverses the whole scene,
* performs multiple `unordered_map` lookups per primitive,
* writes defines and settings,
* builds a fresh `unordered_map<Mesh*, const Node*>` reverse mapping,
* optionally duplicates all transmission drawables and materials (`:1731-1759`).

This can be cached and refreshed only on a structural/transform revision. The reverse mapping can be
built once at scene load (mesh → node is stable per scene).

When wireframe overlay is enabled, `main.cpp:2082` calls `createScene` a **second time** in the same
frame; the first result could be reused with a polygon-mode override instead.

<a id="section-4-2"></a>

### 4.2 `getExtends` Per Frame

`main.cpp:2104` recomputes scene extents every frame when `showSceneBoundingBox` is enabled.
`utils::getExtends` (`OpenGL-Renderer/Tools/tools/gltf/source/Utils.cpp:38-93`) traverses the entire
tree, allocates a vector of `(node, transform)` pairs and iterates every primitive's attributes with
string comparisons. Cache the result and invalidate on transform/scene changes.

<a id="section-4-3"></a>

### 4.3 Recursive `std::function` Lambdas

`updateMeshes`, `updateLights`, `updateAudioEmitters`, `updateSkins` and `createScene` all create
recursive `std::function` objects per call. These heap-allocate and defeat inlining. A small explicit
stack/vector traversal or a member recursive function is cheaper. Only relevant when these run
frequently (dirty updates).

<a id="section-4-4"></a>

### 4.4 Shader Compilation Hitches

`ShaderCache::selectShader` compiles and caches variants on demand. Changing render modes or opening a
model with many feature combinations can compile several variants in one frame, causing a hitch. An
optional disk cache (`glGetProgramBinary` / `glProgramBinary` in `%APPDATA%/Glider`) would remove most
cold-start compilation. This does not affect steady-state frame time.

<a id="section-4-5"></a>

### 4.5 Picking Readback Stall

`glReadPixels` of the 1×1 picking pixel (`main.cpp:2274`) stalls the pipeline on click. This is
acceptable at click frequency. If picking becomes on-demand, ensure the read happens after the picking
pass and does not force a full-scene flush every frame.

---

<a id="section-12"></a>

## 12. Implemented Task Records

