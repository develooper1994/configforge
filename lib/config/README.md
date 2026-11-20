# config

The configuration library: the key space, definitions and validation (`core`), the sources that produce candidates (`sources`), the bridge between key space and bytes (`bridge`) and the diagnostics that explain a resolution (`diag`).

* **Depends on**: port, numeric, base, platform, serialize
* **Public headers**: `cf_define.h`, `cf_edit.h`, `cf_emit.h`, `cf_entry.h`, `cf_explain.h`, `cf_flatten.h`, `cf_image.h`, `cf_iterate.h`, `cf_keyspace.h`, `cf_live.h`, `cf_log.h`, `cf_outcome.def`, `cf_report.h`, `cf_source.h`, `cf_version.h`, `configforge.h`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/config -B build-config -DBUILD_TESTING=ON
cmake --build build-config
ctest --test-dir build-config
```

This builds a static library `cf_config` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
