# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Shared defaults suite: the default views against the baseline binary, rendered once per binary.

Twelve feature suites used to render the same default views on both binaries to prove that their feature,
off by default, leaves the image unchanged. This suite renders them once on the candidate and once on the
baseline (--baseline-exe):

  * HelloCube (no model; 1280x720, AA none, frame 24, profile report), exactly the argument list the suites'
    render helpers built;
  * the E195 reference views exterior_fuselage_nose and cockpit_main_displays (docs/ASOBO_REFERENCE_VIEWS.md,
    validate_asobo_common.render_view, ASOBO on).

Then it runs every comparison the suites made against one of those baseline renders, with the suite's own
wording, tolerance (byte identity) and, for "non-default settings while off" variants, the suite's own render
helper and flags (imported from the suite, so the argument list is the same), each line prefixed with the suite
name:

  sun               HelloCube and both E195 views with the sun off
  shadows           HelloCube and both E195 views with the sun off
  ground            HelloCube and both E195 views with the default ground; HelloCube --ground grid = default
  ssao              HelloCube and both E195 views with SSAO off (default)
  time_of_day       both E195 views with validate_time_of_day.CLOCK_FLAGS_OFF (clock settings, mode off)
  sky               HelloCube default and with validate_sky.SKY_FLAGS_HDR; both E195 views with SKY_FLAGS_HDR
  sun_disk          HelloCube default and with validate_sun_disk.DISK_FLAGS; both E195 views with DISK_FLAGS
  night             HelloCube default, with validate_night.NIGHT_FLAGS and with EXPLICIT_MANUAL; both E195 views
                    with NIGHT_FLAGS
  ambient           HelloCube, HDR source, ambient default (off)
  sky_occlusion     HelloCube (HDR) and its skyOcclusion report (automatic, off, nothing baked); both E195 views
  ocean             HelloCube and the E195 exterior with the default grid
  sun_source        HelloCube HDR defaults
  asobo_sky_portal  HelloCube and the E195 cockpit_main_displays view

Comparisons whose baseline side is not one of these default renders (suite fixtures, the sun or the plane on,
the procedural sky, explicit --environment hdr as in validate_emissive_units.py, the DA62 cockpit, the
time_of_day HelloCube captures with their own argument order) stay in their suites. Each suite module's
render log and the shared HelloCube renders must have zero GL errors.
"""

import argparse
import sys
from pathlib import Path

from glider_harness import gl_errors_zero, read_json, run
from validate_asobo_common import EXE, REPO, render_view, same, views

OUT = REPO / "build" / "defaults"
# The HelloCube default capture of sun, shadows, ground, ssao, sky, sun_disk, night, ambient, sky_occlusion,
# ocean, sun_source and asobo_sky_portal (their COMMON, no view flags, then the capture arguments).
HELLOCUBE = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
E195_VIEWS = ("exterior_fuselage_nose", "cockpit_main_displays")

failures = []
renders = {}       # (kind, exe) -> result, so each default view renders once per binary
shared_logs = []   # (name, log) of the candidate's HelloCube renders


def check(suite, condition, label):
    label = f"{suite}: {label}"
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


def is_candidate(exe):
    return Path(exe) == Path(EXE)


def hellocube(exe):
    """(png or None, profile report or None) of the HelloCube default view on `exe`."""
    key = ("hellocube", str(exe))
    if key not in renders:
        OUT.mkdir(parents=True, exist_ok=True)
        tag = "now" if is_candidate(exe) else "ref"
        png, report = OUT / f"hellocube_{tag}.png", OUT / f"hellocube_{tag}.json"
        png.unlink(missing_ok=True)
        report.unlink(missing_ok=True)
        result = run([exe] + HELLOCUBE + ["--capture-viewport", png, "--capture-frame", "24", "--profile-report", report,
                                          "--exit-after-profile"])
        if is_candidate(exe):
            shared_logs.append(("hellocube", result.stdout + result.stderr))
        renders[key] = (png if png.exists() else None, read_json(report))
    return renders[key]


def reference_view(name):
    try:
        return next(v for v in views() if v["name"] == name)
    except StopIteration:
        return None


def e195(exe, name):
    """The E195 reference view `name` (no extra flags) on `exe`: PNG path or None."""
    key = (name, str(exe))
    if key not in renders:
        entry = reference_view(name)
        tag = "now" if is_candidate(exe) else "ref"
        renders[key] = render_view(exe, entry, f"{name}_{tag}", folder=OUT) if entry else None
    return renders[key]


def e195_against_baseline(suite, baseline, label, flags=None, module=None):
    """Each E195 view on the candidate (shared default render, or `flags` through the suite's own render_view call
    into the suite's folder, as the suite did) against the baseline's shared default render; SKIP when neither
    binary rendered it (asset not available), as in the suites."""
    for name in E195_VIEWS:
        entry = reference_view(name)
        if entry is None:
            print(f"  SKIP {suite}: reference view {name} not found")
            continue
        if flags is None:
            now = e195(EXE, name)
        else:
            module.OUT.mkdir(parents=True, exist_ok=True)
            now = render_view(EXE, entry, f"base_now_{name}", flags, folder=module.OUT)
        before = e195(baseline, name)
        if now is None and before is None:
            print(f"  SKIP {suite}: {name}: E195 asset not available")
            continue
        check(suite, same(now, before), label.format(name=name))


def sun(baseline):
    print("[sun: sun off is byte-identical to the baseline]")
    check("sun", same(hellocube(EXE)[0], hellocube(baseline)[0]), f"hellocube: sun off is byte-identical to {baseline.stem}")
    e195_against_baseline("sun", baseline, "E195 {name} (ASOBO): sun off is byte-identical to " + baseline.stem)


def shadows(baseline):
    print("[shadows: sun off is byte-identical to the baseline]")
    a, b = hellocube(baseline)[0], hellocube(EXE)[0]
    check("shadows", a is not None and same(a, b), f"hellocube: sun off is byte-identical to {baseline.stem}")
    for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
        a, b = e195(baseline, name), e195(EXE, name)
        check("shadows", a is not None and same(a, b), f"{name}: sun off is byte-identical to {baseline.stem}")


def ground(baseline):
    import validate_ground as suite
    print("[ground: Grid is byte-identical to the baseline]")
    a, b = hellocube(baseline)[0], hellocube(EXE)[0]
    suite.OUT.mkdir(parents=True, exist_ok=True)
    c, _ = suite.render(None, "cur_hellocube_grid", ["--ground", "grid"])
    check("ground", a is not None and same(a, b), f"HelloCube: the default ground is byte-identical to {baseline.stem}")
    check("ground", b is not None and same(b, c), "HelloCube: --ground grid equals the default")
    for name in E195_VIEWS:
        a, b = e195(baseline, name), e195(EXE, name)
        check("ground", a is not None and same(a, b), f"{name}: byte-identical to {baseline.stem}")
    return suite


def ssao(baseline):
    print("[ssao: off (default) is byte-identical to the baseline]")
    base_cube, cur_cube = hellocube(baseline)[0], hellocube(EXE)[0]
    check("ssao", base_cube is not None and same(base_cube, cur_cube), f"HelloCube: default is byte-identical to {baseline.stem}")
    for name in E195_VIEWS:
        a, b = e195(baseline, name), e195(EXE, name)
        check("ssao", a is not None and same(a, b), f"{name}: default is byte-identical to {baseline.stem}")


def time_of_day(baseline):
    import validate_time_of_day as suite
    print("[time_of_day: clock settings with the mode off are byte-identical to the baseline]")
    e195_against_baseline("time_of_day", baseline, "E195 {name} (ASOBO), clock settings with the mode off: "
                          "byte-identical to " + baseline.stem, flags=suite.CLOCK_FLAGS_OFF, module=suite)


def sky(baseline):
    import validate_sky as suite
    print("[sky: the HDR source is byte-identical to the baseline]")
    before = hellocube(baseline)[0]
    check("sky", same(hellocube(EXE)[0], before), f"HelloCube (default): byte-identical to {baseline.stem}")
    suite.OUT.mkdir(parents=True, exist_ok=True)
    now, _ = suite.render(None, "base_now_hellocube_sky_settings_hdr", suite.SKY_FLAGS_HDR)
    check("sky", same(now, before), f"HelloCube (sky_settings_hdr): byte-identical to {baseline.stem}")
    e195_against_baseline("sky", baseline, "E195 {name} (ASOBO), sky settings with the HDR source: byte-identical to "
                          + baseline.stem, flags=suite.SKY_FLAGS_HDR, module=suite)
    return suite


def sun_disk(baseline):
    import validate_sun_disk as suite
    print("[sun_disk: sun off, HDR source: byte-identical to the baseline]")
    before = hellocube(baseline)[0]
    check("sun_disk", same(hellocube(EXE)[0], before), f"HelloCube (default): byte-identical to {baseline.stem}")
    suite.OUT.mkdir(parents=True, exist_ok=True)
    now, _ = suite.render(None, "base_now_hellocube_disk_settings", suite.DISK_FLAGS)
    check("sun_disk", same(now, before), f"HelloCube (disk_settings): byte-identical to {baseline.stem}")
    e195_against_baseline("sun_disk", baseline, "E195 {name} (ASOBO), disk/lens settings with the sun off: "
                          "byte-identical to " + baseline.stem, flags=suite.DISK_FLAGS, module=suite)
    return suite


def night(baseline):
    import validate_night as suite
    print("[night: defaults are byte-identical to the baseline]")
    before = hellocube(baseline)[0]
    check("night", same(hellocube(EXE)[0], before), f"HelloCube (default): byte-identical to {baseline.stem}")
    suite.OUT.mkdir(parents=True, exist_ok=True)
    for name, flags in (("night_settings", suite.NIGHT_FLAGS), ("explicit_manual", suite.EXPLICIT_MANUAL)):
        now, _ = suite.render(None, f"base_now_hellocube_{name}", flags)
        check("night", same(now, before), f"HelloCube ({name}): byte-identical to {baseline.stem}")
    e195_against_baseline("night", baseline, "E195 {name} (ASOBO), night settings with the HDR source: byte-identical to "
                          + baseline.stem, flags=suite.NIGHT_FLAGS, module=suite)
    return suite


def ambient(baseline):
    print("[ambient: ambient off is byte-identical to the baseline]")
    check("ambient", same(hellocube(EXE)[0], hellocube(baseline)[0]), f"HelloCube hdr (default): byte-identical to {baseline.stem}")


def sky_occlusion(baseline):
    print("[sky_occlusion: HDR default (off) is byte-identical to the baseline]")
    now, data = hellocube(EXE)
    check("sky_occlusion", same(now, hellocube(baseline)[0]), f"HelloCube (HDR): byte-identical to {baseline.stem}")
    so = (data or {}).get("skyOcclusion") or {}
    check("sky_occlusion", so.get("setting") == "auto" and so.get("enabled") is False and so.get("active") is False
          and so.get("volume") is None,
          "HDR default: sky occlusion automatic and off, nothing baked (get_state / --profile-report skyOcclusion)")
    for name in ("cockpit_main_displays", "exterior_fuselage_nose"):
        a, b = e195(EXE, name), e195(baseline, name)
        if a is None and b is None:
            print(f"  SKIP sky_occlusion: {name}: E195 asset not available")
            continue
        check("sky_occlusion", same(a, b), f"E195 {name} (HDR): byte-identical to {baseline.stem}")


def ocean(baseline):
    print("[ocean: Grid against the baseline]")
    a, b = hellocube(baseline)[0], hellocube(EXE)[0]
    check("ocean", same(a, b), f"HelloCube (default grid): byte-identical to {baseline.stem}")
    a, b = e195(baseline, "exterior_fuselage_nose"), e195(EXE, "exterior_fuselage_nose")
    check("ocean", a is not None and same(a, b), f"E195 exterior_fuselage_nose (default): byte-identical to {baseline.stem}")


def sun_source(baseline):
    print("[sun_source: HDR defaults]")
    check("sun_source", same(hellocube(EXE)[0], hellocube(baseline)[0]), "HDR defaults: byte-identical")


def asobo_sky_portal(baseline):
    print("[asobo_sky_portal: files without portals are byte-identical to the baseline]")
    check("asobo_sky_portal", same(hellocube(EXE)[0], hellocube(baseline)[0]), f"HelloCube: byte-identical to {baseline.stem}")
    a, b = e195(EXE, "cockpit_main_displays"), e195(baseline, "cockpit_main_displays")
    check("asobo_sky_portal", a is not None and same(a, b), f"E195 cockpit_main_displays: byte-identical to {baseline.stem}")


SUITES = {"sun": sun, "shadows": shadows, "ground": ground, "ssao": ssao, "time_of_day": time_of_day, "sky": sky,
          "sun_disk": sun_disk, "night": night, "ambient": ambient, "sky_occlusion": sky_occlusion, "ocean": ocean,
          "sun_source": sun_source, "asobo_sky_portal": asobo_sky_portal}


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", help=f"comma-separated suite names ({', '.join(SUITES)})")
    args = parser.parse_args()
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is not None and not baseline.is_absolute():
        baseline = REPO / baseline
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    wanted = [name.strip() for name in args.only.split(",")] if args.only else list(SUITES)
    unknown = [name for name in wanted if name not in SUITES]
    if unknown:
        sys.exit(f"unknown suite(s): {', '.join(unknown)}")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary found")
        if "ocean" in wanted:
            check("ocean", False, f"baseline binary present ({baseline})")
    else:
        print(f"  INFO candidate {EXE}, baseline {baseline.name}")
        modules = []
        for name in wanted:
            module = SUITES[name](baseline)
            if module is not None:
                modules.append((name, module))
        print("[GL errors]")
        bad = [name for name, log in shared_logs if not gl_errors_zero(log)]
        check("defaults", not bad, f"zero GL errors in the {len(shared_logs)} shared HelloCube render(s) {bad}")
        for name, module in modules:
            logs = getattr(module, "gl_logs", [])
            bad = [label for label, log in logs if not gl_errors_zero(log)]
            check(name, not bad, f"zero GL errors in {len(logs)} renders {bad}")
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
