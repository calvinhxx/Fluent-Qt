#!/usr/bin/env python3
"""Inspect, operate and stage data in a running local FluentQt preview."""

from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path
import sys
import time
import uuid


def write_json(path: Path, value: dict) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name("." + path.name + "." + uuid.uuid4().hex)
    try:
        temporary.write_text(json.dumps(value, ensure_ascii=False, allow_nan=False) + "\n", encoding="utf-8")
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


class PreviewClient:
    def __init__(self, directory: Path):
        self.directory = Path(directory).expanduser().resolve()

    def metadata(self) -> dict:
        try:
            metadata = json.loads((self.directory / "session.json").read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            raise RuntimeError("Preview is not ready. Launch it with --session-dir first.") from error
        if not isinstance(metadata, dict) or metadata.get("status") != "ready" or not metadata.get("session_id"):
            raise RuntimeError("Preview is closed or restarting.")
        return metadata

    def wait_ready(self, timeout: float = 15, *, after: str | None = None, process=None) -> dict:
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            if process is not None and process.poll() is not None:
                raise RuntimeError(
                    f"Preview process exited with code {process.returncode} before becoming ready."
                )
            try:
                metadata = self.metadata()
                if metadata.get("session_id") != after and (
                    process is None or metadata.get("pid") == process.pid
                ):
                    return metadata
            except RuntimeError:
                pass
            time.sleep(0.05)
        raise RuntimeError(f"Preview did not become ready within {timeout:g} seconds.")

    def request(self, method: str, parameters: dict | None = None, timeout: float = 10) -> dict:
        if parameters is not None and not isinstance(parameters, dict):
            raise ValueError("Parameters must be an object.")
        metadata = self.metadata()
        identity = uuid.uuid4().hex
        request = self.directory / "requests" / (identity + ".json")
        response = self.directory / "responses" / (identity + ".json")
        payload = {"id": identity, "session_id": metadata["session_id"],
                   "method": method, "parameters": parameters or {}}
        if len(json.dumps(payload).encode("utf-8")) > 256 * 1024:
            raise ValueError("Preview request exceeds 256 KiB.")
        write_json(request, payload)
        deadline = time.monotonic() + timeout
        try:
            while time.monotonic() < deadline:
                if response.is_file():
                    reply = json.loads(response.read_text(encoding="utf-8"))
                    if reply.get("id") != identity or reply.get("session_id") != metadata["session_id"]:
                        raise RuntimeError("Session changed; inspect the preview again.")
                    result = reply["result"]
                    if "error" in result:
                        raise RuntimeError(json.dumps(result, ensure_ascii=False))
                    return result
                current = self.metadata()
                if current["session_id"] != metadata["session_id"]:
                    raise RuntimeError("Preview restarted; inspect it again before editing.")
                time.sleep(0.025)
            raise RuntimeError("Preview command timed out; check that its window is still running.")
        finally:
            request.unlink(missing_ok=True)
            response.unlink(missing_ok=True)


def parse_value(value: str):
    try:
        return json.loads(value)
    except ValueError:
        return value


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--session-dir", required=True, type=Path)
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("inspect", help="Read the widget tree and diagnostics.")
    commands.add_parser("data", help="List editable preview data and state.")
    commands.add_parser("reset-data", help="Restore values changed by set-data.")
    snapshot = commands.add_parser("screenshot", help="Capture the real preview window.")
    snapshot.add_argument("--output", type=Path, required=True)
    action = commands.add_parser("action", help="Send a click, key or text input.")
    action.add_argument("--target", required=True)
    action.add_argument("--action", choices=("click", "focus", "type_text", "key"), required=True)
    action.add_argument("--text")
    action.add_argument("--key")
    action.add_argument("--modifier", action="append", default=[])
    setting = commands.add_parser("set-data", help="Change a data/state property without rebuilding.")
    setting.add_argument("--target", required=True)
    setting.add_argument("--property", required=True)
    setting.add_argument("--value", type=parse_value, required=True)
    args = parser.parse_args(argv)
    client = PreviewClient(args.session_dir)
    try:
        if args.command == "screenshot":
            result = client.request("screenshot")
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_bytes(base64.b64decode(result["data"], validate=True))
            print(args.output.resolve())
            return 0
        parameters = {}
        if args.command == "set-data":
            parameters = {"target": args.target, "property": args.property, "value": args.value}
        elif args.command == "action":
            parameters = {"target": args.target, "action": args.action,
                          "modifiers": args.modifier}
            if args.text is not None:
                parameters["text"] = args.text
            if args.key is not None:
                parameters["key"] = args.key
        result = client.request(args.command.replace("-", "_"), parameters)
        if args.command == "data":
            result = {"session_id": result["session_id"], "controls": [
                {"id": node["id"], "object_name": node["object_name"], "data": node["data"]}
                for node in result["nodes"] if node.get("data")
            ]}
        print(json.dumps(result, indent=2, ensure_ascii=False))
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print("preview: " + str(error), file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
