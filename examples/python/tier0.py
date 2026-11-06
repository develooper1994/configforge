"""tier0.py - the smallest complete program, in Python.

tier0.c through the Python binding: the command line, the environment
(MYAPP_), and a value the program reads with its own fallback.

    python tier0.py --server.port=7000
"""

import sys

import configforge as cf


def main() -> int:
    try:
        cfg = cf.Builder.app("myapp").create()
    except cf.Error as e:
        print(e, file=sys.stderr)
        return 1
    print("%s:%d tls=%s" % (cfg.get("server.host", "0.0.0.0"),
                            cfg.get("server.port", 8080),
                            "on" if cfg.get("tls.enabled", False) else "off"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
