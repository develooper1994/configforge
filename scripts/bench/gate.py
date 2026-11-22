#!/usr/bin/env python3
"""The gate on instructions (D-111, docs/Performance.md).

    gate.py <results> [<baseline>]            compare a `quick` run to the baseline
    gate.py --write <results> [<baseline>]    write the baseline from a run

The baseline, bench/baseline/instructions.json, is

    {"schema": 1, "commit": "<40 hex>",
     "toolchain": {"compiler": "<first --version line>",
                   "libc": "<first ldd --version line>"},
     "cells": {"json/mixed/16": <instructions>, ...}}

written from the CI runner's own output (the artifact bench-quick of the
commit that adds it), never from a laptop. The gated cells are the count cells
of ConfigForge at 4 MiB (bench-gate took 15 minutes at 16, over the card's ten: P-1b notes): the key is <format>/<doc>/4.

Exit 0: every cell within the limit (or no baseline yet), 1: a cell costs more
than 110% of its baseline, 3: the runner's compiler or C library is not the
baseline's, so nothing was compared. Wall time never gates: a shared runner
varies by tens of percent, and an instruction count does not.
"""

import json
import os
import sys

LIMIT = 10  # percent more instructions than the baseline
NOTE = 5  # percent fewer: the baseline should be lowered
GATED_SIZE_MIB = 4
HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_BASELINE = os.path.normpath(
    os.path.join(HERE, "..", "..", "bench", "baseline", "instructions.json")
)


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def cell_key(cell):
    return "%s/%s/%d" % (cell["format"], cell["doc"], cell["size_mib"])


def gated_cells(results):
    """{key: instructions} of the cells the gate compares."""
    cells = {}
    for c in results.get("cells", []):
        if (
            c.get("lib") == "configforge"
            and c.get("mode") == "count"
            and c.get("size_mib") == GATED_SIZE_MIB
            and c.get("instructions") is not None
        ):
            cells[cell_key(c)] = int(c["instructions"])
    return cells


def toolchain(results):
    return {
        "compiler": results.get("compiler", {}).get("version", "unknown"),
        "libc": results.get("machine", {}).get("libc", "unknown"),
    }


def write(results_path, baseline_path):
    results = load(results_path)
    cells = gated_cells(results)
    if not cells:
        print("bench gate: no count cell at %d MiB in %s" % (GATED_SIZE_MIB, results_path))
        return 2
    baseline = {
        "schema": 1,
        "commit": results.get("commit", "0" * 40),
        "toolchain": toolchain(results),
        "cells": dict(sorted(cells.items())),
    }
    os.makedirs(os.path.dirname(os.path.abspath(baseline_path)), exist_ok=True)
    with open(baseline_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(baseline, f, indent=2)
        f.write("\n")
    return 0


def compare(results_path, baseline_path):
    results = load(results_path)
    if not os.path.exists(baseline_path):
        print(
            "bench gate: no baseline yet; the results are the artifact "
            "bench-quick: run scripts/bench/gate.py --write on it and commit "
            "the file"
        )
        return 0
    baseline = load(baseline_path)
    old, new = baseline.get("toolchain", {}), toolchain(results)
    if old != new:
        print(
            "bench gate: the baseline is for {}, {}; this runner has {}, {}: "
            "regenerate it with scripts/bench/gate.py --write and commit it".format(old.get("compiler"), old.get("libc"), new["compiler"], new["libc"])
        )
        return 3
    have = gated_cells(results)
    rc = 0
    for key in sorted(baseline.get("cells", {})):
        base = baseline["cells"][key]
        if key not in have:
            print(f"bench gate: {key}: not in the results")
            continue
        n = have[key]
        pct = (n / base - 1.0) * 100.0
        if n * 100 > base * (100 + LIMIT):
            print(
                "bench gate: %s: %d instructions, baseline %d (+%.1f%%, limit +%d%%)"
                % (key, n, base, pct, LIMIT)
            )
            rc = 1
        elif n * 100 <= base * (100 - NOTE):
            print(
                f"bench gate: {key}: {-pct:.1f}% fewer instructions than the baseline: "
                "run scripts/bench/gate.py --write and commit it"
            )
    for key in sorted(set(have) - set(baseline.get("cells", {}))):
        print(f"bench gate: {key}: not in the baseline")
    return rc


def main(argv):
    args = list(argv)
    write_mode = False
    if args and args[0] == "--write":
        write_mode = True
        args = args[1:]
    if not args or len(args) > 2:
        print(__doc__.strip().splitlines()[0])
        print("usage: gate.py [--write] <results> [<baseline>]")
        return 2
    baseline = args[1] if len(args) == 2 else DEFAULT_BASELINE
    if write_mode:
        return write(args[0], baseline)
    return compare(args[0], baseline)


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
