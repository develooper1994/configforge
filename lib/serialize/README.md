# serialize

Readers, writers, codecs and document events: INI, JSON, JSON5, TOML, YAML, XML, properties, CBOR and TLV, behind one codec interface. With the files it calls it is also a library of its own, `configforge_serialize` (D-90).

* **Depends on**: port, numeric, base, platform (its file opener, `cf_platform_file.h`, only)
* **Public headers**: `cf_serialize.h`, `cf_serialize_version.h`, `cf_yamlparse.h`
* **Version**: its own: `CF_SERIALIZE_VERSION_*` in `cf_serialize_version.h`.
* **Tests**: `tests/`

## Build it alone

```sh
cmake -S lib/serialize -B build-serialize -DBUILD_TESTING=ON
cmake --build build-serialize
ctest --test-dir build-serialize
```

This builds a static library `cf_serialize` with what it depends on (the
directories beside it) and runs its tests. Inside ConfigForge the same
sources go into `libconfigforge` and `libconfigforge_serialize` (D-100).
