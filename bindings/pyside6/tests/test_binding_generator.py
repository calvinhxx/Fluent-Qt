"""Generation must not invalidate unchanged C++ outputs or publish a failed run."""

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("generator", Path(__file__).parents[1] / "tools/generate_bindings.py")
GENERATOR = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(GENERATOR)


class BindingGeneratorTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.output = self.root / "generated"
        self.stage = self.root / "staging"
        self.output.mkdir()
        (self.stage / "module").mkdir(parents=True)

    def test_unchanged_outputs_preserve_timestamps_and_changed_outputs_update(self):
        first, second = self.stage / "module/first.cpp", self.stage / "module/second.h"
        first.write_text("first")
        second.write_text("second")
        self.assertEqual(GENERATOR.publish(self.stage, self.output), (2, 0))
        before = {p.name: p.stat().st_mtime_ns for p in (self.output / "module").iterdir()}
        header_stamp = self.output / "headers.sha256"
        stamp_before = header_stamp.read_text()
        stamp_mtime = header_stamp.stat().st_mtime_ns
        self.assertEqual(GENERATOR.publish(self.stage, self.output), (0, 0))
        self.assertEqual(before, {p.name: p.stat().st_mtime_ns for p in (self.output / "module").iterdir()})
        self.assertEqual(header_stamp.stat().st_mtime_ns, stamp_mtime)
        second.write_text("changed declaration")
        self.assertEqual(GENERATOR.publish(self.stage, self.output), (1, 0))
        self.assertEqual((self.output / "module/first.cpp").stat().st_mtime_ns, before["first.cpp"])
        self.assertEqual((self.output / "module/second.h").read_text(), "changed declaration")
        self.assertNotEqual(header_stamp.read_text(), stamp_before)

    def test_removes_only_obsolete_owned_outputs(self):
        (self.stage / "module/old.cpp").write_text("old")
        GENERATOR.publish(self.stage, self.output)
        (self.output / "generation.stamp").write_text("owned by CMake")
        (self.stage / "module/old.cpp").unlink()
        (self.stage / "module/new.cpp").write_text("new")
        self.assertEqual(GENERATOR.publish(self.stage, self.output), (1, 1))
        self.assertFalse((self.output / "module/old.cpp").exists())
        self.assertEqual((self.output / "generation.stamp").read_text(), "owned by CMake")

    def test_only_generator_extra_include_order_is_canonicalized(self):
        preamble = b'#include "z.h"\n#include "a.h"\n'
        tail = b'#if FEATURE\n#include "z.h"\n#include "a.h"\n#endif\n'
        one = preamble + b'// Extra includes\n#include <qwidget.h>\n#include <qobject.h>\n' + tail
        two = preamble + b'// Extra includes\n#include <qobject.h>\n#include <qwidget.h>\n' + tail
        self.assertEqual(GENERATOR.stable_includes(one), two)
        header = self.stage / "module/generated.h"
        header.write_bytes(one)
        GENERATOR.publish(self.stage, self.output)
        stamp = (self.output / "module/generated.h").stat().st_mtime_ns
        header.write_bytes(two)
        self.assertEqual(GENERATOR.publish(self.stage, self.output), (0, 0))
        self.assertEqual((self.output / "module/generated.h").stat().st_mtime_ns, stamp)

    def test_protected_helper_order_keeps_virtual_order_and_bodies(self):
        first = b"    void first() override;\n"
        virtual = b"    bool eventFilter(QObject * o, QEvent * e) override;\n"
        helper = b"    inline bool eventFilter_protected(QObject * o, QEvent * e) { return Base::eventFilter(o, e); }\n"
        last = b"    void last() override;\n    int member;\n"
        expected = first + helper + virtual + last
        self.assertEqual(GENERATOR.stable_header(first + virtual + helper + last), expected)
        self.assertEqual(GENERATOR.stable_header(expected), expected)
        unrelated = b"    inline void other_protected() { Base::other(); }\n"
        self.assertEqual(GENERATOR.stable_header(virtual + unrelated), virtual + unrelated)

    def test_generator_failure_keeps_last_successful_outputs(self):
        target = self.output / "module.cpp"
        target.write_text("last good generation")
        before = target.stat().st_mtime_ns

        def failing(argv, *, env):
            self.assertEqual(env["QT_HASH_SEED"], "0")
            stage = Path(argv[1].split("=", 1)[1])
            (stage / "module.cpp").write_text("partial output")
            return subprocess.CompletedProcess(argv, 7)

        with patch.object(GENERATOR.subprocess, "run", side_effect=failing):
            self.assertEqual(GENERATOR.generate(self.output, ["shiboken", "api.h", "types.xml"]), 7)
        self.assertEqual(target.read_text(), "last good generation")
        self.assertEqual(target.stat().st_mtime_ns, before)
        self.assertFalse(list(self.root.glob("shiboken-*")))

    def test_manifest_cannot_remove_outside_output(self):
        (self.output / "generated-files.json").write_text('["../important"]')
        protected = self.root / "important"
        protected.write_text("keep")
        (self.stage / "module/new.cpp").write_text("new")
        with self.assertRaises(ValueError):
            GENERATOR.publish(self.stage, self.output)
        self.assertEqual(protected.read_text(), "keep")


if __name__ == "__main__":
    unittest.main()
