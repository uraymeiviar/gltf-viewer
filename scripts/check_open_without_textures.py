"""Focused checks for texture-free opening (not the regression suites).

Creates private glTF/data-URI/GLB fixtures, exercises queued reopen and inspects
actual resident payload counters. User settings are never accessed.
"""
import base64
import copy
import json
import os
from pathlib import Path
import struct

from PIL import Image
import check_quality_controls
import harness_process as subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "build/review/without_textures"
EXE = ROOT / "build/bin/Release/Glider.exe"


def fixtures():
    check_quality_controls.OUT = OUT
    model = check_quality_controls.fixture()
    # Use the bundled cube's proven geometry, with a single factor-coloured material.
    source = (ROOT / "resources/sampleGltf/HelloCube.glb").read_bytes()
    json_length = struct.unpack_from("<I", source, 12)[0]
    doc = json.loads(source[20:20 + json_length])
    geometry = source[28 + json_length:][:doc["buffers"][0]["byteLength"]]
    doc["nodes"] = [{"mesh": 0}]
    doc["scenes"] = [{"nodes": [0]}]
    for key in ("cameras", "extensions", "extensionsUsed"):
        doc.pop(key, None)
    doc["images"] = [{"uri": "preset.png"}]
    doc["textures"] = [{"source": 0}]
    doc["materials"][0]["pbrMetallicRoughness"]["baseColorTexture"] = {"index": 0}
    doc["buffers"][0] = {"uri": "data:application/octet-stream;base64," + base64.b64encode(geometry).decode(), "byteLength": len(geometry)}
    doc["materials"][0]["pbrMetallicRoughness"]["baseColorFactor"] = [.2, .8, .25, 1]
    model.write_text(json.dumps(doc))
    image_bytes = (OUT / "assets/texture.FSS/preset.png").read_bytes()
    embedded = copy.deepcopy(doc)
    embedded["images"][0]["uri"] = "data:image/png;base64," + base64.b64encode(image_bytes).decode()
    data_uri = model.with_name("data_uri.gltf")
    data_uri.write_text(json.dumps(embedded))
    missing = copy.deepcopy(doc)
    missing["images"][0]["uri"] = "intentionally_absent.png"
    missing_path = model.with_name("missing.gltf")
    missing_path.write_text(json.dumps(missing))
    glb_doc = copy.deepcopy(doc)
    binary = base64.b64decode(glb_doc["buffers"][0]["uri"].split(",", 1)[1])
    glb_doc["buffers"] = [{"byteLength": len(binary) + len(image_bytes)}]
    glb_doc["images"] = [{"bufferView": len(glb_doc["bufferViews"]), "mimeType": "image/png"}]
    glb_doc["bufferViews"].append({"buffer": 0, "byteOffset": len(binary), "byteLength": len(image_bytes)})
    binary += image_bytes
    binary += b"\0" * (-len(binary) % 4)
    encoded = json.dumps(glb_doc).encode()
    encoded += b" " * (-len(encoded) % 4)
    glb = model.with_suffix(".glb")
    glb.write_bytes(struct.pack("<III", 0x46546C67, 2, 28 + len(encoded) + len(binary)) +
        struct.pack("<II", len(encoded), 0x4E4F534A) + encoded +
        struct.pack("<II", len(binary), 0x004E4942) + binary)
    return model, data_uri, missing_path, glb


def run(model, label, omitted, commands=None, open_file=None):
    args = [str(EXE), str(model), "--isolated", "--quality-preset", "low", "--vsync", "off",
        "--camera-distance", "3", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-target", "0,0,0",
        "--window-size", "1600x900", "--search-path", "../texture*",
        "--profile-report", str(OUT / f"{label}.json")]
    if omitted:
        args += ["--without-textures"]
    if label == "quality_change":
        args += ["--show-search-paths"]
    if commands is None:
        args += ["--capture-viewport", str(OUT / f"{label}.png"), "--capture-frame", "10", "--exit-after-profile"]
    else:
        script = OUT / f"{label}.txt"
        script.write_text(commands + "wait 110\n", encoding="utf-8")
        args += ["--ui-script", str(script), "--ui-report", str(OUT / f"{label}.ui.json")]
        # Profile reports otherwise finalize at frame 8, before scripted reopens.
        args += ["--capture-viewport", str(OUT / f"{label}.png"), "--capture-frame", "80"]
    with (OUT / f"{label}.log").open("wb") as log:
        env = {**os.environ,"GLIDER_TEST_OPEN_FILE": str(open_file)} if open_file else None
        result = subprocess.run(args, cwd=ROOT, env=env, stdout=log, stderr=log, timeout=180)
    text = (OUT / f"{label}.log").read_text(encoding="utf-8", errors="replace")
    assert result.returncode == 0, (label, text)
    assert "using error image" not in text and "Cannot resolve image" not in text, (label, text)
    assert "GL automation errors: 0" in text, (label, text)
    if commands is not None:
        assert not json.loads((OUT / f"{label}.ui.json").read_text())["failed"], label
    return json.loads((OUT / f"{label}.json").read_text())["startup"]


def check_payload(data, omitted):
    assert data["texturesIntentionallyOmitted"] == omitted
    assert data["meshCount"] == 1 and data["materialCount"] == 1
    assert data["textureCount"] == 1 and data["imageCount"] == 1
    for key in ("residentModelTextureBytes", "encodedImageBytes", "decodedImageBytes"):
        assert (data[key] == 0) if omitted else (data[key] > 0), (key, data[key])
    assert not omitted or data["estimatedTextureBytes"] == 0


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    model, data_uri, missing, glb = fixtures()
    check_payload(run(model, "normal", False), False)
    for path, label in ((model, "external"), (data_uri, "data_uri"), (missing, "missing"), (glb, "glb")):
        check_payload(run(path, label, True), True)
        with Image.open(OUT / f"{label}.png") as image:
            r, g, b = image.convert("RGB").getpixel((image.width // 2, image.height // 2))
            assert g > r + 20 and g > b + 20, (label, r, g, b)  # Retained green material factor.
        print("PASS", label, "mesh/material retained; zero image payloads and model texture VRAM", flush=True)
    path = model.as_posix()
    # UI reloads enqueue through the same outer-frame path as File menu commands.
    check_payload(run(model, "queue_without", False, f"wait 8\nreloadwithouttextures {path}\nwait 8\n"), True)
    check_payload(run(model, "queue_normal", True, f"wait 8\nreload {path}\nwait 8\n"), False)
    spaced = model.parent.with_name("model with spaces") / model.name
    spaced.parent.mkdir(parents=True, exist_ok=True)
    spaced.write_text(model.read_text(), encoding="utf-8")
    check_payload(run(model, "queue_normal_spaced", True,
        f"wait 8\nreload {spaced.as_posix()}\nwait 8\n"), False)
    check_payload(run(model, "menu_normal", True,
        "wait 8\nclick 18 12\nwait 4\nclickp file.open 0\nwait 8\n", model), False)
    check_payload(run(model, "menu_without", False,
        "wait 8\nclick 18 12\nwait 4\nclickp file.open_without_textures 0\nwait 8\n", model), True)
    # A profile change must not silently turn intentionally omitted textures back on.
    changed = run(model, "quality_change", True,
        "wait 8\nclickp settings.tab.quality 0\nwait 8\nclickp quality.tab 4\nwait 8\nclickp quality.use 4\nwait 8\n")
    check_payload(changed, True)
    assert changed["qualityProfile"] == "Ultra"
    run(model, "file_menu", True,
        "wait 8\nclick 18 12\nwait 4\n" + f"shot {(OUT / 'file_menu_window.png').as_posix()}\nwait 2\n")
    print("PASS actual File/Open menu loads textures, separate texture-free menu omits them; queued reopen, Low -> Ultra and per-open isolation", flush=True)


if __name__ == "__main__":
    main()
