# F - writing files, and safety

The library writes no file of its own today. This group is what writing
one needs, in the order it can be built: a file replaced whole or not at
all (F-1), a file the library owns and keeps what a user changed (F-2),
every operation on a file's data written back in its own format, keeping
the layout where the format has one (F-3, wave 6), and the drawing of the
references (F-4). The Roadmap's items are "A file written whole or not at
all", "An override file the library owns", "Every operation on a file's
data, written back in its format" (wave 6) and "The references drawn".

Each card is read cold against the tree, by someone who did not write it,
before it opens (docs/Plan.md, "Read again after the layout changes"). Line
numbers are those of the tree at 2395d65; a card that finds them moved
finds the symbol by name, and a symbol that is gone is a "stop if".

Four choices are the maintainer's and are marked **TO DECIDE**: the public
shape of the file write (F-1b), of the override source (F-2a) and of the
document edit (F-3j), and whether `doctor graph` is built at all (F-4b).
Every card around them is written so that an answer is the only thing
missing. A card whose TO DECIDE has no answer stops at that step, with
everything before it done and pushed.

Decisions of this group: D-127 (a file replaced whole or not at all, F-1a),
D-128 (the override file, F-2a), D-129 (an edit is a splice, checked by
reading it back, F-3a), D-130 (what each editor does, F-3a), D-131 (TLV
written, F-3h), D-132 (`doctor graph`, F-4b).

---

## F-1a - cf_platform_write_file: a file replaced whole or not at all

* **id**: F-1a
* **goal**: `cf_platform_write_file(path, data, size, err)` makes a file hold exactly `data`, on POSIX and on Windows, by a temporary file in the same directory flushed to the device and put in place by one rename; at every step that can fail, a test fails it and finds the old file intact and no temporary left.
* **why**: nothing the library writes may be half a file that the next start reads. `examples/c/settings.c:66` writes with `fopen(..., "wb")` over the old file (a crash or a full disk leaves neither), and wave 6 (F-3) will write users' files. The Roadmap item is "A file written whole or not at all" (`docs/Roadmap.md:714`).
* **needs**: nothing
* **size**: medium (three platform files of the same shape, one test)
* **decided**:
  * **Where.** The platform layer's file opener's own files, so that the serialization library carries it: `lib/platform/src/posix/file.c`, `win32/file.c`, `none/file.c` (the only platform sources of `SERIALIZE_SOURCES`, `lib/platform/CMakeLists.txt:35-36`). Declared in `lib/platform/include/configforge/cf_platform_file.h` after `cf_platform_fopen` (line 82-84), under `#if CF_INTERNAL_API` as it is (D-51, D-88: the platform tier is outside the 1.x promise; F-1b makes the public one). The header adds `#include "configforge/cf_error.h"` (base; the platform layer depends on it, `lib/platform/CMakeLists.txt:26-30`).
  * **Names.**
    ```c
    /* 1 when `path` holds exactly `size` bytes of `data`; 0 with `err` set
       (CF_E_SOURCE, source = path) and the old file, or its absence, as it was. */
    CF_API int      cf_platform_write_file(const char* path, const void* data, size_t size, cf_error* err);
    /* The same, failing at step `fail_at` (1 to cf_platform_write_steps()) as if that
       system call had failed; 0 is no failure. For the test: no global state (D-101). */
    CF_API int      cf_platform_write_file_at(const char* path, const void* data, size_t size, unsigned fail_at, cf_error* err);
    /* How many steps can fail: POSIX 6, Windows 5, none 0. */
    CF_API unsigned cf_platform_write_steps(void);
    ```
    `cf_platform_write_file` is `cf_platform_write_file_at(..., 0, ...)`.
  * **POSIX steps** (`posix/file.c`, C89 plus the POSIX calls, under its one guard; add `<errno.h>`, `<fcntl.h>`, `<string.h>`, `<sys/stat.h>`, `<unistd.h>`): before step 1, `lstat(path)`: a symbolic link is refused (`'%s' is a symbolic link; write the file it points to`), a file that is not a regular file is refused (`'%s' is not a regular file`), a missing file is fine. Then: **1** `open` the temporary `<dir>/.<base>.cf-tmp-<k>` with `O_WRONLY|O_CREAT|O_EXCL`, `k` from 0 to 99, the next `k` on `EEXIST` (all taken: `no temporary name is free next to '%s'`), mode `0666` when the original is missing (the umask applies, as `fopen` gave) and `0600` when it exists; **2** `write` until all `size` bytes are out (partial writes and `EINTR` handled; zero bytes is fine); **3** `fsync`; **4** `fchmod` to the original's `st_mode & 07777` when it exists (it succeeds trivially when it does not, and still counts as the step); **5** `close`, its result checked; **6** `rename(temp, path)`. After step 6, best effort and not a step: open `<dir>` (`.` when `path` has no `/`) read only, `fsync`, `close`, every failure ignored (the file is in place; reporting a failure then would tell the caller that nothing changed). A failure at step 2 to 6 closes the descriptor if open and `unlink`s the temporary before returning 0. The temporary of a crash is never read or removed by a later write: its name is taken and `k + 1` is used.
  * **Windows steps** (`win32/file.c`, `win32f_wide`, lines 25-32): before step 1, `GetFileAttributesW(path)`: a read-only file is refused (`'%s' is read-only`), a directory is refused as above. **1** `CreateFileW` the temporary (same name, wide) with `CREATE_NEW`, `GENERIC_WRITE`, `FILE_ATTRIBUTE_NORMAL`; **2** `WriteFile` until all bytes are out; **3** `FlushFileBuffers`; **4** `CloseHandle`; **5** `MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`. No attribute or ACL is copied: the new file takes the directory's, and the page says so. A failure deletes the temporary with `DeleteFileW`.
  * **None** (`none/file.c`): `cf_platform_write_file_at` returns 0 with `CF_E_CAPABILITY`, `this build has no files to write`; `cf_platform_write_steps` returns 0.
  * **Errors.** `cf_error_set(err, CF_E_SOURCE, NULL, path, "cannot write '%s': %s", path, why)`, `why` being `strerror(errno)` (Windows: `GetLastError()` as a number, `error %lu`), or `injected failure at step <k>` for `fail_at`. The pre-checks above have their own wording and no `cannot write` prefix. A NULL `path` or NULL `data` with `size > 0` is `CF_E_SOURCE`, `no file to write`.
  * **Rejected**: following a symbolic link (the rename would replace the link, and writing through it would need `realpath`, which C89 and Windows do not share: the caller names the target); a lock file (one writer is the rule, D-128); cleaning an old temporary (it may be another process's live one).
  * **D-127** (copy into `docs/Decisions.md` after D-112, titled "A file is replaced whole or not at all"): the Question is the Roadmap's (settings written with `fopen "wb"`); the Decision is the steps, the names, the symbolic-link and read-only refusals, the directory sync that is not a step and why, and the rejected list above; "Held to": `test_writefile` fails each step.
* **files**: `lib/platform/include/configforge/cf_platform_file.h`, `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/none/file.c`, `lib/config/tests/test_writefile.c` (new), `lib/config/tests/CMakeLists.txt` (add `test_writefile` after `test_file`, line 47), `cmake/configforge.internal.symbols` (the three names, in the file's order), `lib/platform/README.md` (one line: the writer); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/F-1a main`.
  2. Write `lib/config/tests/test_writefile.c`, C89, `#include <configforge/cf_platform.h>`, `"cf_test.h"` (`check`, `check_text`, `spill`, `tests/support/cf_test.h:30-49`), run in the build directory. Cases: (a) a new file `cf_wf.txt` is written and reads back byte for byte; (b) an existing file is replaced and its content is the new one, for sizes 0, 1 and 3 MiB (partial writes); (c) POSIX only (`#if CF_PLATFORM_POSIX`): `chmod 0640` the file first (`<sys/stat.h>` declared by a `#include` under that guard) and see `st_mode & 07777` still 0640; (d) for `step` from 1 to `cf_platform_write_steps()`: with `cf_wf.txt` holding `old`, `cf_platform_write_file_at(..., "new", 3, step, &err)` returns 0, `err.message` contains `injected failure at step <step>`, `cf_wf.txt` still reads `old`, and `fopen(".cf_wf.txt.cf-tmp-0", "rb")` is NULL; (e) the same with no original: after every failed step `fopen("cf_wf_new.txt","rb")` is NULL; (f) `cf_platform_write_file(".", ...)` is refused with `is not a regular file` (POSIX) and a NULL path with `no file to write`; (g) POSIX: `system("ln -s cf_wf.txt cf_wf.lnk")` (skip the case when it returns non-zero), then writing `cf_wf.lnk` is refused with `is a symbolic link` and `cf_wf.txt` is unchanged. Add the name to the list in `lib/config/tests/CMakeLists.txt`. It does not link yet.
  3. See it fail: `cmake --preset default && cmake --build --preset default --target test_writefile 2>&1 | tail -n 20` fails to link (`cf_platform_write_file` undefined). Say so in the commit message.
  4. Add the declarations and `posix/file.c`, `none/file.c`, `win32/file.c`, and the three lines of `cmake/configforge.internal.symbols`. `cmake --build --preset default && ctest --preset default -R "test_writefile|gate" 2>&1 | tail -n 30` passes on the platform at hand.
  5. See that the test can fail: in `posix/file.c` remove the `unlink` of the temporary on a failure (the line itself, with a run-time-false condition such as `if (fail_at == 0xFFFFu)` in front of it, never a constant: negative-proof rule for MSVC); `ctest --preset default -R test_writefile` fails case (d) on the leftover temporary; put it back.
  6. Add the D-127 text to `docs/Decisions.md`; the CHANGELOG entry (docs below).
  7. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake` and `cmake -P scripts/generate/apiref.cmake`, last. Commit `F-1a: cf_platform_write_file, a file replaced whole or not at all (D-127)`.
  8. ACCEPT; push `work/F-1a`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (case (c) and (g) are skipped there, and cases (d) and (e) run); ACCEPT-CI; `ctest --preset default -R test_writefile` exit 0 in the `default` and `minimal` presets (`minimal` has no `CF_WITH_EMIT`: the test uses nothing of it).
* **stop if**: `gate.layering` or `gate.symbols` refuses the new header include or the names and the fix is not one line of `cmake/configforge.internal.symbols`; the amalgamation (`gate.amalgamation`) fails because the three `file.c` now need a header the amalgam does not carry; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": `cf_platform_write_file` (internal tier): a file replaced by a temporary in its directory, flushed and renamed over it, on POSIX and Windows; a failure at any step leaves the old file (F-1a, D-127). `docs/Decisions.md`: D-127. The Roadmap row is marked by F-3j, which finishes what the item says.

---

## F-1b - A public call that writes a configuration to a file whole or not at all (TO DECIDE)

* **id**: F-1b
* **goal**: a program writes `cf_emit`'s document to a file with one public call that uses F-1a, `examples/c/settings.c` uses it, and no example that saves opens a file with `fopen "wb"` for its settings.
* **why**: F-1a is the internal tier; the example and every program that saves can only use the public one. The Roadmap item's "Done when ... every example that saves use it" (`docs/Roadmap.md:723-725`).
* **needs**: F-1a
* **size**: small
* **decided**:
  * **TO DECIDE (the shape of the public interface).** Options: **A** `cf_emit_file(c, path, format, what, rank, err)` in `lib/config/include/configforge/cf_emit.h` after `cf_emit` (line 52), which emits and replaces the file. **B** `cf_file_replace(path, data, size, err)`, the bytes only, in `configforge.h`; each program emits into its own buffer first, as `settings.c:61-70` does. **C** both. **Recommended: A.** It is one new public name, it keeps the "refuse rather than truncate" rule of `cf_emit` (nothing is written when the emit is refused, so the old file stays), and it serves every program that saves a configuration, which is all the Roadmap names; wave 6 writes bytes through the internal function (F-3). B would add a second way to do what A does. The steps below are A's; for B or C the maintainer says so under "Organiser's answers" and the steps follow.
  * **A's shape.** `CF_API cf_bool cf_emit_file(const struct cf_config* c, const char* path, const char* format, cf_emit_what what, int rank, cf_error* err);` `format` NULL: `cf_format_of_path(path)`; still NULL: `CF_E_CAPABILITY`, `cannot tell the format of '%s' from its name; name it`. It measures with `cf_emit(c, format, what, rank, NULL, 0, err)` (`emit.c:230`: a refusal returns 0 with `err` set, nothing is written), allocates `n + 1` bytes from `cf_default_allocator()` (`lib/base/src/alloc.c:33-41`), emits into it, calls `cf_platform_write_file(path, buf, n, err)` and frees. Returns `CF_TRUE` when the file holds the document. Without `CF_WITH_EMIT` it is the `CF_E_CAPABILITY` of `cf_emit`'s own fallback (`emit.c:350-360`). The error is cleared first (`cf_error_clear`, as `cf_write` does, `config.c:2051`).
  * **Not in this card**: the C++, Rust and Python bindings. If a gate demands a wrapper for every public C function, stop and say which.
* **files**: `lib/config/include/configforge/cf_emit.h`, `lib/config/src/bridge/emit.c`, `lib/config/tests/test_emit.c`, `examples/c/settings.c`, `docs/Guide.md` (lines 977-981), `CHANGELOG.md`; last: `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE. `git switch -c work/F-1b main` (after F-1a has landed).
  2. In `test_emit.c` add `test_emit_file`: a configuration emitted to `cf_ef.json` and read back by `cf_builder_file` gives the same `server.port`; `format` NULL with `.toml` writes TOML; a name with no known extension and NULL format fails with `cannot tell the format`; an `ini` file asked to hold an array is refused (`cf_emit`'s message) and a file `cf_ef.ini` written before holds its old bytes; a path in a directory that does not exist fails with `cannot write` and the path.
  3. See it fail: `cmake --build --preset default --target test_emit 2>&1 | tail -n 20` (undeclared `cf_emit_file`).
  4. Add the declaration and the function (inside `#if CF_WITH_EMIT`, and the fallback beside `emit.c:350`). `ctest --preset default -R test_emit`.
  5. `examples/c/settings.c`: `save()` (lines 56-77) becomes `return cf_emit_file(cfg, SETTINGS_FILE, "json", CF_EMIT_CONFIGURED, 0, err) ? 1 : 0;` without `buf`, `f` and `n`. The stdio `fopen` at 170 is the example standing in for an editor and stays. `ctest --preset default -R example` (the example's output is checked by the suite and does not change).
  6. `docs/Guide.md:977-981`: the sentence says to use `cf_emit_file` and that the file is replaced whole or not at all.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `F-1b: cf_emit_file, a configuration written to a file whole or not at all`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_emit|example.settings"` exit 0.
* **stop if**: no answer to the TO DECIDE; `settings.c`'s output changes (the example's test compares it); a gate wants the bindings.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": `cf_emit_file` writes a configuration to a file whole or not at all; `examples/c/settings.c` uses it (F-1b). No new D-number (D-127 holds it). The Roadmap row is marked by F-3j.

---

## F-2a - An override file the library owns: a file source that cf_write can commit to (TO DECIDE)

* **id**: F-2a
* **goal**: `cf_builder_overrides(b, name, rank, path, flags)` adds a file read last, as `cf_builder_file` does, that `cf_write` commits to once the program has granted it (`cf_builder_writable`): the file is read, the changes applied, the whole document written back in its own format through F-1a's function, and the provenance names the file like any other.
* **why**: between the store (SQLite, D-42) and wave 6 there is the common case `examples/c/settings.c` writes by hand: a program keeps what its user changed in a file of its own, and a store may not be there (`docs/Design.md:1982-1986`: "the built-in overrides file where it is not"). The Roadmap item is "An override file the library owns" (`docs/Roadmap.md:726`).
* **needs**: F-1a
* **size**: large (a source, a refactor of `cf_emit`, a contract for what a write does to a file)
* **decided**:
  * **TO DECIDE (the shape of the public interface).** Options: **A** `CF_API cf_bool cf_builder_overrides(cf_builder* b, const char* name, int rank, const char* path, unsigned flags);` in `configforge.h` after `cf_builder_file` (line 152), the format told by the file's name. **B** the same with a `const char* format` argument, as `cf_builder_file` has. **C** no new call: `cf_builder_file` gains a flag `CF_WRITABLE_FILE` that gives its source the write capability. **Recommended: A.** A derived configuration's stand-in for a source keeps the path and nothing else (`derive_ghost`, `config.c:1580-1608`, copies `vt->write` and the path), so the format must be a function of the path; B would need the format to travel in the ghost, and C overloads a flag word that `cf_builder_file` also gives to the encoding (`CF_ENCODING_MASK`, `file.c:482`). The steps are A's.
  * **What it is.** A file source (`file.c:479-500`) whose vtable differs in `write` only: a second `cf_source_vtable`, `cf_overrides_vt`, in `lib/config/src/bridge/file.c` after `cf_file_vt` (line 462), `kind` still `"file"` (so reports and Doctor see a file, as the Roadmap asks), `load`, `destroy` and `path` shared, `write = overrides_write`. `file_state_of` (`file.c:471-477`) accepts both vtables. `cf_source_file_overrides(cf_source* s)` (new, declared under `#if CF_INTERNAL_API` in `cf_source.h` after `cf_source_file`, line 284) sets `s->vt = &cf_overrides_vt`; `cf_builder_overrides` calls `cf_builder_file(b, name, rank, path, NULL, flags)` and then that on the source it added (the slot `builder_slot_file`, `config.c:402`, returned for it: `b->nsources - 1` after the call).
  * **At the builder.** `cf_codec_by_path(path)` must be a codec with `CF_CODEC_WRITE` and without `CF_CODEC_SCHEMA`: otherwise `builder_fail` with `CF_E_CAPABILITY`, `'%s' cannot be an overrides file: <no format is called by its name | '<format>' can be read but not written>`, the same message family as `cf_emit`'s (`emit.c:270-285`). `flags` with any `CF_ENCODING_MASK` bit is refused, `an overrides file is UTF-8`. Writing stays the program's grant (D-42: capability, permission, authorization): the call does not imply `cf_builder_writable` (rejected: it would make a source writable that its program never granted, against D-42's "every source is read only until the program grants it").
  * **overrides_write(s, items, count, meta, err)**, in order, nothing written until the last step: (1) `path = cf_source_path(s)`; the format is `cf_codec_by_path(path)`. (2) The current document: when `cf_platform_stamp(path, &st)` (`cf_platform.h:255`) says the file is there, `cf_blob_file(&blob, ar, path, err)` (`cf_serialize.h:63`) and `cf_source_read_blob(s, ks, codec, &blob, path, err)` (`cf_flatten.h`, "A document's bytes through a codec into the key space") into a fresh `cf_keyspace` at the source's rank, in an arena from the default allocator; a missing file is an empty document; a file the reader refuses is **refused**, `CF_E_SOURCE`, with the reader's message (`file:line:col: ...`), and nothing is written: a file the library cannot read is never replaced. (3) Each `CF_WRITE_SET` item is added with `cf_ks_add(ks, key, strlen(key), s->rank, &item.value, s->name)` (a later candidate of one rank wins, `cf_keyspace.h:160-162`); each `CF_WRITE_DELETE` item's key goes to a `drop` list. (4) `cf_ks_resolve(ks)`. (5) `cfi_emit_keyspace(ks, codec, drop, ndrop, NULL, 0, err)` measures, a buffer of that size plus one is taken from the default allocator, and the same call writes it. (6) `cf_platform_write_file(path, buf, n, err)`. `meta` is not stored (a file has no column for it; the store keeps it): the page says so.
  * **cfi_emit_keyspace** (new, `cf_emit.h` under `#if CF_INTERNAL_API`): `CF_API size_t cfi_emit_keyspace(const cf_keyspace* ks, const cf_codec* codec, const char* const* drop, size_t ndrop, char* buf, size_t cap, cf_error* err);` It is `cf_emit` with `CF_EMIT_ALL`, no definitions and a drop list. Refactor, no behaviour change of `cf_emit`: `emit_filter` (`emit.c:30-34`) gains `const char* const* drop; size_t ndrop;`; `keep` (`emit.c:36`) returns `CF_FALSE` for an entry whose key equals a drop key (`strlen` and `memcmp`) and checks `CF_TRANSIENT` only when `f->defs != NULL`; the body of `cf_emit` after the codec is found (`emit.c:252-344`: the `CF_CODEC_WRITE` check, the array probe, the encoder, the literal sink, `cf_doc_emit_where`) moves unchanged into `static size_t emit_run(const cf_keyspace* ks, const cf_codec* codec, const char* format, const emit_filter* f, char* buf, size_t cap, cf_error* err)`, which `cf_emit` and `cfi_emit_keyspace` both call.
  * **What a write does to a file**: the library owns it. The first write of a hand-written file loses its comments and its layout; the page says so and points at wave 6 for editing a user's file (F-3). One writer at a time: two processes writing the same file lose one update (read, change, replace); no lock (rejected: a lock file is a second thing that a crash leaves behind, and D-127 already refuses to clean one). A `CF_WRITE_DELETE` of a key that is not in the file is not an error. Deleting one element of a list or changing a list's count is refused before `overrides_write` is called (`cf_write`, `configforge.h:772-778`).
  * **D-128** (after D-127, "An override file is a file source that cf_write commits to"): Question: how a program keeps what its user changed when no store is there. Options: a writable file source (decided); a `cf_emit` call by the program (today; F-1b only makes it safe); wave 6's editing of the user's own file (a different thing: the user's layout kept). Decision: the bullets above, in the same order. Rejected: an implied grant, the format as an argument, a lock file, storing `meta`.
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_source.h`, `lib/config/include/configforge/cf_emit.h`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/emit.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_overrides.c` (new), `lib/config/tests/CMakeLists.txt` (after `test_write`, line 54); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE. `git switch -c work/F-2a main` (after F-1a has landed).
  2. Write `test_overrides.c`, C89, after `test_write.c:1-60`'s way of building a configuration with a contract. Cases: (a) no file yet: `cf_write(cfg, "user", {server.port=9000, log.level="debug"}, 2, "t", &err)` creates `cf_ov.json`, a new `cf_builder_file` build reads both, and `cf_explain` of `server.port` names `user` and `cf_ov.json:` with a line and a column; (b) a second write that sets `log.level` and adds `tls.enabled` keeps `server.port`; (c) `CF_WRITE_DELETE` of `log.level` removes it from the file and a rebuilt configuration shows the default again; (d) not granted: `CF_E_POLICY`, the file as it was (absent, or its bytes); (e) the contract refuses `server.port` = 99999: `CF_E_VALIDATE`, the file's bytes unchanged; (f) the file holds `{ bad`: the write returns `CF_E_SOURCE` with `cf_ov.json:1` in the message and the file's bytes unchanged; (g) `cf_derive` a configuration, then `cf_write` through the derived one works; (h) by name: `cf_ov.toml`, `cf_ov.ini` (a dotted key is a section), `cf_ov.yaml`, `cf_ov.cbor` (`#if CF_WITH_CBOR`; read back with `cf_builder_file`), each guarded by its `CF_WITH_*`; (i) `cf_ov.xml` and `cf_ov.zzz` fail `cf_builder_create` with `CF_E_CAPABILITY` and the words above; (j) `CF_ENCODING_UTF16LE` in `flags` fails the same way; (k) an emit refusal (`cf_ov.ini` and an array key) leaves the old bytes. Add `test_overrides` to the test list.
  3. See it fail: `cmake --build --preset default --target test_overrides 2>&1 | tail -n 20` (undeclared `cf_builder_overrides`); say so in the commit message.
  4. Refactor `emit.c` as decided and run `ctest --preset default -R "emit|roundtrip|golden"`: unchanged results before anything else is added.
  5. Add `cf_overrides_vt`, `cf_source_file_overrides`, `overrides_write`, `cf_builder_overrides` and the declarations. `ctest --preset default -R "test_overrides|test_write|test_file|test_emit"`.
  6. See it fail the other way: with `cf_ov.json` unreadable, make `overrides_write` skip the refusal (behind a run-time-false condition, never a constant); case (f) fails; put it back.
  7. `docs/Guide.md`: a paragraph after the one that holds `cf_emit` (line 979) on the override file, with the three things above (comments lost on the first write, one writer, `meta` not kept). D-128 into `docs/Decisions.md`.
  8. `python scripts/work/work.py format`; codemap and apiref, last. Commit `F-2a: cf_builder_overrides, a file source cf_write commits to (D-128)`.
  9. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_overrides|test_emit|test_write"` exit 0, and `ctest --preset minimal -R test_overrides` exit 0 (it builds, and says what is absent).
* **stop if**: no answer to the TO DECIDE; `cf_ks_add` of a second candidate at one rank does not make it win (case (b) fails for that reason): report, it changes how SET is applied; `derive_ghost`'s stand-in cannot write (case (g)); the refactor of `cf_emit` changes any `test_emit` or `test_golden` result.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": `cf_builder_overrides`: a file the library owns and `cf_write` commits to, in any format that is written (F-2a, D-128). `docs/Decisions.md`: D-128. Roadmap: "An override file the library owns" is marked by F-2b, whose example is its "Done when".

---

## F-2b - settings.c is the override source's example

* **id**: F-2b
* **goal**: `examples/c/settings.c` keeps what its user set in an override file through `cf_builder_overrides`, `cf_builder_writable` and `cf_write`, and writes no file itself; its output as a program is unchanged.
* **why**: the Roadmap item is done "when `settings.c` is that source's example and no longer writes a file itself" (`docs/Roadmap.md:739-742`).
* **needs**: F-2a, F-1b (F-1b's change to `save()` is what this card removes)
* **size**: small
* **decided**:
  * `load()` (`settings.c:25-49`): the `cf_builder_file(b, "settings", CF_RANK_USER_FILE, SETTINGS_FILE, NULL, CF_OPTIONAL)` call (line 36) becomes `cf_builder_overrides(b, "settings", CF_RANK_USER_FILE, SETTINGS_FILE, CF_OPTIONAL)` followed by `cf_builder_writable(b, "settings")`; the comment above it says what the file is.
  * The `--set` path (`settings.c:121-165`) no longer derives and saves: it builds one `cf_write_item` with the key, `cf_value_text` of the text after `=` (the contract types it: `cf_write` applies it as it applies a file's value), `op = CF_WRITE_SET`, and calls `cf_write(cfg, "settings", &change, 1, "settings --set", &err)`. On failure it prints `settings: <err.message>` and returns 1, as it does today. On success it prints `saved settings.json`, rebuilds with `load()` and prints `print()`'s line, exactly as the example prints now.
  * `save()` and the `cf_derive` call (lines 56-77 and 135-150) go. The watch demonstration (line 170) still appends a newline with `fopen "ab"`: it stands in for an editor and stays.
  * No library change. If the example's output differs by one byte, the card changes the example, not the test.
* **files**: `examples/c/settings.c`, `docs/Guide.md` (lines 44 and 977-981, where `settings.c` is described); last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/F-2b main` (after F-2a and F-1b).
  2. See the example's own test pass as it is: `ctest --preset default -R example.settings 2>&1 | tail -n 15` (the output it checks is this card's oracle).
  3. Make the change as decided; `cmake --build --preset default --target settings && ctest --preset default -R example.settings`.
  4. See it fail: make `load()` skip `cf_builder_writable`; `--set` must print the `may not be written: nothing granted it` message and the test must fail; put it back.
  5. Documents as below; `python scripts/work/work.py format`; codemap and apiref, last. Commit `F-2b: settings.c writes through the override source`.
  6. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R example` exit 0.
* **stop if**: the example's expected output (the test's) cannot be kept by a change of the example alone.
* **docs**: `CHANGELOG.md`, "### Changed": `examples/c/settings.c` keeps its user's settings in an override file (F-2b). `docs/Roadmap.md`: the item headed "An override file the library owns" becomes 🟩, with "(F-2a, F-2b)" (`docs/Roadmap.md:726`).

---

## F-3a - Wave 6, the core: a change spliced into a document and checked by reading it back

* **id**: F-3a
* **goal**: an internal engine, `cfi_docedit` in `lib/serialize`, applies a list of changes (set a scalar, remove a key) to a document's bytes by splicing only the bytes a format's editor names, reads the result back through the codec, and returns it only when it holds exactly the old document's keys and values with the changes applied; a codec with no layout (CBOR) is rewritten through its encoder instead; a format with neither is refused by name.
* **why**: wave 6 is "every operation on a file's data through its codec - read, add, change, remove, written back in its own format, keeping the layout where the format can" (`docs/Roadmap.md:144-146`, and the assessment's item, `docs/Roadmap.md:437-440`). `cf_emit.h:1-13` and `docs/Design.md:1943-1969` give the reason it was never attempted: a writer that gets the layout slightly wrong destroys what a person wrote. This card makes that impossible to do silently: nothing is returned that the reader does not read back as the old document plus the change.
* **needs**: nothing (the writes through F-1a are F-3i's and F-3j's)
* **size**: large (the engine, its checker, the editor interface, a toy editor for the tests, the decisions)
* **decided**:
  * **Not in the readers.** A reader already gives each scalar's place: `cf_doc_sink.scalar`'s `at` (`cf_serialize.h:126-150`, line and column in bytes from 1, after a byte order mark, `docs/Formats.md`), as the readers emit it (`json.c:663`, `725`; `toml.c:1285`, `1310`; `ini.c:469`, `494`; `properties.c:216`; `yaml.c:570`); a binary reader gives `byte N`. The engine uses that place to start; each editor finds where its own token ends by lexing that one token. No reader changes (the hot loops are P-3's), and a mistake of an editor's lexing is caught by the check below, not trusted.
  * **Where.** `lib/serialize/include/configforge/cf_docedit_engine.h` (new, every function under `#if CF_INTERNAL_API`, D-51) and `lib/serialize/src/docedit.c` (new). In `lib/serialize` because `configforge codec` links `configforge_serialize` and nothing else (`docs/Tools.md:208-212`); no key space, so the engine has its own path recorder: a `cf_doc_sink` that keeps, for every scalar, `path` (the names joined by `.`, an array's members by their index from 0, as `configforge codec dump` writes them, `docs/Tools.md:268-275`), its `cf_value`, `textual` and `at`.
  * **Interface.**
    ```c
    typedef struct cf_doc_change {
        const char* key;      /* a scalar's path, as dump prints it */
        cf_value    value;    /* for CF_DOC_SET */
        unsigned    op;       /* CF_DOC_SET = 0, CF_DOC_UNSET = 1 */
    } cf_doc_change;
    typedef struct cf_doc_pos { const char* path; size_t plen; cf_value value; cf_bool textual; unsigned line, col; size_t byte; } cf_doc_pos;
    typedef struct cf_doc_text { char* p; size_t n, cap; const cf_allocator* alloc; cf_bool failed; } cf_doc_text;
    /* An editor: one per format, a static table entry. `apply` changes `t` (the document so far)
       for ONE change, given the positions of the document as it is now. */
    typedef struct cf_doc_editor {
        const char* format;
        cf_bool (*apply)(cf_doc_text* t, const cf_doc_pos* pos, size_t npos, const cf_doc_change* ch, cf_error* err);
    } cf_doc_editor;
    CF_API const cf_doc_editor* cfi_doc_editor(const char* format);              /* NULL: none */
    CF_API cf_bool cfi_doc_splice(cf_doc_text* t, size_t from, size_t to, const char* bytes, size_t n);
    CF_API cf_bool cfi_docedit(const cf_codec* codec, const cf_doc_editor* ed, const cf_blob* in, const cf_doc_change* ch, size_t count, const cf_allocator* alloc, char** out, size_t* outn, cf_error* err);
    ```
    `cfi_docedit` returns the new bytes in memory from `alloc` (the caller frees them with `alloc->release`, `cf_alloc.h:22-26`), `*outn` their length; false with `err` set and `*out` NULL. `byte` in `cf_doc_pos` is computed by the engine from `line` and `col` (the start of line `line`, found by counting LF, plus `col - 1`, plus 3 on line 1 when `cf_blob_skip_bom`, `cf_serialize.h:54`, says so) for a text format, and parsed from `byte N` for a binary one.
  * **The dispatch** (D-129): `ed` is the format's editor when it has one (`cfi_doc_editor(codec->name)`); else, for a codec with `CF_CODEC_BINARY` and `CF_CODEC_WRITE` (CBOR, which has no layout to keep), the engine decodes the whole document into the path recorder, applies the changes to that list and encodes it again through `codec->encoder` (`cf_serialize.h:234`; the document walk is `cf_doc_emit` over the recorder's list: the same events `convert_core.c` sends); anything else is `CF_E_CAPABILITY`: `'<format>' cannot be edited in place: it has no editor` (XML is read only by decision, `docs/Design.md:1943`; `.env` is not a codec). A format's whole rewrite is never a silent fallback: comments and layout would be lost.
  * **One change at a time.** For each change: decode the current text into the recorder (positions), call `ed->apply`, replace the text. A document costs one read a change: O(size times changes), said in the header. `cfi_doc_splice` replaces bytes `from..to` (exclusive) by `bytes`; an out-of-range or reversed pair is `false` and `failed` (a bug of an editor, not of the input).
  * **The check** (the only thing that makes the result trustworthy): the expected list is the original recorder list with each change applied (SET: the path's entry gets the value, or is added when new; UNSET: removed); after the last change the new text is decoded again and its list must equal the expected one in **order-free** terms: same set of paths, each with the same type (`cf_value.type`), the same value (`CF_NULL` equal; `CF_BOOL` and `CF_INT` by value; `CF_REAL` by the bits of the double; `CF_TEXT` by length and bytes) and the same `textual`. A difference is `CF_E_SOURCE`, source = the format's name, `editing '<key>' would change '<other>' (<what differed>); nothing was written`, and the caller is told it is a bug (`reason` is left `CF_REASON_NONE`; the tool of F-3i maps this refusal to exit 3). The checker is the first thing written and has its own test.
  * **What a change may be** (D-130, copy into `docs/Decisions.md` as the table "What an editor does", titled "What a wave 6 editor changes, adds and removes"): **set an existing scalar**: only its value's token is replaced; the key, the spacing, the comments and the line ending stay. **Set a new key**: it is added at the end of its container, in the container's own style (each format's editor says where, F-3b..g); a missing parent is created. **Unset**: the entry is removed with the separators that belong to it alone and nothing else; a container emptied stays in JSON, JSON5 and TOML, is written `{}` in a YAML block mapping, and an INI section header stays. **An array**: a member is set by its index (`a.0`); adding or removing one, or naming `a.count`, is `CF_E_CAPABILITY`: `'<key>' is in an array; an array's length is changed by writing the array`. **A key naming a table or an array** is `CF_E_KEY`: `'<key>' is a table, set its members`. **A key that is a value and a container** (set `a` when `a.b` exists) is `CF_E_KEY`. A value of a type the format cannot hold is written as the format's encoder writes it (`cf_format_holds`, `cf_serialize.h:304`, D-109): text in a format with no types, refused with `CF_E_CAPABILITY` (`'<format>' cannot hold a null`) when `CF_REFUSES_NULL` is set. **A key that does not exist: unset** is not an error and changes nothing.
  * **The value's own token.** Each editor writes a scalar as its format's encoder does: the card of each format factors its encoder's scalar function (`js_enc_scalar` `json.c:859`, `toml_enc_scalar` `toml.c:1641`, `ini_enc_scalar` `ini.c:640`, `yaml_enc_scalar` `yaml.c:1253`) into `cfi_<format>_put_value(cf_sink*, const cf_value*)` in its own file, declared in `cf_docedit_engine.h`.
  * **D-129** (after D-128, "A document edit is a splice, and it is checked by reading it back"): Question: how to edit a hand-written file without destroying what a person wrote (`cf_emit.h:1-13`). Options: **a concrete syntax tree** every codec maintains (rejected: it is the reader's second copy, in every format, and "slightly wrong" is silent); **rewrite through the encoder** (rejected for text formats: comments and layout are lost; used for CBOR, which has neither); **a splice at the place the reader gives, checked by reading back** (decided). Decision: the bullets above. Held to: `test_docedit`'s checker cases, and each format's sweep (F-3b..g).
* **files**: `lib/serialize/include/configforge/cf_docedit_engine.h` (new), `lib/serialize/src/docedit.c` (new), `lib/serialize/CMakeLists.txt` (sources), `lib/config/tests/test_docedit.c` (new: in config's tests because `tests/support/cf_kv_codec.h` includes `configforge.h`; it reaches the engine through `libconfigforge`, which carries the serialization sources, D-100), `lib/config/tests/CMakeLists.txt` (`test_docedit` after `test_write`, line 54), `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3a main` (after F-1a and F-2a).
  2. Write `test_docedit.c`, C89, with a toy editor of its own over the `kv` codec of `tests/support/cf_kv_codec.h` (one `key: value` a line, `#` comments; "the shape of an integration with another parser"): `kv_apply` replaces the text after `: ` on the key's line (set), appends `key: value\n` (add), deletes the line (unset). Cases: (a) positions: `cfi_docedit` on `# c\nport: 8080\nhost: a\n` records `port` at line 2, col 7; (b) set keeps the comment and the other line byte for byte; (c) add appends; (d) unset removes one line; (e) the checker: a deliberately wrong editor (replaces a different line) is refused with `would change` and `*out == NULL`; an editor that returns a document the reader refuses is refused with the reader's message; (f) a change naming `a.count`, a table, or an array's end is refused with the words above (use a document in the JSON codec of the build for the array and table cases, `#if CF_WITH_JSON`, through a toy editor that must never be reached); (g) `cfi_docedit` on `cbor` (`#if CF_WITH_CBOR`, `CF_WITH_EMIT`): set one value of a document and see the others unchanged, with no editor; (h) `xml` is `CF_E_CAPABILITY` with the words above (`#if CF_WITH_XML`); (i) `cfi_doc_splice` with `to < from` is false.
  3. See it fail: `cmake --build --preset default --target test_docedit 2>&1 | tail -n 20` (no `cf_docedit_engine.h`); say so in the commit.
  4. Write the header and `docedit.c`: the recorder, the byte computation, the splice, the dispatch, the checker; cases (a) and (i) pass first, then the rest. `ctest --preset default -R "test_docedit|gate"`.
  5. See that the checker can fail: make the toy editor in case (b) splice one byte past the value (behind a run-time-false condition); (b) must now be refused by the checker, not by the test's own comparison; put it back.
  6. D-129 and D-130 into `docs/Decisions.md`; the CHANGELOG entry; `cmake/configforge.internal.symbols`.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `F-3a: the document edit engine, a splice checked by reading it back (D-129, D-130)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_docedit` exit 0 in `default`, `portable` (C89, no later standard) and `minimal`.
* **stop if**: a reader's `at` is not the first byte of a scalar's own token for JSON, TOML, INI, properties or YAML (case (a)'s sweep over the fixtures of `tests/codec/data` finds one): write the format and the document under "notes", since F-3b..g all depend on it and the fix may be a reader's; `cf_reason.def` and `cf_value.h:23-46` do not allow the comparison as written.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": the internal document edit engine, a change spliced at the reader's place and checked by reading it back (wave 6; F-3a, D-129, D-130). `docs/Decisions.md`: D-129 and D-130. The Roadmap item is marked by F-3j.

---

## F-3b - Wave 6, JSON and JSON5: a value changed, a member added or removed

* **id**: F-3b
* **goal**: `cfi_doc_editor("json")` and `("json5")` change, add and remove members of a JSON, JSON5 or JSONC document keeping every other byte, and the sweep over the fixtures finds the check never refusing.
* **why**: JSON is what `settings.c` writes and what most programs' files are; it is the first editor so that the engine meets a real format.
* **needs**: F-3a
* **size**: medium
* **decided**:
  * One editor, `lib/serialize/src/json_edit.c` (new), registered for `json` and `json5`, with `js_` helpers for the token it must skip: a string (escapes, JSON5's single quotes and line continuations), a number (JSON5's hex, `+`, `.5`, `Infinity`, `NaN`), `true`, `false`, `null`, an object or array (balanced, strings and comments skipped), a JSON5 comment (`//`, `/* */`). It does not use the reader's `js_scan` (`json.c:41-51`, static): the checker is what guards it.
  * **Set existing**: from `at` (the first byte of the value) to the end of that one token, replaced by `cfi_json_put_value` (factored out of `js_enc_scalar`, `json.c:859`, which writes through `cf_sink_json_value`, `cf_serialize.h:98`).
  * **Add** `a.b.c` = v: find the object holding `a.b` by walking the recorder's paths (`pos` entries with the prefix `a.b.`: the object's `}` is the first `}` after the last of their tokens, outside strings and comments); insert before it. A member after others: `,` is added after the last member's token (before a trailing comment of its line), then a line break, the indent of the last member's line, `"c": v`. An empty object `{}`: replaced by `{`, a line break, the object line's indent plus two spaces, `"c": v`, a line break, the line's indent, `}`; the same if the object is on one line with members (`{"x":1}` becomes `{"x":1,"c":v}`: no breaks added when the object has none). A missing `a.b`: the object is created as the new member, nested as far as missing (`"b": {"c": v}`), always on one line. Keys are written as JSON strings through `cf_sink_json_string` (`cf_serialize.h:97`).
  * **Unset**: from the member's first byte (its key's opening quote, found by scanning back from `at` over blanks, the `:` and the key token) to the end of its value token, with the following `,` and the blanks after it when there is one, else the preceding `,` and the blanks before it; when the member then leaves its line empty, that line's line break goes too. A JSON5 comment after the member on its line goes with it; any other comment stays. The last member of an object removed leaves `{}` (JSON5: its inner whitespace as it was).
  * **Refused**: a change inside an array beyond setting a member (F-3a's words).
* **files**: `lib/serialize/src/json_edit.c` (new), `lib/serialize/src/json.c` (`cfi_json_put_value` factored out; no behaviour change), `lib/serialize/include/configforge/cf_docedit_engine.h`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/docedit.c` (the table of editors), `lib/config/tests/test_docedit.c` (the sweep, a new function `test_json`), `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3b main` (after F-3a).
  2. In `test_docedit.c` add `test_json` (`#if CF_WITH_JSON`): ten documents (compact; indented with 2 and with 4 spaces; tabs; CRLF; a member per line and several per line; a nested array of scalars; JSON5 with comments before, between and after members and a trailing comma; a string with escapes and a surrogate pair; numbers of every kind). **The sweep**: for every scalar of every document, `set` it to a text, an integer and `null` and see that the new document reads as the old with that value changed and that **every byte before the value's token and after it is as it was** (compared in the test, not by the checker); for every scalar, `unset` it and see the same of the rest; for a handful of absent keys (`x`, `a.x`, `a.b.x` into an existing and into a missing parent) `set` and see the member at the end of its object. Named cases for each rule above (the empty object, the one-line object, the comment after a removed member, the last member).
  3. See it fail: `cmake --build --preset default --target test_docedit && ctest --preset default -R test_docedit 2>&1 | tail -n 20` (no editor: `cannot be edited in place`).
  4. Write `json_edit.c`; register it; factor `cfi_json_put_value`. `ctest --preset default -R "test_docedit|test_json5|test_roundtrip"`.
  5. See that the sweep can fail: make `unset` leave a comma behind (behind a run-time-false condition); the checker must refuse and the sweep report it; put it back.
  6. CHANGELOG; `python scripts/work/work.py format`; codemap and apiref, last. Commit `F-3b: JSON and JSON5 edited in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_docedit` exit 0; `ctest --preset default -R "test_json5|test_roundtrip|conformance"` unchanged (the factoring of `js_enc_scalar`).
* **stop if**: the sweep finds a document where the checker refuses an edit the rules above call right and the cause is the reader's place (`at`) rather than the editor's lexing: the F-3a stop-if; a JSON5 form the editor cannot lex (list it under notes and refuse it by name instead).
* **docs**: `CHANGELOG.md`, "### Added": JSON and JSON5 documents are changed, added to and removed from in place, keeping their layout and comments (F-3b). No new D-number (D-130 holds the rules).

---

## F-3c - Wave 6, INI and .properties: a value changed, a line added or removed

* **id**: F-3c
* **goal**: the editors of `ini` and `properties` change, add and remove entries keeping every other line, comment and blank, and the sweep never meets the check refusing.
* **why**: both are line formats whose files people keep by hand; `.properties` has no writer at all today (`docs/Formats.md`, "Java `.properties`": "no") and this is the first.
* **needs**: F-3a
* **size**: medium
* **decided**:
  * Two editors in `lib/serialize/src/ini_edit.c` and `properties_edit.c` (new). Both are untyped: every value is text; `cf_value` of any type is written as its text (`cf_value_text`'s forms), and a null as the text `null` (INI's encoder already does, `CF_HOLDS_NULL` absent, `cf_serialize.h:304`).
  * **INI set existing**: the value token is from the first byte after the `=` and its blanks (the reader's own, `ini.c:375-420` and its comment rules at 335: read them first) to the end of the value as the reader takes it, trailing blanks excluded; replaced by `cfi_ini_put_value` (factored from `ini_enc_scalar`, `ini.c:640`). A value with a line break is refused as the encoder refuses it (`docs/Formats.md`, INI row). **Add** `a.b` = v: INI's naming (the encoder's: `ini.c:56-132`, a path's last segment is the key, the rest the section, `[a]`): after the last key line of section `a` (before the next `[`, so a blank or comment line that belongs to the next section stays after it); a missing section is appended at the end of the file as a blank line, `[a]`, `b = v`; a key with no section goes before the first `[` of the file. **Unset**: the line, whole. An array member `name[] = x` is set through its place and not added or removed.
  * **properties set existing**: from the first byte after the separator (`=`, `:` or blanks) and the blanks after it, to the end of the logical line (continuation lines, a `\` at the end of a line, included); written with Java's rules: `\`, LF, CR, TAB and FF as `\\`, `\n`, `\r`, `\t`, `\f`; a leading blank as `\ `; every other character as its UTF-8 bytes (the reader reads UTF-8, `docs/Formats.md`). **Add**: `key = value` and a line break at the end of the file (a line break first when the last line has none); the key's `\`, blanks, `=`, `:`, and a leading `#` or `!` escaped with `\`. **Unset**: the logical line, including its continuation lines.
  * Neither format's `caps` change (`properties` stays "read" in `cf_codec_kind`; the editor table is separate from the codec's flags): `configforge codec formats` and the generated table in `docs/Formats.md:42-56` are untouched; the row "Write" of the `.properties` line of the hand-written table is `edited in place (wave 6)`.
* **files**: `lib/serialize/src/ini_edit.c`, `lib/serialize/src/properties_edit.c` (new), `lib/serialize/src/ini.c` (factoring), `lib/serialize/include/configforge/cf_docedit_engine.h`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/docedit.c`, `lib/config/tests/test_docedit.c` (`test_ini`, `test_properties`), `cmake/configforge.internal.symbols`; last: `docs/Formats.md` (the `.properties` and INI rows), `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3c main` (after F-3a).
  2. Add `test_ini` and `test_properties` with the sweep of F-3b (set, unset, every byte outside the token as it was) over eight documents each: comments with `#` and `;`, blank lines, CRLF, sections and root keys, `name[]` arrays, a value with trailing blanks; properties: `=`, `:` and blank separators, a continuation, escapes `é` and a UTF-8 character, a key with an escaped space. Named cases for add into an existing and a missing section and for the end-of-file line break.
  3. See it fail (no editor).
  4. Write the editors; factor `cfi_ini_put_value`. `ctest --preset default -R "test_docedit|test_properties|test_roundtrip"`.
  5. See that the sweep can fail: make the INI add insert after the next section's header; the checker refuses; put it back.
  6. Documents; format; codemap and apiref, last. Commit `F-3c: INI and .properties edited in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_properties|codec.formats"` exit 0.
* **stop if**: the INI reader strips an inline comment from a value (`ini.c:375-420`): then the token ends where the reader's does and the card's decided text is wrong only in that sentence: write it under notes and follow the reader; two readings of `at` for a key with an `[]`.
* **docs**: `CHANGELOG.md`, "### Added": INI and `.properties` files are edited in place (F-3c). `docs/Formats.md`: the "Write" cell of the `.properties` row. No new D-number.

---

## F-3d - Wave 6, TOML: a value changed, a key removed

* **id**: F-3d
* **goal**: the TOML editor sets an existing scalar (in a table, an inline table or an array) and removes a key, keeping the rest of the file, and the sweep never meets the check refusing.
* **why**: TOML is "the recommended hand-written format" (`docs/Design.md:1874`) and the one `docs/Design.md:1966` calls a separate project; changing what is there is the common edit, adding is F-3e.
* **needs**: F-3a
* **size**: medium
* **decided**:
  * `lib/serialize/src/toml_edit.c` (new), registered for `toml`; its token lexer covers basic and literal strings (and their `"""`/`'''` forms, which span lines), integers (`_`, hex, octal, binary), floats, booleans, dates and times, inline tables and arrays (balanced), and a comment. The reader's `toml_scan` (`toml.c:115-142`) is static and is not reused.
  * **Set existing**: `at` to the token's end, replaced by `cfi_toml_put_value` (from `toml_enc_scalar`, `toml.c:1641`): a text is written as a basic string (a literal one when the old token was a literal and the text allows it), an integer and a bool as themselves, a real by the encoder's form. Dates and times are text typed by the contract (D-62): a text that reads as one is written as a string, which reads back as the same text. `null` is refused (`CF_REFUSES_NULL`). A member of an inline table or an array is replaced the same way.
  * **Unset**: a `key = value` line is removed whole; comments are never removed except a comment after the value on the same line; a multi-line value (a `"""` string or an array over lines) goes whole. A key in an inline table is removed with one adjoining `,` (the following one, else the preceding). A key of a dotted form (`a.b = 1`) is removed as a line. A table `[a]` emptied stays as its header. An array element is refused (F-3a's words).
  * **Not here**: adding a key, or anything that needs an array of tables to change (a `set` of a value under `[[x]]` is only a replacement and is here): F-3e.
* **files**: `lib/serialize/src/toml_edit.c` (new), `lib/serialize/src/toml.c` (factoring), `lib/serialize/include/configforge/cf_docedit_engine.h`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/docedit.c`, `lib/config/tests/test_docedit.c` (`test_toml`), `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3d main` (after F-3a).
  2. `test_toml` with the sweep of F-3b over ten documents: tables and dotted keys, inline tables, arrays over several lines, multi-line strings, `[[x]]`, dates, comments everywhere, CRLF; named cases for removing a key from an inline table, a multi-line value and a dotted key.
  3. See it fail (no editor).
  4. Write the editor; factor `cfi_toml_put_value`. `ctest --preset default -R "test_docedit|test_toml"`.
  5. See that the sweep can fail: leave a stray `,` after removing an inline-table key; put it back.
  6. Documents; format; codemap and apiref, last. Commit `F-3d: TOML values changed and keys removed in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_toml|conformance"` exit 0.
* **stop if**: the reader's `at` for a value inside an inline table or a multi-line array is not its first byte (the F-3a stop-if); a TOML 1.1 form the lexer cannot span.
* **docs**: `CHANGELOG.md`, "### Added": TOML values are changed and keys removed in place (F-3d). No new D-number.

---

## F-3e - Wave 6, TOML: a key added

* **id**: F-3e
* **goal**: the TOML editor adds a key: into an existing table after its last entry, at the top of the file for a root key, as a new `[table]` at the end of the file for a missing parent, and as a dotted key where the parent was written dotted; and refuses, by name, where adding would change another key.
* **why**: the rest of "add" for the format whose layout rules (a root key must precede every header; a table defined once) make adding the delicate operation.
* **needs**: F-3d
* **size**: medium
* **decided**:
  * **Where.** Root key: after the last root entry line (a key line before the first header), else at the top of the file after the leading comment lines. Into `[a.b]`: after the last line of that table's last entry (a multi-line value included), before the next header and before the blank and comment lines that lead to it. A parent defined only by dotted keys (`a.x = 1` at the root, no `[a]`): the new key is `a.c = v` after the last dotted key of `a`. A missing parent: at the end of the file, a blank line if the file does not end with one, `[a.b]`, `c = v`; for a key whose first segments exist as tables and the last parent does not, the header is the whole path.
  * **Written** as `name = value` (key bare when it is `A-Za-z0-9_-` only, else a basic string), with the table's own habit of spacing around `=` (taken from the line before it; ` = ` when none) and the file's line ending (the first one found).
  * **Refused by name** (`CF_E_CAPABILITY`): the parent is an inline table (`'<key>' is in an inline table; set the table whole`), the parent is an element of `[[x]]` (`'<key>' is in an array of tables; an array's length is changed by writing the array`), or the new key would be both a value and a table (`CF_E_KEY`).
* **files**: `lib/serialize/src/toml_edit.c`, `lib/config/tests/test_docedit.c` (`test_toml_add`); last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3e main` (after F-3d).
  2. `test_toml_add`: for each of the ten documents of F-3d and for keys `zz`, `a.zz`, `a.b.zz` (existing parents), `n.zz` and `n.m.zz` (missing parents), `set` a text and see the reread equal to the old plus the key, the bytes before the insertion and after it unchanged (compared in the test), and the key's own line where the rules above put it; the three refusals by name; a root key in a document that begins with a header.
  3. See it fail (the editor refuses to add).
  4. Add; `ctest --preset default -R "test_docedit|test_toml"`.
  5. See that it can fail: put a root key after the first header (run-time-false); the checker refuses: a key after a header reads as a member of that table; put it back.
  6. Documents; format; codemap and apiref, last. Commit `F-3e: TOML keys added in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_toml|conformance"` exit 0.
* **stop if**: a document where the rules cannot place a key without changing another (list it under notes; the checker's refusal is the right behaviour and the case becomes a named refusal).
* **docs**: `CHANGELOG.md`, "### Added": TOML keys are added in place (F-3e). No new D-number.

---

## F-3f - Wave 6, YAML: a value changed, a key removed

* **id**: F-3f
* **goal**: the YAML editor sets an existing scalar of a block or flow collection and removes a block mapping entry, keeping anchors, tags, comments and every other line, and the sweep never meets the check refusing.
* **why**: YAML is the format in which "a rewrite" loses most (anchors, comments, styles); `docs/Formats.md` says the writer is "yes, block style" only for a whole document.
* **needs**: F-3a
* **size**: medium
* **decided**:
  * `lib/serialize/src/yaml_edit.c` (new), registered for `yaml`; one reading of the document, by the built-in parser's events (`cf_yaml_parse`, `cf_yamlparse.h:57-60`, whose `cf_yp_node` has `style` (`':'` plain, a quote, `'|'` or `'>'` block, `'['` or `'{'` flow), `anchor`, `line`, `col`): the editor takes the style of the node at the position and finds the token's end by it. It works in the default build only: `#if !CF_YAML_LIBYAML && !CF_YAML_LIBFYAML` (the guard `test_text_escape.c:811` uses); in a libyaml or libfyaml build `cfi_doc_editor("yaml")` is NULL and the engine says `cannot be edited in place: it has no editor`.
  * **Set existing**: plain, single- and double-quoted scalars on one line: the token replaced by `cfi_yaml_put_value` (from `yaml_enc_scalar`, `yaml.c:1253`: the style the encoder picks so that the text reads back as itself, plain when it can); a multi-line plain or quoted scalar: from its first byte to its last line; a block scalar (`|`, `>`): from the indicator to the end of its indented lines, replaced by a quoted scalar on the key's line (a text with line breaks as a double-quoted string with `\n`). A scalar with an anchor (`&a`) keeps the anchor; its tag stays. An **alias** (`*a`) is refused: `'<key>' is an alias of an anchor; set the anchor's own key` (`CF_E_CAPABILITY`). A key that has an anchor which an alias elsewhere uses is set: the aliases read the new value, and the check, which compares the whole list, then refuses it as changing other keys: the card lets the check say so, with F-3a's words. An element of a flow collection is replaced as a scalar.
  * **Unset**: a block mapping entry is the key's line and every line more indented than it (its children); a comment line after it that is less indented stays. An entry in a flow mapping or a sequence element is refused (`'<key>' is in a flow collection; set the collection whole`, `CF_E_CAPABILITY`). A block mapping emptied is written `key: {}` on the parent's line; if the reread differs from what was expected for that (the reader gives an empty table no key, or one), the card writes the removal of the emptied mapping's own entry instead, and puts what the reader does under notes.
* **files**: `lib/serialize/src/yaml_edit.c` (new), `lib/serialize/src/yaml.c` (factoring), `lib/serialize/include/configforge/cf_docedit_engine.h`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/docedit.c`, `lib/config/tests/test_docedit.c` (`test_yaml`), `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3f main` (after F-3a).
  2. `test_yaml` with the sweep of F-3b over ten documents: nested mappings, sequences of scalars, flow collections, plain/quoted/block scalars, an anchor and its alias, a tag, comments at every level, CRLF, `---`. Named cases for the alias refusal, the anchor with an alias elsewhere, the emptied mapping and a block scalar replaced.
  3. See it fail (no editor).
  4. Write the editor; factor `cfi_yaml_put_value`. `ctest --preset default -R "test_docedit|test_yaml|conformance"`.
  5. See that the sweep can fail: stop the removal one line early (run-time-false); the checker refuses; put it back.
  6. Documents; format; codemap and apiref, last. Commit `F-3f: YAML values changed and keys removed in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_yaml|conformance"` exit 0 in the default build, and the same target builds with `-DCF_YAML_LIBYAML=ON` where libyaml is installed (the test then expects the refusal).
* **stop if**: `cf_yp_node.line/col` is not the scalar's first byte for a quoted or block scalar (the F-3a stop-if); a case the style list above misses.
* **docs**: `CHANGELOG.md`, "### Added": YAML values are changed and keys removed in place (F-3f). No new D-number.

---

## F-3g - Wave 6, YAML: a key added

* **id**: F-3g
* **goal**: the YAML editor adds a key to a block mapping, creating missing parents as block mappings, in the indentation of the mapping it joins, and refuses by name where adding would change another key.
* **why**: the rest of "add" for YAML, where indentation is the structure.
* **needs**: F-3f
* **size**: medium
* **decided**:
  * **Where.** After the last line of the parent mapping's last entry (its children included), at the indentation of the mapping's first key; a top-level key after the last top-level entry and before a `...` end marker. A missing parent: the new mapping is written as nested block mappings, each level two spaces deeper than the one before, from the last existing parent. The line ending is the file's first.
  * **Written** `name: value` with the value as `cfi_yaml_put_value` writes it; a key that needs quoting (`:` , `#`, a leading special character, or reads as a non-text scalar) is written as a double-quoted string.
  * **Refused** (`CF_E_CAPABILITY`): the parent is a flow mapping (`'<key>' is in a flow collection; set the collection whole`), an alias, or an element of a sequence of mappings (`'<key>' is in an array; an array's length is changed by writing the array`); a document of several documents is already refused by the reader (`docs/Formats.md`, YAML row).
* **files**: `lib/serialize/src/yaml_edit.c`, `lib/config/tests/test_docedit.c` (`test_yaml_add`); last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3g main` (after F-3f).
  2. `test_yaml_add`: the ten documents of F-3f and keys `zz`, `a.zz`, `a.b.zz`, `n.zz`, `n.m.zz`: the reread is the old plus the key, bytes before and after the insertion unchanged, the key at its mapping's indentation; the three refusals; a document ending without a line break and one with `...`.
  3. See it fail (the editor refuses to add).
  4. Add; `ctest --preset default -R "test_docedit|test_yaml"`.
  5. See that it can fail: indent the new line one space too few (run-time-false); the checker refuses; put it back.
  6. Documents; format; codemap and apiref, last. Commit `F-3g: YAML keys added in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_yaml|conformance"` exit 0.
* **stop if**: a document where the end of a mapping cannot be found without a second lexer (a block scalar's last line, an anchor on the mapping): write it under notes and refuse it by name.
* **docs**: `CHANGELOG.md`, "### Added": YAML keys are added in place (F-3g). No new D-number.

---

## F-3h - Wave 6, TLV: a value changed, an entry added or removed, the bytes a layout skips kept

* **id**: F-3h
* **goal**: the TLV editor, by byte offsets, sets, adds and removes an entry of a TLV document read through a layout, fixing the length of the entry's every enclosing nest, and every byte of an entry the layout skips or keeps unknown is as it was.
* **why**: `docs/Formats.md` says "no (wave 6)" for TLV writing and D-93 says "what a layout skips must be kept to write the file back" (`docs/Decisions.md:4082`). A rewrite through an encoder would lose the skipped; a splice cannot.
* **needs**: F-3a
* **size**: large (a length that changes in every enclosing nest, three value kinds, a layout)
* **decided**:
  * **Editor from the layout.** TLV editing needs the layout, which a codec made by `cf_tlv_codec_sized` carries in `cf_codec.data` (`cf_serialize.h:243`, read by `decode_self`). `cf_doc_editor.apply` therefore takes the codec: add `const cf_codec* codec` to `cf_doc_text` (F-3a's struct; set by the engine; NULL changes nothing for the other editors). `cfi_doc_editor` returns the TLV editor for a codec that has `CF_CODEC_SCHEMA` and a TLV layout (`cf_tlv_layout`, `cf_serialize.h:401`), keyed by the codec's pointer rather than by name: `cfi_doc_editor_for(const cf_codec* c)` (new, replaces the call by name in the engine; the name call stays for the other formats).
  * **Places are bytes** (`cf_loc` is `byte N`, `docs/Tools.md:276`): the engine parses it. The editor re-reads the entry at that offset with the layout's widths, byte order and BER rules (`lib/serialize/src/tlv.c`) and finds the entry's tag, length field and value bytes, and the chain of enclosing nests from the document's start.
  * **Set**: the value bytes are replaced by the layout's encoding of the new value (an integer in the tag's width and order, a BCD number of at most 18 digits, hex text as bytes, text as UTF-8; a type that does not fit the tag's kind is `CF_E_VALIDATE`: `'<key>' is <kind> in this layout`); the entry's length field is rewritten in the width it has (a BER length may grow from 1 to `81`.. bytes; a fixed width that cannot hold the new length is refused: `value too long for the length field of '<key>'`), and each enclosing nest's length field changes by the same difference, in its own width, with the same refusal.
  * **Remove**: the whole entry (tag, length, value) is deleted and the enclosing lengths shrink. An entry of a `CF_TLV_LIST` is refused (a list's length is its occurrences, F-3a's array words). **Add**: a new entry at the end of the enclosing nest's value (before the document's end tag when the layout has one), with the tag found from the layout by the key; a key the layout does not name is refused (`'<key>' is not in the layout`); the enclosing lengths grow. With `CF_TLV_PADDING` the padding bytes are not touched.
  * **Kept**: unknown tags and skipped ones are never decoded or moved; the sweep's oracle is bytes.
  * **D-131** (after D-130, "A TLV document is edited by its bytes"): Question: Formats.md says TLV writing waits for wave 6 and D-93 says what a layout skips must be kept. Options: an encoder from the layout (rejected: it cannot know the skipped bytes); a splice by byte offset with the enclosing lengths fixed (decided). Decision: the bullets above.
* **files**: `lib/serialize/src/tlv_edit.c` (new), `lib/serialize/include/configforge/cf_docedit_engine.h`, `lib/serialize/src/docedit.c`, `lib/serialize/CMakeLists.txt`, `lib/config/tests/test_docedit.c` (`test_tlv`), `cmake/configforge.internal.symbols`; last: `docs/Formats.md` (TLV row), `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3h main` (after F-3a).
  2. In `test_docedit.c` add `test_tlv` (`#if CF_WITH_TLV`) over the three layouts of `test_tlv.c` (a device's fixed one; a network one with nests, a list, BCD and hex; an EMV record in BER-TLV with unknown tags kept and skipped): the documents `test_tlv.c` builds. Sweep: set every scalar to a value of its kind; remove every non-list entry; add each key of the layout that the document lacks; each time the reread is the old plus the change **and the bytes of every unknown or skipped entry are identical**, and every length of a nest is the number of bytes it holds (checked by the reader accepting it). Named refusals: a list entry, a value too long for a fixed length field, a key not in the layout, a wrong kind.
  3. See it fail (no editor for a TLV codec).
  4. Write the editor and `cfi_doc_editor_for`. `ctest --preset default -R "test_docedit|test_tlv"`.
  5. See that the sweep can fail: skip the fix of the outermost nest's length (run-time-false); the reader refuses the document, the checker reports it; put it back.
  6. D-131 into `docs/Decisions.md`; `docs/Formats.md`: the TLV row's "Write" cell becomes `edited in place (F-3h)`; CHANGELOG; format; codemap and apiref, last. Commit `F-3h: TLV edited by its bytes (D-131)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit|test_tlv|test_fuzz"` exit 0, and `fuzz.tlv`/`fuzz.ber` for a minute each (`cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=60 ... verify.cmake`, as P-3's step 4e) still pass: the readers are unchanged.
* **stop if**: `tlv.c`'s decoder cannot give the entry boundaries (its entry loop is not callable apart from the sink): then the editor needs a reader change; stop and write which function; a BER length of the indefinite form (`80`) is already refused by the reader and stays refused.
* **docs**: `CHANGELOG.md`, "### Added": TLV documents are edited in place by their bytes, keeping what a layout skips (F-3h, D-131). `docs/Decisions.md`: D-131.

---

## F-3i - Wave 6, `configforge codec get`, `set` and `unset`

* **id**: F-3i
* **goal**: `configforge codec get <file> <key>`, `set <file> <key> <value>` and `unset <file> <key>` read, change and remove a key of a document in any format that has an editor (and CBOR), writing the file in place through F-1a's function.
* **why**: the whole wave as a tool, and the first thing of wave 6 a person can run; "read" is `get`, the rest are the engine.
* **needs**: F-1a, F-3b, F-3c, F-3d, F-3e, F-3f, F-3g (F-3h by its row: a TLV file needs a layout and is not reachable from the tool, as for `dump`)
* **size**: medium
* **decided**:
  * Usage, added to `tools/codec/codec.c:82-90`'s dispatch and its usage text, and to `docs/Tools.md:218-236`:
    ```text
    configforge codec get <file> <key> [--as <format>]
    configforge codec set <file> <key> <value> [--type text|int|real|bool|null] [--as <format>] [--out <file>]
    configforge codec unset <file> <key> [--as <format>] [--out <file>]
    ```
    `--type`'s default is the type the value reads as (`cf_infer_scalar`, `lib/numeric`'s, `docs/Codemap.md:1014-1036`: an integer, a real, `true`/`false`, `null`, else text) for a typed format, and text for an untyped one; `--type` forces it. `--out` default: the file itself, replaced whole through `cf_platform_write_file`; `--out -` is standard output (binary on Windows, as `convert`'s). `-` as `<file>` reads standard input and needs `--out`.
  * **get** prints the value's text and a line break and exits 0; `key not found: <key>` on stderr and exit 1 when there is none; a key naming a table or array prints nothing and exits 1 with `<key> is a table`. It reads through the recorder of F-3a (`configforge_serialize` only).
  * **Exit codes**, as `convert`'s (`docs/Tools.md:339-341`): 0 done; 1 refused (a change the rules refuse, a key not found for `get`, a format that cannot be told); 2 unusable input (the file cannot be read, a bad option); 3 the check refused what an editor wrote (a bug; nothing is written).
  * A format with no editor: `codec set: '<format>' cannot be edited in place`, exit 1. A TLV codec is not reachable (no layout), as for the other commands.
  * New file `tools/codec/edit.c` (after `convert.c`'s shape, `tools/codec/convert.c:1-294`); the program's source list in `tools/codec/CMakeLists.txt`.
* **files**: `tools/codec/edit.c` (new), `tools/codec/codec.c`, `tools/codec/codec.h`, `tools/codec/CMakeLists.txt`, `tests/codec/edit.cmake` (new), `tests/codec/CMakeLists.txt` (the `cf_codec_scripts` list, line 28-33, `edit` after `textconv`, under `CF_WITH_EMIT` like `convert`), `tests/codec/usage.cmake`, `docs/Tools.md`; last: `docs/Formats.md` (a sentence under the table), `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. `git switch -c work/F-3i main` (after the cards it needs).
  2. `tests/codec/edit.cmake`, in the style of `tests/codec/convert.cmake`: for JSON, TOML, YAML, INI and CBOR (each `skipped:` when the format is out of the build): write a document with a comment; `get` a value and compare; `set` an existing key and see only that line changed (`file(READ)` and a string compare of the rest); `set` a new key; `unset`; `set` a key into an array (exit 1, the words); `--out -` leaves the file as it was; a document with a syntax error exits 1 with the reader's message and the file is unchanged; `--type int` on `"x"` writes `x = 7`; `get` of an absent key exits 1; no arguments exits 2 with the usage.
  3. See it fail: `ctest --preset default -R codec.edit 2>&1 | tail -n 15` (the usage text answers).
  4. Write `edit.c` and the dispatch. `ctest --preset default -R "codec"`.
  5. Exit 3 cannot be reached from the tool with a real editor; `test_docedit`'s case (e) (a wrong editor refused by the checker) is its proof: run it, `ctest --preset default -R test_docedit`, and say so in the report.
  6. `docs/Tools.md`: the usage lines, a section "Editing a document" after "Converting between formats" (what is kept, the refusals by their words, the exit codes). Format; codemap and apiref, last. Commit `F-3i: configforge codec get, set and unset`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "codec"` exit 0 and `codec.usage` includes the three new commands.
* **stop if**: `configforge_serialize` alone cannot give `cf_infer_scalar` (it is in `lib/numeric`: if the tool does not link it, take the default type as text and say so); an editor is missing for a format the card lists.
* **docs**: `CHANGELOG.md`, "### Added": `configforge codec get`, `set` and `unset` (F-3i). `docs/Tools.md`: the commands.

---

## F-3j - Wave 6, the document edit as a public interface (TO DECIDE)

* **id**: F-3j
* **goal**: a program edits a file in its own format with a public call that uses the engine and F-1a's write, in C, and the three Roadmap items it finishes are marked.
* **why**: the tool is a program; the Roadmap says "every operation on a file's data" and a program is the other caller. `cf_edit` and `cf_write` do it for a configuration and its stores today (`docs/Roadmap.md:437-440`).
* **needs**: F-3i, F-1b
* **size**: medium
* **decided**:
  * **TO DECIDE (the shape of the public interface).** Options: **A** a family beside `cf_edit` in a new public `cf_docedit.h`: `typedef struct cf_docedit cf_docedit; cf_docedit* cf_docedit_open(const char* path, const char* format, cf_error* err);` (`format` NULL: by name), `cf_docedit_set_text`, `_set_int`, `_set_bool`, `_set_real`, `cf_docedit_unset`, `cf_docedit_count`, `cf_bool cf_docedit_commit(cf_docedit*, cf_error*)` (every change applied to the file's bytes, checked, and the file replaced whole or not at all), `cf_docedit_abort`; the shape of `cf_edit` (`cf_edit.h:36-60`), with the same rules (strings copied, one thread, consumed by commit). **B** a bytes-in, bytes-out function, `cf_docedit_bytes(const char* format, const char* in, size_t n, const cf_doc_change* ch, size_t count, char** out, size_t* outn, cf_error* err)`, no file, no `commit`. **C** `cf_edit_begin` accepts a document as its target (`cf_edit_begin(c, "file:app.toml", ...)`), so one interface edits stores and files. **Recommended: A.** It is the shape a program that knows `cf_edit` expects, it has one place that writes (the commit), and B can be added behind it later; C mixes a configuration's contract (types, ranges, policy, D-42) with a file that has none, which is the "second rule set" D-42 rejected. The steps are A's.
  * **A's behaviour.** `cf_docedit_open` reads the file whole (`cf_blob_file`), finds the codec (`cf_codec_by_path` or by `format`) and fails at once for a format with no editor (F-3a's words). `commit` calls `cfi_docedit` with the changes in the order they were made and `cf_platform_write_file` with the result; a refusal changes nothing, and the file's bytes are as they were. Types: `set_text` is typed as the codec types (`cf_infer_scalar` for an untyped format is not applied: text is text); `set_int`, `set_bool`, `set_real` are typed. No contract and no policy: this is a file, not a configuration (the page says so beside D-42).
  * **Marks**: in `docs/Roadmap.md` the item headed "A file written whole or not at all" (line 714; the written-whole function and every example that saves use it: F-1a, F-1b, F-2b, F-3j), the table row "Every operation on a file's data, written back in its format" (line 55) and the list item "**Wave 6**" (line 144) become 🟩, each with the card ids.
* **files**: `lib/config/include/configforge/cf_docedit.h` (new, public; the engine's own header is `cf_docedit_engine.h`, F-3a), `lib/config/src/core/docedit.c` (new), `lib/config/CMakeLists.txt`, `lib/config/include/configforge/configforge.h` (include), `lib/config/tests/test_docedit_api.c` (new), `lib/config/tests/CMakeLists.txt`, `cmake/configforge.internal.symbols`; last: `docs/Guide.md`, `docs/Roadmap.md`, `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE. `git switch -c work/F-3j main`.
  2. Nothing to rename: the engine's header is `cf_docedit_engine.h` already.
  3. Write `test_docedit_api.c` after `test_edit.c`: open a JSON file with a comment, `set_int`, `set_text`, `unset`, `commit`; the comment and the other members are intact; a second `commit` is refused (consumed); `abort` writes nothing; a file that does not exist, an unreadable one (`{ bad`) and a format without an editor fail at `open`; a change the check refuses leaves the bytes as they were.
  4. See it fail (no `cf_docedit_open`).
  5. Write the header, `docedit.c`, the CMake and symbols lines. `ctest --preset default -R "test_docedit_api|test_edit|gate"`.
  6. `docs/Guide.md`: a section "Editing a user's file" after the one on the override file; the Roadmap marks above; CHANGELOG; format; codemap and apiref, last. Commit `F-3j: cf_docedit, a user's file changed in place`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_docedit_api"` exit 0 in `default` and `minimal`.
* **stop if**: no answer to the TO DECIDE; `gate.abi`, `gate.headers` or `gate.interface` refuses the new public header for a reason other than a missing line of `cmake/configforge.internal.symbols` or `docs/API.md`; a gate wants the bindings.
* **docs**: `CHANGELOG.md`, "### Added": `cf_docedit`: a program edits a file in its own format, keeping its layout (F-3j; wave 6). `docs/Roadmap.md`: the three marks above. No new D-number.

---

## F-4a - The references drawn, first by jq

* **id**: F-4a
* **goal**: `docs/Doctor.md` section 7a holds two jq filters that draw a report's references as Graphviz DOT and as Mermaid, tested against a fixture report where jq is installed, and the card says which of the Roadmap's three wants they meet and which they do not.
* **why**: the Roadmap item ("The references drawn", `docs/Roadmap.md:760-766`) applies "only if a report's JSON through jq proves not to be enough (as with the query language below)". Before a command is written, the question is answered with evidence, as `docs/Doctor.md:262-286` answers it for the other queries. Wave 5 is done, so the condition's first half is met.
* **needs**: nothing
* **size**: small
* **decided**:
  * **The edges a report holds.** A reference: a candidate's `written` field holds the text as written, `${a.b}` among it (`lib/config/src/diag/report.c:303-308`; absent for a secret in a blind report, and for a template, `CF_CAND_TEMPLATE`). A rename: a definition's `replaced_by` (`report.c:111-115`), the new key's name. A source: a key's `selected.source` and its `candidates[].source`. Nothing else is an edge.
  * **Filters** (written into `docs/Doctor.md` 7a after the third example, line 280, each run by the card): (1) references, DOT: every key `k` with a candidate whose `written` contains `${x}` gives `"k" -> "x" [label="ref"]`; (2) renames: `"old" -> "new" [style=dashed,label="replaced_by"]`; (3) sources: `"k" -> "source:<s>" [style=dotted]` for the selected one; the same three as Mermaid (`graph LR`, `k --> x`). One `jq -r` program each, the references' `${...}` found with `scan("\\$\\{([^}]+)\\}")` (a reference's own grammar is D-10's: keys, no nesting).
  * **A fixture**: `tests/doctor/fixtures.c` (the report writer's fixtures) gains one report `graph.json` with a chain of three references, a rename, a key from each of two sources and a secret that is referenced (its `written` absent, so no edge: stated in the page). The test is `tests/doctor/graph_jq.cmake`: `find_program(JQ jq)`; absent: print `skipped: jq is not installed` and exit 0 (no jq in CI's images today: `grep -rn jq .github/workflows` is empty); present: run the six filters and compare to the stored expected lines (`tests/doctor/graph.expected`, sorted).
  * **The finding** (the card's output, written under its "notes" and in the report to the maintainer): the three wants of the Roadmap - "a cycle", "a long chain", "each key's sources" - are met or not by the filters: a **cycle** cannot be drawn from a report because the library refuses it before a report exists (D-10: `CF_REFUSAL_REFERENCE` is the report's `reason_kind`); a **long chain** is drawn (DOT lays it out); **sources** are drawn. If the filters meet all three, F-4b's condition is not met and F-4b closes with no code. If one is not met, the card says which and why.
* **files**: `docs/Doctor.md`, `tests/doctor/fixtures.c`, `tests/doctor/graph_jq.cmake` (new), `tests/doctor/graph.expected` (new), `tests/doctor/CMakeLists.txt` (the script list, after the `severity` entry, line 171); last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/F-4a main`; `ctest --preset default -R doctor 2>&1 | tail -n 15` passes as it is.
  2. Add the fixture report to `fixtures.c` and the expected file by hand (what the filters must print for it).
  3. Write `graph_jq.cmake` and register it. See it fail: with jq installed the filters are not in Doctor.md yet and the script reads them from there: `ctest --preset default -R doctor.graph_jq` fails on a missing filter. Without jq it prints `skipped`: then run the filters by hand on a machine that has jq, once, and say so.
  4. Write the six filters into Doctor.md 7a, each introduced by the question it answers, and the sentence on blind reports. `ctest --preset default -R doctor`.
  5. Write the finding under the card's "notes". CHANGELOG; format; commit `F-4a: the references drawn by jq`.
  6. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0 (`graph_jq` says `skipped` where jq is missing); the filters run by hand once with jq and the output equals `graph.expected`.
* **stop if**: the report holds no `written` for a reference in a normal run (then there is no edge to draw and the finding is "the report must carry it": a library change, to the maintainer).
* **docs**: `CHANGELOG.md`, "### Added": jq filters that draw a report's references, renames and sources as DOT and Mermaid (F-4a). `docs/Doctor.md` 7a. The Roadmap item is marked by F-4b, or closed by F-4a's finding.

---

## F-4b - `configforge doctor graph` (TO DECIDE: whether it is built at all)

* **id**: F-4b
* **goal**: if F-4a found that jq does not meet a want of the Roadmap's, `configforge doctor graph <report> [--format dot|mermaid]` writes the references, renames and sources of a report as a graph, never a value.
* **why**: "The references drawn" (`docs/Roadmap.md:760-766`), `docs/Maintaining.md:95-105` (moved to 1.10.0, "if asked for").
* **needs**: F-4a, and the maintainer's yes
* **size**: medium
* **decided**:
  * **TO DECIDE (whether to build it).** Options: **A** build it, as below. **B** do not: F-4a's filters are the answer, the Roadmap item is closed with them. **Recommended: B unless F-4a's finding names a want the filters do not meet** (the Roadmap's own condition); then A, for that want only. The card stops until the maintainer answers, whatever F-4a found.
  * If built (A): the command `configforge doctor graph <report> [--format dot|mermaid] [--key K] [--redact-all]`, in `tools/doctor/doctor.c` beside `history` (line 502), `dot` the default; the edges of F-4a's list and no other; `--key K` limits the graph to the keys reachable from `K` through references and renames; a node is a key name or `source:<name>`; a key that is sensitive is drawn (its name is not a secret) and has no `written` edge (a blind report has none); no value is ever printed (D-86: the same tests `doctor.redact` and `doctor.marker` run over the output with a marked secret). Exit codes as the other commands (`docs/Doctor.md:555-572`): 0 drawn, 1 a report it cannot read, 2 usage. Output is sorted (edges by source then target) so two runs are equal. Written by the library's own report reader (`cf_doctor_*`, `docs/Doctor.md:288-300`), not by a second JSON reader.
  * **D-132** (after D-131, "Doctor draws the references"): written only if A; Question: whether a report's edges are drawn by Doctor or by jq. Options: jq filters (F-4a); `doctor graph` (this card). Decision: as the maintainer answered; the edges, the formats, the no-value rule.
* **files**: (A) `tools/doctor/doctor.c`, `tools/doctor/engine.c`, `tools/doctor/engine.h`, `tests/doctor/graph.cmake` (new), `tests/doctor/CMakeLists.txt`, `docs/Doctor.md` (section 7, the usage block), `docs/Tools.md` if Doctor's usage is repeated there; last: `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md`.
* **steps** (A):
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE; for B, write under "notes" `closed by F-4a, <date>`, mark the Roadmap item `🟩 (F-4a)`, and stop.
  2. `git switch -c work/F-4b main` (after F-4a).
  3. `tests/doctor/graph.cmake` over F-4a's `graph.json`: DOT and Mermaid equal to the expected files; `--key` limits; a report with a marked secret prints no value (`doctor.marker`'s marker); an unreadable report exits 1; no arguments exits 2.
  4. See it fail (the usage answers).
  5. Write the command in the engine and the dispatch. `ctest --preset default -R doctor`.
  6. See that the no-value rule can fail: print `value` once in the engine behind a run-time-false condition; the marker test fails; put it back.
  7. D-132, Doctor.md, CHANGELOG, the Roadmap mark; format; codemap and apiref, last. Commit `F-4b: configforge doctor graph (D-132)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "doctor"` exit 0.
* **stop if**: no answer to the TO DECIDE; the report's reader in the library does not expose a candidate's `written` (then a library change is needed first: say which).
* **docs**: (A) `CHANGELOG.md`, "### Added": `configforge doctor graph` (F-4b, D-132); `docs/Decisions.md`: D-132; `docs/Roadmap.md`: the item headed "The references drawn" becomes 🟩. (B) the Roadmap mark only.
