#!/usr/bin/env python3
"""MCP stdio adapter for an explicitly enabled local FluentQt preview session."""

from __future__ import annotations

import argparse
import base64
import json
import math
from pathlib import Path
import sys

from fluent_qt_preview_client import PreviewClient
from fluent_qt_preview_editor.protocol import editor_tools


VERSIONS = ("2025-11-25", "2025-06-18", "2025-03-26", "2024-11-05")


def tool(name, description, properties=None, required=()):
    return {"name": name, "description": description, "inputSchema": {
        "type": "object", "properties": properties or {},
        "required": list(required), "additionalProperties": False,
    }}


TARGET = {"type": "string", "minLength": 1, "description": "Current inspect id or unique object_name."}
TOOLS = [
    tool("preview_inspect", "Read the real widget tree, bounds, accessible names, data and Inspector findings."),
    tool("preview_screenshot", "Capture the running preview as a PNG image."),
    tool("preview_click", "Click a visible enabled control using Qt input events.", {"target": TARGET}, ("target",)),
    tool("preview_type_text", "Send text input to a visible enabled control.", {
        "target": TARGET, "text": {"type": "string", "minLength": 1, "maxLength": 32768},
    }, ("target", "text")),
    tool("preview_key", "Send a key; modifiers may include shift, control, alt, meta or shortcut.", {
        "target": TARGET, "key": {"type": "string", "minLength": 1},
        "modifiers": {"type": "array", "items": {"type": "string"}},
    }, ("target", "key")),
    tool("preview_set_data", "Stage a scalar data/state value exposed by inspect, without recompilation.", {
        "target": TARGET, "property": {"type": "string", "minLength": 1},
        "value": {"type": ["string", "number", "boolean"]},
    }, ("target", "property", "value")),
    tool("preview_reset_data", "Restore the original values changed through preview_set_data."),
]
TOOLS.extend(editor_tools(tool))


class ProtocolError(Exception):
    def __init__(self, code, message):
        super().__init__(message)
        self.code = code


class PreviewMcpServer:
    def __init__(self, client):
        self.client = client
        self.initialized = False
        self.negotiated = False

    def call_tool(self, name, arguments):
        definition = next((item for item in TOOLS if item["name"] == name), None)
        if definition is None:
            raise ProtocolError(-32602, "Unknown tool: " + str(name))
        schema = definition["inputSchema"]
        if not isinstance(arguments, dict):
            raise ProtocolError(-32602, "Tool arguments must be an object.")
        if set(arguments) - set(schema["properties"]) or set(schema["required"]) - set(arguments):
            raise ProtocolError(-32602, "Missing or unsupported tool arguments.")
        for key, value in arguments.items():
            kind = schema["properties"][key]["type"]
            if kind == "string" and (not isinstance(value, str) or not value or len(value) > 32768):
                raise ProtocolError(-32602, "Expected a non-empty bounded string: " + key)
            if kind == "array" and (not isinstance(value, list) or not all(isinstance(v, str) for v in value)):
                raise ProtocolError(-32602, "Expected an array of strings: " + key)
            if kind == "object" and (not isinstance(value, dict) or not 1 <= len(value) <= 32):
                raise ProtocolError(-32602, "Expected 1 to 32 registered field changes.")
            if isinstance(kind, list) and (value is None or not isinstance(value, (str, int, float, bool))):
                raise ProtocolError(-32602, "Expected a scalar data value.")
            if isinstance(value, float) and not math.isfinite(value):
                raise ProtocolError(-32602, "Data value must be finite.")
        parameters = dict(arguments)
        method = name.removeprefix("preview_")
        if method in ("click", "type_text", "key"):
            parameters["action"] = method
            method = "action"
        try:
            result = self.client.request(method, parameters)
            if name == "preview_screenshot":
                return {"content": [{"type": "image", "mimeType": result["mime_type"],
                                     "data": result["data"]}]}
            if name == "preview_editor_compare":
                content = [{"type": "text", "text": json.dumps(result, ensure_ascii=False)}]
                for caption, key in (("Before", "before_image"), ("After", "after_image")):
                    with Path(result[key]).open("rb") as stream:
                        png = stream.read(20 * 1024 * 1024 + 1)
                    if len(png) > 20 * 1024 * 1024:
                        raise ValueError("Comparison image exceeds 20 MiB.")
                    content.extend([{"type": "text", "text": caption},
                                    {"type": "image", "mimeType": "image/png",
                                     "data": base64.b64encode(png).decode("ascii")}])
                return {"content": content}
            return {"content": [{"type": "text", "text": json.dumps(result, ensure_ascii=False)}]}
        except (OSError, ValueError, RuntimeError) as error:
            return {"isError": True, "content": [{"type": "text", "text": str(error)}]}

    def handle(self, request):
        if not isinstance(request, dict) or request.get("jsonrpc") != "2.0" or not isinstance(request.get("method"), str):
            return {"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "Invalid request."}}
        identity = request.get("id")
        if "id" in request and type(identity) not in (int, str):
            return {"jsonrpc": "2.0", "id": None, "error": {"code": -32600, "message": "Invalid request id."}}
        method = request["method"]
        if "id" not in request:
            if method == "notifications/initialized" and self.negotiated:
                self.initialized = True
            return None
        try:
            parameters = request.get("params", {})
            if not isinstance(parameters, dict):
                raise ProtocolError(-32602, "Parameters must be an object.")
            if method == "initialize":
                version = parameters.get("protocolVersion")
                self.negotiated = True
                result = {"protocolVersion": version if version in VERSIONS else VERSIONS[0],
                          "capabilities": {"tools": {"listChanged": False}},
                          "serverInfo": {"name": "fluentqt-preview", "version": "1.0.0"},
                          "instructions": "Inspect first. Node IDs expire when a preview restarts or a scene reloads."}
            elif method == "ping":
                result = {}
            elif not self.initialized:
                raise ProtocolError(-32002, "Initialize the MCP session first.")
            elif method == "tools/list":
                result = {"tools": TOOLS}
            elif method == "tools/call":
                result = self.call_tool(parameters.get("name"), parameters.get("arguments", {}))
            else:
                raise ProtocolError(-32601, "Method not found.")
            return {"jsonrpc": "2.0", "id": identity, "result": result}
        except ProtocolError as error:
            return {"jsonrpc": "2.0", "id": identity,
                    "error": {"code": error.code, "message": str(error)}}

    def serve(self, incoming, outgoing):
        for line in incoming:
            try:
                if len(line) > 1024 * 1024:
                    raise ValueError("Message is too large")
                reply = self.handle(json.loads(line))
            except (ValueError, TypeError):
                reply = {"jsonrpc": "2.0", "id": None,
                         "error": {"code": -32700, "message": "Invalid JSON message."}}
            if reply is not None:
                outgoing.write(json.dumps(reply, ensure_ascii=False, allow_nan=False) + "\n")
                outgoing.flush()


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session-dir", type=Path, required=True)
    args = parser.parse_args(argv)
    PreviewMcpServer(PreviewClient(args.session_dir)).serve(sys.stdin, sys.stdout)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
