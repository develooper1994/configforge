"""layers.py - where a value comes from, with every kind of source at
once, in Python.

layers.c through the Python binding, reading the same files: shipped
defaults, conf.d, a .env, the environment and the command line, each
above the one before, and then why each value won. The files are in
examples/c/layers/; the suite runs this there.
"""

import sys

import configforge as cf


def main() -> int:
    try:
        cfg = (cf.Builder()
               .file("app.toml", 35, "app.toml")
               .dir("conf.d", 30, "conf.d", flags=cf.OPTIONAL)
               .dotenv(".env", 15, ".env", "LAYERS_", flags=cf.OPTIONAL)
               .env("env", cf.rank.ENV, "LAYERS_", {"LAYERS_LOG__LEVEL": "warn"})
               .cli("cli", cf.rank.CLI, ["layers", "--server.port=9443"])
               .define_text("server.host", "127.0.0.1")
               .define_int("server.port", 8080, 1, 65535)
               .define_int("server.workers", 2, 1, 64)
               .define_text("db.url", "sqlite:dev.db", flags=cf.flag.SENSITIVE)
               .define_text("log.level", "info")
               .define_int("cache.size", 64, 1, 4096)
               .unknown(cf.UNKNOWN_REJECT)
               .create())
    except cf.Error as e:
        print("layers: %s" % e, file=sys.stderr)
        return 1
    # db.url is sensitive: explain() says where it was set, never what.
    for key in ("server.host", "server.port", "server.workers", "db.url",
                "log.level", "cache.size"):
        print(cfg.explain(key))
    return 0


if __name__ == "__main__":
    sys.exit(main())
