# numeric

Numbers read, and floating point read and written. It uses only the portability layer.

* **Depends on**: port
* **Public headers**: `cf_numeric.h`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/numeric -B build-numeric -DBUILD_TESTING=ON
cmake --build build-numeric
ctest --test-dir build-numeric
```

This builds a static library `cf_numeric` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
