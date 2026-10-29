# ConfigForge for Python

Configuration whose every value can say why it is what it is: which
source won, which lost, and where each was set. A binding over the C
library's shared build, through ctypes, with the standard library only.

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

Build the library with `-DBUILD_SHARED_LIBS=ON` and point
`CONFIGFORGE_LIB` at it, or put it beside the package. `configforge_doctor`
drives the Doctor program from Python. See `docs/Guide.md`, section 13.
