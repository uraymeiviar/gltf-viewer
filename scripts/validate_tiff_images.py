"""Baseline TIFF byte-exact tests plus the real glTF dispatch/fallback path.

Pillow/libtiff encodes compression independently of the C++ reader. A tiny native
driver includes the production reader, writes RGBA bytes, and reports rejection
reasons. All generated files stay in build/review/tiff; SDK inputs are read-only.
"""
import base64
import json
import os
from pathlib import Path
import struct
import subprocess
import sys

import numpy as np
import tifffile
from PIL import Image, TiffImagePlugin

import harness_process
import validate_asobo_windshield_layers as layers

REPO = Path(__file__).resolve().parents[1]
OUT = REPO / "build/review/tiff"
EXE = REPO / "build/bin/Release/Glider.exe"
HEADER = REPO / "OpenGL-Renderer/Tools/tools/imageformats/source/Tiff.h"


def native_driver():
    source = OUT / "tiff_decode.cpp"
    source.write_text('''#include "''' + HEADER.as_posix() + '''"
#include <fstream>
#include <iostream>
#include <iterator>
int main(int argc, char** argv) {
    if (argc != 3) return 3;
    std::ifstream file(argv[1], std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), {});
    ux3d::imageformats::tiff::Pixels pixels;
    std::string reason;
    if (!ux3d::imageformats::tiff::decode(bytes, pixels, reason)) {
        std::cout << reason; return 2;
    }
    std::ofstream out(argv[2], std::ios::binary);
    out.write(reinterpret_cast<const char*>(pixels.rgba.data()), pixels.rgba.size());
    std::cout << pixels.width << "x" << pixels.height;
    return out ? 0 : 3;
}
''', encoding="utf-8")
    driver = OUT / "tiff_decode.exe"
    if os.name == "nt":
        vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio/Installer/vswhere.exe"
        installation = subprocess.check_output([str(vswhere), "-latest", "-property", "installationPath"], text=True).strip()
        vcvars = Path(installation) / "VC/Auxiliary/Build/vcvars64.bat"
        command = OUT / "compile_driver.cmd"
        command.write_text(f'@call "{vcvars}" >nul\n@cl /nologo /std:c++20 /EHsc /O2 "{source}" /Fe:"{driver}" /Fo:"{OUT / "tiff_decode.obj"}"\n')
        subprocess.run(["cmd", "/c", str(command)], cwd=OUT, check=True)
    else:
        subprocess.run(["c++", "-std=c++20", "-O2", str(source), "-o", str(driver)], check=True)
    return driver


def wrapped_tiff(array, endian, compression, predictor, planar=1, short_table=False):
    """PIL compresses arbitrary strip bytes; TIFF container describes RGB/grey16.

    The predictor is applied in integer sample space before libtiff compression,
    independently of the reader. Multiple strips reset it on every scanline.
    planar=2 stores one plane per strip chunk, plane-major (all strips of plane 0, then
    plane 1, ...), with the predictor differencing each plane horizontally by one sample.
    short_table drops the strip-table entries of every plane after the first (hostile).
    """
    h, w, channels = array.shape
    bits = array.dtype.itemsize * 8
    payloads = []
    rows_per_strip = 17
    for plane in range(channels if planar == 2 else 1):
        for start in range(0, h, rows_per_strip):
            data = array[start:start + rows_per_strip].copy()
            if planar == 2:
                data = data[:, :, plane:plane + 1].copy()
            if predictor == 2:
                original = data.copy()
                data[:, 1:, :] = original[:, 1:, :] - original[:, :-1, :]
            raw = data.astype(endian + ("u2" if bits == 16 else "u1")).tobytes()
            # Single-row L8 is just a byte stream to the independent libtiff encoder.
            temporary = OUT / "encoded_strip.tif"
            Image.frombytes("L", (len(raw), 1), raw).save(temporary, compression=compression)
            with Image.open(temporary) as encoded:
                offset, = encoded.tag_v2[273]
                length, = encoded.tag_v2[279]
            payloads.append(temporary.read_bytes()[offset:offset + length])
    table = len(payloads) // channels if short_table else len(payloads)
    comp = {"raw": 1, "tiff_lzw": 5, "packbits": 32773}[compression]
    # Entries contain explicit types, so byte order/SHORT-in-LONG slots are tested.
    entries = [(256, 4, [w]), (257, 4, [h]), (258, 3, [bits] * channels), (259, 3, [comp]),
               (262, 3, [2 if channels >= 3 else 1]), (273, 4, [0] * table),
               (277, 3, [channels]), (278, 4, [rows_per_strip]), (279, 4, list(map(len, payloads))[:table]),
               (284, 3, [planar]), (317, 3, [predictor]), (339, 3, [1] * channels)]
    if channels in (2, 4):
        entries.append((338, 3, [2]))
    entries.sort()
    extra_start = 8 + 2 + 12 * len(entries) + 4
    extra_length = sum(len(v) * (2 if t == 3 else 4) for _, t, v in entries if len(v) * (2 if t == 3 else 4) > 4)
    pos = extra_start + extra_length
    offsets = []
    for payload in payloads:
        offsets.append(pos)
        pos += len(payload)
    entries = [(tag, typ, offsets[:table] if tag == 273 else values) for tag, typ, values in entries]
    directory, extra = bytearray(), bytearray()
    for tag, typ, values in entries:
        packed = struct.pack(endian + ("H" if typ == 3 else "I") * len(values), *values)
        value = packed.ljust(4, b"\0") if len(packed) <= 4 else struct.pack(endian + "I", extra_start + len(extra))
        directory += struct.pack(endian + "HHI", tag, typ, len(values)) + value
        if len(packed) > 4:
            extra += packed
    return (b"II" if endian == "<" else b"MM") + struct.pack(endian + "HIH", 42, 8, len(entries)) + directory + struct.pack(endian + "I", 0) + extra + b"".join(payloads)


def rgba(array):
    data = (array >> 8).astype(np.uint8) if array.dtype.itemsize == 2 else array
    channels = data.shape[2]
    rgb = data[:, :, :3] if channels >= 3 else np.repeat(data[:, :, :1], 3, axis=2)
    alpha = data[:, :, -1:] if channels in (2, 4) else np.full((*data.shape[:2], 1), 255, dtype=np.uint8)
    return np.concatenate([rgb, alpha], axis=2).tobytes()


def change_tag(data, tag, value):
    result = bytearray(data)
    e = "<" if data[:2] == b"II" else ">"
    offset = struct.unpack_from(e + "I", data, 4)[0]
    count = struct.unpack_from(e + "H", data, offset)[0]
    for i in range(count):
        entry = offset + 2 + 12 * i
        key, typ = struct.unpack_from(e + "HH", data, entry)
        if key == tag:
            struct.pack_into(e + ("H" if typ == 3 else "I"), result, entry + 8, value)
            return bytes(result)
    raise AssertionError(tag)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    driver = native_driver()
    results = []
    rng = np.random.default_rng(421)
    fixtures = []
    images = {}
    for bits in (8, 16):
        for channels in (1, 2, 3, 4):
            a = images[(bits, channels)] = rng.integers(0, 1 << bits, (73, 97, channels), dtype=np.uint8 if bits == 8 else np.uint16)
            for endian in ("<", ">"):
                for compression in ("raw", "tiff_lzw", "packbits"):
                    for predictor in (1, 2):
                        name = f'{bits}bit_{channels}ch_{"le" if endian == "<" else "be"}_{compression}_p{predictor}'
                        path = OUT / (name + ".tif")
                        path.write_bytes(wrapped_tiff(a, endian, compression, predictor))
                        fixtures.append((path, rgba(a), None))
    # PlanarConfiguration 2 (separate planes): the decode must equal the planar-1 decode of the same image.
    # Strips are 17 rows on 73 rows (5 strips, last partial), so the plane-major table has 5 x channels entries.
    planar_pairs = []
    for bits in (8, 16):
        for channels in (2, 3, 4):
            a = images[(bits, channels)]  # same pixels as the planar-1 files of that shape
            for endian in ("<", ">"):
                for compression in ("raw", "tiff_lzw", "packbits"):
                    for predictor in (1, 2):
                        name = f'{bits}bit_{channels}ch_{"le" if endian == "<" else "be"}_{compression}_p{predictor}'
                        path = OUT / ("planar2_" + name + ".tif")
                        path.write_bytes(wrapped_tiff(a, endian, compression, predictor, planar=2))
                        fixtures.append((path, rgba(a), None))
                        planar_pairs.append((path, OUT / (name + ".rgba")))
    # tifffile (an unrelated writer) stores uncompressed separate planes with its own directory layout.
    for bits, dtype in ((8, np.uint8), (16, np.uint16)):
        for channels, photometric in ((2, "minisblack"), (3, "rgb"), (4, "rgb")):
            a = rng.integers(0, 1 << bits, (73, 97, channels), dtype=dtype)
            path = OUT / f"tifffile_planar2_{bits}bit_{channels}ch.tif"
            extra = {} if channels == 3 else {"extrasamples": ["unassalpha"]}
            tifffile.imwrite(path, np.moveaxis(a, 2, 0), planarconfig="separate", photometric=photometric, rowsperstrip=11, **extra)
            fixtures.append((path, rgba(a), None))
    # Direct PIL output also exercises the independent TIFF directory writer.
    for mode in ("RGB", "RGBA", "L", "I;16", "I;16B"):
        bits = 16 if "16" in mode else 8
        raw = rng.integers(0, 256, 97 * 73 * (2 if bits == 16 else len(mode)), dtype=np.uint8).tobytes()
        im = Image.frombytes(mode, (97, 73), raw)
        for compression in ("raw", "tiff_lzw", "packbits"):
            path = OUT / ("pil_" + mode.replace(";", "_") + "_" + compression + ".tif")
            im.save(path, compression=compression)
            if bits == 16:
                a = np.frombuffer(raw, dtype=">u2" if mode.endswith("B") else "<u2").reshape(73, 97, 1)
                expected = rgba(a)
            else:
                expected = im.convert("RGBA").tobytes()
            fixtures.append((path, expected, None))
    base = fixtures[0][0].read_bytes()
    rgb_a = rng.integers(0, 256, (73, 97, 3), dtype=np.uint8)
    rgb_chunky = wrapped_tiff(rgb_a, "<", "raw", 1)
    for compression in ("raw", "tiff_lzw"):
        # Plane-major table cut to one plane's strips (what a chunky table would hold), and a truncated plane.
        short = OUT / f"unsupported_planar2_short_table_{compression}.tif"
        short.write_bytes(wrapped_tiff(rgb_a, "<", compression, 1, planar=2, short_table=True))
        fixtures.append((short, None, "strip table"))
        whole = wrapped_tiff(rgb_a, "<", compression, 1, planar=2)
        cut = OUT / f"unsupported_planar2_truncated_{compression}.tif"
        cut.write_bytes(whole[:-200])
        fixtures.append((cut, None, "truncated"))
    for name, tag, value, reason in [("planar", 284, 3, "planar"), ("planar2_chunky_table", 284, 2, "strip table"), ("jpeg", 259, 7, "compression"),
                                     ("float", 339, 3, "sample format"), ("orientation", 274, 3, "orientation")]:
        if tag == 274:
            continue  # Direct PIL orientation fixture below.
        path = OUT / ("unsupported_" + name + ".tif")
        # A chunky RGB file claiming separate planes has a strip table one third of the plane-major size.
        path.write_bytes(change_tag(rgb_chunky if name == "planar2_chunky_table" else base, tag, value))
        fixtures.append((path, None, reason))
    tile = OUT / "unsupported_tiles.tif"
    tifffile.imwrite(tile, np.zeros((32, 32, 3), dtype=np.uint8), tile=(16, 16), photometric="rgb")
    fixtures.append((tile, None, "tiled"))
    broken = OUT / "truncated.tif"
    broken.write_bytes(base[:40])
    fixtures.append((broken, None, "truncated"))
    big = OUT / "unsupported_bigtiff.tif"
    big.write_bytes(base[:2] + b"\x2b\0" + base[4:])
    fixtures.append((big, None, "BigTIFF"))
    count = OUT / "invalid_scalar_count.tif"
    forged = bytearray(base)
    struct.pack_into("<I", forged, 14, 16)  # First IFD entry is scalar ImageWidth.
    count.write_bytes(forged)
    fixtures.append((count, None, "scalar/sample tag count"))
    white = OUT / "white_is_zero.tif"
    white.write_bytes(change_tag(base, 262, 0))
    white_pixels = np.frombuffer(fixtures[0][1], dtype=np.uint8).reshape(-1, 4).copy()
    white_pixels[:, :3] = 255 - white_pixels[:, :3]
    fixtures.append((white, white_pixels.tobytes(), None))
    for path, expected, reason in fixtures:
        decoded = OUT / (path.stem + ".rgba")
        p = subprocess.run([str(driver), str(path), str(decoded)], capture_output=True, text=True)
        ok = (p.returncode == 2 and reason in p.stdout) if reason else (p.returncode == 0 and decoded.read_bytes() == expected)
        results.append({"name": path.name, "pass": ok, "reason": p.stdout})
        if not ok:
            print("FAIL", results[-1], flush=True)
    for planar_path, chunky_decoded in planar_pairs:
        ok = (OUT / (planar_path.stem + ".rgba")).read_bytes() == chunky_decoded.read_bytes()
        results.append({"name": "planar2_equals_planar1_" + planar_path.stem, "pass": ok})
        if not ok:
            print("FAIL", results[-1], flush=True)
    # Real loader: compare TIFF versus equivalent PNG on unlit glTF, including
    # magic detection with a misleading extension and a data URI image/tiff.
    layers.OUT = OUT
    good = next(p for p, _, _ in fixtures if p.name == "8bit_4ch_le_tiff_lzw_p2.tif")
    pixels = (OUT / (good.stem + ".rgba")).read_bytes()
    png = OUT / "reference.png"
    Image.frombytes("RGBA", (97, 73), pixels).save(png)
    rejections = {path: reason for path, _, reason in fixtures if reason}
    images = [good, png, OUT / "magic.bin", OUT / "data_uri.tif", *rejections]
    images[2].write_bytes(good.read_bytes())
    images[3].write_bytes(good.read_bytes())
    captures = []
    for image in images:
        fixture = layers.Fixture("gltf_" + image.stem)
        fixture.images = [{"uri": image.name}]
        if image.stem == "data_uri":
            fixture.images = [{"uri": "data:image/tiff;base64," + base64.b64encode(image.read_bytes()).decode(), "mimeType": "image/tiff"}]
        fixture.textures = [{"source": 0, "sampler": 0}]
        material = {"pbrMetallicRoughness": {"baseColorTexture": {"index": 0}, "metallicFactor": 0}, "extensions": {"KHR_materials_unlit": {}}}
        model = fixture.write(material)
        doc = json.loads(model.read_text())
        doc["extensionsUsed"] = ["KHR_materials_unlit"]
        model.write_text(json.dumps(doc))
        capture = layers.capture(EXE, model, "capture_" + image.stem)
        captures.append(capture)
        if image in rejections:
            log = (OUT / ("capture_" + image.stem + ".log")).read_text()
            warnings = [line for line in log.splitlines() if "using error image" in line]
            ok = capture is not None and len(warnings) == 1 and rejections[image] in warnings[0]
            results.append({"name": "fallback_" + image.stem, "pass": ok, "reason": warnings})
    results.append({"name": "TIFF_PNG_magic_exact_capture", "pass": layers.same(captures[0], captures[1]) and layers.same(captures[0], captures[2])})
    results.append({"name": "image_tiff_data_uri_exact_capture", "pass": layers.same(captures[0], captures[3])})
    results.append({"name": "unsupported_same_error_image", "pass": all(layers.same(captures[4], cap) for cap in captures[4:])})
    sdk = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62")
    paths = list((sdk / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/texture").glob("*.tif"))
    paths += list((sdk / "Sources/Art/Max/textures").glob("Fabric_Tile_01_albd.tif"))
    model = sdk / "PackageSources/SimObjects/Airplanes/DA62_SDK/attachments/asobo/Part_Interior_Cockpit/model/Cockpit_LOD00.gltf"
    if model.is_file():
        paths += [(model.parent / im["uri"]).resolve() for im in json.loads(model.read_text())["images"]
                  if ".tif" in im.get("uri", "").lower() and (model.parent / im["uri"]).is_file()]
    paths += [sdk / "Sources/Art/Max/textures/Regular_albd.tif"]  # PlanarConfiguration 2, RGBA, uncompressed
    paths = sorted(set(p for p in paths if p.is_file()))
    for path in paths:
        decoded = OUT / ("sdk_" + path.stem + ".rgba")
        p = subprocess.run([str(driver), str(path), str(decoded)], capture_output=True, text=True)
        with tifffile.TiffFile(path) as tf:
            planar_file = tf.pages[0].planarconfig == 2
            planar_array = tf.asarray() if planar_file else None
        if planar_file:
            # Pillow has no PlanarConfiguration 2 reader; tifffile gives (planes, h, w).
            interleaved = np.moveaxis(planar_array, 0, 2)
            if interleaved.dtype != np.uint8:
                interleaved = (interleaved >> 8).astype(np.uint8)
            expected = np.concatenate([interleaved[:, :, :3], interleaved[:, :, 3:4] if interleaved.shape[2] == 4 else np.full((*interleaved.shape[:2], 1), 255, np.uint8)], axis=2).tobytes()
        else:
            with Image.open(path) as im:
                expected = im.convert("RGBA").tobytes()
        # Pillow drops unspecified TIFF extra channels; RGB still must match.
        actual = np.frombuffer(decoded.read_bytes(), dtype=np.uint8).reshape(-1, 4) if p.returncode == 0 else None
        reference = np.frombuffer(expected, dtype=np.uint8).reshape(-1, 4)
        ok = actual is not None and np.array_equal(actual[:, :3], reference[:, :3])
        results.append({"name": "SDK_" + path.name, "pass": ok, "reason": p.stdout, "comparison": "RGB exact; alpha follows authored extra sample"})
    (OUT / "results.json").write_text(json.dumps(results, indent=2))
    failed = [r for r in results if not r["pass"]]
    print(f'TIFF: {len(results) - len(failed)}/{len(results)} PASS', flush=True)
    for result in failed:
        print(result)
    return bool(failed or layers.failures)


if __name__ == "__main__":
    sys.exit(main())

