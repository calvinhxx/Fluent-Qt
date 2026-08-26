#!/usr/bin/env python3

import importlib.util
import json
import subprocess
import sys
import unittest
from pathlib import Path
from unittest import mock


SCRIPT = Path(__file__).with_name("check-release-branch-freshness.py")
SPEC = importlib.util.spec_from_file_location("release_branch_freshness", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def completed(payload: object) -> subprocess.CompletedProcess[str]:
    return subprocess.CompletedProcess(
        args=["gh", "api"], returncode=0, stdout=json.dumps(payload), stderr=""
    )


class ReleaseBranchFreshnessTest(unittest.TestCase):
    @mock.patch.object(MODULE.subprocess, "run")
    def test_reads_current_base_and_compare_distance(self, run):
        run.side_effect = [completed({"sha": "base123"}), completed({"behind_by": 0})]

        base_sha = MODULE.current_base_sha("owner/repo", "main")
        behind = MODULE.behind_count("owner/repo", base_sha, "head456")

        self.assertEqual(base_sha, "base123")
        self.assertEqual(behind, 0)
        self.assertEqual(
            run.call_args_list[1].args[0],
            ["gh", "api", "repos/owner/repo/compare/base123...head456"],
        )

    @mock.patch.object(MODULE.subprocess, "run")
    def test_branch_names_are_url_encoded(self, run):
        run.return_value = completed({"sha": "base123"})
        MODULE.current_base_sha("owner/repo", "release/1.7.x")
        self.assertEqual(
            run.call_args.args[0],
            ["gh", "api", "repos/owner/repo/commits/release%2F1.7.x"],
        )

    @mock.patch.object(MODULE.subprocess, "run")
    def test_invalid_compare_payload_is_rejected(self, run):
        run.return_value = completed({"behind_by": "1"})
        with self.assertRaisesRegex(RuntimeError, "invalid behind_by"):
            MODULE.behind_count("owner/repo", "base123", "head456")


if __name__ == "__main__":
    unittest.main()
