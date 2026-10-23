# Phase 6 — Load Pipeline and Background Work

[Journal index](RENDERER_OPTIMIZATION_JOURNAL.md) · [Reference audit](RENDERER_OPTIMIZATION_REFERENCE.md) · [Validation rules](RENDERER_OPTIMIZATION_VALIDATION.md)

**Status:** Complete ([§12.50](#section-12-50)). P6.1 PASS ([§12.47](#section-12-47)), P6.2 PASS ([§12.48](#section-12-48)), P6.3 DEFERRED ([§12.49](#section-12-49)).

This file contains the phase task list and its implementation/review history. Original section numbers are retained for traceability.

<a id="section-5-8"></a>

### 5.8 Phase 6 — Load Pipeline and Background Work (R2 then R3)

**Entry:** Phase 0 load-stage metrics. These tasks need not wait for Phase 5.

- [x] **P6.1 — Validation source reuse ([§3.26](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-26), R2).** Preserve original bytes/container diagnostics and
  report target mapping; compare complete normalized reports and peak RAM on valid/malformed assets.
- [x] **P6.2 — Bounded parallel image decode ([§3.16](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-16), R3).** Audit decoder/thread-safety and logging first.
  Bound concurrency by memory as well as CPU count; keep GL uploads on the context thread. Test all
  supported formats, failures/cancellation, reload and peak RAM. Use standard threads/a lightweight
  queue; do not assume `std::execution::par` avoids a TBB/backend dependency.
- [ ] **P6.3 — Background validation ([§3.26](RENDERER_OPTIMIZATION_REFERENCE.md#section-3-26), R3).** Only if P6.1 still leaves meaningful UI blocking.
  Use immutable inputs, generation tokens and main-thread target resolution/publication. Test closing
  or replacing an asset before completion and stale results. Keep pending/failed status distinguishable
  from valid. Compare both responsiveness and total validation completion time.
  *DEFERRED: P6.1 left too little blocking to justify it ([§12.49](#section-12-49)).*

**Exit gate:** same decoded/validated results, no stale publication or GL calls on workers, bounded
peak memory, and repeatable load/response improvements. More threads are not an acceptance criterion.

---

## 12. Implementation Records

<a id="section-12-47"></a>

### 12.47 P6.1 — Validation Source Reuse (2026-10-23)

**Status:** PASS for P6.1a (GLB range reads). P6.1b (reusing the loader's parsed JSON) is DEFERRED; see §4.
R2. Base `110c786`. Pre binary: `C05F2C87…` (base plus the stage instrumentation below, `build/p6.1/pre/`).
Candidate `13319ECC…`. Same-binary reference: `--validation-full-read`.

#### 1. Audit and measurement (before changing behaviour)

- `loadGltfModel` loads the model through `GltfParsing::fromFile`, which reads the whole file and parses its
  JSON with `nlohmann::json::parse(str, nullptr, false)`. It then calls `GltfValidator::validate`
  synchronously.
- For a GLB, `validate` reads the **whole file** again into a `core::Vector`. It then checks the header,
  copies the JSON chunk into a string, parses it, and reads 8 bytes of the BIN chunk header. A text glTF is
  read and parsed again.
- **Instrumentation (R0, kept).** `ValidationReport::timing` holds read, parse, schema and semantic stage
  times and the bytes read. These appear in the profile report as `validation.*` startup stages and
  `validationBytesRead`.
- **New fixture.** `scripts/generate_load_fixture.py` is deterministic. It writes a 159 MB GLB: a 3.6 MB
  JSON, 4,000 nodes and meshes, a 108 MB vertex buffer and 16 embedded 1024² PNGs. It writes the same asset
  as `.gltf` + `.bin` + external PNGs. The largest asset in the repository is 3.3 MB.
- **Measurement on the GLB.** Validation took 222–333 ms: read 142–206 ms, parse 42–70 ms, schema about
  1 ms, semantic checks 21–30 ms. The text glTF took 76 ms: read 2 ms, parse 37 ms, semantic 21 ms.
  **The read of the whole GLB dominated, and all it was needed for was the header, the JSON chunk and one
  8-byte chunk header.**
- **Hypothesis (P6.1a).** Read only those ranges, plus the file size. Every check sees the same values, so
  every diagnostic and target mapping is unchanged, and the transient whole-file buffer disappears.
- **Predeclared gates.**
  - The complete report must be byte-identical to `--validation-full-read`: issues, codes, messages, JSON
    pointers, target type and index, and counts.
  - The same assets must load.
  - Validation time must improve by more than 3% and the noise (§6.5).
  - Peak working set must not grow.

#### 2. Changes

- `StdFileSystem::readRange(path, offset, size, out, fileSize)`. It checks `EmbeddedVFS` first (the embedded
  HelloCube), then reads from disk with the same search-path fallback as `load`.
- `GltfValidator`: the GLB branch reads 20 header bytes, then the JSON chunk, then the 8-byte BIN chunk header
  at the computed offset. The checks are unchanged. `--validation-full-read` (`s_fullFileRead`) keeps the
  original whole-file path verbatim.
- Test hooks:
  - `--validation-report <path>` writes each load's report without timings, as JSON.
  - `--validate-file <path>` validates a file without loading it, then exits. The loader rejects most malformed
    containers before validation can run, so only this hook reaches those checks.

#### 3. Validation (`python scripts/validate_p6_1.py`; outputs in `build/p6.1/`)

| Check | Result |
|---|---|
| Reports after a real load, candidate vs `--validation-full-read`: embedded HelloCube (VFS), Fox, BarramundiFish (KTX2), MaterialsVariantsShoe, the 159 MB GLB and its `.gltf` variant, the production fixture, plus 15 container variants of Fox (length ±, trailing bytes, truncated BIN, wrong BIN type, version 1, bad magic, chunk past end, wrong JSON type, 8- and 16-byte files, unpadded JSON, JSON syntax error) and the semantic-error GLB | PASS 22/22. 11 assets loaded and validated, identically (including version 1 with 1 error, unpadded JSON with 1 warning, and semantic errors with 2 errors). The others were rejected by the loader in both modes. |
| Validator alone (`--validate-file`) on all 16 container files | PASS 16/16 identical, exercising every container code: `GLB_HEADER_TOO_SMALL`, `GLB_INVALID_MAGIC`, `GLB_UNSUPPORTED_VERSION`, `GLB_LENGTH_MISMATCH`, `GLB_NO_JSON_CHUNK`, `GLB_INVALID_CHUNK_TYPE`, `GLB_CHUNK_EXCEEDS_FILE`, `JSON_SYNTAX_ERROR`, `GLB_UNEXPECTED_CHUNK_TYPE` |
| Bytes read by validation | 159,044,296 → 3,569,652 (large GLB); 3,289,040 → 2,592 (BarramundiFish); 162,852 → 16,184 (Fox) |
| Timing, 159 MB GLB, 5 alternating pairs, fresh processes (warm file cache) | Validation 190.9–295.6 → 77.0–120.1 ms (median paired −69.7%, 5/5, noise 40.4%). Read stage 111.7–171.2 → 2.3–2.7 ms. |
| Peak working set (psutil `peak_wset`) | 823.6–836.1 → 658.5–698.1 MiB (median paired −16.2%, 5/5, noise 1.5%) |
| Total startup | 1,825–2,197 → 1,661–2,475 ms (−5.7%, 2/5, noise 18.8%): within noise. It is dominated by IBL filtering and load/decode variance, so no total-time claim is made. |
| `--validation-cache-selfcheck` | PASS |

#### 4. P6.1b — reuse of the loader's parsed JSON: DEFERRED

The remaining validation cost on the large GLB is the JSON parse (37–42 ms) plus the semantic checks (about
21 ms). The loader parses the same chunk with the same grammar. Sharing its DOM was rejected for now:
- `GltfFromJson` reads `m_json` through non-const `operator[]`, which inserts null members for missing keys.
  The validator could then see a mutated document, and schema diagnostics would change.
- A defensive deep copy costs a large part of what it saves.
- It needs `GltfParsing` API changes to hand the DOM over.

The benefit is about 2% of a 2 s load, so per §6.5 this optional complexity is DEFERRED.

#### 5. Decision

P6.1 PASS. Rollback boundary: `StdFileSystem::readRange`, the GLB range branch and `s_fullFileRead`/
`--validation-full-read`, `--validation-report`, `--validate-file`, the stage instrumentation, and
`scripts/validate_p6_1.py` and `generate_load_fixture.py`.

<a id="section-12-48"></a>

### 12.48 P6.2 — Bounded Parallel Image Decode (2026-10-23)

**Status:** PASS (uncommitted). R3. Pre binaries: `13319ECC…` (P6.1, `build/p6.2/pre/`) and `B41EF1C9…`
(after the KTX fix, before the budget, `build/p6.2/pre_budget/`). Candidate `B3C03C1A…`. Off-switch:
`--decode-budget-mib 0` (unlimited, the earlier behaviour).

#### 1. Audit (before editing): the §3.16 premise no longer holds

- **Decoding is already parallel.** `Gltf::loadBinaryDataToMemory` launches one
  `std::async(std::launch::async)` per buffer, then one per image and audio source. Each image task reads its
  bytes and runs `imageformats::decodeImage`.
  - On MSVC these tasks run on the Windows thread pool. On the 96-image fixture the process peaked at 34–42
    threads, and at most 24 decodes (the core count) ran at once.
  - 96 PNGs of 1024² decode in 240–250 ms. GL uploads stay on the context thread (`updateGltf`).
- **Thread safety.**
  - Logging: worker threads log through `core::Logger`, whose callbacks run under its mutex. The main thread is
    blocked on the futures for the whole decode.
  - stb_image: no global settings are used.
  - **libktx is the exception.** `basis_transcode.cpp` initialises the Basis transcoder's global tables through
    an unguarded `static bool`, so two KTX2 images transcoding first in parallel can run
    `basisu_transcoder_init()` concurrently. That is a data race. 12 runs of a 48-image KTX2 stress asset gave
    identical, correct output, so it did not show up in practice.
- **Memory.** A decode holds the decoder's output and the image's copy of it at the same time (stb's buffer is
  copied into `image::Image`), plus decoder scratch.
  - 8 PNGs of 8192² (`scripts/generate_big_texture_fixture.py`) peak at **4.6–4.8 GiB, reached during decode**:
    the after-decode and after-upload peaks are equal.
  - Retained data plus the app's baseline is about 3.6 GiB, so about 1.1 GiB is transient decode memory, with
    all 8 decodes in flight.
- **Plan.**
  - (a) Run the first KTX2 transcode alone.
  - (b) Bound concurrency by memory, not only by the pool's CPU bound. `decodeImage` estimates each decode's
    transient bytes from the header and waits while the decodes in flight would exceed a budget.
- **Predeclared gates.**
  - Decoded output byte-identical at every budget, across all formats and failures, compared with both earlier
    binaries.
  - No hang on failed decodes.
  - In-flight estimate within the budget.
  - A lower peak working set under a binding budget.
  - No load-time regression beyond 5% when the default budget does not bind.

#### 2. Changes

- `ktx2.cpp`: the first `ktxTexture2_TranscodeBasis` runs inside a `std::call_once`. Later transcodes run in
  parallel as before.
- `imageformats`: `setDecodeMemoryBudget` and `getDecodeBudgetStats`/`resetDecodeBudgetStats`.
  - `decodeImage` holds an RAII `DecodeLease` of 2× the estimated decoded size for the whole decode. The
    estimate comes from the header: stb `info` for PNG/JPEG/BMP/TGA/HDR (16-bit and float aware), the KTX2
    header (RGBA32 with mips), and the DDS header (RGBA32F bound).
  - WebP and EXR use a conservative 8× the encoded size.
  - A lease waits while others are in flight and the total would exceed the budget, so a single larger image
    runs alone. It is released on every return path.
- `main.cpp`: the budget defaults to an eighth of physical memory, at least 512 MiB.
  `--decode-budget-mib N` overrides it (0 = unlimited). Startup profiles gain `decodeBudget*` and
  `peakWorkingSetAfterDecode`/`AfterUpload`. `physicalMemoryBytes`/`processPeakWorkingSetBytes` live in
  `IdleWait.cpp` with the other Win32 calls.

#### 3. Validation (`python scripts/validate_p6_2.py`; outputs in `build/p6.2/`)

| Check | Result |
|---|---|
| Captures across budgets and builds: 8×8192² PNG (unlimited, 2048, 512 MiB); 96×1024² PNG as GLB and as glTF (unlimited, 64); 48 KTX2/Basis (unlimited, 16); BarramundiFish KTX2 (unlimited, 16); Fox; MaterialsVariantsShoe; DDS (BC1) and TGA scenarios; failure fixture with a corrupt PNG, a truncated KTX2 and a missing file (unlimited, 1 MiB) | PASS 27/27: every capture is byte-identical to both earlier binaries. Small budgets made decodes wait (8K PNG: 4 and 7 waits; 96 PNG: 87–88; KTX2: 47; failure fixture: 44, **no hang**). The estimated in-flight bytes stayed within the budget whenever more than one decode ran. |
| Trade-off, 8×8192² PNG, 3 runs per budget | unlimited: peak 4.56–4.65 GiB, decode 2.03–2.23 s, 8 concurrent. 4 GiB: unchanged. **2 GiB: 3.86–4.03 GiB (−14%), 3.15–3.59 s**, 4 concurrent. 1 GiB: 3.65–3.70 GiB (−20%), 5.3–5.7 s. 512 MiB: 3.60–3.66 GiB, 9.7–10.1 s, 1 concurrent. |
| Default budget on this machine | 64 GiB → 8 GiB budget. It never binds on any test asset (0 waits), so outputs and timings match unlimited. |
| Overhead when the budget does not bind (96×1024² glTF, 5 alternating pairs vs `B41EF1C9…`) | decode 234–242 → 237–257 ms (median +3.8%, about 9 ms, 4/5 worse, noise 3.4%). Under the 5% investigation threshold, but probably real: a header probe and a lock per image. |

#### 4. Limits

- The budget limits *estimated* transient memory. Retained data (decoded pixels and the encoded bytes each image
  keeps) is not reduced. On the 8K asset that is about 3.2 GiB.
- A binding budget trades decode time for peak memory, as shown above. On machines with at least 32 GiB the
  default never binds for assets like these. On an 8 GiB machine it caps decode transients at 1 GiB.
- The GLB path peaks about 540 MiB higher than the same asset as `.gltf` (1,376 vs 833 MiB on the 96-image
  fixture). The parser holds the whole file and copies the BIN chunk and image bytes. That is not decode
  concurrency, and it is left as a follow-up.
- `decodeKTX` leaks the `ktxTexture2` when a transcode fails (pre-existing; not changed here).

#### 5. Decision

PASS. Rollback boundaries:
- KTX fix: the `call_once` in `ktx2.cpp`.
- Budget: the `DecodeLease` and its estimate, the budget API, the `main.cpp` default and
  `--decode-budget-mib`, the stats, and `scripts/validate_p6_2.py`, `generate_big_texture_fixture.py` and
  `generate_ktx2_stress_fixture.py`.

<a id="section-12-49"></a>

### 12.49 P6.3 — Background Validation: DEFERRED (2026-10-23)

The task's entry condition is that P6.1 still leaves meaningful UI blocking. Measurements after P6.1 (§12.47,
§12.48):

| Asset | Validation (synchronous, after load) | Load blocking it sits in (parse + decode, upload) |
|---|---|---|
| 159 MB GLB, 4,000 nodes, 16 images | 77–120 ms | 410–690 ms + 270–340 ms |
| Same asset as `.gltf` + `.bin` | about 76 ms | 370–550 ms + 270–330 ms |
| Repository samples (Fox, BarramundiFish, MaterialsVariantsShoe, CesiumMilkTruck, GlamVelvetSofa) | 1–3.4 ms | 2–100 ms |

Validation already runs inside the same synchronous load, which freezes the UI for the whole parse, decode and
upload. A worker would remove at most about 10% of that freeze on the largest test asset, and nothing
noticeable on typical assets.

The cost would be R3 work:
- an immutable snapshot of the source and model;
- a generation token so a reload discards stale results;
- main-thread resolution of issue targets, because issues hold raw `Node*`/`Material*` pointers into the
  model;
- a pending state that the Validation panel and status-bar badge keep distinct from "valid";
- after P5.4, a `glfwPostEmptyEvent` on completion to wake the idle loop.

Per §6.5, optional complexity without a demonstrated need is DEFERRED. Revisit it if the load itself moves off
the UI thread, since validation would then be the remaining freeze, or if real assets show validation
dominating.

<a id="section-12-50"></a>

### 12.50 Phase 6 Exit-Gate Review (2026-10-23)

**Status:** PASS on the final binary `B3C03C1A…`. P6.1 PASS, P6.2 PASS, P6.3 DEFERRED (§12.49) and P6.1b
DEFERRED (§12.47 §4).

| Criterion | Evidence | Result |
|---|---|---|
| **Same decoded and validated results** | `validate_p6_1.py` reports 39/39: complete validation reports byte-identical to the whole-file read, after real loads and through `--validate-file` on every GLB container error code. `validate_p6_2.py` 27/27: captures byte-identical across decode budgets and to both pre-P6.2 binaries, for PNG (8K and 1K, GLB and glTF), KTX2/Basis, DDS, TGA, JPEG samples and corrupt or missing images. | PASS |
| **No stale publication; no GL calls on workers** | No background publication was added (P6.3 deferred). Decode workers run only `imageformats` code (stb, libktx, WebP, tinyexr, DDS); GL uploads stay in `updateGltf` on the context thread, unchanged. The one shared-state hazard found on the workers, libktx's transcoder initialisation, is serialised. | PASS |
| **Bounded peak memory** | GLB validation no longer allocates the whole file: peak working set 824–836 → 659–698 MiB on the 159 MB GLB. Image-decode transients are bounded by the decode budget: on 8×8192² PNG, 2 GiB → −14% and 1 GiB → −20% peak. The default (RAM/8) never binds on this 64 GiB machine. | PASS |
| **Repeatable load/response improvements** | Validation of the 159 MB GLB: 191–296 → 77–120 ms (median −69.7%, 5/5). Total startup is within noise, and no claim is made for it. More threads were not a criterion; decoding was already parallel and pool-bound. | PASS |
| Regression gates | `validate_p4_exit_gate.py` 93/93. `--revision-selfcheck` 85/85. `--validation-cache-selfcheck` PASS. `validate_p5_1.py` 355/355, byte-identical to the pre-Phase-5 HEAD binary. | PASS |

Follow-ups (none blocks the gate):
- The GLB parse path holds the whole file and copies the BIN chunk and image bytes: about 540 MiB above the
  `.gltf` path on the 96-image fixture.
- The budget's header probe costs about 9 ms per 96 images when it does not bind.
- `decodeKTX` leaks the texture on a transcode failure.
- P6.1b and P6.3 are deferred.
- The pre-existing shutdown hang after `WM_CLOSE` (Phase 5 §12.46).
