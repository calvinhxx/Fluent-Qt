"""Exercise build failure/recovery across real watcher and fixture processes."""

import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
import unittest

from fluent_qt_preview_client import PreviewClient, write_json
from fluent_qt_preview_watch import fingerprint, watcher_lock


TOOLS = Path(__file__).resolve().parent

HOST = '''
import os, sys, time, uuid
from pathlib import Path
from fluent_qt_preview_client import write_json
directory = Path(sys.argv[1])
session = uuid.uuid4().hex
(directory / "requests").mkdir(parents=True, exist_ok=True)
write_json(directory / "session.json", {"status": "ready", "session_id": session, "pid": os.getpid()})
while True:
    for path in (directory / "requests").glob("*.json"):
        import json
        request = json.loads(path.read_text())
        path.unlink()
        method = request["method"]
        result = {"widgets": [{"values": {"text": "retained"}}]} if method == "capture_state" else {}
        if method == "restore_state":
            assert request["parameters"]["widgets"][0]["values"]["text"] == "retained"
            result = {"restored": 1}
        write_json(directory / "responses" / path.name, {"id": request["id"], "session_id": session, "result": result})
    time.sleep(.02)
'''


@unittest.skipUnless(os.name == "posix", "SIGINT process cleanup check uses POSIX signals")
class PreviewWatchTest(unittest.TestCase):
    def test_failed_build_keeps_child_and_success_restarts_with_state(self):
        with tempfile.TemporaryDirectory() as temporary:
            folder = Path(temporary)
            host = folder / "host.py"
            host.write_text(HOST, encoding="utf-8")
            source = folder / "sample.cpp"
            source.write_text("valid", encoding="utf-8")
            build = folder / "build.py"
            build.write_text("from pathlib import Path\nimport sys\nraise SystemExit(2 if Path(sys.argv[1]).read_text() == 'broken' else 0)\n", encoding="utf-8")
            driver = folder / "driver.py"
            directory = folder / "session"
            driver.write_text(
                "from fluent_qt_preview_watch import run_watch\nraise SystemExit(run_watch(" +
                repr([sys.executable, str(host), str(directory)]) + "," +
                repr([sys.executable, str(build), str(source)]) + "," +
                repr(str(directory)) + "," + repr([str(source)]) + "," + repr(str(folder)) + "))\n",
                encoding="utf-8",
            )
            environment = dict(os.environ, PYTHONPATH=str(TOOLS))
            log_path = folder / "watch.log"
            with log_path.open("w") as log:
                process = subprocess.Popen([sys.executable, str(driver)], env=environment, stdout=log, stderr=log)

                def wait_status(state, generation):
                    deadline = time.monotonic() + 10
                    while time.monotonic() < deadline:
                        path = directory / "watch.json"
                        if path.exists():
                            status = json.loads(path.read_text())
                            if status["state"] == state and status["generation"] == generation:
                                return status
                        if process.poll() is not None:
                            break
                        time.sleep(.05)
                    self.fail(log_path.read_text())

                try:
                    wait_status("ready", 1)
                    first = json.loads((directory / "session.json").read_text())
                    source.write_text("broken", encoding="utf-8")
                    wait_status("build_failed", 1)
                    self.assertEqual(json.loads((directory / "session.json").read_text()), first)
                    os.kill(first["pid"], 0)
                    source.write_text("fixed", encoding="utf-8")
                    status = wait_status("ready", 2)
                    second = json.loads((directory / "session.json").read_text())
                    self.assertNotEqual(second["pid"], first["pid"])
                    self.assertEqual(status["restored_values"], 1)
                finally:
                    process.send_signal(signal.SIGINT)
                    process.wait(timeout=10)
                wait_status("stopped", 2)
                with self.assertRaises(ProcessLookupError):
                    os.kill(second["pid"], 0)


class PreviewWatchBoundariesTest(unittest.TestCase):
    def test_scanning_tracks_create_replace_delete_and_ignores_caches(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "page.cpp"
            source.write_text("old", encoding="utf-8")
            for folder in (".git", "__pycache__", "node_modules", "CMakeFiles"):
                (root / folder).mkdir()
                (root / folder / "generated.cpp").write_text("ignore", encoding="utf-8")
            before = fingerprint([root])
            self.assertEqual(set(before), {str(source.resolve())})
            replacement = root / "replacement.cpp"
            replacement.write_text("new", encoding="utf-8")
            os.utime(replacement, ns=(source.stat().st_atime_ns, source.stat().st_mtime_ns))
            replacement.replace(source)
            self.assertNotEqual(fingerprint([root]), before)
            source.unlink()
            self.assertEqual(fingerprint([root]), {})

    def test_watcher_directory_is_exclusive_and_unlocks_after_exit(self):
        with tempfile.TemporaryDirectory() as temporary:
            command = [sys.executable, "-c",
                       "from fluent_qt_preview_watch import watcher_lock\n"
                       f"with watcher_lock({temporary!r}): print('acquired')\n"]
            environment = dict(os.environ, PYTHONPATH=str(TOOLS))
            with watcher_lock(temporary):
                busy = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=5)
                self.assertNotEqual(busy.returncode, 0)
                self.assertIn("Another watcher", busy.stderr)
            available = subprocess.run(command, env=environment, capture_output=True, text=True, timeout=5)
            self.assertEqual(available.returncode, 0, available.stderr)
            self.assertEqual(available.stdout.strip(), "acquired")

    def test_wait_ready_rejects_another_process_and_reports_early_exit(self):
        with tempfile.TemporaryDirectory() as temporary:
            client = PreviewClient(Path(temporary))
            write_json(client.directory / "session.json", {
                "status": "ready", "session_id": "another-preview", "pid": os.getpid(),
            })
            child = subprocess.Popen([sys.executable, "-c", "raise SystemExit(7)"])
            try:
                with self.assertRaisesRegex(RuntimeError, "exited with code 7"):
                    client.wait_ready(timeout=2, process=child)
            finally:
                child.wait(timeout=5)

    def test_wait_ready_obeys_timeout_without_killing_the_owned_process(self):
        with tempfile.TemporaryDirectory() as temporary:
            client = PreviewClient(Path(temporary))
            code = (
                "import os,time\nfrom pathlib import Path\n"
                "from fluent_qt_preview_client import write_json\n"
                "time.sleep(.3)\n"
                f"write_json(Path({str(client.directory / 'session.json')!r}), "
                "{'status':'ready','session_id':'new','pid':os.getpid()})\n"
                "time.sleep(5)\n"
            )
            child = subprocess.Popen([sys.executable, "-c", code], env=dict(os.environ, PYTHONPATH=str(TOOLS)))
            try:
                with self.assertRaisesRegex(RuntimeError, "0.05 seconds"):
                    client.wait_ready(timeout=.05, process=child)
                self.assertIsNone(child.poll())
                self.assertEqual(client.wait_ready(timeout=2, process=child)["pid"], child.pid)
            finally:
                child.terminate()
                child.wait(timeout=5)


if __name__ == "__main__":
    unittest.main()
