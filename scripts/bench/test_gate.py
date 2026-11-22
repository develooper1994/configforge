"""Tests of scripts/bench/gate.py (D-111): the gate on instructions.

Run by CI's bench-gate job first, and by ctest as bench.gate (under
-DCF_BUILD_BENCH=ON):

    python3 -m unittest scripts/bench/test_gate.py
"""

import contextlib
import io
import json
import os
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import gate

CELL = "json/mixed/4"
BASE = 4100000000
COMPILER = "gcc (Ubuntu 15.2.0-16ubuntu1) 15.2.0"
LIBC = "ldd (Ubuntu GLIBC 2.43-0ubuntu1) 2.43"


def results(instructions, compiler=COMPILER, libc=LIBC):
    return {
        "schema": 1,
        "kind": "quick",
        "compiler": {"id": "gcc", "version": compiler, "flags": "shipped"},
        "machine": {"libc": libc},
        "cells": [
            {
                "lib": "configforge",
                "format": "json",
                "doc": "mixed",
                "size_mib": 4,
                "mode": "count",
                "instructions": instructions,
            },
            {
                "lib": "configforge",
                "format": "json",
                "doc": "mixed",
                "size_mib": 1,
                "mode": "speed",
                "mib_s": 100.0,
            },
        ],
    }


def baseline(instructions=BASE, compiler=COMPILER, libc=LIBC):
    return {
        "schema": 1,
        "commit": "0" * 40,
        "toolchain": {"compiler": compiler, "libc": libc},
        "cells": {CELL: instructions},
    }


class GateTest(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.addCleanup(self.dir.cleanup)

    def write(self, name, obj):
        path = os.path.join(self.dir.name, name)
        with open(path, "w", encoding="utf-8") as f:
            json.dump(obj, f)
        return path

    def run_gate(self, args):
        out = io.StringIO()
        with contextlib.redirect_stdout(out):
            rc = gate.main(args)
        return rc, out.getvalue()

    def test_111_percent_fails_with_the_message(self):
        r = self.write("r.json", results(5123456789))
        b = self.write("b.json", baseline())
        rc, out = self.run_gate([r, b])
        self.assertEqual(rc, 1)
        self.assertEqual(
            out.strip(),
            "bench gate: json/mixed/4: 5123456789 instructions, baseline "
            "4100000000 (+25.0%, limit +10%)",
        )

    def test_exactly_the_limit_and_109_percent_pass(self):
        b = self.write("b.json", baseline(1000))
        for n in (1090, 1100):
            r = self.write("r.json", results(n))
            rc, out = self.run_gate([r, b])
            self.assertEqual(rc, 0, out)
            self.assertEqual(out.strip(), "")

    def test_94_percent_passes_with_a_warning(self):
        r = self.write("r.json", results(940))
        b = self.write("b.json", baseline(1000))
        rc, out = self.run_gate([r, b])
        self.assertEqual(rc, 0)
        self.assertEqual(
            out.strip(),
            "bench gate: json/mixed/4: 6.0% fewer instructions than the "
            "baseline: run scripts/bench/gate.py --write and commit it",
        )

    def test_a_different_compiler_line_is_exit_3_and_compares_nothing(self):
        other = "gcc (Ubuntu 16.1.0) 16.1.0"
        r = self.write("r.json", results(9999999999, compiler=other))
        b = self.write("b.json", baseline())
        rc, out = self.run_gate([r, b])
        self.assertEqual(rc, 3)
        self.assertEqual(
            out.strip(),
            f"bench gate: the baseline is for {COMPILER}, {LIBC}; this runner has {other}, {LIBC}: "
            "regenerate it with scripts/bench/gate.py --write and commit it",
        )

    def test_a_different_libc_line_is_exit_3(self):
        r = self.write("r.json", results(1000, libc="ldd (GNU libc) 2.99"))
        b = self.write("b.json", baseline(1000))
        rc, _ = self.run_gate([r, b])
        self.assertEqual(rc, 3)

    def test_no_baseline_file_is_exit_0_with_its_message(self):
        r = self.write("r.json", results(1000))
        rc, out = self.run_gate([r, os.path.join(self.dir.name, "none.json")])
        self.assertEqual(rc, 0)
        self.assertEqual(
            out.strip(),
            "bench gate: no baseline yet; the results are the artifact "
            "bench-quick: run scripts/bench/gate.py --write on it and commit "
            "the file",
        )

    def test_write_makes_a_baseline_that_then_passes(self):
        r = self.write("r.json", results(1234567))
        out_path = os.path.join(self.dir.name, "base.json")
        rc, _ = self.run_gate(["--write", r, out_path])
        self.assertEqual(rc, 0)
        with open(out_path, encoding="utf-8") as f:
            b = json.load(f)
        self.assertEqual(b["cells"], {CELL: 1234567})
        self.assertEqual(b["toolchain"], {"compiler": COMPILER, "libc": LIBC})
        rc, out = self.run_gate([r, out_path])
        self.assertEqual(rc, 0)
        self.assertEqual(out.strip(), "")

    def test_only_the_4_mib_count_cells_are_gated(self):
        # a speed cell, and a count cell of another size, are never compared
        res = results(1000)
        res["cells"].append(
            {"lib": "configforge", "format": "yaml", "doc": "mixed",
             "size_mib": 1, "mode": "count", "instructions": 10**12}
        )
        r = self.write("r.json", res)
        b = self.write("b.json", baseline(1000))
        rc, out = self.run_gate([r, b])
        self.assertEqual(rc, 0, out)


if __name__ == "__main__":
    unittest.main()
