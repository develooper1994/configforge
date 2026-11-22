# base

What every component shares: allocation, errors and the value model (`cf_value`, which holds numbers).

* **Depends on**: port, numeric
* **Public headers**: `cf_alloc.h`, `cf_code.def`, `cf_reason.def`, `cf_error.h`, `cf_type.def`, `cf_value.h`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/base -B build-base -DBUILD_TESTING=ON
cmake --build build-base
ctest --test-dir build-base
```

This builds a static library `cf_base` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
