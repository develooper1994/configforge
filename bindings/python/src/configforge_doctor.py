"""ConfigForge Doctor from Python: lint and diff provenance reports.

    import configforge_doctor as doctor

    for f in doctor.lint("app.json", fail_on="never"):
        print(f.rule, f.level, f.key, f.message, f.file, f.line)

It runs the `doctor` program and reads its SARIF output, so a Python
script sees exactly what a CI job running Doctor sees - the same rules,
the same severities, the same baseline - and there is no second
implementation of either to drift. No dependencies beyond the standard
library, and no ABI: Doctor's library is static (D-41), and a program
is the one interface Python can rely on everywhere.

The program is found as $CONFIGFORGE_DOCTOR, or `doctor` on PATH, or
passed as `doctor=`.
"""

import json
import os
import shutil
import subprocess

__all__ = ["Finding", "DoctorError", "lint", "diff", "version", "find_doctor"]


class DoctorError(Exception):
    """Doctor could not run, or could not read what it was given."""


class Finding:
    """One finding: a rule, its level, the key, what is wrong, and where -
    a file and a line when the value came from one."""

    __slots__ = ("rule", "level", "key", "message", "file", "line")

    def __init__(self, rule, level, key, message, file=None, line=None):
        self.rule = rule
        self.level = level
        self.key = key
        self.message = message
        self.file = file
        self.line = line

    def __repr__(self):
        where = " at %s:%s" % (self.file, self.line) if self.line else ""
        return "Finding(%s %s %s%s: %s)" % (self.rule, self.level, self.key, where, self.message)


# SARIF says "note" where Doctor says "info".
_LEVELS = {"error": "error", "warning": "warning", "note": "info"}


def find_doctor(doctor=None):
    """The doctor program to run, or DoctorError naming where it looked."""
    path = doctor or os.environ.get("CONFIGFORGE_DOCTOR") or shutil.which("doctor")
    if not path:
        raise DoctorError("no doctor program: set CONFIGFORGE_DOCTOR or put doctor on PATH")
    return path


def _run(args, doctor):
    try:
        p = subprocess.run([find_doctor(doctor)] + args, capture_output=True, text=True)
    except OSError as e:
        raise DoctorError("cannot run doctor: %s" % e)
    # 0 and 1 are results - nothing at the --fail-on level, something at
    # it; 2 is input Doctor could not use.
    if p.returncode not in (0, 1):
        raise DoctorError(p.stderr.strip() or "doctor exited %d" % p.returncode)
    return p.stdout


def _findings(sarif_text):
    try:
        doc = json.loads(sarif_text)
    except ValueError as e:
        raise DoctorError("doctor's SARIF output did not parse: %s" % e)
    out = []
    for run in doc.get("runs", []):
        for r in run.get("results", []):
            text = r.get("message", {}).get("text", "")
            key, _, message = text.partition(": ")
            file = line = None
            for loc in r.get("locations", []):
                phys = loc.get("physicalLocation", {})
                region = phys.get("region")
                if region is not None:
                    file = phys.get("artifactLocation", {}).get("uri")
                    line = region.get("startLine")
            out.append(Finding(r.get("ruleId"), _LEVELS.get(r.get("level"), r.get("level")),
                               key, message, file, line))
    return out


def _options(baseline, severity, fail_on):
    args = ["--format", "sarif"]
    if baseline:
        args += ["--baseline", baseline]
    if severity:
        args += ["--severity", severity]
    if fail_on:
        args += ["--fail-on", fail_on]
    return args


def lint(report, baseline=None, severity=None, fail_on=None, doctor=None):
    """Every finding in one report, as `doctor lint` makes them."""
    return _findings(_run(["lint", report] + _options(baseline, severity, fail_on), doctor))


def diff(before, after, baseline=None, severity=None, fail_on=None, doctor=None):
    """The findings `doctor diff` makes between two reports."""
    return _findings(
        _run(["diff", before, after] + _options(baseline, severity, fail_on), doctor))


def version(doctor=None):
    """Doctor's version, and the commit it was built from."""
    return _run(["--version"], doctor).strip()
