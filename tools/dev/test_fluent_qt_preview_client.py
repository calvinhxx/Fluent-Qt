"""Keep pending preview requests safe across transient file sharing failures."""

import json
from pathlib import Path
from tempfile import TemporaryDirectory
import unittest
from unittest.mock import patch

from fluent_qt_preview_client import PreviewClient, write_json


class PreviewClientTest(unittest.TestCase):
    def setUp(self):
        temporary = TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.client = PreviewClient(Path(temporary.name))
        self.metadata = {"status": "ready", "session_id": "original", "pid": 42}
        self.metadata_path = self.client.directory / "session.json"
        write_json(self.metadata_path, self.metadata)

    def test_sharing_failure_retries_reads_without_resending_the_edit(self):
        original_read = Path.read_text
        for locked_file in ("session", "response"):
            with self.subTest(locked_file=locked_file):
                write_json(self.metadata_path, self.metadata)
                failures = []
                handled = []

                def read(path, *args, **kwargs):
                    pending = list((self.client.directory / "requests").glob("*.json"))
                    selected = path == self.metadata_path if locked_file == "session" else path.parent.name == "responses"
                    if pending and selected and not failures:
                        failures.append(path)
                        raise PermissionError("file is being replaced")
                    return original_read(path, *args, **kwargs)

                def host(_delay):
                    if handled:
                        return
                    request = next((self.client.directory / "requests").glob("*.json"))
                    payload = json.loads(original_read(request))
                    handled.append(payload)
                    write_json(self.metadata_path, dict(self.metadata, session_id="reloaded"))
                    write_json(self.client.directory / "responses" / request.name,
                               {"id": payload["id"], "session_id": payload["session_id"],
                                "result": {"pending": "edit-1"}})

                with patch.object(Path, "read_text", read), \
                        patch("fluent_qt_preview_client.time.sleep", side_effect=host), \
                        patch("fluent_qt_preview_client.write_json", wraps=write_json) as send:
                    self.assertEqual(self.client.request("editor_apply"), {"pending": "edit-1"})
                self.assertEqual(send.call_count, 1)
                self.assertEqual(len(handled), 1)
                self.assertEqual(len(failures), 1)
                self.assertEqual(list((self.client.directory / "requests").glob("*.json")), [])
                self.assertEqual(list((self.client.directory / "responses").glob("*.json")), [])

    def test_persistent_sharing_failure_obeys_timeout_and_cleans_request(self):
        original_read = Path.read_text

        def read(path, *args, **kwargs):
            if path == self.metadata_path and list((self.client.directory / "requests").glob("*.json")):
                raise PermissionError("file remains locked")
            return original_read(path, *args, **kwargs)

        with patch.object(Path, "read_text", read), \
                patch("fluent_qt_preview_client.time.monotonic", side_effect=[0, 0.01, 0.02, 0.2]), \
                patch("fluent_qt_preview_client.time.sleep"), \
                patch("fluent_qt_preview_client.write_json", wraps=write_json) as send:
            with self.assertRaisesRegex(RuntimeError, "command timed out"):
                self.client.request("editor_apply", timeout=0.1)
        self.assertEqual(send.call_count, 1)
        self.assertEqual(list((self.client.directory / "requests").glob("*.json")), [])

    def test_closed_and_replaced_sessions_still_fail_immediately(self):
        for current, message in ((dict(self.metadata, status="closed"), "closed"),
                                 (dict(self.metadata, session_id="other", pid=99), "restarted")):
            with self.subTest(current=current), \
                    patch.object(self.client, "metadata", wraps=self.client.metadata) as metadata, \
                    patch("fluent_qt_preview_client.time.sleep") as sleep:
                write_json(self.metadata_path, self.metadata)

                def send(path, payload):
                    write_json(path, payload)
                    write_json(self.metadata_path, current)

                with patch("fluent_qt_preview_client.write_json", side_effect=send):
                    with self.assertRaisesRegex(RuntimeError, message):
                        self.client.request("editor_apply")
                self.assertEqual(metadata.call_count, 2)
                sleep.assert_not_called()


if __name__ == "__main__":
    unittest.main()
