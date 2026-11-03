"""Portable contracts for the incremental WSL source snapshot."""

import importlib.util
import json
from contextlib import nullcontext
from pathlib import Path
import subprocess
import tempfile
import unittest
import sys
from types import SimpleNamespace
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("wsl", Path(__file__).with_name("fluent_qt_wsl.py"))
WSL = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(WSL)


class EnvironmentTest(unittest.TestCase):
    def test_inherited_windows_paths_do_not_leak_into_linux_tool_search(self):
        inherited = {"PATH": "/home/user/bin:/mnt/c/Windows/System32:/usr/bin:/mnt/D/tools:/mnt/wslg:/usr/lib/wsl/lib",
                     "DISPLAY": ":0"}
        original = dict(inherited)
        result = WSL.linux_environment(inherited, {}, {})
        self.assertEqual(result["PATH"], "/home/user/bin:/usr/bin:/mnt/wslg:/usr/lib/wsl/lib")
        self.assertEqual(result["DISPLAY"], ":0")
        self.assertEqual(inherited, original)

    def test_explicit_environment_overrides_keep_their_priority(self):
        result = WSL.linux_environment({"PATH": "/usr/bin:/mnt/c/Windows"},
                                      {"environment": {"PATH": "/opt/shared/bin", "KIT": "shared"}},
                                      {"environment": {"PATH": "/opt/qt/bin:/usr/bin", "KIT": "profile"}})
        self.assertEqual(result, {"PATH": "/opt/qt/bin:/usr/bin", "KIT": "profile"})


class SnapshotTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "checkout"
        self.source.mkdir()
        subprocess.run(["git", "init", "-q", str(self.source)], check=True)
        self.target = self.root / "cache/source"
        self.state = self.root / "cache/source-index.json"
        self.put("CMakeLists.txt", "project(example)")
        self.put("tools/dev/fluent_qt_build.py", "print('build')")
        self.put(".gitignore", "build/\n")
        self.put("build/ignored", "large artifact")

    def put(self, name, text):
        path = self.source / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding="utf-8")

    def sync(self, **kwargs):
        return WSL.sync_source(self.source, self.target, self.state, **kwargs)

    def test_noop_and_one_file_edit_keep_other_timestamps(self):
        self.put("src/control.cpp", "first")
        self.sync()
        self.assertFalse((self.target / "build").exists())
        before = {p: p.stat().st_mtime_ns for p in self.target.rglob("*") if p.is_file()}
        self.assertEqual(self.sync()["copied"], [])
        self.assertEqual(before, {p: p.stat().st_mtime_ns for p in before})
        self.put("src/control.cpp", "new source")
        self.assertEqual(self.sync()["copied"], ["src/control.cpp"])
        self.assertEqual((self.target / "CMakeLists.txt").stat().st_mtime_ns, before[self.target / "CMakeLists.txt"])

    def test_tracked_delete_and_untracked_add_are_synchronized(self):
        self.put("old.cpp", "old")
        subprocess.run(["git", "-C", str(self.source), "add", "old.cpp"], check=True)
        self.sync()
        (self.target.parent / "local-note").write_text("not managed")
        (self.source / "old.cpp").unlink()
        self.put("new.cpp", "not committed yet")
        result = self.sync()
        self.assertEqual(result["removed"], ["old.cpp"])
        self.assertEqual(result["copied"], ["new.cpp"])
        self.assertTrue((self.target.parent / "local-note").exists())

    def test_unmanaged_snapshot_inputs_fail_before_any_source_is_changed(self):
        self.put("src/control.cpp", "original")
        self.sync()
        self.put("src/control.cpp", "updated")
        for name in ("src/extra.cpp", "include/extra.h", "cmake/extra.cmake",
                     "res/extra.qrc", "local-note", "tools/__pycache__/extra.py"):
            with self.subTest(name=name):
                extra = self.target / name
                extra.parent.mkdir(parents=True, exist_ok=True)
                extra.write_text("keep this file")
                before = self.state.read_bytes()
                with self.assertRaisesRegex(ValueError, "Unmanaged snapshot file"):
                    self.sync()
                self.assertEqual(extra.read_text(), "keep this file")
                self.assertEqual((self.target / "src/control.cpp").read_text(), "original")
                self.assertEqual(self.state.read_bytes(), before)
                extra.unlink()

    def test_snapshot_tool_bytecode_does_not_block_reuse(self):
        self.sync()
        cached = self.target / "tools/dev/__pycache__/fluent_qt_build.cpython-312.pyc"
        cached.parent.mkdir()
        cached.write_bytes(b"runtime cache")
        self.assertEqual(self.sync()["copied"], [])
        self.assertEqual(cached.read_bytes(), b"runtime cache")

    def test_direct_snapshot_edits_are_not_overwritten(self):
        self.sync()
        (self.target / "CMakeLists.txt").write_text("another agent's edit")
        with self.assertRaisesRegex(ValueError, "edited directly"):
            self.sync()
        self.assertEqual((self.target / "CMakeLists.txt").read_text(), "another agent's edit")

    def test_adoption_requires_identical_existing_files(self):
        self.target.mkdir(parents=True)
        (self.target / "CMakeLists.txt").write_text("project(example)")
        with self.assertRaisesRegex(ValueError, "adopt-source"):
            self.sync()
        self.sync(adopt=True)
        self.assertTrue(self.state.exists())

    def test_changed_unmanaged_file_rejects_adoption(self):
        self.target.mkdir(parents=True)
        (self.target / "CMakeLists.txt").write_text("different project")
        with self.assertRaisesRegex(ValueError, "Cannot adopt"):
            self.sync(adopt=True)

    def test_manifest_cannot_delete_outside_snapshot(self):
        self.sync()
        marker = self.root / "keep"
        marker.write_text("protected")
        data = json.loads(self.state.read_text())
        data["files"]["../../keep"] = {"sha256": WSL.digest(marker), "executable": False}
        self.state.write_text(json.dumps(data))
        with self.assertRaises(ValueError):
            self.sync()
        self.assertEqual(marker.read_text(), "protected")

    def test_sync_cannot_target_source_or_git_checkout(self):
        with self.assertRaisesRegex(ValueError, "overlap"):
            WSL.sync_source(self.source, self.source / "build/cache", self.state)
        self.target.mkdir(parents=True)
        (self.target / ".git").mkdir()
        with self.assertRaisesRegex(ValueError, "Git checkout"):
            self.sync()

    def test_cache_reuse_checks_runtime_and_rejects_a_different_kit(self):
        build = self.root / "build"
        build.mkdir()
        (build / "CMakeCache.txt").write_text("CMAKE_GENERATOR:INTERNAL=Ninja\n"
                                            "FLUENT_QT_DEBUG_RELEASE_RUNTIME:BOOL=ON\nQT_DIR:PATH=/qt6\n")
        self.assertTrue(WSL.cache_matches(build, ["-DQT_DIR=/qt6"]))
        self.assertFalse(WSL.cache_matches(build, ["-DFLUENT_QT_DEBUG_RELEASE_RUNTIME=OFF"]))
        with self.assertRaisesRegex(ValueError, "mixing toolchains"):
            WSL.cache_matches(build, ["-DQT_DIR=/qt5"])

    def test_worker_rejects_kit_changes_before_every_reconfigure_path(self):
        synced = self.sync()
        workspace = self.target.parent
        build = workspace / "build/qt6"
        build.mkdir(parents=True)
        stamp = workspace / "profiles/qt6.json"
        for key in ("QT_DIR", "CMAKE_CXX_COMPILER", "CMAKE_TOOLCHAIN_FILE"):
            for reason in ("changed-signature", "forced-configure", "missing-stamp"):
                with self.subTest(key=key, reason=reason):
                    (build / "CMakeCache.txt").write_text(
                        f"CMAKE_HOME_DIRECTORY:INTERNAL={self.target.resolve()}\n"
                        "CMAKE_GENERATOR:INTERNAL=Ninja\n"
                        f"FLUENT_QT_DEBUG_RELEASE_RUNTIME:BOOL=ON\n{key}:PATH=/old-kit\n")
                    if reason == "missing-stamp":
                        stamp.unlink(missing_ok=True)
                    else:
                        WSL.write_json(stamp, {"command": ["old configuration"]})
                    args = SimpleNamespace(project_root=self.source, profile="qt6", inventory=None,
                                           adopt_source=False, sync_only=False, configure_only=True,
                                           configure=reason == "forced-configure", run_id=f"{key}-{reason}")
                    config = {"workspace": str(workspace),
                              "profiles": {"qt6": {"cmake_args": [f"-D{key}=/new-kit"]}}}
                    with patch.object(WSL.sys, "platform", "linux"), \
                            patch.object(WSL, "workspace_lock", return_value=nullcontext()), \
                            patch.object(WSL, "sync_source", return_value=synced), \
                            patch.object(WSL.subprocess, "run") as run:
                        self.assertEqual(WSL.run_linux(args, config), 1)
                    run.assert_not_called()
                    report = json.loads((workspace / "last-run.json").read_text())
                    self.assertEqual(report["status"], "failed")
                    self.assertIn("mixing toolchains", report["error"])

    def test_worker_places_native_evidence_outside_the_snapshot(self):
        synced = self.sync()
        workspace = self.target.parent
        args = SimpleNamespace(project_root=self.source, profile="qt6", inventory=None,
                               adopt_source=False, sync_only=False, configure_only=True,
                               configure=False, run_id="native-evidence")
        config = {"workspace": str(workspace), "profiles": {"qt6": {}}}
        with patch.object(WSL.sys, "platform", "linux"), \
                patch.object(WSL, "workspace_lock", return_value=nullcontext()), \
                patch.object(WSL, "sync_source", return_value=synced), \
                patch.object(WSL, "inventory", return_value=synced["files"]), \
                patch.dict(WSL.os.environ, {}, clear=True), \
                patch.object(WSL.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)) as run:
            self.assertEqual(WSL.run_linux(args, config), 0)
        evidence = Path(run.call_args.kwargs["env"]["FLUENTQT_QMLBRIDGE_NATIVE_EVIDENCE_DIR"])
        self.assertEqual(evidence.resolve(), (workspace / "logs/native-evidence/qmlbridge-native").resolve())
        self.assertFalse(evidence.resolve().is_relative_to(self.target.resolve()))

    def test_launcher_rejects_source_changes_even_when_worker_passes(self):
        args = SimpleNamespace(project_root=self.source, config=self.source / "settings.json", profile="qt6")
        config = {"distribution": "fixture", "workspace": "/workspace"}

        def edit_during_run(_command):
            self.put("CMakeLists.txt", "changed while the snapshot was being tested")
            return 0

        with patch.object(WSL.subprocess, "call", side_effect=edit_during_run):
            self.assertEqual(WSL.run_windows(args, config, ["--profile", "qt6", "--sync-only"]), 1)
        report = json.loads((self.source / "build/wsl-validation/last-run.json").read_text())
        self.assertFalse(report["source_unchanged"])
        self.assertEqual(report["status"], "failed")
        self.assertEqual(report["worker_report"], "/workspace/logs/" + report["run_id"] + "/run.json")

    def test_supplied_inventory_does_not_rescan_the_mounted_checkout(self):
        files = WSL.inventory(self.source)
        with patch.object(WSL, "inventory", side_effect=AssertionError("cross-mount scan")):
            WSL.sync_source(self.source, self.target, self.state, files=files)
        with self.assertRaisesRegex(ValueError, "Incomplete"):
            WSL.sync_source(self.source, self.target, self.state, files={})

    @unittest.skipUnless(sys.platform == "linux", "flock is a Linux worker contract")
    def test_workspace_lock_excludes_another_writer(self):
        with WSL.workspace_lock(self.root / "workspace"):
            with self.assertRaisesRegex(RuntimeError, "Another validation"):
                with WSL.workspace_lock(self.root / "workspace"):
                    self.fail("second writer entered")


if __name__ == "__main__":
    unittest.main()
