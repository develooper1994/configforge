# text

What the readers share of text: UTF-8 decoded, validated and encoded, one
hexadecimal digit read, and the seeded hash of a reader's name table (D-102).
It is internal: every declaration of `cf_text.h` is inside `CF_INTERNAL_API`,
outside the 1.x promise.

* **Depends on**: port, numeric, base, lanes
* **Public headers**: `cf_text.h`
* **Version**: ConfigForge's: `CF_VERSION_*` in `cf_version.h` (`lib/config`).
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/text -B build-text -DBUILD_TESTING=ON
cmake --build build-text
ctest --test-dir build-text
```

This builds a static library `cf_text` with what it depends on (the
directories beside it; lanes under ConfigForge's prefix, as `cf_lanes`) and
runs its tests. Inside ConfigForge the same sources go into `libconfigforge`
and `libconfigforge_serialize` (D-100).
