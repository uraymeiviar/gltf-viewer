# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""KHR_materials_diffuse_transmission, KHR_materials_diffuse_roughness and the MSFT vendor extensions MSFT_lod,
MSFT_packing_occlusionRoughnessMetallic and MSFT_packing_normalRoughnessMetallic.

Fixtures are written at test time into build/diffuse_extensions/fixtures (small .gltf files with data-URI buffers and
images, never committed); nothing needs the network. The Khronos samples DiffuseTransmissionTest.glb,
DiffuseTransmissionPlant.glb, DiffuseTransmissionTeacup.glb and MandarinOrange/MandarinOrange.gltf are rendered when
present in build/khronos-samples (INFO / skip otherwise). Measurements read the linear HDR scene target through the
control pipe (probe / probe_region: radiance before tone mapping), with a black environment, the sun and the ambient
light off unless stated, and no anti-aliasing.

Diffuse transmission ("dt_*"): a 2x2 quad facing the camera (normal +Z), a directional KHR_lights_punctual light
straight behind it (shining towards the camera) or in front of it. The BTDF is Lambert of the transmission colour:
  * factor 0 (and the extension switched off) = no extension, byte for byte (back light, front light, default HDR
    environment, the sun);
  * factor 1, back light: the front receives E x colour / pi; with specularFactor 0 (no Fresnel weight) it equals the
    front-lit radiance of the plain material whose base colour is the transmission colour (0.5 %); the channels follow
    the colour factor; the core material stays black;
  * front light: the diffuse reflection scales with (1 - factor): factor 0.5 = the mean of factor 0 and 1 (1 %);
  * textures: diffuseTransmissionTexture (A: left half 1, right half 0) and diffuseTransmissionColorTexture (RGB, sRGB:
    left half red, right half the colour factor's white) modulate the halves;
  * KHR_materials_volume: attenuationColor 0.5 at attenuationDistance = thickness halves the transmitted light (the
    sample renderer's thickness x mean model scale);
  * KHR_materials_transmission 1 overrides it (within one 8-bit level of the transmission-only twin);
  * IBL only (an environment bright behind the quad): the front gets irradiance(-N) x colour (channel ratios), the
    core material does not;
  * the sun behind the quad (shadows on): transmitted and not shadowed, as the sample renderer (= shadows off, 1 %);
  * KHR_animation_pointer STEP 0 -> 1 at 1 s: 0.5 s / 1.5 s = the factor 0 / 1 files; pipe set_material_param 0 -> 1
    = the file with 1; get_material_param reads it back; the per-material display switch (UI script) off = none.

Diffuse roughness ("dr_*"): a unit sphere (specularFactor 0, base 0.8), a directional light from the camera side.
EON (Portsmouth et al. 2024, the OpenPBR model the draft recommends, with its fast albedo approximation):
  * roughness 0 / switched off = no extension, byte for byte;
  * the radiance along the horizontal centre row follows E mu_i f_EON(rho, r, mu_i, mu_o, l.v) (per-pixel normal and
    view from the probed world position; scale calibrated on roughness 0 = Lambert) within 2 % for r = 0.5 and 1;
    the falloff flattens: limb / centre grows with r;
  * energy: the disc mean of r = 1 over Lambert equals the formula's value (stated, 2 %); the formula's directional
    albedo for rho = 1 stays within [0.97, 1.0] (energy preserving, Python check of the transcription);
  * texture R (left half 1, right half 0) = the r = 1 / r = 0 renders on those halves; animation pointer / pipe edit.

MSFT_lod, MSFT_packing_*: a file with MSFT_lod (lower LOD nodes out of the scene, screen-coverage extras) renders
byte-identical to its twin without it, also required; required + switched off is refused; the packed ORM texture of a
material without core textures = the twin with the same texture in the core slots; with core textures it is ignored;
MSFT_packing_normalRoughnessMetallic = its twin without it; no validator notice for any fixture.

Run: python scripts/validate_diffuse_extensions.py [--baseline-exe build/baselines/<sha>.exe] [--only dt|dr|msft]
"""

import argparse
import base64
import io
import json
import math
import struct
import sys
import time

import numpy as np
from PIL import Image

from glider_harness import (EXE, REPO, baseline_exe, capture, capture_run, gl_errors_zero, read_json, run, same,
                            start_pipe_session)

OUT = REPO / "build" / "diffuse_extensions"
FIXTURES = OUT / "fixtures"
SAMPLES = REPO / "build" / "khronos-samples"
DT = "KHR_materials_diffuse_transmission"
DR = "KHR_materials_diffuse_roughness"
FLAGS = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
VIEW = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral", "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0",
        "--camera-distance", "4", "--sun", "off", "--ambient", "off"]
LIGHT_I = math.pi   # directional light intensity (lux): a white Lambert surface facing it reflects 1
TINT = [1.0, 0.5, 0.25]

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label)
    if not condition:
        failures.append(label)
    return condition


# --------------------------------------------------------------------------------------------- glTF writing

class Doc:
    def __init__(self):
        self.data = bytearray()
        self.views, self.accessors = [], []

    def accessor(self, payload, ctype, count, kind, minmax=None, target=None):
        while len(self.data) % 4:
            self.data.append(0)
        view = {"buffer": 0, "byteOffset": len(self.data), "byteLength": len(payload)}
        if target:
            view["target"] = target
        self.data += payload
        self.views.append(view)
        entry = {"bufferView": len(self.views) - 1, "componentType": ctype, "count": count, "type": kind}
        if minmax:
            entry["min"], entry["max"] = minmax
        self.accessors.append(entry)
        return len(self.accessors) - 1

    def finish(self, document):
        document["buffers"] = [{"byteLength": len(self.data), "uri": "data:application/octet-stream;base64,"
                                + base64.b64encode(bytes(self.data)).decode("ascii")}]
        document["bufferViews"], document["accessors"] = self.views, self.accessors
        return document


def mesh_primitive(doc, positions, normals, uvs, indices, material):
    pos = np.array(positions, dtype=np.float32)
    attributes = {
        "POSITION": doc.accessor(pos.tobytes(), 5126, len(pos), "VEC3", (pos.min(0).tolist(), pos.max(0).tolist()), 34962),
        "NORMAL": doc.accessor(np.array(normals, dtype=np.float32).tobytes(), 5126, len(pos), "VEC3", target=34962),
        "TEXCOORD_0": doc.accessor(np.array(uvs, dtype=np.float32).tobytes(), 5126, len(pos), "VEC2", target=34962),
    }
    return {"primitives": [{"attributes": attributes, "material": material,
                            "indices": doc.accessor(np.array(indices, dtype=np.uint32).tobytes(), 5125, len(indices),
                                                    "SCALAR", target=34963)}]}


def quad_mesh(doc, material):
    return mesh_primitive(doc, [(-1, -1, 0), (1, -1, 0), (1, 1, 0), (-1, 1, 0)], [(0, 0, 1)] * 4,
                          [(0, 1), (1, 1), (1, 0), (0, 0)], [0, 1, 2, 0, 2, 3], material)


def sphere_mesh(doc, material, segments=192, rings=96):
    pos, uv, idx = [], [], []
    for r in range(rings + 1):
        theta = math.pi * r / rings
        for s in range(segments + 1):
            phi = 2.0 * math.pi * s / segments
            pos.append((math.sin(theta) * math.sin(phi), math.cos(theta), math.sin(theta) * math.cos(phi)))
            uv.append((s / segments, r / rings))
    for r in range(rings):
        for s in range(segments):
            a = r * (segments + 1) + s
            b = a + segments + 1
            idx += [a, b, a + 1, a + 1, b, b + 1]
    return mesh_primitive(doc, pos, pos, uv, idx, material)


def png_uri(pixels):
    """pixels: H x W x 4 uint8 array."""
    data = io.BytesIO()
    Image.fromarray(np.asarray(pixels, dtype=np.uint8)).save(data, format="PNG")
    return "data:image/png;base64," + base64.b64encode(data.getvalue()).decode("ascii")


def halves(left, right, size=64):
    """A size x size RGBA image: the left half (u < 0.5) one colour, the right half the other."""
    image = np.zeros((size, size, 4), dtype=np.uint8)
    image[:, : size // 2] = left
    image[:, size // 2:] = right
    return image


LIGHTS = {  # directional lights shine along the node's -Z
    "back": {"translation": [0, 0, -5], "rotation": [0, 1, 0, 0]},
    "front": {"translation": [0, 0, 5]},
}


def write_model(name, material, shape="quad", light="back", images=(), animation=None, used_extra=()):
    """One mesh node with `material`; `light` a LIGHTS key or None; `images`: data URIs (textures i = image i, nearest
    filtering); `animation`: (pointer, [v0, v1, v1]) STEP keys at 0, 1, 2 s."""
    doc = Doc()
    mesh = quad_mesh(doc, 0) if shape == "quad" else sphere_mesh(doc, 0)
    nodes = [{"name": "Shape", "mesh": 0}]
    document = {"asset": {"version": "2.0", "generator": "validate_diffuse_extensions.py"}, "scene": 0,
                "materials": [material], "meshes": [mesh]}
    used = set(material.get("extensions", {}).keys()) | set(used_extra)
    if images:
        document["images"] = [{"uri": uri} for uri in images]
        document["samplers"] = [{"magFilter": 9728, "minFilter": 9728}]
        document["textures"] = [{"source": i, "sampler": 0} for i in range(len(images))]
    if light is not None:
        nodes.append({"name": "Light", **LIGHTS[light], "extensions": {"KHR_lights_punctual": {"light": 0}}})
        document["extensions"] = {"KHR_lights_punctual": {"lights": [{"type": "directional", "intensity": LIGHT_I,
                                                                       "color": [1, 1, 1]}]}}
        used.add("KHR_lights_punctual")
    if animation is not None:
        pointer, values = animation
        times = doc.accessor(np.array([0.0, 1.0, 2.0], dtype=np.float32).tobytes(), 5126, 3, "SCALAR",
                             ([0.0], [2.0]))
        output = doc.accessor(np.array(values, dtype=np.float32).tobytes(), 5126, 3, "SCALAR")
        document["animations"] = [{"name": "Step", "samplers": [{"input": times, "output": output,
                                                                  "interpolation": "STEP"}],
                                   "channels": [{"sampler": 0, "target": {"path": "pointer", "extensions": {
                                       "KHR_animation_pointer": {"pointer": pointer}}}}]}]
        used.add("KHR_animation_pointer")
    document["nodes"] = nodes
    document["scenes"] = [{"nodes": list(range(len(nodes)))}]
    if used:
        document["extensionsUsed"] = sorted(used)
    path = FIXTURES / f"{name}.gltf"
    path.write_text(json.dumps(doc.finish(document)), encoding="utf-8")
    return path


def material(base=(0.8, 0.8, 0.8), roughness=1.0, metallic=0.0, ext=None, specular_off=False, name="surface",
             double_sided=False, **core):
    m = {"name": name, "pbrMetallicRoughness": {"baseColorFactor": [*base, 1.0], "metallicFactor": metallic,
                                                 "roughnessFactor": roughness}}
    m["pbrMetallicRoughness"].update(core.pop("pbr", {}))
    m.update(core)
    if double_sided:
        m["doubleSided"] = True
    extensions = dict(ext or {})
    if specular_off:
        extensions["KHR_materials_specular"] = {"specularFactor": 0.0}
    if extensions:
        m["extensions"] = extensions
    return m


def write_hdr(path, width, height, pixel):
    """Uncompressed Radiance RGBE; pixel(x, y) -> (r, g, b) linear."""
    def rgbe(r, g, b):
        v = max(r, g, b)
        if v < 1e-32:
            return bytes((0, 0, 0, 0))
        m, e = math.frexp(v)
        scale = m * 256.0 / v
        return bytes((int(r * scale), int(g * scale), int(b * scale), e + 128))
    header = f"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y {height} +X {width}\n".encode("ascii")
    body = bytearray()
    for y in range(height):
        for x in range(width):
            body += rgbe(*pixel(x, y))
    path.write_bytes(header + bytes(body))
    return path


# --------------------------------------------------------------------------------------------- Glider runs

def render(exe, model, name, flags=(), session=False):
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    result = capture_run([exe, model, *FLAGS, *VIEW, *flags, "--capture-viewport", png, "--capture-frame", "24",
                          "--profile-report", report, "--exit-after-profile"], session=session)
    return (png if png.exists() else None), result.stdout + result.stderr


class Session:
    def __init__(self, model, flags=(), exe=EXE):
        self.process, self.pipe = start_pipe_session([exe, model, *FLAGS, *VIEW, *flags, "--control-pipe"])
        deadline = time.time() + 120
        while time.time() < deadline:
            opened = (self.call("get_state").get("gltfExtensions") or {}).get("openFile") or {}
            if opened.get("loaded") and not opened.get("loadPending"):
                break
            time.sleep(0.2)
        self.call("capture", path=str(OUT / "session_settle.png"))

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def region(self, x0, y0, x1, y1):
        return self.call("probe_region", x0=int(x0), y0=int(y0), x1=int(x1), y1=int(y1))["rgb"]["mean"]

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:
                self.process.kill()
        return errors


def with_session(model, flags, body, exe=EXE):
    session = Session(model, flags, exe)
    try:
        value = body(session)
        errors = session.close()
    except Exception:
        session.process.kill()
        raise
    return value, errors


def centre_rect(session, half=20):
    """The capture image's central square (the quad / sphere centre with the fixed camera)."""
    shot = session.call("capture", path=str(OUT / "session_size.png"))
    w, h = Image.open(shot["path"]).size
    return w // 2 - half, h // 2 - half, w // 2 + half, h // 2 + half, w, h


def validate(model, name):
    report = OUT / f"{name}.validation.json"
    report.unlink(missing_ok=True)
    run([EXE, "--isolated", "--validate-file", model, "--validation-report", report])
    data = read_json(report)
    return None if data is None else data.get("issues", [])


def notices(issues):
    return [i.get("code") for i in issues or [] if str(i.get("code", "")).startswith("EXTENSION_")
            or i.get("severity") == "Error"]


def rel(a, b):
    return abs(a - b) / max(abs(b), 1e-9)


def ratio(a, b):
    """a / b, NaN (every comparison false: the check fails) when b is 0 (a build without the feature renders black)."""
    return a / b if b != 0 else float("nan")


def load(png):
    return np.asarray(Image.open(png).convert("RGB")).astype(np.int32)


# --------------------------------------------------------------------------------------------- EON reference (Python)

C1 = 0.5 - 2.0 / (3.0 * math.pi)
C2 = 2.0 / 3.0 - 28.0 / (15.0 * math.pi)


def e_fon_approx(mu, r):
    mc = 1.0 - mu
    g = mc * (0.0571085289 + mc * (0.491881867 + mc * (-0.332181442 + mc * 0.0714429953)))
    return (1.0 + r * g) / (1.0 + C1 * r)


def f_eon(rho, r, mu_i, mu_o, ldotv):
    s = ldotv - mu_i * mu_o
    st = s / max(mu_i, mu_o, 1e-7) if s > 0.0 else s
    af = 1.0 / (1.0 + C1 * r)
    f_ss = rho / math.pi * af * (1.0 + r * st)
    efo, efi = e_fon_approx(mu_o, r), e_fon_approx(mu_i, r)
    avg = af * (1.0 + C2 * r)
    rho_ms = rho * rho * avg / (1.0 - rho * (1.0 - avg))
    f_ms = rho_ms / math.pi * max(1e-7, 1.0 - efo) * max(1e-7, 1.0 - efi) / max(1e-7, 1.0 - avg)
    return f_ss + f_ms


def eon_albedo(r, mu_o, n=96):
    """Directional albedo of EON for rho = 1 (midpoint quadrature over the hemisphere)."""
    total = 0.0
    sin_o = math.sqrt(max(0.0, 1.0 - mu_o * mu_o))
    for i in range(n):
        mu_i = (i + 0.5) / n
        sin_i = math.sqrt(1.0 - mu_i * mu_i)
        for j in range(2 * n):
            phi = math.pi * (j + 0.5) / n
            ldotv = mu_i * mu_o + sin_i * sin_o * math.cos(phi)
            total += f_eon(1.0, r, mu_i, mu_o, ldotv) * mu_i * (1.0 / n) * (math.pi / n)
    return total


def disc_ratio(r, n=200):
    """Mean radiance of a sphere lit and seen along the same axis (orthographic): EON r over Lambert, rho 0.8."""
    eon = lam = 0.0
    for i in range(n):
        for j in range(n):
            x, y = (i + 0.5) / n * 2 - 1, (j + 0.5) / n * 2 - 1
            if x * x + y * y >= 1.0:
                continue
            mu = math.sqrt(1.0 - x * x - y * y)
            eon += mu * f_eon(0.8, r, mu, mu, 1.0)
            lam += mu * 0.8 / math.pi
    return eon / lam


# --------------------------------------------------------------------------------------------- diffuse transmission

def dt_ext(factor=None, color=None, texture=None, color_texture=None):
    e = {}
    if factor is not None:
        e["diffuseTransmissionFactor"] = factor
    if color is not None:
        e["diffuseTransmissionColorFactor"] = color
    if texture is not None:
        e["diffuseTransmissionTexture"] = {"index": texture}
    if color_texture is not None:
        e["diffuseTransmissionColorTexture"] = {"index": color_texture}
    return {DT: e}


def dt_fixtures():
    f = {}
    base = (0.2, 0.4, 0.8)
    for light in ("back", "front"):
        f[f"none_{light}"] = write_model(f"dt_none_{light}", material(base), light=light)
        f[f"zero_{light}"] = write_model(f"dt_zero_{light}", material(base, ext=dt_ext(0.0, TINT)), light=light)
        f[f"one_{light}"] = write_model(f"dt_one_{light}", material(base, ext=dt_ext(1.0, TINT)), light=light)
        f[f"half_{light}"] = write_model(f"dt_half_{light}", material(base, ext=dt_ext(0.5, TINT)), light=light)
    # No Fresnel weight (specularFactor 0): the BTDF of the tint seen through the quad = the Lambert BRDF of the tint.
    f["one_back_nospec"] = write_model("dt_one_back_nospec", material(base, ext=dt_ext(1.0, TINT), specular_off=True))
    f["tint_front_nospec"] = write_model("dt_tint_front_nospec", material(tuple(TINT), specular_off=True), light="front")
    f["none_ibl"] = write_model("dt_none_ibl", material((0.0, 0.0, 0.0)), light=None)
    f["one_ibl"] = write_model("dt_one_ibl", material((0.0, 0.0, 0.0), ext=dt_ext(1.0, TINT)), light=None)
    f["zero_ibl"] = write_model("dt_zero_ibl", material(base, ext=dt_ext(0.0, TINT)), light=None)
    f["none_ibl_base"] = write_model("dt_none_ibl_base", material(base), light=None)
    f["one_sun"] = write_model("dt_one_sun", material(base, ext=dt_ext(1.0, TINT), double_sided=True), light=None)
    f["zero_sun"] = write_model("dt_zero_sun", material(base, ext=dt_ext(0.0, TINT), double_sided=True), light=None)
    f["none_sun"] = write_model("dt_none_sun", material(base, double_sided=True), light=None)
    # Textures: factor texture A = 1 on the left half, 0 on the right; colour texture red / white halves (sRGB).
    f["tex"] = write_model("dt_tex", material(base, ext=dt_ext(1.0, None, texture=0)),
                           images=[png_uri(halves((0, 0, 0, 255), (0, 0, 0, 0)))])
    f["ctex"] = write_model("dt_ctex", material(base, ext=dt_ext(1.0, None, color_texture=0)),
                            images=[png_uri(halves((255, 0, 0, 255), (255, 255, 255, 255)))])
    f["one_white"] = write_model("dt_one_white", material(base, ext=dt_ext(1.0)))
    f["one_red"] = write_model("dt_one_red", material(base, ext=dt_ext(1.0, [1.0, 0.0, 0.0])))
    # KHR_materials_volume: attenuation 0.5 over one attenuation distance = the thickness (model scale 1).
    vol = dt_ext(1.0, TINT)
    vol["KHR_materials_volume"] = {"thicknessFactor": 1.0, "attenuationDistance": 1.0,
                                   "attenuationColor": [0.5, 0.5, 0.5]}
    f["volume"] = write_model("dt_volume", material(base, ext=vol))
    # KHR_materials_transmission overrides the diffuse transmission.
    tr = dt_ext(1.0, TINT)
    tr["KHR_materials_transmission"] = {"transmissionFactor": 1.0}
    f["with_transmission"] = write_model("dt_with_transmission", material(base, roughness=0.5, ext=tr), light="back")
    f["transmission_only"] = write_model("dt_transmission_only", material(
        base, roughness=0.5, ext={"KHR_materials_transmission": {"transmissionFactor": 1.0}}), light="back")
    f["anim"] = write_model("dt_anim", material(base, ext=dt_ext(0.0, TINT)),
                            animation=(f"/materials/0/extensions/{DT}/diffuseTransmissionFactor", [0.0, 1.0, 1.0]))
    return f


def dt_checks(files, baseline, black, behind):
    print("[KHR_materials_diffuse_transmission: byte identity]")
    for light in ("back", "front"):
        none, log = render(EXE, files[f"none_{light}"], f"dt_none_{light}", black)
        zero, log2 = render(EXE, files[f"zero_{light}"], f"dt_zero_{light}", black)
        check(same(none, zero) and gl_errors_zero(log) and gl_errors_zero(log2),
              f"factor 0, light {light}: byte-identical to the material without the extension")
        off, _ = render(EXE, files[f"one_{light}"], f"dt_one_{light}_off", black + ["--disable-extension", DT])
        check(same(off, none), f"factor 1 with the extension switched off, light {light}: byte-identical to none")
    none, _ = render(EXE, files["none_ibl_base"], "dt_none_default_env")
    zero, _ = render(EXE, files["zero_ibl"], "dt_zero_default_env")
    check(same(none, zero), "factor 0 under the default HDR environment (IBL, ambient): byte-identical to none")
    sun = ["--sun", "on", "--sun-mode", "manual", "--sun-dir", "180,30", "--sun-intensity", "3"]
    none, _ = render(EXE, files["none_sun"], "dt_none_sun", black + sun)
    zero, _ = render(EXE, files["zero_sun"], "dt_zero_sun", black + sun)
    check(same(none, zero), "factor 0 with the sun (shadows on): byte-identical to none")

    print("[KHR_materials_diffuse_transmission: back light]")
    values = {}
    for key in ("none_back", "one_back", "half_back", "one_back_nospec", "tint_front_nospec", "none_front",
                "one_front", "half_front", "volume", "one_white", "one_red"):
        values[key], errors = with_session(files[key], black, lambda s: s.region(*centre_rect(s)[:4]))
        if errors:
            check(False, f"{key}: GL errors {errors}")
    print(f"  INFO centre linear radiance: " + ", ".join(f"{k} {[round(c, 4) for c in v]}" for k, v in values.items()))
    check(max(values["none_back"]) < 1e-4, f"core material lit from behind: black front ({values['none_back']})")
    one = values["one_back"]
    check(min(one) > 0.05, f"factor 1 lit from behind: the front is lit ({[round(c, 4) for c in one]})")
    ratios = [ratio(one[c], one[0]) for c in range(3)]
    check(all(rel(ratios[c], TINT[c]) < 0.01 for c in range(3)),
          f"channel ratios follow diffuseTransmissionColorFactor {TINT}: {[round(r, 4) for r in ratios]}")
    a, b = values["one_back_nospec"], values["tint_front_nospec"]
    check(all(rel(a[c], b[c]) < 0.005 for c in range(3)),
          f"BTDF = Lambert of the transmission colour: back-lit {[round(c, 4) for c in a]} = front-lit base colour = "
          f"tint {[round(c, 4) for c in b]} (specularFactor 0, 0.5 %)")
    check(all(rel(a[c], TINT[c]) < 0.01 for c in range(3)),
          f"absolute: E x colour / pi with E = pi -> {TINT} ({[round(c, 4) for c in a]}, 1 %)")
    h = values["half_back"]
    check(all(rel(h[c], 0.5 * one[c]) < 0.01 for c in range(3)), f"factor 0.5 lit from behind = half of factor 1 ({h})")
    nf, of, hf = values["none_front"], values["one_front"], values["half_front"]
    check(all(abs(hf[c] - 0.5 * (nf[c] + of[c])) < 0.01 * nf[c] for c in range(3)) and all(of[c] < 0.2 * nf[c] for c in range(3)),
          f"front light: diffuse reflection x (1 - factor): factor 0.5 {hf} = mean of factor 0 {nf} and 1 {of} "
          f"(specular only)")
    v = values["volume"]
    check(all(rel(v[c], 0.5 * one[c]) < 0.02 for c in range(3)),
          f"KHR_materials_volume attenuation 0.5 over thickness 1: {[round(c, 4) for c in v]} = half of "
          f"{[round(c, 4) for c in one]} (2 %)")
    w, r = values["one_white"], values["one_red"]
    check(rel(w[1], w[0]) < 0.01 and r[1] < 1e-4 and rel(r[0], w[0]) < 0.01, f"white / red colour factor: {w} / {r}")

    print("[KHR_materials_diffuse_transmission: textures]")

    def halves_probe(s):
        x0, y0, x1, y1, width, height = centre_rect(s, 30)
        cx, cy = width // 2, height // 2
        return s.region(cx - 70, cy - 20, cx - 30, cy + 20), s.region(cx + 30, cy - 20, cx + 70, cy + 20)
    (left, right), _ = with_session(files["tex"], black, halves_probe)
    (wl, wr), _ = with_session(files["one_white"], black, halves_probe)
    check(all(rel(left[c], wl[c]) < 0.01 for c in range(3)) and max(right) < 1e-4,
          f"diffuseTransmissionTexture A: left half (1) = factor 1 {[round(c, 4) for c in left]} vs "
          f"{[round(c, 4) for c in wl]}; right half (0) black {right}")
    (cl, cr), _ = with_session(files["ctex"], black, halves_probe)
    check(rel(cl[0], wl[0]) < 0.01 and cl[1] < 1e-4 and all(rel(cr[c], wr[c]) < 0.01 for c in range(3)),
          f"diffuseTransmissionColorTexture RGB: red half {[round(c, 4) for c in cl]}, white half "
          f"{[round(c, 4) for c in cr]} (white colour factor {[round(c, 4) for c in wr]})")

    print("[KHR_materials_diffuse_transmission: IBL, sun, transmission, animation, pipe]")
    ibl = {}
    for key in ("none_ibl", "one_ibl"):
        ibl[key], _ = with_session(files[key], behind, lambda s: s.region(*centre_rect(s)[:4]))
    delta = [ibl["one_ibl"][c] - ibl["none_ibl"][c] for c in range(3)]
    print(f"  INFO IBL (bright behind): core front {ibl['none_ibl']}, factor 1 {ibl['one_ibl']}")
    check(delta[0] > 0.05 and all(rel(ratio(delta[c], delta[0]), TINT[c]) < 0.05 for c in range(3)),
          f"IBL only, environment behind: the front receives irradiance(-N) x colour ({[round(c, 4) for c in delta]}, "
          f"ratios {[round(ratio(delta[c], delta[0]), 3) for c in range(3)]})")
    sun_values = {}
    for shadows in ("on", "off"):
        sun_values[shadows], _ = with_session(files["one_sun"], black + sun + ["--shadows", shadows],
                                              lambda s: s.region(*centre_rect(s)[:4]))
    sun_none, _ = with_session(files["none_sun"], black + sun, lambda s: s.region(*centre_rect(s)[:4]))
    print(f"  INFO sun behind (az 180, el 30): factor 1 shadows on {sun_values['on']}, off {sun_values['off']}, "
          f"core {sun_none}")
    check(min(sun_values["on"]) > 0.05 and max(sun_none) < 1e-3
          and all(rel(sun_values["on"][c], sun_values["off"][c]) < 0.01 for c in range(3)),
          "the sun behind the quad: transmitted to the front, not shadowed (sample renderer rule: shadows on = off)")
    a, _ = render(EXE, files["with_transmission"], "dt_with_transmission", black)
    b, _ = render(EXE, files["transmission_only"], "dt_transmission_only", black)
    diff = int(np.abs(load(a) - load(b)).max()) if a and b else 999
    check(diff <= 1, f"KHR_materials_transmission 1 overrides the diffuse transmission ({diff} levels from the "
                     f"transmission-only twin)")
    zero_png = OUT / "dt_zero_back.png"
    one_png, _ = render(EXE, files["one_back"], "dt_one_back", black)
    a, _ = render(EXE, files["anim"], "dt_anim_0.5", black + ["--anim-time", "0.5"])
    b, _ = render(EXE, files["anim"], "dt_anim_1.5", black + ["--anim-time", "1.5"])
    check(same(a, zero_png) and same(b, one_png),
          "KHR_animation_pointer diffuseTransmissionFactor STEP 0 -> 1: 0.5 s = factor 0 file, 1.5 s = factor 1 file")

    def edit(s):
        reply = s.call("set_material_param", material="surface", extension=DT, param="diffuseTransmissionFactor", value=1.0)
        return reply, s.call("capture", path=str(OUT / "dt_pipe_edit.png"))["path"]
    (reply, edited), errors = with_session(files["zero_back"], black, edit)
    shot, _ = with_session(files["one_back"], black, lambda s: s.call("capture", path=str(OUT / "dt_pipe_one.png"))["path"])
    check(reply.get("value") == 1.0 and same(edited, shot) and errors == 0,
          f"set_material_param diffuseTransmissionFactor 0 -> 1 = the file with 1 (pipe captures), read back ({reply})")
    display_switch_check(files["one_back"], files["none_back"], DT, black)
    issues = validate(files["one_back"], "dt_one_back")
    check(issues is not None and not notices(issues), f"dt: no validation error, no extension notice ({notices(issues)})")
    if baseline is not None:
        base_none, _ = render(baseline, files["none_back"], "base_dt_none_back", black)
        base_front, _ = render(baseline, files["none_front"], "base_dt_none_front", black)
        check(same(base_none, OUT / "dt_none_back.png") and same(base_front, OUT / "dt_none_front.png"),
              f"dt_none (back / front light): byte-identical to the baseline {baseline.name}")


def display_switch_check(model, plain, extension, flags):
    """The per-material display switch (Material Inspector checkbox, active viewport) off renders like the material
    without the extension (the validate_asobo_anisotropy pattern: a UI script clicks the probe, then a late capture)."""
    ui_flags = ["--isolated", "--window-size", "1920x1080", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none",
                *VIEW, *flags, "--select-material", "0"]
    name = extension.replace("KHR_materials_", "")
    script, report, png = (OUT / f"ui_switch_{name}{suffix}" for suffix in (".txt", ".json", ".png"))
    script.write_text("\n".join(["wait 8", f"clickp khr.display.{extension} 0", "wait 10", "snap final",
                                 "move 900 20", "wait 220"]), encoding="utf-8")
    report.unlink(missing_ok=True)
    png.unlink(missing_ok=True)
    result = run([EXE, model, *ui_flags, "--ui-script", script, "--ui-report", report, "--capture-viewport", png,
                  "--capture-frame", "180"])
    data = read_json(report) or {}
    snaps = {snap["label"]: snap for snap in data.get("snaps", [])}
    probe = next((p for p in snaps.get("final", {}).get("probes", []) if p.get("tag") == f"khr.display.{extension}"), None)
    reference = OUT / f"ui_switch_{name}_reference.png"
    reference.unlink(missing_ok=True)
    run([EXE, plain, *ui_flags, "--capture-viewport", reference, "--capture-frame", "180", "--profile-report",
         OUT / f"ui_switch_{name}_reference.json", "--exit-after-profile"])
    check(not data.get("failed", True) and probe is not None and probe.get("selected") is False
          and "GL error 0x" not in result.stdout + result.stderr and same(png, reference),
          f"{extension}: the Material Inspector display switch exists; off renders byte-identical to the material "
          f"without the extension ({data.get('errors')})")


# --------------------------------------------------------------------------------------------- diffuse roughness

def dr_ext(factor=None, texture=None):
    e = {}
    if factor is not None:
        e["diffuseRoughnessFactor"] = factor
    if texture is not None:
        e["diffuseRoughnessTexture"] = {"index": texture}
    return {DR: e}


def dr_fixtures():
    f = {}
    f["none"] = write_model("dr_none", material(specular_off=True), shape="sphere", light="front")
    for label, r in (("zero", 0.0), ("half", 0.5), ("one", 1.0)):
        f[label] = write_model(f"dr_{label}", material(ext=dr_ext(r), specular_off=True), shape="sphere", light="front")
    f["tex"] = write_model("dr_tex", material(ext=dr_ext(1.0, texture=0), specular_off=True), shape="sphere",
                           light="front", images=[png_uri(halves((255, 0, 0, 255), (0, 0, 0, 255)))])
    f["anim"] = write_model("dr_anim", material(ext=dr_ext(0.0), specular_off=True), shape="sphere", light="front",
                            animation=(f"/materials/0/extensions/{DR}/diffuseRoughnessFactor", [0.0, 1.0, 1.0]))
    f["none_env"] = write_model("dr_none_env", material(), shape="sphere", light="front")
    f["zero_env"] = write_model("dr_zero_env", material(ext=dr_ext(0.0)), shape="sphere", light="front")
    return f


def dr_profile(session):
    """Probes along the sphere's horizontal centre row: (x, world, hdr) per pixel on the sphere."""
    rect = session.call("node_screen_rect", node="Shape")["screenRect"]
    cx, cy = (rect[0] + rect[2]) / 2.0, (rect[1] + rect[3]) / 2.0
    half = (rect[2] - rect[0]) / 2.0
    eye = session.call("get_camera")["eye"]
    rows = []
    for k in range(-30, 31):
        x = int(round(cx + k / 31.0 * half * 0.97))
        p = session.call("probe", x=x, y=int(round(cy)))
        if p.get("world") is not None:
            rows.append((x, p["world"], p["hdr"]))
    disc = session.region(cx - half - 2, cy - half - 2, cx + half + 2, cy + half + 2)
    return {"rows": rows, "eye": eye, "disc": disc, "rect": rect}


def dr_expected(world, eye, r):
    n = np.array(world, float)
    n /= np.linalg.norm(n)
    v = np.array(eye, float) - np.array(world, float)
    v /= np.linalg.norm(v)
    l = np.array([0.0, 0.0, 1.0])
    mu_i, mu_o = max(float(n @ l), 0.0), max(float(n @ v), 0.0)
    if r == 0.0:
        return mu_i * 0.8 / math.pi
    return mu_i * f_eon(0.8, r, mu_i, mu_o, float(l @ v))


def dr_checks(files, baseline, black):
    print("[KHR_materials_diffuse_roughness: formula]")
    albedos = [eon_albedo(1.0, mu) for mu in (1.0, 0.7, 0.3, 0.05)]
    check(all(abs(a - 1.0) <= 0.005 for a in albedos),
          f"EON transcription energy-preserving: directional albedo for rho 1, r 1 at mu_o 1 / 0.7 / 0.3 / 0.05 = "
          f"{[round(a, 4) for a in albedos]} (1 within the 0.5 % quadrature error: no gain, no loss)")
    print("[KHR_materials_diffuse_roughness: byte identity]")
    none, log = render(EXE, files["none"], "dr_none", black)
    zero, log2 = render(EXE, files["zero"], "dr_zero", black)
    check(same(none, zero) and gl_errors_zero(log) and gl_errors_zero(log2), "roughness 0: byte-identical to no extension")
    off, _ = render(EXE, files["one"], "dr_one_off", black + ["--disable-extension", DR])
    check(same(off, none), "roughness 1 with the extension switched off: byte-identical to no extension")
    a, _ = render(EXE, files["none_env"], "dr_none_env")
    b, _ = render(EXE, files["zero_env"], "dr_zero_env")
    check(same(a, b), "roughness 0 with specular and the default HDR environment: byte-identical to no extension")
    one_png, _ = render(EXE, files["one"], "dr_one", black)
    check(one_png is not None and not same(one_png, none), "roughness 1 differs from Lambert")

    print("[KHR_materials_diffuse_roughness: EON profile and energy]")
    prof = {}
    for label in ("zero", "half", "one"):
        prof[label], errors = with_session(files[label], black, dr_profile)
        check(errors == 0 and len(prof[label]["rows"]) > 40, f"dr_{label}: {len(prof[label]['rows'])} probes, GL errors {errors}")
    rows0 = prof["zero"]["rows"]
    centre = min(rows0, key=lambda row: abs(row[1][0]))
    scale = centre[2][1] / dr_expected(centre[1], prof["zero"]["eye"], 0.0)
    print(f"  INFO scale (radiance per unit BRDF x N.L, from the Lambert centre) {scale:.4f} (light intensity pi)")
    lam_err = max(rel(row[2][1], scale * dr_expected(row[1], prof["zero"]["eye"], 0.0)) for row in rows0
                  if dr_expected(row[1], prof["zero"]["eye"], 0.0) > 0.05)
    check(lam_err < 0.02, f"roughness 0 follows Lambert mu_i rho / pi (max deviation {lam_err * 100:.2f} %)")
    for label, r in (("half", 0.5), ("one", 1.0)):
        errs = [rel(row[2][1], scale * dr_expected(row[1], prof[label]["eye"], r)) for row in prof[label]["rows"]
                if dr_expected(row[1], prof[label]["eye"], 0.0) > 0.05]
        check(max(errs) < 0.02, f"roughness {r}: the row follows mu_i f_EON (max deviation {max(errs) * 100:.2f} % "
                                f"over {len(errs)} probes)")

    def limb_ratio(rows):
        c = min(rows, key=lambda row: abs(row[1][0]))[2][1]
        # The probe nearest |x| = 0.86 on each side: the fixed 0.84..0.88 window held no probe once the UI theme's
        # smaller viewport changed the probe spacing in world x.
        limb = [min((row for row in rows if side * row[1][0] > 0), key=lambda row: abs(abs(row[1][0]) - 0.86),
                    default=None) for side in (-1, 1)]
        limb = [row[2][1] for row in limb if row is not None and abs(abs(row[1][0]) - 0.86) < 0.03]
        return ratio(float(np.mean(limb)), c) if limb else float("nan")
    ratios = {label: limb_ratio(prof[label]["rows"]) for label in prof}
    check(ratios["zero"] < ratios["half"] < ratios["one"],
          f"the falloff flattens: limb (|x| ~ 0.86) / centre = {ratios['zero']:.3f} (r 0), {ratios['half']:.3f} "
          f"(r 0.5), {ratios['one']:.3f} (r 1)")
    # Energy: a near-orthographic view along the light (camera 46 units away, 5 degree field of view: the view and light
    # directions differ by at most 1.3 degrees), where the disc mean of EON over Lambert has a closed reference.
    tele = black + ["--camera-distance", "46", "--camera-fov", "5"]
    disc = {}
    for label in ("zero", "one"):
        disc[label], _ = with_session(files[label], tele, lambda s: dr_profile(s)["disc"])
    expected = disc_ratio(1.0)
    measured = ratio(disc["one"][1], disc["zero"][1])
    check(rel(measured, expected) < 0.02,
          f"energy: disc mean r 1 / Lambert, light = view = {measured:.4f}; EON gives {expected:.4f} for this "
          f"back-scattering view ({(expected - 1) * 100:+.1f} % against Lambert, stated limit; 2 %)")

    print("[KHR_materials_diffuse_roughness: texture, animation, pipe, validator]")
    tex, _ = render(EXE, files["tex"], "dr_tex", black)
    t, o, z = load(tex), load(one_png), load(none)
    w = t.shape[1] // 2
    # u = 0 .. 0.5 of the sphere is its back-left half: on screen the texture's left half (R 1) covers x > centre.
    right_match = np.abs(t[:, w + 4:] - o[:, w + 4:]).max()
    left_match = np.abs(t[:, : w - 4] - z[:, : w - 4]).max()
    check(right_match <= 1 and left_match <= 1,
          f"diffuseRoughnessTexture R: the half with 1 = roughness 1 ({right_match} levels), the half with 0 = Lambert "
          f"({left_match} levels)")
    a, _ = render(EXE, files["anim"], "dr_anim_0.5", black + ["--anim-time", "0.5"])
    b, _ = render(EXE, files["anim"], "dr_anim_1.5", black + ["--anim-time", "1.5"])
    check(same(a, zero) and same(b, one_png), "KHR_animation_pointer diffuseRoughnessFactor STEP 0 -> 1 at 0.5 / 1.5 s")

    def edit(s):
        reply = s.call("set_material_param", material="surface", extension=DR, param="diffuseRoughnessFactor", value=1.0)
        return reply, s.call("capture", path=str(OUT / "dr_pipe_edit.png"))["path"]
    (reply, edited), errors = with_session(files["zero"], black, edit)
    shot, _ = with_session(files["one"], black, lambda s: s.call("capture", path=str(OUT / "dr_pipe_one.png"))["path"])
    check(reply.get("value") == 1.0 and same(edited, shot) and errors == 0,
          f"set_material_param diffuseRoughnessFactor 0 -> 1 = the file with 1 (pipe captures) ({reply})")
    display_switch_check(files["one"], files["none"], DR, black)
    issues = validate(files["one"], "dr_one")
    check(issues is not None and not notices(issues), f"dr: no validation error, no extension notice ({notices(issues)})")
    if baseline is not None:
        base, _ = render(baseline, files["none"], "base_dr_none", black)
        check(same(base, none), f"dr_none: byte-identical to the baseline {baseline.name}")


# --------------------------------------------------------------------------------------------- MSFT

def msft_fixtures():
    f = {}

    def lod_doc(mode):
        doc = Doc()
        materials = [{"name": f"lod{i}", "pbrMetallicRoughness": {"baseColorFactor": c, "metallicFactor": 0.0}}
                     for i, c in enumerate(([0.8, 0.3, 0.2, 1], [0.2, 0.8, 0.3, 1], [0.2, 0.3, 0.8, 1]))]
        meshes = [sphere_mesh(doc, i, 48 // (i + 1), 24 // (i + 1)) for i in range(3)]
        nodes = [{"name": "High_LOD", "mesh": 0}, {"name": "Medium_LOD", "mesh": 1}, {"name": "Low_LOD", "mesh": 2}]
        document = {"asset": {"version": "2.0"}, "scene": 0, "scenes": [{"nodes": [0]}], "nodes": nodes,
                    "materials": materials, "meshes": meshes}
        if mode != "plain":
            nodes[0]["extensions"] = {"MSFT_lod": {"ids": [1, 2]}}
            nodes[0]["extras"] = {"MSFT_screencoverage": [0.5, 0.2, 0.01]}
            materials[0]["extensions"] = {"MSFT_lod": {"ids": [1, 2]}}
            document["extensionsUsed"] = ["MSFT_lod"]
            if mode == "required":
                document["extensionsRequired"] = ["MSFT_lod"]
        else:
            nodes[0]["extras"] = {"MSFT_screencoverage": [0.5, 0.2, 0.01]}
        path = FIXTURES / f"msft_lod_{mode}.gltf"
        path.write_text(json.dumps(doc.finish(document)), encoding="utf-8")
        return path
    for mode in ("lod", "plain", "required"):
        f[f"lod_{mode}"] = lod_doc(mode)

    orm = np.zeros((64, 64, 4), dtype=np.uint8)
    orm[..., 0] = np.linspace(40, 255, 64, dtype=np.uint8)[None, :]   # occlusion
    orm[..., 1] = np.linspace(60, 230, 64, dtype=np.uint8)[:, None]   # roughness
    orm[..., 2] = 255 - np.linspace(0, 255, 64, dtype=np.uint8)[None, :]  # metallic
    orm[..., 3] = 255
    other = orm.copy()
    other[..., 0], other[..., 1], other[..., 2] = 255, 128, 0
    images = [png_uri(orm), png_uri(other)]

    def orm_doc(name, core_index, packed_index, used):
        m = material((0.9, 0.6, 0.3), roughness=1.0, metallic=1.0, name="orm")
        if core_index is not None:
            m["pbrMetallicRoughness"]["metallicRoughnessTexture"] = {"index": core_index}
            m["occlusionTexture"] = {"index": core_index}
        if packed_index is not None:
            m["extensions"] = {"MSFT_packing_occlusionRoughnessMetallic": {
                "occlusionRoughnessMetallicTexture": {"index": packed_index}}}
        return write_model(f"msft_{name}", m, shape="sphere", light=None, images=images, used_extra=used)
    f["orm_packed"] = orm_doc("orm_packed", None, 0, ["MSFT_packing_occlusionRoughnessMetallic"])
    f["orm_core"] = orm_doc("orm_core", 0, None, [])
    f["orm_both"] = orm_doc("orm_both", 0, 1, ["MSFT_packing_occlusionRoughnessMetallic"])
    f["orm_none"] = orm_doc("orm_none", None, None, [])
    m = material((0.9, 0.6, 0.3), name="nrm")
    m["extensions"] = {"MSFT_packing_normalRoughnessMetallic": {"normalRoughnessMetallicTexture": {"index": 0}}}
    f["nrm"] = write_model("msft_nrm", m, shape="sphere", light=None, images=images,
                           used_extra=["MSFT_packing_normalRoughnessMetallic"])
    f["nrm_plain"] = write_model("msft_nrm_plain", material((0.9, 0.6, 0.3), name="nrm"), shape="sphere", light=None,
                                 images=images)
    return f


def msft_checks(files, baseline):
    print("[MSFT_lod]")
    plain, log = render(EXE, files["lod_plain"], "msft_lod_plain")
    check(plain is not None and gl_errors_zero(log), "msft_lod_plain: captured, zero GL errors")
    for mode in ("lod", "required"):
        image, _ = render(EXE, files[f"lod_{mode}"], f"msft_lod_{mode}")
        check(same(image, plain), f"msft_lod_{mode}: the highest LOD renders byte-identical to the twin without MSFT_lod")
    image, log = render(EXE, files["lod_required"], "msft_lod_required_off", ["--disable-extension", "MSFT_lod"])
    check("requires MSFT_lod, which is switched off" in log and "Failed to load" in log,
          "required + switched off: not loaded, the log names the reason")

    print("[MSFT_packing_occlusionRoughnessMetallic / normalRoughnessMetallic]")
    img = {}
    for key in ("orm_packed", "orm_core", "orm_both", "orm_none", "nrm", "nrm_plain"):
        img[key], log = render(EXE, files[key], f"msft_{key}")
        check(img[key] is not None and gl_errors_zero(log), f"msft_{key}: captured, zero GL errors")
    check(same(img["orm_packed"], img["orm_core"]),
          "packed ORM texture without core textures = the same texture in the core metallic-roughness / occlusion slots")
    check(not same(img["orm_packed"], img["orm_none"]), "the packed texture changes the render (it is used)")
    check(same(img["orm_both"], img["orm_core"]), "with core textures the packed texture is ignored (= core twin)")
    off, _ = render(EXE, files["orm_packed"], "msft_orm_packed_off",
                    ["--disable-extension", "MSFT_packing_occlusionRoughnessMetallic"])
    check(same(off, img["orm_none"]), "switched off: renders like the material without textures")
    check(same(img["nrm"], img["nrm_plain"]), "MSFT_packing_normalRoughnessMetallic: recognised only (= plain twin)")

    print("[MSFT: validator]")
    for key in ("lod_lod", "lod_required", "orm_packed", "nrm"):
        issues = validate(files[key], f"msft_{key}")
        check(issues is not None and not notices(issues), f"msft_{key}: no validation error, no extension notice "
                                                          f"({notices(issues)})")
    if baseline is not None:
        base, _ = render(baseline, files["orm_core"], "base_msft_orm_core")
        check(same(base, img["orm_core"]), f"msft_orm_core (core textures): byte-identical to the baseline {baseline.name}")


# --------------------------------------------------------------------------------------------- Khronos samples

def sample_checks():
    print("[Khronos samples, lit from behind]")
    samples = [SAMPLES / "DiffuseTransmissionTest.glb", SAMPLES / "DiffuseTransmissionPlant.glb",
               SAMPLES / "DiffuseTransmissionTeacup.glb", SAMPLES / "MandarinOrange" / "MandarinOrange.gltf"]
    back_sun = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off",
                "--sun", "on", "--sun-mode", "manual", "--sun-dir", "180,25", "--sun-intensity", "4", "--hide-grid"]
    for path in samples:
        if not path.exists():
            print(f"  INFO {path} not present (download it from KhronosGroup/glTF-Sample-Assets): skipped")
            continue
        on, _, log = capture(EXE, path, OUT / f"sample_{path.stem}.png", back_sun)
        off, _, _ = capture(EXE, path, OUT / f"sample_{path.stem}_off.png", back_sun + ["--disable-extension", DT])
        check(on is not None and gl_errors_zero(log) and off is not None and not same(on, off),
              f"{path.stem}: captured lit from behind, zero GL errors, differs with the extension switched off")
        issues = validate(path, f"sample_{path.stem}")
        check(issues is not None and not [c for c in notices(issues) if c.startswith("EXTENSION_")],
              f"{path.stem}: no extension notice")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", help="reference binary (default: the newest build/baselines/*.exe)")
    parser.add_argument("--only", choices=("dt", "dr", "msft", "samples"), action="append")
    args = parser.parse_args()
    baseline = baseline_exe(args.baseline_exe)
    if baseline is not None and not baseline.exists():
        baseline = None
    FIXTURES.mkdir(parents=True, exist_ok=True)
    print(f"[fixtures] candidate {EXE}" + (f", baseline {baseline.name}" if baseline else ", no baseline"))
    black = ["--env", str(write_hdr(OUT / "black.hdr", 64, 32, lambda x, y: (0.0, 0.0, 0.0)))]

    def behind_pixel(x, y):
        u = (x + 0.5) / 128.0
        du = min(abs(u - 0.25), 1.0 - abs(u - 0.25))
        return (0.6, 0.6, 0.6) if du < 0.25 else (0.0, 0.0, 0.0)
    behind = ["--env", str(write_hdr(OUT / "behind.hdr", 128, 64, behind_pixel))]
    parts = args.only or ["dt", "dr", "msft", "samples"]
    runs = {"dt": lambda: dt_checks(dt_fixtures(), baseline, black, behind),
            "dr": lambda: dr_checks(dr_fixtures(), baseline, black),
            "msft": lambda: msft_checks(msft_fixtures(), baseline),
            "samples": sample_checks}
    for part in ("dt", "dr", "msft", "samples"):
        if part in parts:
            try:
                runs[part]()
            except Exception as error:  # a part that cannot continue fails; the other parts still run
                check(False, f"part {part} aborted: {type(error).__name__}: {error}")


if __name__ == "__main__":
    main()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    sys.exit(1 if failures else 0)
