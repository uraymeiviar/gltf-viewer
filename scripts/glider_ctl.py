# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""Plain client for the Glider agent control pipe (docs/AGENT_CONTROL_PLAN.md).

Glider started with --control-pipe (or File > Settings > Agent Control) listens on
\\\\.\\pipe\\glider-<pid> for line-delimited JSON requests {"id", "cmd", "args"}. This module is
dependency free; scripts/glider_mcp.py and scripts/validate_agent_control.py import it.

Usage:
  python scripts/glider_ctl.py --list
  python scripts/glider_ctl.py describe
  python scripts/glider_ctl.py frame_node node=Wing view=iso margin=1.2
  python scripts/glider_ctl.py --pid 1234 capture path=build/shot.png
  python scripts/glider_ctl.py --launch --model resources/sampleGltf/HelloCube.glb get_state

Argument values are parsed as JSON when they parse (numbers, true/false, [1,2,3]) and are strings
otherwise. The reply's result is printed as JSON; a failed command exits with status 1.
"""

import argparse
import ctypes
import json
import msvcrt
import os
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

REPO = Path(__file__).resolve().parents[1]
DEFAULT_EXE = REPO / "build" / "bin" / "Release" / "Glider.exe"
PIPE_PREFIX = "\\\\.\\pipe\\"
PIPE_PATTERN = re.compile(r"^glider-(\d+)$")
ERROR_PIPE_BUSY = 231


class GliderError(Exception):
    """A command reply with ok=false, or a broken connection."""

    def __init__(self, code: str, message: str, reply: Optional[Dict[str, Any]] = None):
        super().__init__(f"{code}: {message}")
        self.code = code
        self.message = message
        self.reply = reply or {}


def pipe_name(pid: int) -> str:
    return f"{PIPE_PREFIX}glider-{pid}"


def list_instances() -> List[int]:
    """Process ids of running Glider instances with an open control pipe."""
    try:
        names = os.listdir(PIPE_PREFIX)
    except OSError:
        return []
    pids = []
    for name in names:
        match = PIPE_PATTERN.match(name)
        if match:
            pids.append(int(match.group(1)))
    return sorted(pids)


def launch(exe: Optional[str] = None, model: Optional[str] = None, extra: Optional[List[str]] = None,
           timeout: float = 60.0) -> subprocess.Popen:
    """Starts Glider with --control-pipe and GLIDER_NO_FOCUS=1 and waits for its pipe."""
    exe_path = str(Path(exe) if exe else DEFAULT_EXE)
    command = [exe_path]
    if model:
        command.append(str(model))
    command.append("--control-pipe")
    command.extend(extra or [])
    env = os.environ.copy()
    env["GLIDER_NO_FOCUS"] = "1"
    process = subprocess.Popen(command, env=env, stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
    deadline = time.time() + timeout
    while time.time() < deadline:
        if process.poll() is not None:
            raise GliderError("launch_failed", f"Glider exited with code {process.returncode}")
        if os.path.exists(pipe_name(process.pid)):
            return process
        time.sleep(0.1)
    process.kill()
    raise GliderError("launch_timeout", "Glider did not open its control pipe in time")


class GliderPipe:
    """One connection to a Glider control pipe. Requests are answered in order."""

    def __init__(self, pid: Optional[int] = None, connect_timeout: float = 10.0):
        if pid is None:
            pids = list_instances()
            if not pids:
                raise GliderError("no_instance", "no Glider with an open control pipe (start it with --control-pipe)")
            if len(pids) > 1:
                raise GliderError("several_instances", f"several Glider instances: {pids}; pass a pid")
            pid = pids[0]
        self.pid = pid
        self.name = pipe_name(pid)
        deadline = time.time() + connect_timeout
        while True:
            try:
                self._file = open(self.name, "r+b", buffering=0)
                break
            except OSError as error:
                # Busy, not yet created, or briefly between pipe instances (seen as EINVAL right after
                # launch): all transient, so retry until the deadline.
                if time.time() > deadline:
                    raise GliderError("connect_failed", f"cannot open {self.name}: {error} "
                                      f"(winerror {getattr(error, 'winerror', None)})")
                time.sleep(0.05)
        # Synchronous pipe handles serialise reads and writes, so there is no reader thread:
        # replies are polled with PeekNamedPipe, which also gives read timeouts.
        self._handle = msvcrt.get_osfhandle(self._file.fileno())
        self._buffer = b""
        self._next_id = 0

    def _available(self) -> int:
        available = ctypes.c_ulong(0)
        if not ctypes.windll.kernel32.PeekNamedPipe(ctypes.c_void_p(self._handle), None, 0, None,
                                                    ctypes.byref(available), None):
            raise GliderError("disconnected", "the pipe was closed")
        return available.value

    def read_reply(self, timeout: float = 60.0) -> Dict[str, Any]:
        deadline = time.time() + timeout
        while b"\n" not in self._buffer:
            count = self._available()
            if count:
                chunk = self._file.read(count)
                if not chunk:
                    raise GliderError("disconnected", "the pipe was closed")
                self._buffer += chunk
                continue
            if time.time() > deadline:
                raise GliderError("timeout", f"no reply within {timeout} s")
            time.sleep(0.002)
        line, self._buffer = self._buffer.split(b"\n", 1)
        return json.loads(line.decode("utf-8"))

    def send(self, cmd: str, args: Optional[Dict[str, Any]] = None) -> int:
        self._next_id += 1
        message = {"id": self._next_id, "cmd": cmd, "args": args or {}}
        data = (json.dumps(message) + "\n").encode("utf-8")
        self._file.write(data)
        return self._next_id

    def request(self, cmd: str, args: Optional[Dict[str, Any]] = None, timeout: float = 120.0) -> Dict[str, Any]:
        """Sends one command and returns the whole reply object ({"id", "ok", "result"|"error"})."""
        request_id = self.send(cmd, args)
        while True:
            reply = self.read_reply(timeout)
            if reply.get("id") == request_id or reply.get("id") is None:
                return reply

    def call(self, cmd: str, timeout: float = 120.0, **args: Any) -> Any:
        """Sends one command and returns its result; raises GliderError when it fails."""
        reply = self.request(cmd, args, timeout)
        if not reply.get("ok"):
            error = reply.get("error") or {}
            raise GliderError(error.get("code", "error"), error.get("message", "command failed"), reply)
        return reply.get("result")

    def close(self) -> None:
        try:
            self._file.close()
        except OSError:
            pass

    def __enter__(self) -> "GliderPipe":
        return self

    def __exit__(self, *exc: Any) -> None:
        self.close()


def parse_value(text: str) -> Any:
    try:
        return json.loads(text)
    except ValueError:
        return text


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pid", type=int, help="Glider process id (default: the only running instance)")
    parser.add_argument("--list", action="store_true", help="list running instances and exit")
    parser.add_argument("--launch", action="store_true", help="start Glider with --control-pipe first")
    parser.add_argument("--exe", help="Glider.exe for --launch")
    parser.add_argument("--model", help="model for --launch")
    parser.add_argument("--json", dest="json_args", help="arguments as one JSON object")
    parser.add_argument("--timeout", type=float, default=120.0)
    parser.add_argument("cmd", nargs="?")
    parser.add_argument("args", nargs="*", help="key=value pairs")
    options = parser.parse_args()

    if options.list:
        for pid in list_instances():
            print(json.dumps({"pid": pid, "pipe": pipe_name(pid)}))
        return 0
    if not options.cmd:
        parser.error("a command is required (try describe)")
    args: Dict[str, Any] = json.loads(options.json_args) if options.json_args else {}
    for item in options.args:
        if "=" not in item:
            parser.error(f"expected key=value, got {item!r}")
        key, value = item.split("=", 1)
        args[key] = parse_value(value)

    pid = options.pid
    if options.launch:
        pid = launch(options.exe, options.model).pid
    try:
        with GliderPipe(pid) as pipe:
            reply = pipe.request(options.cmd, args, options.timeout)
    except GliderError as error:
        print(json.dumps({"ok": False, "error": {"code": error.code, "message": error.message}}), file=sys.stderr)
        return 1
    if reply.get("ok"):
        print(json.dumps(reply.get("result"), indent=2))
        return 0
    print(json.dumps(reply.get("error"), indent=2), file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
