# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""A3.7: ASOBO_sky_portal (docs/ASOBO_EXTENSIONS_PHASE_3.md, A3.7; semantics section 19).

A portal is a disc area light at its node, facing into the cabin along the node's +Z axis, radius source_radius / 100
(centimetres), cone falloff between outer_cone_angle / 2 and inner_cone_angle / 2. pbr.frag adds, for OPAQUE / MASK
materials of a file with portals, (1 - occlusion) x min(strength x portal diffuse, unoccluded IBL diffuse), the portal
diffuse being albedo / pi x E with E = pi L cos(emit) cos(receive) r^2 / (d^2 + r^2) x cone and L the environment seen
through the window (rough GGX mip in the direction receiver -> disc centre).

Fixture (written here): an open-topped 4 x 4 m room (floor, walls 2.5 m high, no occlusion map) with one portal in the
+X wall at (2, 1.25, 0) facing -X (r = 0.5 m, inner 160, outer 180), white 0.2 m floor patches (albedo 0.8, KHR
specular 0, a constant occlusion map 77/255) at known positions inside, one such patch outside behind the window
wall, and a ground quad outside without an occlusion map. Lambert only, uniform environment L = 1 unless stated.

Checks:
  * typed model / Inspector parse (--asobo-report) of the DA62 SDK cockpit LOD00 (8 portals: names, source radius
    in cm and metres, cone angles, shape 1, every direction pointing into the cabin), LOD01 and LOD02 (their own
    portals), the support status (Partial) and no ASOBO diagnostics on the portal nodes; the schema is in the VFS
    (hasSpec);
  * files without portals: byte-identical to the baseline (HelloCube, the fixture without its portal node, the E195
    cockpit_main_displays view); the DA62 cockpit with --asobo-disable ASOBO_sky_portal and with --asobo off is
    byte-identical to the baseline (its default and its --asobo off render), and the default differs (the portal is
    on); its textures load (no missing-texture magenta);
  * fixture: every inside patch gains exactly the analytic amount (tolerance 4% + 2e-4), the gain falls with distance
    along the axis, the grazing patch is reduced by the cone; strength 2 doubles the gain; the floor and walls
    (occlusion 1), the patch behind the window wall and the outside ground are unchanged; changed pixels lie only on
    the inside patches; all probes finite; an environment bright towards +X (outside the window) gives more window
    light than the same environment turned by 180 degrees (ratio >= BRIGHT_RATIO);
  * registry: ASOBO_sky_portal, sky_portal_strength (out-of-range refused), show_sky_portals; get_state skyPortals
    (1 portal, active 1, 0 when switched off); the gizmo changes the window image (show_sky_portals, selected portal);
  * zero GL errors.

Every Glider process this script starts is killed if its session fails (no window is left open).

Run: python scripts/validate_asobo_sky_portal.py [--baseline-exe build/baselines/<sha>.exe] [--only a,b,...]
"""

import argparse
import atexit
import io
import json
import math
import os
import sys
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402
from shadow_fixtures import Builder  # noqa: E402
from review_shadows import DA62_COCKPIT, DA62_TEXTURES  # noqa: E402
from validate_asobo_common import EXE, REPO, check, finish, same, magenta, view, render_view  # noqa: E402
from validate_asobo_sss import write_hdr  # noqa: E402
from validate_sun import gl_errors_zero  # noqa: E402

OUT = REPO / "build" / "asobo_sky_portal"
COMMON = ["--isolated", "--window-size", "1280x720", "--fixed-frame-time", "0.05", "--vsync", "off", "--aa", "none"]
LOOK = ["--clear-color", "0,0,0", "--hide-grid", "--bloom", "off", "--auto-exposure", "off", "--exposure", "0",
        "--tone-mapping", "neutral"]
ROOM_VIEW = ["--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "75", "--camera-distance", "9"]
DA62_PACKAGED = Path(r"C:\MSFS 2024 Flighting SDK\Samples\DevmodeProjects\SimObjects\Aircraft\DA62\Packages"
                     r"\asobo-aircraft-da62-sdk\SimObjects\Airplanes\DA62_SDK\attachments\asobo\Part_Interior_Cockpit"
                     r"\model")
DA62_VIEW = ["--camera-target", "0.3,0.62,0.1", "--camera-distance", "0.05", "--camera-pitch", "-20",
             "--camera-fov", "55", "--camera-yaw", "220", "--hide-grid", "--search-path", str(DA62_TEXTURES)]
DA62_CABIN_CENTRE = (0.0, 0.6, -0.4)  # between the seats, below the canopy (DA62 cockpit file, metres)

PORTAL_POSITION = (2.0, 1.25, 0.0)
PORTAL_NORMAL = (-1.0, 0.0, 0.0)
PORTAL_RADIUS_CM = 50.0
PORTAL_INNER, PORTAL_OUTER = 160.0, 180.0
ALBEDO = 0.8
AO_LEVEL = 77
PATCH = 0.2
INSIDE = {"PatchA": (1.5, 0.0), "PatchB": (0.5, 0.0), "PatchC": (-0.5, 0.0), "PatchD": (-1.5, 0.0),
          "PatchE": (1.0, 1.5), "PatchF": (1.8, -1.6)}
OUTSIDE = {"PatchOut": (3.0, 0.0)}
PATCH_Y = 0.002
TOLERANCE_REL, TOLERANCE_ABS = 0.04, 2e-4
BRIGHT_RATIO = 3.0

gl_logs = []
_processes = []


def _kill_all():
    for process in _processes:
        if process.poll() is None:
            process.kill()


atexit.register(_kill_all)


def run(args, timeout=900):
    return subprocess.run([str(a) for a in args], cwd=REPO, capture_output=True, text=True, timeout=timeout)


def render(model, name, flags=(), exe=EXE, frame=24, common=COMMON):
    OUT.mkdir(parents=True, exist_ok=True)
    png, report = OUT / f"{name}.png", OUT / f"{name}.json"
    png.unlink(missing_ok=True)
    report.unlink(missing_ok=True)
    args = [exe] + ([model] if model else []) + list(common) + list(flags)
    result = run(args + ["--capture-viewport", png, "--capture-frame", str(frame), "--profile-report", report,
                         "--exit-after-profile"])
    if Path(exe) == EXE:
        gl_logs.append((name, result.stdout + result.stderr))
    return png if png.exists() else None


# ---------------------------------------------------------------------------------------------
# Fixtures
# ---------------------------------------------------------------------------------------------

def grey_texture(builder, level):
    from PIL import Image
    import base64
    out = io.BytesIO()
    Image.new("RGB", (4, 4), (level, level, level)).save(out, "PNG")
    builder.images.append({"uri": "data:image/png;base64," + base64.b64encode(out.getvalue()).decode("ascii")})
    if not builder.samplers:
        builder.samplers.append({"magFilter": 9728, "minFilter": 9728, "wrapS": 33071, "wrapT": 33071})
    builder.textures.append({"source": len(builder.images) - 1, "sampler": 0})
    return len(builder.textures) - 1


def room(path, portal=True, inner=PORTAL_INNER, outer=PORTAL_OUTER):
    b = Builder("A3.7 sky portal room")
    wall = b.material("Room", color=(0.5, 0.5, 0.5, 1.0))
    patch = b.material("Patch", color=(ALBEDO, ALBEDO, ALBEDO, 1.0))
    b.materials[patch]["occlusionTexture"] = {"index": grey_texture(b, AO_LEVEL)}
    ground = b.material("Ground", color=(0.3, 0.3, 0.3, 1.0))
    b.quad("Ground", (0.0, -0.07, 0.0), (14.0, 14.0), ground)
    b.box("Floor", (-2.0, -0.05, -2.0), (2.0, 0.0, 2.0), wall)
    b.box("WallPX", (2.0, 0.0, -2.0), (2.05, 2.5, 2.0), wall)
    b.box("WallNX", (-2.05, 0.0, -2.0), (-2.0, 2.5, 2.0), wall)
    b.box("WallPZ", (-2.0, 0.0, 2.0), (2.0, 2.5, 2.05), wall)
    b.box("WallNZ", (-2.0, 0.0, -2.05), (2.0, 2.5, -2.0), wall)
    for name, (x, z) in {**INSIDE, **OUTSIDE}.items():
        b.quad(name, (x, PATCH_Y, z), (PATCH, PATCH), patch)
    if portal:
        # Node +Z = into the room = world -X: a -90 degree turn about Y.
        s = math.sqrt(0.5)
        b.nodes.append({"name": "PORTAL_Window", "translation": list(PORTAL_POSITION), "rotation": [0.0, -s, 0.0, s],
                        "extensions": {"ASOBO_sky_portal": {"shape_type": 1, "source_radius": PORTAL_RADIUS_CM,
                                                            "inner_cone_angle": inner, "outer_cone_angle": outer}}})
        b.used.add("ASOBO_sky_portal")
    return b.write(path)


def uniform_hdr(path, value=1.0):
    return write_hdr(path, 64, 32, lambda x, y: (value, value, value))


def bright_hdr(path, bright=4.0, dim=0.25):
    """Bright where the direction's x > 0.3 (outside the room's window), dim elsewhere. Equirectangular, row 0 = up,
    the convention of validate_sun.synthetic_hdr (x = sin(theta) cos(phi))."""
    width, height = 128, 64

    def pixel(x, y):
        theta = (y + 0.5) / height * math.pi
        phi = ((x + 0.5) / width - 0.5) * 2.0 * math.pi
        value = bright if math.sin(theta) * math.cos(phi) > 0.3 else dim
        return (value, value, value)
    return write_hdr(path, width, height, pixel)


def expected_gain(x, z, strength=1.0, radiance=1.0):
    """Analytic added linear value of an inside patch centre under a uniform environment (see the docstring)."""
    px, py, pz = PORTAL_POSITION
    d = (px - x, py - PATCH_Y, pz - z)
    d2 = sum(c * c for c in d)
    length = math.sqrt(d2)
    l = tuple(c / length for c in d)
    cos_emit = -sum(n * c for n, c in zip(PORTAL_NORMAL, l))
    cos_receive = l[1]
    if cos_emit <= 0 or cos_receive <= 0:
        return 0.0
    cos_inner, cos_outer = math.cos(math.radians(PORTAL_INNER / 2)), math.cos(math.radians(PORTAL_OUTER / 2))
    t = min(max((cos_emit - cos_outer) / (cos_inner - cos_outer), 0.0), 1.0)
    cone = t * t * (3 - 2 * t)
    r2 = (PORTAL_RADIUS_CM / 100.0) ** 2
    form = cos_emit * cos_receive * r2 / (d2 + r2)
    portal = strength * radiance * ALBEDO * form * cone
    unoccluded = ALBEDO * radiance
    return (1.0 - AO_LEVEL / 255.0) * min(portal, unoccluded)


def cone_factor(x, z):
    px, py, pz = PORTAL_POSITION
    d = (px - x, py - PATCH_Y, pz - z)
    length = math.sqrt(sum(c * c for c in d))
    cos_emit = d[0] / length
    cos_inner, cos_outer = math.cos(math.radians(PORTAL_INNER / 2)), math.cos(math.radians(PORTAL_OUTER / 2))
    t = min(max((cos_emit - cos_outer) / (cos_inner - cos_outer), 0.0), 1.0)
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------------------------------------
# Control-pipe session
# ---------------------------------------------------------------------------------------------

class Session:
    """A --control-pipe Glider; the process is killed whenever the session fails or the script exits."""

    def __init__(self, model, flags=()):
        args = [str(EXE)] + ([str(model)] if model else []) + COMMON + list(flags) + ["--control-pipe"]
        self.process = subprocess.Popen(args, cwd=REPO, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        _processes.append(self.process)
        self.pipe = None
        try:
            deadline = time.time() + 120
            while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
                if self.process.poll() is not None or time.time() > deadline:
                    raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
                time.sleep(0.1)
            while True:
                try:
                    self.pipe = glider_ctl.GliderPipe(self.process.pid)
                    break
                except glider_ctl.GliderError:
                    if self.process.poll() is not None or time.time() > deadline:
                        raise
                    time.sleep(0.2)
        except BaseException:
            self.kill()
            raise

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def request(self, cmd, **args):
        return self.pipe.request(cmd, args)

    def capture(self, name, frames=8):
        path = OUT / f"{name}.png"
        self.call("capture", path=str(path), settle_frames=frames, overlays=False)
        return path

    def kill(self):
        if self.pipe is not None:
            try:
                self.pipe.close()
            except Exception:  # noqa: BLE001
                pass
        if self.process.poll() is None:
            self.process.kill()
            self.process.wait(30)

    def close(self):
        errors = None
        try:
            errors = self.call("get_state")["glErrors"]
            self.call("quit")
            self.pipe.close()
            self.pipe = None
            self.process.wait(60)
        finally:
            self.kill()
        return errors


def patch_rects(session, names):
    rects = {}
    for name in names:
        rect = session.call("node_screen_rect", node=name)
        rects[name] = rect["screenRect"] if rect.get("onScreen", True) else None
    return rects


def inner_rect(rect, fraction=0.3):
    x0, y0, x1, y1 = rect
    cx, cy, hw, hh = (x0 + x1) / 2, (y0 + y1) / 2, (x1 - x0) * fraction / 2, (y1 - y0) * fraction / 2
    return int(cx - hw), int(cy - hh), int(math.ceil(cx + hw)), int(math.ceil(cy + hh))


def probe(session, rect):
    x0, y0, x1, y1 = inner_rect(rect)
    return session.call("probe_region", x0=x0, y0=y0, x1=max(x1, x0 + 1), y1=max(y1, y0 + 1))["rgb"]["mean"][1]


def probe_all(session, rects):
    return {name: probe(session, rect) for name, rect in rects.items() if rect is not None}


def set_portal(session, applied, strength=None):
    session.call("set_viewport_setting", key="ASOBO_sky_portal", value=applied)
    if strength is not None:
        session.call("set_viewport_setting", key="sky_portal_strength", value=strength)


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def asobo_report(model, name):
    path = OUT / f"{name}.asobo.json"
    path.unlink(missing_ok=True)
    result = run([EXE, "--isolated", model, "--asobo-report", path])
    if path.exists() and result.returncode == 0:
        return json.loads(path.read_text(encoding="utf-8"))
    return None


def check_model():
    print("[typed model / Inspector parse: DA62 SDK cockpit]")
    lod0 = DA62_PACKAGED / "Cockpit_LOD00.gltf"
    if not lod0.exists():
        print("  SKIP DA62 SDK cockpit not found")
        return
    rep = asobo_report(lod0, "da62_lod00")
    check(rep is not None, "LOD00 --asobo-report written")
    if rep is None:
        return
    portals = rep.get("skyPortals", [])
    check(len(portals) == 8 and all(p["name"].startswith("PORTAL_") for p in portals),
          f"LOD00: 8 PORTAL_* nodes parsed ({len(portals)})")
    radii = {p["name"]: p["sourceRadiusCm"] for p in portals}
    check(radii.get("PORTAL_Window_BL") == 10.0 and radii.get("PORTAL_Window_FL") == 27.0
          and radii.get("PORTAL_Window_ML") == 25.0 and radii.get("PORTAL_Windshield_L") == 20.0,
          f"source radii in cm as written (BL 10, FL 27, ML 25, windshield 20): {radii}")
    check(all(abs(p["radiusMetres"] - p["sourceRadiusCm"] / 100.0) < 1e-9 for p in portals),
          "radius in metres = source_radius / 100")
    check(all(p["shapeType"] == 1 and p["outerConeDeg"] == 180.0 and p["innerConeDeg"] in (160.0, 180.0)
              for p in portals), "shape 1 (disc), outer 180, inner 160 (FL: 180)")
    inward = []
    for p in portals:
        to_centre = [c - q for c, q in zip(DA62_CABIN_CENTRE, p["position"])]
        inward.append(sum(a * b for a, b in zip(to_centre, p["direction"])) / math.sqrt(sum(c * c for c in to_centre)))
    check(all(value > 0.2 for value in inward),
          f"every portal's +Z points into the cabin (cos to the cabin centre min {min(inward):.2f})")
    check(rep.get("support", {}).get("ASOBO_sky_portal") == "Partial", "support status: Partial")
    node_objects = [o for o in rep.get("objects", []) if any(e["name"] == "ASOBO_sky_portal" for e in o["extensions"])]
    check(len(node_objects) == 8 and all(e["hasSpec"] for o in node_objects for e in o["extensions"]
                                         if e["name"] == "ASOBO_sky_portal"),
          "the 8 node extensions have the typed spec (schema in the VFS: the Inspector shows typed fields)")
    pointers = {o["pointer"] for o in node_objects}
    check(not [d for d in rep.get("diagnostics", []) if d["pointer"].split("/extensions")[0] in pointers],
          "no ASOBO diagnostics on the portal nodes")
    for lod in ("Cockpit_LOD01.gltf", "Cockpit_LOD02.gltf"):
        model = DA62_PACKAGED / lod
        if not model.exists():
            continue
        doc = json.loads(model.read_text(encoding="utf-8"))
        written = sum("ASOBO_sky_portal" in n.get("extensions", {}) for n in doc.get("nodes", []))
        lod_rep = asobo_report(model, lod.replace(".gltf", ""))
        count = len(lod_rep.get("skyPortals", [])) if lod_rep else -1
        check(count == written, f"{lod}: all {written} portals parsed ({count})")


def check_baseline(baseline, uniform):
    print("[off and files without portals: byte-identical to the baseline]")
    if baseline is None or not baseline.exists():
        print("  SKIP no baseline binary")
        return
    plain = room(OUT / "room_without_portal.gltf", portal=False)
    flags = ["--env", uniform] + LOOK + ROOM_VIEW
    a = render(plain, "cur_room_plain", flags)
    b = render(plain, "base_room_plain", flags, exe=baseline)
    check(same(a, b), f"fixture without the portal node: byte-identical to {baseline.stem}")
    a = render(None, "cur_hellocube")
    b = render(None, "base_hellocube", exe=baseline)
    check(same(a, b), f"HelloCube: byte-identical to {baseline.stem}")
    entry = view("cockpit_main_displays")
    a = render_view(EXE, entry, "cur_e195_cockpit", folder=OUT)
    b = render_view(baseline, entry, "base_e195_cockpit", folder=OUT)
    check(a is not None and same(a, b), f"E195 cockpit_main_displays: byte-identical to {baseline.stem}")
    if not DA62_COCKPIT.exists():
        print("  SKIP DA62 cockpit not found")
        return
    # The same switch on both sides: a baseline from before A3.7 ignores it and renders without portals anyway.
    disable = ["--asobo-disable", "ASOBO_sky_portal"]
    base = render(DA62_COCKPIT, "base_da62", DA62_VIEW + disable, exe=baseline)
    check(base is not None and magenta(base) < 0.001, f"DA62 cockpit textures load (magenta {magenta(base):.5f})")
    off = render(DA62_COCKPIT, "cur_da62_disable", DA62_VIEW + disable)
    check(same(off, base), f"DA62 cockpit, --asobo-disable ASOBO_sky_portal: byte-identical to {baseline.stem}")
    on = render(DA62_COCKPIT, "cur_da62_on", DA62_VIEW)
    check(on is not None and not same(on, off), "DA62 cockpit: the default (portals on) differs from portals off")
    base_off = render(DA62_COCKPIT, "base_da62_asobo_off", DA62_VIEW + ["--asobo", "off"], exe=baseline)
    cur_off = render(DA62_COCKPIT, "cur_da62_asobo_off", DA62_VIEW + ["--asobo", "off"])
    check(same(cur_off, base_off), f"DA62 cockpit, --asobo off: byte-identical to {baseline.stem} --asobo off")


def changed_outside(off_png, on_png, rects, margin=2):
    import numpy as np
    from PIL import Image
    a = np.asarray(Image.open(off_png).convert("RGB"), dtype=np.int16)
    b = np.asarray(Image.open(on_png).convert("RGB"), dtype=np.int16)
    changed = np.abs(a - b).max(axis=2) > 0
    allowed = np.zeros_like(changed)
    for rect in rects:
        x0, y0, x1, y1 = rect
        allowed[max(0, int(y0) - margin):int(math.ceil(y1)) + margin, max(0, int(x0) - margin):int(math.ceil(x1)) + margin] = True
    return int((changed & ~allowed).sum()), int(changed.sum())


def check_fixture(uniform, bright):
    print("[fixture: box room with one window portal]")
    model = room(OUT / "room.gltf")
    session = Session(model, ["--env", str(uniform), "--sky-portal-strength", "1"] + LOOK + ROOM_VIEW)
    try:
        state = session.call("get_state")
        portals = state.get("skyPortals", {})
        session.capture("_settle", 12)
        check(len(portals.get("portals", [])) == 1 and session.call("get_state")["skyPortals"]["active"] == 1,
              "get_state skyPortals: one portal, active 1")
        portal = portals["portals"][0] if portals.get("portals") else {}
        check(portal and max(abs(a - b) for a, b in zip(portal["direction"], PORTAL_NORMAL)) < 1e-5
              and abs(portal["radius"] - PORTAL_RADIUS_CM / 100) < 1e-6,
              f"portal direction {portal.get('direction')} (node +Z) and radius {portal.get('radius')} m")
        names = list(INSIDE) + list(OUTSIDE) + ["Ground", "Floor"]
        rects = patch_rects(session, names)
        # Floor and Ground rects cover the patches; probe them at points away from every patch.
        rects["Floor"] = None
        rects["Ground"] = None
        on_png = session.capture("room_on")
        on = probe_all(session, rects)
        floor_point = session.call("probe_region", x0=0, y0=0, x1=1, y1=1)
        set_portal(session, False)
        off_png = session.capture("room_off")
        check(session.call("get_state")["skyPortals"]["active"] == 0, "switched off: active 0")
        off = probe_all(session, rects)
        set_portal(session, True, 2.0)
        session.capture("room_on_x2")
        on2 = probe_all(session, rects)
        set_portal(session, True, 1.0)
        bad = session.request("set_viewport_setting", key="sky_portal_strength", value=5)
        check(not bad["ok"] and bad["error"]["code"] == "bad_value", "sky_portal_strength 5 is refused (range 0..4)")
        listed = {e["key"] for e in session.call("list_settings")["settings"]}
        check({"ASOBO_sky_portal", "sky_portal_strength", "show_sky_portals"} <= listed,
              "registry: ASOBO_sky_portal, sky_portal_strength, show_sky_portals")
        errors = session.close()
        check(errors == 0, f"uniform-environment session: zero GL errors ({errors})")
    except BaseException:
        session.kill()
        raise

    values = list(on.values()) + list(off.values()) + list(on2.values())
    check(all(math.isfinite(v) for v in values) and math.isfinite(floor_point["rgb"]["mean"][1]), "every probe is finite")
    gains = {}
    for name, (x, z) in INSIDE.items():
        gain, want = on[name] - off[name], expected_gain(x, z)
        gains[name] = gain
        check(abs(gain - want) <= TOLERANCE_REL * want + TOLERANCE_ABS,
              f"{name} ({x}, {z}): gain {gain:.5f} = analytic {want:.5f} (off {off[name]:.4f})")
        gain2 = on2[name] - off[name]
        check(abs(gain2 - 2 * gain) <= 0.02 * gain + TOLERANCE_ABS, f"{name}: strength 2 doubles the gain ({gain2:.5f})")
    check(gains["PatchA"] > gains["PatchB"] > gains["PatchC"] > gains["PatchD"] > 0,
          "the gain falls with the distance along the axis: " + ", ".join(f"{gains[k]:.4f}" for k in "PatchA PatchB PatchC PatchD".split()))
    factor = cone_factor(*INSIDE["PatchF"])
    check(0.05 < factor < 0.95 and gains["PatchF"] < expected_gain(*INSIDE["PatchF"]) / factor * 0.97,
          f"grazing PatchF is cut by the cone (factor {factor:.3f}): gain {gains['PatchF']:.5f}")
    check(on["PatchOut"] == off["PatchOut"], f"patch outside, behind the window wall: unchanged ({on['PatchOut']:.5f})")
    outside, total = changed_outside(off_png, on_png, [rects[n] for n in INSIDE])
    check(total > 0 and outside == 0,
          f"changed pixels lie only on the inside patches ({total} changed, {outside} elsewhere: floor, walls, ground)")

    print("[fixture: environment bright towards the window]")
    results = {}
    for label, rotation in (("bright_out", "0"), ("bright_in", "180")):
        session = Session(model, ["--env", str(bright), "--hdr-rotation", rotation, "--sky-portal-strength", "1"] + LOOK + ROOM_VIEW)
        try:
            session.capture("_settle", 12)
            rect = patch_rects(session, ["PatchB"])
            session.capture(f"room_{label}_on")
            on_value = probe_all(session, rect)["PatchB"]
            set_portal(session, False)
            session.capture(f"room_{label}_off")
            off_value = probe_all(session, rect)["PatchB"]
            errors = session.close()
            check(errors == 0, f"{label}: zero GL errors ({errors})")
        except BaseException:
            session.kill()
            raise
        results[label] = on_value - off_value
    check(results["bright_out"] > 0 and results["bright_out"] >= BRIGHT_RATIO * max(results["bright_in"], 1e-9),
          f"window towards the bright side adds {results['bright_out']:.5f}, turned 180 deg {results['bright_in']:.5f}")


def check_gizmo(uniform):
    print("[gizmo]")
    model = room(OUT / "room.gltf")
    shots = {}
    for label, flags in (("none", []), ("all", ["--show-sky-portals"]), ("selected", ["--select-node", "PORTAL_Window"])):
        png = OUT / f"gizmo_{label}.png"
        png.unlink(missing_ok=True)
        result = run([EXE, model] + COMMON + ["--env", uniform] + LOOK + ROOM_VIEW + flags +
                     ["--dump-frame", png, "--exit-after-dump"])
        # --dump-frame runs print no GL error summary; the gizmo is ImGui draw-list geometry only.
        shots[label] = png if png.exists() and result.returncode == 0 else None
    check(all(shots.values()), "window dumps written (exit code 0)")
    check(shots["none"] and shots["all"] and not same(shots["none"], shots["all"]),
          "--show-sky-portals draws the gizmo (window image changes)")
    check(shots["none"] and shots["selected"] and not same(shots["none"], shots["selected"]),
          "a selected portal node shows its gizmo")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--baseline-exe", default=None)
    parser.add_argument("--only", default=None, help="comma-separated: model,baseline,fixture,gizmo")
    args = parser.parse_args()
    wanted = (lambda name: True) if not args.only else (lambda name: name in args.only.split(","))
    OUT.mkdir(parents=True, exist_ok=True)
    baseline = Path(args.baseline_exe) if args.baseline_exe else None
    if baseline is None:
        candidates = sorted((REPO / "build" / "baselines").glob("*.exe"), key=lambda p: p.stat().st_mtime)
        baseline = candidates[-1] if candidates else None
    uniform = uniform_hdr(OUT / "uniform.hdr")
    bright = bright_hdr(OUT / "bright_x.hdr")
    if wanted("model"):
        check_model()
    if wanted("baseline"):
        check_baseline(baseline, uniform)
    if wanted("fixture"):
        check_fixture(uniform, bright)
    if wanted("gizmo"):
        check_gizmo(uniform)
    print("[GL errors]")
    bad = [name for name, log in gl_logs if not gl_errors_zero(log)]
    check(not bad, f"zero GL errors in every capture run ({len(gl_logs)} runs; failing: {bad or 'none'})")
    finish()


if __name__ == "__main__":
    main()
