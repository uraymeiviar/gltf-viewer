# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Viewport image capture, save and copy (Viewport menu, save_viewport_image / copy_viewport_image).

One Glider process at a time. Every run points GLIDER_DATA_DIR at a temporary folder, so no user state is read
or written, and no check writes into the real Pictures folder: the capture folder is always set to a temporary
folder first, and the default-folder resolution is compared with SHGetKnownFolderPath(FOLDERID_Pictures)
through get_state without saving anything.

Checks:
  * get_state.captures: the default folder is <Pictures>\\Glider, an unset setting uses it; a configured folder
    in settings.ini is used instead;
  * save_viewport_image without a path writes Glider_<model>_<yyyyMMdd-HHmmss>[_n].png into the configured
    folder; three calls give three different files; the folder is created on demand;
  * the saved files, save_viewport_image(path) and the registry capture of the same frame hold identical pixels
    (and report the same rgbaHash);
  * copy_viewport_image puts CF_DIB and PNG on the clipboard; both decode to the capture's pixels (the previous
    clipboard text is restored);
  * an unusable capture folder fails with capture_failed and an error log line; Glider keeps running;
  * the three Viewport menu items (clicked through the UI script; the Save As dialog is answered through
    GLIDER_TEST_SAVE_FILE) produce the same pixels as capture, and the menu screenshot is stored;
  * the capture folder typed into File > Settings > Captures is stored in settings.ini ([Captures] folder),
    read back by the next start and removed by Reset to default;
  * --isolated reads and writes nothing, yet the in-memory folder still works for the session;
  * describe lists both commands; zero GL errors in every session.

Usage: python scripts/validate_viewport_image.py
Screenshots: build/review/viewport-image/.
"""

import ctypes
import json
import os
import re
import shutil
import struct
import sys
import tempfile
import time
from ctypes import wintypes
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import harness_process as subprocess  # noqa: E402
import glider_ctl  # noqa: E402

REPO = Path(__file__).resolve().parents[1]
EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
OUT = REPO / "build" / "viewport_image"
REVIEW = REPO / "build" / "review" / "viewport-image"
MODEL = REPO / "resources" / "test_models" / "hierarchy_animation_fixture.gltf"
COMMON = ["--window-size", "1600x900", "--aa", "none", "--hide-grid", "--fixed-frame-time", "0.05", "--vsync", "off"]
VIEW = ["--clear-color", "0.1,0.2,0.3", "--bloom", "off", "--auto-exposure", "off", "--tone-mapping", "neutral",
        "--camera-target", "0,0,0", "--camera-yaw", "0", "--camera-pitch", "0", "--camera-distance", "7"]
SETTINGS_MARKER = "; Glider settings file (schema 1)\n\n"
NAME_PATTERN = re.compile(r"^Glider_hierarchy_animation_fixture_\d{8}-\d{6}(_\d+)?\.png$")

failures = []


def check(condition, label):
    print(("  PASS " if condition else "  FAIL ") + label, flush=True)
    if not condition:
        failures.append(label)
    return condition


def forward(path):
    return str(path).replace("\\", "/")


def pixels(path):
    from PIL import Image
    return Image.open(path).convert("RGBA").tobytes()


def same_pixels(a, b):
    return Path(a).exists() and Path(b).exists() and pixels(a) == pixels(b)


def pictures_folder():
    """FOLDERID_Pictures through SHGetKnownFolderPath (the reference for the default capture folder)."""
    class GUID(ctypes.Structure):
        _fields_ = [("a", wintypes.DWORD), ("b", wintypes.WORD), ("c", wintypes.WORD), ("d", ctypes.c_ubyte * 8)]
    guid = GUID(0x33E28130, 0x4E1E, 0x4676, (ctypes.c_ubyte * 8)(0x83, 0x5A, 0x98, 0x39, 0x5C, 0x3B, 0xC3, 0xBB))
    shell32 = ctypes.windll.shell32
    shell32.SHGetKnownFolderPath.argtypes = [ctypes.POINTER(GUID), wintypes.DWORD, wintypes.HANDLE,
                                             ctypes.POINTER(ctypes.c_wchar_p)]
    result = ctypes.c_wchar_p()
    code = shell32.SHGetKnownFolderPath(ctypes.byref(guid), 0, None, ctypes.byref(result))
    value = result.value
    ctypes.windll.ole32.CoTaskMemFree(ctypes.cast(result, ctypes.c_void_p))
    return value if code == 0 else None


# ---------------------------------------------------------------------------------------------
# Clipboard (ctypes)
# ---------------------------------------------------------------------------------------------

user32 = ctypes.windll.user32
kernel32 = ctypes.windll.kernel32
user32.OpenClipboard.argtypes = [wintypes.HWND]
user32.GetClipboardData.argtypes = [wintypes.UINT]
user32.GetClipboardData.restype = wintypes.HANDLE
user32.SetClipboardData.argtypes = [wintypes.UINT, wintypes.HANDLE]
user32.SetClipboardData.restype = wintypes.HANDLE
user32.RegisterClipboardFormatW.argtypes = [wintypes.LPCWSTR]
user32.IsClipboardFormatAvailable.argtypes = [wintypes.UINT]
kernel32.GlobalLock.argtypes = [wintypes.HANDLE]
kernel32.GlobalLock.restype = ctypes.c_void_p
kernel32.GlobalUnlock.argtypes = [wintypes.HANDLE]
kernel32.GlobalSize.argtypes = [wintypes.HANDLE]
kernel32.GlobalSize.restype = ctypes.c_size_t
kernel32.GlobalAlloc.argtypes = [wintypes.UINT, ctypes.c_size_t]
kernel32.GlobalAlloc.restype = wintypes.HANDLE
CF_UNICODETEXT = 13
CF_DIB = 8


def open_clipboard():
    for _ in range(50):
        if user32.OpenClipboard(None):
            return True
        time.sleep(0.05)
    return False


def clipboard_bytes(fmt):
    handle = user32.GetClipboardData(fmt)
    if not handle:
        return None
    pointer = kernel32.GlobalLock(handle)
    if not pointer:
        return None
    try:
        return ctypes.string_at(pointer, kernel32.GlobalSize(handle))
    finally:
        kernel32.GlobalUnlock(handle)


def read_clipboard():
    """CF_DIB, PNG and text of the clipboard, or None when it cannot be opened."""
    if not open_clipboard():
        return None
    try:
        png_format = user32.RegisterClipboardFormatW("PNG")
        text = clipboard_bytes(CF_UNICODETEXT)
        if text is not None:
            text = text.decode("utf-16-le", "replace").split("\x00")[0]
        return {"dib": clipboard_bytes(CF_DIB), "png": clipboard_bytes(png_format), "text": text}
    finally:
        user32.CloseClipboard()


def restore_clipboard(text):
    if not open_clipboard():
        return
    try:
        user32.EmptyClipboard()
        if text is not None:
            data = (text + "\x00").encode("utf-16-le")
            handle = kernel32.GlobalAlloc(0x0002, len(data))  # GMEM_MOVEABLE
            pointer = kernel32.GlobalLock(handle)
            ctypes.memmove(pointer, data, len(data))
            kernel32.GlobalUnlock(handle)
            user32.SetClipboardData(CF_UNICODETEXT, handle)
    finally:
        user32.CloseClipboard()


def dib_to_rgb(dib):
    """Decodes a bottom-up (positive height) BI_RGB 24/32-bit DIB to (width, height, top-down RGB bytes)."""
    from PIL import Image
    size, width, height, planes, bits, compression = struct.unpack_from("<IiiHHI", dib, 0)
    if size < 40 or compression != 0 or bits not in (24, 32) or height <= 0:
        return None
    stride = ((width * bits + 31) // 32) * 4
    image = Image.frombuffer("RGB", (width, height), dib[size:size + stride * height], "raw",
                             "BGR" if bits == 24 else "BGRX", stride, -1)
    return width, height, image.tobytes()


# ---------------------------------------------------------------------------------------------
# Glider runs
# ---------------------------------------------------------------------------------------------

def make_data_dir(folder=None):
    data = Path(tempfile.mkdtemp(prefix="glider_vi_"))
    if folder is not None:
        (data / "settings.ini").write_text(SETTINGS_MARKER + f"[Captures]\nfolder={forward(folder)}\n", encoding="utf-8")
    return data


class Session:
    """A Glider with --control-pipe and a connected client (not isolated; GLIDER_DATA_DIR set)."""

    def __init__(self, data_dir, isolated=False):
        args = [str(EXE), str(MODEL)] + COMMON + (["--isolated"] if isolated else []) + VIEW + ["--control-pipe"]
        env = os.environ.copy()
        env["GLIDER_DATA_DIR"] = str(data_dir)
        self.process = subprocess.Popen(args, cwd=REPO, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                                        stderr=subprocess.DEVNULL)
        deadline = time.time() + 120
        while not os.path.exists(glider_ctl.pipe_name(self.process.pid)):
            if self.process.poll() is not None or time.time() > deadline:
                raise RuntimeError(f"Glider did not open its pipe (exit {self.process.poll()})")
            time.sleep(0.1)
        # The pipe name can exist a moment before the server accepts: retry the connection.
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
        return gl_errors


def ui_run(name, lines, data_dir, env_extra=None, isolated=False, extra=()):
    """Runs Glider with a --ui-script; returns the report path."""
    OUT.mkdir(parents=True, exist_ok=True)
    script = OUT / f"{name}.script"
    report = OUT / f"{name}.report.json"
    log = OUT / f"{name}.log"
    script.write_text("\n".join(lines) + "\n", encoding="utf-8")
    env = os.environ.copy()
    env["GLIDER_DATA_DIR"] = str(data_dir)
    env.update(env_extra or {})
    args = [str(EXE), str(MODEL)] + COMMON + (["--isolated"] if isolated else []) + VIEW + list(extra) + \
        ["--ui-script", str(script), "--ui-report", str(report)]
    result = subprocess.run(args, cwd=REPO, env=env, capture_output=True, text=True, timeout=300)
    log.write_text((result.stdout or "") + (result.stderr or ""), encoding="utf-8")
    if result.returncode != 0:
        print((result.stdout or "")[-1500:], (result.stderr or "")[-1500:])
    return report, result.returncode


# ---------------------------------------------------------------------------------------------
# Checks
# ---------------------------------------------------------------------------------------------

def check_commands_and_pixels():
    print("[save_viewport_image / copy_viewport_image over the pipe]")
    work = Path(tempfile.mkdtemp(prefix="glider_vi_work_"))
    caps = work / "caps" / "nested"  # created on demand
    data = make_data_dir(caps)
    previous = read_clipboard()
    session = None
    try:
        session = Session(data)
        names = {c["name"] for c in session.call("describe")["commands"]}
        check({"save_viewport_image", "copy_viewport_image"} <= names, "describe lists both commands")
        folders = session.call("get_state")["captures"]
        check(Path(folders["configured"]) == caps and Path(folders["effective"]) == caps,
              f"settings.ini [Captures] folder is the effective folder ({folders['effective']})")
        check(not caps.exists(), "the capture folder is not created before the first capture")

        replies = [session.call("save_viewport_image") for _ in range(3)]
        paths = [Path(r["path"]) for r in replies]
        check(len({str(p).lower() for p in paths}) == 3 and all(p.exists() for p in paths),
              f"three automatic captures give three different files ({[p.name for p in paths]})")
        check(all(p.parent == caps and NAME_PATTERN.match(p.name) for p in paths),
              "names are Glider_<model stem>_<yyyyMMdd-HHmmss>[_n].png in the capture folder")
        seconds = [re.search(r"\d{8}-\d{6}", p.name).group(0) for p in paths]
        check(len(set(seconds)) == 3 or any(re.search(r"_\d+$", p.stem) for p in paths),
              "captures within the same second get a _2, _3 suffix")

        ref = work / "ref.png"
        capture = session.call("capture", path=str(ref), settle_frames=1)
        explicit = work / "explicit.png"
        saved = session.call("save_viewport_image", path=str(explicit))
        after = session.call("save_viewport_image")
        check(Path(saved["path"]) == explicit and explicit.exists(), "save_viewport_image(path) writes that file")
        check(all(same_pixels(p, ref) for p in paths) and same_pixels(explicit, ref) and same_pixels(after["path"], ref),
              "saved files and the registry capture hold identical pixels")
        check(capture["rgbaHash"] == saved["rgbaHash"] == after["rgbaHash"] == replies[0]["rgbaHash"] and
              (saved["width"], saved["height"]) == (capture["width"], capture["height"]),
              f"rgbaHash and size match capture ({capture['rgbaHash']}, {capture['width']}x{capture['height']})")

        lines = [l["text"] for l in session.call("log_tail", lines=40)["lines"]]
        check(any("Viewport image saved: " + replies[0]["path"] in l for l in lines),
              "a log line names the full path of the capture")

        copied = session.call("copy_viewport_image")
        board = read_clipboard()
        check(board is not None and board["dib"] is not None and board["png"] is not None,
              "the clipboard holds CF_DIB and PNG")
        if board and board["dib"] and board["png"]:
            from PIL import Image
            import io
            decoded = dib_to_rgb(board["dib"])
            reference_rgb = Image.open(ref).convert("RGB")
            check(decoded is not None and decoded[:2] == reference_rgb.size and decoded[2] == reference_rgb.tobytes(),
                  "CF_DIB pixels (orientation, colour order) equal the capture")
            png = Image.open(io.BytesIO(board["png"])).convert("RGBA")
            check(png.tobytes() == pixels(ref), "PNG clipboard format equals the capture")
        check(copied["rgbaHash"] == capture["rgbaHash"], "copy_viewport_image reports the capture's rgbaHash")
        check(any("copied to the clipboard" in l["text"] for l in session.call("log_tail", lines=10)["lines"]),
              "a log line reports the clipboard copy")
        gl_errors = session.close()
        session = None
        check(gl_errors == 0, f"no GL errors ({gl_errors})")
        check(not (data / "GliderData").exists(), "no GliderData folder was created")
    finally:
        if session is not None:
            session.close(quit_app=False)
        if previous is not None:
            restore_clipboard(previous["text"])
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def check_default_folder_and_errors():
    print("[default folder resolution and an unusable folder]")
    pictures = pictures_folder()
    data = make_data_dir()  # no [Captures]: the default folder
    session = None
    try:
        session = Session(data)
        folders = session.call("get_state")["captures"]
        expected = str(Path(pictures) / "Glider") if pictures else None
        check(expected is not None and os.path.normcase(folders["default"]) == os.path.normcase(expected),
              f"default capture folder is <Pictures>\\Glider ({folders['default']})")
        check(folders["configured"] == "" and folders["effective"] == folders["default"],
              "an unset folder falls back to the default")
        check(session.call("get_state")["glErrors"] == 0, "no GL errors")
        session.close()
        session = None
    finally:
        if session is not None:
            session.close(quit_app=False)
        shutil.rmtree(data, ignore_errors=True)

    work = Path(tempfile.mkdtemp(prefix="glider_vi_work_"))
    blocker = work / "blocker.txt"
    blocker.write_text("a file, not a folder", encoding="utf-8")
    data = make_data_dir(blocker / "sub")
    try:
        session = Session(data)
        reply = session.request("save_viewport_image")
        check(not reply["ok"] and reply["error"]["code"] == "capture_failed",
              f"an unusable folder fails with capture_failed ({reply.get('error')})")
        entries = session.call("log_tail", lines=10, level="error")["lines"]
        check(any("cannot create the capture folder" in e["text"] for e in entries), "an error line is logged")
        check(session.call("get_state")["glErrors"] == 0, "Glider keeps running, no GL errors")
        session.close()
        session = None
        check(not (blocker / "sub").exists() and blocker.read_text(encoding="utf-8") == "a file, not a folder",
              "nothing was written")
    finally:
        if session is not None:
            session.close(quit_app=False)
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def check_menu_items():
    print("[Viewport menu items through the UI script]")
    REVIEW.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="glider_vi_work_"))
    caps = work / "caps"
    data = make_data_dir(caps)
    saved = work / "save_as.png"
    ref = work / "ref.png"
    previous = read_clipboard()
    try:
        script = [
            "wait 10",
            "clickp menu.viewport 0", "wait 6",
            f"shot {forward(REVIEW / 'viewport_menu.png')}",
            "clickp viewport.save_view 0", "wait 8",
            "clickp menu.viewport 0", "wait 6",
            "clickp viewport.capture 0", "wait 8",
            "clickp menu.viewport 0", "wait 6",
            "clickp viewport.copy_view 0", "wait 8",
            f"capture {forward(ref)} 1",
            "wait 4",
        ]
        report, code = ui_run("menu_items", script, data, env_extra={"GLIDER_TEST_SAVE_FILE": str(saved)})
        check(code == 0, f"UI script run exits cleanly ({code})")
        auto = sorted(caps.glob("*.png")) if caps.exists() else []
        check(len(auto) == 1 and NAME_PATTERN.match(auto[0].name), f"Capture Viewport wrote one automatic file ({[p.name for p in auto]})")
        check(saved.exists(), "Save Viewport View... wrote the chosen file")
        check(ref.exists() and auto and saved.exists() and same_pixels(auto[0], ref) and same_pixels(saved, ref),
              "menu captures equal the registry capture pixel for pixel")
        board = read_clipboard()
        if board and board["dib"] and ref.exists():
            from PIL import Image
            decoded = dib_to_rgb(board["dib"])
            check(decoded is not None and decoded[2] == Image.open(ref).convert("RGB").tobytes(),
                  "Copy Viewport View put the same pixels on the clipboard")
        else:
            check(False, "Copy Viewport View put a bitmap on the clipboard")
        check((REVIEW / "viewport_menu.png").exists(), "menu screenshot stored")
        check(no_gl_errors("menu_items"), "no GL errors in the menu run")
    finally:
        if previous is not None:
            restore_clipboard(previous["text"])
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def no_gl_errors(name):
    log = OUT / f"{name}.log"
    return log.exists() and "GL error 0x" not in log.read_text(encoding="utf-8", errors="replace")


def settings_text(data):
    path = data / "settings.ini"
    return path.read_text(encoding="utf-8") if path.exists() else ""


def check_settings_round_trip():
    print("[File > Settings > Captures: settings.ini round trip]")
    REVIEW.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="glider_vi_work_"))
    folder = work / "my captures"
    data = make_data_dir()
    open_settings = ["--show-search-paths"]
    try:
        type_folder = ["wait 10", "clickp settings.tab.captures 0", "wait 6",
                       f"shot {forward(REVIEW / 'settings_captures_default.png')}",
                       "clickp settings.captures.folder 0", "wait 4", f"type {forward(folder)}", "wait 4",
                       "clickp settings.tab.captures 0", "wait 6", "wait 4"]
        ui_run("settings_type", type_folder, data, extra=open_settings)
        text = settings_text(data)
        check("[Captures]" in text and f"folder={forward(folder)}" in text,
              f"the typed folder is stored as [Captures] folder ({text.strip().splitlines()[-1:] or text!r})")

        session = Session(data)
        try:
            folders = session.call("get_state")["captures"]
            check(Path(folders["configured"]) == folder and Path(folders["effective"]) == folder,
                  "the next start reads the stored folder")
            saved = session.call("save_viewport_image")
            check(Path(saved["path"]).parent == folder and Path(saved["path"]).exists(),
                  "the stored folder is used and created on demand")
            session.close()
        except Exception:
            session.close(quit_app=False)
            raise

        ui_run("settings_reset",
               ["wait 10", "clickp settings.tab.captures 0", "wait 6",
                f"shot {forward(REVIEW / 'settings_captures.png')}",
                "clickp settings.captures.reset 0", "wait 6", "wait 4"], data, extra=open_settings)
        check("[Captures]" not in settings_text(data) and "folder=" not in settings_text(data),
              "Reset to default removes the stored folder")
        check((REVIEW / "settings_captures.png").exists(), "Settings > Captures screenshot stored")
    finally:
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def check_isolated():
    print("[--isolated reads and writes nothing]")
    work = Path(tempfile.mkdtemp(prefix="glider_vi_work_"))
    ignored = work / "ignored"
    typed = work / "typed"
    data = make_data_dir(ignored)  # --isolated must not read this
    before = settings_text(data)
    try:
        script = ["wait 10", "clickp settings.tab.captures 0", "wait 6", "clickp settings.captures.folder 0", "wait 4",
                  f"type {forward(typed)}", "wait 4", "clickp settings.tab.captures 0", "wait 6",
                  "save_viewport_image", "wait 8"]
        ui_run("isolated", script, data, isolated=True, extra=["--show-search-paths"])
        files = sorted(typed.glob("*.png")) if typed.exists() else []
        check(len(files) == 1, f"the in-memory folder is used for the session ({[f.name for f in files]})")
        check(not ignored.exists(), "the stored folder was not read")
        check(no_gl_errors("isolated"), "no GL errors in the isolated run")
        check(settings_text(data) == before and sorted(p.name for p in data.iterdir()) == ["settings.ini"],
              "settings.ini was not touched and nothing else was written")
    finally:
        shutil.rmtree(data, ignore_errors=True)
        shutil.rmtree(work, ignore_errors=True)


def main():
    if not EXE.exists():
        sys.exit(f"no build at {EXE}")
    OUT.mkdir(parents=True, exist_ok=True)
    check_commands_and_pixels()
    check_default_folder_and_errors()
    check_menu_items()
    check_settings_round_trip()
    check_isolated()
    print("=" * 60)
    print("RESULT: " + ("PASS" if not failures else f"FAIL ({len(failures)})"))
    for failure in failures:
        print("  FAIL " + failure)
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()
