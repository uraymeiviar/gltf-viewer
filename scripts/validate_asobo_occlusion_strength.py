# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A1.5: ASOBO_occlusion_strength.

The hierarchy fixture's material 0 gets a uniform grey occlusion map (R = 128). The MSFS
exporter writes the material occlusion strength only as ASOBO_occlusion_strength, so Glider
uses it in place of occlusionTexture.strength with the core operator (inferred):
  strength 1   -> identical to the map without the extension (core default 1);
  strength 0   -> identical to the file without an occlusion map;
  strength 1.6 -> darker than strength 1 (values above 1 deepen the occlusion);
  --asobo-disable ASOBO_occlusion_strength and --asobo off -> identical to no extension.
"""

import base64
import json
import struct
import sys
import zlib
from pathlib import Path

from validate_asobo_common import EXE, PLAIN_FIXTURE, REPO, check, finish, render, same
from validate_asobo_normal import _read_png

OUT = REPO / "build" / "asobo_occlusion_strength"
EXT = "ASOBO_occlusion_strength"


def grey_png(value, size=4):
    rows = b"".join(b"\x00" + bytes([value, value, value]) * size for _ in range(size))
    def chunk(kind, data):
        return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", size, size, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def write(name, occlusion, strength=None):
    doc = json.loads(Path(PLAIN_FIXTURE).read_text(encoding="utf-8"))
    material = doc["materials"][0]
    if occlusion:
        doc.setdefault("images", []).append(
            {"uri": "data:image/png;base64," + base64.b64encode(grey_png(128)).decode("ascii")})
        doc.setdefault("textures", []).append({"source": len(doc["images"]) - 1})
        material["occlusionTexture"] = {"index": len(doc["textures"]) - 1}
    if strength is not None:
        material.setdefault("extensions", {})[EXT] = {"strength": strength}
        doc["extensionsUsed"] = sorted(set(doc.get("extensionsUsed", [])) | {EXT})
    path = OUT / f"{name}.gltf"
    OUT.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(doc, indent=1), encoding="utf-8")
    return path


def luminance(path):
    image = _read_png(path) if path is not None else None
    if image is None:
        return None
    width, height, channels, pixels = image
    return sum(pixels[i] + pixels[i + 1] + pixels[i + 2] for i in range(0, len(pixels), channels))


def main():
    plain = render(EXE, write("plain", occlusion=False), "plain", folder=OUT)
    mapped = render(EXE, write("mapped", occlusion=True), "mapped", folder=OUT)
    one = render(EXE, write("strength_1", occlusion=True, strength=1.0), "strength_1", folder=OUT)
    zero = render(EXE, write("strength_0", occlusion=True, strength=0.0), "strength_0", folder=OUT)
    strong_model = write("strength_1_6", occlusion=True, strength=1.6)
    strong = render(EXE, strong_model, "strength_1_6", folder=OUT)
    disabled = render(EXE, strong_model, "strength_1_6_disabled", ["--asobo-disable", EXT], folder=OUT)
    master_off = render(EXE, strong_model, "strength_1_6_asobo_off", ["--asobo", "off"], folder=OUT)

    check(all(p is not None for p in (plain, mapped, one, zero, strong, disabled, master_off)), "every capture exists")
    check(not same(plain, mapped), "the grey occlusion map darkens the fixture")
    check(same(one, mapped), "strength 1 equals the map without the extension")
    check(same(zero, plain), "strength 0 equals the file without an occlusion map")
    lum_mapped, lum_strong = luminance(mapped), luminance(strong)
    check(lum_mapped is not None and lum_strong is not None and lum_strong < lum_mapped,
          f"strength 1.6 is darker than strength 1 ({lum_strong} < {lum_mapped})")
    check(same(disabled, mapped), "--asobo-disable ASOBO_occlusion_strength equals no extension")
    check(same(master_off, mapped), "--asobo off equals no extension")
    return finish()


if __name__ == "__main__":
    sys.exit(main())
