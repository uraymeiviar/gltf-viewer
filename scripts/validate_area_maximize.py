# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Maximize Area (Ctrl+Space), Full Screen (Ctrl+Alt+Space / Esc) and drag-and-drop open.

One Glider process at a time; every run uses a temporary GLIDER_DATA_DIR (or --isolated / follow mode), and
every launched instance is closed (full screen is a borderless window, never a display-mode change).

Checks:
  * pipe (toggle_maximize_area): a viewport and a non-viewport panel fill the dock area (menu and status bar stay),
    every other panel is hidden; the maximized viewport renders at the new size with the same aspect (node rect
    ratio) and picks the same node at its centre; restore gives the identical docking layout (ImGui ini text) and a
    byte-identical viewport capture;
  * pipe (toggle_fullscreen): the window is borderless and covers its monitor, no menu or status bar, the area fills
    the screen; leaving restores the window rectangle, decoration and layout; maximized -> full screen -> back ends
    maximized, then restored;
  * UI script keys (follow mode): one Ctrl+Space round trip with the mouse over the viewport (maximized, restored to
    the exact layout); with --keys (on-demand) the whole section: Ctrl+Space over the viewport and over the
    Inspector, Ctrl+Alt+Space and Esc, the shortcut ignored while a text field is active, camera input (wheel) in the
    maximized viewport, the Panel menu and the viewport menu items; full-frame screenshots of the maximized viewport,
    the maximized Inspector and full screen;
  * open_dropped (the drop callback's path): a .glb, a .gltf, a non-glTF file (warning, nothing opened), two files
    (first opens, the rest is logged as ignored), a Unicode file name (model path, window title, recent files with
    set_persist), a corrupt .glb (error, Glider keeps running);
  * settings.ini: exit while maximized stores the restored layout (same [Layout] as without maximizing), exit in
    full screen stores no full-screen window rectangle, --isolated writes nothing;
  * no GL errors in any session.

Usage: python scripts/validate_area_maximize.py [--keys] [pipe] [keys] [drops] [settings]
  --keys   on-demand: the whole "keys and menus" section instead of the single Ctrl+Space round trip
Screenshots: build/review/area-maximize/.
"""

import ctypes
import difflib
import json
import os
import shutil
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
EXE = subprocess.candidate_exe()
OUT = REPO / "build" / "area_maximize"
REVIEW = REPO / "build" / "review" / "area-maximize"
MODEL = REPO / "resources" / "test_models" / "hierarchy_animation_fixture.gltf"
GLB = REPO / "resources" / "sampleGltf" / "HelloCube.glb"
COMMON = ["--window-size", "1600x900", "--aa", "none", "--hide-grid", "--fixed-frame-time", "0.05", "--vsync", "off"]
VIEW = ["--clear-color", "0.1,0.2,0.3", "--bloom", "off", "--auto-exposure", "off", "--tone-mapping", "neutral",
        "--camera-target", "0,0,0", "--camera-yaw", "20", "--camera-pitch", "15", "--camera-distance", "9"]
IGNORED_WINDOWS = {"gizmo"}  # ImGuizmo's overlay window
VIEWPORT = "###Viewport_1"
INSPECTOR = " Inspector"

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label, flush=True)
    if not condition:
        failures.append(label)
    return condition


def forward(path):
    return str(path).replace("\\", "/")


def near(a, b, tolerance=1.0):
    return a is not None and b is not None and len(a) == len(b) and all(abs(x - y) <= tolerance for x, y in zip(a, b))


def panels(area):
    return [w for w in area["visibleWindows"] if w["name"] not in IGNORED_WINDOWS]


def panel_names(area):
    return sorted(w["name"] for w in panels(area))


def find_panel(area, suffix):
    for w in panels(area):
        if w["name"].endswith(suffix):
            return w
    return None


def pixels(path):
    from PIL import Image
    image = Image.open(path)
    return image.size, image.convert("RGBA").tobytes()


def short(name):
    return name.split("###")[0].encode("ascii", "ignore").decode().strip() or name


# ---------------------------------------------------------------------------------------------
# Window title (Win32) of a process
# ---------------------------------------------------------------------------------------------

user32 = ctypes.windll.user32
EnumWindowsProc = ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)


def window_title(pid):
    titles = []

    def callback(hwnd, _):
        owner = wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, ctypes.byref(owner))
        if owner.value == pid and user32.IsWindowVisible(hwnd):
            length = user32.GetWindowTextLengthW(hwnd)
            buffer = ctypes.create_unicode_buffer(length + 1)
            user32.GetWindowTextW(hwnd, buffer, length + 1)
            if buffer.value:
                titles.append(buffer.value)
        return True

    user32.EnumWindows(EnumWindowsProc(callback), 0)
    return titles[0] if titles else ""


# ---------------------------------------------------------------------------------------------
# Glider sessions
# ---------------------------------------------------------------------------------------------

class Session:
    """A Glider with --control-pipe (GLIDER_DATA_DIR set, not isolated)."""

    def __init__(self, data_dir, model=MODEL, extra=()):
        args = [str(EXE), str(model)] + COMMON + VIEW + list(extra) + ["--control-pipe"]
        env = os.environ.copy()
        env["GLIDER_DATA_DIR"] = str(data_dir)
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
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
                if self.process.poll() is not None or time.time() > deadline + 30:
                    self.process.kill()
                    raise
                time.sleep(0.2)

    def call(self, cmd, **args):
        return self.pipe.call(cmd, **args)

    def request(self, cmd, **args):
        return self.pipe.request(cmd, args)

    def area(self):
        return self.call("get_state")["area"]

    def close(self, quit_app=True):
        gl_errors = None
        try:
            if quit_app:
                gl_errors = self.call("get_state")["glErrors"]
                self.call("quit")
        finally:
            self.pipe.close()
            try:
                self.process.wait(60)
            except Exception:
                self.process.kill()
                self.process.wait(30)
        return gl_errors


class Follow:
    """A Glider with --ui-script-follow (isolated): harness keys and mouse plus registry commands with replies."""

    def __init__(self, name, extra=()):
        OUT.mkdir(parents=True, exist_ok=True)
        self.script = OUT / f"{name}.follow"
        self.out = Path(str(self.script) + ".out.jsonl")
        self.script.write_text("", encoding="utf-8")
        if self.out.exists():
            self.out.unlink()
        self.data = Path(tempfile.mkdtemp(prefix="glider_area_follow_"))
        env = os.environ.copy()
        env["GLIDER_DATA_DIR"] = str(self.data)
        args = [str(EXE), str(MODEL)] + COMMON + VIEW + list(extra) + ["--ui-script-follow", str(self.script)]
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        self.lines = 0
        self.records = {}

    def push(self, *lines):
        with open(self.script, "a", encoding="utf-8") as f:
            for line in lines:
                f.write(line + "\n")
        self.lines += len(lines)

    def call(self, line, timeout=120):
        """Appends a registry command (or shot / snap) and returns its reply record."""
        self.push(line)
        wanted = self.lines
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.out.exists():
                for text in self.out.read_text(encoding="utf-8", errors="replace").splitlines():
                    try:
                        record = json.loads(text)
                    except json.JSONDecodeError:
                        continue
                    self.records[record.get("line")] = record
            if wanted in self.records:
                return self.records[wanted]
            if self.process.poll() is not None:
                raise RuntimeError(f"Glider exited ({self.process.returncode}) before line {wanted}: {line}")
            time.sleep(0.05)
        raise TimeoutError(f"no reply to line {wanted}: {line}")

    def result(self, line, timeout=120):
        record = self.call(line, timeout)
        if not record.get("ok"):
            raise RuntimeError(f"{line}: {record.get('error')}")
        return record["result"]

    def area(self):
        return self.result("get_state")["area"]

    def close(self):
        gl_errors = None
        try:
            if self.process.poll() is None:
                gl_errors = self.result("get_state")["glErrors"]
                self.push("quit")
                self.process.wait(60)
        except Exception:
            pass
        finally:
            if self.process.poll() is None:
                self.process.kill()
                self.process.wait(30)
            shutil.rmtree(self.data, ignore_errors=True)
        return gl_errors


def ui_run(name, lines, data_dir, isolated=False, window_size=True):
    """Runs Glider with a --ui-script (persistent state in data_dir unless isolated)."""
    OUT.mkdir(parents=True, exist_ok=True)
    script = OUT / f"{name}.script"
    report = OUT / f"{name}.report.json"
    log = OUT / f"{name}.log"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    env = os.environ.copy()
    env["GLIDER_DATA_DIR"] = str(data_dir)
    common = COMMON if window_size else COMMON[2:]
    args = [str(EXE), str(MODEL)] + common + (["--isolated"] if isolated else []) + VIEW + \
        ["--ui-script", str(script), "--ui-report", str(report)]
    result = subprocess.run(args, cwd=REPO, env=env, capture_output=True, text=True, encoding="utf-8",
                            errors="replace", timeout=300)
    log.write_text((result.stdout or "") + (result.stderr or ""), encoding="utf-8")
    return result.returncode, (result.stdout or "") + (result.stderr or "")


def layout_section(data):
    path = data / "settings.ini"
    if not path.exists():
        return None, ""
    text = path.read_text(encoding="utf-8")
    lines, inside = [], False
    for line in text.splitlines():
        if line.startswith("["):
            inside = line.strip() == "[Layout]"
            continue
        if inside and line.strip():
            lines.append(line)
    return ("\n".join(lines) if lines else None), text


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_pipe_maximize():
    print("[toggle_maximize_area / toggle_fullscreen over the pipe]")
    work = Path(tempfile.mkdtemp(prefix="glider_area_work_"))
    data = Path(tempfile.mkdtemp(prefix="glider_area_data_"))
    session = None
    try:
        session = Session(data)
        names = {c["name"] for c in session.call("describe")["commands"]}
        check({"toggle_maximize_area", "toggle_fullscreen", "open_dropped", "get_layout"} <= names,
              "describe lists toggle_maximize_area, toggle_fullscreen, open_dropped and get_layout")
        area0 = session.area()
        layout0 = session.call("get_layout")["live"]
        vp_window0 = find_panel(area0, VIEWPORT)
        state0 = session.call("get_state")
        vp0 = state0["viewports"][0]
        check(not area0["maximized"] and not area0["fullscreen"] and area0["menuBar"] and area0["statusBar"],
              "starts windowed with menu and status bar")
        check(len(panels(area0)) >= 4 and vp_window0 is not None,
              f"the default layout shows the viewport and the side panels ({[short(n) for n in panel_names(area0)]})")
        before = work / "before.png"
        cap0 = session.call("capture", path=str(before), settle_frames=6)
        rect0 = session.call("node_screen_rect", node="StaticSibling")["screenRect"]
        center0 = session.call("pick", x=int((rect0[0] + rect0[2]) / 2), y=int((rect0[1] + rect0[3]) / 2)) or {}
        scale = vp0["width"] / vp_window0["rect"][2]

        # A viewport.
        reply = session.call("toggle_maximize_area", target="viewport:1")
        dock = reply["dockRect"]
        check(reply["maximized"] and reply["target"].endswith(VIEWPORT), f"the viewport is maximized ({short(reply['target'] or '')})")
        check(near(reply["targetRect"], dock) and near(dock, area0["dockRect"]),
              f"it fills the dock area below the menu bar and above the status bar ({reply['targetRect']} = {dock})")
        check(reply["menuBar"] and reply["statusBar"], "menu bar and status bar stay")
        check([n for n in panel_names(reply)] == [vp_window0["name"]],
              f"every other panel is hidden ({[short(n) for n in panel_names(reply)]})")
        vp1 = session.call("get_state")["viewports"][0]
        check(abs(vp1["width"] - round(dock[2] * scale)) <= 1 and vp1["width"] > vp0["width"],
              f"the viewport renders at the new size ({vp0['width']}x{vp0['height']} -> {vp1['width']}x{vp1['height']})")
        check(vp1["height"] < round(dock[3] * scale) - 20,
              f"the maximized viewport keeps its toolbar ({vp1['height']} px image in a {dock[3]} px area)")
        maximized = REVIEW / "pipe_maximized_viewport.png"
        cap1 = session.call("capture", path=str(maximized), settle_frames=6)
        check((cap1["width"], cap1["height"]) == (vp1["width"], vp1["height"]) and pixels(maximized)[0] == (vp1["width"], vp1["height"]),
              f"the capture has the new size ({cap1['width']}x{cap1['height']})")
        rect1 = session.call("node_screen_rect", node="StaticSibling")["screenRect"]
        ratio0 = (rect0[2] - rect0[0]) / max(1e-6, rect0[3] - rect0[1])
        ratio1 = (rect1[2] - rect1[0]) / max(1e-6, rect1[3] - rect1[1])
        check(abs(ratio1 / ratio0 - 1.0) < 0.03 and abs((rect1[3] - rect1[1]) - (rect0[3] - rect0[1])) < 2.0,
              f"correct aspect: node rect ratio {ratio0:.3f} -> {ratio1:.3f}, same height (same window height)")
        center1 = session.call("pick", x=int((rect1[0] + rect1[2]) / 2), y=int((rect1[1] + rect1[3]) / 2)) or {}
        check(center1.get("node") is not None and center1.get("node") == center0.get("node"),
              f"picking at the node's new position finds it ({center1.get('node')})")
        reply = session.call("toggle_maximize_area", state=False)
        check(not reply["maximized"] and session.call("get_layout")["live"] == layout0,
              "restore: the docking layout ini text is identical to before")
        check(near(find_panel(reply, VIEWPORT)["rect"], vp_window0["rect"]) and
              [n for n in panel_names(reply)] == panel_names(area0), "restore: the same panels at the same rectangles")
        vp2 = session.call("get_state")["viewports"][0]
        after = work / "after.png"
        cap2 = session.call("capture", path=str(after), settle_frames=6)
        check((vp2["width"], vp2["height"]) == (vp0["width"], vp0["height"]), "restore: the viewport has its old size")
        check(pixels(before) == pixels(after) and cap2["rgbaHash"] == cap0["rgbaHash"],
              f"restore: the viewport capture is byte-identical ({cap0['rgbaHash']})")

        # A non-viewport panel.
        reply = session.call("toggle_maximize_area", target="inspector")
        check(reply["maximized"] and reply["target"].endswith(INSPECTOR) and near(reply["targetRect"], reply["dockRect"]),
              "the Inspector is maximized and fills the dock area")
        check([short(n) for n in panel_names(reply)] == ["Inspector"], "only the Inspector is shown (no viewport)")
        reply = session.call("toggle_maximize_area")
        check(not reply["maximized"] and session.call("get_layout")["live"] == layout0, "toggle again restores the layout")
        cap3 = session.call("capture", path=str(work / "after2.png"), settle_frames=6)
        check(cap3["rgbaHash"] == cap0["rgbaHash"], "the viewport image after the Inspector round trip is unchanged")

        # Full screen.
        reply = session.call("toggle_fullscreen", target="viewport:1")
        monitor = reply["monitorRect"]
        check(reply["fullscreen"] and reply["maximized"] and not reply["decorated"],
              "full screen: borderless, the area maximized")
        check(reply["windowRect"] == monitor, f"the window covers its monitor ({reply['windowRect']} = {monitor})")
        check(not reply["menuBar"] and not reply["statusBar"] and near(reply["dockRect"], [0, 0, monitor[2], monitor[3]]) and
              near(reply["targetRect"], reply["dockRect"]), "no menu or status bar, the viewport fills the screen")
        vpf = session.call("get_state")["viewports"][0]
        check(abs(vpf["width"] - round(monitor[2] * scale)) <= 1, f"the viewport renders at the screen width ({vpf['width']}x{vpf['height']})")
        check(abs(vpf["height"] - round(monitor[3] * scale)) <= 1,
              f"no viewport toolbar in full screen: the image is the screen height ({vpf['height']} = {monitor[3]} px)")
        session.call("capture", path=str(REVIEW / "pipe_fullscreen_viewport.png"), settle_frames=6)
        reply = session.call("toggle_fullscreen", state=False)
        check(not reply["fullscreen"] and not reply["maximized"] and reply["decorated"] and
              reply["windowRect"] == area0["windowRect"], f"leaving restores the window rectangle {reply['windowRect']}")
        check(session.call("get_layout")["live"] == layout0, "and the docking layout")
        cap4 = session.call("capture", path=str(work / "after3.png"), settle_frames=6)
        check(cap4["rgbaHash"] == cap0["rgbaHash"], "the viewport image after full screen is unchanged")

        # Maximized -> full screen -> back ends maximized, then restored.
        session.call("toggle_maximize_area", target="hierarchy")
        reply = session.call("toggle_fullscreen")
        check(reply["fullscreen"] and reply["fullscreenFromMaximized"] and reply["target"].endswith("Scene Hierarchy"),
              "full screen of the maximized Scene Hierarchy")
        reply = session.call("toggle_fullscreen")
        check(not reply["fullscreen"] and reply["maximized"] and reply["target"].endswith("Scene Hierarchy") and
              reply["windowRect"] == area0["windowRect"], "leaving full screen returns to the maximized area")
        reply = session.call("toggle_maximize_area")
        check(not reply["maximized"] and session.call("get_layout")["live"] == layout0, "then Ctrl+Space restores the layout")
        bad = session.request("toggle_maximize_area", target="viewport:7")
        check(not bad["ok"] and bad["error"]["code"] == "target_not_found", "an unknown target fails with target_not_found")
        gl_errors = session.close()
        session = None
        check(gl_errors == 0, f"no GL errors ({gl_errors})")
    finally:
        if session is not None:
            session.close(quit_app=False)
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def check_keys_round_trip():
    print("[Ctrl+Space round trip through the UI script (the whole section: --keys)]")
    live = Follow("keys_round_trip")
    try:
        live.push("wait 20")
        area0 = live.area()
        layout0 = live.result("get_layout")["live"]
        vp = find_panel(area0, VIEWPORT)
        check(vp is not None, "the viewport is on screen")
        vx, vy = vp["rect"][0] + vp["rect"][2] * 0.5, vp["rect"][1] + vp["rect"][3] * 0.5
        live.push(f"move {vx:.0f} {vy:.0f}", "wait 3", "key space ctrl", "wait 8")
        area = live.area()
        check(area["maximized"] and (area["target"] or "").endswith(VIEWPORT) and near(area["targetRect"], area["dockRect"]),
              "Ctrl+Space with the mouse over the viewport maximizes it")
        live.push("key space ctrl", "wait 8")
        area = live.area()
        check(not area["maximized"] and live.result("get_layout")["live"] == layout0,
              "Ctrl+Space again restores the exact layout")
        gl = live.close()
        check(gl == 0, f"no GL errors ({gl})")
    finally:
        live.close()


def check_keys_and_menus():
    print("[Ctrl+Space / Ctrl+Alt+Space / Esc and the menus through the UI script]")
    live = Follow("keys")
    try:
        live.push("wait 20")
        area0 = live.area()
        layout0 = live.result("get_layout")["live"]
        vp = find_panel(area0, VIEWPORT)
        inspector = find_panel(area0, INSPECTOR)
        check(vp is not None and inspector is not None, "viewport and Inspector are on screen")
        vx, vy = vp["rect"][0] + vp["rect"][2] * 0.5, vp["rect"][1] + vp["rect"][3] * 0.5
        ix, iy = inspector["rect"][0] + inspector["rect"][2] * 0.5, inspector["rect"][1] + inspector["rect"][3] * 0.5

        live.push(f"move {vx:.0f} {vy:.0f}", "wait 3", "key space ctrl", "wait 8")
        area = live.area()
        check(area["maximized"] and (area["target"] or "").endswith(VIEWPORT) and near(area["targetRect"], area["dockRect"]),
              "Ctrl+Space with the mouse over the viewport maximizes it")
        live.result(f"shot {forward(REVIEW / 'maximized_viewport.png')}")
        camera0 = live.result("get_camera")
        live.push("wait 2", "wheel 2", "wait 6")
        camera1 = live.result("get_camera")
        check(camera1["eye"] != camera0["eye"], "camera input (mouse wheel) works in the maximized viewport")
        live.push("key space ctrl", "wait 8")
        area = live.area()
        check(not area["maximized"] and live.result("get_layout")["live"] == layout0,
              "Ctrl+Space again restores the exact layout")

        live.push(f"move {ix:.0f} {iy:.0f}", "wait 3", "key space ctrl", "wait 8")
        area = live.area()
        check(area["maximized"] and (area["target"] or "").endswith(INSPECTOR) and
              [short(n) for n in panel_names(area)] == ["Inspector"],
              "Ctrl+Space over the Inspector maximizes it, the viewport and the other panels are hidden")
        live.result(f"shot {forward(REVIEW / 'maximized_inspector.png')}")
        live.push("key space ctrl", "wait 8")
        check(not live.area()["maximized"] and live.result("get_layout")["live"] == layout0, "and restores")

        live.push(f"move {vx:.0f} {vy:.0f}", "wait 3", "key space ctrl+alt", "wait 12")
        area = live.area()
        check(area["fullscreen"] and not area["decorated"] and area["windowRect"] == area["monitorRect"] and
              (area["target"] or "").endswith(VIEWPORT) and not area["menuBar"] and not area["statusBar"],
              f"Ctrl+Alt+Space: full screen of the viewport on its monitor ({area['monitorRect']})")
        live.result(f"shot {forward(REVIEW / 'fullscreen_viewport.png')}")
        live.push("key esc", "wait 12")
        area = live.area()
        check(not area["fullscreen"] and not area["maximized"] and area["decorated"] and
              area["windowRect"] == area0["windowRect"] and live.result("get_layout")["live"] == layout0,
              f"Esc leaves full screen: window rectangle {area['windowRect']} and layout restored")

        live.push(f"move {vx:.0f} {vy:.0f}", "wait 3", "key space ctrl", "wait 8", "key space ctrl+alt", "wait 12")
        area = live.area()
        check(area["fullscreen"] and area["fullscreenFromMaximized"], "Ctrl+Space then Ctrl+Alt+Space: full screen from maximized")
        live.push("key space ctrl+alt", "wait 12")
        area = live.area()
        check(not area["fullscreen"] and area["maximized"] and area["windowRect"] == area0["windowRect"],
              "Ctrl+Alt+Space returns to the maximized area")
        live.push("key space ctrl", "wait 8")
        check(not live.area()["maximized"] and live.result("get_layout")["live"] == layout0, "Ctrl+Space ends where it started")

        # Not while typing: the Scene Hierarchy filter field is active.
        live.push("clickp hier.search 0", "wait 4",
                  f"move {vx:.0f} {vy:.0f}", "key space ctrl", "wait 6", "key space ctrl+alt", "wait 6")
        area = live.area()
        check(not area["maximized"] and not area["fullscreen"], "no shortcut while a text input is active")
        live.push("key esc", "wait 4", f"click {vx:.0f} {vy:.0f}", "wait 6")
        check(not live.area()["fullscreen"], "Esc in a text field does nothing to the window")
        check(live.result("get_layout")["live"] == layout0, "the layout is unchanged")

        # Panel menu (acts on the last focused panel: the viewport just clicked).
        live.push("clickp menu.panel 0", "wait 6")
        live.result(f"shot {forward(REVIEW / 'panel_menu.png')}")
        live.push("clickp panel.maximize_area 0", "wait 8")
        area = live.area()
        check(area["maximized"] and (area["target"] or "").endswith(VIEWPORT), "Panel > Maximize Area maximizes the focused viewport")
        live.push("clickp menu.panel 0", "wait 6", "clickp panel.maximize_area 0", "wait 8")
        check(not live.area()["maximized"] and live.result("get_layout")["live"] == layout0, "Panel > Maximize Area again restores")

        # The viewport's own menu (Viewport menu body: main menu, viewport context menu, hierarchy row).
        live.push("clickp menu.viewport 0", "wait 6", "clickp viewport.maximize 0", "wait 8")
        area = live.area()
        check(area["maximized"] and (area["target"] or "").endswith(VIEWPORT), "Viewport > Maximize Viewport")
        live.push("clickp menu.viewport 0", "wait 6")
        live.result(f"shot {forward(REVIEW / 'viewport_menu_maximized.png')}")
        live.push("key esc", "wait 4", "clickp menu.viewport 0", "wait 6", "clickp viewport.fullscreen 0", "wait 12")
        area = live.area()
        check(area["fullscreen"] and area["fullscreenFromMaximized"] and (area["target"] or "").endswith(VIEWPORT),
              "Viewport > Full Screen from the maximized viewport")
        live.push("key esc", "wait 12")
        area = live.area()
        check(not area["fullscreen"] and area["maximized"], "Esc returns to the maximized viewport")
        live.push("clickp menu.viewport 0", "wait 6", "clickp viewport.maximize 0", "wait 8")
        check(not live.area()["maximized"] and live.result("get_layout")["live"] == layout0,
              "Viewport > Maximize Viewport (checked) restores the layout")
        gl = live.close()
        check(gl == 0, f"no GL errors ({gl})")
    finally:
        live.close()


def check_drops():
    print("[open_dropped: the drop callback path]")
    work = Path(tempfile.mkdtemp(prefix="glider_area_drop_"))
    data = Path(tempfile.mkdtemp(prefix="glider_area_data_"))
    session = None
    try:
        text = work / "notes.txt"
        text.write_text("not a model", encoding="utf-8")
        unicode_glb = work / "Glider_Ünïcødé_模型.glb"
        shutil.copyfile(GLB, unicode_glb)
        corrupt = work / "corrupt.glb"
        corrupt.write_bytes(b"glTF\x02\x00\x00\x00 this is not a binary glTF")
        session = Session(data)
        reply = session.call("open_dropped", paths=[str(GLB)])
        check(reply["opened"] == str(GLB) and reply["loaded"] and reply["state"]["model"] == str(GLB) and
              reply["state"]["nodeCount"] >= 1, f"a dropped .glb opens ({GLB.name}, {reply['state']['nodeCount']} nodes)")
        reply = session.call("open_dropped", paths=[str(MODEL)])
        check(reply["opened"] == str(MODEL) and reply["loaded"] and reply["state"]["model"] == str(MODEL),
              "a dropped .gltf opens")
        reply = session.call("open_dropped", paths=[str(text)])
        lines = [l["text"] for l in session.call("log_tail", lines=10)["lines"]]
        check(reply["opened"] is None and reply["ignored"] == [str(text)] and
              session.call("get_state")["model"] == str(MODEL), "a non-glTF file opens nothing (the model stays)")
        check(any("[Drop] Not a glTF/GLB file" in l and "notes.txt" in l for l in lines), "and logs a warning naming it")
        reply = session.call("open_dropped", paths=[str(text), str(GLB), str(MODEL)])
        lines = [l["text"] for l in session.call("log_tail", lines=10)["lines"]]
        check(reply["opened"] == str(GLB) and reply["ignored"] == [str(MODEL), str(text)] and reply["loaded"],
              "several files: the first glTF/GLB opens, the others are ignored")
        check(any("[Drop] Ignored 2 other dropped file(s)" in l for l in lines), "the ignored files are logged")
        session.call("set_persist", enabled=True)
        reply = session.call("open_dropped", paths=[str(unicode_glb)])
        check(reply["opened"] == str(unicode_glb) and reply["loaded"] and reply["state"]["model"] == str(unicode_glb),
              f"a Unicode file name opens ({unicode_glb.name})")
        title = window_title(session.process.pid)
        check(unicode_glb.name in title, f"the window title shows it ({title})")
        reply = session.call("open_dropped", paths=[str(corrupt)])
        state = session.call("get_state")
        lines = [l["text"] for l in session.call("log_tail", lines=20, level="error")["lines"]]
        check(reply["opened"] == str(corrupt) and not reply["loaded"] and lines, f"a corrupt .glb fails with an error ({lines[-1:]})")
        check(state["glErrors"] == 0 and session.call("open_dropped", paths=[str(GLB)])["loaded"],
              "Glider keeps running and opens the next drop")
        gl_errors = session.close()
        session = None
        check(gl_errors == 0, f"no GL errors ({gl_errors})")
        settings = (data / "settings.ini").read_text(encoding="utf-8") if (data / "settings.ini").exists() else ""
        check(forward(unicode_glb) in settings, "the dropped file is in [RecentFiles] (File > Open path, set_persist)")
    finally:
        if session is not None:
            session.close(quit_app=False)
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def check_settings():
    print("[settings.ini keeps the restored layout]")
    plain = Path(tempfile.mkdtemp(prefix="glider_area_plain_"))
    maxed = Path(tempfile.mkdtemp(prefix="glider_area_max_"))
    full = Path(tempfile.mkdtemp(prefix="glider_area_full_"))
    isolated = Path(tempfile.mkdtemp(prefix="glider_area_iso_"))
    try:
        # Ctrl+7 shows Statistics: a layout different from the default, so [Layout] is written.
        code_a, _ = ui_run("settings_plain", ["wait 12", "key 7 ctrl", "wait 12"], plain)
        code_b, _ = ui_run("settings_maximized", ["wait 12", "key 7 ctrl", "wait 12",
                                                  "toggle_maximize_area viewport:1", "wait 12"], maxed)
        layout_a, _ = layout_section(plain)
        layout_b, text_b = layout_section(maxed)
        check(code_a == 0 and code_b == 0, f"both runs exit cleanly ({code_a}, {code_b})")
        check(layout_a is not None and "Statistics" in layout_a, "the reference run stores its layout")
        same = layout_a == layout_b
        if not same and layout_a and layout_b:
            print("".join(list(difflib.unified_diff(layout_a.splitlines(True), layout_b.splitlines(True)))[:40]))
        check(same, "exit while maximized stores the restored layout (identical [Layout])")

        code_c, _ = ui_run("settings_fullscreen", ["wait 12", "toggle_fullscreen viewport:1", "wait 12"], full,
                           window_size=False)
        _, text_c = layout_section(full)
        check(code_c == 0, f"the full-screen run exits cleanly ({code_c})")
        check("[Window]" not in text_c and "[Layout]" not in text_c,
              "exit in full screen stores neither a full-screen window rectangle nor the maximized layout")

        code_d, _ = ui_run("settings_isolated", ["wait 12", "toggle_maximize_area viewport:1", "wait 8",
                                                 "toggle_fullscreen", "wait 12"], isolated, isolated=True)
        check(code_d == 0 and not any(isolated.iterdir()), "--isolated writes nothing")
        for name in ("settings_plain", "settings_maximized", "settings_fullscreen", "settings_isolated"):
            log = (OUT / f"{name}.log").read_text(encoding="utf-8", errors="replace")
            if "GL error 0x" in log:
                check(False, f"no GL errors in {name}")
    finally:
        for d in (plain, maxed, full, isolated):
            shutil.rmtree(d, ignore_errors=True)


def main():
    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    REVIEW.mkdir(parents=True, exist_ok=True)
    keys = "--keys" in sys.argv[1:]
    only = {arg for arg in sys.argv[1:] if not arg.startswith("--")}
    for name, fn in (("pipe", check_pipe_maximize), ("keys", check_keys_and_menus if keys else check_keys_round_trip),
                     ("drops", check_drops),
                     ("settings", check_settings)):
        if not only or name in only:
            fn()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    for failure in failures:
        print("  FAIL " + failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
