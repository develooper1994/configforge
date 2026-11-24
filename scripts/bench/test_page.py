#!/usr/bin/env python3
"""Tests of `page.py --validate` (L-9): a results file is untrusted data.

.github/workflows/bench.yml reads CI's artifact bench-quick with a token that
writes, and scripts/bench/publish.sh makes a path and a commit message from
its fields, so `--validate` must refuse every field that is not exactly what
run.py writes. Run by CI's bench-gate job:

    python3 -m unittest scripts/bench/test_page.py
"""

import contextlib
import copy
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import page

GOOD = {
    "schema": 1,
    "kind": "quick",
    "commit": "0123456789abcdef0123456789abcdef01234567",
    "committed_at": "2026-11-24T10:20:30Z",
    "run_id": "1234567890",
    "machine": {},
    "compiler": {"id": "gcc", "version": "gcc 15", "flags": "shipped"},
    "libraries": [],
    "cells": [{"lib": "configforge", "format": "json", "doc": "mixed", "size_mib": 4, "mode": "parse"}],
}


def validate(results):
    with tempfile.TemporaryDirectory() as tmp:
        path = os.path.join(tmp, "results.json")
        with open(path, "w", encoding="utf-8") as handle:
            json.dump(results, handle)
        with contextlib.redirect_stdout(io.StringIO()):
            return page.validate(path)


def changed(**fields):
    r = copy.deepcopy(GOOD)
    r.update(fields)
    return r


class ValidateTest(unittest.TestCase):
    def test_what_run_py_writes_passes(self):
        self.assertEqual(validate(GOOD), 0)
        full = changed(kind="full", compiler={"id": "clang", "version": "x", "flags": "native"})
        self.assertEqual(validate(full), 0)

    def test_a_time_that_is_a_path_is_refused(self):
        self.assertEqual(validate(changed(committed_at="../../../etc/x")), 1)
        self.assertEqual(validate(changed(committed_at="2026-11-24T10:20:30Z/../x")), 1)
        self.assertEqual(validate(changed(committed_at="")), 1)

    def test_a_compiler_that_is_a_path_or_a_command_is_refused(self):
        for bad in ("../x", "gcc; id", "gcc\n", "$(id)"):
            self.assertEqual(validate(changed(compiler={"id": bad, "flags": "shipped"})), 1, bad)
            self.assertEqual(validate(changed(compiler={"id": "gcc", "flags": bad})), 1, bad)
        self.assertEqual(validate(changed(compiler="gcc")), 1)

    def test_a_run_id_that_is_not_a_number_is_refused(self):
        self.assertEqual(validate(changed(run_id="1; id")), 1)

    def test_a_commit_that_is_not_forty_hex_digits_is_refused(self):
        self.assertEqual(validate(changed(commit="$(id)" + "0" * 36)), 1)

    def test_cells_that_are_not_a_list_are_refused(self):
        self.assertEqual(validate(changed(cells={"lib": "x"})), 1)


if __name__ == "__main__":
    unittest.main()
