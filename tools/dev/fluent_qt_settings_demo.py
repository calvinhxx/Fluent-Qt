#!/usr/bin/env python3
"""Show a real settings page and replay inspect/action, hot reload, and data edits."""

from __future__ import annotations

import argparse
import base64
import json
from pathlib import Path
import shutil
import subprocess
import sys
import time

from fluent_qt_preview_client import PreviewClient, write_json


ROOT = Path(__file__).resolve().parents[2]
TOOLS = Path(__file__).resolve().parent
SCENE = TOOLS / "scenes" / "settings_page.preview.py"


class Demo:
    def __init__(self, output, runtime_arguments):
        self.output = output
        self.output.mkdir(parents=True, exist_ok=True)
        self.scene = output / "settings.preview.py"
        self.source = SCENE.read_text(encoding="utf-8")
        self.client = PreviewClient(output / "live")
        try:
            self.previous_session = self.client.metadata()["session_id"]
        except RuntimeError:
            self.previous_session = None
        try:
            self.client.request("inspect", timeout=0.4)
        except (RuntimeError, OSError):
            pass
        else:
            raise RuntimeError("This demo is already open. Close it or choose another --output directory.")
        for frame in (output / "frames").glob("[0-9][0-9][0-9][0-9].png"):
            frame.unlink()
        self.scene.write_text(self.source.replace("SIDEBAR_WIDTH = 184", "SIDEBAR_WIDTH = 248"), encoding="utf-8")
        self.log = (output / "host.log").open("w", encoding="utf-8")
        self.host = subprocess.Popen([
            sys.executable, str(TOOLS / "fluent_qt_live_preview.py"),
            "--scene", str(self.scene), "--session-dir", str(self.client.directory),
            "--content-only", "--size", "980x660", "--report", str(output / "host-report.json"),
            *runtime_arguments,
        ], cwd=ROOT, stdout=self.log, stderr=self.log, start_new_session=True)
        self.server = None
        self.sequence = 0
        self.frames = 0
        self.transcript = []

    def start(self):
        metadata = self.client.wait_ready(after=self.previous_session)
        self.pid = metadata["pid"]
        self.server = subprocess.Popen([
            sys.executable, str(TOOLS / "fluent_qt_preview_mcp.py"),
            "--session-dir", str(self.client.directory),
        ], stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=self.log, text=True)
        self.rpc("initialize", {"protocolVersion": "2025-11-25", "capabilities": {},
                                "clientInfo": {"name": "settings-demo", "version": "1"}})
        self.server.stdin.write(json.dumps({"jsonrpc": "2.0", "method": "notifications/initialized"}) + "\n")
        self.server.stdin.flush()
        self.rpc("tools/list")

    def rpc(self, method, parameters=None):
        self.sequence += 1
        message = {"jsonrpc": "2.0", "id": self.sequence, "method": method, "params": parameters or {}}
        self.server.stdin.write(json.dumps(message, ensure_ascii=False) + "\n")
        self.server.stdin.flush()
        line = self.server.stdout.readline()
        if not line:
            raise RuntimeError("MCP adapter stopped; see host.log")
        result = json.loads(line)
        if "error" in result:
            raise RuntimeError(str(result["error"]))
        return result["result"]

    def call(self, tool, **arguments):
        result = self.rpc("tools/call", {"name": tool, "arguments": arguments})
        if result.get("isError"):
            raise RuntimeError(str(result))
        if tool != "preview_screenshot":
            self.transcript.append({"tool": tool, "arguments": arguments})
        return result["content"][0]

    def inspect(self):
        return json.loads(self.call("preview_inspect")["text"])

    def node(self, name):
        return next(node for node in self.inspect()["nodes"] if node["object_name"] == name)

    def click(self, name):
        target = self.node(name)
        self.call("preview_click", target=target["id"])

    def data(self, name, property_name, value):
        target = self.node(name)
        self.call("preview_set_data", target=target["id"], property=property_name, value=value)

    def capture(self, filename=None):
        payload = self.call("preview_screenshot")
        png = base64.b64decode(payload["data"], validate=True)
        frame = self.output / "frames" / f"{self.frames:04d}.png"
        frame.parent.mkdir(exist_ok=True)
        frame.write_bytes(png)
        self.frames += 1
        if filename:
            (self.output / filename).write_bytes(png)

    def hold(self, seconds, filename=None):
        for index in range(round(seconds * 6)):
            start = time.monotonic()
            self.capture(filename if index == 0 else None)
            time.sleep(max(0, 1 / 6 - (time.monotonic() - start)))

    def run(self):
        self.start()
        report = self.inspect()
        write_json(self.output / "initial-tree.json", report)
        print("1/3 读取真实界面，然后点击、输入、切换主题和通知。", flush=True)
        self.hold(2, "before.png")
        self.click("workspaceName")
        self.click("fluentLineEditClearButton")
        self.call("preview_type_text", target=self.node("workspaceName")["id"], text="设计工作室")
        assert self.node("workspaceName")["data"]["text"]["value"] == "设计工作室"
        self.hold(2)
        self.click("navAppearance")
        self.click("darkAppearance")
        self.hold(3, "dark.png")
        self.click("darkAppearance")
        self.click("navNotifications")
        self.click("allowNotifications")
        assert not self.node("notificationSound")["enabled"]
        assert not self.node("testNotification")["enabled"]
        self.hold(3, "disabled.png")
        self.click("allowNotifications")
        self.click("navGeneral")

        print("2/3 保存布局修改：侧栏收窄，原窗口和已输入的名称保留。", flush=True)
        self.hold(2, "before-reload.png")
        previous = self.client.metadata()["session_id"]
        self.scene.write_text(self.source, encoding="utf-8")
        self.client.wait_ready(after=previous)
        assert self.client.metadata()["pid"] == self.pid
        assert self.node("workspaceName")["data"]["text"]["value"] == "设计工作室"
        assert self.node("settingsSidebar")["rect"]["width"] == 184
        self.hold(4, "after-reload.png")

        print("3/3 直接换成长文字和只读状态，源码及窗口进程保持不变。", flush=True)
        before = self.client.metadata()["session_id"]
        self.data("workspaceName", "text", "产品设计与开发协作工作空间")
        self.data("workspaceName", "readOnly", True)
        self.data("workspaceNameDescription", "text", "此工作空间由团队统一管理，名称暂时不可修改。")
        self.data("checkUpdates", "isOn", False)
        self.data("checkUpdatesDescription", "text", "当前处于离线状态；恢复连接后，可以重新开启版本更新提醒。")
        assert self.client.metadata()["session_id"] == before
        assert self.scene.read_text(encoding="utf-8") == self.source
        self.hold(5, "long-data.png")
        self.call("preview_reset_data")
        self.hold(3, "settings-final.png")
        report = self.inspect()
        write_json(self.output / "result.json", {
            "pid": self.pid, "scene": str(self.scene), "session_dir": str(self.client.directory),
            "real_mcp_actions": True, "same_process_reload": True, "input_preserved": True,
            "data_changed_without_reload": True, "preview_data_restored": True,
            "inspector": report["inspector"], "tool_calls": self.transcript,
            "frames": self.frames, "fps": 6,
        })
        self.server.stdin.close()
        self.server.wait(timeout=5)
        ffmpeg = shutil.which("ffmpeg")
        if ffmpeg:
            video = self.output / "settings-demo.mp4"
            subprocess.run([ffmpeg, "-y", "-loglevel", "error", "-framerate", "6",
                            "-i", str(self.output / "frames" / "%04d.png"),
                            "-vf", "scale=980:-2", "-c:v", "libx264", "-crf", "18",
                            "-pix_fmt", "yuv420p", "-r", "12", "-movflags", "+faststart", str(video)], check=True)
            print("演示回放：" + str(video), flush=True)
        print("设置页保持打开；可直接编辑：" + str(self.scene), flush=True)

    def close_on_error(self):
        for process in (self.server, self.host):
            if process is not None and process.poll() is None:
                process.terminate()
                process.wait(timeout=5)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "build/preview/settings-demo")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--python", type=Path)
    args = parser.parse_args()
    runtime = []
    if args.build_dir:
        runtime.extend(["--build-dir", str(args.build_dir.resolve())])
    if args.python:
        runtime.extend(["--python", str(args.python.absolute())])
    demo = Demo(args.output.resolve(), runtime)
    try:
        demo.run()
    except BaseException:
        demo.close_on_error()
        raise
    finally:
        demo.log.close()


if __name__ == "__main__":
    main()
