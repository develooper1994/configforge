"""The Python binding (bindings/python/src/configforge) against a shared
build of the library: reads by attribute, item and type; refusals with
the key; explain; derive; the dataclass contract; and Live.

    CONFIGFORGE_LIB=<libconfigforge> python3 test_binding.py <bindings/python/src> <workdir>
"""

import dataclasses
import os
import sys
import tempfile
import threading
import unittest

sys.path.insert(0, sys.argv.pop(1))
WORK = sys.argv.pop(1) if len(sys.argv) > 1 else tempfile.mkdtemp()

import configforge as cf  # noqa: E402
from configforge import flag, rank, section, setting  # noqa: E402


def server_cfg(values=None):
    return (cf.Builder()
            .memory("file", rank.USER_FILE, values or {"server.port": 9443, "server.host": "web1"})
            .define_int("server.port", 8080, 1, 65535)
            .define_text("server.host", "0.0.0.0")
            .define_bool("tls.enabled", False)
            .create())


class Reading(unittest.TestCase):
    def test_attributes_items_and_get(self):
        cfg = server_cfg()
        self.assertEqual(cfg.server.port, 9443)
        self.assertEqual(cfg.server.host, "web1")
        self.assertIs(cfg.tls.enabled, False)
        self.assertEqual(cfg["server.port"], 9443)
        self.assertEqual(cfg.server["port"], 9443)
        self.assertIn("server.port", cfg)
        self.assertIsNone(cfg.get("server.nope"))
        self.assertEqual(cfg.get("server.port", str, "x"), "x", "another type is the default")
        self.assertIsNone(cfg.get("tls.enabled", int), "a bool is not an int here")
        with self.assertRaises(KeyError):
            cfg["server.nope"]
        with self.assertRaises(AttributeError):
            cfg.server.nope
        self.assertEqual(sorted(cfg.server), ["host", "port"])
        self.assertEqual(cfg.to_dict()["server"], {"host": "web1", "port": 9443})

    def test_refused_with_the_key(self):
        with self.assertRaises(cf.Error) as e:
            server_cfg({"server.port": 99999})
        self.assertEqual(e.exception.key, "server.port")
        self.assertIn("65535", str(e.exception))
        self.assertEqual(e.exception.name, "validate")

    def test_explain_and_derive(self):
        cfg = server_cfg()
        self.assertRegex(cfg.explain("server.port"), r"file +9443 +selected")
        admin = cfg.derive("admin", {"server.port": 7000})
        self.assertEqual(admin.server.port, 7000)
        self.assertEqual(cfg.server.port, 9443, "the snapshot derived from is unchanged")
        self.assertIn("server.port: 9443 -> 7000", admin.changes_since(cfg))
        with self.assertRaises(cf.Error):
            cfg.derive("admin", {"server.port": 0})

    def test_a_consumed_builder(self):
        b = cf.Builder()
        b.create()
        with self.assertRaises(RuntimeError):
            b.create()


@dataclasses.dataclass
class Server:
    host: str = setting("server.host", "0.0.0.0")
    port: int = setting("server.port", 8080, range=(1, 65535), flags=flag.RESTART)


@dataclasses.dataclass
class Settings:
    server: Server = section(Server)
    debug: bool = setting("app.debug", False)
    workers: int = setting("app.workers", 4, range=(1, 64))


class Dataclasses(unittest.TestCase):
    def test_the_dataclass_is_the_contract(self):
        b = cf.Builder().memory("file", rank.USER_FILE, {"server.port": "9443", "app.debug": "true"})
        cfg = cf.define(b, Settings).create()
        s = cf.read(cfg, Settings)
        self.assertEqual(s, Settings(Server("0.0.0.0", 9443), True, 4))
        bad = cf.Builder().memory("file", rank.USER_FILE, {"app.workers": "65"})
        with self.assertRaises(cf.Error) as e:
            cf.define(bad, Settings).create()
        self.assertEqual(e.exception.key, "app.workers")

    def test_a_field_without_a_key(self):
        @dataclasses.dataclass
        class Bad:
            x: int = 3

        with self.assertRaises(TypeError):
            cf.define(cf.Builder(), Bad)


class Reloading(unittest.TestCase):
    def setUp(self):
        self.path = os.path.join(WORK, "py_live.ini")
        with open(self.path, "w") as f:
            f.write("[server]\nport = 8080\n")

    def build(self):
        return cf.define(cf.Builder().file("app.ini", rank.USER_FILE, self.path), Settings)

    def write(self, text):
        with open(self.path, "w") as f:
            f.write(text)

    def test_approve_restart_and_refuse(self):
        with cf.Live(cf.Mode.APPROVE, self.build) as live:
            self.assertEqual(live.generation, 1)
            self.write("[server]\nport = 9443\n[app]\ndebug = true\n")
            self.assertEqual(live.poll(), cf.Reload.PENDING)
            self.assertIs(live.get().app.debug, False, "not before approval")
            self.assertIs(live.pending.app.debug, True)
            self.assertEqual(live.approve(), cf.Reload.SWAPPED)
            cfg = live.get()
            self.assertIs(cfg.app.debug, True)
            self.assertEqual(cfg.server.port, 8080, "the port waits for a restart")
            self.assertEqual(live.restarts, ["server.port"])
            self.assertRegex(cfg.explain("server.port"), r"running +8080 +selected")
            self.write("[app]\nworkers = lots\n")
            with self.assertRaises(cf.Error) as e:
                live.poll()
            self.assertIn("lots", str(e.exception))
            self.assertIs(live.get().app.debug, True, "still serving")
        self.assertEqual(cfg.server.port, 8080, "a snapshot outlives its Live")

    def test_a_build_that_raises(self):
        calls = []

        def build():
            calls.append(1)
            if len(calls) > 1:
                raise ValueError("no builder today")
            return cf.Builder()

        with cf.Live(cf.Mode.MANUAL, build) as live:
            with self.assertRaises(cf.Error) as e:
                live.reload()
            self.assertIsInstance(e.exception.__cause__, ValueError)

    def test_readers_on_threads(self):
        with cf.Live(cf.Mode.MANUAL, self.build) as live:
            seen = set()

            def reader():
                for _ in range(500):
                    seen.add(live.get().app.debug)

            threads = [threading.Thread(target=reader) for _ in range(4)]
            for t in threads:
                t.start()
            for i in range(50):
                self.write("[app]\ndebug = %s\n%s" % ("true" if i % 2 else "false", "#" * i))
                live.reload()
            for t in threads:
                t.join()
            self.assertTrue(seen <= {True, False})


class Library(unittest.TestCase):
    def test_version_and_features(self):
        self.assertTrue(cf.version().startswith(cf.__version__))
        self.assertTrue(cf.has_feature("ini"))
        self.assertFalse(cf.has_feature("no-such-thing"))


if __name__ == "__main__":
    unittest.main(verbosity=1)
