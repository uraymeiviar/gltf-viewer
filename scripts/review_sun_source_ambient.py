# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.10 review: E195/DA62 noon, sunset, night off/on and exterior noon; serial, headless."""
import ctypes
import json
import os
from pathlib import Path

if os.name == "nt":
    # Keep the harness below half the logical CPUs (12 on the task host); use a pointer-sized pseudo-handle.
    core_limit = min(ctypes.sizeof(ctypes.c_size_t) * 8, max(1, (os.cpu_count() or 1) // 2))
    assert ctypes.windll.kernel32.SetProcessAffinityMask(ctypes.c_void_p(-1), ctypes.c_size_t((1 << core_limit) - 1)), ctypes.WinError()

import review_night as review
import validate_night as night
from validate_asobo_common import E195_SEARCH_PATHS, EXE, REAL_ASSETS, REPO, view
from validate_ground import without_ground_equipment

OUT = REPO / "build/review/sun_source_ambient"


def contact_sheet(rows):
    from PIL import Image, ImageDraw, ImageFont
    font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 16)
    width, thumb_height, margin = 720, 380, 12
    sheet = Image.new("RGB", (2 * width + 3 * margin, 48 + ((len(rows) + 1) // 2) * (thumb_height + 54)), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((12, 10), "A3.10: Auto ambient off / on, k=0.05; procedural sky, default exposure", font=font, fill="white")
    for index, (name, path, report) in enumerate(rows):
        x, y = margin + (index % 2) * (width + margin), 48 + (index // 2) * (thumb_height + 54)
        draw.text((x, y), name, font=font, fill="white")
        if report:
            ambient = report["ambient"]
            applied = ambient["intensity"] if ambient["enabled"] else 0.0
            draw.text((x, y + 20), f"EV {night.exposure_of(report)[0]:.3f}; E_out {ambient['outdoorIlluminanceLux']:.5g} lux; "
                      f"L_applied {applied:.5g} cd/m2", font=font, fill=(190, 200, 210))
        if path:
            image = Image.open(path).convert("RGB")
            image.thumbnail((width, thumb_height))
            sheet.paste(image, (x, y + 42))
    sheet.save(OUT / "contact_sheet.png")


def inspector():
    """UI-only input injection, screenshot and conflict telemetry, through Glider's script channel."""
    result_rows = []
    for source in ("hdr", "sky"):
        script, report = OUT / f"inspector_{source}.txt", OUT / f"inspector_{source}.json"
        commands = ["wait 24", "clickp hier.scene 0", "wait 6", "clickp scene.environment 0", "wait 6",
                    "hoverp scene.sun.source 0", "wait 6", f"snap {source}_source",
                    f"shot {OUT / ('inspector_' + source + '.png')}",
                    "clickp scene.sun.source 0", "wait 6", f"shot {OUT / ('inspector_' + source + '_options.png')}",
                    "key esc", "wait 4", "hoverp scene.tod.clock 0", "wait 6", f"snap {source}_clock",
                    "hoverp scene.sun.azimuth 0", "wait 6", f"snap {source}_azimuth",
                    "hoverp scene.sun.elevation 0", "wait 6", f"snap {source}_elevation"]
        if source == "hdr":
            commands += ["hoverp scene.sun.intensity 0", "wait 6", "snap hdr_sun_intensity"]
        commands += ["clickp scene.sun_time 0", "wait 6"]
        for section, controls in (("scene.sky", ["scene.sky.source"]),
                                  ("scene.sun_disk", ["scene.sun_disk.on", "scene.sun_disk.scale", "scene.sun_disk.limb"]),
                                  ("scene.ambient", ["scene.ambient.on", "scene.ambient.mode"]),
                                  ("scene.sky_occlusion", ["scene.sky_occlusion.mode", "scene.sky_occlusion.quality", "scene.sky_occlusion.occluders"])):
            commands += [f"clickp {section} 0", "wait 6"]
            for tag in controls:
                commands += [f"hoverp {tag} 0", "wait 6", f"snap {source}_{tag}"]
            if section == "scene.ambient":
                commands += ["set_environment ambient_mode auto", "wait 6", "hoverp scene.ambient.fill 0", "wait 6",
                             f"snap {source}_ambient_fill", f"shot {OUT / (source + '_ambient_auto.png')}",
                             "set_environment ambient_mode manual", "wait 6", "hoverp scene.ambient.color 0", "wait 6",
                             f"snap {source}_ambient_color", "hoverp scene.ambient.intensity 0", "wait 6",
                             f"snap {source}_ambient_intensity"]
            commands += [f"shot {OUT / (source + '_' + section + '.png')}", f"clickp {section} 0", "wait 4"]
        commands += ["snap final", ""]
        script.write_text("\n".join(commands), encoding="utf-8")
        result = night.run([EXE, "--isolated", "--environment", source, "--sun-source", "manual", "--sun", "on",
                            "--ambient", "on", "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--aa", "none",
                            "--vsync", "off", "--ui-script", script, "--ui-report", report])
        (OUT / f"inspector_{source}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
        data = json.loads(report.read_text()) if report.exists() else {}
        snapshots = data.get("snaps", [])
        failures = data.get("errors", [])
        conflicts = [s for s in snapshots if s.get("idConflict", -1) != 0 or s.get("hoveredIdCount", 0) > 1]
        errors = max((s["glErrors"] for s in snapshots), default=-1)
        good = result.returncode == 0 and not data.get("failed", True) and not failures and snapshots and not conflicts and errors == 0
        result_rows.append({"source": source, "ok": bool(good), "snapshots": len(snapshots),
                            "failures": failures, "conflicts": conflicts, "glErrors": errors})
        print(f"Inspector {source}: {result_rows[-1]}", flush=True)
    (OUT / "inspector_checks.json").write_text(json.dumps(result_rows, indent=2), encoding="utf-8")
    return all(row["ok"] for row in result_rows)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    review.OUT = night.OUT = OUT
    review.SKY += ["--sky-time-slicing", "off"]
    sunset, _ = night.sunset_times()
    times = (("noon", ["--time-preset", "noon"]), ("sunset", ["--time", night.hhmm(sunset)]),
             ("night", night.NIGHT_EMISSIVE))
    rows, metrics, failures = [], {}, []
    def add(name, model, flags, paths=()):
        png, report, ok = review.capture(name, model, flags, extra_paths=paths)
        rows.append((name, png, report))
        if not ok:
            failures.append(name)
        if report:
            metrics[name] = {"ambient": report["ambient"], "exposure": night.exposure_of(report)[0]}
    for label, model, camera, paths in (
        ("e195_cockpit", REAL_ASSETS["Cockpit"], list(view("cockpit_windshield_inside")["flags"]), E195_SEARCH_PATHS),
        ("da62_cockpit", night.DA62_COCKPIT, night.DA62_CAMERA, ())):
        if not Path(model).exists():
            failures.append(f"missing {label}")
            continue
        for tod, flags in times:
            for enabled in ("off", "auto"):
                add(f"{label}_{tod}_{enabled}", model, camera + flags + ["--time-of-day", "auto", "--ambient", enabled,
                                                                        "--ambient-mode", "auto"], paths)
    model = without_ground_equipment(REAL_ASSETS["Exterior"], OUT / "e195_no_gse.gltf")
    for enabled in ("off", "auto"):
        add(f"e195_exterior_noon_{enabled}", model, review.E195_CAMERA + ["--time-preset", "noon", "--ground", "plane",
                                                                        "--ambient", enabled, "--ambient-mode", "auto"])
    (OUT / "metrics.json").write_text(json.dumps(metrics, indent=2), encoding="utf-8")
    contact_sheet(rows)
    ui_ok = inspector()
    print(f"Review failures {failures}; UI {'PASS' if ui_ok else 'FAIL'}", flush=True)
    return int(bool(failures) or not ui_ok)


if __name__ == "__main__":
    raise SystemExit(main())
