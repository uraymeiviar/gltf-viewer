# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Starts Glider harness children without requesting foreground keyboard focus, and caches baseline renders.

Harnesses import this module as ``import harness_process as subprocess``. Its ``run()``
and ``Popen()`` add ``GLIDER_NO_FOCUS=1`` to every child environment and, on Windows,
start Glider with ``STARTF_USESHOWWINDOW`` / ``SW_SHOWNOACTIVATE``. Current builds also
read that variable (GLFW ``GLFW_FOCUSED`` and ``GLFW_FOCUS_ON_SHOW`` false), so their
window is still shown and rendered normally but never activated. Old baseline binaries
do not know the variable; they are still shown with ``SW_SHOWNOACTIVATE``.

Baseline render cache
---------------------
A binary under ``build/baselines/`` never changes for a given hash, so ``run()`` caches
what a baseline launch wrote. It applies to every executable under ``build/baselines/``
(the HEAD baseline passed by run_regression.py and the pinned references such as 167a5e0
and a9c0a29), never to the current build (``build/bin/Release/Glider.exe``) or any other
executable, and never to ``Popen()`` (pipe sessions). The key (SHA-256) covers:

  * the SHA-256 of the executable;
  * the working directory and the exact argument list, in order;
  * the content (SHA-256) and resolved path of every argument that names an existing
    file, and for each .gltf/.glb among them every file it names through a ``uri``
    anywhere in its JSON (buffers, images, extensions);
  * the child environment: every ``GLIDER_*`` variable (except this module's own
    ``GLIDER_HARNESS_*`` / ``GLIDER_RENDER_CACHE``), ``UI_SCENARIO_TRACE``, graphics driver
    variables (``__GL*``, ``MESA*``, ...) and every variable the caller set to a value that
    differs from this process's environment;
  * the machine: display adapter names and driver versions, system DPI and screen sizes;
  * how stdout/stderr are captured (text mode, encoding).

The value is the files the command wrote (dump/capture image, report JSON), its stdout
and stderr, stored under ``build/regression_cache/<key>/``. A hit copies the files back
and returns the stored output with ``from_cache = True`` instead of starting the binary.
Any change of a key part is a miss. The cache is used only when everything is understood:
``--isolated``, at least one known output flag, no other output-writing or interactive
flag, no directory argument (``--search-path``), every model URI resolving to a file, and
stdout/stderr captured or discarded. Anything else starts the binary normally. A result is
stored only when the process exited with 0, every output was rewritten, and neither the
executable nor an input changed while it ran.

``GLIDER_RENDER_CACHE=0`` turns the cache off (cold runs). ``prune_cache()`` removes entries
whose executable is gone or changed. ``GLIDER_HARNESS_LAUNCH_LOG=<file>`` appends one JSON
line per Glider launch (role current/baseline/other, run or popen, cache hit/miss/off and
why, seconds, argv); run_regression.py sets it per suite for its launch summary.
"""

import functools
import hashlib
import json
import os
import re
import shutil
import subprocess as _subprocess
import sys
import threading
import time
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
CURRENT_EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
BASELINE_DIR = REPO / "build" / "baselines"
CACHE_DIR = REPO / "build" / "regression_cache"
CACHE_VERSION = 2
GLIDER_NO_FOCUS = "GLIDER_NO_FOCUS"
CACHE_SWITCH = "GLIDER_RENDER_CACHE"
LAUNCH_LOG = "GLIDER_HARNESS_LAUNCH_LOG"
SW_SHOWNOACTIVATE = 4


def __getattr__(name):
    """Keep the rest of the subprocess API (PIPE, STDOUT, TimeoutExpired, ...) available."""
    return getattr(_subprocess, name)


def _child_env(env=None):
    result = dict(os.environ if env is None else env)
    result[GLIDER_NO_FOCUS] = "1"
    return result


def _command_list(args):
    if isinstance(args, (str, bytes, os.PathLike)):
        return None
    try:
        return [str(arg) for arg in args]
    except TypeError:
        return None


def _resolve(value, cwd):
    path = Path(value)
    if not path.is_absolute() and cwd is not None:
        path = Path(cwd) / path
    try:
        return path.resolve()
    except OSError:
        return path


def _is_glider_exe(path):
    if path.suffix.lower() != ".exe":
        return False
    name = path.name.lower()
    stem = path.stem.lower()
    if (name.startswith("glider") or path.parent.name.lower() == "baselines"
            or (len(stem) == 7 and all(char in "0123456789abcdef" for char in stem))):
        return True
    try:
        path.relative_to(REPO)
        return True
    except ValueError:
        return False


def _glider_target(command, cwd):
    """The Glider executable a command ultimately starts, or None."""
    first = Path(command[0])
    if first.name.lower() in ("cmd", "cmd.exe"):
        for index, arg in enumerate(command[1:], 1):
            if arg.lower() in ("/c", "/k") and index + 1 < len(command):
                target = _resolve(command[index + 1], cwd)
                return target if _is_glider_exe(target) else None
        return None
    target = _resolve(command[0], cwd)
    return target if _is_glider_exe(target) else None


def _inside(path, folder):
    try:
        Path(path).resolve().relative_to(Path(folder).resolve())
        return True
    except (OSError, ValueError):
        return False


def role(path):
    """'current' for build/bin/Release/Glider.exe, 'baseline' under build/baselines/, else 'other'."""
    try:
        if Path(path).resolve() == CURRENT_EXE.resolve():
            return "current"
    except OSError:
        pass
    return "baseline" if _inside(path, BASELINE_DIR) else "other"


@functools.lru_cache(maxsize=None)
def _has_no_focus_marker(path, mtime_ns, size):
    del mtime_ns, size
    try:
        return b"GLIDER_NO_FOCUS" in Path(path).read_bytes()
    except OSError:
        # The current build is always readable; if it is not, leave it on the normal
        # desktop rather than breaking harnesses that inspect its window.
        return True


def _supports_no_focus(target):
    try:
        stat = target.stat()
    except OSError:
        return True
    return _has_no_focus_marker(str(target), stat.st_mtime_ns, stat.st_size)


def supports_no_focus(path):
    """True when an existing executable contains the GLIDER_NO_FOCUS marker."""
    path = Path(path)
    try:
        return path.is_file() and _supports_no_focus(path)
    except OSError:
        return False


def _apply_no_activate(kwargs):
    if os.name != "nt":
        return
    startupinfo = kwargs.get("startupinfo")
    startupinfo = startupinfo.copy() if startupinfo is not None else _subprocess.STARTUPINFO()
    startupinfo.dwFlags |= _subprocess.STARTF_USESHOWWINDOW
    startupinfo.wShowWindow = SW_SHOWNOACTIVATE
    kwargs["startupinfo"] = startupinfo


def _prepare(command, kwargs):
    kwargs = dict(kwargs)
    kwargs["env"] = _child_env(kwargs.get("env"))
    target = _glider_target(command, kwargs.get("cwd"))
    if target is not None:
        _apply_no_activate(kwargs)
    return target, kwargs


# ----------------------------------------------------------------------------- launch log

_log_lock = threading.Lock()


def log_launch(target, kind, cache, seconds, command):
    """Appends one JSON line to $GLIDER_HARNESS_LAUNCH_LOG (no-op when unset)."""
    path = os.environ.get(LAUNCH_LOG)
    if not path:
        return
    record = {"exe": Path(target).name, "role": role(target), "kind": kind, "cache": cache,
              "seconds": round(seconds, 3), "argv": [str(arg) for arg in command[1:]]}
    try:
        with _log_lock, open(path, "a", encoding="utf-8") as stream:
            stream.write(json.dumps(record) + "\n")
    except OSError:
        pass


def _run(command, popenargs, kwargs):
    target, kwargs = _prepare(command, kwargs)
    if target is None:
        return _subprocess.run(*popenargs, **kwargs)
    start = time.time()
    plan, reason = None, "not a baseline"
    try:
        plan, reason = _cache_plan(command, target, kwargs)
    except Exception as error:  # noqa: BLE001 - caching must never break a harness
        plan, reason = None, f"error {type(error).__name__}"
    if plan is not None:
        cached = _cache_apply(plan, command, kwargs)
        if cached is not None:
            log_launch(target, "run", "hit", time.time() - start, command)
            return cached
    result = _subprocess.run(*popenargs, **kwargs)
    status = f"off: {reason}"
    if plan is not None:
        status = "miss"
        try:
            if _cache_write(plan, result, kwargs):
                status = "miss, stored"
        except Exception:  # noqa: BLE001 - caching must never break a harness
            pass
    log_launch(target, "run", status, time.time() - start, command)
    return result


def run(*popenargs, **kwargs):
    if not popenargs and "args" in kwargs:
        popenargs = (kwargs.pop("args"),)
    command = _command_list(popenargs[0] if popenargs else None)
    if command is None:
        kwargs = dict(kwargs)
        kwargs["env"] = _child_env(kwargs.get("env"))
        return _subprocess.run(*popenargs, **kwargs)
    return _run(command, popenargs, kwargs)


def popen(command, **kwargs):
    target, kwargs = _prepare(command, kwargs)
    if target is not None:
        log_launch(target, "popen", "off: popen", 0.0, command)
    return _subprocess.Popen(command, **kwargs)


def Popen(*popenargs, **kwargs):  # noqa: N802 - mirrors subprocess.Popen
    if not popenargs and "args" in kwargs:
        popenargs = (kwargs.pop("args"),)
    command = _command_list(popenargs[0] if popenargs else None)
    if command is None:
        kwargs = dict(kwargs)
        kwargs["env"] = _child_env(kwargs.get("env"))
        return _subprocess.Popen(*popenargs, **kwargs)
    return popen(command, **kwargs)


# ----------------------------------------------------------------------------- render cache

# Flags whose value is a file the command writes. Only these are cache outputs.
_OUTPUT_FLAGS = {
    "--dump-frame", "--dump-next-frame", "--capture-viewport", "--capture-first-frame",
    "--profile-report", "--asobo-report", "--decal-prepass-check", "--validation-report",
    "--scenario-report",
}
# Flags that write files the cache does not model (timing output, UI scripts and their shot
# files, loading series, self-check reports) or that make the run interactive.
_UNSUPPORTED_FLAGS = {
    "--benchmark", "--benchmark-output", "--benchmark-warmup", "--benchmark-frames",
    "--exit-after-benchmark", "--idle-stats", "--capture-loading",
    "--ui-script-follow", "--control-pipe", "--agent-guide",
    "--search-path",
}
# --ui-script: the script file is an input; its "shot <path>" and "catch_realloc <vp> <shot> <snap>"
# lines name outputs and its "reload"/"reloadwithouttextures <path>" lines name inputs
# (source/imgui/UiScenarioHarness.cpp). Scripts with any other command, or that touch the
# clipboard, are not cached.
_UI_SCRIPT_COMMANDS = {
    "aamode", "animplay", "append", "asoboparam", "catch_realloc", "click", "clickarrowp", "clickarrowrow",
    "clickp", "clickrow", "dclick", "dclickp", "dclickrow", "drag", "hover", "hoverp", "hoverrow", "move",
    "picknode", "rclick", "rclickp", "reload", "reloadwithouttextures", "resize", "scroll", "shot", "snap",
    "vpcreate", "vpfocus", "vpsetting", "vpsplit", "vptoggle", "wait", "waitms", "wheel",
}
# Any other flag whose name suggests it writes a file or talks to something is refused too,
# so a flag added later is not mistaken for a plain setting. These are known settings.
_OUTPUT_LIKE = re.compile(r"report|output|dump|capture|selfcheck|script|stats|pattern|benchmark|guide|"
                          r"follow|pipe|loading|export|save|write|log-file")
_SAFE_OUTPUT_LIKE = {"--capture-frame", "--capture-time"}
# Environment variables Glider (or the GL driver) reads; their values are part of the key.
_ENV_NAMES = {"UI_SCENARIO_TRACE"}
_ENV_PREFIXES = ("GLIDER_", "__GL", "MESA", "LIBGL", "GALLIUM", "GLFW", "DXVK", "VK_", "OPENGL", "NVIDIA", "AMD_")
_ENV_IGNORED_PREFIXES = ("GLIDER_HARNESS_",)
_ENV_IGNORED = {CACHE_SWITCH}


def cache_enabled():
    return os.environ.get(CACHE_SWITCH, "1").strip() not in ("0", "off", "false", "no")


def _cacheable_target(target):
    if role(target) != "baseline":
        return False
    try:
        return target.is_file()
    except OSError:
        return False


def _stream_mode(kwargs):
    """How stdout/stderr come back; None when the cache cannot reproduce it."""
    if kwargs.get("input") is not None:
        return None
    stdin = kwargs.get("stdin")
    if stdin not in (None, _subprocess.DEVNULL, _subprocess.PIPE):
        return None
    if kwargs.get("capture_output"):
        streams = ("pipe", "pipe")
    else:
        streams = []
        for key in ("stdout", "stderr"):
            stream = kwargs.get(key)
            if stream == _subprocess.PIPE:
                streams.append("pipe")
            elif stream == _subprocess.DEVNULL:
                streams.append("devnull")
            else:
                return None  # inherited, a file or STDOUT: the output would not be reproduced
        streams = tuple(streams)
    text = bool(kwargs.get("text") or kwargs.get("universal_newlines")
                or kwargs.get("encoding") or kwargs.get("errors"))
    return {"streams": list(streams), "text": text, "encoding": kwargs.get("encoding"),
            "errors": kwargs.get("errors")}


def _ui_script_files(script, cwd):
    """([input paths], [output paths]) named by a UI script, or None when the script is not understood."""
    try:
        text = script.read_text(encoding="utf-8")
    except (OSError, ValueError):
        return None
    inputs, outputs = [], []
    for line in text.splitlines():
        line = line.strip(" \t\r")
        if not line or line.startswith("#"):
            continue
        args = line.split()
        command = args[0]
        if command in ("key", "type"):
            # Keyboard shortcuts can copy to or paste from the system clipboard, which the key does not cover.
            if any(modifier in line.lower() for modifier in ("ctrl", "cmd", "super", "shift+ins")):
                return None
            continue
        if command not in _UI_SCRIPT_COMMANDS:
            return None
        if command == "shot" and len(args) >= 2:
            outputs.append(_resolve(args[1], cwd))
        elif command == "catch_realloc" and len(args) >= 4:
            outputs.append(_resolve(args[2], cwd))
        elif command in ("reload", "reloadwithouttextures"):
            path = line.split(None, 1)[1].strip(" \t") if len(args) >= 2 else ""
            if len(path) >= 2 and path[0] == '"' and path[-1] == '"':
                path = path[1:-1]
            if not path:
                return None
            inputs.append(path)
    return inputs, outputs


def _parse_cache_arguments(command, cwd):
    """(value tokens, outputs) for a command whose outputs are fully understood, or (None, reason)."""
    tokens = []
    outputs = []
    isolated = False
    args = command[1:]
    index = 0
    while index < len(args):
        arg = args[index]
        if arg in _UNSUPPORTED_FLAGS:
            return None, f"flag {arg}"
        if arg == "--ui-report":
            if index + 1 >= len(args):
                return None, f"{arg} without a value"
            outputs.append(_resolve(args[index + 1], cwd))
            index += 2
            continue
        if arg == "--ui-script":
            if index + 1 >= len(args):
                return None, f"{arg} without a value"
            script = _ui_script_files(_resolve(args[index + 1], cwd), cwd)
            if script is None:
                return None, "UI script not understood"
            tokens.append(args[index + 1])
            tokens += [str(path) for path in script[0]]
            outputs += script[1]
            index += 2
            continue
        if arg in _OUTPUT_FLAGS:
            if index + 1 >= len(args):
                return None, f"{arg} without a value"
            outputs.append(_resolve(args[index + 1], cwd))
            index += 2
            continue
        if arg.startswith("--"):
            if arg in ("--isolated", "--no-persist"):
                isolated = True
            elif _OUTPUT_LIKE.search(arg) and arg not in _SAFE_OUTPUT_LIKE:
                return None, f"flag {arg}"
        else:
            tokens.append(arg)
        index += 1
    if not isolated:
        return None, "not --isolated"
    if not outputs:
        return None, "no output flag"
    if len(set(outputs)) != len(outputs):
        return None, "repeated output"
    return tokens, outputs


@functools.lru_cache(maxsize=None)
def _file_sha256(path, mtime_ns, size):
    del mtime_ns, size
    digest = hashlib.sha256()
    with open(path, "rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _sha256_file(path):
    try:
        stat = Path(path).stat()
        return _file_sha256(str(path), stat.st_mtime_ns, stat.st_size)
    except OSError:
        return None


def _gltf_document(model):
    """The glTF JSON of a .gltf or .glb model, or None when it cannot be read."""
    if model.suffix.lower() == ".gltf":
        try:
            return json.loads(model.read_text(encoding="utf-8"))
        except (OSError, ValueError):
            return None
    if model.suffix.lower() == ".glb":
        try:
            data = model.read_bytes()
        except OSError:
            return None
        if len(data) < 20 or data[:4] != b"glTF":
            return None
        chunk_length = int.from_bytes(data[12:16], "little")
        if data[16:20] != b"JSON" or chunk_length > len(data) - 20:
            return None
        try:
            return json.loads(data[20:20 + chunk_length].decode("utf-8"))
        except (UnicodeDecodeError, ValueError):
            return None
    return None


def _uris(node):
    """Every string stored under a "uri" key anywhere in the glTF JSON."""
    if isinstance(node, dict):
        for key, value in node.items():
            if key == "uri" and isinstance(value, str):
                yield value
            else:
                yield from _uris(value)
    elif isinstance(node, list):
        for value in node:
            yield from _uris(value)


def _model_files(model):
    """The files a .gltf/.glb names, or None when any cannot be resolved."""
    document = _gltf_document(model)
    if document is None:
        return None
    files = []
    for uri in _uris(document):
        if not uri or uri.startswith("data:"):
            continue
        if "://" in uri:
            return None
        path = Path(uri)
        if not path.is_absolute():
            path = model.parent / path
        if not path.is_file():
            # Glider may URL-decode the URI before opening it.
            from urllib.parse import unquote
            decoded = Path(unquote(uri))
            if not decoded.is_absolute():
                decoded = model.parent / decoded
            if not decoded.is_file():
                return None
            path = decoded
        files.append(path.resolve())
    return files


def _inputs(tokens, cwd, outputs):
    """[(path, sha256)] of every file argument and the files its models name, or (None, reason)."""
    output_paths = {str(path) for path in outputs}
    files = []
    for token in tokens:
        path = _resolve(token, cwd)
        try:
            if path.is_dir():
                return None, f"directory argument {token}"
            if not path.is_file():
                continue
        except OSError:
            continue
        if str(path) in output_paths:
            return None, "an output is also an input"
        files.append(path)
        if path.suffix.lower() in (".gltf", ".glb"):
            named = _model_files(path)
            if named is None:
                return None, f"unresolved file in {path.name}"
            files += named
    result = []
    seen = set()
    for path in files:
        key = str(path)
        if key in seen:
            continue
        if key in output_paths:
            return None, "an output is also an input"
        seen.add(key)
        digest = _sha256_file(path)
        if digest is None:
            return None, f"unreadable input {path.name}"
        result.append((path, digest))
    return result, None


def _env_signature(env):
    """Sorted (name, value) pairs of the child environment that can change a render."""
    parent = {name.upper(): value for name, value in os.environ.items()}
    items = {}
    for name, value in env.items():
        upper = name.upper()
        if upper in _ENV_IGNORED or upper.startswith(_ENV_IGNORED_PREFIXES):
            continue
        if upper in _ENV_NAMES or upper.startswith(_ENV_PREFIXES) or parent.get(upper) != value:
            items[upper] = value
    present = {name.upper() for name in env}
    for upper in parent:
        if upper not in present and upper not in _ENV_IGNORED and not upper.startswith(_ENV_IGNORED_PREFIXES):
            items[upper] = None  # the caller removed a variable
    return sorted(items.items(), key=lambda item: item[0])


@functools.lru_cache(maxsize=1)
def machine_signature():
    """Display adapters with driver versions, system DPI and screen sizes (Windows)."""
    parts = []
    if os.name == "nt":
        try:
            import winreg
            root = r"SYSTEM\CurrentControlSet\Control\Class\{4d36e968-e325-11ce-bfc1-08002be10318}"
            with winreg.OpenKey(winreg.HKEY_LOCAL_MACHINE, root) as display:
                index = 0
                while True:
                    try:
                        name = winreg.EnumKey(display, index)
                    except OSError:
                        break
                    index += 1
                    if not name.isdigit():
                        continue
                    try:
                        with winreg.OpenKey(display, name) as adapter:
                            values = []
                            for value in ("DriverDesc", "DriverVersion", "DriverDate"):
                                try:
                                    values.append(str(winreg.QueryValueEx(adapter, value)[0]))
                                except OSError:
                                    values.append("")
                            parts.append("adapter " + name + " " + "|".join(values))
                    except OSError:
                        continue
        except OSError:
            parts.append("adapters unknown")
        try:
            import ctypes
            user32 = ctypes.windll.user32
            metrics = [user32.GetSystemMetrics(index) for index in (0, 1, 78, 79, 80)]
            parts.append("screen " + ",".join(str(value) for value in metrics))
            get_dpi = getattr(user32, "GetDpiForSystem", None)
            parts.append(f"dpi {get_dpi() if get_dpi else 'unknown'}")
        except (AttributeError, OSError):
            parts.append("screen unknown")
    return "\n".join(parts)


def cache_key(target, command, cwd, inputs, env, mode):
    """The cache key of one launch (see the module docstring)."""
    digest = hashlib.sha256()
    digest.update(f"glider-render-cache-v{CACHE_VERSION}\0".encode("ascii"))
    digest.update(_sha256_file(target).encode("ascii"))
    digest.update(b"\0cwd\0")
    digest.update(str(cwd).encode("utf-8", "surrogatepass"))
    for arg in command[1:]:
        digest.update(b"\0arg\0")
        digest.update(arg.encode("utf-8", "surrogatepass"))
    for path, file_hash in inputs:
        digest.update(b"\0input\0")
        digest.update(str(path).encode("utf-8", "surrogatepass"))
        digest.update(b"\0")
        digest.update(file_hash.encode("ascii"))
    for name, value in _env_signature(env):
        digest.update(b"\0env\0")
        digest.update(name.encode("utf-8", "surrogatepass"))
        digest.update(b"\0unset\0" if value is None else b"\0=" + value.encode("utf-8", "surrogatepass"))
    digest.update(b"\0machine\0")
    digest.update(machine_signature().encode("utf-8", "surrogatepass"))
    digest.update(b"\0streams\0")
    digest.update(json.dumps(mode, sort_keys=True).encode("utf-8"))
    return digest.hexdigest()


def _cache_plan(command, target, kwargs):
    """(plan, None) when this launch can be served or stored, else (None, reason)."""
    if not cache_enabled():
        return None, f"{CACHE_SWITCH}=0"
    if not _cacheable_target(target):
        return None, "not a baseline"
    if Path(command[0]).name.lower() in ("cmd", "cmd.exe"):
        return None, "started through cmd"
    mode = _stream_mode(kwargs)
    if mode is None:
        return None, "stdout/stderr not captured"
    cwd = _resolve(kwargs.get("cwd") or Path.cwd(), None)
    tokens, outputs = _parse_cache_arguments(command, cwd)
    if tokens is None:
        return None, outputs
    inputs, reason = _inputs(tokens, cwd, outputs)
    if inputs is None:
        return None, reason
    if any(path.is_dir() for path in outputs):
        return None, "output is a directory"
    exe_hash = _sha256_file(target)
    if exe_hash is None:
        return None, "unreadable executable"
    key = cache_key(target, command, cwd, inputs, kwargs["env"], mode)
    return {
        "key": key,
        "entry": CACHE_DIR / key,
        "target": target,
        "exe_hash": exe_hash,
        "cwd": cwd,
        "command": list(command),
        "inputs": inputs,
        "mode": mode,
        "outputs": [{"path": path, "before": _stat_signature(path)} for path in outputs],
    }, None


def _stat_signature(path):
    try:
        stat = path.stat()
        return stat.st_mtime_ns, stat.st_size
    except OSError:
        return None


def _encode_stream(value):
    if value is None:
        return None
    return value.encode("utf-8", "surrogatepass") if isinstance(value, str) else bytes(value)


def _decode_stream(data, mode):
    if data is None:
        return None
    return data.decode("utf-8", "surrogatepass") if mode["text"] else data


def _cache_apply(plan, command, kwargs):
    entry = plan["entry"]
    manifest_path = entry / "entry.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if (manifest.get("key") != plan["key"] or manifest.get("version") != CACHE_VERSION
            or manifest.get("mode") != plan["mode"]):
        _remove_entry(entry)
        return None
    stored = manifest.get("outputs")
    if not isinstance(stored, list) or len(stored) != len(plan["outputs"]):
        _remove_entry(entry)
        return None

    files = []
    for item, output in zip(stored, plan["outputs"]):
        if not isinstance(item, dict) or item.get("path") != str(output["path"]):
            _remove_entry(entry)
            return None
        source = entry / str(item.get("file", ""))
        if not source.is_file() or _sha256_file(source) != item.get("sha256"):
            _remove_entry(entry)
            return None
        files.append((source, output["path"]))
    streams = {}
    for name in ("stdout", "stderr"):
        item = manifest.get(name)
        if item is None:
            streams[name] = None
            continue
        source = entry / str(item.get("file", ""))
        if not source.is_file() or _sha256_file(source) != item.get("sha256"):
            _remove_entry(entry)
            return None
        streams[name] = _decode_stream(source.read_bytes(), plan["mode"])

    try:
        for source, destination in files:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    except OSError:
        return None
    result = _subprocess.CompletedProcess(command, int(manifest.get("returncode", 0)),
                                          stdout=streams["stdout"], stderr=streams["stderr"])
    result.from_cache = True
    print(f"harness_process: render cache hit {plan['target'].name}: "
          f"{entry.relative_to(REPO)} ({len(files)} output(s))", file=sys.stderr)
    return result


def _cache_write(plan, result, kwargs):
    if result.returncode != 0:
        return False
    # The key names the input state the render started from; never store a result when
    # the exe or an input file changed while the process ran.
    if _sha256_file(plan["target"]) != plan["exe_hash"]:
        return False
    for path, digest in plan["inputs"]:
        if _sha256_file(path) != digest:
            return False
    entry = plan["entry"]
    if (entry / "entry.json").is_file():
        return False
    for output in plan["outputs"]:
        path = output["path"]
        current = _stat_signature(path)
        if current is None or current == output["before"]:
            return False
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    # Not tempfile.mkdtemp: its 0700 mode gives the directory an ACL the sandboxed
    # process cannot write to under Windows. A normally inherited directory works.
    temporary = CACHE_DIR / f".tmp-{os.getpid()}-{os.urandom(4).hex()}"
    try:
        temporary.mkdir()
    except OSError:
        return False
    try:
        stored = []
        for index, output in enumerate(plan["outputs"]):
            suffix = "".join(char for char in output["path"].suffix
                             if char.isalnum() or char == ".")[:16]
            name = f"{index}{suffix}"
            shutil.copyfile(output["path"], temporary / name)
            digest = _sha256_file(temporary / name)
            if digest is None:
                _remove_entry(temporary)
                return False
            stored.append({"path": str(output["path"]), "file": name, "sha256": digest})
        streams = {}
        for name in ("stdout", "stderr"):
            data = _encode_stream(getattr(result, name, None))
            if data is None:
                streams[name] = None
                continue
            (temporary / name).write_bytes(data)
            streams[name] = {"file": name, "sha256": _sha256_file(temporary / name)}
        manifest = {
            "version": CACHE_VERSION,
            "key": plan["key"],
            "exe": {"path": str(plan["target"]), "sha256": plan["exe_hash"]},
            "cwd": str(plan["cwd"]),
            "argv": plan["command"],
            "inputs": [{"path": str(path), "sha256": digest} for path, digest in plan["inputs"]],
            "env": [[name, value] for name, value in _env_signature(kwargs["env"])],
            "machine": machine_signature(),
            "mode": plan["mode"],
            "returncode": result.returncode,
            "outputs": stored,
            "stdout": streams["stdout"],
            "stderr": streams["stderr"],
            "created": time.strftime("%Y-%m-%d %H:%M:%S"),
        }
        (temporary / "entry.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
        if entry.exists():
            _remove_entry(entry)
        os.replace(temporary, entry)
        return True
    except (OSError, ValueError):
        _remove_entry(temporary)
        return False


def _remove_entry(entry):
    try:
        shutil.rmtree(entry)
    except OSError:
        pass


def prune_cache(max_temporary_age_s=86400):
    """Removes entries of older cache versions and entries whose executable is gone or changed.

    Returns (kept, removed)."""
    kept = removed = 0
    if not CACHE_DIR.is_dir():
        return kept, removed
    now = time.time()
    for entry in CACHE_DIR.iterdir():
        if not entry.is_dir():
            continue
        if entry.name.startswith(".tmp-"):
            try:
                if now - entry.stat().st_mtime > max_temporary_age_s:
                    _remove_entry(entry)
                    removed += 1
            except OSError:
                pass
            continue
        try:
            manifest = json.loads((entry / "entry.json").read_text(encoding="utf-8"))
            exe = manifest["exe"]
            valid = (manifest.get("version") == CACHE_VERSION and Path(exe["path"]).is_file()
                     and _sha256_file(Path(exe["path"])) == exe["sha256"])
        except (OSError, ValueError, KeyError, TypeError):
            valid = False
        if valid:
            kept += 1
        else:
            _remove_entry(entry)
            removed += 1
    return kept, removed
