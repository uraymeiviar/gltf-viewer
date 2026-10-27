"""Reproduce A4.2.1 after captures/cost and assemble the preserved before review.

The before manifest/captures were made with the initial current build before any
edits. This helper never rebuilds, copies an executable, or writes to the SDK.
Run --capture-after, then --measure-cost with no other renderer process running.
Run without arguments to regenerate crops, measurements and contact sheets.
"""
import argparse
import json
from pathlib import Path
import statistics

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import harness_process as subprocess

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/da62"
EXE = REPO / "build/bin/Release/Glider.exe"
ZERO = ["--set-asobo-param", "Windshield", "ASOBO_material_windshield_v3", "detail1Opacity", "0",
        "--set-asobo-param", "Windshield", "ASOBO_material_windshield_v3", "detail2Opacity", "0"]


def capture(name, arguments, common, extra=(), frame=40):
    command = [str(EXE), *arguments, *common, *extra, "--capture-viewport", str(OUT / (name + ".png")),
               "--capture-frame", str(frame), "--profile-report", str(OUT / (name + ".json")), "--exit-after-profile"]
    print("CAPTURE", name, flush=True)
    p = subprocess.run(command, cwd=REPO, capture_output=True, text=True, timeout=300)
    (OUT / (name + ".log")).write_text(p.stdout + p.stderr, encoding="utf-8")
    if p.returncode or not (OUT / (name + ".png")).is_file() or "GL automation errors: 0" not in p.stdout + p.stderr:
        raise RuntimeError(f"{name}: capture failed ({p.returncode}); see log; no retries")
    return command


def difference(a, b):
    x, y = np.array(Image.open(a).convert("RGB"), dtype=np.int16), np.array(Image.open(b).convert("RGB"), dtype=np.int16)
    delta = x - y
    return {"byteIdentical": a.read_bytes() == b.read_bytes(), "min": int(delta.min()), "max": int(delta.max()),
            "meanAbs": float(np.abs(delta).mean()), "changedPixels": int((np.abs(delta).max(2) > 0).sum()),
            "lighterPixelsAbove2": int((delta.max(2) > 2).sum()), "darkerPixelsBelowMinus1": int((delta.min(2) < -1).sum())}


def assemble():
    metrics = {}
    for name in ("hellocube", "e195_exterior", "e195", "pmdg737"):
        metrics[name + "_before_after"] = difference(OUT / (name + "_after.png"), OUT / (name + "_before.png"))
    for name in ("da62_forward", "da62_side_dark"):
        metrics[name + "_scattering"] = difference(OUT / (name + "_after.png"), OUT / (name + "_after_zero.png"))
    (OUT / "review_metrics.json").write_text(json.dumps(metrics, indent=2))
    try:
        font = ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf", 22)
    except OSError:
        font = ImageFont.load_default()
    w, h, margin = 918, 796, 20
    sheet = Image.new("RGB", (3 * w + 4 * margin, 2 * (h + 520) + 200), (24, 26, 30))
    draw = ImageDraw.Draw(sheet)
    draw.text((margin, 12), "DA62 windshield / A4.2.1 / preserved before, final after, user's MSFS references", font=font, fill="white")

    def cell(path, col, top, label, max_h=796, resize=True):
        x = margin + col * (w + margin)
        draw.text((x, top), label, font=font, fill=(220, 230, 235))
        im = Image.open(path).convert("RGB")
        if resize:
            im.thumbnail((w, max_h), Image.Resampling.LANCZOS)
        sheet.paste(im, (x + (w - im.width) // 2, top + 38))

    rows = [("da62_forward", (340, 385, 570, 550), "msfs2024_da62_canopy_fwd.webp"),
            ("da62_side_dark", (340, 0, 740, 230), "msfs2024_da62_side_window.webp")]
    for index, (name, box, reference) in enumerate(rows):
        top = 60 + index * (h + 560)
        for col, phase in enumerate(("before", "after")):
            path = OUT / (name + "_" + phase + ".png")
            label = ("Forward canopy / " if index == 0 else "Left window, dark background / ") + phase
            cell(path, col, top, label)
            crop = Image.open(path).crop(box)
            crop = crop.resize((crop.width * 2, crop.height * 2), Image.Resampling.NEAREST)
            crop_path = OUT / (name + "_" + phase + "_crop2x.png")
            crop.save(crop_path)
            cell(crop_path, col, top + h + 52, "2x crop / native pixels, no contrast enhancement", 460, resize=False)
        cell(OUT / reference, 2, top, "MSFS 2024 / user's annotated reference")
        if index == 0:
            cell(OUT / "msfs2024_da62_canopy_up.webp", 2, top + h + 52, "MSFS 2024 / upward canopy reference", 460)
        else:
            ref = Image.open(OUT / reference).crop((820, 230, 1120, 410)).resize((600, 360), Image.Resampling.NEAREST)
            ref_path = OUT / "msfs_side_crop2x.png"
            ref.save(ref_path)
            cell(ref_path, 2, top + h + 52, "MSFS 2024 / 2x fingerprint crop", 460, resize=False)
    draw.text((margin, sheet.height - 42), "Different illumination and exterior content; the SDK cockpit-only side view uses a dark clear colour. Pink marks are user annotations.", font=font, fill=(205, 210, 220))
    sheet.save(OUT / "da62_contact_sheet.png")
    other = Image.new("RGB", (2 * w + 3 * margin, 2 * (h + 60)), (24, 26, 30))
    pen = ImageDraw.Draw(other)
    for row, name in enumerate(("e195", "pmdg737")):
        for col, phase in enumerate(("before", "after")):
            x, y = margin + col * (w + margin), row * (h + 60)
            pen.text((x, y + 10), name + " / " + phase, font=font, fill="white")
            im = Image.open(OUT / (name + "_" + phase + ".png")).convert("RGB")
            im.thumbnail((w, h))
            other.paste(im, (x, y + 48))
    other.save(OUT / "other_windshields_contact_sheet.png")
    print(json.dumps(metrics, indent=2))


def measure_cost(tasks, common):
    results, commands = {}, []
    args = dict(tasks)["da62_forward"]
    for repeat in range(3):
        for mode, extra in (("zero", ZERO), ("on", [])):
            name = f"cost_{mode}_{repeat}"
            commands.append(capture(name, args, common, [*extra, "--disable-redraw-skip", "--disable-idle-wait",
                                                        "--fixed-frame-time", "0.05", "--vsync", "off"], 120))
            profile = json.loads((OUT / (name + ".json")).read_text())
            frames = [f for f in profile["frames"] if 20 <= f["frame"] < 119]
            gpu = [sum(p["ms"] for p in f["passes"] if p["ms"] is not None)
                   for f in profile["gpu"]["completed"] if 20 <= f["frame"] < 119]
            transparent = [p["ms"] for f in profile["gpu"]["completed"] if 20 <= f["frame"] < 119
                           for p in f["passes"] if p["name"] == "Transparent" and p["ms"] is not None]
            results[name] = {"cpuMedianMs": statistics.median(f["cpuFrameMs"] for f in frames),
                             "gpuPassSumMedianMs": statistics.median(gpu) if gpu else None,
                             "transparentMedianMs": statistics.median(transparent) if transparent else None,
                             "gpuSamples": len(gpu)}
    (OUT / "cost_commands.json").write_text(json.dumps(commands, indent=2))
    (OUT / "cost_results.json").write_text(json.dumps(results, indent=2))
    print(json.dumps(results, indent=2))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-after", action="store_true")
    parser.add_argument("--measure-cost", action="store_true")
    options = parser.parse_args()
    manifest = json.loads((OUT / "capture_commands.json").read_text())
    tasks, common = manifest["tasks"], manifest["common"]
    if options.capture_after:
        commands = []
        for name, arguments in tasks:
            if name in ("da62_side", "da62_side_y270_p35", "da62_side_y90_p20"):
                continue
            commands.append(capture(name + "_after", arguments, common))
        for name in ("da62_forward", "da62_side_dark"):
            arguments = dict(tasks)[name]
            commands.append(capture(name + "_after_zero", arguments, common, ZERO))
            commands.append(capture(name + "_after_off", arguments, common, ["--asobo-disable", "ASOBO_material_windshield_v3"]))
        (OUT / "after_commands.json").write_text(json.dumps(commands, indent=2))
    if options.measure_cost:
        measure_cost(tasks, common)
    assemble()


if __name__ == "__main__":
    main()
