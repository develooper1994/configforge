# cxx

`configforge.hpp`, the C++03 wrapper and its compile-time layer. It is headers only: nothing of it is compiled into a library.

* **Depends on**: config
* **Public headers**: `configforge.hpp`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/cxx -B build-cxx -DBUILD_TESTING=ON
cmake --build build-cxx
ctest --test-dir build-cxx
```

This builds an interface library `cf_cxx` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
