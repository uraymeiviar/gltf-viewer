# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Starts Glider harness children without requesting foreground keyboard focus.

Harnesses import this module as ``import harness_process as subprocess``. Its ``run()``
and ``Popen()`` add ``GLIDER_NO_FOCUS=1`` to every child environment and, on Windows,
start Glider with ``STARTF_USESHOWWINDOW`` / ``SW_SHOWNOACTIVATE``. Current builds also
read that variable (GLFW ``GLFW_FOCUSED`` and ``GLFW_FOCUS_ON_SHOW`` false), so their
window is still shown and rendered normally but never activated.

Old baseline binaries do not know the variable. They are still shown with
``SW_SHOWNOACTIVATE`` but may activate their window; that is accepted until the next
commit's baseline carries the marker.

To cut those remaining launches, ``run()`` caches renders of marker-less binaries under
``build/baselines/`` (the pinned references such as 167a5e0 and a9c0a29). The key is the
SHA-256 of the executable, the exact argument list, the working directory and the content
of the model and of every buffer/image it names; the value is the output files the command
wrote (dump/capture image, report JSON), stored under ``build/regression_cache/``. A hit
copies the outputs and skips the launch. The cache applies only when the arguments name
known outputs and every input resolves; anything unclear (UI scripts, benchmark output,
search paths, unknown flags, a missing output) starts the binary normally. The current
build and marker-carrying baselines always render.
"""

import functools
import hashlib
import json
import os
import shutil
import subprocess as _subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parents[1]
BASELINE_DIR = REPO / "build" / "baselines"
CACHE_DIR = REPO / "build" / "regression_cache"
CACHE_VERSION = 1
GLIDER_NO_FOCUS = "GLIDER_NO_FOCUS"
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


def _run(command, popenargs, kwargs):
    target, kwargs = _prepare(command, kwargs)
    plan = None
    if target is not None:
        try:
            plan = _cache_plan(command, target, kwargs)
        except Exception:  # noqa: BLE001 - caching must never break a harness
            plan = None
        if plan is not None:
            cached = _cache_apply(plan, command, kwargs)
            if cached is not None:
                return cached
    result = _subprocess.run(*popenargs, **kwargs)
    if plan is not None:
        try:
            _cache_write(plan, result)
        except Exception:  # noqa: BLE001 - caching must never break a harness
            pass
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
    del target
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

# Flags that write files. Only these are understood as cache outputs; every other
# output-producing flag below is a reason to skip the cache.
_OUTPUT_FLAGS = {
    "--dump-frame": ".png",
    "--capture-viewport": ".png",
    "--capture-first-frame": ".png",
    "--profile-report": ".json",
    "--asobo-report": ".json",
}
# Output-producing flags the cache does not model (timing reports, UI scripts and their
# shot files, validation reports written for an unloaded file, self-check reports).
_UNSUPPORTED_OUTPUT_FLAGS = {
    "--benchmark", "--benchmark-output", "--benchmark-warmup", "--benchmark-frames",
    "--exit-after-benchmark",
    "--ui-script", "--ui-report",
    "--validate-file", "--validation-report",
    "--scenario-report", "--idle-stats",
    "--conflict-selfcheck", "--validation-cache-selfcheck", "--cache-invalidation-selfcheck",
    "--logger-layout-selfcheck", "--revision-selfcheck", "--aa-target-selfcheck",
    "--sampler-selfcheck", "--sampler-selfcheck-case", "--sampler-dump-pattern",
    "--vao-selfcheck", "--vao-selfcheck-case", "--polygon-mode-selfcheck",
    "--polygon-mode-selfcheck-case", "--cull-winding-selfcheck", "--cull-winding-selfcheck-case",
    "--depth-selfcheck", "--depth-selfcheck-case", "--blend-selfcheck", "--blend-selfcheck-case",
}
# Inputs outside the model's own buffers/images: their content would have to be part of
# the key, so the cache leaves these launches alone.
_UNSUPPORTED_INPUT_FLAGS = {"--search-path", "--env", "--hdr", "--reload-model"}

# Every other argument Glider accepts. A flag missing from both tables means the command
# is not understood well enough to cache (the parser might mistake its value for a model).
_VALUE_FLAGS = {
    "--capture-frame", "--capture-time",
    "--benchmark-warmup", "--benchmark-frames",
    "--vsync", "--fixed-frame-time",
    "--decode-budget-mib", "--anim-time",
    "--scenario-name",
    "--camera-yaw", "--camera-pitch", "--camera-distance", "--camera-target",
    "--camera-fov", "--camera-projection", "--window-size",
    "--select-node", "--hide-node",
    "--rename-node", "--rename-mesh", "--rename-material",
    "--hierarchy-display", "--resource-display",
    "--select-material", "--select-image", "--select-texture", "--select-viewport",
    "--create-viewport-cam",
    "--log-test", "--filter-node", "--filter-resource", "--filter-validation",
    "--validation-severity",
    "--aa", "--anti-aliasing", "--hdr-rotation", "--exposure", "--tone-mapping",
    "--render-mode",
    "--quality", "--ibl-size", "--ibl-irradiance-size", "--ibl-lut-size",
    "--ibl-precision", "--ibl-samples", "--grid-mode", "--render-scale",
    "--asobo", "--asobo-disable",
}
_FLAG_FLAGS = {
    "--exit-after-dump", "--profile", "--no-profile", "--exit-after-profile",
    "--force-full-sync", "--disable-transform-reuse", "--always-pick", "--full-size-picking",
    "--disable-pick-culling", "--disable-shared-traversal", "--show-invisible-materials",
    "--disable-instancing", "--disable-redraw-skip", "--validation-full-read",
    "--disable-culling", "--disable-idle-wait", "--disable-command-caches",
    "--isolated", "--no-persist",
    "--show-validation", "--validate", "--show-search-paths", "--search-paths",
    "--show-statistics", "--show-logger", "--logger-wrap", "--logger-no-auto-scroll",
    "--log-test-variable", "--force-double-sided", "--invert-winding",
    "--selfcheck-force-failure",
}


def _cacheable_target(target):
    try:
        target.resolve().relative_to(BASELINE_DIR.resolve())
    except (OSError, ValueError):
        return False
    try:
        return target.is_file() and not _supports_no_focus(target)
    except OSError:
        return False


def _cache_io_supported(kwargs):
    if kwargs.get("input") is not None:
        return False
    for key in ("stdin", "stdout", "stderr"):
        stream = kwargs.get(key)
        if stream is not None and not isinstance(stream, int) and stream not in (
                _subprocess.PIPE, _subprocess.DEVNULL):
            return False
    return True


def _parse_cache_arguments(command, cwd):
    """(model, outputs) for a command whose outputs and inputs are fully understood."""
    positionals = []
    outputs = []
    isolated = False
    args = command[1:]
    index = 0
    while index < len(args):
        arg = args[index]
        if arg in _UNSUPPORTED_OUTPUT_FLAGS or arg in _UNSUPPORTED_INPUT_FLAGS:
            return None
        if arg in _OUTPUT_FLAGS:
            if index + 1 >= len(args):
                return None
            outputs.append(_resolve(args[index + 1], cwd))
            index += 2
        elif arg in _VALUE_FLAGS:
            if index + 1 >= len(args):
                return None
            index += 2
        elif arg in _FLAG_FLAGS:
            if arg in ("--isolated", "--no-persist"):
                isolated = True
            index += 1
        elif arg.startswith("-"):
            return None
        else:
            positionals.append(arg)
            index += 1
    if not isolated or len(positionals) > 1 or not outputs:
        return None
    if len(set(outputs)) != len(outputs):
        return None
    model = _resolve(positionals[0], cwd) if positionals else None
    if model is not None and not model.is_file():
        return None
    return model, outputs


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
        stat = path.stat()
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


def _model_inputs(model):
    """The model and the buffers/images it names, or None when any cannot be resolved."""
    if model is None:
        return []
    document = _gltf_document(model)
    if document is None:
        return None
    files = [model]
    uris = [entry.get("uri") for entry in document.get("buffers", [])]
    uris += [entry.get("uri") for entry in document.get("images", [])]
    for uri in uris:
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
    result = []
    seen = set()
    for path in files:
        key = str(path)
        if key in seen:
            continue
        seen.add(key)
        result.append((path, _sha256_file(path)))
        if result[-1][1] is None:
            return None
    return result


def _stat_signature(path):
    try:
        stat = path.stat()
        return stat.st_mtime_ns, stat.st_size
    except OSError:
        return None


def _cache_plan(command, target, kwargs):
    if not _cacheable_target(target) or not _cache_io_supported(kwargs):
        return None
    if Path(command[0]).name.lower() in ("cmd", "cmd.exe"):
        return None
    cwd = Path(kwargs.get("cwd") or Path.cwd())
    parsed = _parse_cache_arguments(command, cwd)
    if parsed is None:
        return None
    model, outputs = parsed
    inputs = _model_inputs(model)
    if inputs is None:
        return None
    input_paths = {str(path) for path, _ in inputs}
    if any(path.is_dir() or str(path) in input_paths for path in outputs):
        return None
    exe_hash = _sha256_file(target)
    if exe_hash is None:
        return None

    digest = hashlib.sha256()
    digest.update(f"glider-render-cache-v{CACHE_VERSION}\0".encode("ascii"))
    digest.update(exe_hash.encode("ascii"))
    digest.update(b"\0cwd\0")
    digest.update(str(cwd).encode("utf-8", "surrogatepass"))
    for arg in command:
        digest.update(b"\0arg\0")
        digest.update(arg.encode("utf-8", "surrogatepass"))
    for path, file_hash in inputs:
        digest.update(b"\0input\0")
        digest.update(str(path).encode("utf-8", "surrogatepass"))
        digest.update(b"\0")
        digest.update(file_hash.encode("ascii"))
    key = digest.hexdigest()
    return {
        "key": key,
        "entry": CACHE_DIR / key,
        "target": target,
        "exe_hash": exe_hash,
        "cwd": cwd,
        "command": list(command),
        "inputs": inputs,
        "outputs": [{"path": path, "before": _stat_signature(path)} for path in outputs],
    }


def _empty_completed_process(command, kwargs):
    text_mode = bool(kwargs.get("text") or kwargs.get("universal_newlines")
                     or kwargs.get("encoding") or kwargs.get("errors"))
    empty = "" if text_mode else b""
    return _subprocess.CompletedProcess(command, 0, stdout=empty, stderr=empty)


def _cache_apply(plan, command, kwargs):
    entry = plan["entry"]
    manifest_path = entry / "entry.json"
    try:
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return None
    if manifest.get("key") != plan["key"] or manifest.get("version") != CACHE_VERSION:
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

    try:
        for source, destination in files:
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
    except OSError:
        return None
    result = _empty_completed_process(command, kwargs)
    result.from_cache = True
    print(f"harness_process: render cache hit {plan['target'].name}: "
          f"{entry.relative_to(REPO)} ({len(files)} output(s))", file=sys.stderr)
    return result


def _cache_write(plan, result):
    if result.returncode != 0:
        return
    # The key names the input state the render started from; never store a result when
    # the exe or a model file changed while the process ran.
    if _sha256_file(plan["target"]) != plan["exe_hash"]:
        return
    for path, digest in plan["inputs"]:
        if _sha256_file(path) != digest:
            return
    entry = plan["entry"]
    if (entry / "entry.json").is_file():
        return
    for output in plan["outputs"]:
        path = output["path"]
        current = _stat_signature(path)
        if current is None or current == output["before"]:
            return
    CACHE_DIR.mkdir(parents=True, exist_ok=True)
    # Not tempfile.mkdtemp: its 0700 mode gives the directory an ACL the sandboxed
    # process cannot write to under Windows. A normally inherited directory works.
    temporary = CACHE_DIR / f".tmp-{os.getpid()}-{os.urandom(4).hex()}"
    try:
        temporary.mkdir()
    except OSError:
        return
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
                return
            stored.append({"path": str(output["path"]), "file": name, "sha256": digest})
        manifest = {
            "version": CACHE_VERSION,
            "key": plan["key"],
            "exe": {"path": str(plan["target"]), "sha256": plan["exe_hash"]},
            "cwd": str(plan["cwd"]),
            "argv": plan["command"],
            "inputs": [{"path": str(path), "sha256": digest} for path, digest in plan["inputs"]],
            "outputs": stored,
        }
        (temporary / "entry.json").write_text(json.dumps(manifest, indent=1), encoding="utf-8")
        if entry.exists():
            _remove_entry(entry)
        os.replace(temporary, entry)
    except (OSError, ValueError):
        _remove_entry(temporary)


def _remove_entry(entry):
    try:
        shutil.rmtree(entry)
    except OSError:
        pass
