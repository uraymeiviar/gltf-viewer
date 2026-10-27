"""Live script control client for Glider glTF viewer.

Supports bidirectional communication via file-watching follow mode:
Glider is launched with `--ui-script-follow <file>` and reads commands
line-by-line as they are appended, writing structured JSONL responses
to `<file>.out.jsonl`.
"""

import argparse
import json
import os
import subprocess
import sys
import time
import uuid
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union


DEFAULT_EXE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build",
    "bin",
    "Release",
    "Glider.exe",
)

DEFAULT_SESSIONS_DIR = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "build",
    "live_sessions",
)


class GliderLiveError(Exception):
    """Exception raised for Glider live control errors."""
    pass


class GliderLive:
    """Live interactive session manager for Glider glTF viewer."""

    def __init__(
        self,
        model: str,
        exe: Optional[str] = None,
        session_dir: Optional[str] = None,
        extra_args: Optional[List[str]] = None,
        startup_timeout: float = 10.0,
    ):
        self.model = os.path.abspath(model)
        self.exe = os.path.abspath(exe or DEFAULT_EXE)
        if not os.path.isfile(self.exe):
            raise FileNotFoundError(f"Glider executable not found at '{self.exe}'")

        self.session_id = f"glider_live_{int(time.time())}_{uuid.uuid4().hex[:8]}"
        base_dir = session_dir or DEFAULT_SESSIONS_DIR
        self.session_dir = os.path.join(base_dir, self.session_id)
        os.makedirs(self.session_dir, exist_ok=True)

        self.script_path = os.path.join(self.session_dir, "commands.script")
        self.out_path = self.script_path + ".out.jsonl"

        # Initialize empty files
        with open(self.script_path, "w", encoding="utf-8") as f:
            pass

        # Prepare environment
        env = os.environ.copy()
        env["GLIDER_NO_FOCUS"] = "1"

        cmd = [
            self.exe,
            self.model,
            "--isolated",
            "--ui-script-follow",
            self.script_path,
        ]
        if extra_args:
            cmd.extend(extra_args)

        self.cmd = cmd
        self.proc = subprocess.Popen(
            cmd,
            env=env,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )

        self._line_count = 0
        self._out_file = None
        self._last_read_pos = 0

        # Wait briefly for process to start and verify it hasn't crashed
        time.sleep(0.5)
        if self.proc.poll() is not None:
            raise GliderLiveError(
                f"Glider process exited immediately with returncode {self.proc.returncode}"
            )

        # Open out file for reading responses
        self._ensure_out_file(timeout=startup_timeout)

    def _ensure_out_file(self, timeout: float = 5.0):
        start = time.time()
        while time.time() - start < timeout:
            if os.path.exists(self.out_path):
                try:
                    self._out_file = open(self.out_path, "r", encoding="utf-8")
                    return
                except IOError:
                    pass
            time.sleep(0.05)
        # If not created yet, create it or wait when reading
        if not os.path.exists(self.out_path):
            with open(self.out_path, "a", encoding="utf-8") as f:
                pass
        self._out_file = open(self.out_path, "r", encoding="utf-8")

    def send(self, cmd: str, timeout: float = 5.0) -> Dict[str, Any]:
        """Send a single command string to Glider and wait for the response."""
        if self.proc.poll() is not None:
            raise GliderLiveError(
                f"Glider process has already terminated with code {self.proc.returncode}"
            )

        cmd_clean = cmd.strip()
        self._line_count += 1
        expected_line = self._line_count

        with open(self.script_path, "a", encoding="utf-8") as f:
            f.write(cmd_clean + "\n")
            f.flush()

        start = time.time()
        poll_interval = 0.02
        while time.time() - start < timeout:
            if self._out_file is None:
                self._ensure_out_file(timeout=0.5)

            line = self._out_file.readline()
            if line:
                line = line.strip()
                if line:
                    try:
                        record = json.loads(line)
                        if record.get("line") == expected_line:
                            return record
                    except json.JSONDecodeError:
                        pass
            else:
                if self.proc.poll() is not None and cmd_clean != "quit":
                    raise GliderLiveError(
                        f"Glider exited unexpectedly with code {self.proc.returncode}"
                    )
                time.sleep(poll_interval)

        raise TimeoutError(
            f"Timed out after {timeout}s waiting for response to command #{expected_line}: '{cmd_clean}'"
        )

    def frame(
        self,
        node: Union[str, int],
        view: Optional[str] = None,
        margin: Optional[float] = None,
        timeout: float = 5.0,
    ) -> Dict[str, Any]:
        """Frame a node by name or #index with optional view orientation and margin."""
        node_spec = f"#{node}" if isinstance(node, int) else str(node)
        parts = ["frame", node_spec]
        if view:
            parts.append(view)
        if margin is not None:
            parts.append(str(margin))
        return self.send(" ".join(parts), timeout=timeout)

    def select(self, node: Union[str, int], timeout: float = 5.0) -> Dict[str, Any]:
        """Select a node by name or #index."""
        node_spec = f"#{node}" if isinstance(node, int) else str(node)
        return self.send(f"select {node_spec}", timeout=timeout)

    def isolate(self, node: Union[str, int], children: bool = False, timeout: float = 5.0) -> Dict[str, Any]:
        """Isolate a node (and optionally its children)."""
        node_spec = f"#{node}" if isinstance(node, int) else str(node)
        cmd = f"isolate {node_spec}" if not children else f"isolate {node_spec} children"
        return self.send(cmd, timeout=timeout)

    def exitisolate(self, timeout: float = 5.0) -> Dict[str, Any]:
        """Exit isolation mode."""
        return self.send("exitisolate", timeout=timeout)

    def view(self, orientation: str, timeout: float = 5.0) -> Dict[str, Any]:
        """Set camera orientation (front, back, left, right, top, bottom, iso)."""
        return self.send(f"view {orientation}", timeout=timeout)

    def camera(
        self,
        eye: Union[Tuple[float, float, float], List[float]],
        target: Union[Tuple[float, float, float], List[float]],
        fov: Optional[float] = None,
        near: Optional[float] = None,
        far: Optional[float] = None,
        timeout: float = 5.0,
    ) -> Dict[str, Any]:
        """Set camera eye and target coordinates, with optional fov and near/far clipping planes."""
        eye_str = f"{eye[0]},{eye[1]},{eye[2]}"
        tgt_str = f"{target[0]},{target[1]},{target[2]}"
        parts = ["camera", eye_str, tgt_str]
        if fov is not None:
            parts.append(str(fov))
        elif near is not None and far is not None:
            parts.append("0")  # 0 indicates keep current fov
        if near is not None and far is not None:
            parts.append(f"{near},{far}")
        return self.send(" ".join(parts), timeout=timeout)

    def capture(self, path: str, timeout: float = 10.0) -> Dict[str, Any]:
        """Capture the 3D viewport to a PNG file."""
        abs_path = os.path.abspath(path)
        os.makedirs(os.path.dirname(abs_path), exist_ok=True)
        return self.send(f"capture {abs_path}", timeout=timeout)

    def noderect(self, node: Union[str, int], timeout: float = 5.0) -> Dict[str, Any]:
        """Query 2D screen bounding rectangle and visibility for a node."""
        node_spec = f"#{node}" if isinstance(node, int) else str(node)
        return self.send(f"noderect {node_spec}", timeout=timeout)

    def pick(self, x: int, y: int, timeout: float = 5.0) -> Dict[str, Any]:
        """Perform on-demand GPU picking at viewport pixel (x, y)."""
        return self.send(f"pick {x} {y}", timeout=timeout)

    def debugview(self, mode: str, timeout: float = 5.0) -> Dict[str, Any]:
        """Switch debug render mode (e.g. none, roughness, metallic, normal, basecolor)."""
        return self.send(f"debugview {mode}", timeout=timeout)

    def state(self, timeout: float = 5.0) -> Dict[str, Any]:
        """Query current viewer state (model, camera, window, viewport, selection, isolation)."""
        return self.send("state", timeout=timeout)

    def open(self, path: str, timeout: float = 15.0) -> Dict[str, Any]:
        """Open a new model file."""
        abs_path = os.path.abspath(path)
        return self.send(f"open {abs_path}", timeout=timeout)

    def close(self, timeout: float = 3.0):
        """Send quit command, wait for process exit, terminate if necessary."""
        if self.proc.poll() is None:
            try:
                self.send("quit", timeout=timeout)
            except Exception:
                pass

            try:
                self.proc.wait(timeout=timeout)
            except subprocess.TimeoutExpired:
                self.proc.kill()
                self.proc.wait()

        if self._out_file is not None:
            try:
                self._out_file.close()
            except Exception:
                pass
            self._out_file = None

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()


# State file for CLI session tracking
CLI_SESSION_FILE = os.path.join(DEFAULT_SESSIONS_DIR, "last_session.json")


def _save_session_info(info: dict):
    os.makedirs(DEFAULT_SESSIONS_DIR, exist_ok=True)
    with open(CLI_SESSION_FILE, "w", encoding="utf-8") as f:
        json.dump(info, f, indent=2)


def _load_session_info() -> Optional[dict]:
    if os.path.exists(CLI_SESSION_FILE):
        try:
            with open(CLI_SESSION_FILE, "r", encoding="utf-8") as f:
                return json.load(f)
        except Exception:
            return None
    return None


def main():
    parser = argparse.ArgumentParser(description="Live script control client for Glider glTF viewer")
    subparsers = parser.add_subparsers(dest="subcommand", required=True)

    # start
    p_start = subparsers.add_parser("start", help="Start a live Glider session")
    p_start.add_argument("model", help="Path to glTF / GLB model to load")
    p_start.add_argument("--exe", default=None, help="Path to Glider executable")
    p_start.add_argument("--session", default=None, help="Custom session directory")
    p_start.add_argument("--extra-args", nargs="*", default=None, help="Extra CLI flags to pass to Glider")

    # send
    p_send = subparsers.add_parser("send", help="Send command to live Glider session")
    p_send.add_argument("command", help="Command string to execute")
    p_send.add_argument("--session", default=None, help="Session directory or script file")
    p_send.add_argument("--timeout", type=float, default=10.0, help="Response timeout in seconds")

    # quit
    p_quit = subparsers.add_parser("quit", help="Quit live Glider session")
    p_quit.add_argument("--session", default=None, help="Session directory or script file")

    args = parser.parse_args()

    if args.subcommand == "start":
        live = GliderLive(
            model=args.model,
            exe=args.exe,
            session_dir=args.session,
            extra_args=args.extra_args,
        )
        info = {
            "pid": live.proc.pid,
            "session_id": live.session_id,
            "session_dir": live.session_dir,
            "script_path": live.script_path,
            "out_path": live.out_path,
        }
        _save_session_info(info)
        print(json.dumps(info, indent=2))
        sys.exit(0)

    elif args.subcommand in ("send", "quit"):
        script_path = None
        if args.session:
            if os.path.isfile(args.session):
                script_path = args.session
            elif os.path.isdir(args.session):
                script_path = os.path.join(args.session, "commands.script")
        else:
            info = _load_session_info()
            if info:
                script_path = info.get("script_path")

        if not script_path or not os.path.exists(script_path):
            print(f"Error: active session script not found. Run 'start' first or pass --session.", file=sys.stderr)
            sys.exit(1)

        out_path = script_path + ".out.jsonl"
        cmd_str = "quit" if args.subcommand == "quit" else args.command.strip()

        # Count current lines to determine expected line number
        current_lines = 0
        if os.path.exists(script_path):
            with open(script_path, "r", encoding="utf-8") as f:
                current_lines = sum(1 for _ in f)

        expected_line = current_lines + 1

        with open(script_path, "a", encoding="utf-8") as f:
            f.write(cmd_str + "\n")
            f.flush()

        # Poll output file
        timeout = getattr(args, "timeout", 10.0)
        start = time.time()
        record = None

        while time.time() - start < timeout:
            if os.path.exists(out_path):
                with open(out_path, "r", encoding="utf-8") as f:
                    for line in f:
                        line = line.strip()
                        if not line:
                            continue
                        try:
                            item = json.loads(line)
                            if item.get("line") == expected_line:
                                record = item
                                break
                        except json.JSONDecodeError:
                            pass
            if record is not None:
                break
            time.sleep(0.05)

        if record is None:
            print(f"Error: timeout waiting for response to command #{expected_line}: '{cmd_str}'", file=sys.stderr)
            sys.exit(1)

        print(json.dumps(record, indent=2))
        sys.exit(0 if record.get("ok", False) else 1)


if __name__ == "__main__":
    main()
