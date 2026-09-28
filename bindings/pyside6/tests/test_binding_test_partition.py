#!/usr/bin/env python3
"""Keep isolated lifecycle registration from silently dropping binding coverage."""

import types
import unittest

from run_binding_tests import binding_suite, test_cases


class BindingTestPartitionTest(unittest.TestCase):
    def setUp(self):
        class BindingContracts(unittest.TestCase):
            def test_value(self):
                pass

            def test_lifetime(self):
                pass

        BindingContracts.__module__ = "fixture"
        BindingContracts.__qualname__ = "BindingContracts"
        self.module = types.SimpleNamespace(BindingContracts=BindingContracts)

    def test_full_and_isolated_partitions_cover_each_method_once(self):
        complete = {case.id() for case in test_cases(binding_suite(self.module, set()))}
        ordinary = {case.id() for case in test_cases(binding_suite(self.module, {"BindingContracts.test_lifetime"}))}
        isolated = {"fixture.BindingContracts.test_lifetime"}
        self.assertEqual(ordinary | isolated, complete)
        self.assertFalse(ordinary & isolated)

    def test_stale_isolated_method_is_an_error(self):
        with self.assertRaisesRegex(ValueError, "not found"):
            binding_suite(self.module, {"BindingContracts.test_removed"})

    def test_an_empty_ordinary_partition_is_an_error(self):
        with self.assertRaisesRegex(ValueError, "No binding contracts"):
            binding_suite(self.module, {"BindingContracts.test_value", "BindingContracts.test_lifetime"})


if __name__ == "__main__":
    unittest.main()
