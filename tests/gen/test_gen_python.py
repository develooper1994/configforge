"""The contract as a Python module (`configforge gen --lang python`, D-95):
the program writes it from the test contract, the module is imported by its
path, and a configuration is read through its dataclass.

    CONFIGFORGE_LIB=<libconfigforge> python3 test_gen_python.py
        <bindings/python/src> <configforge-gen> <schema> <workdir>
"""

import importlib.util
import os
import subprocess
import sys

sys.path.insert(0, sys.argv[1])
GEN, SCHEMA, WORK = sys.argv[2], sys.argv[3], sys.argv[4]

import configforge as cf  # noqa: E402

os.makedirs(WORK, exist_ok=True)
OUT = os.path.join(WORK, "gen_contract.py")
subprocess.run([GEN, "--schema", SCHEMA, "--lang", "python", "--out", OUT], check=True)

spec = importlib.util.spec_from_file_location("gen_contract", OUT)
gen = importlib.util.module_from_spec(spec)
sys.modules["gen_contract"] = gen
spec.loader.exec_module(gen)


def settings(**values):
    """Settings read through the generated contract, the token given."""
    values.setdefault("server.token", "t")
    b = gen.contract(cf.Builder()).memory("m", cf.rank.USER_FILE, values)
    return cf.read(b.create(), gen.Settings)


s = settings()
assert s.server.port == 8080, s.server.port
assert s.server.host == "0.0.0.0", s.server.host
assert s.server.mode == "dev" and s.server.tls is False
assert s.ratio == 0.25, s.ratio
assert settings(**{"server.port": 9443}).server.port == 9443

try:
    settings(**{"server.port": 70000})
except cf.Error as err:
    assert "server.port" in str(err), str(err)
else:
    raise AssertionError("a port of 70000 was accepted")

assert "token" not in repr(s), repr(s)
print("gen.python ok")
