# Copyright (c) 2026 UX3D GmbH / Gestaltor Contributors. All rights reserved.
"""MCP server (stdio) for the Glider agent control pipe (docs/AGENT_CONTROL_PLAN.md section 3.5).

Dependency free: JSON-RPC 2.0 over newline-delimited stdio is implemented here, so the `mcp`
package is not needed. The tools are built from Glider's own `describe` reply, so they always
match the executable it attaches to. `capture` results are returned as MCP image content.

  python scripts/glider_mcp.py                      # attach to the only running instance
  python scripts/glider_mcp.py --pid 1234           # attach to this instance
  python scripts/glider_mcp.py --launch [--exe Glider.exe] [--model file.glb] [-- extra Glider args]

Only protocol messages are written to stdout; diagnostics go to stderr.
"""

import argparse
import json
import sys
from pathlib import Path
from typing import Any, Dict, List, Optional

sys.path.insert(0, str(Path(__file__).resolve().parent))
import glider_ctl  # noqa: E402

SERVER_NAME = "glider"
SERVER_VERSION = "1.0.0"
PROTOCOL_VERSIONS = ("2025-06-18", "2024-11-05")
GUIDE_URI = "glider://guide"


def log(message: str) -> None:
    print(f"[glider_mcp] {message}", file=sys.stderr, flush=True)


class GliderMcpServer:
    def __init__(self, options: argparse.Namespace):
        self.options = options
        self.pipe: Optional[glider_ctl.GliderPipe] = None
        self.process = None
        self.commands: List[Dict[str, Any]] = []
        self.guide = ""
        self.connect_error = ""

    # ---- Glider connection (attach or launch) ----

    def connect(self) -> bool:
        if self.pipe is not None:
            return True
        try:
            pid = self.options.pid
            if pid is None and self.options.launch and not glider_ctl.list_instances():
                self.process = glider_ctl.launch(self.options.exe, self.options.model, self.options.glider_args)
                pid = self.process.pid
                log(f"launched Glider pid {pid}")
            self.pipe = glider_ctl.GliderPipe(pid)
            self.commands = self.pipe.call("describe")["commands"]
            self.guide = self.pipe.call("guide")["text"]
            self.connect_error = ""
            log(f"attached to {self.pipe.name} ({len(self.commands)} commands)")
            return True
        except glider_ctl.GliderError as error:
            self.connect_error = str(error)
            self.pipe = None
            log(f"cannot attach: {error}")
            return False

    def disconnect(self) -> None:
        if self.pipe is not None:
            self.pipe.close()
        self.pipe = None

    # ---- MCP methods ----

    def initialize(self, params: Dict[str, Any]) -> Dict[str, Any]:
        requested = params.get("protocolVersion")
        version = requested if requested in PROTOCOL_VERSIONS else PROTOCOL_VERSIONS[0]
        self.connect()
        instructions = self.guide or (
            "Glider agent control. No Glider instance is attached yet (" + self.connect_error +
            "). Start Glider with --control-pipe (or File > Settings > Agent Control) and call any tool, "
            "or restart this server with --launch.")
        return {
            "protocolVersion": version,
            "capabilities": {"tools": {"listChanged": False}, "resources": {}},
            "serverInfo": {"name": SERVER_NAME, "version": SERVER_VERSION},
            "instructions": instructions,
        }

    def tools_list(self) -> Dict[str, Any]:
        self.connect()
        tools = []
        for command in self.commands:
            description = command.get("description", "")
            if command.get("kind") == "multi-frame":
                description += " (multi-frame: replies after rendering)"
            tools.append({"name": command["name"], "description": description,
                          "inputSchema": command.get("inputSchema") or {"type": "object", "properties": {}}})
        return {"tools": tools}

    def tools_call(self, params: Dict[str, Any]) -> Dict[str, Any]:
        name = params.get("name", "")
        arguments = dict(params.get("arguments") or {})
        if not self.connect():
            return self.error_content(f"Glider is not attached: {self.connect_error}")
        if name == "capture" and "encoding" not in arguments:
            arguments["encoding"] = "png-base64"
        try:
            reply = self.pipe.request(name, arguments, timeout=600.0)
        except glider_ctl.GliderError as error:
            # The instance went away: drop the connection so the next call re-attaches.
            self.disconnect()
            return self.error_content(f"{error.code}: {error.message}")
        if not reply.get("ok"):
            return self.error_content(json.dumps(reply.get("error"), indent=2))
        result = reply.get("result")
        content: List[Dict[str, Any]] = []
        if isinstance(result, dict) and result.get("encoding") == "png-base64" and "data" in result:
            content.append({"type": "image", "data": result["data"], "mimeType": result.get("mimeType", "image/png")})
            result = {key: value for key, value in result.items() if key != "data"}
        if name == "guide" and isinstance(result, dict) and "text" in result:
            content.append({"type": "text", "text": result["text"]})
        else:
            content.append({"type": "text", "text": json.dumps(result, indent=2)})
        return {"content": content, "isError": False}

    @staticmethod
    def error_content(text: str) -> Dict[str, Any]:
        return {"content": [{"type": "text", "text": text}], "isError": True}

    def resources_list(self) -> Dict[str, Any]:
        return {"resources": [{"uri": GUIDE_URI, "name": "Glider agent guide", "mimeType": "text/markdown",
                               "description": "How to drive Glider over the control pipe, with the command reference."}]}

    def resources_read(self, params: Dict[str, Any]) -> Dict[str, Any]:
        if params.get("uri") != GUIDE_URI:
            raise JsonRpcError(-32602, f"unknown resource {params.get('uri')}")
        self.connect()
        return {"contents": [{"uri": GUIDE_URI, "mimeType": "text/markdown",
                              "text": self.guide or f"Glider is not attached: {self.connect_error}"}]}

    def handle(self, message: Dict[str, Any]) -> Optional[Dict[str, Any]]:
        method = message.get("method")
        params = message.get("params") or {}
        is_request = "id" in message
        if method is None:
            return None  # a response from the client; the server sends no requests
        try:
            if method == "initialize":
                result = self.initialize(params)
            elif method == "ping":
                result = {}
            elif method == "tools/list":
                result = self.tools_list()
            elif method == "tools/call":
                result = self.tools_call(params)
            elif method == "resources/list":
                result = self.resources_list()
            elif method == "resources/templates/list":
                result = {"resourceTemplates": []}
            elif method == "resources/read":
                result = self.resources_read(params)
            elif method.startswith("notifications/"):
                return None
            else:
                raise JsonRpcError(-32601, f"method not found: {method}")
        except JsonRpcError as error:
            return {"jsonrpc": "2.0", "id": message.get("id"), "error": {"code": error.code, "message": error.message}} if is_request else None
        except Exception as error:  # keep serving after an unexpected failure
            log(f"{method} failed: {error!r}")
            return {"jsonrpc": "2.0", "id": message.get("id"), "error": {"code": -32603, "message": str(error)}} if is_request else None
        return {"jsonrpc": "2.0", "id": message["id"], "result": result} if is_request else None


class JsonRpcError(Exception):
    def __init__(self, code: int, message: str):
        super().__init__(message)
        self.code = code
        self.message = message


def write_message(message: Dict[str, Any]) -> None:
    sys.stdout.buffer.write((json.dumps(message, separators=(",", ":")) + "\n").encode("utf-8"))
    sys.stdout.buffer.flush()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--pid", type=int, help="attach to this Glider process")
    parser.add_argument("--launch", action="store_true", help="start Glider with --control-pipe when none is running")
    parser.add_argument("--exe", help="Glider.exe for --launch (default build/bin/Release/Glider.exe)")
    parser.add_argument("--model", help="model for --launch")
    parser.add_argument("glider_args", nargs="*", help="extra Glider arguments for --launch (after --)")
    options = parser.parse_args()
    server = GliderMcpServer(options)
    for raw in sys.stdin.buffer:
        line = raw.strip()
        if not line:
            continue
        try:
            message = json.loads(line.decode("utf-8"))
        except ValueError as error:
            write_message({"jsonrpc": "2.0", "id": None, "error": {"code": -32700, "message": f"parse error: {error}"}})
            continue
        if isinstance(message, list):
            replies = [reply for reply in (server.handle(item) for item in message if isinstance(item, dict)) if reply]
            if replies:
                sys.stdout.buffer.write((json.dumps(replies, separators=(",", ":")) + "\n").encode("utf-8"))
                sys.stdout.buffer.flush()
            continue
        if not isinstance(message, dict):
            write_message({"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "invalid request"}})
            continue
        reply = server.handle(message)
        if reply is not None:
            write_message(reply)
    server.disconnect()
    return 0


if __name__ == "__main__":
    sys.exit(main())
