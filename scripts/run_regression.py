# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Renderer regression run: the current build against a saved baseline binary.

Usage:
  python scripts/run_regression.py                  # quick tier (core + suites triggered by the diff), baseline of HEAD
  python scripts/run_regression.py --tier full      # every suite and each feature's A/B switch
  python scripts/run_regression.py --plan           # print the selected suites and why, launch nothing
  python scripts/run_regression.py --plan --changed resources/shaders/ssao.glsl   # selection for these paths
  python scripts/run_regression.py --baseline build/baselines/23f61c6.exe --extra=--show-invisible-materials
  python scripts/run_regression.py --extra=--asobo --extra=off   # uncommitted work that changes ASOBO rendering
  python scripts/run_regression.py --only sun,sky                 # a subset of the tier (suite names as printed)
  python scripts/run_regression.py --on-demand night              # also night's on-demand part (--sweep)
  python scripts/run_regression.py --tier full --with-on-demand   # every suite (also the on-demand suites) with
                                                                  # every on-demand part
  python scripts/run_regression.py --no-cache                     # start every launch (cold timing; no memo, no seeding)
  python scripts/run_regression.py --no-memo                      # the candidate's repeated launches start again
  GLIDER_CANDIDATE_EXE=build/baselines/474b81c.exe python scripts/run_regression.py --only ocean   # other candidate

Tiers. The full tier runs every suite. The quick tier runs the core suites (CORE: p8_1, p4_exit_gate,
asobo_common, defaults and the self-checks) plus the suites the changed files trigger (TRIGGERS below). Changed
files are `git diff --name-only HEAD` plus untracked files under source/, header/, resources/,
OpenGL-Renderer/ and scripts/ (or the --changed list). docs/, scratch/, legacy/, Markdown outside
resources/ and top-level files other than CMakeLists.txt are ignored. A suite also runs when its own
scripts/validate_<suite>.py or a script it imports or names (recursively) changed; a change to
scripts/run_regression.py itself runs the core only. BROAD paths (pbr.frag, primitive.vert, ibl.glsl,
brdf.glsl, Application.*, Frame.cpp, Scene.cpp, CMake, thirdparty/, the default model and HDR) select
every suite. A resources/ file outside the shaders triggers the suites whose scripts name it. Any other
file that matches no trigger selects every suite and prints a warning naming it, so TRIGGERS can be
extended. --only replaces the selection with the named suites of the tier.

On-demand parts (ON_DEMAND below): a few slow sections of a suite run only when its flag is passed: night
--sweep (the evening exposure sweeps and the minute-by-minute emissive blend), area_maximize --keys (the whole
keys-and-menus section; one Ctrl+Space round trip always runs) and sky_occlusion --da62 (the DA62 SDK cockpit
in the real-asset bake checks). The quick tier passes the flag when a changed path matches the part's trigger
patterns (subsets of the suite's TRIGGERS group, plus its own script); --on-demand a,b selects those suites
with their flags in any tier; --with-on-demand passes every flag of the selected suites. The full tier does
not run them otherwise.

On-demand suites (ON_DEMAND_SUITES below): p8_followup, texture_dds, texture_ktx2, asobo_model,
asobo_support, asobo_normal and asobo_occlusion are not in the full tier. The quick tier still runs them when
a changed path triggers them (TRIGGERS, their own scripts); --on-demand <suite> or --with-on-demand (with
--tier full: every suite) selects them in any tier. --plan lists them with "on-demand suite:".

Candidate: GLIDER_CANDIDATE_EXE=<path> (relative to the repository) replaces build/bin/Release/Glider.exe
in this script and every harness (harness_process.candidate_exe()), e.g. to prove that a known-bad binary
from build/baselines/ fails. That binary then counts as the current build: never cached, and with the
variable set every cache key also holds the candidate's SHA-256.

Baseline render cache: scripts/harness_process.py stores what each baseline launch wrote (captures,
reports, UI-script shots, stdout/stderr) under build/regression_cache/, keyed by the baseline's SHA-256,
the exact arguments, the content of every input file (models and the files they name, HDRs, UI
scripts), render-relevant environment variables and the machine's display drivers/DPI. A warm run
replays those instead of starting the baseline. Output paths are not part of the key (their position and
suffix are); a replay writes the stored files to the launch's own paths and rewrites the stored paths in
text outputs and stdout/stderr. Stale entries (exe gone or changed) and the entries of all but the 4 most
recently used executables ($GLIDER_HARNESS_CACHE_EXES) are pruned at the start of each run. Shared helpers
for harnesses are in scripts/glider_harness.py.

In-run memo: within one invocation, a candidate launch whose output the cache rules call deterministic
(the same eligibility as the baseline cache) is stored in build/regression_memo/run-<pid>-<time>/ under the
same kind of key (it holds the candidate's SHA-256); an identical launch later in the run (another suite's
copy of the same view) replays it ("memo hit"). The directory is deleted when the run ends, so nothing
crosses invocations or executables. Harness checks that launch the same arguments twice to prove
determinism wrap the second launch in harness_process.fresh(). --no-memo turns it off.

Baseline seeding: each stored memo entry is also copied into build/regression_cache/ under its key
("seeded") when no entry is there yet and GLIDER_CANDIDATE_EXE is not set. After the commit,
save_baseline.py copies the same Glider.exe to build/baselines/<sha>.exe; the SHA-256 is the same, so the
first run against that baseline replays the seeded entries ("hit, seed") instead of a cold baseline.

At the end the run prints each suite's wall time and Glider launches (current build, baseline,
baseline served from the cache, baseline started, repeated argument sets, control-pipe/follow sessions
and their summed seconds), slowest first, and writes build/regression/summary.json; each suite's
launches are in build/regression/<suite>.launches.jsonl.

Save a baseline right after each commit with scripts/save_baseline.py. Without
--baseline, the run uses build/baselines/<HEAD short sha>.exe, so it answers "did my
uncommitted change alter anything since the last commit?".

Suites (equivalence against the baseline plus each feature's analytic acceptance checks):
  validate_p8_1.py     9 models x 4 AA modes, plus scripted UI cases
  validate_p8_3.py     instancing grid in 11 situations (wireframe, ortho, playback, split viewports...)
  validate_p8_5.py     --quick: picking clicks on the 1,600-drawable grid, the fixture (AA none), ortho,
                       wireframe and alpha masks
  validate_p8_followup.py --only traversal: three TAA viewports sharing one scene traversal
  validate_p4_exit_gate.py  every UI producer of a scene revision
  validate_defaults.py the shared default views (HelloCube, E195 exterior and cockpit) once per binary against the
                       baseline, plus every suite's comparison of them (and its "settings while off" variants),
                       prefixed with the suite name
  the built-in self-checks (last): revision, blend, aa-target, cache-invalidation, compute, sampler, vao,
  polygon-mode, cull-winding and depth; all but cache-invalidation and compute also run with
  --selfcheck-force-failure, which must exit 1 with a report (the contracts of the retired P3.x/P4.1 scripts)
  then, one at a time: validate_asobo_model.py, validate_asobo_normal.py, validate_asobo_common.py,
  validate_asobo_support.py, validate_asobo_draworder.py, validate_asobo_packaged.py,
  validate_asobo_uv_options.py
  validate_asobo_occlusion_strength.py, validate_asobo_fresnel_ghost.py, validate_asobo_dither.py,
  validate_asobo_anisotropy.py, validate_asobo_sss.py, validate_sun.py (A3.1 sun; sun off against the baseline),
  validate_shadows.py (A3.2 cascaded sun shadows; sun off and shadows off against the baseline),
  validate_ground.py (A3.6 ground plane: shadow, lit value, IBL, model base, stability; Grid against the baseline),
  validate_ssao.py (A3.5 SSAO: box interior/open plane, no halo, TAA stability, sun unchanged; off against the baseline),
  validate_asobo_sky_portal.py (A3.7 sky portals: DA62 parse, fixture room against the analytic disc light,
  switches and files without portals against the baseline),
  validate_time_of_day.py (A6.1 solar/lunar position against reference tables, presets, heading, fade, automatic
  day/night, registry/CLI; mode off against the baseline),
  validate_sky.py (A6.2/A6.3 procedural sky against a float64 reference, sunset, sun transmittance, IBL following the
  sky, time-sliced budget and swaps; the HDR source against the baseline),
  validate_sun_disk.py (A6.7 sun disk: position/size/radiance/limb darkening, not in the IBL, mirror highlight, glare
  and lens effect, registry/CLI; the sun off against the baseline),
  validate_background_projection.py (HDR background markers, sun disk and seams on the projected world directions at
  several views, FoVs, aspects and AA modes; the baseline must miss; orthographic against the baseline),
  validate_night.py (A6.4-A6.6 exposure across the day, moon position/phase/moonlight/shadows, stars and sidereal
  rotation, emissive day/night blend, registry/CLI; the HDR defaults against the baseline),
  validate_ambient.py (A3.8 viewer ambient light: Lambert/metal radiance, AO/SSAO like the IBL, emissive and
  background unchanged, CLI = pipe, HDR = sky; ambient off against the baseline),
  validate_emissive_units.py (A1.6b ASOBO sky emissive in cd/m^2, HDR/core parity, explicit overrides and bloom),
  validate_sky_occlusion.py (A3.9 sky occlusion: open box against the analytic opening, occluders, thin-wall leak,
  open plane/spheres unchanged, bake completion and swap, real-asset bake cost; the HDR default against the baseline),
  validate_ocean.py (A6.8a FFT ocean: loop seam, deterministic captures, CLI = pipe, GL 4.3 gate, hull depth, not
  pickable, model interior, GPU cost and precompute; Grid/Plane against the baseline),
  validate_asobo_decal_modes.py,
  validate_review_fixes.py (v1.2.0 review fixes: no renderer sync while the clock plays, SSAO freed on viewport close,
    no stale sky-occlusion volume without casters, pipe time_of_day_mode = Inspector Sun Source),
  validate_agent_control.py (control pipe, shared command registry, MCP shim; CLI flags against the baseline),
  validate_viewport_image.py (Viewport capture/save/copy, capture folder setting),
  validate_area_maximize.py (Maximize Area / Full Screen / drag-and-drop open: restored layout and byte-identical
  viewport image, borderless full screen and Esc, keys and menus, drops, settings.ini keeps the restored layout),
  validate_settings_scopes.py (Quality levels / scene appearance and shared ground / viewport Shaded and overlays:
  legacy flags against the baseline, CLI = pipe, migration/restart, shared ocean, UI ID conflicts),
  validate_meshopt.py (EXT/KHR_meshopt_compression: compressed fixtures written by build/tools meshopt_fixture_gen
  render byte-identical to their plain twins, fallback buffers, decode failure / extension off refuse the load,
  validator notices for unsupported required / used extensions; plain twins against the baseline)
Full tier: every suite above except the on-demand suites (see above), plus
  --with-ab (each feature against its own switch; validate_p8_5.py then also runs the P5.1 cases with
  --always-pick) and validate_p8_5.py's 10,000-drawable grid and MSAA/TAA fixture copies (the quick tier passes
  --quick). validate_p5_1.py is superseded: its on-demand picking counters run in validate_p8_5.py.
Window screenshots (p8_1, p8_3, p8_5) are compared byte for byte on the viewports' 3D image only
(glider_harness.viewport_sha: below the menu bar, viewport tabs and viewport toolbar, between the side
panels, above the status bar), so changes to the toolbar or the panels do not fail render checks; the
toolbar and panels are covered by validate_p4_exit_gate.py.
Benchmarks never run here: they back timing claims (each harness's --bench).

Harnesses run in parallel (--jobs). Captures are deterministic under --fixed-frame-time, but
validate_p8_5.py's 1-px edge clicks have failed while heavy E195 renders ran beside it, so the ASOBO
harnesses run after the parallel batch. Timing runs must not be parallel and are not part of this
script.
"""

import argparse
import ast
import fnmatch
import json
import os
import re
import harness_process as subprocess
import sys
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
EXE = subprocess.candidate_exe()  # build/bin/Release/Glider.exe, or $GLIDER_CANDIDATE_EXE
if os.environ.get(subprocess.CANDIDATE_ENV, "").strip():
    os.environ[subprocess.CANDIDATE_ENV] = str(EXE)  # harnesses resolve the same absolute path
OUT = REPO / "build" / "regression"
SELFCHECKS = ("revision-selfcheck", "blend-selfcheck", "aa-target-selfcheck", "cache-invalidation-selfcheck",
              "compute-selfcheck", "sampler-selfcheck", "vao-selfcheck", "polygon-mode-selfcheck",
              "cull-winding-selfcheck", "depth-selfcheck")
# Self-checks whose forced failure (--selfcheck-force-failure) must exit 1 with a report; they carry the
# contracts of the retired P3.x / P4.1 scripts (validate_p3_*.py, validate_p4_1.py, see git history).
FORCED_FAILURE = {"revision-selfcheck", "blend-selfcheck", "aa-target-selfcheck", "sampler-selfcheck", "vao-selfcheck",
                  "polygon-mode-selfcheck", "cull-winding-selfcheck", "depth-selfcheck"}
FIXTURES = (("build/p8/fixtures/many_1600.gltf", "40", "none"), ("build/p8/fixtures/many_10000.gltf", "100", "none"),
            ("build/p8/fixtures/many_leaf.gltf", "40", "leaf"), ("build/p8/fixtures/many_root.gltf", "40", "root"))

# ----------------------------------------------------------------------------- quick-tier selection
# Core suites run in every quick tier: they answer "did any image change" on real assets in every AA mode
# (p8_1), every UI producer and panel (p4_exit_gate), ASOBO on/off and per switch on the E195 (asobo_common)
# and the GL contracts of the self-checks.
CORE = ("p8_1", "p4_exit_gate", "asobo_common", "defaults") + SELFCHECKS
# Paths that select every suite. A pattern without "/" matches the file name, one with "/" the whole
# repository-relative path; fnmatch, case-insensitive, "*" also crosses "/".
BROAD = ("pbr.frag", "primitive.vert", "ibl.glsl", "brdf.glsl", "Application.cpp", "Application.h",
         "source/imgui/application/Frame.cpp", "source/imgui/application/Scene.cpp", "CMakeLists.txt", "*.cmake",
         "cmake/*", "thirdparty/*", "*/thirdparty/*", "HelloCube.glb", "default_hdr_env4k.hdr")
ASOBO_SUITES = ("asobo_model", "asobo_normal", "asobo_support", "asobo_draworder", "asobo_uv_options",
                "asobo_occlusion", "asobo_fresnel_ghost", "asobo_dither", "asobo_anisotropy", "asobo_sss",
                "asobo_sky_portal", "asobo_packaged", "asobo_decal_modes", "emissive_units", "texture_dds",
                "texture_ktx2")
# (suites, path patterns), docs/BUILD_AND_TEST.md section 4.5; every pattern matches files in the tree.
TRIGGERS = (
    (("p8_3", "p8_5", "p8_followup"),
     ("OpenGL-Renderer/renderer/*Command*", "GltfRenderLayer*", "Viewports.cpp", "DiagnosticsPicking.cpp",
      "picking.frag", "PickingRenderLayer.*", "TransformHierarchy*", "SceneRevisions.*",
      "generate_many_drawables_fixture.py")),
    (("sun", "shadows"),
     ("SunShadowMap.*", "shadow*.glsl", "shadow_caster.frag", "SceneEnvironment.*", "punctual.glsl",
      "CascadedShadows.*", "ShadowCasterLayer.*")),
    (("ground", "ocean", "review_fixes"),
     ("GroundSettings.*", "ground*.frag", "ground*.vert", "ground*.glsl", "ocean*", "OceanSimulation.*", "Ocean.cpp",
      "GridRenderer.*", "grid.frag", "grid.vert")),
    (("ssao", "ambient", "sky_occlusion", "review_fixes"),
     ("AmbientOcclusion.*", "Ssao*", "ssao*", "SkyOcclusion.*", "sky_occlusion*")),
    (("sky", "sun_disk", "background_projection", "night", "time_of_day", "sun_source", "emissive_units"),
     ("Sky*", "sky_*", "SunDisk.*", "cubemap.*", "NightSky.*", "TimeOfDay.*", "SolarPosition.*",
      "EnvironmentState.cpp", "CommandsSettingsEnvironment.cpp", "tonemapping.glsl", "environment_block.glsl",
      "EnvironmentMap.*", "IBLSampler.*", "*/ibl_sampler/*")),
    (ASOBO_SUITES,
     ("asobo_*.glsl", "decal_*", "material_info.glsl", "textures.glsl", "AsoboExtension*", "AsoboSupport.*",
      "resources/schemas/asobo/*", "TextureUtils.*", "Decal*", "DrawOrder.h", "TextureDds.*", "Dds.*", "Ktx2*",
      "GltfRenderLayerMaterials.cpp")),
    # glTF loading: EXT/KHR_meshopt_compression decode, buffers and bufferViews, extension lists, the validator.
    (("meshopt",),
     ("MeshoptCompression.*", "meshopt_*", "Buffer.cpp", "Buffer.h", "BufferView.cpp", "BufferView.h",
      "BuffersFromJson.*", "BufferViewsFromJson.*", "GltfObjectFromJson.*", "GltfFromJson.*", "GltfFromBinary.*",
      "GltfParsing.*", "OpenGL-Renderer/Tools/tools/gltf/source/Gltf.cpp", "GltfResult.h", "Extensions.cpp",
      "Extensions.h", "GltfValidator.*")),
    (("settings_scopes", "viewport_image", "area_maximize", "agent_control", "review_fixes"),
     ("Settings*", "ViewportOverrides.*", "source/imgui/application/Commands*", "ControlPipe.*",
      "CommandRegistry.*", "AreaLayout.cpp", "Menus.cpp", "ViewportImage.cpp", "Panels.cpp", "AppSettings.*",
      "CaptureSettings.*", "GltfExtensionSettings.*", "ViewportSettingsPanel.*", "ViewportToolbar.cpp",
      "GLIDER_AGENT_GUIDE.md")),
)
# On-demand parts: (suite, flag, path patterns that pass the flag in the quick tier). The patterns are taken from
# the suite's TRIGGERS group (plus its own script), narrowed to the code the part exercises.
ON_DEMAND = (
    ("night", "--sweep", ("NightSky.*", "SkyEnvironment.*", "EnvironmentState.cpp", "tonemapping.glsl",
                          "validate_night.py")),
    ("area_maximize", "--keys", ("AreaLayout.cpp", "CommandsArea.cpp", "Menus.cpp", "Application.cpp",
                                 "validate_area_maximize.py")),
    ("sky_occlusion", "--da62", ("SkyOcclusion.*", "sky_occlusion*", "validate_sky_occlusion.py")),
)
# On-demand suites: whole suites the full tier leaves out (phase acceptance of one or two commits, or a same-binary
# A/B of a switch whose image the core suites already compare against the baseline). They stay in TRIGGERS, so the
# quick tier runs them when their paths (or their own scripts) change; --on-demand <suite> or --with-on-demand
# selects them in any tier.
ON_DEMAND_SUITES = ("p8_followup", "texture_dds", "texture_ktx2", "asobo_model", "asobo_support", "asobo_normal",
                    "asobo_occlusion")
# Never part of Glider.exe or a suite.
IGNORED = ("docs/*", "scratch/*", "legacy/*", "licenses/*", ".git*", "*/.git*", ".clang-*", "*/.clang-*")
UNTRACKED_ROOTS = ("source", "header", "resources", "OpenGL-Renderer", "scripts")


def _match(path, pattern):
    path, pattern = path.lower(), pattern.lower()
    return fnmatch.fnmatchcase(path if "/" in pattern else path.rsplit("/", 1)[-1], pattern)


def _ignored(path):
    if any(_match(path, pattern) for pattern in IGNORED):
        return True
    if "/" not in path and path.lower() != "cmakelists.txt":
        return True  # AGENTS.md, README.md, LICENSE, ...
    return path.lower().endswith(".md") and not path.lower().startswith("resources/")


def _script_closure(script):
    """The scripts/ files a harness depends on: itself, the modules it imports and the scripts it names, recursively."""
    seen, todo = set(), [script]
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        path = REPO / "scripts" / name
        try:
            text = path.read_text(encoding="utf-8")
        except OSError:
            continue
        seen.add(name)
        names = set()
        try:
            tree = ast.parse(text)
        except SyntaxError:
            tree = None
        if tree is not None:
            # Docstrings name other harnesses for reference only; string literals in code name scripts it runs.
            docstrings = {id(body[0].value) for body in [tree.body] + [node.body for node in ast.walk(tree)
                          if isinstance(node, (ast.FunctionDef, ast.AsyncFunctionDef, ast.ClassDef))]
                          if body and isinstance(body[0], ast.Expr) and isinstance(body[0].value, ast.Constant)}
            for node in ast.walk(tree):
                if isinstance(node, ast.Import):
                    names.update(alias.name.split(".")[0] + ".py" for alias in node.names)
                elif isinstance(node, ast.ImportFrom) and node.module and not node.level:
                    names.add(node.module.split(".")[0] + ".py")
                elif isinstance(node, ast.Constant) and isinstance(node.value, str) and id(node) not in docstrings:
                    names.update(re.findall(r"([A-Za-z_]\w*\.py)\b", node.value))
        todo += [other for other in names if (REPO / "scripts" / other).is_file()]
    return seen


def changed_files(value):
    """Repository-relative changed paths: the --changed list, or git diff HEAD plus untracked files."""
    if value is not None:
        return sorted({item.strip().replace("\\", "/") for item in value.split(",") if item.strip()})
    tracked = subprocess.run(["git", "diff", "--name-only", "HEAD"], cwd=REPO, capture_output=True, text=True).stdout
    untracked = subprocess.run(["git", "ls-files", "--others", "--exclude-standard", "--", *UNTRACKED_ROOTS],
                               cwd=REPO, capture_output=True, text=True).stdout
    return sorted({line.strip() for line in (tracked + untracked).splitlines() if line.strip()})


def classify(path, suites, closures, texts):
    """(suite names, reason) a changed path selects; None for suite names means every suite."""
    if _ignored(path):
        return set(), "ignored"
    if any(_match(path, pattern) for pattern in BROAD):
        return None, "broad"
    if path.lower().startswith("scripts/"):
        name = path.split("/", 1)[1]
        if name == "run_regression.py":
            return set(), "the runner itself (core only)"
        users = {suite for suite in suites if name in closures.get(suite, ())}
        triggered = {suite for group, patterns in TRIGGERS if any(_match(path, pattern) for pattern in patterns)
                     for suite in group}
        if users | triggered:
            return (users | triggered), "suite script or helper"
        return set(), "no suite uses it"
    selected = {suite for group, patterns in TRIGGERS if any(_match(path, pattern) for pattern in patterns)
                for suite in group}
    if selected:
        return selected, "trigger"
    if path.lower().startswith("resources/") and "/shaders/" not in path.lower():
        base = path.rsplit("/", 1)[-1]
        named = {suite for suite, text in texts.items() if base in text}
        if named:
            return named, "named by suite scripts"
    return None, "no trigger"

parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
parser.add_argument("--tier", choices=["quick", "full"], default="quick")
parser.add_argument("--baseline", help="reference binary (default: build/baselines/<HEAD short sha>.exe)")
parser.add_argument("--jobs", type=int, default=3, help="harnesses run at the same time")
parser.add_argument("--extra", action="append", default=[], help="extra Glider argument passed to both binaries")
parser.add_argument("--no-cache", action="store_true",
                    help="start every launch (GLIDER_RENDER_CACHE=0): the cache is neither read nor written, no memo, "
                         "no seeding")
parser.add_argument("--no-memo", action="store_true",
                    help="no in-run memo of the candidate's launches (and therefore no baseline seeding)")
parser.add_argument("--only", help="comma-separated suite names to run (default: the whole tier)")
parser.add_argument("--plan", action="store_true", help="print the selected suites and why, then exit without launching")
parser.add_argument("--changed", help="comma-separated repository-relative paths used instead of the git diff "
                                      "(quick-tier selection; an empty value means nothing changed)")
parser.add_argument("--on-demand", dest="on_demand", default=None,
                    help="comma-separated suites whose on-demand part runs (their flag); selects them in any tier: "
                         + ", ".join(f"{suite} {flag}" for suite, flag, _ in ON_DEMAND))
parser.add_argument("--with-on-demand", action="store_true",
                    help="pass every on-demand flag of the selected suites (the full tier does not otherwise)")
args = parser.parse_args()
if args.no_cache:
    os.environ[subprocess.CACHE_SWITCH] = "0"

if args.baseline:
    baseline = Path(args.baseline).resolve()
else:
    sha = subprocess.run(["git", "rev-parse", "--short", "HEAD"], cwd=REPO, capture_output=True, text=True).stdout.strip()
    baseline = REPO / "build" / "baselines" / f"{sha}.exe"

extra = [f"--extra={e}" for e in args.extra]
ab = ["--with-ab"] if args.tier == "full" else []
jobs = [
    ("p8_1", ["scripts/validate_p8_1.py", "--only", "equivalence", "--ref-exe", str(baseline), "--label", "regression_p8_1"] + extra),
    ("p8_3", ["scripts/validate_p8_3.py", "--ref-exe", str(baseline)] + ab + extra),
    ("p8_5", ["scripts/validate_p8_5.py", "--only", "equivalence", "--pre-exe", str(baseline)] + ab + extra
             + (["--quick"] if args.tier == "quick" else [])),
    ("p8_followup", ["scripts/validate_p8_followup.py", "--only", "traversal", "--ref-exe", str(baseline)]),
    ("p4_exit_gate", ["scripts/validate_p4_exit_gate.py"]),
]
# Run one at a time after the parallel batch: their E195 renders load the machine enough to upset
# validate_p8_5.py's timing-sensitive edge clicks when they overlap.
serial_jobs = [
    ("asobo_model", ["scripts/validate_asobo_model.py"]),
    ("asobo_normal", ["scripts/validate_asobo_normal.py"]),
    ("asobo_common", ["scripts/validate_asobo_common.py", "--baseline-exe", str(baseline)]),
    ("defaults", ["scripts/validate_defaults.py", "--baseline-exe", str(baseline)]),
    ("asobo_support", ["scripts/validate_asobo_support.py"]),
    ("asobo_draworder", ["scripts/validate_asobo_draworder.py"]),
    ("asobo_uv_options", ["scripts/validate_asobo_uv_options.py"]),
    ("asobo_occlusion", ["scripts/validate_asobo_occlusion_strength.py"]),
    ("asobo_fresnel_ghost", ["scripts/validate_asobo_fresnel_ghost.py", "--baseline-exe", str(baseline)]),
    ("asobo_dither", ["scripts/validate_asobo_dither.py", "--baseline-exe", str(baseline)]),
    ("asobo_anisotropy", ["scripts/validate_asobo_anisotropy.py", "--baseline-exe", str(baseline)]),
    ("asobo_sss", ["scripts/validate_asobo_sss.py", "--baseline-exe", str(baseline)]),
    ("sun", ["scripts/validate_sun.py", "--baseline-exe", str(baseline)]),
    ("shadows", ["scripts/validate_shadows.py", "--baseline-exe", str(baseline)]),
    ("ground", ["scripts/validate_ground.py", "--baseline-exe", str(baseline)]),
    ("ssao", ["scripts/validate_ssao.py", "--baseline-exe", str(baseline)]),
    ("asobo_sky_portal", ["scripts/validate_asobo_sky_portal.py", "--baseline-exe", str(baseline)]),
    ("time_of_day", ["scripts/validate_time_of_day.py", "--baseline-exe", str(baseline)]),
    ("sky", ["scripts/validate_sky.py", "--baseline-exe", str(baseline)]),
    ("sun_disk", ["scripts/validate_sun_disk.py", "--baseline-exe", str(baseline)]),
    ("background_projection", ["scripts/validate_background_projection.py", "--baseline-exe", str(baseline)]),
    ("night", ["scripts/validate_night.py", "--baseline-exe", str(baseline)]),
    ("ambient", ["scripts/validate_ambient.py", "--baseline-exe", str(baseline)]),
    ("sun_source", ["scripts/validate_sun_source.py", "--baseline-exe", str(baseline)]),
    ("emissive_units", ["scripts/validate_emissive_units.py", "--baseline-exe", str(baseline)]),
    ("sky_occlusion", ["scripts/validate_sky_occlusion.py", "--baseline-exe", str(baseline)]),
    ("ocean", ["scripts/validate_ocean.py", "--baseline-exe", str(baseline)]),
    ("review_fixes", ["scripts/validate_review_fixes.py", "--baseline-exe", str(baseline)]),
    ("asobo_packaged", ["scripts/validate_asobo_packaged.py"]),
    ("asobo_decal_modes", ["scripts/validate_asobo_decal_modes.py", "--baseline-exe", str(baseline)]),
    ("texture_dds", ["scripts/validate_texture_dds.py"]),
    ("texture_ktx2", ["scripts/validate_texture_ktx2.py", "--baseline-exe", str(baseline)]),
    ("agent_control", ["scripts/validate_agent_control.py", "--baseline-exe", str(baseline)]),
    ("viewport_image", ["scripts/validate_viewport_image.py"]),
    ("area_maximize", ["scripts/validate_area_maximize.py"]),
    ("settings_scopes", ["scripts/validate_settings_scopes.py", "--baseline-exe", str(baseline)]),
    ("meshopt", ["scripts/validate_meshopt.py", "--baseline-exe", str(baseline)]),
]
# validate_p5_1.py is superseded (its counter checks run in validate_p8_5.py) and no longer registered.
tier_suites = [name for name, _ in jobs + serial_jobs] + list(SELFCHECKS)
scripts = {name: command[0].split("/", 1)[1] for name, command in jobs + serial_jobs}

# Why each suite runs: "core", "full tier", "--only", or the changed paths that trigger it.
reasons, warnings, file_rows = {}, [], []
demand_flags = {suite: flag for suite, flag, _ in ON_DEMAND}
demanded = {name.strip() for name in (args.on_demand or "").split(",") if name.strip()}
if demanded - set(demand_flags) - set(ON_DEMAND_SUITES):
    sys.exit(f"no on-demand part in: {', '.join(sorted(demanded - set(demand_flags) - set(ON_DEMAND_SUITES)))} "
             f"(on-demand parts: {', '.join(demand_flags)}; on-demand suites: {', '.join(ON_DEMAND_SUITES)})")
on_demand = {}  # suite -> reasons its flag is passed
if args.only:
    wanted = {name.strip() for name in args.only.split(",") if name.strip()}
    if wanted - set(tier_suites):
        sys.exit(f"unknown suite(s): {', '.join(sorted(wanted - set(tier_suites)))}")
    reasons = {name: ["--only"] for name in tier_suites if name in wanted}
elif args.tier == "full":
    reasons = {name: ["full tier"] for name in tier_suites
               if name not in ON_DEMAND_SUITES or args.with_on_demand}
else:
    reasons = {name: ["core"] for name in CORE}
    closures = {suite: _script_closure(script) for suite, script in scripts.items()}
    texts = {}
    for suite, names in closures.items():
        parts = []
        for name in names:
            try:
                parts.append((REPO / "scripts" / name).read_text(encoding="utf-8"))
            except OSError:
                pass
        texts[suite] = "\n".join(parts)
    for path in changed_files(args.changed):
        selected, why = classify(path, scripts, closures, texts)
        if selected is None:
            selected = set(tier_suites)
            if why == "no trigger":
                warnings.append(f"WARNING: {path} matches no trigger; every suite selected "
                                f"(extend TRIGGERS in scripts/run_regression.py)")
        selected &= set(tier_suites)
        file_rows.append((path, why, selected))
        for suite in selected:
            reasons.setdefault(suite, []).append(path)
    unreachable = [suite for suite in scripts if suite not in CORE
                   and not any(suite in group for group, _ in TRIGGERS)]
    warnings += [f"WARNING: suite {suite} has no path trigger besides its own scripts" for suite in unreachable]
for suite in sorted(demanded):
    reasons.setdefault(suite, []).append("--on-demand")
    if suite in demand_flags:
        on_demand.setdefault(suite, []).append("--on-demand")
if args.with_on_demand:
    for suite in demand_flags:
        if suite in reasons:
            on_demand.setdefault(suite, []).append("--with-on-demand")
if args.tier == "quick" and not args.only:
    for suite, flag, patterns in ON_DEMAND:
        hits = [path for path, _, selected in file_rows if suite in selected and
                any(_match(path, pattern) for pattern in patterns)]
        if hits:
            on_demand.setdefault(suite, []).extend(hits)
jobs = [job for job in jobs if job[0] in reasons]
serial_jobs = [(name, command + ([demand_flags[name]] if name in on_demand else []))
               for name, command in serial_jobs if name in reasons]
selfchecks = [check for check in SELFCHECKS if check in reasons]

if args.plan:
    source = "--only" if args.only else ("full tier" if args.tier == "full" else
                                         ("--changed" if args.changed is not None else "git diff HEAD + untracked"))
    print(f"[plan: {args.tier} tier, candidate {EXE} vs baseline {baseline.name}; selection from {source}]")
    if file_rows:
        print(f"changed files ({len(file_rows)}):")
        for path, why, selected in file_rows:
            names = "every suite" if selected == set(tier_suites) else (", ".join(sorted(selected)) or "-")
            print(f"  {path}  [{why}] -> {names}")
    elif args.tier == "quick" and not args.only:
        print("changed files: none")
    order = [name for name in tier_suites if name in reasons]
    print(f"selected suites ({len(order)} of {len(tier_suites)}):")
    for name in order:
        why = reasons[name]
        label = why[0] if why[0] in ("core", "full tier", "--only", "--on-demand") else \
            "triggered by " + ", ".join(why[:3]) + (f" (+{len(why) - 3} more)" if len(why) > 3 else "")
        print(f"  {name:28s} {label}")
    skipped = [name for name in tier_suites if name not in reasons]
    if skipped:
        print(f"not selected ({len(skipped)}): {', '.join(skipped)}")
    for suite, flag, _ in ON_DEMAND:
        if suite in on_demand:
            why = on_demand[suite]
            print(f"on-demand: {suite} {flag} ({', '.join(why[:3])}" + (f" +{len(why) - 3} more" if len(why) > 3 else "") + ")")
        elif suite in reasons:
            print(f"on-demand: {suite} {flag} not run (--on-demand {suite} or a trigger path passes it)")
    for suite in ON_DEMAND_SUITES:
        if suite in reasons:
            print(f"on-demand suite: {suite} selected ({', '.join(reasons[suite][:3])})")
        else:
            print(f"on-demand suite: {suite} not run (--on-demand {suite}, --with-on-demand or a trigger path "
                  f"selects it)")
    for line in warnings:
        print(line)
    sys.exit(0)
for line in warnings:
    print(line)

if not baseline.exists():
    sys.exit(f"baseline {baseline} not found; save one after committing (scripts/save_baseline.py) or pass --baseline")
if not EXE.exists():
    sys.exit(f"no build at {EXE}")

# Shared fixtures are written once, before harnesses run side by side.
for path, size, anim in FIXTURES:
    (REPO / path).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run([sys.executable, str(REPO / "scripts" / "generate_many_drawables_fixture.py"), str(REPO / path), size, anim],
                   check=True, capture_output=True)

OUT.mkdir(parents=True, exist_ok=True)


def launch_log(name):
    """build/regression/<suite>.launches.jsonl, emptied; harness_process appends one line per Glider launch."""
    path = OUT / f"{name}.launches.jsonl"
    path.unlink(missing_ok=True)
    return path


def run_job(job):
    name, command = job
    log = OUT / f"{name}.txt"
    start = time.time()
    env = dict(os.environ)
    env[subprocess.LAUNCH_LOG] = str(launch_log(name))
    result = subprocess.run([sys.executable] + command, cwd=REPO, capture_output=True, text=True, env=env)
    log.write_text(result.stdout + result.stderr, encoding="utf-8")
    passed = result.returncode == 0 and "RESULT: PASS" in result.stdout
    fails = [line.strip() for line in result.stdout.splitlines() if line.strip().startswith("FAIL")]
    if not passed and not fails:
        # A harness that stopped (traceback) or exited without FAIL lines: show where it ended.
        tail = [line.strip() for line in (result.stdout + result.stderr).splitlines() if line.strip()]
        fails = [f"FAIL exit {result.returncode}, no FAIL line; last output: {tail[-1][:200] if tail else '(none)'}"]
    return name, passed, time.time() - start, fails, log


def run_selfcheck(check):
    log = OUT / f"{check}.txt"
    start = time.time()
    os.environ[subprocess.LAUNCH_LOG] = str(launch_log(check))
    fails = []
    try:
        result = subprocess.run([str(EXE), "--isolated", f"--{check}", str(log)], cwd=REPO, capture_output=True, text=True)
        if result.returncode != 0:
            fails.append(f"exit {result.returncode}")
        if check in FORCED_FAILURE:
            forced = OUT / f"{check}.forced.txt"
            forced.unlink(missing_ok=True)
            result = subprocess.run([str(EXE), "--isolated", f"--{check}", str(forced), "--selfcheck-force-failure"],
                                    cwd=REPO, capture_output=True, text=True)
            if result.returncode != 1 or not forced.is_file() or forced.stat().st_size == 0:
                fails.append(f"forced failure: exit {result.returncode} (expected 1 and a report)")
    finally:
        os.environ.pop(subprocess.LAUNCH_LOG, None)
    return check, not fails, time.time() - start, fails, log


def launch_summary(name):
    """One suite's Glider launches: current build, baseline (served from the cache / started), repeated argument sets."""
    records = []
    try:
        for line in (OUT / f"{name}.launches.jsonl").read_text(encoding="utf-8").splitlines():
            try:
                records.append(json.loads(line))
            except ValueError:
                pass
    except OSError:
        pass
    baseline = [r for r in records if r.get("role") == "baseline"]
    current = [r for r in records if r.get("role") == "current"]
    hits = sum(1 for r in baseline if str(r.get("cache", "")).startswith("hit"))
    sessions = [r for r in records if r.get("kind") == "popen"]
    repeats = Counter((r.get("exe"), tuple(r.get("argv", ()))) for r in records)
    return {
        "current": len(current),
        "memo_hits": sum(1 for r in current if r.get("cache") == "memo hit"),
        "seeded": sum(1 for r in current if str(r.get("cache", "")).endswith(", seeded")),
        "baseline": len(baseline),
        "baseline_hits": hits,
        "seed_hits": sum(1 for r in baseline if r.get("cache") == "hit, seed"),
        "baseline_started": len(baseline) - hits,
        "other": sum(1 for r in records if r.get("role") not in ("current", "baseline")),
        "repeated": sum(count - 1 for count in repeats.values() if count > 1),
        "uncacheable": dict(Counter(str(r.get("cache"))[5:] for r in baseline if str(r.get("cache", "")).startswith("off"))),
        "sessions": len(sessions),
        "session_seconds": round(sum(float(r.get("seconds") or 0.0) for r in sessions), 1),
    }


cache_state = "off (--no-cache)" if args.no_cache else "on"
if not args.no_cache:
    kept, removed = subprocess.prune_cache()
    cache_state += f", {kept} entries" + (f", {removed} stale removed" if removed else "")
memo = None
if not args.no_cache and not args.no_memo:
    memo = subprocess.start_memo()  # one directory for this invocation, deleted at the end
    os.environ[subprocess.SEED_ENV] = "1"
    cache_state += ", in-run memo on" + (", seeding" if subprocess.seeding_enabled() else
                                          " (no seeding: GLIDER_CANDIDATE_EXE)")
else:
    cache_state += ", in-run memo off"
try:
    candidate_name = EXE.relative_to(REPO)
except ValueError:
    candidate_name = EXE
selection = "" if args.only or args.tier == "full" else \
    f", {len(jobs) + len(serial_jobs) + len(selfchecks)} of {len(tier_suites)} suites (core + triggered)"
print(f"[regression: {args.tier} tier{selection}, candidate {candidate_name} vs baseline {baseline.name}, {args.jobs} jobs, "
      f"baseline render cache {cache_state}]")
if on_demand:
    print("[on-demand: " + ", ".join(f"{suite} {demand_flags[suite]}" for suite in on_demand) + "]")
start = time.time()
try:
    with ThreadPoolExecutor(max_workers=max(1, args.jobs)) as pool:
        results = list(pool.map(run_job, jobs))
    results += [run_job(job) for job in serial_jobs]
    results += [run_selfcheck(check) for check in selfchecks]
finally:
    if memo is not None:
        subprocess.end_memo(memo)

failed = False
summary = []
for name, passed, seconds, fails, log in results:
    print(f"  {'PASS' if passed else 'FAIL'} {name:28s} {seconds:6.1f} s  ({log.relative_to(REPO)})")
    for line in fails[:8]:
        print(f"         {line}")
    if len(fails) > 8:
        print(f"         ... {len(fails) - 8} more FAIL line(s) in the log")
    failed |= not passed
    summary.append({"suite": name, "passed": passed, "seconds": round(seconds, 1), **launch_summary(name)})

# Wall time and Glider launches per suite, slowest first (baseline: served from the cache / started).
print("-" * 60)
# current: candidate launches (memo: replayed from this run's memo); baseline: hit (seed: from a seeded entry).
print(f"  {'suite':28s} {'time':>8s} {'current':>8s} {'memo':>5s} {'baseline':>9s} {'hit':>5s} {'seed':>5s} "
      f"{'started':>8s} {'repeat':>7s} {'sessions':>9s} {'session s':>10s}")
for row in sorted(summary, key=lambda row: -row["seconds"]):
    print(f"  {row['suite']:28s} {row['seconds']:6.1f} s {row['current']:8d} {row['memo_hits']:5d} {row['baseline']:9d} "
          f"{row['baseline_hits']:5d} {row['seed_hits']:5d} {row['baseline_started']:8d} {row['repeated']:7d} "
          f"{row['sessions']:9d} {row['session_seconds']:8.1f} s")
totals = {key: sum(row[key] for row in summary)
          for key in ("current", "memo_hits", "seeded", "baseline", "baseline_hits", "seed_hits", "baseline_started",
                      "sessions", "session_seconds")}
print(f"  {'total':28s} {time.time() - start:6.0f} s {totals['current']:8d} {totals['memo_hits']:5d} "
      f"{totals['baseline']:9d} {totals['baseline_hits']:5d} {totals['seed_hits']:5d} {totals['baseline_started']:8d} "
      f"{'':7s} {totals['sessions']:9d} {totals['session_seconds']:8.1f} s")
print(f"  in-run memo: {totals['memo_hits']} of {totals['current']} candidate launches replayed; "
      f"{totals['seeded']} seeded into the baseline cache; {totals['seed_hits']} baseline launches served from seeds")
(OUT / "summary.json").write_text(json.dumps({"tier": args.tier, "candidate": str(EXE), "baseline": baseline.name,
                                              "cache": cache_state, "totals": totals, "selected": {name: reasons[name] for name in reasons},
                                              "on_demand": {name: [demand_flags[name]] + why for name, why in on_demand.items()},
                                              "on_demand_suites_not_run": [name for name in ON_DEMAND_SUITES
                                                                           if name not in reasons],
                                              "seconds": round(time.time() - start, 1), "suites": summary}, indent=1),
                                  encoding="utf-8")
print("=" * 60)
print(f"RESULT: {'FAIL' if failed else 'PASS'} ({time.time() - start:.0f} s)")
sys.exit(1 if failed else 0)
