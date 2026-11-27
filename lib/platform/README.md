# platform

The operating system, chosen by the build and never by a macro in the code above it: the environment, UTF-8 arguments, directory listing, file stamps and the change handle, as `posix`, `win32` or `none`. The serialization library takes the file opener only.

* **Depends on**: port, numeric, base
* **Public headers**: `cf_platform.h`, `cf_platform_file.h`, `cf_file.h` (the file layer: `cf_file_writer`, `cf_file_replace`, a file written whole or not at all; D-224, D-127)
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: none of its own: the tests that use it are config's

## Build it alone

```sh
cmake -S lib/platform -B build-platform -DBUILD_TESTING=ON
cmake --build build-platform
ctest --test-dir build-platform
```

This builds a static library `cf_platform` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
