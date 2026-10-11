"""Protocol checks for the preview MCP adapter, including real stdio framing."""

import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

from fluent_qt_preview_mcp import PreviewMcpServer


class FakeClient:
    def __init__(self):
        self.calls = []

    def request(self, method, parameters):
        self.calls.append((method, parameters))
        if method == "screenshot":
            return {"mime_type": "image/png", "data": "iVBORw=="}
        return {"applied": True}


class PreviewMcpTest(unittest.TestCase):
    def setUp(self):
        self.client = FakeClient()
        self.server = PreviewMcpServer(self.client)

    def initialize(self):
        result = self.server.handle({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                                     "params": {"protocolVersion": "2025-11-25"}})
        self.assertEqual(result["result"]["protocolVersion"], "2025-11-25")
        self.server.handle({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def test_requires_initialization_and_validates_arguments_before_mutation(self):
        request = {"jsonrpc": "2.0", "id": 2, "method": "tools/list"}
        self.assertEqual(self.server.handle(request)["error"]["code"], -32002)
        self.initialize()
        names = {tool["name"] for tool in self.server.handle(request)["result"]["tools"]}
        self.assertEqual(len(names), 15)
        self.assertIn("preview_editor_apply", names)
        for args in ({}, {"target": "button", "code": "arbitrary"}, {"target": 1}):
            result = self.server.handle({"jsonrpc": "2.0", "id": 3, "method": "tools/call",
                                        "params": {"name": "preview_click", "arguments": args}})
            self.assertIn("error", result)
        self.assertEqual(self.client.calls, [])

    def test_dispatches_actions_and_returns_image_content(self):
        self.initialize()
        self.server.call_tool("preview_click", {"target": "save"})
        self.assertEqual(self.client.calls, [("action", {"target": "save", "action": "click"})])
        result = self.server.call_tool("preview_screenshot", {})
        self.assertEqual(result["content"][0]["type"], "image")
        self.assertEqual(result["content"][0]["mimeType"], "image/png")

    def test_editor_dispatch_keeps_revision_and_rejects_invalid_change_objects(self):
        self.initialize()
        arguments = {"region": "notifications", "revision": "source-revision", "changes": {"padding": 8}}
        self.server.call_tool("preview_editor_apply", arguments)
        self.assertEqual(self.client.calls, [("editor_apply", arguments)])
        for changes in ([], {}, {str(i): i for i in range(33)}):
            from fluent_qt_preview_mcp import ProtocolError
            with self.assertRaises(ProtocolError):
                self.server.call_tool("preview_editor_apply", {**arguments, "changes": changes})

    def test_stdio_is_json_only_and_disconnected_ui_is_a_tool_error(self):
        with tempfile.TemporaryDirectory() as directory:
            messages = [
                {"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {"protocolVersion": "2025-11-25"}},
                {"jsonrpc": "2.0", "method": "notifications/initialized"},
                {"jsonrpc": "2.0", "id": 2, "method": "tools/list"},
                {"jsonrpc": "2.0", "id": 3, "method": "tools/call", "params": {"name": "preview_inspect"}},
            ]
            process = subprocess.run([sys.executable, str(Path(__file__).with_name("fluent_qt_preview_mcp.py")),
                                      "--session-dir", directory],
                                     input="\n".join(json.dumps(m) for m in messages) + "\n",
                                     text=True, capture_output=True, timeout=5)
            self.assertEqual(process.returncode, 0, process.stderr)
            replies = [json.loads(line) for line in process.stdout.splitlines()]
            self.assertEqual([r["id"] for r in replies], [1, 2, 3])
            self.assertTrue(replies[2]["result"]["isError"])

    def test_bad_json_does_not_terminate_the_server(self):
        outgoing = io.StringIO()
        self.server.serve(io.StringIO('broken\n{"jsonrpc":"2.0","id":1,"method":"ping"}\n'), outgoing)
        replies = [json.loads(line) for line in outgoing.getvalue().splitlines()]
        self.assertEqual(replies[0]["error"]["code"], -32700)
        self.assertEqual(replies[1]["result"], {})


if __name__ == "__main__":
    unittest.main()
