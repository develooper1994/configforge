# port

The compiler's door: `cf_port.h` decides which compiler, which standard and which atomics, the build options say which limits, and the bounded formatter and the integer writers need nothing else. It uses nothing, and every other component uses it.

* **Depends on**: -
* **Public headers**: `cf_build_options.h`, `cf_port.h`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/port -B build-port -DBUILD_TESTING=ON
cmake --build build-port
ctest --test-dir build-port
```

This builds a static library `cf_port` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
