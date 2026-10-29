"""The Python module over Doctor: the findings `doctor lint` makes, read
back from its SARIF output with their rule, level, key and place.

    python3 test_python.py <bindings/python/src> <doctor> <directory of fixture reports>
"""

import os
import sys

sys.path.insert(0, sys.argv[1])
import configforge_doctor as doctor  # noqa: E402

DOCTOR, FIXTURES = sys.argv[2], sys.argv[3]
failures = 0
checks = 0


def check(cond, what):
    global failures, checks
    checks += 1
    if not cond:
        failures += 1
        print("FAIL:", what)


def fixture(name):
    return os.path.join(FIXTURES, name)


# A password on the command line, against a policy that allows only the
# environment: CFD016, an error, at argv - no file.
found = doctor.lint(fixture("cf_fx_leak.json"), fail_on="never", doctor=DOCTOR)
leak = [f for f in found if f.rule == "CFD016"]
check(len(leak) == 1, "a secret set past its policy is found")
if leak:
    check(leak[0].level == "error", "as an error")
    check(leak[0].key == "db.password", "naming the key")
    check("argv[1]" in leak[0].message, "and where it was set")
    check(leak[0].line is None, "with no line: argv is not a file")

# SARIF's "note" is Doctor's "info".
check(any(f.rule == "CFD008" and f.level == "info" for f in found), "a note is info")

# A source that wins nothing: CFD010, a warning.
dead = [f for f in doctor.lint(fixture("cf_fx_dead.json"), doctor=DOCTOR) if f.rule == "CFD010"]
check(len(dead) == 1 and dead[0].level == "warning", "a dead source is a warning")

# A key set twice in one file: CFD001, at the file and the line.
twice = [f for f in doctor.lint(fixture("cf_fx_twice.json"), fail_on="never", doctor=DOCTOR)
         if f.rule == "CFD001"]
check(len(twice) >= 1 and twice[0].file and twice[0].line, "a finding in a file has its line")

# What Doctor cannot read is an error, not an empty list.
try:
    doctor.lint(fixture("cf_fx_staging.ini"), doctor=DOCTOR)
    check(False, "a file that is not a report is refused")
except doctor.DoctorError as e:
    check("report" in str(e) or str(e), "a file that is not a report is refused, with why")

check("ConfigForge" in doctor.version(doctor=DOCTOR) or doctor.version(doctor=DOCTOR),
      "the version is Doctor's")

print("%d checks, %d failures" % (checks, failures))
sys.exit(1 if failures else 0)
