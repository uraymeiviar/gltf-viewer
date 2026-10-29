# Build & Development Workflows (Glider)

Moved from AGENTS.md §§4.1, 4.3–4.6; original subsection numbers are retained. Flags: [CLI_REFERENCE.md](CLI_REFERENCE.md). Guardrails: [AGENTS.md](../AGENTS.md#5-critical-rules--guardrails-for-ai-agents).

### Proportional testing

For each change, run its own focused check plus the closest 1–3 validators. Run the full tier once per commit for broad changes. Use the baseline/regression workflow in §4.5 below. Broaden or repeat checks only for new changes, failures or unresolved concerns. Documentation-only edits use documentation checks.

### 4.1 Building from Source (Windows)

Prerequisites:
- Visual Studio 2019/2022 (MSVC with C++20 support)
- CMake 3.20+
- Python 3 with `pip install zstandard`

```cmd
:: 1. Configure build directory
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

:: 2. Compile standalone Glider.exe
cmake --build build --config Release --target Glider
```

Output binary:
```
build/bin/Release/Glider.exe
```

### 4.3 Updating Runtime Resources

Edit, add or remove files under `resources/` (shaders, fonts, environments, sampleGltf, schemas, icons), then rebuild:
```cmd
cmake --build build --config Release --target Glider
```
CMake reruns `scripts/pack_resources.py` before compiling whenever a packed resource or the packer changes, and the generated `EmbeddedResources.cpp` is never committed. Python 3 with `pip install zstandard` is required at build time. New files are picked up through `CONFIGURE_DEPENDS` globbing. The default environment's file name is `kDefaultEnvironmentFile` in `header/imgui/SceneEnvironment.h`. Keep it at 4096×2048 or below: the renderer resamples it to 1024-pixel cube faces, and the generated hex array grows by about 6 bytes of source per compressed byte.

### 4.4 Publishing GitHub Releases

The script [`scripts/create_release.py`](file:///H:/git-repos/gltf-viewer/scripts/create_release.py) automatically packages `Glider.exe`, `README.md`, and `LICENSE` into a `.zip` archive, checks authentication via Windows Git Credential Manager, and uploads the release asset to GitHub:
```cmd
python scripts/create_release.py
```

---

### 4.5 Regression Testing

After each commit, build it and save its binary as the baseline. Then check later uncommitted changes against it:

```cmd
python scripts/save_baseline.py
python scripts/run_regression.py
python scripts/run_regression.py --tier full
```

- **`save_baseline.py`** writes `build/baselines/<short sha>.exe`. It refuses when tracked files differ from the
  commit.
- **`run_regression.py`** (quick tier, about 2 minutes) runs the renderer harnesses
  (`scripts/validate_p8_*.py`, `validate_p4_exit_gate.py`) three at a time, then the built-in self-checks and the ASOBO
  harnesses (`validate_asobo_*.py`) one at a time, against that baseline. They check byte-identical captures and
  identical selections. Window screenshots are compared on their viewport area only, so side-panel UI changes do
  not fail them (`validate_p4_exit_gate.py` covers the panels). The script's docstring lists what each suite covers.
- **`--tier full`** adds each feature's same-binary A/B switch, `validate_p5_1.py`, and `validate_p8_5.py`'s
  10,000-drawable and MSAA/TAA picking cases.
- **Harness launches never request keyboard focus**: they go through `scripts/harness_process.py`, which sets `GLIDER_NO_FOCUS=1` for the current build and any baseline that carries the marker. Older baselines are only started with `SW_SHOWNOACTIVATE` and may still take focus until the next commit's baseline carries the marker; the pinned references (`167a5e0`, `a9c0a29`) are cached under `build/regression_cache/` after their first render, keyed by the exe, the exact arguments and the model's input files, so repeat runs skip the launch. `scripts/check_no_focus.py` checks the current build.
- **Benchmarks** never run here. They back timing claims only: pass `--bench` to the harness concerned, and do not
  run them in parallel.

### 4.6 Third-party code

There are no git submodules. GLM is vendored as headers in `thirdparty/glm` (version in its README); the other
libraries are fetched by CMake `FetchContent` (`cmake/standalone_dependencies.cmake`). The glTF, Khronos and ASOBO
JSON schemas live in `resources/schemas`. The UX3D libraries' own unit tests (`UX3D_RUNTIME_TESTING`,
`UX3D_TOOLS_TESTING`) and the editor's `Operations` library are not built.
