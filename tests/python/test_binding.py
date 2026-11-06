"""The Python binding (bindings/python/src/configforge) against a shared
build of the library: reads by attribute, item and type; refusals with
the key; explain; derive; the dataclass contract; and Live.

    CONFIGFORGE_LIB=<libconfigforge> python3 test_binding.py <bindings/python/src> <workdir>
"""

import dataclasses
import json
import logging
import os
import sys
import tempfile
import threading
import unittest
from unittest import mock

sys.path.insert(0, sys.argv.pop(1))
WORK = sys.argv.pop(1) if len(sys.argv) > 1 else tempfile.mkdtemp()

import configforge as cf  # noqa: E402
from configforge import _native as native  # noqa: E402
from configforge import flag, rank, section, setting  # noqa: E402


def server_cfg(values=None):
    return (cf.Builder()
            .memory("file", rank.USER_FILE, values or {"server.port": 9443, "server.host": "web1"})
            .define_int("server.port", 8080, 1, 65535)
            .define_text("server.host", "0.0.0.0")
            .define_bool("tls.enabled", False)
            .create())


class Lists(unittest.TestCase):
    @staticmethod
    def values(cfg, key):
        return [cfg["%s.%d" % (key, i)] for i in range(cfg[key + ".count"])]

    def test_append_defaults_aliases_and_derivation(self):
        defaults = ["base"]
        b = (cf.Builder()
             .define_list("tags", str, defaults, append=True, aliases=["t"])
             .env("env", rank.ENV, "APP_", {"APP_TAGS": "e1,e2"})
             .cli("cli", rank.CLI, ["app", "--tags=c1", "-t", "c2"]))
        defaults[0] = "changed after definition"
        cfg = b.create()
        expected = ["base", "e1", "e2", "c1", "c2"]
        self.assertEqual(self.values(cfg, "tags"), expected)
        self.assertEqual(json.loads(cfg.schema("Lists"))["properties"]["tags"]["default"], ["base"])
        derived = cfg.derive("view", {})
        del cfg
        self.assertEqual(self.values(derived, "tags"), expected)

    def test_empty_and_unsplit_inputs(self):
        cfg = (cf.Builder()
               .define_list("empty", str, [])
               .define_list("whole", str, separator="")
               .define_list("cli_empty", str)
               .env("env", rank.ENV, "APP_", {"APP_EMPTY": "", "APP_WHOLE": "a,b"})
               .cli("cli", rank.CLI, ["app", "--cli_empty="])
               .create())
        self.assertEqual(self.values(cfg, "empty"), [])
        self.assertEqual(self.values(cfg, "whole"), ["a,b"])
        self.assertEqual(self.values(cfg, "cli_empty"), [""])

    def test_element_types_constraints_and_policy(self):
        cfg = (cf.Builder()
               .define_list("ports", int, [1], min=1, max=9, policy=["env"])
               .define_list("switches", bool, [False], append=True)
               .define_list("required", str, flags=flag.REQUIRED)
               .env("env", rank.ENV, "APP_", {"APP_PORTS": "2,3"})
               .cli("cli", rank.CLI,
                    ["app", "--ports=8", "--switches=true", "--required=present"])
               .create())
        self.assertEqual(self.values(cfg, "ports"), [2, 3])
        self.assertEqual(self.values(cfg, "switches"), [False, True])
        self.assertEqual(self.values(cfg, "required"), ["present"])
        with self.assertRaises(cf.Error):
            cf.Builder().define_list("ports", int, [10], min=1, max=9).create()
        with self.assertRaises(cf.Error):
            cf.Builder().define_list("tags", str, ["bad"], allowed=["good"]).create()
        with self.assertRaises(cf.Error):
            cf.Builder().define_list("required", str, flags=flag.REQUIRED).create()

    @unittest.skipUnless(cf.has_feature("real"), "real-valued build")
    def test_real_elements(self):
        cfg = (cf.Builder().define_list("ratios", float, [0.125], append=True, min=0, max=1)
               .cli("cli", rank.CLI, ["app", "--ratios=0.25"]).create())
        self.assertEqual(self.values(cfg, "ratios"), [0.125, 0.25])

    def test_invalid_binding_arguments_are_not_coerced(self):
        with self.assertRaises(TypeError):
            cf.Builder().define_list("ports", int, [True])
        with self.assertRaises(OverflowError):
            cf.Builder().define_list("ports", int, [1 << 64])
        with self.assertRaises(OverflowError):
            cf.Builder().define_list("ports", int, min=0, max=1 << 64)
        with self.assertRaises(TypeError):
            cf.Builder().define_list("tags", str, "not a sequence of defaults")
        for separator in (",;", "\u00e9"):
            with self.assertRaises(ValueError):
                cf.Builder().define_list("tags", str, separator=separator)
        from configforge import _native
        with mock.patch.object(_native, "HAVE_LIST", False):
            with self.assertRaisesRegex(RuntimeError, "list contracts"):
                cf.Builder().define_list("tags", str)

    def test_secret_defaults_and_live_reload(self):
        state = {"tags": "$${literal}", "level": "info"}

        def build():
            return (cf.Builder()
                    .define_list("tags", str, ["default-canary"], append=True,
                                 flags=flag.SENSITIVE | flag.RESTART, policy=["env"])
                    .define_text("level", "info")
                    .env("env", rank.ENV, "APP_", {"APP_TAGS": state["tags"]})
                    .memory("file", rank.USER_FILE, {"level": state["level"]}))

        with cf.Live(cf.Mode.MANUAL, build) as live:
            cfg = live.get()
            self.assertNotIn("default-canary", cfg.schema("Lists"))
            state.update(tags="changed,extra", level="debug")
            live.reload()
            cfg = live.get()
            self.assertEqual(self.values(cfg, "tags"), ["default-canary", "${literal}"])
            self.assertEqual(cfg["level"], "debug")


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
        self.assertEqual(cfg.get("server.port", "x", type=str), "x", "another type is the default")
        self.assertIsNone(cfg.get("tls.enabled", type=int), "a bool is not an int here")
        self.assertEqual(cfg.get("server.port"), 9443)
        with self.assertRaises(KeyError):
            cfg["server.nope"]
        with self.assertRaises(AttributeError):
            cfg.server.nope
        self.assertEqual(sorted(cfg.server), ["host", "port"])
        self.assertEqual(cfg.to_dict()["server"], {"host": "web1", "port": 9443})
        import collections.abc
        self.assertIsInstance(cfg, collections.abc.Mapping)
        d = dict(cfg)
        self.assertEqual(d["server.port"], 9443)
        self.assertEqual(len(cfg), len(d))
        self.assertEqual(sorted(cfg), sorted(d))
        self.assertNotIn("server.nope", cfg)
        self.assertNotIn(7, cfg)

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

    def test_one_define_matches_the_typed_calls(self):
        # D-74: the same contract, byte for byte, from either spelling.
        typed = (cf.Builder()
                 .define_int("server.port", 8080, 1, 65535)
                 .define_text("server.host", "0.0.0.0")
                 .define_bool("tls.enabled", False)
                 .define_text("tls.ca", None)
                 .create())
        one = (cf.Builder()
               .define("server.port", 8080, min=1, max=65535)
               .define("server.host", "0.0.0.0")
               .define("tls.enabled", False)
               .define("tls.ca", type=str)
               .create())
        self.assertEqual(one.schema("t"), typed.schema("t"))
        self.assertEqual(one.server.port, 8080)
        self.assertIs(one.tls.enabled, False)
        with self.assertRaises(TypeError):
            cf.Builder().define("k")
        with self.assertRaises(TypeError):
            cf.Builder().define("k", "8080", type=int)
        with self.assertRaises(TypeError):
            cf.Builder().define("k", [1])

    def test_a_consumed_builder(self):
        b = cf.Builder()
        b.create()
        with self.assertRaises(RuntimeError):
            b.create()

    def test_read_coverage(self):
        self.assertFalse(server_cfg().tracks_reads, "off unless asked for")
        with mock.patch("configforge._native.HAVE_READ_TRACKING", False):
            with self.assertRaisesRegex(RuntimeError, "does not support read tracking"):
                cf.Builder().track_reads()
            plain = server_cfg()
            self.assertFalse(plain.tracks_reads)
            self.assertFalse(plain.was_read("server.port"))
            self.assertEqual(plain.read_count, 0)
        if not native.HAVE_READ_TRACKING:
            return
        cfg = (cf.Builder()
               .memory("file", rank.USER_FILE, {"server.port": 9443})
               .define_int("server.port", 8080, 1, 65535)
               .define_text("server.host", "0.0.0.0")
               .define_bool("tls.enabled", False)
               .track_reads()
               .create())
        self.assertTrue(cfg.tracks_reads)
        cfg.explain("server.port")
        cfg.keys()
        self.assertFalse(cfg.was_read("server.port"), "explaining and listing are not reads")
        self.assertEqual(cfg.server.port, 9443)
        self.assertIn("tls.enabled", cfg)
        self.assertTrue(cfg.was_read("server.port"))
        self.assertTrue(cfg.was_read("tls.enabled"))
        self.assertFalse(cfg.was_read("server.host"))
        self.assertEqual(cfg.read_count, 2)
        self.assertEqual(cfg.derive("admin", {}).read_count, 0, "a derived snapshot starts afresh")


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
            st = live.status
            self.assertEqual((st.generation, st.reloads, st.failures, st.pending), (2, 2, 1, False))
            self.assertIsNone(st.last)
            self.assertIn("lots", st.last_error.message)
        self.assertEqual(cfg.server.port, 8080, "a snapshot outlives its Live")

    def test_probation_and_rollback(self):
        healthy = [True]

        def health(cfg):
            if not healthy[0]:
                raise RuntimeError("upstream answered 503 with debug=%s" % cfg.app.debug)
            return True

        with cf.Live(cf.Mode.AUTO, self.build, probation=2, health=health) as live:
            self.write("[app]\ndebug = true\n")
            self.assertEqual(live.poll(), cf.Reload.SWAPPED)
            self.assertTrue(live.status.probation)
            self.assertEqual(live.rollback("errors doubled"), cf.Reload.ROLLED_BACK)
            self.assertIs(live.get().app.debug, False, "back to the snapshot before")
            self.assertEqual(live.poll(), cf.Reload.UNCHANGED,
                             "the file it rolled back from does not come back")
            self.assertIs(live.get().app.debug, False)
            st = live.status
            self.assertEqual((st.rollbacks, st.probation, st.last), (1, False, cf.Reload.ROLLED_BACK))
            with self.assertRaises(cf.Error):
                live.rollback()
            self.assertEqual(live.reload(), cf.Reload.SWAPPED, "asked for, it is a retry")
            healthy[0] = False
            self.assertEqual(live.poll(), cf.Reload.ROLLED_BACK, "the health function says no")
            self.assertIs(live.get().app.debug, False)
            healthy[0] = True
            self.write("[app]\nworkers = 3\n")
            self.assertEqual(live.poll(), cf.Reload.SWAPPED)
            self.assertTrue(live.confirm())
            self.assertFalse(live.status.probation)
            self.assertFalse(live.confirm(), "nothing left to confirm")

    def test_reloads_do_not_keep_every_builds_strings(self):
        with cf.Live(cf.Mode.MANUAL, self.build) as live:
            for _ in range(50):
                live.reload()
            self.assertLessEqual(len(live._kept), 1)
            self.assertIs(live.get().app.debug, False)

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


class Editing(unittest.TestCase):
    def test_an_edit_is_checked_at_commit(self):
        cfg = server_cfg()
        e = cf.edit(cfg, "file")
        e["server.port"] = 9443
        e["server.host"] = "web2"
        del e["tls.enabled"]
        self.assertEqual(len(e), 3)
        with self.assertRaises(cf.Error) as x:
            e.commit("note")
        self.assertIn("cannot be written", str(x.exception))
        with self.assertRaises(RuntimeError):
            e.commit()

    def test_a_block_that_raises_writes_nothing(self):
        cfg = server_cfg()
        with self.assertRaises(KeyError):
            with cf.edit(cfg, "file") as e:
                e.set("server.port", 1)
                raise KeyError("stop")
        with self.assertRaises(TypeError):
            cf.edit(cfg, "file").set("server.port", [1])


class Logging(unittest.TestCase):
    class Capture(logging.Handler):
        def __init__(self):
            super().__init__(logging.DEBUG)
            self.records = []

        def emit(self, record):
            self.records.append(record)

    def setUp(self):
        self.cap = self.Capture()
        self.logger = logging.getLogger("configforge.test")
        self.logger.addHandler(self.cap)
        self.logger.setLevel(logging.DEBUG)

    def tearDown(self):
        self.logger.removeHandler(self.cap)

    def test_a_warning_is_a_log_record(self):
        cfg = (cf.Builder()
               .memory("file", rank.USER_FILE, {"server.prot": "1"})
               .define_int("server.port", 8080, 1, 65535)
               .unknown(cf.UNKNOWN_WARN)
               .log(self.logger)
               .create())
        self.assertEqual(cfg.server.port, 8080)
        r = [x for x in self.cap.records if getattr(x, "cf_event", None) == "unknown-key"]
        self.assertEqual(len(r), 1)
        self.assertEqual(r[0].levelno, logging.WARNING)
        self.assertEqual(r[0].cf_key, "server.prot")
        self.assertIn("unknown key", r[0].getMessage())

    def test_reloads_are_logged(self):
        values = {"server.port": "8080"}

        def build():
            return (cf.Builder().memory("m", rank.USER_FILE, dict(values))
                    .define_int("server.port", 80, 1, 65535, flag.RESTART))

        with cf.Live(cf.Mode.MANUAL, build, logger=self.logger, level=logging.DEBUG) as live:
            values["server.port"] = "9443"
            live.reload()
            now = live.get().fingerprint
        events = [getattr(x, "cf_event", None) for x in self.cap.records]
        self.assertIn("reload", events)
        self.assertIn("restart-pending", events)
        reload = [x for x in self.cap.records if getattr(x, "cf_event", None) == "reload"][-1]
        self.assertEqual(reload.cf_fingerprint, now, "the reload names the new fingerprint")
        # The port is RESTART: readers keep 8080, so what they see - and with
        # it the fingerprint - is what it was.
        self.assertEqual(reload.cf_previous, now, "a pinned change leaves it as it was")
        self.assertRegex(now, r"^fnv1a:[0-9a-f]{16}$")

    def test_fingerprint_counts_a_secret_by_digest(self):
        def cfg(secret, port):
            return (cf.Builder()
                    .memory("m", rank.USER_FILE, {"db.password": secret, "port": port})
                    .define_text("db.password", None, flag.SENSITIVE)
                    .define_int("port", 1, 1, 65535)
                    .create())

        a = cfg("hunter2", "80")
        self.assertEqual(a.fingerprint, cfg("hunter2", "80").fingerprint)
        self.assertNotEqual(a.fingerprint, cfg("hunter3", "80").fingerprint)
        self.assertNotEqual(a.fingerprint, cfg("hunter2", "81").fingerprint)
        self.assertNotEqual(a.fingerprint, "fnv1a:%016x" % a.digest,
                            "the digest hashes the secret itself; the fingerprint does not")


class Library(unittest.TestCase):
    def test_version_and_features(self):
        self.assertTrue(cf.version().startswith(cf.__version__))
        self.assertTrue(cf.has_feature("ini"))
        self.assertFalse(cf.has_feature("no-such-thing"))


if __name__ == "__main__":
    unittest.main(verbosity=1)
