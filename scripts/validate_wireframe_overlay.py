# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Wireframe Overlay fix: before/after evidence.

Before the fix, the overlay pass redrew createScene's transmission set-up for
scenes with transmissive materials: the duplicate Opaque2/Grid2/Transparent2
drawables and the TransmissiveBlit pass, whose post-command clears the whole
framebuffer. The shaded image was wiped and only lines (plus the redrawn
skybox) remained. The fix leaves those drawables out of the overlay scene.

For each model, viewport-only screenshots are taken with the overlay off and on,
on the pre-fix binary (build/wireframe_fix/before/Glider.exe) and the fixed one:
  * overlay off and Wireframe debug mode: byte-identical before/after,
  * models without transmission: overlay byte-identical before/after,
  * transmissive model: before, the overlay changes a large fraction of the
    viewport (image wiped); after, only a small fraction (the lines).
"""

import json
import harness_process as subprocess
import sys
from pathlib import Path

sys.modules["numpy"] = None
from PIL import Image, ImageChops  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
AFTER = REPO / "build" / "bin" / "Release" / "Glider.exe"
BEFORE = REPO / "build" / "wireframe_fix" / "before" / "Glider.exe"
OUT = REPO / "build" / "wireframe_fix"
VIEWPORT = (290, 75, 1015, 760)  # viewport image area at 1280x800 (left, top, right, bottom)
MODELS = {
    "fixture": "resources/test_models/production_passes_fixture.gltf",
    "fox": "OpenGL-Renderer/Tools/tools/operations/tests/resources/Fox/glb/Fox.glb",
    "duck": "OpenGL-Renderer/Tools/tools/operations/tests/resources/Duck/glTF-Binary/Duck.glb",
}
# Models with a transmissive material; only these add the TransmissiveBlit clear.
TRANSMISSIVE = {"fixture"}
failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)


def capture(exe, label, model, steps, aa):
    folder = OUT / label
    folder.mkdir(parents=True, exist_ok=True)
    names = [name for name, _ in steps]
    lines = ["wait 8"]
    for name, cmds in steps:
        lines += cmds + ["wait 12", f"shot {(folder / (name + '.png')).relative_to(REPO).as_posix()}"]
        (folder / f"{name}.png").unlink(missing_ok=True)
    lines.append("wait 2")
    script, report = folder / "script.txt", folder / "report.json"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    result = subprocess.run([str(exe), model, "--isolated", "--window-size", "1280x800", "--aa", aa,
                             "--camera-distance", "4", "--camera-yaw", "25", "--camera-pitch", "15",
                             "--ui-script", str(script), "--ui-report", str(report)],
                            cwd=REPO, capture_output=True, text=True, timeout=120)
    ok = result.returncode == 0 and report.exists() and not json.loads(report.read_text(encoding="utf-8")).get("failed", True)
    check(ok, f"{label}: run completed")
    return {n: Image.open(folder / f"{n}.png").convert("RGB").crop(VIEWPORT) for n in names} if ok else None


def changed_fraction(a, b):
    diff = ImageChops.difference(a, b).convert("L").point(lambda v: 255 if v > 8 else 0)
    return sum(1 for v in diff.getdata() if v) / (a.width * a.height)


for model_name, model in MODELS.items():
    for aa in ("none", "taa"):
        steps = [
            ("off", []),
            ("overlay", ["vpsetting 1 wireframe on"]),
            ("overlay_nosky", ["clickp hier.viewport 1", "wait 6", "click 1041 164"]),
            ("debug_wire", ["vpsetting 1 wireframe off", "click 1150 400", "click 930 60", "wait 6", "click 968 92"]),
        ]
        tag = f"{model_name}_{aa}"
        before = capture(BEFORE, f"before_{tag}", model, steps, aa)
        after = capture(AFTER, f"after_{tag}", model, steps, aa)
        if not before or not after:
            continue
        check(before["off"].tobytes() == after["off"].tobytes(), f"{tag}: overlay off identical before/after")
        check(before["debug_wire"].tobytes() == after["debug_wire"].tobytes(),
              f"{tag}: Wireframe debug mode identical before/after")
        if model_name not in TRANSMISSIVE:
            check(before["overlay"].tobytes() == after["overlay"].tobytes(),
                  f"{tag}: no transmission, overlay identical before/after")
            check(before["overlay_nosky"].tobytes() == after["overlay_nosky"].tobytes(),
                  f"{tag}: no transmission, overlay without skybox identical before/after")
            continue
        bug = changed_fraction(before["off"], before["overlay"])
        fixed = changed_fraction(after["off"], after["overlay"])
        check(bug > 0.05, f"{tag}: before fix, overlay changed {bug:.1%} of viewport pixels (shaded image wiped)")
        check(0.0 < fixed < 0.02, f"{tag}: after fix, overlay changes only {fixed:.1%} (lines over shaded geometry)")
        check(before["overlay_nosky"].tobytes() != after["overlay_nosky"].tobytes(),
              f"{tag}: skybox hidden, overlay output changed by the fix (the clear also wiped it)")

print("=" * 60)
print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
sys.exit(1 if failures else 0)
