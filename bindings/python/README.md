# ConfigForge for Python

Configuration whose every value can say why it is what it is: which
source won, which lost, and where each was set. A binding over the C
library's shared build, through ctypes, with the standard library only
at runtime. Wheels carry that shared library beside the Python code.

From the repository root, using an activated Python virtual environment:

```sh
python -m pip install .
```

Building from source needs CMake and a C compiler. The root
`pyproject.toml` uses scikit-build-core, so both the wheel and source
distribution include what they need without another checkout:

```sh
python -m pip install build
python -m build
```

The default `build` command creates a source distribution and builds the
wheel from it. Wheels are platform-specific, not tied to a CPython ABI.
Nothing is uploaded or published by these commands.

```python
import dataclasses
import configforge as cf

@dataclasses.dataclass
class Settings:
    port: int = cf.setting("server.port", 8080, range=(1, 65535))
    debug: bool = cf.setting("app.debug", False)

cfg = cf.define(cf.Builder.app("myapp")
                  .file("app.ini", cf.rank.USER_FILE, "app.ini", flags=cf.OPTIONAL),
                Settings).create()
print(cfg.server.port, cf.read(cfg, Settings).debug)
print(cfg.explain("server.port"))
```

Declare scalar lists with `define_list`. The element type is `str`,
`int`, `bool` or `float` (when reals are built); defaults are copied.

```python
cfg = (cf.Builder()
       .define_list("tags", str, ["base"], append=True, aliases=["t"])
       .env("env", cf.rank.ENV, "APP_", {"APP_TAGS": "one,two"})
       .cli("cli", cf.rank.CLI, ["app", "-t", "three"])
       .create())
tags = [cfg["tags.%d" % i] for i in range(cfg["tags.count"])]
assert tags == ["base", "one", "two", "three"]
```

Without `append`, the winning count replaces the length. An empty
environment value is an empty list; an empty CLI argument is an empty
element. `separator=""` disables splitting. `min`, `max`, `allowed`,
`policy`, `aliases` and definition flags apply through the native list
contract. A custom older library without `has_feature("lists")` is
refused when this method is used rather than silently defining a scalar.

For an existing custom shared build, `CONFIGFORGE_LIB` still overrides
the bundled library; the library's limits must match the binding.
`configforge_doctor` drives a separately installed Doctor program from
Python. See `docs/Guide.md`, section 13.
