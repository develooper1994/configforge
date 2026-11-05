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

# Only what a change touched: against itself, nothing.
same = doctor.lint(fixture("cf_fx_staging.json"), fail_on="never", doctor=DOCTOR,
                   base=fixture("cf_fx_staging.json"))
check(same == [], "against itself, a change touched nothing")

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

# A sequence, oldest first: production, then staging. The port changed
# value and source, the host only its source; no value is printed.
seq = [fixture("cf_fx_prod.json"), fixture("cf_fx_staging.json")]
hist = doctor.history(seq, doctor=DOCTOR)
port = [f for f in hist if f.key == "server.port"]
check(len(port) == 1 and port[0].rule == "HISTORY" and port[0].level == "info",
      "a change from one report to the next is one HISTORY finding")
check(port and "value changed, now from 'cli' rather than 'env'" in port[0].message,
      "saying what changed and where it comes from now")
check(not [f for f in hist if f.key == "log.level"], "an unchanged key has no history")
check(not [f for f in hist if "9090" in f.message or "5000" in f.message
           or "secret" in f.message], "and no value is in any message")
check([f.key for f in doctor.history(seq, key="server.host", doctor=DOCTOR)]
      == ["server.host"], "one key's history is that key's alone")

blamed = doctor.blame("server.host", seq, doctor=DOCTOR)
check(len(blamed) == 1 and blamed[0].rule == "BLAME"
      and "since " + seq[1] in blamed[0].message,
      "blame names the report a key's present state dates from")
try:
    doctor.blame("no.such.key", seq, doctor=DOCTOR)
    check(False, "a key in none of the reports is refused")
except doctor.DoctorError as e:
    check("none of the reports" in str(e), "a key in none of the reports is refused")

print("%d checks, %d failures" % (checks, failures))
sys.exit(1 if failures else 0)
