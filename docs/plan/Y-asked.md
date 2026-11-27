# Y - what the maintainer asked for, 2026-11-26

These cards come from the maintainer's five ideas of 2026-11-26
(`docs/Roadmap.md`, "From the maintainer, 2026-11-26"), each researched
first in `docs/research/<topic>.md`: reads and writes at the same time,
guarded by choice (`concurrent-access.md`), threads and processes made
easier (`threads-and-processes.md`), and three more whose cards come
later in this file. Each card is read cold against the tree, by someone
who did not write it, before it opens (docs/Plan.md, "Read again after
the layout changes"). Line numbers are those of the tree at cbff5a9;
a card that finds them moved finds the symbol by name, and a symbol that
is gone is a "stop if". Cards of this group are numbered Y-1 onward; the
decisions of this group are numbered from D-186.

Cards of one topic that touch files F-1a and F-2a (`docs/plan/F-files.md`)
rely on those two cards as they now read, with the amendments of
2026-11-26 written into them (the Windows retry in F-1a, the optimistic
check in F-2a). Neither has landed: a card of this file that names
`cf_platform_write_file` or `overrides_write` names what F-1a and F-2a
decide, and a symbol that is not in the tree when the card opens is that
card's "stop if" only when the card it needs has landed.

Every question a research left open was decided on 2026-11-27 (the
maintainer's standing approval, which leaves nothing undecided or parked):
each card says its choice under "decided", with a "Rejected:" line, and
the date under "Organiser's answers". No card here waits on an answer,
and none is parked.

Decisions of this group: D-186 (a write checks the file is still what it
was read from, Y-2), D-187 (the lock a program asks for, Y-3), D-188
(how long a write waits, and "busy", Y-4), D-189 (a file is read twice
and compared, Y-5), D-190 (every descriptor the library keeps is close-on-exec, Y-8),
D-191 (a second driver of a `cf_live` is refused, Y-9), D-192 (the read path is
measured before it is changed, Y-10), D-193 (a worker's reader handle, Y-11),
D-194 (workers told of a swap, Y-12), D-195 (a shared image, Y-13-m).

Compression (`docs/research/compression.md`), Y-14 to Y-23-m: D-196 and
D-197 (a byte layer under the codecs, its bounds, Y-15), D-198 (gzip, Y-16),
D-199 (zstd, Y-17), D-200 (the sources read through the layer, Y-18), D-201
(writing, deterministic, Y-20), D-202 to D-204 (the built-in inflate,
xz and lz4, zstd dictionaries). The configuration
compiler (`docs/research/config-compiler.md`), Y-24 to Y-33: D-205 to D-210
(Y-24 to Y-29), D-211 (relations across keys, Y-30-m), D-212 (a build file,
Y-32-m), D-213 (the manifest in the image, Y-33). The analysis tool
(`docs/research/analysis-tool.md`), Y-34 to Y-40-m: D-214 and D-215 (a
predicted report and Doctor on it, Y-34 and Y-35), D-216 and D-217
(CFD022, CFD023), D-218 and D-219 (`pattern` in the contract, the language
server's hover through the schema). The analysis tool builds on C-8a and C-8b
(`docs/plan/C-change-2.md`), which it does not repeat.

---

## Approved by the maintainer, 2026-11-26 (relayed from a side session: "I approve")

These two decisions replace Y-15's open questions 1, 4 and 5 (all
decided; every other open question of this file was decided on 2026-11-27). **Y-15 to Y-20,
and Y-21-m to Y-23-m where they touch it, follow D-221** (done: Y-15 is split into
Y-15a and Y-15b as point 7 says, and each of Y-15a to Y-20 carries this
answer under "Organiser's answers"). Both are copied into
`docs/Decisions.md` by the first card that lands under them.

* **D-220 - Parts that compose, options with defaults.** Every later
  design is made of atomic parts, each simple and usable alone, that
  compose into something rich. A limit is an option with a default
  value, never a fixed number ("in a system's design I always prefer an
  option and a default value to a hard limit", the maintainer). A layer
  is made only when a second real user of it exists: no empty layers.
  The platform is one of those parts (the maintainer, 2026-11-26: "Platform
  bir parçadır; OS'suz ortamda olmayan özellik derlenmez ve tabloda
  yazar"): no feature assumes an operating system; a feature the target
  platform cannot provide (files, locks, sync, threads, `cf_live`, clock,
  environment, `malloc`) is either reached through the platform part or
  not compiled at all for that platform (the `none` platform), so its use
  is a link error, never a run-time surprise, or `CF_E_CAPABILITY` where
  the call must exist; the documents carry one table of each feature
  against each platform. The groups L (embedded targets) and every later
  card follow it; `--target c` (Y-27) is the answer offered where there
  is no operating system.
* **D-221 - Compression, its own component.**
  1. `lib/compress` (D-100): bytes in, bytes out; it knows nothing of
     serialization, codecs or configuration, and is usable alone.
  2. Two interfaces, `cf_packer` and `cf_unpacker` (not one struct with
     a NULL half): a method that only decompresses gives a `cf_unpacker`
     only. A method is a value filling the interface (`cf_gzip_packer`,
     `cf_zstd_unpacker`, or the program's own); adding one fills a struct
     and touches no other file. One-call entry points:
     `cf_pack(const cf_packer*, const cf_pack_options*, in, out)` and
     `cf_unpack(const cf_unpacker*, const cf_unpack_options*, in, out)`.
  3. Streaming interfaces (open, read or write, finish, close), so that
     they chain: file, unpacker, codec reader, without the whole document
     in memory; `cf_pack` and `cf_unpack` are the one-call shortcuts.
     `cf_unpack_chain(...)` composes unpackers and is itself a
     `cf_unpacker`.
  4. No fixed limits: `cf_unpack_options` and `cf_pack_options`
     (`max_ratio`, `max_size`, `min_floor`, `level`, `window`, ...).
     NULL options take the defaults; 0 means unlimited, chosen
     explicitly; the bomb guard is on by default. The defaults are the
     method's own: each `cf_packer` and `cf_unpacker` carries its default
     options (a ZPAQ-like method may default to far more memory and window
     than gzip); the caller's options override them field by field.
  5. Configuration binds through one call, e.g.
     `cf_builder_unpacker(b, &cf_zstd_unpacker)`; `lib/compress` knows
     nothing of it. Detection by magic or extension (`cf_unpack_detect`
     over an optional registry) is optional; a program may name the
     method.
  6. The backends (zlib, zstd) are build options, OFF by default; the
     core stays without dependencies.
  6b. (2026-11-26, the maintainer: "compression was only an add-on; it must be
     optional") `lib/compress` itself is an optional component, OFF by
     default: not built or linked unless enabled, and nothing of it enters
     the core otherwise.
  7. Y-15 splits in two: Y-15a, the two interfaces, the options with the
     method's defaults, the one-call shortcuts and a test backend; Y-15b,
     streaming and the chain. Both stay 1.10.0.

## Y-1 - The Guide's "At the same time": what is guarded and what is not

* **id**: Y-1
* **goal**: `docs/Guide.md` has a section "At the same time" under "14. Threads" that tells a program, in the three cases a program meets, what the library guards and what it leaves to the program, and `docs/Decisions.md`'s D-14 sequel no longer says the lock is "a count taken from 0 to 1".
* **why**: the maintainer asked "is there protection if read and write operations happen at the same time?" (`docs/research/concurrent-access.md:17-29`) and the answer is spread over D-14, `cf_live.h` and three plan cards; the one-line answer exists nowhere a user looks. The sequel's line is stale: the code replaced the one-count lock with a ticket lock after the one-count lock livelocked under tcc (`lib/config/src/core/live.c:78-85`, `live_lock` at 86-89). The Roadmap item is "Reads and writes at the same time, guarded by choice" (`docs/Roadmap.md:1555`); this card writes down what is true today and what the later cards of this topic will add.
* **needs**: nothing
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Documentation only, straight to `main`** (CLAUDE.md, "Habits"). No code, no test, no D-number.
  * **Where.** A new `### At the same time` at the end of `docs/Guide.md`'s section 14 (the file's last section; its text ends at line 1680 with "set once."). The contents list at the top of the Guide (line 24) names sections only; it does not change.
  * **The text** (copy as written, wrapped at the Guide's width; a list item is one line here for reading):
    ```markdown
    ### At the same time

    Three questions, three answers.

    **Several threads, one program.** Reading is guarded, by being
    immutable: a built configuration is a snapshot that nothing changes, so
    any number of threads read it with no lock, and a thread that reads
    while another reloads sees the old snapshot or the new one, never a
    mixture. A change is a new snapshot (`cf_derive`), and a reload through
    `cf_live` swaps the pointer under a lock held for a pointer copy and a
    count; readers call `cf_live_get` on any thread and keep what they got.
    Not guarded: building and releasing (the program's to order), and the
    other `cf_live_*` calls, which belong to the one thread that drives the
    reloads. Two threads that both call `cf_live_poll` or `cf_live_reload`
    are outside the contract.

    **Several programs, one file.** Not guarded today, because nothing in
    the library writes a configuration file yet; when the library writes
    one (an override file, "Files written back" in the Roadmap), it is
    replaced whole or not at all, so that a reader never sees half of it.
    What stays the program's: another program that edits the same file;
    a file on a network filesystem, whose locks and renames each
    filesystem keeps in its own way (the library does not detect it); and
    a program that needs several keys to change together, which puts them
    in one file or in the store.

    **A reader and a file being written.** A reader of a file written by
    the library opens a whole file, the old or the new. A reader of a file
    written by anything else (an editor, a deployment tool that writes in
    place) can read it half written; a reload that fails to read or to
    build keeps the running snapshot and tries at the next change, and
    `settle` waits for a burst of writes to end before reading.

    The store (SQLite) keeps its own locks: two writers do not corrupt it,
    and a second one that finds it locked is refused at once.
    ```
    The words "Files written back" are the Roadmap's wave 6 heading's own; the worker checks with `grep -n "written back" docs/Roadmap.md | head -3` that a heading with them exists, and uses its exact text.
  * **The sequel.** `docs/Decisions.md:2194-2196`: replace "under a lock made of the port's one atomic (a count taken from 0 to 1), held for a pointer copy and an increment" with "under a ticket lock made of the port's atomics (the first version, a count taken from 0 to 1, livelocked under tcc; `lib/config/src/core/live.c`), held for a pointer copy and an increment". The Decision's other words stay.
* **files**: `docs/Guide.md`, `docs/Decisions.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch main && git pull --ff-only`.
  2. Find the exact lines: `grep -n "^## 14. Threads" docs/Guide.md` and `grep -n "a count" docs/Decisions.md`. Both must be found once; otherwise stop.
  3. Append the section to `docs/Guide.md` after the last line of section 14 (`tail -n 5 docs/Guide.md` shows it is the file's end; when the file has grown, the section goes after "set once." and before the next `## `).
  4. Edit the sequel in `docs/Decisions.md` with the Edit tool.
  5. `CHANGELOG.md`: the entry below.
  6. See the gates pass on the document change: `cmake --build --preset default --target all 2>&1 | tail -n 5; ctest --preset default -R "gate.links|gate.codespell|gate.text" 2>&1 | tail -n 15`. A failure names a word or a link to fix; this is "see it" for a document: the gates are the test.
  7. Commit `Guide: "At the same time", what is guarded and what is not; D-14's sequel names the ticket lock`; push `main`.
* **accept**: `ctest --preset default -R "gate.links|gate.codespell|gate.text"` exit 0 on Linux and on Windows; `grep -c "a count taken from 0 to 1" docs/Decisions.md` prints 0. (No ACCEPT-CI branch: the change goes to `main` as documents do; CI runs on the push and the card is not closed until it is green.)
* **stop if**: `docs/Decisions.md` has no such sentence at 2194 (it moved: find it by the words "ticket" and "0 to 1"; if neither is found, stop); a gate refuses a word of the new text for a reason that is not a typo.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "The Guide's \"At the same time\": what is guarded for threads, for several programs and one file, and for a reader of a file being written (Y-1)." No Roadmap row (the item "Reads and writes at the same time, guarded by choice" is marked 🟩 by Y-5: see its docs). No D-number.

---

## Y-2 - `cf_platform_write_file_if`: the replace refused when the file no longer holds the bytes read (F-1c in the research)

* **id**: Y-2
* **goal**: `cf_platform_write_file_if(path, data, size, expect, expect_size, o, err)` replaces a file as F-1a's `cf_file_replace` does, but only when the file still holds exactly `expect` (or is absent when `expect` is NULL), checked after the temporary is complete and just before it is put in place, and returns -1 and leaves the file as the other writer left it when it does not; and `overrides_write` (F-2a, as amended) uses it to apply its change again to the new bytes, up to `write_attempts` times (an option of the source, 3 by default: D-227).
* **why**: two programs that read, change and replace one file lose one update, silently (D-128 as F-2a drafted it). The research recommends the optimistic check always, with no option and no new public name (`docs/research/concurrent-access.md:511-529`, "First: no new public interface", point 2): the race then lasts from a re-read to a rename, microseconds, instead of a whole read, change and emit, and a change wins key by key, not file by file. It is what etcd does between its locks and git's, and what a lock would still leave open for writers that do not take it.
* **needs**: F-1a
* **size**: small (one function in each of three files, the call in F-2a's card)
* **version**: 1.9.0
* **status**: done (82ddc27)
* **decided**:
  * **Names** (internal tier, `lib/platform/include/configforge/cf_platform_file.h`, under `#if CF_INTERNAL_API`, after F-1a's `cf_platform_write_steps`; `o` is F-1a's `cf_file_options` (`cf_file.h`), NULL for the defaults, so the Windows rename retry and the sync are the options of the layer, D-224):
    ```c
    /* 1 when `path` holds exactly `size` bytes of `data`. -1 when the file no
       longer holds `expect` (`expect_size` bytes; NULL and 0: the file must not
       exist), with `err` set (CF_E_SOURCE, "'%s' changed while it was written")
       and the file as the other writer left it. 0 with `err` set for any other
       failure, as cf_file_replace. */
    CF_API int cf_platform_write_file_if(const char* path, const void* data, size_t size,
                                         const void* expect, size_t expect_size,
                                         const cf_file_options* o, cf_error* err);
    /* The same, calling `between(user)` once, after the temporary is complete
       and before the comparison: for the test, which changes the file there. No
       global state (D-101). NULL: no call. */
    CF_API int cf_platform_write_file_if_hook(const char* path, const void* data, size_t size,
                                              const void* expect, size_t expect_size,
                                              const cf_file_options* o,
                                              void (*between)(void* user), void* user,
                                              cf_error* err);
    ```
    `cf_platform_write_file_if` is `..._if_hook(..., o, NULL, NULL, err)`. Both are in the three `file.c` files (the serialization library carries them: `lib/platform/CMakeLists.txt:35-36`), and in the three lines each of `cmake/configforge.internal.symbols`, in that file's order.
  * **No code `CF_E_BUSY` yet.** That code arrives with Y-3 (D-227, point 4); this card does not wait for it: the refusal is `CF_E_SOURCE` and the return value -1 is what a caller tells it by. Y-4 changes the final refusal of the retries to `CF_E_BUSY`.
  * **Where the check goes.** F-1a's core is `write_core(path, data, size, o, fail_at, on_retry, user, err)`, one static function in each `file.c` that does the steps; this card gives it `expect`, `expect_size`, `between` and `user`. The check is after the last step before the rename (POSIX: after step 5, `close`; Windows: after step 4, `CloseHandle`) and is not an injectable step (`cf_platform_write_steps()` does not change). It is: call `between` if given; then read `path` whole with `cf_platform_fopen(path, "rb")` and `fread`, at most `expect_size + 1` bytes: equal when the read count is `expect_size` and `memcmp` is 0. When `expect` is NULL: the file must be missing (POSIX: `lstat` fails with `ENOENT`; Windows: `GetFileAttributesW` is `INVALID_FILE_ATTRIBUTES` with `ERROR_FILE_NOT_FOUND` or `ERROR_PATH_NOT_FOUND`), and any other answer is "changed". A file that cannot be read when `expect` is not NULL is "changed". On "changed" the temporary is removed as a failed step removes it and -1 is returned.
  * **Not decided here, and not needed:** an equal size and equal bytes with a different modification time is "unchanged": the check is of what the writer is about to destroy, which is bytes.
  * **D-186** (copy into `docs/Decisions.md` after the last D-number, titled "A write checks that the file is still what it was read from"): Question: two programs that read, change and replace one file lose one update. Decision: the check is of bytes, after the temporary is complete and just before the rename; a mismatch returns -1, the caller applies its change again to the new bytes up to `write_attempts` times (an option of the source, 3 by default, D-227) and then refuses with `'%s' changed while it was written; try again`; no option, no new public name, no file. Rejected: a stamp (a hint that can miss a rewrite of the same length inside one clock tick, `docs/Decisions.md:1689-1695`); unbounded retries (a file two programs rewrite forever never settles; the third refusal is the answer); a lock for this (Y-3 is for the program that asks for one). Held to: `test_writefile` cases (l) to (q).
* **files**: `lib/platform/include/configforge/cf_platform_file.h`, `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/none/file.c`, `lib/config/tests/test_writefile.c`, `cmake/configforge.internal.symbols`; and, only when F-2a has landed without its amendment, `lib/config/src/bridge/file.c`, `lib/config/include/configforge/cf_source.h` and `lib/config/tests/test_overrides.c` (step 6); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-2 main` (after F-1a has landed; `grep -n cf_platform_write_file_with lib/platform/include/configforge/cf_platform_file.h` must find it, else stop). If F-2a has landed too, step 6 is done in this card; if not, step 6 is already part of F-2a's amended card and is skipped here.
  2. In `lib/config/tests/test_writefile.c`, after F-1a's case (k), add C89 cases, each in a file `cf_wi.txt` written by `fopen "wb"` first unless said: (l) `expect` equal: `cf_platform_write_file_if(f, "new", 3, "old", 3, NULL, &err)` is 1 and the file reads `new`; (m) `expect` different: the file holds `other`, the call with `expect` `old` returns -1, `err.code` is `CF_E_SOURCE`, `err.message` contains `changed while it was written`, the file still reads `other`, and `fopen(".cf_wi.txt.cf-tmp-0", "rb")` is NULL; (n) `expect` NULL, 0 with the file present returns -1, and with the file absent returns 1; (o) `expect` `""`, 0 against an empty file returns 1 (note: size 0 with a non-NULL pointer means "must exist and be empty") and against an absent file returns -1; (p) the race: `cf_platform_write_file_if_hook(f, "new", 3, "old", 3, NULL, between, f, &err)` where `between` rewrites `f` with `fopen "wb"` to `mine`: returns -1, the file reads `mine`, no temporary is left. Also (q) a failed step (the loop of F-1a's case (d) with `_if`) leaves the old file and no temporary. Run in the build directory.
  3. See it fail: `cmake --build --preset default --target test_writefile 2>&1 | tail -n 20` (undeclared `cf_platform_write_file_if`).
  4. Add the declarations, the check in the three core functions, the two entry points in each, and the symbol lines. `ctest --preset default -R "test_writefile|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: in `posix/file.c` make the comparison always equal (behind `if (fail_at == 0xFFFFu)`, a run-time-false condition, never a constant); case (m) and (p) fail; put it back. (On Windows run the same with `win32/file.c`.)
  6. F-2a's amendment (written into F-2a's own section of `docs/plan/F-files.md`: its "The optimistic check" bullet, step 2's cases (l) and (m), step 5): when F-2a has not landed, F-2a carries it and this step is already done by that card (say so in the report); when F-2a landed without it, this card carries it out as F-2a's section now reads: `overrides_write` keeps the bytes it read, calls `cf_platform_write_file_if`, applies again on -1 up to the source's `write_attempts` (3 by default; `cf_file_source_options`, D-225, holds it beside `wait_ms`, as D-227 point 2 says an option and point 3 places the wait there) and `test_overrides.c` gains F-2a's cases (l) and (m), which use `cfi_overrides_write`'s `between` hook.
  7. D-186 into `docs/Decisions.md`; the CHANGELOG entry.
  8. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-2: cf_platform_write_file_if, the replace refused when the file changed (D-186)`.
  9. ACCEPT; push `work/Y-2`; ACCEPT-CI.
* **Organiser's answers** (Y-2):
  * Case (q) needs a failing step, which `cf_platform_write_file_if` and `_hook` cannot take; a third internal name, `cf_platform_write_file_if_with(..., o, fail_at, between, user, err)`, is the core the platform files define, and `_if` and `_if_hook` are two small wrappers in `lib/platform/src/file_layer.c` (once, not in three files); the symbols file gets three lines. (the maintainer's standing approval, 2026-11-27)
  * The comparison of bytes is `cfi_file_holds` in `file_layer.c`; each platform file keeps only the absent-file test. F-2a has not landed, so step 6 belongs to F-2a's amended card. (the maintainer's standing approval, 2026-11-27)
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (cases (l) to (q) run on both); ACCEPT-CI; `ctest --preset default -R test_writefile` exit 0 in `default` and `minimal`.
* **stop if**: F-1a is not landed; `gate.symbols` or `gate.layering` refuses the names and the fix is not a line of `cmake/configforge.internal.symbols`; case (l) cannot be made without a thread or a second process (the hook must be enough: report which part is not); F-2a has landed and its `overrides_write` is not shaped as F-2a's "decided" says (report the difference, change nothing in it); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added" (this card's version; the heading is added above the newest one when it is not there): "`overrides_write` applies its change again when the file changed while it was written, up to 3 times by default, then refuses: two programs that set different keys of one override file both keep their key (Y-2, D-186)". `docs/Decisions.md`: D-186. Roadmap: nothing until Y-5.

---

## Y-3 - `cf_locker`: a lock that plugs in, `cf_lock_none` and a kernel lock on a kept sidecar file, `cf_lock_sidecar` (F-5a in the research)

* **id**: Y-3
* **goal**: `cf_locker` is a part that plugs in (D-227), as `cf_packer` and `cf_unpacker` do (D-221): its `take(path, wait_ms, err)` and `give` are fillable by a program; `cf_lock_none()` (the default; takes nothing) and `cf_lock_sidecar()` (an exclusive kernel lock on the file `<dir>/.<base>.cf-lock` beside `path`: `flock`, an open-file-description lock or a classic `fcntl` lock on POSIX, `LockFileEx` on Windows, released by a crash) are built in; the sidecar waits up to `wait_ms` milliseconds in a loop of non-blocking attempts; a test shows two takes of one path in one process exclude each other on every platform, and a locker the test fills in itself is called as a built-in one is.
* **why**: the optimistic check of Y-2 leaves a window of microseconds, and a program that runs several copies of one service writing one file, or an installer and the program at once, may want it closed too; another program may want a lock of its own kind (a service it asks, a database lock, an embedded mutex). The Roadmap item asks for "an advisory lock on the file written (`flock`/`fcntl`, `LockFileEx`)" as an option the program asks for (`docs/Roadmap.md:1555-1561`); the research settles which call on which platform (`docs/research/concurrent-access.md:433-483`, "What P2 needs"); the maintainer settled the shape (D-227, "Organiser's answers" below).
* **needs**: F-1a (the sidecar's name follows the temporary's; `cf_lock.h` sits beside `cf_file.h`)
* **size**: medium (a public header, three platform files of the same shape, one portable file, one test)
* **version**: 1.9.0
* **status**: done (3b241ee)
* **decided**:
  * **The interface (D-227, D-220)**, a new public header `lib/platform/include/configforge/cf_lock.h` (includes `cf_port.h`, `cf_error.h`; the platform is a part, D-220, and the locker knows nothing of configuration):
    ```c
    /* A held lock. `handle` is -1 when nothing is held; the locker's `take` fills it and its
       `give` clears it. `data` is the locker's own (the sidecar does not use it). */
    typedef struct cf_lock { long handle; void* data; } cf_lock;
    typedef struct cf_locker {
        const char*   name;              /* "none", "sidecar", or the program's */
        unsigned long suggested_wait_ms; /* the wait this locker asks for when neither the source
                                            nor the configuration gives one (as git's locks do); 0: none */
        /* 1 with the lock held on `path`, `held->handle` set; 0 with `err` set: CF_E_BUSY after
           `wait_ms` (place: `path`, reason: who holds it, D-223), another code for any other failure. */
        cf_bool (*take)(const struct cf_locker* self, cf_lock* held, const char* path,
                        unsigned long wait_ms, cf_error* err);
        /* Releases; does nothing for a lock not held. */
        void    (*give)(const struct cf_locker* self, cf_lock* held);
        void*         user;              /* the program's own; given back through `self` */
    } cf_locker;
    /* The built-in lockers, as functions (no data is exported from the library): the default,
       which takes nothing and always succeeds, and the kernel lock on the sidecar file. A
       `cf_lock_sidecar()` on the `none` platform returns a locker whose `take` fails
       CF_E_CAPABILITY, `this build has no files to lock`. */
    CF_API const cf_locker* cf_lock_none(void);
    CF_API const cf_locker* cf_lock_sidecar(void);
    ```
    A NULL `cf_locker*` wherever one is taken (Y-4's `cf_file_source_options.lock`) is `cf_lock_none()`. A program fills its own `cf_locker` (a struct on its stack or static, or one from a table); nothing here registers it.
  * **Where.** The two built-ins and the portable part in a new `lib/platform/src/lock.c`, added after the `file.c` line in `SOURCES` of `lib/platform/CMakeLists.txt:33-37` and not in `SERIALIZE_SOURCES` (the serialization library does not take locks; the config library does). The sidecar's body is an internal platform primitive in the full platform files, `lib/platform/src/posix/posix.c`, `win32/win32.c`, `none/none.c`, declared in `lib/platform/include/configforge/cf_platform.h` under `#if CF_INTERNAL_API`, after `cf_platform_stamp` (line 255-256), with its own comment (the header adds `#include "configforge/cf_lock.h"`):
    ```c
    /* The sidecar lock on `path`.cf-lock: 1 with it held; 0 with err set: CF_E_BUSY
       "'%s' is busy: another writer holds its lock; try again" after `wait_ms`,
       CF_E_SOURCE "cannot lock '%s': %s" when the lock file cannot be opened. */
    CF_API int  cf_platform_lock_take(cf_lock* held, const char* path, unsigned long wait_ms, cf_error* err);
    /* Releases and closes; does nothing for a lock not held. The lock file is kept. */
    CF_API void cf_platform_lock_give(cf_lock* held);
    ```
    `held->handle` is a file descriptor on POSIX, `(long)(INT_PTR)` of the `HANDLE` on Windows, -1 when not held; `cf_platform_lock_take` sets it, `give` sets it to -1. `cf_lock_sidecar()`'s `take` and `give` call these two.
  * **The code (D-223).** `lib/base/include/configforge/cf_code.def`: a new last entry, so that no existing value moves: `CF_E_MEMORY` becomes a `CF_CODE_X` line and the new line is the `CF_CODE_LAST_X`: `CF_CODE_LAST_X(CF_E_BUSY, "busy", "another writer holds it; try again")`. Its value is 10 (`CF_E_MEMORY` stays 9; `bindings/rust/src/sys.rs` has `CF_E_POLICY` as 8 at line 49). The language bindings read the name through `cf_code_name`; add `CF_E_BUSY` to Rust's `sys.rs` only if the file lists every code (it lists three: no change). `docs/API.md`'s code table is generated. It is added here, by the first card that returns it; Y-2's refusal (`CF_E_SOURCE` while no such code existed) becomes `CF_E_BUSY` in Y-4. A busy error carries its place and its reason (D-223): the path and `another writer holds its lock`.
  * **POSIX.** The sidecar is `<dir>/.<base>.cf-lock` (`<dir>` as F-1a's temporary: the path up to the last `/`, `.` when there is none). `lstat` first: a symbolic link is refused (`'%s' is a symbolic link`), a non-regular file is refused (`'%s' is not a regular file`), a missing one is fine; then `open(O_RDWR | O_CREAT | O_CLOEXEC where defined | O_NOFOLLOW where defined, mode)` (`fcntl(F_SETFD, FD_CLOEXEC)` right after where `O_CLOEXEC` is not defined: Y-8's rule) with mode the configuration file's own `st_mode & 0666` when it exists and `0666` otherwise (the umask applies; whoever may write the file may lock it). The lock call, **one protocol per platform, chosen at build time, never two**: `flock(fd, LOCK_EX | LOCK_NB)` when `LOCK_EX` is defined (Linux, the BSDs, macOS, musl); else `fcntl(fd, F_OFD_SETLK, &fl)` when `F_OFD_SETLK` is defined; else `fcntl(fd, F_SETLK, &fl)` with `fl` the whole file, whose two faults (closing any descriptor of the file drops the process's locks; threads of one process do not exclude each other) are written into the function's header comment and into D-187. A refusal because it is held (`EWOULDBLOCK`, `EAGAIN`, and `EACCES` for `F_SETLK`) is "busy"; every other failure is "cannot lock". `EINTR` retries at once.
  * **Windows.** `CreateFileW` on the sidecar (the wide name through `win32_wide`, `win32.c:101`) with `GENERIC_READ | GENERIC_WRITE`, `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`, `OPEN_ALWAYS`, no security attributes (not inheritable: D-190); then `LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)` on one byte at offset 0; `ERROR_LOCK_VIOLATION` is "busy". No one reads the sidecar, so a mandatory lock on it costs nothing.
  * **none.** `cf_platform_lock_take` returns 0 with `CF_E_CAPABILITY`, `this build has no files to lock`; `give` does nothing. `cf_lock_none()` works on every platform.
  * **The wait.** Not an operating-system wait: a loop in the platform layer of non-blocking attempts with growing sleeps, as git does: attempt, then sleep 1, 2, 4, ... ms (each at most 100 ms: a pacing of the loop, not a limit on what a program asks, and never past the bound), until `wait_ms` is spent; `wait_ms` 0 is one attempt. POSIX sleeps with `poll(NULL, 0, ms)` (`posix.c:938` is the file's use of `poll`), Windows with `Sleep` (`win32.c:443`'s use). The time spent is the sum of the sleeps (no clock), so the bound is a lower bound on the wall time and an upper bound on the sleeping; a program that wants an exact deadline calls again. No wait is unbounded and none is clamped: the type is `unsigned long` milliseconds and the value is the program's (D-220: an option with a default, never a fixed limit; the default is Y-4's).
  * **The sidecar is kept and never removed** (removing it would bring back the rename-over-a-lock case: a second program opens the old file, the first removes it and a third creates a new one, and two hold "the" lock). It is never followed through a symbolic link.
  * **Rejected**: a `CF_FILE_LOCK` flag on a file source (D-227: the lock is an option that plugs in, `cf_file_source_options.lock`); one fixed lock in the library (a program may want a service's or a database's); a lock on by default (D-227: `cf_lock_none` is the default; no file is left in a directory unless the sidecar is chosen).
  * **D-187** (copy into `docs/Decisions.md` after D-186, titled "A write may take a lock that plugs in: none by default, a kernel lock on a kept file when the program chooses it"): Question: should a write be able to exclude another writer at all, and with what. Options: no lock, the optimistic check only (what the default does); a lock by existence; a kernel lock on a kept sidecar; a locker the program fills in. Decision (D-227): `cf_locker`, `cf_lock_none` by default, `cf_lock_sidecar`, the program's own; the call per platform, the sidecar's name, mode and kept status, the wait as a loop, one protocol per platform, "busy" (`CF_E_BUSY`) distinct from "cannot lock". Rejected: a lock by existence (a stale lock after every crash, and D-127 refuses to clean one); locking the configuration file itself (a Windows reader would be refused and the rename would fail over a locked file); a lock on the read path or inside `cf_config` (D-14 holds, and with writers that never write in place a reader always opens a whole file); a writer service; an unbounded wait. D-128's "Rejected" list: in `docs/Decisions.md`, its words "a lock file" become "a lock file by existence (D-187 holds the kernel lock a program chooses)". Held to: `test_lock`.
* **files**: `lib/platform/include/configforge/cf_lock.h` (new), `lib/platform/include/configforge/cf_platform.h`, `lib/platform/src/lock.c` (new), `lib/platform/CMakeLists.txt`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/win32.c`, `lib/platform/src/none/none.c`, `lib/base/include/configforge/cf_code.def`, `lib/config/tests/test_lock.c` (new), `lib/config/tests/CMakeLists.txt` (after `test_writefile`), `cmake/configforge.symbols` (public, sorted: `cf_lock_none`, `cf_lock_sidecar`), `cmake/configforge.internal.symbols` (`cf_platform_lock_give`, `cf_platform_lock_take`), `lib/platform/README.md` (one line); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-3 main` (after F-1a has landed, and Y-2 when it is open: they touch different files).
  2. Write `lib/config/tests/test_lock.c`, C89, `#include <configforge/cf_lock.h>`, `"cf_test.h"`, in the build directory, file name `cf_lk.txt`; `s` is `cf_lock_sidecar()`. Cases: (a) `s->take(s, &a, "cf_lk.txt", 0, &err)` is 1 and `.cf_lk.txt.cf-lock` exists; (b) a second `s->take(s, &b, "cf_lk.txt", 0, &err2)` in the same process returns 0, `err2.code` is `CF_E_BUSY`, `err2.message` contains `is busy`; (c) after `s->give(s, &a)` the second `take` succeeds, and `give(&b)`; (d) the wait: with `a` held, `take(&b, ..., 1500, &err)` returns 0 and `time(NULL)` has moved by at least 1 between before and after (C89's `time()` is the only wall clock the test has; `clock()` is CPU time and cannot see a sleep; a 1.5 s wait always crosses a second); (e) POSIX only (`#if CF_PLATFORM_POSIX`): `remove(".cf_lk.txt.cf-lock")`, then `system("ln -s cf_other .cf_lk.txt.cf-lock")` (skip the case when it returns non-zero) and `take` is refused with `is a symbolic link`, `cf_other` not created; (f) `give` of a lock never taken (`handle` -1), twice, is harmless; (g) the sidecar is still there after `give` and the file `cf_lk.txt` is untouched (not created); (h) `cf_lock_none()`: `take` is 1 twice for the same path with no file made, `name` is `none`, `suggested_wait_ms` is 0, and a NULL locker is the caller's `cf_lock_none()` (this is Y-4's rule; the case here only checks `name`); (i) a plugged-in locker: a `cf_locker` the test fills (a `take` that counts calls and returns 0 with `CF_E_BUSY` and the message `held by the test` when its `user` counter is odd, else 1; a `give` that counts) is called through its own pointers, `self->user` reaches it, and `suggested_wait_ms` 7 reads back; (j) `cf_code_name(CF_E_BUSY)` is `busy` and `CF_E_MEMORY` is still 9. On a build with no files (`CF_PLATFORM_NONE`) the test checks only `CF_E_CAPABILITY` of the sidecar and cases (h) to (j). Add `test_lock` to the list.
  3. See it fail: `cmake --build --preset default --target test_lock 2>&1 | tail -n 20` (no `cf_lock.h`).
  4. Add the header, the code, `lock.c`, the three platform implementations, the symbol lines and the README line. `ctest --preset default -R "test_lock|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: in `posix.c` (or `win32.c` on Windows) make `cf_platform_lock_take` return 1 without the lock call, behind a run-time-false condition (`if (wait_ms != 0xFFFFFFFFul)` selects the broken path, never a constant: the negative-proof rule for MSVC); case (b) fails; put it back.
  6. D-187 and D-128's edit into `docs/Decisions.md`; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-3: cf_locker, cf_lock_none and cf_lock_sidecar, a kernel lock on a kept sidecar (D-187)`.
  8. ACCEPT; push `work/Y-3`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI (macOS and FreeBSD jobs included: `flock` is exercised there, and the BSD runner is where an `F_OFD_SETLK`-less path would show; the language bindings' own tests still pass in CI: `cf_code_name` carries the new code); `ctest --preset default -R test_lock` exit 0 in `default` and `minimal`.
* **stop if**: F-1a has not landed; a CI target defines neither `LOCK_EX`, `F_OFD_SETLK` nor `F_SETLK` (report which); case (b) passes on a target where the protocol is classic `F_SETLK` (threads of one process share classic locks: the test is then `#if`-excluded there and the report says on which target); `gate.layering` refuses `posix.c`'s new includes (`<sys/file.h>`, `<fcntl.h>`) or `cf_lock.h`'s place in `lib/platform`; `gate.abi` refuses the new enum value because the enum is read as a closed set somewhere (report where).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased" (this card's version; the heading is added above the newest one when it is not there), "### Added": "`cf_locker`, a lock that plugs in, with `cf_lock_none()` (the default) and `cf_lock_sidecar()`, a kernel lock on a kept sidecar file that waits up to a bound the program gives, and the new code `CF_E_BUSY` (Y-3, D-187)". `docs/Decisions.md`: D-187 and the edit of D-128. Roadmap: nothing until Y-5.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve", after the comparison of docs/research/concurrent-access.md section 2), written as **D-227**, answering what the lock is (Y-3), where the wait is set and how "busy" is reported (Y-4):
  * 2026-11-27, the organiser: a build whose platform has no lock call (WASI) compiles and `cf_lock_sidecar`'s take returns `CF_E_CAPABILITY`, as D-220 asks of a feature the target cannot give, instead of the card's stop (no CI target hit it) (the maintainer's standing approval, 2026-11-27).
  1. The lock is a part that plugs in: `cf_locker` (its name; `take(path, wait_ms, err)`; `give`), as `cf_packer` and `cf_unpacker` are. Built in: `cf_lock_none` (the default) and `cf_lock_sidecar` (Y-3's kernel lock on `<dir>/.<base>.cf-lock`: `flock`, OFD or `fcntl` on POSIX, `LockFileEx` on Windows; released by a crash). A program may plug in its own (a service it asks, a database lock, an embedded mutex).
  2. Y-2's optimistic check is always on; its retry count is an option, 3 by default.
  3. The wait is an option of each source in `cf_file_source_options` (`wait_ms`, 0 by default, as SQLite's), with a default for the whole configuration by `cf_builder_write_wait(b, ms)`; a `cf_locker` may carry its own suggested wait (as git's locks do). The SQLite store uses the same wait (`sqlite3_busy_timeout`).
  4. A lock that is busy, or retries spent, return `CF_E_BUSY` with its place and reason (D-223).
  5. The lock is chosen by `cf_file_source_options.lock` (`cf_lock_none` by default). D-128's "no lock file" holds for the default; the sidecar exists only when it is chosen.

## Y-4 - `cf_file_source_options.lock`, `.wait_ms` and `.write_attempts`, `.read_attempts`, `cf_builder_write_wait`: the lock around `overrides_write`, and the store's wait (F-5b in the research)

* **id**: Y-4
* **goal**: a file source added with `cf_file_source_options.lock` set to a `cf_locker` (Y-3: `cf_lock_sidecar()`, or the program's own) has every `cf_write` to it taken under that lock, waiting as long as the wait resolves to (below) before it is refused with `CF_E_BUSY`; `cf_file_source_options.write_attempts` (3 by default) sets how often a write applies its change again to a file that changed (Y-2), and the exhausted attempts are `CF_E_BUSY` too; and the SQLite store waits the same time (`sqlite3_busy_timeout`) before it is refused with `CF_E_BUSY`.
* **why**: the Roadmap asks, as an option the program asks for, for "what the library does when the lock is held (wait with a bound, or refuse)" (`docs/Roadmap.md:1559-1561`). The store has locks of its own and no wait: no `sqlite3_busy_timeout` and no `PRAGMA busy_timeout` is set anywhere in `lib/`, so a second writer that finds the store locked fails at once with `SQLITE_BUSY` (`lib/config/src/bridge/store.c:231`, `BEGIN IMMEDIATE`). The program's reaction to "another writer holds it" (try later, tell the user that another program is saving) differs from a file that cannot be written, so it has a code of its own, `CF_E_BUSY` (Y-3; `docs/research/concurrent-access.md:469-476`). The maintainer decided the lock, the wait and the code (D-227, "Organiser's answers" under Y-3).
* **needs**: Y-3, F-2a
* **size**: medium (a public function, four members of an options struct, three call sites, one test)
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **The options (D-227, D-220).** F-2a (D-225) defines `cf_file_source_options` (in `configforge.h`) and its init function and `cf_builder_file_with`; this card appends four members to the struct, each with its default set by that init function (find both by `grep -n cf_file_source_options lib/config/include/configforge/configforge.h`):
    ```c
    const cf_locker* lock;           /* NULL: cf_lock_none(), nothing taken (D-128's "no lock file" holds for the default) */
    unsigned long    wait_ms;        /* 0: not set (resolution below) */
    unsigned         write_attempts; /* how many times in all a write applies its change to a file that changed; 0 reads as 3, the default */
    unsigned         read_attempts;  /* how many reads in all a stable read makes (Y-5); 0 reads as 3, the default; copied into Y-5's `file_state.read_attempts` by `cf_builder_file_with` */
    ```
    The lock is advisory (a writer that does not take it is not stopped) and the sidecar of `cf_lock_sidecar()` is kept. `cf_builder_overrides(b, name, rank, path)` (D-225's shortcut) sets none of them. There is no `CF_FILE_LOCK` flag (Rejected, D-227: a lock is an option that plugs in).
  * **The wait, and where it is set.** `wait_ms` of each source; a default for the whole configuration, `CF_API cf_bool cf_builder_write_wait(cf_builder* b, unsigned long wait_ms);` in `lib/config/include/configforge/configforge.h` after `cf_builder_writable` (line 272), with the comment: "How long a write waits for another writer of the same file or store, in milliseconds, before it is refused with CF_E_BUSY. 0, the default: not set. Returns CF_FALSE with the builder's error set on a NULL builder." The wait a source uses is resolved once, in `cf_config_create`: the source's own `wait_ms` when not 0, else the builder's `cf_builder_write_wait` when not 0, else its locker's `suggested_wait_ms` (a locker may carry its own, as git's locks do), else 0, which is refused at once as SQLite's default is. 0 therefore means "not set"; a program that wants no wait does not set one. The SQLite store has no locker: it uses the builder's value. The Windows retry of F-1a (`cf_file_options.retry_ms`) is not this wait: it is part of making one write work at all.
  * **How the options travel.** `struct cf_source` (`lib/config/include/configforge/cf_source.h:90-110`, after `u`) gets three appended fields: `const cf_locker* lock;`, `unsigned long write_wait_ms;` (the source's own `wait_ms` until `cf_config_create` resolves it) and `unsigned write_attempts;`; `memset(s, 0, ...)` in the builder (`config.c:1595`) already zeroes them, and `cf_builder_file_with` sets them from the options. The builder keeps `write_wait_ms` (next to `nwritable`, `config.c:79-80`), `cf_builder_write_wait` sets it, `cf_config_create` resolves it into every `c->sources[k]` (the loop at `config.c:1304-1335`), and `derive_onto` copies the base's resolved values (next to the grant loop, `config.c:1660-1671`); `derive_ghost` (`config.c:1580-1608`) carries `vt->write` and the path only and has no `file_state`, so it copies the three fields as it copies `rank` (`s->lock = o->lock;` and the two others after `s->rank`, line 1599). `overrides_write` and `store_write` read `s->lock`, `s->write_wait_ms` and `s->write_attempts`, never `file_state`. `struct cf_config` gets no new field.
  * **The lock around `overrides_write`** (F-2a's steps): when `s->lock` is not NULL, `s->lock->take(s->lock, &held, path, s->write_wait_ms, err)` before step (1), `s->lock->give(s->lock, &held)` after step (6) and on every failure after the take. A take that fails is the locker's error as it is: for the sidecar, `CF_E_BUSY` when the lock is held (`'%s' is busy: another writer holds its lock; try again`, `source` the path) and `CF_E_SOURCE` when the lock file cannot be opened.
  * **Y-2's attempts.** `overrides_write` passes `s->write_attempts` (0 read as 3) where F-2a's amendment has the number of applications; the last refusal (`'%s' changed while it was written; try again`) is now `CF_E_BUSY` (same message, the path as its place). Y-2's tests and F-2a's case (m) that look at `err.code` change from `CF_E_SOURCE` to `CF_E_BUSY`.
  * **The store's wait.** `cf_store_set` keeps its public signature (`cf_source.h:489`, called by `test_store.c`). A new `cf_store_set_wait(path, wait_ms, items, count, meta, err)` next to it, internal (`#if CF_INTERNAL_API`, symbol line in `cmake/configforge.internal.symbols`), is the body; `cf_store_set` calls it with 0; `store_write` (`store.c:332`) calls it with `s->write_wait_ms`. Inside it, after `sqlite3_open_v2` succeeds and before `store_schema` is executed (`store.c:228-231`), `sqlite3_busy_timeout(db, (int)min(wait_ms, 2147483647))` (SQLite's own bound: an `int` of milliseconds). When `store_failed` finds `sqlite3_errcode(db) == SQLITE_BUSY` (or `SQLITE_LOCKED`), the error is `CF_E_BUSY`, message `writing the store '%s': another writer holds it; try again`; any other failure is as it was (`store.c:187-190`).
  * **D-188** (after D-187, "A write waits as long as the program says, and is refused as busy"): Question: what the library does when another writer holds the file or the store. Decision (D-227): the options above; the wait resolved source, then configuration, then locker, then 0 (refused at once); the wait is a bounded loop in the platform layer for a locker and SQLite's own busy handler for the store; one code, `CF_E_BUSY`, for a held lock, a busy store and the exhausted attempts; the attempts are an option, 3 by default. Rejected: an unbounded wait (a program that wants to wait longer calls again); a wait argument on `cf_write` (breaks the signature before 2.0.0); a default wait (a program that asked nothing sees nothing new); a sentinel that tells "not set" from 0 (0 is SQLite's default and the default here); `CF_E_SOURCE` with a reason (reasons are document refusals, D-107). Held to: `test_busy`.
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_source.h`, `lib/config/src/core/config.c`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/store.c`, `lib/config/tests/test_busy.c` (new), `lib/config/tests/CMakeLists.txt`, `cmake/configforge.symbols` (`cf_builder_write_wait`), `cmake/configforge.internal.symbols` (`cf_store_set_wait`), the tests of Y-2 and F-2a that look at the code; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (Y-1's section gains the options), `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-4 main` (after Y-3, F-2a and Y-2 have landed; `grep -n cf_file_source_options lib/config/include/configforge/configforge.h` must find the struct, and `grep -n CF_E_BUSY lib/base/include/configforge/cf_code.def` the code, else stop).
  2. Write `lib/config/tests/test_busy.c`, C89, after `test_overrides.c`'s way of building a configuration. Cases: (a) a file `cf_bz.json` added through `cf_builder_file_with` with `writable` and `lock = cf_lock_sidecar()`, the lock taken by the test's own `cf_lock_sidecar()->take` on `cf_bz.json`: `cf_write` returns `CF_FALSE`, `err.code` is `CF_E_BUSY`, the message contains `is busy`, and the file is as it was; (b) the same with the lock given first: the write succeeds; (c) `cf_builder_write_wait(b, 1500)` with the lock held: refused, and `time(NULL)` has moved by at least 1 (as Y-3's case (d)); (d) without a `lock` (NULL) the write succeeds with the sidecar held (the lock is the program's choice); (e) the resolution: the source's `wait_ms` 1500 wins over the builder's 0; a builder wait of 1500 is used by a source with `wait_ms` 0; a locker with `suggested_wait_ms` 1500 is used when neither is set; each shown by `time(NULL)` as in (c); (f) after the write, the sidecar `.cf_bz.json.cf-lock` exists and the file reads as written; (g) a `cf_derive`d configuration writes with the same wait and lock; (h) `#if CF_WITH_SQLITE`: open a second connection to `cf_bz.db`, `BEGIN EXCLUSIVE` on it, `cf_write` to the store with wait 0 returns `CF_E_BUSY` at once and with `cf_builder_write_wait(b, 1500)` returns it with `time(NULL)` moved by at least 1, and after `COMMIT` of the second connection the write succeeds; (i) a plugged-in locker the test fills in (Y-3's case (i)) is called once by `take` and once by `give` for each `cf_write`, and a `take` of it that fails `CF_E_BUSY` makes `cf_write` fail `CF_E_BUSY` with the file as it was; (j) `cf_builder_write_wait(NULL, 5)` is `CF_FALSE`; (k) `write_attempts` 2 with a `between` hook that changes the file every time (F-2a's `cfi_overrides_write`): `between` is called 2 times and the refusal is `CF_E_BUSY`; with `write_attempts` 0 it is called 3 times. Add `test_busy` to the list.
  3. See it fail: `cmake --build --preset default --target test_busy 2>&1 | tail -n 20` (undeclared `cf_builder_write_wait`, or no `lock` member).
  4. Add the four members and their defaults, the three fields of `cf_source` (`read_attempts` lives in the file source's `file_state`), the builder call, `cf_store_set_wait` and the call sites. `ctest --preset default -R "test_busy|test_overrides|test_store|test_write|test_writefile|test_lock|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: make `store_write` pass 0 instead of `s->write_wait_ms` (behind `if (count != 0xFFFFu)` a run-time-false test, not a constant); case (h)'s wait half fails; put it back. Then make `overrides_write` skip the take (same kind of condition); case (a) fails; put it back.
  6. D-188 into `docs/Decisions.md`; Y-1's section in `docs/Guide.md` gains a paragraph "When the program asks": the `lock` option and `cf_lock_sidecar()`, `wait_ms` and `cf_builder_write_wait`, what `CF_E_BUSY` means, that the lock is advisory (a writer that does not take it is not stopped), that its file is kept, and that a program may plug in its own locker; the CHANGELOG entries.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-4: the lock and wait options of a file source, cf_builder_write_wait, the store's wait (D-188)`.
  8. ACCEPT; push `work/Y-4`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_busy|test_overrides|test_store|test_write"` exit 0, and `ctest --preset minimal -R test_busy` exit 0 (it builds, and says what is absent).
* **stop if**: `cf_file_source_options`, its init function or `cf_builder_file_with` is not in the tree as D-225 describes (report the real names, change nothing); `gate.interface` or `gate.abi` refuses the three appended `cf_source` fields (the struct is read by sources outside the tree: report, change nothing); `sqlite3_busy_timeout` is not in the bundled SQLite (`grep -rn busy_timeout` the vendored header; report); the grant loop of `derive_onto` cannot read the base's wait (the fields are not on `cf_config`'s path: report).
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased" (this card's version; the heading is added above the newest one when it is not there), "### Added": "`lock`, `wait_ms`, `write_attempts` and `read_attempts` of a file source and `cf_builder_write_wait`: a write takes the locker the program chose, waits as long as the program says, then is refused as busy (`CF_E_BUSY`); the store waits the same time (Y-4, D-188)"; "### Changed": "an override file's refused retries are `CF_E_BUSY`, not `CF_E_SOURCE` (Y-4)". `docs/Decisions.md`: D-188. Roadmap: nothing until Y-5.

---
* **Organiser's answers** (2026-11-26): the wait and the code are answered by D-227, written under Y-3's "Organiser's answers"; this card was rewritten to it on 2026-11-27.

## Y-5 - A stable read: a file read twice and compared, up to 3 reads by default (F-5c in the research)

* **id**: Y-5
* **goal**: the file source reads a file only after taking its stamp, and trusts the bytes only when the stamp taken again after the read is the same; when it moved, it reads again, up to `read_attempts` reads in all (3 by default, an option: D-220), and then fails as an unreadable file does (a reload then keeps the running snapshot); and the Roadmap item "Reads and writes at the same time, guarded by choice" is marked done.
* **why**: a reader of a file that something else (an editor, a deployment tool) is writing in place can read half of it, and no lock can stop that, because the writer does not take one (`docs/research/concurrent-access.md:490-507`, option R2). Taking the stamp before and after costs one `stat` more per file read, adds no public name, and catches most in-place writers. `cf_blob_file` reads with `fread` once and nothing checks that the file did not change while it was read (`lib/serialize/src/blob.c:59-110`).
* **needs**: nothing
* **size**: small
* **version**: 1.9.0
* **status**: done (442f52e)
* **decided**:
  * **Where.** Not in `cf_blob_file` (the serialization library has no stamp: `cf_platform_stamp` is in `posix.c`, `win32.c` and `none.c`, outside what the serialization library carries). In the file source, `lib/config/src/bridge/file.c`, at the one call that reads a file whole: `file_load`'s `cf_blob_file(&blob, bytes, st->path, &st->err)` (line 421). A static helper is not testable alone: the card adds an internal function, declared under `#if CF_INTERNAL_API` in `lib/config/include/configforge/cf_source.h` beside `cfi_source_text` (line 277), with its symbol line in `cmake/configforge.internal.symbols`:
    ```c
    /* cf_blob_file, but the file is stamped before and after the read and read
       again when the stamp moved (mtime, size or presence), up to `attempts` reads in all
       (0 reads as 3, the default); CF_E_SOURCE "'%s' changed while it was read"
       when all of them saw it move.
       `between(user)`, when not NULL, is called after each stamp and before the
       read: for the test, which changes the file there. A file that cannot be
       opened fails as cf_blob_file's does, with no retry (an absent file is not a
       file being written). */
    CF_API cf_bool cfi_file_read_stable(cf_blob* b, cf_arena* ar, const char* path,
                                        unsigned attempts,
                                        void (*between)(void* user), void* user,
                                        cf_error* err);
    ```
    `file_load` calls it with `st->read_attempts, NULL, NULL`. The count is an option with a default, never a fixed number (D-220): `file_state` (`lib/config/src/bridge/file.c:38-53`) gets a member `unsigned read_attempts;`, which the source's initialiser sets to 3 right after its `memset` (`lib/config/src/bridge/file.c:488`); the public member `read_attempts` of `cf_file_source_options` (D-225) is appended by Y-4 together with the lock and wait options, and sets this one.
  * **The comparison.** `cf_stamp` (`cf_platform.h:245-249`): equal when `mtime`, `size` and `present` are all equal. A stamp that `cf_platform_stamp` cannot take (returns `CF_FALSE`) makes the read plain: one read, no comparison (a platform with no stamp is the `none` build, which has no files). Each re-read allocates its bytes from the same arena `ar`; the abandoned reads are zeroed when the arena is (a file may hold a secret, `blob.c:50-55`'s `cf_blob_arena_done`).
  * **A hint, said so.** The stamp is a hint (`docs/Decisions.md:1689-1695`): a rewrite of the same length inside one clock tick passes it. The page says "catches most in-place writers", not all.
  * **D-189** (after D-188, "A file is read twice and compared"): Question: a reader of a file being written in place. Options: writers that never write in place (F-1a, already the library's); a stable read; a shared lock on every read (rejected: it buys nothing, since library writers never write in place and other writers take no lock, and it would make a reader wait for a writer, which D-14 refuses on the read path); readers that share delete access (Y-6). Decision: the stable read, `read_attempts` reads (an option, 3 by default), then the source fails as an unreadable file does; which `cf_live` already survives. Held to: `test_stable`.
  * **The Roadmap item.** After Y-2, Y-3, Y-4 and this card have landed, the item "Reads and writes at the same time, guarded by choice." in `docs/Roadmap.md` (heading at line 1555 of the tree at cbff5a9; find it by its words) is marked 🟩 by this card, with one line added under it: what was built and where (Y-2, Y-3, Y-4, Y-5, Y-6, D-186 to D-189). When an earlier card of this topic is still open, this card marks nothing and says so in its report.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/file.c`, `lib/config/tests/test_stable.c` (new), `lib/config/tests/CMakeLists.txt`, `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`, `docs/Guide.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-5 main`.
  2. Write `lib/config/tests/test_stable.c`, C89, `"cf_test.h"`, in the build directory, file `cf_st.json`. Cases: (a) a stable file: `cfi_file_read_stable(&b, ar, "cf_st.json", 0, NULL, NULL, &err)` returns `CF_TRUE`, bytes equal the file; (b) one change: `between` rewrites the file with a different length the first time it is called: the call returns `CF_TRUE` and the bytes are the new ones (2 reads); (c) the file moves at every call of `between` (a counter makes each rewrite a different length): the call returns `CF_FALSE`, `err.code` is `CF_E_SOURCE`, the message contains `changed while it was read`, and `between` was called 3 times with `attempts` 0 and 2 times with `attempts` 2; (d) a missing file fails as `cf_blob_file` does (`cannot open`), `between` called 0 or 1 times and no retry; (e) a configuration built with `cf_builder_file` on a file that is stable reads it as before (one build). Use arenas from `cf_arena_create` (as `test_file.c` does; `grep -n cf_arena_create lib/config/tests/test_file.c | head -3`). Add `test_stable` to the list.
  3. See it fail: `cmake --build --preset default --target test_stable 2>&1 | tail -n 20` (undeclared `cfi_file_read_stable`).
  4. Add the declaration, the function, the symbol line, and the call in `file_load`. `ctest --preset default -R "test_stable|test_file|test_watch|test_live|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: make the loop stop after one read (`tries = (attempts != 0xFFFFu) ? 1 : attempts`: a run-time-false condition on a value the call never has, never a constant); case (b) fails; put it back.
  6. D-189 into `docs/Decisions.md`; the Guide's section "At the same time" (Y-1), third paragraph, gains the sentence "The library's own reader takes the file's stamp before and after reading and reads again, up to three times by default, when it moved"; the CHANGELOG entry; the Roadmap row as decided.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-5: a stable read, a file read twice and compared (D-189)`.
  8. ACCEPT; push `work/Y-5`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_stable|test_file|test_live|test_watch"` exit 0; `bench` unchanged: the quick benchmark's file-reading cells (`scripts/bench/run.py --scope quick`) are not gated on it, and the one extra `stat` is not measured here.
* **stop if**: `cf_platform_stamp` of a file written within one clock tick cannot tell two lengths apart (it compares `size`, so it can; if case (b) is flaky, report the platform); `gate.symbols` refuses the name; an existing test of `file_load`'s error text changes.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased" (this card's version; the heading is added above the newest one when it is not there), "### Changed": "the file source reads a file only when its stamp is the same after the read as before, and reads again when it moved, up to 3 reads by default (Y-5, D-189)". `docs/Decisions.md`: D-189. `docs/Roadmap.md`: "**Reads and writes at the same time, guarded by choice.**" marked 🟩, when the cards above have landed.
* **Organiser's answers** (Y-5): the function is named `cf_file_read_stable`, not `cfi_file_read_stable`: `gate.abi` lists only `cf_` names, and an exported `cfi_` name is refused ("listed and not defined"); `cf_platform_write_file_if` is the precedent for an internal `cf_` name under `#if CF_INTERNAL_API`. The Guide section "At the same time" (Y-1) and D-187, D-188 are not in the tree yet, so the Guide sentence and the Roadmap mark wait for the card that lands last (the maintainer's standing approval, 2026-11-27).

---

## Y-6 - Windows readers that share delete access, if a measurement shows a rename then succeeds over an open reader (F-5d in the research)

* **id**: Y-6
* **goal**: on Windows, `cf_platform_fopen` opens a file with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` through `CreateFileW` and a C stream on the handle, if and only if a measurement in this card shows that `MoveFileExW` then replaces a file that a reader still has open; otherwise nothing changes and the card is closed with the measurement written down.
* **why**: `cf_platform_fopen` opens with `_wfopen` (`lib/platform/src/win32/file.c:33-43`), which does not share delete access: while a reader has the file open, F-1a's rename over it fails. F-1a's Windows retry (`cf_file_options.retry_ms`, 2 s by default: D-224) waits for the reader to close, but a reader that is slow or stuck blocks the write for the whole `retry_ms`. Opening with `FILE_SHARE_DELETE` is what lets a rename through on POSIX-like terms; whether `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` over a file opened that way succeeds is what the research could not settle from documents (`docs/research/concurrent-access.md:168-188`, and option R5, "to be measured").
* **needs**: F-1a
* **size**: small
* **version**: 1.9.0
* **status**: done (16b115d)
* **decided**:
  * **Not inheritable, always.** Y-8 opens files with `_wfopen`'s `N` mode flag; if this card replaces that call it keeps the property: `CreateFileW` with NULL security attributes is not inheritable, and `_open_osfhandle` is given `_O_RDONLY | _O_NOINHERIT`.
  * **A measurement first, and it decides.** The card runs the measurement from a throwaway C file in the scratch directory (not added to the tree), compiled with `cl` in the Visual Studio developer prompt, on Windows: `CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)`, keeping the handle open; then, from the same process, `MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; the result and `GetLastError()` are written under the card's "notes" together with the Windows build (`ver`). Also the same with `_wfopen(path, L"rb")` as the control (it must fail, `ERROR_ACCESS_DENIED` or `ERROR_SHARING_VIOLATION`; if it succeeds the control is wrong: stop).
  * **If the rename succeeds** with the share-delete handle open: `cf_platform_fopen` on Windows becomes `CreateFileW` with the three share flags and `OPEN_EXISTING` (mode `r`) or `CREATE_ALWAYS` (mode `w`) or `OPEN_ALWAYS` with `_O_APPEND` given to `_open_osfhandle` (mode `a`), `_open_osfhandle`, `_wfdopen`; the mode strings the library uses are `rb`, `wb`, `ab` (F-1a's `cf_file_writer`) and `r` (`grep -rn "cf_platform_fopen" lib tools --include=*.c | sed 's/.*cf_platform_fopen//' | sort -u | head` lists them: any other string is a "stop if"). A reader that opens a file this way no longer blocks a writer's rename. The retry of F-1a stays, as the option `retry_ms` (a reader that opened the file by other means still blocks it).
  * **If it fails**: no code. The card is closed, the measurement is under "notes", the research's R5 is written as rejected in a line of `docs/Decisions.md` under D-189 ("Windows readers that share delete access: measured, a rename over one still fails, 2026-xx-xx, build ..."), and the Roadmap is not touched.
  * **No new public name; no D-number of its own** (D-189 holds it).
* **files**: `lib/platform/src/win32/file.c` (only when the measurement says so), `lib/config/tests/test_writefile.c` (a Windows-only case (r)), `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-6 main` (after F-1a has landed). On a machine that is not Windows, stop: this card is Windows-only and is carried by the organiser's Windows machine.
  2. Measure as decided and write the numbers under "notes" in this card's section of `docs/plan/Y-asked.md`.
  3. If the rename fails: write the line of `docs/Decisions.md`, commit it as `Y-6: Windows readers that share delete access, measured and rejected`, push to `main` (documents), and stop; the card is done.
  4. If the rename succeeds, write the test first: `test_writefile.c` case (r), `#if CF_PLATFORM_WIN32`: open the target with `cf_platform_fopen(path, "rb")`, keep it open, call `cf_file_replace(path, "new", 3, NULL, &err)`, expect 1 at once (not after the retry bound, `retry_ms` 2000 by default: measure the call and fail over 500 ms), close the reader, expect the file to read `new`.
  5. See it fail: `ctest --preset default -R test_writefile` on Windows fails case (r) (the write waits out the bound and fails, or takes more than 500 ms).
  6. Change `cf_platform_fopen` as decided; `ctest --preset default -R "test_writefile|test_file|test_serialize"` passes.
  7. See that the test can fail: change the share flags back to `FILE_SHARE_READ` behind a run-time-false condition (`if (mode[0] == 'z')`); case (r) fails; put it back.
  8. `python scripts/work/work.py format`; codemap last. Commit `Y-6: cf_platform_fopen shares delete access on Windows`.
  9. ACCEPT; push `work/Y-6`; ACCEPT-CI.
* **accept**: ACCEPT-WINDOWS with `test_writefile` case (r) when the measurement says yes, and the measurement under "notes" in both cases; ACCEPT-LINUX and ACCEPT-CI when code changed (the change is Windows-only, and both must still be green); `ctest --preset default -R "test_writefile|test_file"` exit 0.
* **stop if**: the control (`_wfopen`) does not fail; a mode string other than `rb`, `wb`, `ab` or `r` is passed to `cf_platform_fopen` anywhere; a path that `win32f_wide` accepts is not accepted by `CreateFileW` (a long path with no `\\?\` prefix: report); the measurement is not available because the machine is not Windows.
* **docs**: when the code changed: `CHANGELOG.md`, "## 1.9.0 — unreleased" (this card's version), "### Changed": "on Windows a file the library reads is opened with delete sharing, so a writer's rename over it no longer waits for the reader (Y-6)". When it did not: only the line in `docs/Decisions.md`. No Roadmap row.
* **result**: closed by measurement (rename fails; no code).
* **notes**: measured 2026-11-27 on Windows `10.0.26300.10075` (`ver`), `cl` of Visual Studio 18, a throwaway C file, files on D:. Reader opened with `CreateFileW(GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, OPEN_EXISTING)` and kept open, same process: `MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` returned 0, `GetLastError()` = 5 (`ERROR_ACCESS_DENIED`). Control `_wfopen(path, L"rb")` open: 0, error 5 (it fails, as required). No reader open: the rename succeeds. So R5 is rejected; D-189 line in `docs/Decisions.md`.

---

## Y-7 - "Threads and processes" in the Guide: four recipes, compiled in CI, a `fork` test

* **id**: Y-7
* **goal**: `docs/Guide.md` has a section "Threads and processes made easier" with four recipes that work on today's interface, each an example under `examples/c/` that CI builds and runs, and the `fork` one has a test that a child reads a snapshot taken before it.
* **why**: threads are served by D-14 and `cf_live`, but nothing shows a program how to share a configuration with request handlers, reload on `SIGHUP`, `fork` after building, or hand an image to a child (`docs/research/threads-and-processes.md:539-556`, "First", point 1); the rules for `fork` and signals are written nowhere (its section 3, items 7-9 and 13). The Roadmap item is "Threads and processes made easier" (`docs/Roadmap.md`, by its words).
* **needs**: Y-1 (its "At the same time" is the section this one follows), Y-8 (recipe 4 hands a file to an `exec`ed child that must not leak descriptors)
* **size**: medium (four small examples, one test, one Guide section)
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Where.** A new `## 15. Threads and processes` after Y-1's section of `docs/Guide.md` (section 14 is the file's last: `grep -n "^## 14. Threads" docs/Guide.md`); a line for it in the contents list at line 24 in the form the other entries have.
  * **The four recipes**, each a C file under `examples/c/`, C89 except for the calls named (POSIX headers under `#if defined(__unix__) || defined(__APPLE__)`; on other targets the file prints `skipped: no POSIX` and exits 0 and the CMake registration is `if(UNIX)` only):
    1. `examples/c/threads_requests.c`: a `build` function as `examples/c/live.c:33-36` has it (a memory source with `server.port` and `log.level`), `cf_live_open(CF_LIVE_MANUAL, build, NULL, NULL, NULL, &err)`; 4 pthreads each loop 1000 times `cfg = cf_live_get(live); read cf_get_int(cfg, "server.port", 0); cf_config_free(cfg);` ("one request, one snapshot"); the main thread calls `cf_live_reload` 20 times between `sched_yield()`s with a build that returns a different port each time; after joining it prints `readers saw only whole snapshots` when every port read was one the build ever returned, else exits 1. Linked with `Threads::Threads` as `test_threads` is (`examples/c/CMakeLists.txt`'s `find_package(Threads QUIET)` as `lib/config/tests/CMakeLists.txt:289-298` does).
    2. `examples/c/sighup_reload.c`: `volatile sig_atomic_t flag`; the handler (`signal(SIGHUP, h)`) only sets it; the loop checks the flag, clears it and calls `cf_live_reload(live, &err)`, prints the result (`swapped` or `unchanged`); the example sends itself `SIGHUP` with `raise(SIGHUP)` after rewriting its file, so it runs without a second process, and ends by printing `reload ran outside the handler`. The page says why: `cf_live_reload` allocates, opens files and takes a spin lock, none of which is async-signal-safe (the research's item 13).
    3. `examples/c/fork_child.c`: build a configuration, `fork()`, the child reads three keys and exits 0 when they equal the parent's, the parent `waitpid`s and prints `child read the parent's snapshot`. The page says what the child may not touch: the parent's `cf_live` (its lock may be held by a thread the child does not have, and on Linux its watcher descriptor is shared with the parent), and that the child opens its own `cf_live` if it reloads at all; and that `cf_config` reference counts written by children copy one page each.
    4. `examples/c/image_child.c`: the parent writes `cf_image_write(cf_keys(c), cf_sources(c), ...)` (`cf_image.h:54`) to a file made by `cf_platform_create_private` (internal tier: the example uses `fopen "wb"` and `chmod(path, 0600)` instead and the page says the library's own is private from the first byte), runs itself with `system()` as `argv[0] --child <path>`, and the child builds with the same definitions and `cf_source_image_file` (`cf_image.h:112`), reads the keys and exits 0 when they match; the parent prints `child read the image`. The page says the child's own definitions decide `CF_RESTART` and secret flags (the image carries values, not the contract) and that a file is a copy, not shared pages.
  * **Registration.** In `examples/c/CMakeLists.txt`, each with `add_executable` and `example_expect(<name> <dir> "<the line it prints>")` as the file's other examples (`examples/CMakeLists.txt:9-20` documents `example_expect`); the `fork_child` and `image_child` ones also run under the `sanitize` preset's ctest as every `example.*` does.
  * **The `fork` test.** `lib/config/tests/test_fork.c`, `#if defined(__unix__) || defined(__APPLE__)`, C89 plus `fork`, `waitpid`: build, take a snapshot, `fork`; the child does 1000 `cf_get_int` of each key and `_exit(0)` when all equal the parent's, `_exit(1)` otherwise; the parent also reads while the child runs; the parent checks `WIFEXITED` and status 0. Also: with a `cf_live` open in the parent, the child calls `cf_live_get(live)` once (it works: no thread held the lock at the `fork`) and `_exit`s, to show the one safe thing; the test does not call anything else on it. On other targets the test is `return 0` after printing `skipped`.
  * **No decision, no new interface.** The page says the rules, D-14, D-35, D-48 and D-03 decided them.
* **files**: `examples/c/threads_requests.c`, `examples/c/sighup_reload.c`, `examples/c/fork_child.c`, `examples/c/image_child.c` (new), `examples/c/CMakeLists.txt`, `lib/config/tests/test_fork.c` (new), `lib/config/tests/CMakeLists.txt`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md` (by Y-12's or the last card's mark, see docs); last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-7 main` (after Y-1 and Y-8 have landed).
  2. Write `test_fork.c` and add `test_fork` to the list in `lib/config/tests/CMakeLists.txt` (guarded for POSIX as `test_threads` is guarded at lines 289-298).
  3. See it fail: `cmake --build --preset default --target test_fork 2>&1 | tail -n 15` fails (no file yet is the failure; so first add the CMake line, then the test fails to compile on its missing source). Then write the file with the child's check made wrong on purpose (`_exit(0)` replaced by `_exit(2)`); `ctest --preset default -R test_fork` fails; put it right and see it pass.
  4. Write the four examples and their registration; `ctest --preset default -R "example\.(threads_requests|sighup_reload|fork_child|image_child)"` passes.
  5. Write the Guide section: the four recipes (each: the problem in one paragraph, the code's key lines quoted from the example, what stays the program's), then a closing list "What the library does not do": start threads or processes, handle signals, pick the moment a worker takes a new snapshot, restart workers for a `CF_RESTART` key, carry a descriptor to a child.
  6. `python scripts/work/work.py format`; codemap last. Commit `Y-7: the Guide's threads and processes, four recipes in CI`.
  7. ACCEPT; push `work/Y-7`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the examples and the test print `skipped` and exit 0; the build is the check); ACCEPT-CI (macOS included: `fork` and `SIGHUP` run there); `ctest --preset default -R "test_fork|example"` exit 0; `ctest --preset tsan -R example.threads_requests` exit 0.
* **stop if**: Y-1 or Y-8 has not landed; `cf_image_write`'s arguments are not `cf_keys(c)` and `cf_sources(c)` as the research says (read `cf_image.h:54-60` and `examples/c/` first and report the real call); `example.threads_requests` is flaky under TSan (that is a finding about D-14: report, change nothing); the `sanitize` preset's leak check fails the `fork` example in the child (report which allocation).
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "The Guide's \"Threads and processes\": four recipes (request handlers, a `SIGHUP` reload, `fork` after building, an image in a private file for a child), each an example run in CI, and a `fork` test (Y-7)". No D-number. Roadmap: marked by Y-12 (the last card of the item's research), see there.

---

## Y-8 - Close-on-exec and non-inheritable everywhere the library opens a file, with a child that lists its descriptors

* **id**: Y-8
* **goal**: every descriptor or handle the library keeps open while it runs (a file opened by `cf_platform_fopen`, F-1a's writer and temporary, Y-3's sidecar lock file, the watcher's `inotify` or `kqueue` descriptor and the files it watches, a private file being created) is close-on-exec on POSIX and not inheritable on Windows, and a test `exec`s a child that lists its open descriptors and finds none of them.
* **why**: a child that another thread starts while the library has a file open inherits the descriptor: `cf_platform_fopen` calls `fopen` with the caller's mode and no close-on-exec flag in `cf_platform_fopen` (`lib/platform/src/posix/file.c:39`) and `_wfopen` with no no-inherit flag (`lib/platform/src/win32/file.c:45`), and `cf_platform_create_private` opens with no close-on-exec flag (`lib/platform/src/posix/posix.c:670`); the `kqueue()` descriptor is made without it too (`posix.c:1364`) (`docs/research/threads-and-processes.md:512-523`, item 11 of its section 3). A secret file read by a library should not reach a stranger's child.
* **needs**: F-1a, Y-3 (the test also holds a `cf_file_writer` and a sidecar lock open; their opens are close-on-exec from the start, and this card pins that)
* **size**: small
* **version**: 1.9.0
* **status**: done (62812e1)
* **decided**:
  * **POSIX `cf_platform_fopen`** (`posix/file.c`, C89 plus `open`, `fdopen`, `fcntl`): `fopen(path, mode)` becomes `open` then `fdopen`. The mode string maps as C says: first letter `r` is `O_RDONLY`, `w` is `O_WRONLY | O_CREAT | O_TRUNC`, `a` is `O_WRONLY | O_CREAT | O_APPEND`; a `+` makes it `O_RDWR`; `b` is ignored; any other first letter returns NULL. The permission is `0666` (the umask applies, as `fopen` gave). Flags carry `O_CLOEXEC` where it is defined; where it is not, `fcntl(fd, F_SETFD, FD_CLOEXEC)` right after the `open` (a window of two calls remains and the file's comment says so). `fdopen` failing closes `fd` and returns NULL. Only `"rb"` is passed anywhere today (`grep -rn "cf_platform_fopen" lib tools --include=*.c`); a caller found with another mode string is handled by the mapping above and is not a stop.
  * **`none/file.c`** is plain `fopen` and stays (no descriptors the library can mark).
  * **Windows `cf_platform_fopen`**: the mode string gets `N` appended (`_wfopen(wpath, wmode)` with `wmode` one character longer: the `wchar_t wmode[8]` already has room; refuse a mode longer than 6 characters); the C runtime then opens the file `_O_NOINHERIT`. Y-6 may later replace this call; its card keeps the property.
  * **`cf_platform_create_private`** (POSIX, `posix.c:670`): `open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600)`, with `O_CLOEXEC` under `#ifdef O_CLOEXEC` as `posix.c:1203-1205` does, and `fcntl(fd, F_SETFD, FD_CLOEXEC)` otherwise. The Windows twin already passes `bInheritHandle = FALSE` (`win32.c:213`): no change.
  * **The `kqueue` descriptor** (`posix.c:1364`): after `kqueue()` succeeds, `fcntl(w->fd, F_SETFD, FD_CLOEXEC)`; a failure closes it and fails the watcher as `kqueue` failing does. (`kqueue` has no flags argument on macOS; FreeBSD's `kqueue1` is not used so that one path serves both.) The files it watches are already `O_CLOEXEC` (`posix.c:1203-1205`) and the `inotify` descriptor is `IN_CLOEXEC` (`posix.c:1022`): no change. The Windows watcher's directory handle and event are made with NULL security attributes (`win32.c:420-424`, `520`), which is not inheritable: no change.
  * **Directory scans** (`opendir`, `posix.c:118`, `593`, `1081`): not changed. The C library's `opendir` marks its descriptor close-on-exec on glibc, musl and the BSDs, and the descriptor lives inside one scan that closes it before returning; the page says so. The test cannot observe it (a scan is too short to list from a child) and D-190 says "held by the C library".
  * **The test.** `lib/config/tests/test_cloexec.c`, `"cf_test.h"`, C89, run in the build directory, one binary that is its own child: with `argc >= 3 && strcmp(argv[1], "--fds") == 0` it lists, into the file `argv[2]`, the numbers of the descriptors 3 to 255 that are open (`fcntl(fd, F_GETFD) != -1` on POSIX; on Windows `_get_osfhandle(fd) != -1` for 3 to 255 after `_set_invalid_parameter_handler` with an empty handler and `_CrtSetReportMode(_CRT_ASSERT, 0)`), one per line, then returns 0. The parent: opens `cf_cx.txt` with `cf_platform_fopen(..., "rb")` and keeps it open; builds a `cf_config` with a file source and opens `cf_watcher_open` on it (`configforge.h:827-865`; skip the watcher case with a printed line when `cf_watcher_open` returns NULL, `CF_E_CAPABILITY`); records the numbers of those descriptors (`fileno` of the first; `cf_watcher_fd` of the second); lists its own open descriptors with the same function the child uses (a helper `list_fds(file)` the test calls in the parent and in the child), opens `cf_file_writer_open("cf_cx_w.txt", NULL, &err)` and takes `cf_lock_sidecar()` on `cf_cx.txt` (Y-3), lists again, and records the new numbers as well; runs the child (POSIX: `system("\"<argv[0]>\" --fds cf_fds.txt")`; Windows: `_spawnl(_P_WAIT, argv[0], argv[0], "--fds", "cf_fds.txt", NULL)`, which passes C-runtime descriptors that are not `N`); reads `cf_fds.txt` and checks that none of the recorded numbers is in it. A control: a descriptor the test itself opens with plain `fopen` (POSIX; Windows `_wfopen` with no `N`) and `fileno`s IS in the list (so the child's list works; when the control is not found the test fails with `the child's list is empty: the control descriptor was not seen`). `cf_platform_create_private` is exercised by a call with a new file name and checked by reading the source (`grep -n "O_CLOEXEC" lib/platform/src/posix/posix.c` shows the line): the descriptor is closed before the function returns, so no child can see it.
  * **D-190** (copy into `docs/Decisions.md` after D-189, titled "Every descriptor the library keeps is close-on-exec"): Question: a child started by another thread while the library has a file open inherits it. Decision: the four changes above, and the opens of F-1a's writer and temporary and of Y-3's sidecar, which are born close-on-exec; the directory scan held by the C library; the test with a control, which also holds a writer and a lock open. Rejected: `fopen` with `e` (not in the macOS C library until recently, and a mode string the C standard does not know); `O_CLOEXEC` alone where `fcntl` would do (a platform without the flag gets the two-call form and a comment, not a refusal). Held to: `test_cloexec`.
* **files**: `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/posix/posix.c`, `lib/config/tests/test_cloexec.c` (new), `lib/config/tests/CMakeLists.txt` (after `test_lock` or `test_writefile`, whichever is last), `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-8 main`.
  2. Write `test_cloexec.c` and add it to the list.
  3. See it fail: `cmake --build --preset default --target test_cloexec && ctest --preset default -R test_cloexec 2>&1 | tail -n 15`: the library's own file descriptor is listed by the child (the test fails on the recorded number, and the control passes).
  4. Make the POSIX and Windows changes. `ctest --preset default -R "test_cloexec|test_file|test_watch|gate" 2>&1 | tail -n 25`.
  5. See that the test can fail: in `posix/file.c` drop `O_CLOEXEC` behind a run-time-false condition (`if (mode[0] == 'z')` selects the old flags, never a constant); `test_cloexec` fails; put it back.
  6. D-190 into `docs/Decisions.md`; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; codemap last. Commit `Y-8: close-on-exec everywhere the library opens a file (D-190)`.
  8. ACCEPT; push `work/Y-8`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI (macOS and FreeBSD jobs included: the `kqueue` change runs there); `ctest --preset default -R "test_cloexec|test_file|test_watch"` exit 0.
* **stop if**: a callers' mode string maps to no `open` flags and the file then opens differently from `fopen` (a case in `test_file` fails: report which string); `cf_watcher_fd` is -1 on Linux (the test then has nothing to check: report); `gate.layering` refuses `<fcntl.h>` in `posix/file.c`; the Windows child cannot be started by `_spawnl` in the test environment.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Fixed": "files and descriptors the library opens are close-on-exec (POSIX) and not inheritable (Windows), so a child started while one is open does not receive it; the test lists a child's descriptors (Y-8, D-190)". `docs/Decisions.md`: D-190. Roadmap: marked by Y-12.

---

## Y-9 - A second `cf_live` driver refused, and D-48's sentence checked

* **id**: Y-9
* **goal**: while one thread is inside a driving call of a `cf_live` (`cf_live_poll`, `_reload`, `_wait`, `_approve`, `_reject`, `_rollback`, `_confirm`, `_commit`), a second driving call from another thread (or from inside a callback on the same thread) returns `CF_LIVE_FAILED` (`CF_FALSE`) with `CF_E_POLICY` and a message that names the call, and changes nothing; the first call is not affected.
* **why**: `cf_live.h:69-73` says every call but `cf_live_get` belongs to the one thread that drives the reloads, and nothing detects a second driver: two threads that call `cf_live_poll` or `cf_live_reload` at once corrupt `pending`, `kept` and `rejected` (`lib/config/src/core/live.c:51-60`, "the driving thread's alone"). One atomic test-and-set makes the mistake a refusal instead of memory corruption (`docs/research/concurrent-access.md:420-431`, option T2; `docs/research/threads-and-processes.md:512-523`). This is the card the first research calls "a guard on `cf_live`'s driving calls". D-48's stale sentence was fixed by Y-1; this card only checks it.
* **needs**: Y-1
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **No new public name and no new enum value.** The refusal is `CF_LIVE_FAILED` with the error `CF_E_POLICY` (a program using the library against its contract: "policy conflict", `cf_code.def:30`); the research suggested a new `cf_live_result` value, and that would be a public addition for nothing the existing failure cannot say. The message: `another thread is driving this cf_live: %s was called while it ran; its calls belong to one thread at a time`, `%s` the call's name (`cf_live_poll`, ...). `cf_live_reject`, which returns void, logs the same text as an error event `driver` (`live_log(l, CF_LOG_ERROR, "driver", CF_E_POLICY, NULL, msg)`, `live.c:866`'s use of `live_log` shows the call) and does nothing; `cf_live_confirm` returns `CF_FALSE`; `cf_live_commit` aborts the edit first (`cf_edit_abort(e)`, as its NULL case does at `live.c:863-865`) and returns `CF_LIVE_FAILED`.
  * **The guard.** A `cf_refcount driving;` in `struct cf_live` (after `settling`, `live.c:75`), `cf_ref_init(&l->driving, 0)` beside the ticket counters' (`live.c:706-707`). Two static functions: `live_enter(l, what, err)`: `if (cf_ref_inc(&l->driving) != 1) { cf_ref_dec(&l->driving); cf_error_set(err, CF_E_POLICY, NULL, NULL, "...", what); return CF_FALSE; } return CF_TRUE;` and `live_leave(l)`: `(void)cf_ref_dec(&l->driving);`. The port has `cf_ref_inc` (returns the new value, as `live_lock`'s `cf_ref_inc(...) - 1` shows), `cf_ref_dec` and `cf_ref_get` and no compare-and-swap (`cf_port.h:357-360`); an increment that finds more than 1 is backed out, so of any set of simultaneous callers exactly the one that saw 1 proceeds, and the others are refused (never all refused: the first increment always sees 1 unless a call is already in).
  * **Structure.** Each driving function's body moves, unchanged, into a static function (`live_poll_body`, `live_reload_body`, `live_approve_body`, `live_reject_body`, `live_rollback_body`, `live_confirm_body`) and the public function is `enter; body; leave`. Inner calls go to the bodies: `cf_live_wait` is `enter`, the watcher wait, `live_poll_body`, `leave` (so the guard is held while it sleeps, which is right: a second driver during the wait is refused); `cf_live_commit` is `enter`, `cf_edit_commit`, `live_reload_body`, `leave`. `cf_live_get`, `cf_live_pending`, `cf_live_status_get`, `cf_live_last_error`, `cf_live_generation`, `cf_live_restarts`, `cf_live_open*` and `cf_live_close` are not guarded (`get`, `status_get`, `last_error` and `generation` are for any thread; `close` is the program's to order, D-14).
  * **A callback that drives is refused too.** `build`, `check`, `health`, `log` are called on the driving thread inside a driving call (`cf_live.h:131-162`'s comments; D-35); one that calls a driving call on its own `cf_live` finds the guard taken and is refused. That is the test's way to hold two drivers at once without a second thread, and it is the documented contract anyway.
  * **`cf_live.h:69-73`** (its closing comment) gains: "A second one at the same moment is refused with `CF_LIVE_FAILED` and `CF_E_POLICY`, and changes nothing." **D-48**: its sequel at `docs/Decisions.md:2188-2200` gains the sentence "A driving call made while another runs is refused (D-191)." and the text Y-1 corrected is checked: `grep -c "ticket lock" docs/Decisions.md` finds it.
  * **D-191** (copy into `docs/Decisions.md` after D-190, titled "A second driver of a cf_live is refused"): Question: two threads calling the reload calls at once is outside the contract and nothing says so at run time. Options: nothing; a refusal; a lock that makes them wait. Decision: a refusal by one atomic increment, no new name, the guard held across a wait. Rejected: a lock that waits (only a spin is available in C89, and a driver that waits is a program that has two drivers: it should know); a new `cf_live_result` value; guarding `cf_live_get` (D-14: reads are free). Held to: `test_live`'s new case.
* **files**: `lib/config/src/core/live.c`, `lib/config/include/configforge/cf_live.h`, `lib/config/tests/test_live.c`, `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-9 main` (after Y-1 has landed: `grep -c "ticket lock" docs/Decisions.md` is at least 1).
  2. In `lib/config/tests/test_live.c`, near the thread tests at 346-380, add `test_second_driver`: a `build` function that, when the user struct's `live` is not NULL and its `calls` counter is 0, sets `calls = 1` and calls each of `cf_live_poll`, `cf_live_reload`, `cf_live_wait(live, 0, ...)`, `cf_live_approve`, `cf_live_rollback(live, "x", ...)`, `cf_live_commit(live, e, "note", &err)` with a `cf_edit` the test begins before the reload (`cf_edit_begin`, `lib/config/include/configforge/cf_edit.h`; the refused commit aborts it), `cf_live_confirm` on the same `live` and records each result and `err.code`/message; the test then calls `cf_live_reload(live, &err)` from the main thread, which runs `build` once (the nested calls happen inside it). Checks: every nested driving call returned `CF_LIVE_FAILED` (`confirm`: `CF_FALSE`), `err.code` is `CF_E_POLICY`, the message contains `another thread is driving` and the call's name; the outer reload returned `CF_LIVE_SWAPPED` or `UNCHANGED` as it would without the nested calls (the build returns a changed value so it is `SWAPPED`); afterwards (guard free) `cf_live_poll` works again and returns without `CF_E_POLICY`; `cf_live_get`, `cf_live_generation` and `cf_live_status_get` called inside the build (the same way) are not refused. Under pthreads (`#if` as the file's other thread tests): a thread blocks inside `check` on a flag while the main thread calls `cf_live_reload`: refused; the flag is released and the thread finishes.
  3. See it fail: `cmake --build --preset default --target test_live && ctest --preset default -R test_live 2>&1 | tail -n 15`: the nested calls do not return `CF_LIVE_FAILED` (they run, and some may crash or return success).
  4. Add the field, the two functions, the bodies and the public wrappers; the two comment edits. `ctest --preset default -R "test_live|test_watch|test_derive|gate" 2>&1 | tail -n 25`; `ctest --preset tsan -R test_live` too.
  5. See that the test can fail: make `live_enter` always succeed behind a run-time-false condition (`if (what == NULL)` selecting the old path, never a constant); the case fails; put it back.
  6. D-191 into `docs/Decisions.md` with the D-48 sentence; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; codemap last. Commit `Y-9: a second driver of a cf_live is refused (D-191)`.
  8. ACCEPT; push `work/Y-9`; ACCEPT-CI (the `tsan` job included).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live` exit 0 and `ctest --preset tsan -R test_live` exit 0 on Linux.
* **stop if**: Y-1 has not landed; an existing caller (a test, an example, a binding, `tools/`) calls a driving call from inside one of its own `build`, `check`, `health` or `log` callbacks and the suite fails because of the guard (report which: it is a use outside the contract, but the owner decides); a binding wraps `cf_live_*` and its test checks that a second thread can drive; `cf_ref_inc` does not return the new value on some port (read `cf_port.h` and the `CF_SINGLE_THREADED` fallback first).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Changed": "a second thread driving a `cf_live` at the same time is refused with `CF_LIVE_FAILED` and `CF_E_POLICY` instead of corrupting its pending state; one atomic test-and-set per driving call (Y-9, D-191)". `docs/Decisions.md`: D-191. Roadmap: marked by Y-12.

---

## Y-10 - `cf_live_get` and `cf_config_free` under threads on the benchmark's page; `cf_live_generation` without the lock

* **id**: Y-10
* **goal**: `cf_bench` times the `cf_live_get` plus `cf_config_free` pair on 1, 2, 4 and so on up to twice the machine's cores, and `docs/Performance.md` shows the numbers in a part "Readers under threads"; and `cf_live_generation` reads an atomic instead of taking the lock.
* **why**: the read path takes three shared writes on `get` and one on free, under a ticket lock with no pause and no yield (`lib/config/src/core/live.c:86-93`), and nobody has measured it under threads: the benchmark times a reload (`bench/bench.c:287-317`), not `cf_live_get` under contention (`docs/research/threads-and-processes.md:313-337`, items 2 and 3). A worker that asks "has it changed since I looked?" through `cf_live_generation` pays the same lock as a `get` (`live.c:910-920`). The numbers decide whether the reader handle of Y-11 is built.
* **needs**: nothing
* **size**: medium (a benchmark with threads, a page part, one small change in `live.c`)
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Decided 1 (a new public interface for the read path): (b)**, A5 only: `cf_live_generation` lock-free, no new name, as part of this measurement card. The reader handle (c) is Y-11's, after this card's numbers. **Rejected:** (a) none (a worker asking "has it changed?" keeps paying the lock for nothing); (c) `cf_live_reader` here (a borrowed pointer, which D-03 weighed: Y-11 decides it on the page's numbers).
  * **The measurement.** A new cell in `bench/bench.c` beside `run_reload`: `run_live_get(threads)`. It opens a `cf_live` (as `run_reload`, `bench.c:293-300`) on a small configuration, starts `threads` threads (pthreads where `<pthread.h>` exists, `_beginthreadex` on Windows, behind one small shim in the file; a build with neither prints `live-get: skipped (no threads)`) that each do 1,000,000 times `cfg = cf_live_get(live); cf_config_free(cfg);` after a start barrier, and reports, per `threads` in `1, 2, 4, ...` up to twice the cores (`sysconf(_SC_NPROCESSORS_ONLN)`, `GetSystemInfo`), the pairs per second for all threads together and the nanoseconds per pair per thread, in the line format `run_reload` uses (label `live-get/<threads>`). Also the control, a thread count of 1 with `cf_config_ref`/`cf_config_free` alone (no lock), label `ref/1`, so the lock's own cost shows. A knee is where the total pairs per second at 2 threads is under 1.2 times that at 1, or falls from one count to the next; the page says whether one appears.
  * **The page.** `docs/Performance.md` gains "Readers under threads" with a table of the cells and one paragraph: the machine (cores, OS, compiler), the numbers, and "no knee, so a reader handle waits" or "a knee at N, so Y-11 is the next card". The page's static block is generated and gated (`cmake/gates/GatePerfpage.cmake`): this part is hand-written below it and must not be inside the generated block; run `ctest --preset default -R gate.perfpage` after writing it and follow its message. The numbers come from one Linux run in WSL from a copy off `/mnt` and are not gated (bench-gate gates instruction counts of `--scope quick` cells; this cell is wall-clock and is not added to it).
  * **The lock-free generation.** A `cf_refcount gen32;` in `struct cf_live`, initialised beside the ticket counters (`live.c:706-707`) and incremented in `live_swap` next to `l->generation += quiet ? 0u : 1u;` (`live.c:331`) with the same condition and still under the lock (so `gen32` equals `generation` modulo 2^32). `cf_live_generation` returns `(cf_u64)(cf_u32)cf_ref_get(&l->gen32)` and takes no lock. The `cf_u64 generation` stays for `cf_live_status_get`. The header comment of `cf_live_generation` (`cf_live.h:234`) says: "reads an atomic and takes no lock; compare it for equality: it counts swaps modulo 2^32". Rejected: a 64-bit atomic (the port has `i32` ones); changing the status field.
  * **D-192** (after D-191, "The read path is measured before it is changed"): Question: is the lock on `cf_live_get` a cost, and what does a worker pay to ask for the generation. Decision: the measurement is on the page; `cf_live_generation` is lock-free; `cf_live_get` is unchanged; a reader handle is Y-11 and waits for the page's numbers or a binding's request. Rejected: hazard pointers and liburcu (the research's A3 and A4: the same handle as Y-11 at more code, or an LGPL dependency that fails `minimal` and `misra`); a pause or yield in the spin (not measured to matter; the page's numbers decide). Held to: `test_live`'s new generation case and the page.
* **files**: `bench/bench.c`, `docs/Performance.md`, `lib/config/src/core/live.c` and `lib/config/include/configforge/cf_live.h` (step 5), `lib/config/tests/test_live.c`, `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-10 main`.
  2. In `test_live.c` add `test_generation_lockfree`: after `cf_live_open` the generation is 1 or what `cf_live_generation` returned before; a reload that changes a value makes it one more; a reload that finds the same values leaves it; a `CF_LIVE_APPROVE` live: pending leaves it, `cf_live_approve` adds one; a rollback leaves it (quiet swap); and four pthread readers (`#if` as the file's other thread tests) call `cf_live_generation` 100000 times each during 20 reloads and every value they see is at least the one seen before it by the same thread (monotonic). It passes before the change (it checks behaviour, not the lock): it is the oracle.
  3. Add `run_live_get` and the `ref/1` control to `bench/bench.c`, and the command-line way the other cells are asked for (`grep -n "run_reload" bench/bench.c` shows how it is selected; use the same, label `live-get`). Build `cf_bench` (`cmake --build --preset default --target cf_bench`) and run it: `build/bench/cf_bench live-get 2>&1 | tail -n 20` prints one line per thread count.
  4. See the cell is real: run it with `threads` forced to 1 and then to the largest, and see the two totals differ (a cell that prints equal numbers for both did not run its threads: fix before going on). Write "Readers under threads" into `docs/Performance.md` from one Linux run; `ctest --preset default -R gate.perfpage`.
  5. Add `gen32`, change `cf_live_generation`, the header comment. `ctest --preset default -R "test_live|gate" 2>&1 | tail -n 20`; `ctest --preset tsan -R test_live`.
  6. See that the test can fail: make `live_swap` skip the `gen32` increment behind a run-time-false condition (`if (quiet == 0xFFu)`, never a constant); `test_generation_lockfree` fails; put it back.
  7. D-192 into `docs/Decisions.md`; the CHANGELOG entry.
  8. `python scripts/work/work.py format`; codemap last. Commit `Y-10: cf_live_get under threads on the page; cf_live_generation without the lock (D-192)`.
  9. ACCEPT; push `work/Y-10`; ACCEPT-CI (the `bench-gate` job included: it must not move).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI, bench-gate and `tsan` included; `ctest --preset default -R "test_live|gate.perfpage"` exit 0; `docs/Performance.md` holds the part with numbers from a real run.
* **stop if**: the bench's thread shim cannot be written without a new dependency (a build without pthreads prints the skip: that is not a stop); bench-gate moves on a cell (the card changed something it must not: revert and report); the machine has one core (the table has one row: write it, and say the knee cannot be seen).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "the benchmark times `cf_live_get` under threads, and the page shows readers under 1 to twice-the-cores threads (Y-10)"; and "### Changed": "`cf_live_generation` reads an atomic and takes no lock (Y-10, D-192)". `docs/Decisions.md`: D-192. Roadmap: marked by Y-12.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.
  * 2026-11-27, the organiser: question 1 is (b), the card's recommendation: `cf_live_generation` lock-free, no new name; the reader handle stays Y-11's (the maintainer's standing approval, 2026-11-27).

## Y-11 - `cf_live_reader`: a worker's handle that caches a snapshot, its test under ThreadSanitizer, C++ and Rust wrappers

* **id**: Y-11
* **goal**: `cf_live_reader_open`, `_get`, `_generation`, `_release` and `_close` give a worker one borrowed snapshot that costs one shared read per call when nothing was swapped, with a test under ThreadSanitizer and wrappers in C++ and Rust.
* **why**: `cf_live_get` writes three shared counters and the snapshot's count on every call, and a shared write does not scale with cores (`docs/research/threads-and-processes.md:413-450`, option A2; arc-swap's `Cache`, Envoy's slot). Y-10 measures whether this matters; this card is for the case where it does or a binding asks.
* **needs**: Y-10 (its numbers and its lock-free generation), Y-9
* **size**: medium (about 120 lines of C, two wrappers, a test)
* **version**: 2.7.0
* **status**: open
* **decided**:
  * **Decided 1c (a new public interface for the read path): (c).** `cf_live_reader` as sketched below, with Y-10's lock-free generation inside it; it is a borrowed pointer, bounded by the worker's own next call, which D-03 did not weigh and which is accepted (2026-11-26). `cf_live_get` stays the safe default, unchanged. The card is carried out when Y-10's page shows a knee with the number of cores or a binding asks for it; until then Y-10's generation read without the lock is all that exists. **Rejected**: (a) none, Y-10's measurement alone; (b) the lock-free generation alone as the end (it is the first stage, not the end).
  * **The interface** (`lib/config/include/configforge/cf_live.h`, after `cf_live_generation`, line 234; names and comments as the research sketches them):
    ```c
    typedef struct cf_live_reader cf_live_reader;
    /* A worker's handle on `l`: open once, keep, close before cf_live_close.
       One thread at a time per reader; any number of readers. */
    CF_NODISCARD CF_API cf_live_reader* cf_live_reader_open(cf_live* l, cf_error* err);
    CF_API void cf_live_reader_close(cf_live_reader* r);
    /* The current snapshot, borrowed: valid until the next call on this reader,
       cf_live_reader_release or cf_live_reader_close. When no swap happened since
       the last call, no shared memory is written. */
    CF_API const struct cf_config* cf_live_reader_get(cf_live_reader* r);
    /* The generation of the snapshot the reader holds (0 before the first get). */
    CF_API cf_u64 cf_live_reader_generation(const cf_live_reader* r);
    /* Drop the held snapshot, for a worker going idle. */
    CF_API void cf_live_reader_release(cf_live_reader* r);
    ```
    `cf_live_reader_open` with a NULL `l` is `CF_E_CAPABILITY`, `no cf_live to read`; with no memory `CF_E_MEMORY`. `get` on a NULL reader returns NULL. A reader is allocated from the live's allocator (`l->a`).
  * **The epoch.** `struct cf_live` gains `cf_refcount epoch;` (init beside the ticket counters, `live.c:706-707`), incremented in `live_swap` on every pointer change, quiet ones (rollback) included, under the lock (`live.c:326-335`'s section). A static `live_get_epoch(l, &epoch, &gen)` does `cf_live_get`'s work (`live.c:740-751`) and also reads `epoch` and `generation` under the same lock, so the pointer, the count taken, the epoch and the generation are one moment. `struct cf_live_reader { cf_live* l; cf_config* held; cf_i32 epoch; cf_u64 gen; }`. `get`: `e = cf_ref_get(&l->epoch)`; if `held != NULL && e == r->epoch` return `held` (one atomic load, no write); else `live_get_epoch(l, &r->epoch, &r->gen)` into `next`, `cf_config_free(r->held)`, `r->held = next`, return it. `release` frees `held` and sets it NULL; `close` releases and frees the reader. The epoch is 32 bits compared for equality; a reader would have to miss exactly 2^32 swaps to be fooled (the page says so).
  * **The cost, said.** An idle reader pins the snapshot it last saw (item 5 of the research's section 3): the header comment of `cf_live_reader_release` says to call it when a worker goes idle. A reader outliving `cf_live_close` is a use after free of the live: the comment says close readers first (as D-14 says for configurations: the live owns the lock, not the snapshot).
  * **C++** (`lib/cxx/include/configforge/configforge.hpp`, beside `cf::Live`, line 1493 region): `class cf::LiveReader`, non-copyable, movable, constructed by `Live::reader()`, `const Config& get()`-like accessor is not possible (a borrowed C pointer): `const cf_config* get()` is returned wrapped in the existing non-owning view type if one exists (`grep -n "class ConfigView\|struct ConfigView\|class Ref" lib/cxx/include/configforge/configforge.hpp`), else as `const cf_config*` and the comment says it is borrowed. **Rust** (`bindings/rust/src/live.rs`, beside `Live::reader()` at `live.rs:14`'s doc): `pub struct LocalReader` that is `Send` and not `Sync` (a `PhantomData<*const ()>` makes it `!Sync` and an `unsafe impl Send`), `get(&mut self) -> &Config` borrowed from the `&mut self` (so the borrow checker enforces "valid until the next call"), `Drop` closes it. **Python**: nothing (the GIL makes it moot; the page says so). The `sys.rs` declarations of the C functions are added in the file's form.
  * **D-193** (after D-192, "A worker may hold a reader handle that caches a snapshot"): Question: how a worker avoids the shared writes of `cf_live_get`. Options: Y-10's A1 (nothing), A2 (this), A3 hazard pointers, A4 liburcu, A5 only. Decision: A2 with A5 inside; borrowed until the reader's own next call; one thread at a time per reader; the epoch is 32 bits; C++ and Rust enforce the thread rule by type, C says it. Rejected: A3 (the same handle and more code), A4 (LGPL, thread registration, Linux first), implicit thread-local caching (D-03's option 3: stays rejected). Held to: `test_reader`.
* **files**: `lib/config/include/configforge/cf_live.h`, `lib/config/src/core/live.c`, `lib/config/tests/test_reader.c` (new), `lib/config/tests/CMakeLists.txt`, `lib/cxx/include/configforge/configforge.hpp`, `lib/cxx/tests/` (a `test_wrapper_reader.cpp` beside `test_wrapper_access.cpp`), `bindings/rust/src/live.rs`, `bindings/rust/src/sys.rs`, `cmake/configforge.symbols` (the five names), `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 15's closing list gains the reader); last: `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-11 main` (after Y-10 and Y-9 have landed).
  2. Write `test_reader.c`, C89, `"cf_test.h"`, linked with `Threads::Threads` (guarded as `test_live` is, `lib/config/tests/CMakeLists.txt:289-298`). Cases: (a) one reader: `get` twice with no swap returns the same pointer and `cf_live_reader_generation` is unchanged; (b) after a reload that changes `server.port` the next `get` returns a different pointer with the new value and the generation is one more; (c) a rollback (probation) also changes the pointer: after `cf_live_rollback` the reader's `get` shows the rolled-back value; (d) `release` then `get` returns a snapshot again; (e) the old snapshot is not freed while another thread holds it by `cf_live_get` (a `cf_config_ref` count check through `cf_get_int` after the reader moved on); (f) `open(NULL, &err)` is NULL with `CF_E_CAPABILITY`; (g) four threads, each with its own reader, read `server.port` 100000 times while the main thread reloads 50 times with a different port each time; every value read is one the build returned and no thread reads a port older than one it read before. The same file is run under `ctest --preset tsan`.
  3. See it fail: `cmake --build --preset default --target test_reader 2>&1 | tail -n 15` (undeclared `cf_live_reader_open`).
  4. Add the epoch, `live_get_epoch`, the five functions, the declarations and the symbol lines. `ctest --preset default -R "test_reader|test_live|gate"`; `ctest --preset tsan -R "test_reader|test_live"`.
  5. See that the test can fail: make `live_swap` skip the epoch increment for a quiet swap behind a run-time-false condition (`if (keep == 0xFFu)`); case (c) fails; put it back.
  6. C++ wrapper and its test; Rust wrapper and its test (`cargo test` in `bindings/rust`, `cargo clippy -- -D warnings`, `cargo fmt --check`).
  7. D-193 into `docs/Decisions.md`; Guide; CHANGELOG.
  8. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-11: cf_live_reader, a worker's cached snapshot (D-193)`.
  9. ACCEPT; push `work/Y-11`; ACCEPT-CI (tsan, Rust, C++ jobs included).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_reader|test_live"` exit 0 and `ctest --preset tsan -R "test_reader|test_live"` exit 0; the benchmark of Y-10 re-run with a `reader/<threads>` cell shows the reader's cost at the thread counts of the page (written into "Readers under threads" in the same commit).
* **stop if**: Y-10's page shows no knee and nobody asked (the card then waits: report); `gate.abi` or `gate.symbols` refuses the names; ThreadSanitizer reports a race in case (g) (a design fault of the epoch: report with the report's first lines, change nothing); a binding's gate demands the Python wrapper.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_live_reader`: a worker's handle that caches the live snapshot and costs one shared read when nothing was swapped; C++ `LiveReader`, Rust `LocalReader` (Y-11, D-193)". `docs/Decisions.md`: D-193. Roadmap: marked by Y-12.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve"): the choice 1c is **(c)**: `cf_live_reader`, its borrowed pointer valid until the same thread's next call, a part beside `cf_live_get` (which stays the safe default, unchanged). It is gated by measurement: this card opens only when Y-10's page shows a knee with the number of cores, or a binding asks for it; until then (b), Y-10's generation read without the lock. Held to by its ThreadSanitizer test, and by lifetimes in the C++ and Rust wrappers.
  * 2026-11-27, the organiser: the choice is (c), as the recommendation and the maintainer's "I approve" (2026-11-26) say; the card opens in the sense that it is complete, and its first step waits for Y-10's page (a knee) or a binding's request (the maintainer's standing approval, 2026-11-27).

## Y-12 - `swapped` in `cf_live_options`: workers told of a swap

* **id**: Y-12
* **goal**: `cf_live_options` has two members, `swapped` and `swapped_user`, and the live calls `swapped(user, generation, prev, next)` on the driving thread after every swap and rollback, once readers can get `next`; and the Roadmap item "Threads and processes made easier" is marked done.
* **why**: the library tells the driving thread a file moved (`cf_watcher`) and a program learns of a swap only by asking for the generation; a program with workers posts a closure to each (Envoy's way) from the one place that knows. The function runs on the program's own thread, as `check`, `health` and `log` do (D-35, D-48), so it adds no thread of the library's (`docs/research/threads-and-processes.md:452-468`, option B2, and 580-606).
* **needs**: Y-9
* **size**: small
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided 2 (workers told of a swap): (b)**, `swapped` in `cf_live_options`, with 2.2.0's changed keys (a different card) not waited for. **Rejected**: (a) nothing, the generation read at a safe point (it stays possible); (c) a descriptor per reader that becomes readable on a swap (platform code per reader and a second watcher; a new card if asked).
  * **The type and the members** (`lib/config/include/configforge/cf_live.h`, the type before `cf_live_options` at line 131, the members after `settle`, line 165; the research's wording):
    ```c
    /* Called on the driving thread, inside poll, reload, wait, approve, rollback
       and commit, after readers can get `next`. `prev` may be NULL. For posting to
       the program's own workers; it must not call back into `l` except
       cf_live_get and cf_live_generation. */
    typedef void (*cf_live_swap_fn)(void* user, cf_u64 generation,
                                    const struct cf_config* prev,
                                    const struct cf_config* next);
    /* in cf_live_options, after `settle` (1.4 in the comment's form): */
    cf_live_swap_fn swapped;
    void*           swapped_user;
    ```
    `CF_LIVE_OPTIONS_INIT` (line 170) gains `, NULL, NULL` (two more zeros) at its end; the comment says "1.4: read only where `size` reaches them" as the 1.1 and 1.3 members' comments do (the header comment at 126-127 explains the size rule, D-51; use the next minor number the file's own pattern gives and say which in the commit).
  * **Where it is called.** In `live_swap` (`live.c:314-350`), after `live_unlock(l)` and the fingerprint and the log of changes (`live.c:337-341`) and before the keep-or-free block (`live.c:343-349`): `if (l->swapped != NULL) { l->swapped(l->swapped_user, <the generation read under the lock>, old, next); }`, where `<the generation>` is captured under the lock in a local (`cf_u64 gen = l->generation;` before `live_unlock`). `old` is the previous snapshot, still alive here (it is released after the call), NULL on the first swap if it ever is. `l->swapped` and `l->swapped_user` are copied from the options in `cf_live_open_with` (`live.c:653`) only when `o->size` reaches the members (the same test the 1.3 `settle` member uses: `grep -n "settle" lib/config/src/core/live.c` shows it), else NULL. Every swap goes through `live_swap`, including approve, rollback (quiet) and commit's reload, so every named call reports. The rollback's `generation` is the unchanged one (a rollback does not count), and `prev` and `next` differ.
  * **Callbacks and the guard.** `swapped` runs inside a driving call, so Y-9's guard is held: a `swapped` that calls a driving call is refused with `CF_E_POLICY`. The header comment says only `cf_live_get` and `cf_live_generation` are allowed.
  * **The changed keys.** 2.2.0's "a reload that names the keys it changed" is a different card; `swapped` gives both snapshots and `cf_diff_text` (`examples/c/live.c:100`) can be asked of them. This card does not wait for it.
  * **Bindings.** Not in this card (a binding wraps `cf_live_options` with its own members and its own gate decides; if a gate demands every member wrapped, stop and say which).
  * **The Roadmap item.** After this card has landed with Y-7 to Y-10 landed (Y-11 and Y-13-m may still be open), "**Threads and processes made easier.**" in `docs/Roadmap.md` (by its words) is marked 🟩 with one line under it: what was built (Y-7 to Y-12, D-190 to D-194) and that the reader handle (Y-11) and the shared image (Y-13-m) are later cards. When one of Y-7 to Y-10 is still open, this card marks nothing and says so.
  * **D-194** (after D-193, "Workers are told of a swap by a function on the driving thread"): Question: how a worker learns of a reload. Options: the generation at a safe point; a function the program supplies (decided); a descriptor per reader. Decision: `swapped`, called on the driving thread from `live_swap` after readers can get the new snapshot, before the old is released; it posts to the program's own workers and calls back only `cf_live_get` and `cf_live_generation`. Rejected: a descriptor per reader (platform code per reader, a second watcher, `eventfd` wakes one); waiting on a generation (a blocking primitive the C89 core has not); a thread of the library's (D-35). Held to: `test_live`'s new case.
* **files**: `lib/config/include/configforge/cf_live.h`, `lib/config/src/core/live.c`, `lib/config/tests/test_live.c`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 15's closing list gains it), `docs/Roadmap.md`; last: `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-12 main` (after Y-9 has landed).
  2. In `test_live.c` add `test_swapped`: a `swapped` that records into a struct (`calls`, last `generation`, last `prev`, last `next`, and for each call `cf_get_int(next, "server.port", 0)` and, when `prev != NULL`, of `prev`); a live opened with it in `CF_LIVE_MANUAL`; a changing reload: `calls` is 1, `next`'s port is the new one, `prev`'s is the old, `generation` equals `cf_live_generation(live)` called inside the callback (it is allowed); a reload that finds the same values: `calls` stays 1; `CF_LIVE_APPROVE` mode: a change builds pending with no call, `cf_live_approve` makes one; probation: a swap, then `cf_live_rollback`: `calls` goes up by one, `generation` is unchanged by the rollback, `next` shows the old value again; a live opened with a `size` below the new members (`sizeof(cf_live_options)` minus them, as the 1.0 callers pass) never calls it; a `swapped` that calls `cf_live_reload` on its live gets `CF_LIVE_FAILED` with `CF_E_POLICY` (Y-9). Add it to the test's main.
  3. See it fail: `cmake --build --preset default --target test_live 2>&1 | tail -n 15` (no member `swapped`).
  4. Add the type, the members, the initialiser, the copy in `cf_live_open_with` and the call in `live_swap`. `ctest --preset default -R "test_live|test_watch|gate" 2>&1 | tail -n 25`; `ctest --preset tsan -R test_live`.
  5. See that the test can fail: skip the call for the rollback's swap behind a run-time-false condition (`if (quiet == 0xFFu)` selecting a skip), the rollback case fails; put it back.
  6. D-194 into `docs/Decisions.md`; Guide; CHANGELOG; the Roadmap row as decided.
  7. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-12: swapped in cf_live_options, workers told of a swap (D-194)`.
  8. ACCEPT; push `work/Y-12`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live` exit 0 and `ctest --preset tsan -R test_live` exit 0; `gate.abi` and `gate.interface` green (the struct grows by its size rule).
* **stop if**: `cf_live_options` is used by a binding that copies the struct by value at a fixed size (report which: a size rule is then broken); a rollback's swap does not go through `live_swap` (read `live_rollback` first and report where it swaps); Y-9 is not landed (the last check of step 2 needs it: drop that check and say so).
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`swapped` in `cf_live_options`: a function called on the driving thread after every swap and rollback, for posting to a program's own workers (Y-12, D-194)". `docs/Decisions.md`: D-194. `docs/Roadmap.md`: "**Threads and processes made easier.**" marked 🟩 as decided.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: the choice 2 is (b), the card's recommendation (the maintainer's standing approval, 2026-11-27).

## Y-13-m - A shared image across processes: memfd and seals, `shm_open`, a read-only section, the source

* **id**: Y-13-m
* **goal**: a parent process writes a configuration's image into an anonymous, sealed, read-only shared object; a child it hands the descriptor or handle to maps it read-only, refuses one that is not sealed or is writable, and reads it in place as a source, on Linux, FreeBSD, macOS and Windows, with the private file of the Guide's recipe 4 on every other platform.
* **why**: the image was designed to be used in place (`docs/ImageFormat.md`, "Why it exists"; `lib/config/include/configforge/cf_image.h:58-63`) and a child can only read it from a private file today (a copy). Sharing pages is the zero-copy form (`docs/research/threads-and-processes.md:607-640`, section 5, "Later"); behind a build option that is OFF by default, its platform code costs nothing to a build that does not ask for it.
* **needs**: Y-7 (recipe 4 is the portable floor this card falls back to), Y-8
* **size**: large (a platform capability in three files, a public type, a source, a build option; split by the organiser into Y-13a, the platform capability and its tests, and Y-13b, the public type, the source and the Guide, if the first alone passes ~600 lines)
* **version**: 2.4.0
* **status**: open
* **decided**:
  * **Decided 3 (what the library gives for a shared image): (b)**, the platform capability and the public type below, behind a build option `option(CF_WITH_IMAGE_SHARE "a configuration's image in a sealed read-only shared object" OFF)` in `cmake/ConfigForgeOptions.cmake` (OFF by default; off in `minimal`, `misra` and the embedded profiles; ON in `server`); listed for profiles in `cmake/ConfigForgeProfile.cmake` as the other `CF_WITH_*` are. Without the option nothing below is compiled and Y-7's recipes 3 and 4 stay the way. **Rejected:** (a) documentation only (C2 copies once; the image was designed to be used in place, and the option keeps the cost off every build that does not ask).
  * **Decided 4 (does an image carry the contract): no.** The child's own contract decides; a child must be built with the same definitions, as today. **Rejected:** a contract section in the image (a new section and a minor format version, `docs/ImageFormat.md` "fields only grow"; the configuration compiler's business, not this card's).
  * **The public interface** (new `lib/config/include/configforge/cf_image_share.h`, under `#if CF_WITH_IMAGE_SHARE`; the research's sketch, copied):
    ```c
    typedef struct cf_image_share {
        size_t size;      /* sizeof, D-51                        */
        long   handle;    /* a descriptor, or a HANDLE's value   */
        const char* base; /* the mapping, once mapped            */
        size_t n;
        void*  reserved[4];
    } cf_image_share;
    /* Parent: the image of `c` in a sealed, read-only shared object. */
    CF_NODISCARD CF_API cf_bool cf_image_share_create(const struct cf_config* c, unsigned flags,
                                                      cf_image_share* out, cf_error* err);
    /* Child: map `handle` read-only, refuse it unsealed or writable, then cf_image_open. */
    CF_NODISCARD CF_API cf_bool cf_image_share_map(cf_image_share* s, long handle, cf_error* err);
    CF_API void cf_image_share_close(cf_image_share* s);
    /* A source over a mapped share, as cf_source_image_mem. */
    CF_API cf_bool cf_image_share_source(cf_source* src, const char* name, int rank,
                                         const cf_image_share* s, void* state, size_t state_size);
    ```
    (the research names the last `cf_source_image_share`; the card uses `cf_image_share_source` to keep the prefix of the type.) `flags` is 0 now and refused otherwise (`CF_E_CAPABILITY`, `unknown flags 0x%x`). Errors (D-223): each names the operation, the handle and the system's reason: `cannot share the image: %s` (create), `cannot map image handle %ld: %s` (map), with `strerror(errno)` on POSIX and `error %lu` from `GetLastError()` on Windows, as F-1a's writes do.
  * **The platform capability** (internal, `lib/platform/include/configforge/cf_platform.h`, `CF_INTERNAL_API`): `cf_platform_share_create(const void* bytes, size_t n, long* handle, cf_error*)` and `cf_platform_share_map(long handle, const char** base, size_t* n, cf_error*)`, `cf_platform_share_unmap(const char* base, size_t n)`. **Linux and FreeBSD 13+:** `memfd_create("configforge", MFD_CLOEXEC | MFD_ALLOW_SEALING)`, `ftruncate`, `write`, then `fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL)`; the child maps with `mmap(PROT_READ, MAP_SHARED)` and checks `fcntl(fd, F_GET_SEALS)` has all four, refusing otherwise (`'%s' is not a sealed image`): a read-only descriptor alone is not enough, since `/proc/<pid>/fd` reopens it (the research's item 10). **macOS and other BSDs** (no seals exist): `shm_open` of the name `/cf-<pid>-<n>-<t>` (`n` a process-wide counter, `t` the low 32 bits of the monotonic clock, both hex; under 31 characters for macOS's limit; on `EEXIST` the counter moves and it retries up to 8 times, then fails) with `O_CREAT | O_EXCL | O_RDWR`, `ftruncate`, `write`; then the same name opened a second time with `O_RDONLY`; then `shm_unlink` of the name at once. The read-only descriptor is the one handed over, the writable one is closed, so the child can only map `PROT_READ` (the parent is trusted: the child is given no way to write). **Windows:** an unnamed section (`CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, ...)`), filled through `MapViewOfFile(FILE_MAP_WRITE)`, then `DuplicateHandle` with `FILE_MAP_READ` access only; the child's `MapViewOfFile(FILE_MAP_READ)`; handed over by `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` in the program's own `CreateProcess` (never a name: item 16). **None, and any target where none of these is known:** `CF_E_CAPABILITY`, `this platform cannot share an image; write it to a private file (cf_platform_create_private) and pass the path`. How the number reaches the child (an environment variable as `LISTEN_FDS` and nginx's `NGINX` do, `SCM_RIGHTS`, the systemd store) is the program's: the library reads a number it is given and starts no process.
  * **The split.** The carrier splits the card into two commits on the one branch when the diff passes ~600 lines: first the platform capability and its tests, then the public type, the source and the Guide; it lands as one card.
  * **D-195** (after D-194, "A shared image is a sealed read-only object handed over by the program"): Question: how a child process reads its parent's configuration without a copy. Decision: `cf_image_share` behind `CF_WITH_IMAGE_SHARE` (OFF by default): memfd with four seals on Linux and FreeBSD, a read-only `shm_open` descriptor with the name unlinked at once elsewhere on POSIX, a read-only duplicated section handle on Windows; the child refuses an unsealed or writable object; the program hands the number over and starts the process; the image does not carry the contract. Rejected: documentation only; a named object (item 16); a contract in the image. Held to: `test_share`.
* **files**: `lib/config/include/configforge/cf_image_share.h` (new), `lib/config/src/bridge/image_share.c` (new), `lib/platform/include/configforge/cf_platform.h`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/win32.c`, `lib/platform/src/none/none.c`, `lib/config/tests/test_share.c` (new), `lib/config/tests/CMakeLists.txt`, `CMakeLists.txt` and `cmake/profiles/*.cmake` (the option), `cmake/configforge.symbols`, `cmake/configforge.internal.symbols`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-13-m main` (after Y-7 and Y-8). Add the option to `cmake/ConfigForgeOptions.cmake` and `cmake/ConfigForgeProfile.cmake`, ON in the `server` profile (`cmake/profiles/server.cmake`; `ls cmake/profiles` for its name).
  2. Test first: `lib/config/tests/test_share.c` (compiled only under `CF_WITH_IMAGE_SHARE`, otherwise a test that prints `skipped: no image share`), added in `lib/config/tests/CMakeLists.txt` as `test_image` is: (a) the parent builds a config from a small TOML text, `cf_image_share_create`, then a child (`fork` on POSIX, `CreateProcessW` of the test binary itself with an argument `child <handle>` and `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` on Windows) maps the handle, opens the source and reads two keys, exit 0; (b) on Linux and FreeBSD a memfd without seals, made by the test itself, is refused with `is not a sealed image`; (c) on every platform a writable descriptor or handle (the test opens one itself) is refused; (d) `flags` 1 is refused; (e) on `none` the create is `CF_E_CAPABILITY` with the recipe-4 message.
  3. See it fail: `cmake -S . -B build-share -DCF_WITH_IMAGE_SHARE=ON && cmake --build build-share --target test_share 2>&1 | tail -n 20` (nothing declared).
  4. The platform capability: the three functions in `cf_platform.h` (`CF_INTERNAL_API`), `posix.c` (memfd and seals under `#if defined(__linux__) || defined(__FreeBSD__)`, `shm_open` otherwise), `win32.c`, `none.c`; their names in `cmake/configforge.internal.symbols`. `ctest --test-dir build-share -R "test_share|test_platform" 2>&1 | tail -n 20` for (b), (c), (e).
  5. The public part: `cf_image_share.h`, `lib/config/src/bridge/image_share.c` (create = `cf_image_write` into an arena buffer then `cf_platform_share_create`; map = `cf_platform_share_map` then `cf_image_open`; the source as `cf_source_image_mem`'s, `grep -n cf_source_image_mem lib/config/src/*/*.c`), the names in `cmake/configforge.symbols`. `ctest --test-dir build-share -R "test_share|test_image|gate" 2>&1 | tail -n 30`; the default build's `ctest --preset default` unchanged.
  6. See that the test can fail: skip the seal check in the child behind a run-time-false condition (never a constant); (b) fails; put it back.
  7. The Guide: a paragraph after recipe 4 in "Threads" (`grep -n "^## 14. Threads" docs/Guide.md`), D-195, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-13-m: cf_image_share, a sealed read-only image for a child process (D-195)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX (Linux and the `sanitize` preset), ACCEPT-WINDOWS, ACCEPT-CI with the macOS and FreeBSD jobs; `ctest --test-dir build-share -R test_share` exit 0 (a child maps the share and reads the parent's keys; an unsealed or writable object is refused); the default build's tests unchanged.
* **stop if**: a CI platform's kernel lacks `memfd_create` or `F_ADD_SEALS` (FreeBSD older than 13: fall back to the `shm_open` path there and say so; stop only if neither works); `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` is unavailable in the CI's Windows SDK (report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_image_share`: a configuration's image in a sealed read-only shared object, mapped by a child process (`CF_WITH_IMAGE_SHARE`) (Y-13-m, D-195)". `docs/Decisions.md`: D-195. Roadmap: nothing (the item is marked by Y-12).
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Question 3 is (b), the shared image built behind `CF_WITH_IMAGE_SHARE`, OFF by default; question 4 is no, the card's recommendation; the `shm_open` name, the error wording and the split are written above; version 2.4.0 kept (the maintainer's standing approval, 2026-11-27).

---

## Y-14 - Compressed input by the program's own zlib: the D-103 recipe, a Guide section and an example

* **id**: Y-14
* **goal**: a program that links its own zlib can read a `.toml.gz` through ConfigForge today: the Guide has a section "Compressed input" showing a repair that inflates (D-103's composed codec) and `examples/c/inflate.c` builds and runs it under the suite.
* **why**: `docs/research/compression.md` section 5 ("A new interface ... Guide shows both") and section 6, first row: the recipe is what the library lacks a backend for, and it is also the answer before Y-15 to Y-18 exist.
* **needs**: nothing
* **size**: small
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **The recipe** is D-103's: a composed codec of a base codec and a repair (`CF_WITH_COMPOSE`, `cmake/ConfigForgeOptions.cmake:19`). The repair is a function the program supplies: it receives the compressed bytes and returns the text. The example inflates with the program's own `zlib` (`inflateInit2(&zs, 16 + MAX_WBITS)` for gzip), checking the output against a limit the program chooses (the example uses 1 MiB and says why: it is the program's bound, not the library's; Y-15's bound replaces it).
  * **Named only, read only, no shared bound** (research section 4.1): the Guide says so in one sentence each, and names `lib/compress` (Y-15a) as the layer that detects and bounds.
  * **The example builds only when the program's zlib is there**: `examples/c/CMakeLists.txt` gets `find_package(ZLIB)`; with no zlib the example is skipped with a status line. `gate.dependencies` (`docs/Dependencies.md`, top) forbids a package the page does not name: add one row to `docs/Dependencies.md` section 2 (developing), "zlib | Zlib | the compressed-input example | skipped when absent", and say it is never linked into the library.
  * **Shape of the example**: reads `examples/c/data/app.toml.gz` (a 200-byte file made once with `gzip -9n` and committed; its text is a three-key TOML) through the composed codec, prints `server.port`; the suite's example harness compares the output (as `examples/c/settings.c`'s test does: find it with `grep -rn "example.settings" examples tests cmake | head -3`).
  * No decision number.
* **files**: `examples/c/inflate.c` (new), `examples/c/data/app.toml.gz` (new), `examples/c/CMakeLists.txt`, `docs/Guide.md` (section 8, "Formats": a subsection "Compressed input" at its end; find the end with `grep -n "^## 9" docs/Guide.md`), `docs/Dependencies.md`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-14 main`.
  2. Read the composed-codec example in the Guide (`grep -n "cf_compose\|repair" docs/Guide.md | head`) and copy its shape.
  3. Write the example's expected output into the harness first (the test), then the CMake wiring with `inflate.c` absent.
  4. See it fail: `cmake --preset default && cmake --build --preset default 2>&1 | tail -n 20 && ctest --preset default -R example.inflate 2>&1 | tail -n 20` fails (no such source).
  5. Write `inflate.c` and make `app.toml.gz` (`printf '[server]\nport = 8080\nhost = "localhost"\n' | gzip -9n > examples/c/data/app.toml.gz`). The test passes.
  6. See that the test can fail: change the expected port in the harness to 8081, see the test fail, put it back.
  7. Guide, Dependencies row, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-14: compressed input by the program's own zlib, a recipe and an example`.
  8. ACCEPT; push; ACCEPT-CI (a runner without zlib skips the example and passes).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the example is skipped when zlib is not found there: say so in the commit); ACCEPT-CI; `ctest --preset default -R "example.inflate|gate.dependencies"` exit 0.
* **stop if**: the composed codec's repair cannot receive binary bytes (D-103's interface takes text): report it, and Y-15a's layer becomes the only route; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide section and `examples/c/inflate.c` on reading a gzip file through a composed codec with the program's own zlib (Y-14)." Roadmap: Y-20 marks "compression".

---

## Y-15a - `lib/compress`: the two interfaces `cf_packer` and `cf_unpacker`, their options with the method's own defaults, the one-call `cf_pack` and `cf_unpack`, and a test backend

* **id**: Y-15a
* **goal**: with `-DCF_WITH_COMPRESS=ON` (OFF by default) `lib/compress` exists: the structs `cf_unpacker` and `cf_packer` that a method fills (a method is a value, `cf_gzip_unpacker` later), `cf_unpack_options` and `cf_pack_options` with no fixed limit (each method carries its own defaults, a caller overrides field by field, 0 is unlimited, the bomb guard is on), the one-call `cf_unpack` and `cf_pack`, `cf_unpack_limit`, `cf_unpack_detect` over a list the program passes, two new refusal reasons, and a test backend of the test's own (a run-length form), with no compression library linked and nothing of the component in a build that leaves the option off.
* **why**: `docs/research/compression.md` section 5, "First: reading gzip and zstd, as a byte layer" (option B): one bound against bombs instead of one per program, every reader unchanged, and (D-221) a component that is usable alone and optional. Sections 4.1 (placement) and 4.3 (bounds) hold the reasons; D-220 and D-221 (docs/plan/Y-asked.md, "Approved by the maintainer") the shape.
* **needs**: Y-14, K-9b
* **size**: large
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **The component (D-221 points 1 and 6b).** `lib/compress/` as D-100 lays components out: `include/configforge/cf_compress.h`, `src/compress.c`, `tests/`, and `CMakeLists.txt` with one `cf_component(compress DEPENDS port numeric base SOURCES src/compress.c)` as `lib/text/CMakeLists.txt` writes its own (no `SERIALIZE_SOURCES`: the serialization library does not carry it). It uses only `cf_port.h`, `cf_alloc.h` (the arena and the allocator) and `cf_error.h`; it knows nothing of serialization, codecs or configuration, and `cmake -S lib/compress` builds it alone as the static library `cf_compress`. `option(CF_WITH_COMPRESS "the compression component: the packer and unpacker interfaces and their bounds (backends are options of their own)" OFF)` in `cmake/ConfigForgeOptions.cmake` after `CF_WITH_COMPOSE`; `lib/CMakeLists.txt` adds the directory after `serialize` only when it is ON. The places that name `CF_WITH_COMPOSE` are the places that learn `CF_WITH_COMPRESS` (the worker lists them with `grep -rn "CF_WITH_COMPOSE" cmake lib scripts CMakeLists.txt`; more than 8 files is a "stop if"). With the option OFF no byte of the component is compiled and `cf_has_feature("compress")` is false.
  * **The shapes.** All in `cf_compress.h`; `size` first in every struct (D-51: a member appended later is read only where `size` reaches it, and a caller's older struct gets the default for the rest).
    ```c
    /* A member left at this value takes the method's own default. */
    #define CF_COMPRESS_DEFAULT ((size_t)-1)

    typedef struct cf_unpack_options {
        size_t size;      /* sizeof(cf_unpack_options): cf_unpack_options_init sets it */
        size_t max_size;  /* output bytes at most; 0: unlimited; default: the method's, 256 MiB for the built-in methods */
        size_t max_ratio; /* output bytes per input byte at most; 0: no ratio bound; default: the method's, 64 */
        size_t min_floor; /* output bytes always allowed, whatever the ratio; default: the method's, 1 MiB */
        size_t window;    /* a decoder's window or memory in bytes, for a method that has one; 0: the method's own cap; default: the method's (zstd: 8 MiB) */
    } cf_unpack_options;

    typedef struct cf_pack_options {
        size_t size;      /* sizeof(cf_pack_options): cf_pack_options_init sets it */
        int    level;     /* the method's level; CF_COMPRESS_LEVEL_DEFAULT (-1): the method's own default */
        size_t window;    /* a window or memory in bytes, for a method that has one; default: the method's */
        size_t max_size;  /* output bytes at most; 0: unlimited; default: the method's, 256 MiB */
    } cf_pack_options;

    typedef struct cf_unpacker {
        size_t               size;       /* sizeof(cf_unpacker) */
        const char*          name;       /* "gzip", "zstd" */
        const char*          extensions; /* ".gz", ".zst;.zstd"; NULL: by magic or by name only */
        const unsigned char* magic;      /* NULL: no magic */
        size_t               magic_len;
        cf_unpack_options    defaults;   /* this method's own: every member set, none CF_COMPRESS_DEFAULT */
        cf_bool (*unpack)(const struct cf_unpacker* self, const cf_unpack_options* o,
                          const unsigned char* in, size_t n, size_t limit, cf_arena* ar,
                          unsigned char** out, size_t* out_n, cf_error* err);
        const void*          data;       /* a dictionary, a program's state */
    } cf_unpacker;

    typedef struct cf_packer {
        size_t               size;       /* sizeof(cf_packer) */
        const char*          name;
        const char*          extensions;
        cf_pack_options      defaults;   /* the method's own: every member set */
        cf_bool (*pack)(const struct cf_packer* self, const cf_pack_options* o,
                        const unsigned char* in, size_t n, cf_arena* ar,
                        unsigned char** out, size_t* out_n, cf_error* err);
        const void*          data;
    } cf_packer;

    CF_API void   cf_unpack_options_init(cf_unpack_options* o); /* size set, every member CF_COMPRESS_DEFAULT */
    CF_API void   cf_pack_options_init(cf_pack_options* o);     /* size set, level -1, the others CF_COMPRESS_DEFAULT */
    CF_API size_t cf_unpack_limit(const cf_unpacker* u, const cf_unpack_options* o, size_t n);
    CF_NODISCARD CF_API cf_bool cf_unpack(const cf_unpacker* u, const cf_unpack_options* o,
                                          const unsigned char* in, size_t n, cf_arena* ar,
                                          unsigned char** out, size_t* out_n, cf_error* err);
    CF_NODISCARD CF_API cf_bool cf_pack(const cf_packer* p, const cf_pack_options* o,
                                        const unsigned char* in, size_t n, cf_arena* ar,
                                        unsigned char** out, size_t* out_n, cf_error* err);
    CF_API const cf_unpacker* cf_unpack_detect(const cf_unpacker* const* list, size_t count,
                                               const unsigned char* in, size_t n,
                                               const char* path);
    ```
    The names of the default values a method may use to fill `defaults`: `CF_UNPACK_DEFAULT_RATIO` 64, `CF_UNPACK_DEFAULT_FLOOR` 1048576, `CF_UNPACK_DEFAULT_MAX` 268435456 (macros documenting the defaults of the built-in methods, never read by `cf_unpack`: the number that counts is the method's `defaults`).
  * **Options resolved field by field (D-221 point 4, D-220).** `cf_unpack` resolves an effective option set before it calls the method: for each member, the caller's value when `o` is not NULL, `o->size` reaches the member and it is not `CF_COMPRESS_DEFAULT`; otherwise the method's `u->defaults`. `0` stays `0`: unlimited, chosen explicitly. `cf_unpack_limit(u, o, n)` is the output bound for `n` input bytes under those effective values: with `max_ratio` and `max_size` both 0, `SIZE_MAX`; with `max_ratio` 0, `max_size`; with `max_size` 0, `max_ratio * n + min_floor`; otherwise the smaller of `max_size` and `max_ratio * n + min_floor`; every product and sum is checked and saturates at `SIZE_MAX` (`n > (SIZE_MAX - min_floor) / max_ratio` gives `SIZE_MAX`, then `max_size` decides). The method gets the resolved `o` (no `CF_COMPRESS_DEFAULT` left) and `limit`; `cf_unpack` also refuses output a method returns above `limit` (a backend is not trusted to count). `cf_pack` resolves `cf_pack_options` the same way; `max_size` bounds its output.
  * **Refusals (D-223).** Reasons in `lib/base/include/configforge/cf_reason.def`, after `CF_REASON_BUDGET` (the reason-count test `lib/config/tests/test_reason.c` and `docs/Limits.md` follow): `CF_REASON_UNPACK_BOMB` (`"unpack-bomb"`, "a compressed document that makes more than its bound") and `CF_REASON_UNPACK_CORRUPT` (`"unpack-corrupt"`, "a compressed document that does not decompress"). Messages, code `CF_E_SOURCE`, set with `cf_error_set` then `cf_error_place(err, <reason>, 0, 0)`: `'<name>' would make more than <limit> bytes (the bound for <n> compressed bytes)`; `'<name>' is damaged: <the backend's words>`. `cf_unpack` with a NULL `unpack`: `CF_E_CAPABILITY`, `'<name>' cannot unpack`; `cf_pack` with a NULL `pack`: `'<name>' cannot pack`; an arena that has no room: `CF_E_MEMORY`. `CF_REASON_NESTED` is not here: a layer that finds compressed bytes inside compressed bytes is the source layer's rule (Y-18); `lib/compress` composes layers only when asked (Y-15b's chain).
  * **Detection with no registry (D-101, D-221 point 5).** `cf_unpack_detect(list, count, in, n, path)` looks only at the `count` unpackers the program passes: first the outer extension of `path` (the last `.`-suffix, compared to each method's `extensions`, a `;` separated list, ignoring ASCII case; NULL `path` skips this), then the magic (`n >= magic_len` and the first bytes equal); the first method of `list` that matches wins; NULL when none does. There is no global registry: a program that wants one keeps its own array. Binding a method to a configuration is Y-18's `cf_builder_unpacker`, which needs the component and so lives in `lib/config` under `#if CF_WITH_COMPRESS`.
  * **The test backend.** In `lib/compress/tests/test_compress.c`, the test's own `static const cf_unpacker rle_unpacker` and `static const cf_packer rle_packer`: the form `<byte><count>` pairs (count 1 to 255) after the magic `"RL"`, extension `.rl`, `defaults` filled from the three macros; written by the test as a program's own method would be, so that the interface is proved from outside. A second method `rle_big` with other defaults (`max_ratio` 1000, `max_size` 1 GiB, `min_floor` 4 MiB) proves the method's own defaults win over the macros.
  * **Conventions held (K-9c, K-11a, K-9a, K-9b).** Every declaration that reports a failure is `CF_NODISCARD`; the two structs named `..._options` have `size` first and an `_init` function (`gate.options`); each new public function has its `Guarantee:` line: `cf_unpack_options_init`, `cf_pack_options_init`, `cf_unpack_limit` and `cf_unpack_detect` `nothrow`, `cf_unpack` and `cf_pack` `strong` (`*out` and `*out_n` are unspecified on failure); a sweep case with `tests/support/cf_oom.h` refuses the nth allocation of `cf_unpack` and `cf_pack` and expects `CF_E_MEMORY` (K-9b).
  * **The symbols.** The new names are in a list of their own, `cmake/configforge.compress.symbols`, read by `gate.abi` only when the library was built with `CF_WITH_COMPRESS=ON` (the worker reads `cmake/gates/GateAbi.cmake` for how a name that a build option leaves out is treated and follows it; no such way is a "stop if"). The default build's symbol list does not change.
  * **D-196** ("Compression is a component with two interfaces, bounded once"; supersedes the earlier one-struct text): Question: how a program reads `app.toml.gz`, and where the bound against bombs lives. Options: composed codecs (named only, M x N, no shared bound, read only); a byte layer in the serialization library; a component of its own with a `cf_unpacker` and a `cf_packer` (decided). Decision: `lib/compress`, optional and OFF by default, bytes in and bytes out; two interfaces filled by a method value; options with the method's own defaults and no fixed limit; the bomb guard on by default; the bound is checked while unpacking and never from a header. Rejected: one struct with a NULL half; a global registry; nested decompression by default; a vendored zlib or zstd. **D-197** (the bounds): the four option members and how `cf_unpack_limit` combines them, with the reasons of research section 4.3. D-196 and D-197 extend to the stream (Y-15b). D-220 and D-221 are copied into `docs/Decisions.md` by this card as they stand in docs/plan/Y-asked.md ("Approved by the maintainer"), unless `grep -n "^## D-221" docs/Decisions.md` finds them.
* **files**: `lib/compress/CMakeLists.txt` (new), `lib/compress/include/configforge/cf_compress.h` (new), `lib/compress/src/compress.c` (new), `lib/compress/tests/test_compress.c` (new), `lib/compress/tests/CMakeLists.txt` (new), `lib/CMakeLists.txt`, `cmake/ConfigForgeOptions.cmake` and the other files the `CF_WITH_COMPOSE` search names, `lib/base/include/configforge/cf_reason.def`, `lib/config/tests/test_reason.c`, `cmake/configforge.compress.symbols` (new) and the file that reads it, `docs/Limits.md` (generated); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guarantees.md` (generated by K-9a's script), `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-15a main`. Write `lib/compress/tests/test_compress.c` (C89) and its `CMakeLists.txt`, with the cases: (a) `cf_unpack_detect` by extension (`x.toml.rl` finds `rle_unpacker` from a list of two methods), by magic with a name that has no extension, `NULL` for neither, a method listed first wins, and `NULL` `path` uses the magic only; (b) the limit: `cf_unpack_limit(&rle_unpacker, NULL, 0)` is 1 MiB, `(.., 1024)` is 64 * 1024 + 1 MiB, `(.., SIZE_MAX / 2)` is 256 MiB (`max_size`); a caller's `max_ratio = 1` changes only the ratio (`(.., 1024)` is 1024 + 1 MiB); `max_ratio = 0` gives `max_size`; `max_size = 0` gives the ratio bound; both 0 give `SIZE_MAX`; an options struct whose `size` stops before `max_ratio` leaves the method's ratio; `rle_big`'s own defaults apply with a NULL `o` and a member the caller sets wins over them; (c) a bomb (`"RL"` then 5000 pairs of `x 255`: 1.2 MB for 10 KB, under 64 x + 1 MiB: unpacks; then 300000 pairs for 600 KB making 76 MB, over 64 x 600 KB + 1 MiB: refused, reason `unpack-bomb`, message `'rle' would make more than 39448704 bytes (the bound for 600002 compressed bytes)`); with `max_ratio = 0` and the default `max_size` the same input unpacks (76 MB is under 256 MiB) and with `max_size = 1000` is refused; (d) a truncated body: `unpack-corrupt`; (e) `cf_pack` with `rle_packer` then `cf_unpack`: the round trip is the input; `cf_pack` with a NULL `pack`: `'rle' cannot pack`; `cf_unpack` with a NULL `unpack`: `'rle' cannot unpack`; (f) a method that returns more than `limit`: refused as a bomb; (g) the output is owned by the arena (`cf_arena_used` grows; freeing the arena frees it: the sanitize build shows no leak); (h) K-9b's sweep (`tests/support/cf_oom.h`) over `cf_unpack` and `cf_pack`, every failure `CF_E_MEMORY`.
  2. See it fail: `cmake -S . -B build-compress -DCF_WITH_COMPRESS=ON && cmake --build build-compress --target test_compress 2>&1 | tail -n 20` (undeclared names).
  3. Write the header, `compress.c`, the reasons, the option and the symbol list. `ctest --test-dir build-compress -R "test_compress|gate|reason" 2>&1 | tail -n 30`.
  4. The component alone: `cmake -S lib/compress -B build-alone && cmake --build build-alone && ctest --test-dir build-alone` exit 0 (the proof that it is usable alone); the default build: `ctest --preset default` exit 0 with no compress test and `gate.abi` unchanged.
  5. See that the test can fail: skip the check that a method's output is within `limit` behind a run-time-false condition (a compared run-time value, never a constant); case (f) fails; put it back.
  6. D-196, D-197, D-220 and D-221, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-15a: lib/compress, cf_unpacker and cf_packer, their options and one-call shortcuts (D-196, D-197)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --test-dir build-compress -R test_compress` exit 0; `cmake -S lib/compress` alone builds and passes; `ctest --preset default` and `gate.abi` unchanged in the default build; `gate.options`, `gate.nodiscard`, `gate.guarantees`, `gate.oom` exit 0.
* **stop if**: `gate.layering`, `gate.symbols`, `gate.abi` or the reason-count test wants a change this card does not name (say which); the option cannot be added without a change to more than 8 files; `lib/compress` needs a header of `lib/text` or `lib/serialize` (the component is wrong: report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "`lib/compress` (`CF_WITH_COMPRESS`, OFF by default): the `cf_unpacker` and `cf_packer` interfaces, their options with each method's own defaults, and the one-call `cf_unpack` and `cf_pack`, with one bound against bombs (Y-15a, D-196, D-197)." `docs/Decisions.md`: D-196, D-197, D-220, D-221 (the last two if not there). Roadmap: Y-20 marks "compression".
* **Organiser's answers**:
  * Question 1 of the earlier card (where compression sits): a byte layer, and now a component of its own, `lib/compress`, optional and OFF by default (D-221 points 1 and 6b); the composed codec stays Y-14's recipe (the maintainer's standing approval, 2026-11-27).
  * Question 4 of the earlier card (the bounds' numbers): not fixed numbers but options with the method's own defaults (D-220, D-221 point 4): ratio 64, floor 1 MiB, ceiling 256 MiB, zstd window 8 MiB (its `windowLogMax` 23) are the defaults of the built-in methods; 0 is unlimited when chosen (the maintainer's standing approval, 2026-11-27).
  * Question 5 of the earlier card (the interface): two interfaces, `cf_unpacker` and `cf_packer`, not one struct with a NULL half (D-221 point 2); the name `cf_unpacker` stays (the maintainer's standing approval, 2026-11-27).
  * Errors follow D-223: a reason, a code, `CF_NODISCARD`, `CF_E_MEMORY` for an arena with no room; `CF_REASON_NESTED` and `cf_builder_unpacker` belong to Y-18, which is the card that binds a method to a configuration (the maintainer's standing approval, 2026-11-27).
  * The version is 2.3.0, Layout.md's module 4 (the 1.10.0 of D-221 point 7 is the earlier plan's name for it) (the maintainer's standing approval, 2026-11-27).

## Y-15b - Streaming `cf_unpacker` and `cf_packer`, and `cf_unpack_chain`

* **id**: Y-15b
* **goal**: a method can also fill a streaming form (open, read or write, finish, close) so that a file, an unpacker and a consumer chain without the whole document in memory, the bound is checked as bytes flow, `cf_unpack` and `cf_pack` still work for a method that fills only one of the two forms, and `cf_unpack_chain` composes unpackers into a value that is itself a `cf_unpacker`.
* **why**: D-221 point 3: the streams are the real interface and the one-call functions its shortcut; a chain (`.tar.gz` style: two layers) needs them, and a large non-configuration input should not have to sit whole in memory. `docs/research/compression.md` section 4.2 holds the reasons.
* **needs**: Y-15a
* **size**: large
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **The stream types** (appended to `cf_compress.h`).
    ```c
    /* Bytes from a source: how many were put in buf (0: the end), or -1 with err set. */
    typedef long (*cf_read_fn)(void* ctx, unsigned char* buf, size_t cap, cf_error* err);
    /* Bytes to a sink: CF_TRUE, or CF_FALSE with err set. */
    typedef cf_bool (*cf_write_fn)(void* ctx, const unsigned char* p, size_t n, cf_error* err);

    typedef struct cf_unpack_stream cf_unpack_stream; /* opaque: a method's state */
    typedef struct cf_pack_stream   cf_pack_stream;
    ```
    Members appended to `cf_unpacker` after `data` (D-51): `cf_unpack_stream* (*open)(const struct cf_unpacker* self, const cf_unpack_options* o, cf_read_fn src, void* src_ctx, const cf_allocator* a, cf_error* err);`, `long (*read)(cf_unpack_stream* s, unsigned char* buf, size_t cap, cf_error* err);` (same result as `cf_read_fn`), `void (*close)(cf_unpack_stream* s);`. Members appended to `cf_packer` after `data`: `cf_pack_stream* (*open)(const struct cf_packer* self, const cf_pack_options* o, cf_write_fn dst, void* dst_ctx, const cf_allocator* a, cf_error* err);`, `cf_bool (*write)(cf_pack_stream* s, const unsigned char* p, size_t n, cf_error* err);`, `cf_bool (*finish)(cf_pack_stream* s, cf_error* err);` (flushes the last block), `void (*close)(cf_pack_stream* s);`. A method fills the one-call form, the streaming form, or both.
  * **The calls.** `CF_NODISCARD CF_API cf_unpack_stream* cf_unpack_open(const cf_unpacker* u, const cf_unpack_options* o, cf_read_fn src, void* src_ctx, const cf_allocator* a, cf_error* err);` (NULL `a` is `cf_default_allocator()`; none available: `CF_E_CAPABILITY`), `CF_API long cf_unpack_read(cf_unpack_stream* s, unsigned char* buf, size_t cap, cf_error* err);`, `CF_API void cf_unpack_close(cf_unpack_stream* s);`, and for packing `cf_pack_open`, `cf_pack_write`, `cf_pack_finish`, `cf_pack_close` with the same shapes. A method with only the one-call form, opened as a stream, is served by `lib/compress` itself: `cf_unpack_open` reads the whole input from `src` into the arena it makes, calls `unpack` once and then serves the result in pieces (not constant memory: its comment says so; the bound still holds); and `cf_unpack` over a method with only the streaming form opens it on a memory source and drains it into the caller's arena. `cf_unpack` prefers the one-call form when the method has it. `cf_pack` and `cf_pack_open` are symmetric.
  * **The bound while it flows.** The input length is not known: the stream counts the bytes it has taken from `src` (`in_total`) and the bytes it has produced (`out_total`) and refuses, with `CF_REASON_UNPACK_BOMB` and the Y-15a message (the bound named for the bytes consumed so far), when `out_total` exceeds the effective `max_size`, or exceeds `max_ratio * in_total + min_floor` (the same saturating arithmetic as `cf_unpack_limit`). It is checked before each block is handed on, never from a header.
  * **The chain.** `typedef struct cf_unpack_chain { cf_unpacker as_unpacker; const cf_unpacker* const* list; size_t count; } cf_unpack_chain;` owned by the caller; `CF_API cf_bool cf_unpack_chain_init(cf_unpack_chain* c, const cf_unpacker* const* list, size_t count, cf_error* err);` fills `c->as_unpacker` (name `"chain"`, no extensions or magic, `defaults` the first method's, a streaming form, a one-call form built over it, `data = c`) and refuses `count == 0` or a NULL entry (`CF_E_DEFINITION`, `a chain needs at least one method`). The caller passes `&c->as_unpacker` wherever an unpacker goes. The first method of `list` reads the outer bytes. Each layer enforces its own options against its own input; the ratio therefore compounds across layers and `max_size` bounds each layer's output, which the comment on `cf_unpack_chain_init` says in those words. A chain is itself a method, so a chain of chains is allowed; the nesting depth is the program's. The one-layer-only rule is the source layer's (Y-18), not this component's.
  * **Streams and errors (D-223).** Every `open` and `read` failure is a `cf_error` with a code and a message; a failure of the upstream `cf_read_fn` is passed through unchanged, with the layer's name added to the message (`'<name>': <upstream's words>`); an arena or allocator with no room is `CF_E_MEMORY`. `cf_unpack_read` after a failure keeps returning -1 with the same error; `cf_unpack_close` of a NULL stream does nothing.
  * **The test backend** grows a streaming form (the test's own `rle` read in steps, pair by pair, with a three-byte `cf_read_fn` source that delivers its input in pieces that cut pairs in the middle), so that a block boundary is exercised; and the chain is tried as `rle` over `rle` (two layers) and a chain of one.
  * **Guarantees (K-9a) and faults (K-9b)**: `cf_unpack_open`, `cf_pack_open` `strong`; `cf_unpack_read`, `cf_pack_write`, `cf_pack_finish` `basic: the stream is valid; the bytes already handed on stay handed on`; `cf_unpack_close`, `cf_pack_close` `nothrow`; `cf_unpack_chain_init` `strong`. A sweep case over `cf_unpack_open` with a read to the end, and over `cf_pack_open` with a write and a finish.
  * **D-196 and D-197 are extended**, not replaced (no new D-number): the stream form, the bound as it flows, the chain.
  * **Rejected**: a push interface for unpacking (a pull `read` composes with `cf_read_fn` and needs no second shape); one fixed block size in the interface (it is the method's); a chain that checks one bound over all layers (each layer knows only its own input).
* **files**: `lib/compress/include/configforge/cf_compress.h`, `lib/compress/src/compress.c`, `lib/compress/tests/test_compress_stream.c` (new) and `lib/compress/tests/CMakeLists.txt`, `cmake/configforge.compress.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-196 and D-197 extended), `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-15b main` (after Y-15a). Write `lib/compress/tests/test_compress_stream.c` (C89): (a) the streaming `rle` read through `cf_unpack_open` with a three-byte source equals the one-call result for the same input; (b) a method with only the one-call form opened as a stream gives the same bytes; a method with only the streaming form through `cf_unpack` too; (c) the bound as it flows: the 600 KB bomb of Y-15a's case (c) is refused after the first block that crosses `64 * in_total + 1 MiB`, with the bytes produced before it equal to that bound at most, and the message names the bytes consumed so far; (d) `cf_pack_open`, writes of 1, 7 and 1000 bytes, `cf_pack_finish`, equal to `cf_pack` over the whole; (e) the chain `rle` over `rle` unpacks a twice packed input to the original, and a chain of one equals the method; `count == 0` is refused; (f) an upstream source that fails after 10 bytes: the error carries the layer's name and the upstream's words; (g) a read after a failure repeats it; (h) the sweep cases of the decided bullet.
  2. See it fail: `cmake --build build-compress --target test_compress_stream 2>&1 | tail -n 20` (undeclared names).
  3. Write the stream forms, `cf_unpack_open` and the rest, the chain. `ctest --test-dir build-compress -R "test_compress" 2>&1 | tail -n 30`; `cmake -S lib/compress` alone builds and passes.
  4. See that the test can fail: let the stream check the bound only at its end behind a run-time-false condition; case (c) fails; put it back.
  5. D-196 and D-197 extended, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-15b: streaming cf_unpacker and cf_packer, and cf_unpack_chain`.
  6. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX (the `sanitize` preset runs both compress tests); ACCEPT-WINDOWS; ACCEPT-CI; `ctest --test-dir build-compress -R test_compress` exit 0 and `cmake -S lib/compress` alone passes; the default build unchanged (`ctest --preset default`, `gate.abi`); `gate.guarantees`, `gate.oom` exit 0.
* **stop if**: Y-15a's structs cannot take the appended members without a change to a field already read (`gate.abi` for the compress list: report); a method needs a block size the interface does not carry (report which: do not add a member for it); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "`lib/compress` streams: `cf_unpack_open` and `cf_pack_open`, the bound checked as bytes flow, and `cf_unpack_chain` that composes unpackers (Y-15b, D-196, D-197)."
* **Organiser's answers**:
  * Streaming is what D-221 point 3 asks, split from Y-15a by point 7: Y-15a's structs carry `size`, so these members are appended and no caller of Y-15a is broken (the maintainer's standing approval, 2026-11-27).
  * A reader of a configuration still takes a whole document (`lib/serialize/include/configforge/cf_serialize.h`, the comment above `cf_blob`); the stream chains layers and serves programs with large inputs, and a codec reads its final output through `cf_unpack` into an arena (the maintainer's standing approval, 2026-11-27).

---

## Y-16 - gzip through zlib: CF_WITH_ZLIB

* **id**: Y-16
* **goal**: with `-DCF_WITH_ZLIB=ON`, the value `cf_gzip_unpacker` (a `cf_unpacker`, D-221) given to `cf_unpack` unpacks gzip, members in a loop, with the library's own header reading, CRC-32 and ISIZE checked, trailing bytes refused, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (reading gzip first), section 4.2 (the options) and 4.4 (licence and supply chain: zlib 1.2.13 or later, CVE-2022-37434).
* **needs**: Y-15a
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided 2 (new optional dependencies): both zlib and libzstd** (this card and Y-17), both OFF by default, ON in the `full` and `server` profiles (`CMakePresets.json:123-137`), a built-in inflate (Y-21-m, `CF_WITH_INFLATE`, OFF by default) after them. **Rejected**: zlib only; none (a built-in inflate first). Both need `CF_WITH_COMPRESS` (D-221 point 6b).
  * **The option**: `option(CF_WITH_ZLIB "gzip through zlib (requires zlib >= 1.2.13; zlib-ng in compat mode also)" OFF)` in `cmake/ConfigForgeOptions.cmake` after `CF_WITH_SQLITE` (line 23); `cmake/ConfigForgeProfile.cmake:20` lists the option for profiles (add `CF_WITH_ZLIB:BOOL` as `CF_WITH_SQLITE:BOOL` is); `lib/compress/CMakeLists.txt` finds zlib as `lib/config/CMakeLists.txt:62-68` finds sqlite3 and fails with `CF_WITH_ZLIB needs zlib: install it (zlib1g-dev, brew install zlib, vcpkg zlib)`. The profiles `minimal` and `misra` keep it OFF. `CF_WITH_ZLIB=ON` with `CF_WITH_COMPRESS=OFF` is a configure `FATAL_ERROR` (`CF_WITH_ZLIB needs CF_WITH_COMPRESS`).
  * **The floor, checked three ways** (D-… "enforce version floors", memory note, and the sqlite3 floor's pattern in `lib/config/CMakeLists.txt`): at configure time (`ZLIB_VERSION_STRING` `VERSION_LESS 1.2.13` is a `FATAL_ERROR`), at build time (a `#if ZLIB_VERNUM < 0x12d0` `#error` in `gzip.c`), at run time (`zlibVersion()` compared digit by digit in `cf_gzip_unpacker`'s `unpack`, a refusal `CF_E_CAPABILITY`, `the zlib in use is <v>; ConfigForge needs 1.2.13 or later`); zlib-ng in compat mode reports its zlib version and passes.
  * **gzip's header is read by the library's own code**, never `inflateGetHeader`: 10 fixed bytes (`1f 8b`, method 8, flags, MTIME, XFL, OS); FEXTRA (2-byte length, skipped by length), FNAME and FCOMMENT (skipped to a NUL, at most 4096 bytes: more is `unpack-corrupt`), FHCRC (2 bytes, skipped, not checked), reserved flag bits set is `unpack-corrupt`. Then raw inflate (`inflateInit2(&zs, -15)`) over the body with the output limit; then CRC-32 (zlib's `crc32`) and ISIZE (low 32 bits of the length) checked against the 8-byte trailer; another member starting with `1f 8b` is read in a loop (the limit counts all members together); other trailing bytes are `unpack-corrupt`, `trailing bytes after the last gzip member`.
  * **The backend**: `lib/compress/src/gzip.c` (new, added to the component's sources only under `CF_WITH_ZLIB`), an exported value `const cf_unpacker cf_gzip_unpacker` declared in `cf_compress.h` under `#if CF_WITH_ZLIB`, with `name "gzip"`, `extensions ".gz"`, magic `1f 8b 08`, `unpack = gzip_unpack` and its own `defaults` (D-221 point 4: the bomb guard's numbers, `CF_UNPACK_DEFAULT_*`). No packer here: `cf_gzip_packer` is another value in the same file, Y-20's. There is no registry: a program passes the value (`cf_builder_unpacker(b, &cf_gzip_unpacker)`, Y-18) and `cf_unpack_detect` looks at the list it is given. The output is allocated from the arena in steps doubling from 4 KiB up to the limit; zlib's allocator hooks are the arena's (`zalloc`, `zfree` with the arena, so no `malloc` the program does not see).
  * **Fuzz target** `tests/fuzz/unpack_gzip.c` in the form of the existing ones (`ls tests/fuzz | head`; copy the smallest), input: any bytes through `cf_unpack`, never crashing, never beyond the limit; a seed corpus of three gzip files (one good, one with FEXTRA and FNAME, one two-member).
  * **Dependencies.md**: a row in section 1 "Using the library": `zlib | zlib licence (Zlib) | CF_WITH_ZLIB | gzip; >= 1.2.13 (CVE-2022-37434); never vendored`. The SBOM generator (`configforge.spdx.json`) lists it when ON: find where sqlite3 is added (`grep -rn sqlite cmake/*.cmake scripts | head -5`) and add zlib the same way.
  * **D-198** ("gzip is read by our header code over raw inflate"): the three bullets above in one paragraph; Rejected: `inflateGetHeader` (a second parser inside zlib), a vendored zlib, `gzopen` (a file API, no bound).
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/compress/CMakeLists.txt`, `lib/compress/include/configforge/cf_compress.h`, `lib/compress/src/gzip.c` (new), `lib/compress/tests/test_gzip.c` (new), `lib/compress/tests/data/` (the gzip fixtures), `tests/fuzz/unpack_gzip.c` (new) and its CMake entry, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-16 main`.
  2. Make fixtures with the system `gzip`: `good.gz`, `extra.gz` (FEXTRA and FNAME; made by a ten-line Python script `scripts/work/` is not needed: write the bytes by hand in the test as a C array), `two.gz` (`cat a.gz b.gz`), `badcrc.gz` (one flipped CRC byte), `trunc.gz` (cut at half), `bomb.gz` (`head -c 100000000 /dev/zero | gzip -9`, 97 KB: 1000x: refused).
  3. Write `test_gzip.c` (guarded by `CF_WITH_ZLIB`; compiled to a test that prints `skipped: no zlib` otherwise): each fixture's result, the bomb's `unpack-bomb`, the bad CRC's `unpack-corrupt`, trailing garbage `trailing bytes after the last gzip member`, a header with a FNAME of 5000 bytes `unpack-corrupt`, the floor's message when `zlibVersion()` is faked (a function pointer seam `cfi_gzip_version_fn` in `gzip.c`, not a global).
  4. See it fail: `cmake -S . -B build-zlib -DCF_WITH_ZLIB=ON && cmake --build build-zlib --target test_gzip 2>&1 | tail -n 20` (no `gzip.c`, no `cf_gzip_unpacker`).
  5. Write `gzip.c` and the CMake changes. `ctest --test-dir build-zlib -R "test_gzip|test_unpack|gate" 2>&1 | tail -n 30` and the default build's `ctest --preset default` still green.
  6. See that the test can fail: skip the CRC compare behind a run-time-false condition; `badcrc.gz` passes wrongly and its case fails; put it back.
  7. Fuzz a minute: `cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` (as P-3's step 4e does), with `CF_WITH_ZLIB=ON` in the fuzz preset.
  8. D-198, Dependencies row, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-16: gzip through zlib, CF_WITH_ZLIB (D-198)`.
  9. ACCEPT; push; ACCEPT-CI, whose `full` profile now builds with zlib.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (zlib from vcpkg, or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-zlib -R test_gzip` exit 0; the fuzz minute with no finding.
* **stop if**: `gate.dependencies` refuses the find call and the fix is more than one line of its allow-list; a gzip made by a tool of the CI image does not unpack (report the bytes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZLIB`: gzip through zlib, read by the library's own header code with CRC-32 and ISIZE checked (Y-16, D-198)." `docs/Decisions.md`: D-198. Roadmap: Y-20.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: Y-16 follows D-221: the method is the value `cf_gzip_unpacker` filling the `cf_unpacker` interface in `lib/compress`, with its own default options, not an entry of a registry in `lib/serialize`; choice 2 is both zlib and libzstd, as recommended (the maintainer's standing approval, 2026-11-27).

## Y-17 - zstd through libzstd: CF_WITH_ZSTD

* **id**: Y-17
* **goal**: with `-DCF_WITH_ZSTD=ON`, the value `cf_zstd_unpacker` (a `cf_unpacker`, D-221) given to `cf_unpack` unpacks zstd frames (several frames in a loop, skippable frames skipped) with `windowLogMax` 23 and the output limit, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (zstd beside gzip), section 4.2 and 4.4 (the zstd CVEs; the decoder's own memory bound).
* **needs**: Y-16 (its option wiring and fixtures' pattern; the choice of both libraries is made there)
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **The option**: `CF_WITH_ZSTD` OFF by default, ON in `full` and `server`, wired exactly as Y-16 wires `CF_WITH_ZLIB` (it also needs `CF_WITH_COMPRESS`); the floor is the zstd release the research names in section 4.4 (read it: it is one number; the card's worker copies it into the three checks, configure, `#if ZSTD_VERSION_NUMBER < <n>` `#error`, and `ZSTD_versionNumber()` at run time).
  * **The backend** `lib/compress/src/zstd.c`: the exported value `const cf_unpacker cf_zstd_unpacker` (declared in `cf_compress.h` under `#if CF_WITH_ZSTD`; `cf_zstd_packer` is Y-20's, another value), `name "zstd"`, `extensions ".zst;.zstd"`, magic `28 b5 2f fd`, its `defaults` with `window` 8 MiB (D-221 point 4). Streaming decompression (`ZSTD_createDCtx`, `ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, <the log2 of the resolved `window` option, 23 by default: a program that raises `cf_unpack_options.window` accepts a larger window>)`, `ZSTD_decompressStream` into the arena in doubling steps up to the limit); a frame that asks for more window is refused with `unpack-bomb` and the size it asked for in the message (`'%s' (zstd) asks for a window of <n> bytes; the bound is 8388608`). Frames in a loop; a skippable frame (magic `5e 2a 4d 18` to `5f 2a 4d 18`) skipped by its length; other trailing bytes `unpack-corrupt`. The decompressed size in the frame header is never trusted: the limit is checked as bytes come out. zstd's allocator hooks are the arena's.
  * **Dictionaries are not here** (Y-23-m).
  * **Fuzz target** `tests/fuzz/unpack_zstd.c`, as Y-16's; seeds: one good frame, two frames, one with a skippable frame first.
  * **D-199** ("zstd is read with a window limit and a loop of frames"): the bullets in one paragraph. Rejected: a vendored libzstd (a large C99 surface under `misra`); dictionaries by default.
  * `docs/Dependencies.md` row: `libzstd | BSD-3-Clause (dual with GPL-2.0; the library uses the BSD one) | CF_WITH_ZSTD | zstd; never vendored`.
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/compress/CMakeLists.txt`, `lib/compress/include/configforge/cf_compress.h`, `lib/compress/src/zstd.c` (new), `lib/compress/tests/test_zstd.c` (new), `tests/fuzz/unpack_zstd.c` (new), `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-17 main` (after Y-16 landed).
  2. Fixtures with the system `zstd`: `good.zst`, `two.zst` (`cat`), `skip.zst` (a skippable frame written by hand as bytes `50 2a 4d 18 04 00 00 00 de ad be ef` then `good.zst`), `badframe.zst` (flipped byte), `bigwin.zst` (`zstd --long=27 -19` of 200 MB of a repeating text: a window above 2^23; if the host's `zstd` lacks `--long`, the fixture is skipped and the test says so), `bomb.zst` (`head -c 100000000 /dev/zero | zstd -19`).
  3. `test_zstd.c`: each fixture's result and the three refusals by reason.
  4. See it fail: `cmake -S . -B build-zstd -DCF_WITH_ZSTD=ON && cmake --build build-zstd --target test_zstd 2>&1 | tail -n 20`.
  5. Write the backend. `ctest --test-dir build-zstd -R "test_zstd|test_unpack|gate"`.
  6. See that the test can fail: set the window parameter to 27 behind a run-time-false condition; `bigwin.zst` is no longer refused; put it back.
  7. Fuzz a minute as Y-16 step 7. D-199, Dependencies, CHANGELOG. Format; codemap and apiref, last. Commit `Y-17: zstd through libzstd, CF_WITH_ZSTD (D-199)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (libzstd from vcpkg or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-zstd -R test_zstd` exit 0; a clean fuzz minute.
* **stop if**: Y-16 is not landed; `ZSTD_d_windowLogMax` is not in the installed zstd (older than the floor): the floor was wrong, report; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZSTD`: zstd frames through libzstd with a window limit (Y-17, D-199)." `docs/Decisions.md`: D-199. Roadmap: Y-20.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: Y-17 follows D-221: the value `cf_zstd_unpacker` in `lib/compress`, the window limit being the `window` option's resolved value (the maintainer's standing approval, 2026-11-27).

## Y-18 - The sources read through the layer: flags, extensions, provenance, bindings

* **id**: Y-18
* **goal**: a file source, a drop-in directory source and the image file source unpack compressed bytes before a reader sees them: the program can name the compression (`CF_COMPRESSED_GZIP`, `CF_COMPRESSED_ZSTD`) or refuse it (`CF_COMPRESSED_NONE`); otherwise the outer extension and then the magic decide; the inner extension names the format; provenance reads `app.toml.gz (gzip)`; the C++, Rust and Python bindings pass the flags.
* **why**: `docs/research/compression.md` section 5 ("The layer is called from `cfi_source_text` ..., from the image file source, and from `configforge codec`" and "Provenance and errors name both").
* **needs**: Y-15a; Y-16 or Y-17 for a real backend (the tests use a run-length test backend where neither is built)
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided 3 (detection by magic bytes when built in): (a).** On, refusable per source with `CF_COMPRESSED_NONE`: a magic is a declaration like a BOM, the bounds hold in both cases, and a program that built the backend in asked for it. **Rejected**: (b) off unless the source names a compression or the outer extension does (the safer default for a program that never expects compressed input but links a build that has one; it can pass `CF_COMPRESSED_NONE`). Detection looks at the unpackers the builder holds (`cf_builder_unpacker`) and then at the built-in values, never a global registry.
  * **Flags** (`lib/config/include/configforge/cf_source.h`, source flags; beside the encoding value `CF_ENCODING_*` which takes bits 24-27, `lib/text/include/configforge/cf_encoding.h:27-28`): `CF_COMPRESSED_SHIFT 28`, `CF_COMPRESSED_MASK (3u << 28)`, values `CF_COMPRESSED_AUTO = 0` (the default), `CF_COMPRESSED_NONE = 1u << 28` (refuse compressed bytes: `'%s' is compressed and this source takes none`, reason `unpack-corrupt`'s sibling is not needed: `CF_E_SOURCE`, no reason), `CF_COMPRESSED_GZIP = 2u << 28`, `CF_COMPRESSED_ZSTD = 3u << 28`. They are accepted where `CF_ENCODING_MASK` is: `CF_FILE_SOURCE_FLAGS` and `CF_DROPIN_SOURCE_FLAGS` (`cf_source.h:249-255`) gain `CF_COMPRESSED_MASK`; other sources refuse them by name as they refuse any flag. If bits 28-29 are taken by a flag the worker finds (`grep -n "1u << 2[89]\|<< 28\|<< 29" lib/*/include/configforge/*.h`), stop and report.
  * **Where it is called**: `cfi_source_text` (`lib/config/src/bridge/file.c:292`) gains a first step before `cfi_source_recode`: when the flags and the name say the bytes are compressed, `cf_unpack` into the source's arena (the same arena as the text), `raw` then names the unpacked bytes; the codec was chosen from the path by the caller before this call (`file_named`), so the caller (`file_load` at 421-434 and `dir.c:178`) is changed to strip one outer extension when it is a known unpacker's (`app.toml.gz` names `toml`: `cf_codec_by_path` of the path without `.gz`). After unpacking, the recoding (D-108) and the detection by content go on as today. Nested compression is refused by Y-15's `nested-compression`.
  * **Provenance**: the source's `path` stays `app.toml.gz`; `cf_explain` and Doctor name `app.toml.gz (gzip)`: `file_state` gains `const char* unpacked` (the unpacker's name or NULL), shown by the code that shows `st->encoding` today (find with `grep -n "st->encoding" lib/config/src/bridge/file.c`); an error line and column are "in the decompressed text": the message is `app.toml.gz (gzip):3:5: ...` using the provenance's text as the file name for the reader's errors.
  * **Digest and fingerprint** are of the uncompressed bytes (the text the reader reads), so a file recompressed at another level has the same fingerprint.
  * **The image file source** (`cf_image_open` or the function that reads an image file: `grep -n "image" docs/Codemap.md | head`): the same call before the image header is read. The image bytes keep their own layout (nothing inside the format is compressed, research "Not to build").
  * **Bindings**: the C++, Rust and Python wrappers each pass the three new flag values where they pass `CF_ENCODING_*` (`grep -rn "CF_ENCODING_UTF16LE" bindings | head`); one test each that a flag reaches the call (with Y-15's backend registered through the C API, or skipped when no backend is built).
  * **D-200** ("A compressed file is read through the layer, by name, extension or magic"): the flags, the order (program's flag, outer extension, magic), `CF_COMPRESSED_NONE`, the digest of the uncompressed bytes, one layer.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/dir.c`, the image file source (named by the grep above), `bindings/cpp/`, `bindings/rust/`, `bindings/python/` (the files the grep above names), `lib/config/tests/test_compressed.c` (new; add to `lib/config/tests/CMakeLists.txt` after `test_file`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 8's "Compressed input", replacing Y-14's recipe-only text with both), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-18 main` (after Y-15a).
  2. Write `test_compressed.c` with Y-15's `rle` backend registered through `cf_builder_unpacker`: (a) `cf_ct.toml.rl` read by extension, `cf_explain` names `cf_ct.toml.rl (rle)`; (b) a file with no compression extension but the `RL` magic: read by magic (choice 3a); (c) `CF_COMPRESSED_NONE` on the same file: refused with `is compressed and this source takes none`; (d) `CF_COMPRESSED_GZIP` on a file whose bytes are plain text: refused `is not gzip` (the flag names a compression, the bytes must be it); (e) a syntax error on line 2 of the unpacked text: the message names `cf_ct.toml.rl (rle):2:`; (f) the fingerprint of the unpacked file equals that of the same text in `cf_ct.toml`; (g) a drop-in directory holding `10-a.toml.rl` and `20-b.toml`: both read; (h) nested compression refused; (i) a flag the other sources do not know (`cf_builder_env_document` with `CF_COMPRESSED_GZIP`) is refused by name.
  3. See it fail: build `test_compressed` (the flags are undeclared).
  4. Write the flags, the call in `cfi_source_text`, the outer-extension strip, the provenance, the image source and the bindings. `ctest --preset default -R "test_compressed|test_file|test_dir|test_image|binding" 2>&1 | tail -n 30` (list the binding tests with `ctest --preset default -N | grep -i "py\|rust\|cpp"`).
  5. See that the test can fail: skip the unpack call behind a run-time-false condition; (a) fails; put it back.
  6. D-200, Guide, CHANGELOG. Format; codemap and apiref, last. Commit `Y-18: the file, directory and image sources read compressed bytes through the layer (D-200)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_compressed` exit 0 in `default` and `minimal`; with `-DCF_WITH_ZLIB=ON` a gzip case of the same test also passes.
* **stop if**: bits 28 and 29 are taken; the outer-extension strip changes a result of `test_file` or `test_dir` (a file named `x.json.gz` today is refused or read by content: say which); a binding gate asks for more than the flag values; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "File, directory and image sources read gzip and zstd files by flag, outer extension or magic; provenance names `app.toml.gz (gzip)` (Y-18, D-200)." `docs/Decisions.md`: D-200. Roadmap: Y-20.
* **Organiser's answers**:
  * Y-15a splits the layer's work: the component `lib/compress` has no registry and no builder call, so this card adds `cf_builder_unpacker(b, const cf_unpacker*)` to `lib/config` under `#if CF_WITH_COMPRESS` (the builder holds the program's methods in a growing list, no fixed count, and looks at them before any built-in one) and the refusal reason `CF_REASON_NESTED` (`"nested-compression"`, "compressed bytes inside compressed bytes", message `'<name>' unpacks to compressed bytes again; one layer only`), which the source layer applies with `cf_unpack_detect` over the builder's list. Y-15a's `cf_unpack` takes options and its test backend is the test's own `rle`; where this card's text says `cf_unpack(u, in, ar, out, err)`, the call is Y-15a's (the maintainer's standing approval, 2026-11-27).

---
  * 2026-11-27, the organiser: Y-18 follows D-221 (the sources call `cf_unpack` and `cf_unpack_detect` of `lib/compress` over the builder's list of `cf_unpacker` values) and choice 3 is (a) (the maintainer's standing approval, 2026-11-27).

## Y-19 - `configforge codec` through the layer, and a bomb corpus

* **id**: Y-19
* **goal**: `configforge codec detect`, `check` and `dump` read compressed files through the layer (`detect` says "gzip, then TOML by the extension"), and the test corpus holds the classic bombs.
* **why**: `docs/research/compression.md` section 5 ("`configforge codec` ... `detect` says ...") and section 6; the corpus is what holds the bounds against regressions.
* **needs**: Y-18
* **size**: small
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Where**: `tools/codec/codec.c` (`detect` at line 76; find `check` and `dump` by name). Each calls `cf_unpack_detect` on the input as the source does, over the list of the built-in values the build has (`&cf_gzip_unpacker` under `CF_WITH_ZLIB`, `&cf_zstd_unpacker` under `CF_WITH_ZSTD`; the tool has no builder to hold a program's own); when it answers, `detect` prints `gzip, then TOML by the extension` (or `by the content`), `check` and `dump` read the unpacked bytes.
  * **A flag `--no-unpack`** on the three commands refuses compressed input: it sets `CF_COMPRESSED_NONE` as the sources do. No other flag.
  * **The corpus** `tests/conformance/bombs/` (new): `gzip-1000x.gz` (a 98 KB file making 100 MB), `gzip-nested.gz` (a gzip of a gzip), `gzip-truncated.gz`, `gzip-bad-crc.gz`, `zstd-window.zst` and `zstd-1000x.zst` (made with `zstd`; only when `CF_WITH_ZSTD`), each with a `.expect` line: the reason. A test `test_bombs` runs each through `cf_unpack` and compares the reason; a backend not built skips its files with a line saying so.
* **files**: `tools/codec/codec.c`, `tools/codec/tests/` (the codec tool's tests: find with `ls tools/codec`), `tests/conformance/bombs/*` (new), `tests/conformance/CMakeLists.txt` or the place the existing corpus is wired; last: `CHANGELOG.md`, `docs/Tools.md` (the codec page's three commands).
* **steps**:
  1. `git switch -c work/Y-19 main` (after Y-18).
  2. Write the tests first: `detect` of `app.toml.gz` prints the line above; `check` of the same exits 0; `check --no-unpack` exits non-zero with `takes none`; each corpus file's expectation.
  3. See it fail: run the codec tool's tests (`ctest --preset default -R "codec|bombs" 2>&1 | tail -n 20`).
  4. Write the tool's change and the corpus wiring. The tests pass in the build with zlib and in the default build (skipping).
  5. See that the test can fail: change one `.expect` reason, see `test_bombs` fail, put it back.
  6. CHANGELOG, `docs/Tools.md`. Format; codemap and apiref, last. Commit `Y-19: configforge codec reads compressed files; a corpus of bombs`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "codec|bombs"` exit 0.
* **stop if**: Y-18 is not landed; the codec tool links no part of `lib/config` and cannot reach `cf_unpack_detect` (report: it is `lib/serialize`'s, so it should); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`configforge codec detect`, `check` and `dump` read compressed files; a corpus of decompression bombs (Y-19)." No D-number. Roadmap: Y-20.

---

## Y-20 - Writing: cf_pack, deterministic gzip and zstd, `convert --compress`, a compressed image

* **id**: Y-20
* **goal**: `cf_pack` writes gzip and zstd over a finished buffer, byte for byte the same on every run (gzip with MTIME 0 and OS 255, a fixed level), `configforge codec convert --compress gzip|zstd` and `cf_image_write` use it, the digest and the provenance stay those of the uncompressed bytes, and the whole-or-nothing write of wave 6 writes the compressed bytes like any other.
* **why**: `docs/research/compression.md` section 5, "After: writing" (option C), and section 6, "Writing" (2.2.0). It marks the Roadmap item "compression" done.
* **needs**: Y-16, Y-17, Y-19, F-1b (the public file write)
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **gzip** (`lib/compress/src/gzip.c`, Y-16): the `cf_packer` value `cf_gzip_packer` (beside `cf_gzip_unpacker`, D-221: two interfaces, two values; declared under `#if CF_WITH_ZLIB`) has a `pack` that sets `deflateInit2(&zs, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)` and writes the 10-byte header itself (`1f 8b 08 00 00 00 00 00 <xfl> ff`: no name, MTIME 0, OS 255, XFL 2 for level 9, 4 for level 1, else 0), the deflate body, then CRC-32 and ISIZE; the default level is 6 (`cf_pack_options.level` at `CF_COMPRESS_LEVEL_DEFAULT`, -1, takes the packer's own `defaults.level`, 6; `1` to `9` are zlib's). Deterministic: the same input and level give the same bytes on any platform with zlib 1.2.13 or later and zlib-ng does not (zlib-ng's deflate output differs): the page and D-201 say determinism holds for one zlib implementation, and the test compares with a fixture only when `ZLIB_VERSION` is not zlib-ng's (`#ifdef ZLIBNG_VERSION`).
  * **zstd** (`cf_zstd_packer`, `lib/compress/src/zstd.c`, Y-17): `ZSTD_compressStream2` with `ZSTD_c_compressionLevel` (default 3; `level` 1 to 19), `ZSTD_c_checksumFlag` 1, `ZSTD_c_contentSizeFlag` 1, `ZSTD_c_nbWorkers` 0 (a thread count changes nothing in the bytes, but 0 keeps the library thread-free, D-14), no dictionary; one frame.
  * **The measure-or-write form**: `cf_pack` first emits to a sink that measures (`cf_sink`, as `cf_emit` does), then the caller allocates and calls again; the backend's `pack` supports it by running its encoder twice (research section 5: "Measure-or-write, like cf_sink").
  * **`configforge codec convert --compress gzip|zstd`** (`tools/codec/codec.c`): the output is the converted text packed; without the flag, an output named `.gz` or `.zst` packs by its outer extension and says so on stderr (`packing as gzip by the extension`).
  * **`cf_image_write`** (find with `grep -n cf_image_write docs/Codemap.md`): a path ending in a known outer extension is packed; `ImageFormat.md` gains one paragraph that the image's header and sections are those of the uncompressed bytes.
  * **The digest and fingerprint** are of the uncompressed bytes (Y-18), so a recompressed file has the same.
  * **Writing through F-1b's call** (`cf_emit_file`): an output path with a known outer extension is packed before the platform write; the file is replaced whole or not at all as any other.
  * **D-201** ("Compressed output is deterministic for one library and level"): the bullets above in one paragraph; Rejected: a name and a timestamp in the gzip header; threads in zstd; compression inside the image format.
* **files**: `lib/compress/src/gzip.c`, `lib/compress/src/zstd.c`, `lib/config/src/bridge/emit.c` (`cf_emit_file`), the image writer, `tools/codec/codec.c`, `lib/compress/tests/test_pack.c` (new), `tools/codec/tests/`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/ImageFormat.md`, `docs/Guide.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-20 main` (after the cards above).
  2. `test_pack.c`: (a) pack then `cf_unpack` returns the input for sizes 0, 1, 4096 and 3 MiB, both backends; (b) two packs of the same input and level are equal byte for byte; (c) a gzip pack has bytes 4 to 7 (MTIME) zero and byte 9 `ff` (the system `gzip -dc` accepts it: `system("gzip -dc cf_pk.gz > cf_pk.out")`, skipped when `system` fails); (d) the measure call returns the exact size the write needs; (e) the fingerprint of a configuration read from `cf_pk.toml.gz` equals the one read from `cf_pk.toml`; (f) `convert --compress gzip` of a JSON file gives a file `gzip -dc` turns back into the TOML text; (g) `cf_emit_file` to `x.toml.gz` reads back through Y-18.
  3. See it fail: build `test_pack` (`cf_gzip_packer` does not exist yet).
  4. Write the packers and the three callers. `ctest --test-dir build-zlib ...` and `build-zstd` as before.
  5. See that the test can fail: write MTIME from a clock behind a run-time-false condition; (b) fails; put it back.
  6. D-201, docs, CHANGELOG, the Roadmap row (🟩 "compression"). Format; codemap and apiref, last. Commit `Y-20: cf_pack, deterministic gzip and zstd output (D-201)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX with `-DCF_WITH_ZLIB=ON -DCF_WITH_ZSTD=ON`; ACCEPT-WINDOWS; ACCEPT-CI; `ctest -R "test_pack|codec"` exit 0.
* **stop if**: F-1b or Y-19 is not landed; two packs differ on the same machine (a nondeterminism in the backend: report where); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_pack`, `configforge codec convert --compress` and compressed images: gzip and zstd written deterministically (Y-20, D-201)." `docs/Decisions.md`: D-201. `docs/Roadmap.md`: the item "Compression" under "From the maintainer, 2026-11-26" 🟩.

---

## Y-21-m - A built-in inflate for the embedded presets

* **id**: Y-21-m
* **goal**: a `CF_WITH_INFLATE` option adds a gzip reader with no dependency, written in C89 under the `misra` rules, differentially fuzzed against zlib, for the embedded presets that want a compressed image.
* **why**: `docs/research/compression.md` section 5, "Later" (option D) and section 4.5 (embedded and minimal profiles).
* **needs**: Y-16, Y-20
* **size**: large
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided (whether to build it): build**, a small inflate (about 400 lines, as `tinfl` or `puff.c` shows; research section 4.5) behind `option(CF_WITH_INFLATE "gzip reading with no dependency (built-in inflate)" OFF)`, OFF by default and in every profile; a build turns it on. **Rejected:** never (an embedded target that wants a compressed image would have no way without zlib); only the program's own inflate through Y-14's recipe (stays the way for a program that has zlib; it does not help one that cannot link it).
  * **The shape**: `lib/compress/src/inflate.c`, one function of the `cf_unpacker` interface under the name `"gzip"` when `CF_WITH_ZLIB` is OFF and `CF_WITH_INFLATE` is ON (never both: a configure error `CF_WITH_INFLATE and CF_WITH_ZLIB both provide gzip`); reads only; the bounds of Y-15 and the header/trailer code of Y-16 are shared (`gzip_header.c`, moved out of `gzip.c` by the card, with no behaviour change); fixed and dynamic Huffman blocks, stored blocks, a 32 KiB window in the arena; no allocation outside the arena; a differential fuzz target `tests/fuzz/inflate_diff.c` comparing against zlib on every input (the `yamldiff` pattern: `tests/fuzz/yamldiff*.c`), required to find no difference in an hour.
  * Errors (D-223): as Y-16's gzip (`unpack-corrupt`, `unpack-bomb` with the source and the offset); an inflate error names the block: `'%s': bad Huffman code in block %u`.
  * **D-202** ("A built-in inflate, OFF by default, never beside zlib"): the bullets above; Rejected: never; the program's own inflate only.
* **files**: `lib/compress/src/inflate.c` (new), `lib/compress/src/gzip_header.c` (new, moved out of `gzip.c`), `lib/compress/src/gzip.c`, `lib/compress/CMakeLists.txt`, `lib/compress/tests/test_inflate.c` (new), `tests/fuzz/inflate_diff.c` (new) and its CMake entry, `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-202), `docs/Dependencies.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-21-m main` (after Y-16 and Y-20). Move the gzip header and trailer code out of `gzip.c` into `gzip_header.c` with no behaviour change; `ctest --test-dir build-zlib -R "test_gzip|test_unpack"` unchanged (Y-16's build directory).
  2. Test first: `test_inflate.c` (compiled under `CF_WITH_INFLATE`, otherwise prints `skipped: no inflate`), on Y-16's fixtures (`lib/compress/tests/data/`): the same results as `test_gzip`, plus a stored block, a fixed-Huffman block and a dynamic one written as C arrays, and a distance past the window refused; the configure error when both options are ON (a CMake test with `WILL_FAIL`).
  3. See it fail: `cmake -S . -B build-inflate -DCF_WITH_INFLATE=ON && cmake --build build-inflate --target test_inflate 2>&1 | tail -n 20`.
  4. Write `inflate.c` and the option; `ctest --test-dir build-inflate -R "test_inflate|test_unpack|gate" 2>&1 | tail -n 30`; the `misra` preset builds with it ON (`cmake --preset misra -DCF_WITH_INFLATE=ON` and build).
  5. See that the test can fail: skip the CRC compare behind a run-time-false condition; the bad-CRC case fails; put it back.
  6. The differential fuzz target `inflate_diff.c` (needs both inflate and zlib in one fuzz build: it links zlib directly as the oracle, not through `CF_WITH_ZLIB`); run it an hour: `cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=3600 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` as Y-16 step 7, with `CF_WITH_INFLATE=ON`.
  7. D-202, Dependencies (no new dependency: one line saying so), CHANGELOG. `python scripts/work/work.py format`; codemap last. Commit `Y-21-m: a built-in inflate, CF_WITH_INFLATE (D-202)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --test-dir build-inflate -R test_inflate` exit 0; an hour of the differential fuzz without a difference.
* **stop if**: the inflate cannot be held to the `misra` rules without a deviation that `docs/Compliance.md` does not already list (report the rule); the differential fuzz finds a difference that is zlib's leniency, not a bug (report the input); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "`CF_WITH_INFLATE`: gzip read with no dependency, OFF by default (Y-21-m, D-202)"; `docs/Decisions.md`: D-202.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Build it, in its smallest form: reading only, behind `CF_WITH_INFLATE`, OFF by default and never beside `CF_WITH_ZLIB`; version 2.3.0, the version of Y-16 and Y-20 it needs (the maintainer's standing approval, 2026-11-27).

---

## Y-22-m - xz and lz4 frames

* **id**: Y-22-m
* **goal**: `CF_WITH_LZMA` (xz through liblzma, refusing 5.6.0 and 5.6.1, with a `memlimit` of the order of zstd's window) and `CF_WITH_LZ4` (lz4 frames) as backends of the `cf_unpacker` interface, each OFF by default.
* **why**: `docs/research/compression.md` section 5, "Later" (E) and section 4.4 (the xz backdoor of 2024 and its refusal).
* **needs**: Y-17
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided (whether to build, and which): both, xz first**, each its own option OFF by default and in every profile but `full`: `CF_WITH_LZMA` (liblzma) and `CF_WITH_LZ4` (liblz4); two commits on the one branch, xz then lz4. Configuration archives from distributions are xz; lz4 is for a program whose own files are lz4. **Rejected:** xz only or lz4 only (the second is the same pattern at little more cost, and the card is opened once); neither (leaves a common archive format unread).
  * **The shape**: as Y-17: `lib/compress/src/xz.c` (`name "xz"`, `.xz`, magic `fd 37 7a 58 5a 00`, `lzma_stream_decoder` with a `memlimit` whose default is 8 MiB (D-220: `CF_UNPACK_DEFAULT_*` in `cf_compress.h` beside the others, an option of the backend's `defaults`), `LZMA_CONCATENATED`, `lzma_version_number()` refusing `50060002` to `50060012`, i.e. 5.6.0 and 5.6.1, at run time as well as at configure time: `liblzma 5.6.0 and 5.6.1 are refused (CVE-2024-3094)`) and `lib/compress/src/lz4.c` (`name "lz4"`, `.lz4`, magic `04 22 4d 18`, `LZ4F_decompress` with a frame-size check against the bomb guard); fuzz target and fixtures for each; Dependencies rows. Errors (D-223) and refusal reasons as Y-17's.
  * **D-203** ("xz and lz4 frames, each OFF by default; the backdoored liblzma refused"): the bullets above; Rejected: one of the two only; neither.
* **files**: `lib/compress/src/xz.c` (new), `lib/compress/src/lz4.c` (new), `lib/compress/include/configforge/cf_compress.h`, `lib/compress/CMakeLists.txt`, `lib/compress/tests/test_xz.c` and `test_lz4.c` (new), `tests/fuzz/unpack_xz.c` and `unpack_lz4.c` (new), `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-203), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-22-m main` (after Y-17).
  2. xz, on Y-17's steps 2 to 7: fixtures with the system `xz` (`good.xz`, `two.xz` by `cat`, `badcrc.xz`, `trunc.xz`, `bomb.xz`, and one needing more than the `memlimit`); `test_xz.c` (skipped without the option); see it fail with `-DCF_WITH_LZMA=ON` in `build-xz`; write `xz.c` and the option; see the test can fail (skip the version refusal behind a run-time-false condition and a faked `lzma_version_number` seam returning `50060002` passes wrongly); fuzz a minute. Commit `Y-22-m: xz through liblzma, CF_WITH_LZMA`.
  3. lz4, the same steps with the system `lz4` in `build-lz4` (`good.lz4`, `two.lz4`, `badframe.lz4`, `bomb.lz4`); see it fail; write `lz4.c`; see the test can fail (skip the frame-size check; the bomb passes); fuzz a minute. Commit `Y-22-m: lz4 frames through liblz4, CF_WITH_LZ4`.
  4. D-203, Dependencies rows, CHANGELOG. Format; codemap and apiref, last; amend into the last commit.
  5. ACCEPT; push; ACCEPT-CI, whose `full` profile now builds both.
* **accept**: as Y-17, for each backend: ACCEPT-LINUX; ACCEPT-WINDOWS (from vcpkg, or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-xz -R test_xz` and `ctest --test-dir build-lz4 -R test_lz4` exit 0; a clean fuzz minute each.
* **stop if**: the installed liblzma is 5.6.0 or 5.6.1 (the refusal fires: the card stops and says so); `gate.dependencies` refuses a find call and the fix is more than one line of its allow-list; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "`CF_WITH_LZMA` and `CF_WITH_LZ4`: xz and lz4 frames read, each OFF by default (Y-22-m, D-203)"; `docs/Decisions.md`: D-203.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Both backends, xz first, each OFF by default; version 2.3.0, Y-17's (the maintainer's standing approval, 2026-11-27).

---

## Y-23-m - zstd dictionaries on a source

* **id**: Y-23-m
* **goal**: a source can name a zstd dictionary (through `cf_unpacker`'s `data` field) so that small files compressed with one are read.
* **why**: `docs/research/compression.md` section 5, "Later" (F) and section 4.6 (dictionaries).
* **needs**: Y-18, Y-20
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided (the interface): the program's own `cf_unpacker` whose `data` is the dictionary**: no new public name; the program copies the built-in `cf_zstd_unpacker` and sets its `data` to the dictionary's bytes. **Rejected:** a builder call `cf_builder_dictionary(b, source, bytes, n)` (a new public name for what the descriptor already carries); never (small files compressed with a dictionary stay unreadable).
  * **The shape**: `lib/compress/src/zstd.c` reads `self->data` as a dictionary (`ZSTD_DCtx_loadDictionary`), refuses a dictionary above 1 MiB (`CF_UNPACK_DICT_MAX`), and the frame's dictionary id is checked against the dictionary's (`'%s' needs dictionary <id>`); a frame that names a dictionary with none given is refused. The 1 MiB is a default (D-220): a member of the zstd backend's `defaults`, 0 unlimited. Errors (D-223): `unpack-corrupt` with the source and the message above.
  * **D-204** ("A zstd dictionary is the descriptor's `data`"): the bullets above; Rejected: a builder call; never.
* **files**: `lib/compress/src/zstd.c`, `lib/compress/include/configforge/cf_compress.h` (the constant), `lib/compress/tests/test_zstd.c`, `lib/compress/tests/data/` (a dictionary and two files), `docs/Guide.md` (the "Compressed input" section, Y-14's: `grep -n "Compressed input" docs/Guide.md`); last: `CHANGELOG.md`, `docs/Decisions.md` (D-204), `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/Y-23-m main` (after Y-18 and Y-20).
  2. Fixtures with the system `zstd`: `zstd --train` a dictionary from twenty small TOML files (`dict.bin`, committed), `dict.zst` (a file compressed with it), `otherdict.zst` (with a second dictionary).
  3. Tests in `test_zstd.c` (under `CF_WITH_ZSTD`): a copy of `cf_zstd_unpacker` with `data` = `dict.bin` reads `dict.zst`; without `data` it is refused `needs dictionary <id>`; `otherdict.zst` with `dict.bin` is refused by id; a 2 MiB dictionary is refused, and read with the option at 0; the descriptor without `data` reads every Y-17 fixture as before.
  4. See it fail: `cmake --build build-zstd --target test_zstd && ctest --test-dir build-zstd -R test_zstd 2>&1 | tail -n 20`.
  5. Write the dictionary load in `zstd.c`. `ctest --test-dir build-zstd -R "test_zstd|test_unpack|gate"`.
  6. See that the test can fail: skip the id compare behind a run-time-false condition; the wrong-dictionary case fails; put it back.
  7. The Guide paragraph, D-204, CHANGELOG. Format; codemap last. Commit `Y-23-m: zstd dictionaries through the descriptor's data (D-204)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (libzstd from vcpkg or skipped and said so); ACCEPT-CI; `ctest --test-dir build-zstd -R test_zstd` exit 0.
* **stop if**: the installed libzstd lacks `ZSTD_getDictID_fromFrame` (older than Y-17's floor: report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "zstd dictionaries, through the descriptor's `data` (Y-23-m, D-204)"; `docs/Decisions.md`: D-204.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". The first option, the card's recommendation: no new public name; version 2.3.0, Y-18's and Y-20's (the maintainer's standing approval, 2026-11-27).

---

## Y-24 - `configforge gen build`: sources by rank, a contract, a target, deterministic output

* **id**: Y-24
* **goal**: `configforge gen build --schema S --source <file>=<rank> ... [--runtime env,cli] [--profile P] --target image|report|<format> [--out F]` reads the sources in rank order, builds the configuration through the library, and writes the target (`image` through `cf_image_write`, `report` through `cf_report`, a format name through `cf_emit`) to `--out` or stdout; the same inputs give the same bytes; exit 0 built, 1 a diagnostic of severity error (nothing written), 2 unusable input.
* **why**: `docs/research/config-compiler.md` section 5.1 item 1 and section 4.2 B and C: pieces that exist (`cf_image_write`, `cf_emit`, `cf_report`) become one command, which is the compiler the maintainer asked for. It is also the moment to stop the image freezing defaults (Y-26).
* **needs**: Y-34 (it shares the front half of `gen check`: the same function builds the configuration)
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided A (where the compiler lives)**, research section 5.3 A: (1). Options: **(1)** `configforge gen build`, a subcommand beside `gen check` and `gen schema`, with D-104's `gen --bake` spelled as its `--target c` (Y-27); **(2)** a fourth program `configforge build`, which reopens D-99's three programs; **(3)** only `gen --bake` as D-104 wrote it, the image left to C calls. **Rejected**: (2) a fourth program (it reopens D-99's three programs); (3) only `gen --bake` (the image left to C calls, and Y-25, Y-26 and Y-28 to Y-31 with it).
  * The research's other questions are decided where they belong: **E (the manifest's place)** is Y-29's; **F (defaults in the image)** is Y-26's; **B (a build file)** is Y-32-m's; **C (diagnostics)** is Y-28's; **D (rules across keys)** is Y-30-m's.
  * **Where.** A new `tools/gen/build.c` (`int gen_build(int argc, char** argv)`, declared in `tools/gen/gen.h`), listed in `tools/gen/CMakeLists.txt`; `gen.c`'s `main` (line 42-60) gets `if (argc >= 2 && strcmp(argv[1], "build") == 0) return gen_build(argc - 2, argv + 2);` before the `check` branch; `usage()` (gen.c:22-40) gains the `build` line. The front half is shared with `gen_check` (`tools/gen/contract.c:50-100`): the code that makes a builder from a schema and the files moves into one function `static cf_builder* gen_builder(const char* schema, int nsrc, const gen_source* src, cf_error* err)` in `tools/gen/gen.h`'s companion `common.c` (new; both commands call it), a change of structure with no behaviour change: `ctest -R gen_check` (Y-34's tests) must pass unchanged after step 4.
  * **Options.** `--schema <file>` once (required). `--source <file>=<rank>` repeatable: the rank is a name of `cf_define.h:345-348`'s ranks (`sys-file`, `user-file`, `env`, `cli`, ...: read the enum and accept exactly its names with `-` for `_`, lowercase; an unknown name is exit 2, `unknown rank '<x>'; ranks are <list>`); sources are applied in the order of their rank (the library does that: the command takes them in the order given). `--runtime <ranks>` (comma-separated, from the same names): the ranks the program will still have at run time, used only by Y-25's checks; accepted and stored here, `unused` until Y-25. `--profile <name>`: passed to `cf_report_info.profile` and, when the contract has profile-dependent definitions, to the builder as the profile (find the builder call with `grep -n "cf_builder_profile" lib/config/include/configforge/*.h`; if there is none the option only labels the report). `--target <t>`: `image`, `report`, or the name of a codec that writes (`cf_codec_by_name`, with `CF_CODEC_WRITE`); anything else is exit 2, `unknown target '<t>'; targets are image, report and the formats that can be written: <list>`. `--out <file>` (a path, or `-` for stdout; the default is stdout for `report` and a format and a refusal for `image` without `--out`: `an image is binary; give --out`). Written whole or not at all (`cf_platform_write_file`, F-1a).
  * **Targets**: `image` calls `cf_image_write(ks, sv, flags=0, buf, cap)` measure-then-write (`cf_image.h:54`; the key space and source view come from the built configuration: find the accessor with `grep -n "cf_config_keyspace\|cf_config_view" lib/config/include/configforge/*.h`; if none is public, the tool uses the internal tier (`CF_INTERNAL_API`) as `tools/doctor` does for reports); `report` calls `cf_report` with `CF_REPORT_BLIND`; a format calls `cf_emit(c, format, CF_EMIT_ALL, 0, ...)` as `cf_emit_file` does.
  * **Deterministic**: no clock, no host name, no environment read for the output (a `--source` of kind `env` is not offered: environment is a run-time rank, `--runtime env`). The report's `generated` is NULL. Two runs give identical bytes: a test compares them.
  * **Exit codes** as `gen check`: 0; 1 when the build refuses (`cf_builder_create` returns NULL: the first error is printed as `<file>:<line>:<col>: <message>` the way `gen check` prints it, `printf("%s\n", err.message)`), nothing written; 2 unusable input (a schema or file unreadable, a bad option, an unwritable `--out`).
  * **D-205** ("The compiler is `gen build`, and it is deterministic"): A's (1) with the shape above; the targets are those that exist; `--target c` is Y-27's. Rejected: a fourth program; a configuration language of our own (research 4.2 A; [38]); text templates (section 3, item 4); unification in place of ranks (it removes the override every program needs from its command line); planning against live state; an input-addressed store; a plug-in protocol for targets beyond D-110's.
* **files**: `tools/gen/build.c` (new), `tools/gen/common.c` (new), `tools/gen/gen.c`, `tools/gen/gen.h`, `tools/gen/contract.c`, `tools/gen/CMakeLists.txt`, `tests/gen_build/*` (new fixtures), `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-24 main` (after Y-34).
  2. Fixtures: Y-34's schema; `defaults.toml` (all keys), `site.toml` (overrides `server.port`), `bad.toml` (`server.port = 70000`).
  3. Tests: (a) `gen build --schema S --source defaults.toml=sys-file --source site.toml=user-file --target toml --out out.toml` exits 0 and `out.toml` holds `port = 9000` from site (the override wins by rank, in either order of the options); (b) `--target report` writes a blind report that `doctor lint` accepts and that names `site.toml` for `server.port`; (c) `--target image --out out.cfgb` then a C test program (`tests/gen_build/load_image.c`, built into the suite) opens it with `cf_image_open` and reads `server.port` = 9000, and `explain` names the source when the image was written with provenance (Y-26 adds the flag; here only the value); (d) two runs, `cmp` equal, for each of the three targets; (e) `bad.toml` exits 1 with `server.port` in the message and no output file; (f) an unknown rank, an unknown target, a missing `--schema`, `--target image` with no `--out`: each exit 2 with its message; (g) the compiled image's values equal those of a configuration built by `cf_build` with the same files in the same program (`load_image.c` builds both and compares every key: "compared with layer 0").
  4. Move the common front half; run `ctest --preset default -R gen_check` unchanged. See the new tests fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20` (no `build` subcommand).
  5. Write `build.c`. `ctest --preset default -R "gen_build|gen_check|image"`.
  6. See that the test can fail: let a later source lose by order of options behind a run-time-false condition; (a) fails; put it back.
  7. D-205, `docs/Tools.md`, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-24: configforge gen build (D-205)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_build|gen_check"` exit 0.
* **stop if**: the key space and source view are not reachable by the tool without a new public function (report which: it is a new public interface and the maintainer's); `gen` links only part of the library and cannot reach `cf_image_write` (report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`configforge gen build`: sources by rank and a contract to an image, a report or a written format, deterministically (Y-24, D-205)." `docs/Decisions.md`: D-205. `docs/Roadmap.md`: "A configuration compiler" under "From the maintainer, 2026-11-26" stays 🟨 until Y-31.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I accept your recommendations"): the choice A is **(1)**: `configforge gen build`, a subcommand of `gen`, no fourth program; `gen --bake` becomes its C target (Y-27). Sources of mixed formats (`base.yaml`, `prod.toml`, `secrets.json`, the environment, the command line) are merged by rank under one contract, with provenance across formats: the Guide and the feature page present it as "mixed-format compilation", and a step of the card checks every claim about CUE, Pkl, Jsonnet, Dhall and dasel against their own documentation before any comparison is written. Extensions follow D-228. The card follows this (2026-11-27).
  * 2026-11-27, the organiser: the choice A is (1), as the maintainer answered (the maintainer's standing approval, 2026-11-27).

## Y-25 - The open-key checks: what the compiler can prove about the keys left to run time

* **id**: Y-25
* **goal**: `gen build --runtime <ranks>` computes, for each key, which run-time ranks may still set it, and refuses (exit 1) a required key no source supplies and no run-time rank may supply, and a template whose referenced key is open; it lists the keys no run-time rank may change as sealed.
* **why**: `docs/research/config-compiler.md` section 4.2 E and section 6, second row: the compiler proves the contract can be met, which no other tool the research names does for a layered configuration.
* **needs**: Y-24
* **size**: small
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Where**: `tools/gen/build.c`, a function `gen_open_keys(const cf_defset*, const char* runtime_ranks, diag*)` after the build. The inputs: each definition's `policy` (`cf_def.policy`, `lib/config/include/configforge/cf_define.h:345-348`: which ranks may set the key) and the rank names Y-24 parsed from `--runtime`.
  * **The three checks** (research 4.2 E): (1) a key that is required, has no value from the build-time sources, and whose policy allows none of the `--runtime` ranks: error `'<key>' is required, no source gives it and no run-time source (<ranks>) may: <key> can never be set`; (2) a template (`${other.key}` interpolation, as the library's reference feature) whose referenced key is open (no build-time value and a run-time rank may set it): error naming both keys, `'<key>' refers to '<other>', which is set at run time; a template cannot be computed at build time`; (3) a key whose build-time value no run-time rank may change: sealed, listed by Y-29's manifest and by `--target report`'s note `sealed: <key>` (a `notes` member of the report? no: the list goes only to the manifest and, without it, to stderr as `sealed: <key>` lines when `--verbose` is given; the option `--verbose` is added here).
  * Without `--runtime` none of the checks run (nothing is known to be open): a note `no --runtime given; open keys not checked` on stderr. Each check's severity is error; Y-28 turns them into `cf_diag` records.
  * **D-206** ("The compiler proves what the contract can be met with"): the three checks, the sealed list, and that they use the policy and the rank names only. Rejected: running the program; guessing ranks.
* **files**: `tools/gen/build.c`, `tests/gen_build/`, `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-25 main` (after Y-24).
  2. Fixtures: a contract with `server.port` required and policy allowing `cli` and `env` only; `tls.cert` required with policy `user-file` only; a template `url = "http://${server.host}:${server.port}"`.
  3. Tests: (a) `--runtime env,cli` with `server.port` not in the files: passes check (1); (b) `--runtime env` and `server.port` policy `cli` only: exit 1, message as above; (c) the template over the open `server.port`: exit 1 naming both keys; (d) the template over a key set in a file: passes; (e) no `--runtime`: the note and no check; (f) `--verbose` lists the sealed keys.
  4. See it fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20`.
  5. Write the checks. `ctest --preset default -R gen_build`.
  6. See that the test can fail: skip check (1) behind a run-time-false condition; (b) fails; put it back.
  7. D-206, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-25: gen build proves the open keys (D-206)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R gen_build` exit 0.
* **stop if**: the policy of a definition cannot be read by the tool (it is not in the public `cf_defset` accessors: report the accessor needed); the library has no templates (then check (2) is dropped and the card says so under notes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`gen build --runtime`: required keys proved to be settable, templates over open keys refused, sealed keys listed (Y-25, D-206)." `docs/Decisions.md`: D-206.

---

## Y-26 - `CF_IMAGE_CONFIGURED`: an image that holds only what a source configured, and defaults and secrets out unless asked

* **id**: Y-26
* **goal**: `cf_image_write` takes a new flag `CF_IMAGE_CONFIGURED` that writes only keys a source set (no defaults); `gen build --target image` leaves defaults out unless `--with-defaults` and refuses a `CF_SENSITIVE` value in the artifact unless `--allow-secrets`; the C call with the old flags writes what it always wrote.
* **why**: `docs/research/config-compiler.md` section 4.2 F and section 5.3 F: an artifact must not freeze defaults the next release of the program changes, and a full report or image is a credential-bearing file.
* **needs**: Y-24
* **size**: small
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided F (defaults in a compiled image): (1), left out unless asked** (2026-11-26, see "Organiser's answers": the command's options are `--include defaults` and `--include secrets`, which the `--with-defaults` and `--allow-secrets` of this card's text mean); the C call keeps today's behaviour unless the new flag is given, so 1.x callers see no change. **Rejected:** (2) written, as today (an artifact would freeze defaults the next release changes).
  * **The flag**: `cf_image.h:44-49`: `CF_IMAGE_CONFIGURED = 1u << 1`, "only what a source set: no defaults". An added enum value; the version of the format does not change (the image carries fewer keys). `cf_image_write` (`lib/config/src/*/image.c`: find with `grep -n "cf_image_write" lib/config/src/*/*.c`) skips a key whose winning candidate's source is the contract's default (the `CF_TRANSIENT`-like test `emit.c:36`'s `keep` uses for `CF_EMIT_CONFIGURED`: copy its test, do not write a second one).
  * **Secrets**: `gen build` refuses to write a `CF_SENSITIVE` key's value into the image, a report with `CF_REPORT_FULL`, or an emitted document, unless `--allow-secrets`: exit 1, `'<key>' is sensitive and would be written to the output; pass --allow-secrets to write it`. A blind report never holds a value, so it is not refused.
  * **D-207** ("An image keeps only what was configured, unless asked"): the flag, the two command options and their defaults. Rejected: stripping defaults silently from the existing flags.
* **files**: `lib/config/include/configforge/cf_image.h`, the image writer, `tools/gen/build.c`, `lib/config/tests/test_image.c` (the image test file: `ls lib/config/tests | grep image`), `tests/gen_build/`, `docs/ImageFormat.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-26 main` (after Y-24).
  2. Tests: in `test_image.c` (a) an image written with `CF_IMAGE_CONFIGURED` holds no key whose only candidate is a default and holds each configured key, and an image written with flags 0 is byte-identical to the one before this card (a golden: take it from `git stash`-free means: write the image on main first and commit it as `tests/data/image-v1.cfgb` as the first commit of the card); (b) `gen build --target image` without `--with-defaults` writes the configured image; with it, the full one; (c) a sensitive key set in a file and `--target toml`: exit 1 with the message; with `--allow-secrets` exit 0.
  3. See it fail: build `test_image` (the flag is undeclared) and `ctest -R gen_build`.
  4. Write the flag and the options. `ctest --preset default -R "test_image|gen_build"`.
  5. See that the test can fail: ignore the flag behind a run-time-false condition; (a) fails; put it back.
  6. D-207, `docs/ImageFormat.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-26: CF_IMAGE_CONFIGURED, defaults and secrets out of an artifact unless asked (D-207)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_image|gen_build"` exit 0; the golden image is unchanged by the card.
* **stop if**: a default's candidate cannot be told from a configured one in the key space (report: the key space needs a marker); an image test golden changes; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`CF_IMAGE_CONFIGURED`; `gen build` leaves defaults out of an image and refuses secrets unless asked (Y-26, D-207)." `docs/Decisions.md`: D-207.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I accept your recommendations"): Decided F: defaults and secrets stay out of the image unless asked for, by `--include defaults` and `--include secrets`, both off by default; a secret written into an image puts a warning in the report.

## Y-27 - The generated C: `gen build --target c` (plain typed code) and `--target c-baked` (the baked tables)

* **id**: Y-27
* **goal**: two C targets (the maintainer's answer: "why should it read the data again at run time: it makes C code directly"). `--target c`, the default of the two, writes plain typed C89 code: a header with `struct app_config { ... }` and `extern const struct app_config APP_CONFIG;` and its `.c`, no library linked, no lookup, no parsing, a misspelled key a compile error. `--target c-baked` (as this card first said): `configforge gen build --schema S --source ... --target c-baked --out device_cfg.c` resolves a configuration at build time and writes it as C89 `static const` tables with provenance ("from device.toml line 12, at build time"); a program that links only that file and the baked reader reads the same values and gets the same `explain` text as layer 0, with no parser linked.
* **why**: `docs/research/config-compiler.md` section 4.2 C (baked C tables, "designed (D-104)", L) and section 6, fourth row; D-104, layer 1 for C (`docs/plan/A-architecture.md:258-270`): "no parser in the binary ... the field docs/Market.md found open". The card is not written in any other group's file (searched `docs/plan/*.md` for `cf_config_from_baked`: only the architecture page names it), so it is here, as the research's own list says it belongs to 1.9.0.
* **needs**: Y-24, Y-26
* **size**: large
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **The plain target `--target c`** (the default of the two). `--out app_config.h` names the header and the source is its sibling `.c`; `--name <prefix>` (default `app`, checked as a C identifier, exit 2 otherwise). The header holds, in C89, `struct <prefix>_config { ... };` with one member per key of the resolved configuration, a nested struct for each dotted table (`server.port` is `cfg.server.port`), and `extern const struct <prefix>_config <PREFIX>_CONFIG;`; the `.c` holds the initialiser. Member types: bool `int`; int `<prefix>_int` (a typedef in the header: `long`, and with `--int64` `long long`, the pair then being C99; a value outside `long` without `--int64` is refused by `gen build`: `key X needs 64 bits: pass --int64`); real `double`; text `const char*`; a list `const T name[N]; unsigned name_count;`. A key left open (Y-25) or absent is not a member: a program reads it through layer 0. Provenance is a comment above each member (`/* from device.toml:12 */`); there is no run-time `explain`. No function is generated and no library is linked. `CF_SENSITIVE` keys are withheld (Y-26's rule; `--include secrets` writes them). Deterministic: no date, members in the contract's order. **Rejected**: one target only (the baked tables read the data again at run time: kept as `c-baked` for `explain` and run-time overrides); a macro per key.
  * **The baked target**: `--target c-baked` in `gen_build` (Y-24's target list gains `c`; the unknown-target message lists it). The output is one C89 file: a header comment with the schema's name, the sources and their digests, `static const cf_baked_entry <name>_entries[]` sorted by key (for binary search by the reader), and `const cf_baked <name> = { sizeof(cf_baked), 1, <count>, <name>_entries, <name>_strings }`; `--name <name>` (default `baked_config`) is the C identifier prefix, checked as a C identifier (exit 2 otherwise).
  * **The shapes** (declared in a new public header `lib/config/include/configforge/cf_baked.h`, experimental tier (D-89); `size` first, D-51):
    ```c
    typedef struct cf_baked_entry {
        const char* key;      /* NUL-terminated */
        unsigned char kind;   /* CF_BAKED_BOOL, _INT, _FLOAT, _STR (from cf_value's kinds) */
        cf_i64      i;        /* bool, int */
        double      f;
        const char* s;        /* string, NUL-terminated */
        const char* origin;   /* "device.toml:12" at build time, or NULL */
        const char* source;   /* the source's name */
    } cf_baked_entry;
    typedef struct cf_baked { size_t size; unsigned version; size_t count; const cf_baked_entry* entries; } cf_baked;
    CF_API const cf_baked_entry* cf_baked_find(const cf_baked* b, const char* key);        /* NULL when absent */
    CF_API cf_bool cf_baked_get_int(const cf_baked* b, const char* key, cf_i64* out);
    CF_API cf_bool cf_baked_get_bool(const cf_baked* b, const char* key, cf_bool* out);
    CF_API cf_bool cf_baked_get_str(const cf_baked* b, const char* key, const char** out);
    CF_API size_t  cf_baked_explain(const cf_baked* b, const char* key, char* buf, size_t cap); /* measure-or-write; the text of layer 0's explain for a baked winner, ending "(baked at build time)" */
    ```
    `cf_config_from_baked()` of D-104's text is these functions (one reader, `get` and `explain` over the tables): the name is kept in D-208 as an alias note, not as a function: **Decided (the public shape)**: (a). Options weighed: **(a)** the five functions above (a baked configuration is its own small type); **(b)** `cf_config_from_baked(const cf_baked*)` returning a `cf_config*` that works with the whole existing API (needs the config core, so the program links layer 0, which defeats "no reader linked"); **(c)** both, (b) as a `lib/config` convenience. **Rejected**: (b) and (c) (a `cf_config*` needs the config core, so the program links layer 0, which defeats "no reader linked"; (b) may come later if a program wants the full API over baked data). (a) is what "links no reader at all" needs; D-104 names the function as an idea, not a signature.
  * **No parser linked**: `cf_baked.c` (new, `lib/baked/` as a component of its own, D-100, built as `libconfigforge_baked.a`, depends on `base` only: `lib/baked/CMakeLists.txt`; `gate.layering` is told it may include `cf_base` and nothing else: one line in the layering table, found with `grep -rn "serialize" scripts/work/layering* cmake/*layer* | head -3`) is C89, allocates nothing, calls nothing from `lib/serialize`. A test links a program against `libconfigforge_baked.a` only and checks with `nm` that no `cf_codec_`/`json`/`toml` symbol is in the link (`gate.baked_nolink`, an `add_test` running `nm --defined-only` over the test binary; skipped without `nm`).
  * **Compared with layer 0** (the test of the card): for every key of the example contract, `cf_baked_find` and `cf_config_get` agree on kind and value; `cf_baked_explain` and `cf_explain` agree on the winning source name and the origin, up to the suffix.
  * **Deterministic output**: no date; entries sorted; the same inputs give the same file. **Secrets**: refused unless `--allow-secrets` (Y-26's rule): a `CF_SENSITIVE` key's value is never baked by default; the entry is written with kind `CF_BAKED_ABSENT` and the origin kept, so `explain` says it was set and withheld.
  * **Arrays and lists**: a key with `.count` and indexed keys (`list.0`, `list.1`) is baked as separate entries, as the key space holds them; a key kind with no baked form (a duration or size, which `cf_value` may carry) is baked as its integer in the unit the value holds, with the kind it has. If the key space holds a kind this list does not name, stop (`stop if`).
  * **D-208** ("Two C targets: plain typed code, and baked tables with a small reader"): `--target c` (typed C, no library), `--target c-baked` (the five functions, the no-parser rule, provenance at build time), secrets withheld. Rejected: a parser in the baked reader; C23 `#embed` (D-104's other kit, a separate thing: read at start-up).
* **files**: `tools/gen/build.c`, `tools/gen/baked.c` (new: the baked emitter), `tools/gen/cgen.c` (new: the plain typed C emitter), `tests/gen_build/plain_main.c` (new), `lib/baked/` (new: `CMakeLists.txt`, `include/configforge/cf_baked.h`, `src/baked.c`, `tests/CMakeLists.txt`, `tests/test_baked.c`), `CMakeLists.txt` (add the subdirectory, found by `grep -n add_subdirectory CMakeLists.txt`), `cmake/configforge.symbols`, `scripts/work/` layering table (named above), `tests/gen_build/baked_main.c` (new), `docs/Guide.md` (a section "A configuration baked into the program", after section 12 "Small targets"), `docs/Design.md` (one paragraph under D-104's layer 1); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-27 main` (after Y-24 and Y-26).
  2. Write `lib/baked/tests/test_baked.c`, C89, with a hand-written `cf_baked` table of five entries (one of each kind, one with origin NULL): `cf_baked_find` hits and misses, the three getters, a getter of the wrong kind returns `CF_FALSE`, `cf_baked_explain` measure-or-write, a NULL table is refused.
  3. See it fail: `cmake --build --preset default --target test_baked 2>&1 | tail -n 20` (no component).
  4. Write the component, the symbols, the layering line. `ctest --preset default -R "test_baked|gate" 2>&1 | tail -n 30`.
  5. In `tests/gen_build/` write the end-to-end test: `gen build ... --target c-baked --out baked_cfg.c`; `baked_main.c` includes the generated file, prints each key; a CMake `add_test` builds it with the generated file (`add_custom_command` from the `gen` target) and compares with `cf_config_get` over the same files in a second program (layer 0): the two programs' outputs are `diff`ed by the test. `gate.baked_nolink`.
  6. See it fail: run before the emitter exists (`ctest -R gen_build_baked`); then write `tools/gen/baked.c`; it passes.
  7. See that the test can fail: bake a value off by one behind a run-time-false condition in the emitter; the diff fails; put it back.
  7b. The plain target: write `tests/gen_build/plain_main.c` first (it includes the generated header and prints each member; the test builds it with `-std=c89 -pedantic -Wall -Werror` and links no library; `nm` finds no `cf_` symbol in it; its output is `diff`ed against layer 0's values for the same files); see it fail (no `--target c`); write `tools/gen/cgen.c`; it passes; a misspelled member name does not compile (a negative case: the build of a file naming `cfg.server.prot` fails).
  8. D-208, Guide, Design paragraph, CHANGELOG. Format; codemap and apiref, last. Commit `Y-27: the baked configuration, gen build --target c-baked and cf_baked (D-208)`.
  9. ACCEPT; push; ACCEPT-CI (the embedded targets' jobs, if they exist yet, build the baked example).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_baked|gen_build|gate.baked_nolink"` exit 0; `ctest --preset minimal -R test_baked` exit 0.
* **stop if**: the key space holds a kind of value this card does not name; `gate.layering` or `gate.symbols` cannot take a new component without a rule this card does not write; the generated file is not valid C89 under `-std=c89 -pedantic` (a gate: add it to the test); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`gen build --target c` (plain typed C) and `--target c-baked` with `cf_baked`: a configuration resolved when the program is built, read with no parser linked (Y-27, D-208)." `docs/Decisions.md`: D-208. `docs/Roadmap.md`: the item "A baked configuration" (found by `grep -n "baked" docs/Roadmap.md`, line 1581 area) 🟩.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I accept your recommendations") ("why should it read the data again at run time: it makes C code directly"): two C targets. `--target c`, **the default**: plain typed C89 code, a generated header with `struct app_config { ... }` and `extern const struct app_config APP_CONFIG;` and its `.c`; no library linked, no lookup, no parsing; a misspelled key is a compile error. `--target c-baked`: this card as written (static const tables loaded by the baked reader, `--runtime env,cli` overrides, `explain` works). The card follows this (2026-11-27, below).
  * 2026-11-27, the organiser: Y-27 has the two targets (the plain `--target c` is new; the baked tables are `--target c-baked`), and the public shape of the baked reader is (a), the five functions (the maintainer's standing approval, 2026-11-27).

## Y-28 - `cf_builder_errors`: every diagnostic with its place, and `gen build --json` and `gen check --all`

* **id**: Y-28
* **goal**: a builder option `cf_builder_diagnostics(b, fn, user)` keeps going past the first refusal and hands every error (a `cf_error`: code, reason, key, source, place, message; and its severity) to `o.handler` during `cf_builder_create`, which still fails when any was an error; `gen build --json` and `gen check --all` print the whole list, shaped like `terraform validate -json`.
* **why**: `docs/research/config-compiler.md` section 4.2 D and section 5.1 item 2: a compiler needs the whole list, not the first error; the program itself can then list every problem at start-up. It is where "one diagnostic model" (docs/Maintaining.md, the 1.8.0 row; what remains is K-6a and K-6b in docs/plan/K-correctness.md) leads.
* **needs**: Y-24, K-9c (the `cf_error` model of D-223)
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided C (a public diagnostics interface): (1)**, as D-223 shapes it (one error model, K-correctness.md). Every diagnostic is a `cf_error` (D-223 points 2 and 3: its code, its place, its reason text, a chain), so this card defines no struct of its own, only the option of D-223 point 4. **Rejected**: (2) a walk inside the tool over every key as `CF_INVALID_FALLBACK` (it misses errors that are not a candidate's and holds a second reading of what is an error); a `cf_diag` struct beside `cf_error` (a second error model).
  * **The names** (experimental tier, `lib/config/include/configforge/cf_error.h` beside the model, or `cf_diag.h` if K-9c has already filled that header: the worker chooses by what is there and writes it under notes; `size` first, D-51, D-220):
    ```c
    typedef void (*cf_error_handler)(const cf_error* e, int severity, void* user); /* severity: cf_reason_severity(e->reason) (N-13) */
    typedef struct cf_error_options {
        size_t size;
        cf_bool collect;          /* CF_FALSE (default): the first error stops the work and is returned; CF_TRUE: every error is collected */
        cf_error_handler handler; /* NULL (default); called for every collected error, or for the error that stops the work (logging, an indicator) */
        void* user;
    } cf_error_options;
    CF_API void    cf_error_options_init(cf_error_options* o);
    CF_NODISCARD CF_API cf_bool cf_builder_errors(cf_builder* b, const cf_error_options* o);
    ```
    `cf_builder_errors` is the compiler's case; the handler also serves a program that logs. `key` of the error is NULL when none; `at` is `file:line:col` as `explain` writes it, or NULL; a sensitive key's value never appears in `message` (a test greps for it).
  * **Where**: `lib/config/src/core/config.c`: the builder struct (line 48) holds the options; where the builder stops at the first strict refusal (find each `builder_fail` call that ends `cf_builder_create`, with `grep -n "builder_fail\|return NULL" lib/config/src/core/config.c | head -40`), with `collect` set it records the diagnostic and continues when the refusal is local to one key or one source (a value out of range under strict; an unreadable source; an unknown key under reject), and stops where continuing is meaningless (out of memory, a schema that cannot be read). After the build, `cf_builder_create` returns NULL if any diagnostic had severity error, with the first as its `err`, as today.
  * **`gen build --json`**: prints `{"valid": false, "error_count": n, "warning_count": m, "diagnostics": [{"severity": "error", "summary": <message>, "detail": null, "key": ..., "source": ..., "range": {"filename": ..., "start": {"line": l, "column": c}}}]}` (the shape of `terraform validate -json`, research source [22]) to stdout; exit 1 when `valid` is false. **`gen check --all`** prints the same list (as text without `--json`: one `<place>: <severity>: <message>` line each). Default `gen check` stays at the first error (exit codes unchanged).
  * **D-209** ("Every diagnostic, as the one error model, by an option"): C's (1) as D-223 shapes it, the option `collect` and the handler, and that a failed build with a handler still fails.
* **files**: `lib/config/include/configforge/cf_error.h` or `cf_diag.h` (see above), `lib/config/src/core/config.c`, `cmake/configforge.symbols`, `tools/gen/build.c`, `tools/gen/contract.c`, `lib/config/tests/test_diag.c` (new), `tests/gen_build/`, bindings: none (experimental tier, not wrapped); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-28 main` (after Y-24).
  2. `test_diag.c`: a builder with a schema and three bad files (a value out of range, an unknown key, an unreadable optional-not file): (a) without the option `cf_builder_create` returns NULL with the first error only; (b) with `collect` the handler is called three times, severities error, each with its key and `file:line` place; (c) the build still returns NULL; (d) a sensitive key's bad value does not appear in any message; (e) with all files good the handler is never called and the build succeeds; (f) a warning-only build (a deprecated key set) calls it with a warning severity and succeeds.
  3. See it fail: `cmake --build --preset default --target test_diag 2>&1 | tail -n 20`.
  4. Write the types, the builder option and the continuing. `ctest --preset default -R "test_diag|test_config|test_define|gate"`: every existing result unchanged.
  5. See that the test can fail: stop after the first diagnostic behind a run-time-false condition; (b) fails; put it back.
  6. `gen build --json` and `gen check --all`, with their tests (a golden JSON in `tests/gen_build/`).
  7. D-209, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-28: cf_builder_errors, every error with its place (D-209)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_diag|gen_build"` exit 0; every `test_config*` result unchanged.
* **stop if**: continuing past a refusal leaves the builder in a state that a later step reads wrongly (a crash or a wrong second diagnostic: report the case); K-9c changed `cf_error` so that `code`/`reason` differ from the names this card uses (the card then follows the model's names, and says so under notes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`cf_builder_errors` (experimental): every error of a build with its place, as `cf_error`; `gen build --json` and `gen check --all` print them (Y-28, D-209)." `docs/Decisions.md`: D-209.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: Y-28 follows D-223 (one error model): the diagnostics are `cf_error`s, the option is `cf_error_options` with `collect` and `handler` (D-223 point 4), choice C is (1) (the maintainer's standing approval, 2026-11-27).

## Y-29 - The manifest: inputs and their digests, the contract's digest, version, profile, open keys, the output's digest

* **id**: Y-29
* **goal**: `gen build --manifest <file>` writes a small JSON beside the artifact: each input file and its SHA-256, the contract's SHA-256, the library's version, the profile, the open (run-time) and sealed keys, the output's SHA-256 and `SOURCE_DATE_EPOCH` when it is set; the same inputs give the same manifest.
* **why**: `docs/research/config-compiler.md` section 4.2 H and section 6 (sixth row): a build can say what it was made from, as a reproducible build does.
* **needs**: Y-24, Y-25, and M-34 (SHA-256 as a digest, D-184: "one digest a purpose")
* **size**: small
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided E (the manifest's place)**, research section 5.3 E. Options: **(1)** a JSON file beside the artifact; **(2)** a section of the image. **Decided: (1) now, (2) with Y-33 (2.2.0, in the header M-12 reserves).** The steps are (1)'s. **Rejected**: (2) before Y-33 (the image has no header to hold it yet).
  * **Shape**: `{"manifest_version": 1, "configforge": "<cf_version()>", "profile": "<p>"|null, "inputs": [{"path": "<as given>", "rank": "<name>", "sha256": "<hex>"}], "contract": {"path": "<schema>", "sha256": "<hex>"}, "open": ["<key>"], "sealed": ["<key>"], "output": {"target": "<t>", "path": "<out>", "sha256": "<hex>"}, "source_date_epoch": <n>|null}`, keys in this order, lists sorted (inputs in the order given, then none re-sorted; `open`, `sealed` sorted), two-space indentation, a final newline. Paths are written as given (no absolute path is added: a build in another directory differs only by what the user typed). `SOURCE_DATE_EPOCH` is read once from the environment, the only environment variable the command reads; no other clock.
  * **SHA-256** from M-34's interface (`cf_sha256` or what D-184 names: read `docs/plan/M-maintainer.md` around line 736 for the function; if M-34 is not landed, this card does not open).
  * **D-210** ("The manifest is a JSON file beside the artifact"): the shape above. Rejected: the manifest inside the image now; a timestamp; absolute paths.
* **files**: `tools/gen/build.c`, `tools/gen/manifest.c` (new), `tests/gen_build/`, `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-29 main` (after Y-25 and M-34).
  2. Tests: (a) the manifest of Y-24's fixture has the six members in order; (b) the `sha256` of each input equals `sha256sum` of the file (the test runs `sha256sum` when present, else compares against digests written into the test); (c) two runs are byte-identical; (d) with `SOURCE_DATE_EPOCH=1700000000` the member is that number, without it `null`; (e) `sealed` lists the keys Y-25 computes; (f) the `output.sha256` equals that of the artifact.
  3. See it fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20`.
  4. Write `manifest.c` and the option. `ctest --preset default -R gen_build`.
  5. See that the test can fail: add the time of day to the manifest behind a run-time-false condition; (c) fails; put it back.
  6. D-210, `docs/Tools.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-29: the manifest of a build (D-210)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R gen_build` exit 0.
* **stop if**: M-34 is not landed; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`gen build --manifest`: inputs and their SHA-256, the contract's digest, open and sealed keys, the output's digest (Y-29, D-210)." `docs/Decisions.md`: D-210.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: the choice E is (1) now and (2) with Y-33, as the card recommended (the maintainer's standing approval, 2026-11-27).

## Y-30-m - Rules across keys in the contract: a fixed declarative set

* **id**: Y-30-m
* **goal**: the contract can say, with a fixed set of declarative rules and no expression language, that one key requires another (`dependentRequired`), that a key's value is at least another's (`minimum` by key), or that one of a set of keys is given (`oneOf`); the library checks them at run time, and therefore the compiler at build time.
* **why**: `docs/research/config-compiler.md` section 4.2 G (option 2), section 5.3 D and section 6 (seventh row): a compiler that cannot check "if `tls.enabled` then `tls.cert`" proves less than the program does at start-up.
* **needs**: Y-24
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided D (rules across keys): (2), a fixed declarative set in the contract**, the research's recommendation, decided once for this card and the analysis tool's question 2 (Y-39-m): this card holds the relations, Y-39-m adds `pattern` on the same call. Recipes for OPA and CUE (Y-36, Y-31) stay for what the set cannot say. **Rejected:** (1) recipes only (a compiler that cannot check "if `tls.enabled` then `tls.cert`" proves less than the program does at start-up); (3) an expression language (a language of our own: CUE, Pkl, Jsonnet and Dhall exist).
  * **The set**: JSON Schema's `dependentRequired` and `if`/`then` over `const` and `enum` tests (not over patterns), at the object level of the schema (`lib/config/src/bridge/schema.c`, the walker that reads `required`, near line 700; today these keywords are refused by name, so no schema that loads now changes meaning), plus `x-configforge-atleast: {"key": "pool.max", "other": "pool.min"}` for "at least another key" (JSON Schema has no relation between two values; the `x-` prefix keeps the schema valid for every other validator, which ignore it). `oneOf` of `required` lists (`"oneOf": [{"required": ["a"]}, {"required": ["b"]}]`) for "exactly one of these keys is given"; any other `oneOf` stays refused.
  * **The C call** (experimental tier, beside `cf_builder_schema_file`: `grep -n cf_builder_schema_file lib/config/include/configforge/*.h`): `typedef enum cf_relation { CF_REL_REQUIRES = 1, CF_REL_AT_LEAST = 2, CF_REL_ONE_OF = 3, CF_REL_IF_EQUALS = 4 } cf_relation;` and `CF_NODISCARD CF_API cf_bool cf_builder_relation(cf_builder* b, cf_relation kind, const char* key, const char* const* others, size_t n, const char* value, cf_error* err);` (`REQUIRES`: `key` set means every one of `others` is set; `AT_LEAST`: `n` is 1, `key`'s number is at least `others[0]`'s; `ONE_OF`: exactly one of `key` and `others` is set; `IF_EQUALS`: when `key`'s value is `value`, every one of `others` is set; `value` is NULL for the other kinds). An unknown kind, a key not in the contract, or `AT_LEAST` on a non-number is `CF_E_DEFINITION` at the call. The schema reader lowers each keyword to this call, so there is one checker.
  * Run by the library when the configuration is built (so a program refuses at start-up what the compiler refused at build time), each violation a `CF_E_VALIDATE` (D-223) with both keys and the place of the winning value of each: `'tls.enabled' at app.toml:3 requires 'tls.cert', which no source sets`; `'pool.max' (app.toml:7) must be at least 'pool.min' (site.toml:2)`; `exactly one of 'a', 'b' must be set; both are (app.toml:1, app.toml:2)`. A sensitive key's value is never quoted.
  * **D-211** ("Relations between keys are a fixed declarative set"): the bullets above; Rejected: recipes only; an expression language; relations over patterns (Y-39-m).
* **files**: `lib/config/src/bridge/schema.c`, the builder and its build step (`grep -n "cf_builder_create" lib/config/src/*/*.c | head -3`), the header of `cf_builder_schema_file`, `lib/config/tests/test_relation.c` (new) and `lib/config/tests/CMakeLists.txt`, `tests/gen_build/` (a schema with a relation), `cmake/configforge.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-30-m main` (after Y-24).
  2. Tests in `test_relation.c`: for each kind, through the C call and through a schema (`cf_builder_schema_file` on a small file the test writes): a configuration that holds passes; one that breaks it is refused with the message above and both places; `AT_LEAST` on a string key is `CF_E_DEFINITION`; a sensitive key's value is not in the message; a schema with `oneOf` of anything but `required` lists is still refused by name; every existing `test_schema` case unchanged. A `tests/gen_build/` case: `gen build` on a source that breaks a relation exits 1 with the message.
  3. See it fail: `cmake --build --preset default --target test_relation 2>&1 | tail -n 20` (`cf_builder_relation` undeclared).
  4. Write the call, the check at build, and the schema keywords. `ctest --preset default -R "test_relation|test_schema|gen_build|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: skip the `REQUIRES` check behind a run-time-false condition (never a constant); its case fails; put it back.
  6. The Guide (a paragraph "Relations between keys" in the contract's section, `grep -n "^## " docs/Guide.md | grep -i contract`), D-211, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-30-m: relations between keys in the contract, a fixed declarative set (D-211)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "test_relation|test_schema|gen_build"` exit 0; every existing test unchanged.
* **stop if**: the builder has no step after precedence where every key's winning value and place are known (report: the check needs a new stage); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_builder_relation` and the schema's `dependentRequired`, `if`/`then`, `oneOf` of `required` and `x-configforge-atleast`: relations between keys (Y-30-m, D-211)"; `docs/Decisions.md`: D-211.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Question D is (2), the research's recommendation, answered once with Y-39-m's question 2; the C call's shape is written above; version 2.2.0, Y-24's (the maintainer's standing approval, 2026-11-27).

---

## Y-31 - Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel and KCL as front ends

* **id**: Y-31
* **goal**: the Guide has a page "A language in front of ConfigForge" showing, for each of CUE, Pkl, Jsonnet, Dhall, Nickel and KCL, the command that exports its result to JSON or YAML and the ConfigForge command that takes it from there (`gen build --source out.json=user-file ...`); the JSON/YAML-producing ones are run by a test when the tool is on the PATH.
* **why**: `docs/research/config-compiler.md` section 4.2 J and section 5.1 item 4: the library has no configuration language and builds none (research 4.2 A); these languages are used in front of it.
* **needs**: Y-24
* **size**: small (documentation)
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Where**: `docs/Guide.md`, a new section after Y-36's subsection; examples in `examples/frontends/` (new): `app.cue`, `app.pkl`, `app.jsonnet`, `app.dhall`, `app.ncl`, `app.k`, each defining the same three keys (`server.port`, `log.level`, `db.host`) and each with the export command in a comment line: `cue export app.cue`, `pkl eval -f json app.pkl`, `jsonnet app.jsonnet`, `dhall-to-json --file app.dhall`, `nickel export app.ncl`, `kcl run app.k --format json`. Each command's flags are checked against the tool's own `--help` where installed; where not, the Guide says `as the tool's documentation says` and links its page (source numbers in research section 7).
  * **Test**: `ctest -R frontends`: for each tool on the PATH, run its export into `fe_<tool>.json`, then `gen build --schema S --source fe_<tool>.json=user-file --target toml` and compare with one golden; the rest print `skipped: <tool> not found` and pass.
  * **What the Guide says it does not do**: no templates (research section 3, item 4), no language of our own, and that unification in these languages is theirs, not a rank of ours.
  * No decision number: documentation.
* **files**: `docs/Guide.md`, `examples/frontends/*` (new), the test wiring in `tests/CMakeLists.txt`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-31 main` (after Y-24).
  2. Write the harness and the golden first (the six files' outputs reduce to one JSON).
  3. See it fail: `ctest --preset default -R frontends 2>&1 | tail -n 20` (no such test).
  4. Write the six examples and the Guide section; run each tool that is installed by hand and copy the real command lines into the Guide.
  5. See that the test can fail: change a value in `app.jsonnet`; the comparison fails when `jsonnet` is installed (else this step is done on one tool that is: say which); put it back.
  6. CHANGELOG. Format; codemap and apiref, last. Commit `Y-31: recipes for CUE, Pkl, Jsonnet, Dhall, Nickel and KCL in front of gen build`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R frontends` exit 0 (skips count).
* **stop if**: none of the six tools can be run on any machine at hand (the recipes would be unrun: write which are unrun under notes and mark them so in the Guide); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide page and `examples/frontends/` on putting CUE, Pkl, Jsonnet, Dhall, Nickel or KCL in front of `gen build` (Y-31)." `docs/Roadmap.md`: "A configuration compiler" 🟩 when Y-27 to Y-29 are done.

---

## Y-32-m - A build file read with the tool's own contract

* **id**: Y-32-m
* **goal**: `configforge gen build --file build.toml` reads the sources, ranks, runtime ranks, profile and targets from a file that is itself read by the library with a contract of the tool's own (so it has the same diagnostics, places and formats as any configuration), instead of from the command line.
* **why**: `docs/research/config-compiler.md` section 4.2 I and section 5.3 B (**recommended: (3), both, the file later**) and section 6 (no version of its own): the command line comes first (Y-24); a build file is for a repository with many targets.
* **needs**: Y-24, Y-29
* **size**: medium
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Decided B (how the build is described): (3), both**: the command line (Y-24) and this card's build file, read by the library with the tool's own contract. **Rejected:** (1) the command line only (a repository with many targets repeats long command lines in scripts); (2) the file only (breaks Y-24's callers).
  * **The shape**: the tool's own contract, in `tools/gen/build.schema.json` (committed, and used by `gen build --file` to read its own input through `cf_builder_schema_file`): `schema` (path), `runtime` (list), `profile`, `[[source]]` (path, rank), `[[target]]` (name, kind, out, `include` (a list of `defaults` and `secrets`, Y-26's `--include`), `manifest`); one `gen build --file b.toml` builds every target, in the file's order; relative paths are relative to the file. Any format the library reads works for the build file. `--file` with any other build option is a usage error (exit 2): the file is the whole description. A target that fails stops the run with exit 1 and names the target; targets before it are written (each output is written whole or not at all).
  * Errors (D-223): a build file that does not hold to the tool's contract is exit 2 with the library's own diagnostic (key and place in the build file).
  * **D-212** ("A build file is a configuration read with the tool's own contract"): the bullets above; Rejected: a format of our own for the build file; options beside `--file`.
* **files**: `tools/gen/build.c`, `tools/gen/build.schema.json` (new), `tools/gen/CMakeLists.txt` (the schema installed beside the tool or compiled in as a string: compiled in, so the tool has no file to find), `tests/gen_build/` (a `build.toml` with two targets, a bad one), `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-212), `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/Y-32-m main` (after Y-24 and Y-29).
  2. Tests in `tests/gen_build/` (Y-24's style): (a) `gen build --file build.toml` writes both targets, byte-identical to the same two `gen build` command lines of Y-24 (`cmp`); (b) the same file as YAML gives the same outputs; (c) `--file` with `--target` is exit 2; (d) a build file with `[[target]]` missing `out` is exit 2 naming `target[1].out` and the place; (e) a second target whose source does not exist is exit 1 naming it, the first target written; (f) a target with `include = ["defaults"]` matches `--include defaults`.
  3. See it fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20` (`--file` is a usage error today).
  4. Write the schema and the reading in `build.c` (each target lowered to the same internal call the command line makes, so there is one build path). `ctest --preset default -R "gen_build|gen_check"`.
  5. See that the test can fail: resolve a relative path against the working directory behind a run-time-false condition; (a), run from another directory, fails; put it back.
  6. `docs/Tools.md`, D-212, CHANGELOG. Format; codemap last. Commit `Y-32-m: gen build --file, a build file read with the tool's own contract (D-212)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R gen_build` exit 0.
* **stop if**: Y-24's build cannot be called per target without its command-line parsing (report: the build needs a split first); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`configforge gen build --file`: a build file read with the tool's own contract (Y-32-m, D-212)"; `docs/Decisions.md`: D-212.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Question B is (3), the research's recommendation; the target's `with_defaults` and `allow_secrets` become `include`, after Y-26's answer; version 2.2.0, Y-24's and Y-29's (the maintainer's standing approval, 2026-11-27).

---

## Y-33 - The manifest inside the image (2.2.0, in the header M-12 reserves for it)

* **id**: Y-33
* **goal**: an image of major 2 and minor 1 carries the manifest of Y-29 after its other regions, found through two header fields, so an artifact says what it was made from without a file beside it.
* **why**: `docs/research/config-compiler.md` section 5.3 E (2) and section 6 (last row, with the image header): the one place a manifest cannot be lost from its image. The image's layout is fixed in 2.0.0 by M-12, which reserves bytes 104-111 of its 128-byte header for this card; the manifest needs Y-29 (2.2.0), so the card is 2.2.0's and nothing of 2.0.0 waits for it.
* **needs**: Y-29, M-12
* **size**: small
* **version**: 2.2.0
* **status**: open
* **decided**:
  * **Shape**: the manifest's JSON bytes as Y-29 writes them, placed after the last region (the strings), at an offset that is a multiple of 8 and ending at `total`; the header's `manifest_off` (byte 104) and `manifest_len` (byte 108), reserved zero by M-12 (`docs/ImageFormat.md`), say where. `minor` is 1 for an image that carries a manifest and 0 for one that does not, so Y-26's golden (an image of minor 0) is unchanged, and `CF_IMAGE_MINOR` (`lib/config/include/configforge/cf_image.h:42`) is the highest minor this build reads and writes, 1. There is no new section kind and no section table: the image has none, and a reader of minor 0 ignores bytes it does not know (M-12 tests it: "accept a higher one; fields only grow"), so no break is made. `cf_image_open` checks that a nonzero `manifest_off`/`manifest_len` lies inside `total` (a claim, like every number in an image).
  * **The digest rule**: the manifest's `output.sha256` is the SHA-256 of the image as written without the manifest, that is, the bytes up to `manifest_off` with `manifest_off` and `manifest_len` zeroed, `minor` 0 and `total` equal to `manifest_off`: a verifier can rebuild exactly that from a finished image, and `gen build` makes the same bytes first (writes the image without a manifest, hashes it, writes the manifest, writes the image again with it). The determinism digest in the header is of the key space and is the same in both. Written in `ImageFormat.md`.
  * **Names (D-220's convention, K-11a)**: `typedef struct cf_image_options { size_t size; unsigned flags; const char* manifest; size_t manifest_len; } cf_image_options;` in `cf_image.h`, with `CF_API void cf_image_options_init(cf_image_options* o);` (defaults: `size` set, `flags` 0, `manifest` NULL and `manifest_len` 0, which is no manifest) and `CF_EXPERIMENTAL(2, 2) size_t cf_image_write_with(const cf_keyspace* ks, const cf_source_view* sv, const cf_image_options* o, char* buf, size_t cap);` (measure-or-write like `cf_image_write`, NULL `o` is the defaults; `cf_image_write(ks, sv, flags, buf, cap)` is unchanged and calls it with the flags only). A manifest with `CF_IMAGE_V1` (M-12) is refused: nothing is written and the length is 0, as for any image that cannot be written. `CF_EXPERIMENTAL(2, 2) cf_bool cf_image_manifest(const cf_image* img, const char** json, size_t* n);` points into the image, `CF_FALSE` when there is none (major 1, or `manifest_len` 0). `gen build --manifest-in-image` writes it (with `--manifest <file>` the file is written too).
  * **D-213** ("The manifest may live in the image"): the header fields, the minor, the digest rule.
  * Rejected: a section table or a new section kind (the image has none; a table would be a second major), a new major (M-12's header already has the room).
* **files**: `lib/config/include/configforge/cf_image.h`, `lib/config/src/bridge/image.c`, `tools/gen/build.c`, `docs/ImageFormat.md`, `lib/config/tests/test_image.c`, `tests/gen_build/`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-33 main` (after Y-29 and M-12).
  2. Tests: (a) an image written with a manifest through `cf_image_write_with` reads back its JSON byte for byte through `cf_image_manifest`, and has `minor` 1; (b) an image of minor 0 (the golden of Y-26) opens unchanged, `cf_image_manifest` false; (c) the same image opened by a reader that knows only minor 0 (the test reads the header with the minor-0 rules, ignoring bytes 104-111) opens it; (d) the output digest in the manifest equals the SHA-256 of the image rebuilt by the digest rule above (the test rebuilds it itself, not through the writer); (e) a nonzero `manifest_off` past `total` is refused; (f) a manifest with `CF_IMAGE_V1` writes nothing.
  3. See it fail: build the image test (`cf_image_manifest` undeclared).
  4. Write the options, the header fields, the minor and the call. `ctest --preset default -R "image|gen_build"`.
  5. See that the test can fail: hash the whole image behind a run-time-false condition; (d) fails; put it back.
  6. D-213, `docs/ImageFormat.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-33: the manifest inside the image (D-213)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "image|gen_build"` exit 0; the minor-0 golden unchanged.
* **stop if**: M-12's header does not have `manifest_off` and `manifest_len` at 104 and 108 (the card then waits for M-12's correction); three failures of the same step.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): the card moves to 2.2.0, after Y-29, and M-12's header v2 reserves the manifest's place (bytes 104-111), so the image's layout is fixed once in 2.0.0 and no card of 2.0.0 needs a later one; the image has no section table, so the manifest is found through the header, not as a section (the maintainer's standing approval, 2026-11-27).
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "The image format 2.1 carries a build's manifest; `cf_image_manifest` (Y-33, D-213)." `docs/Decisions.md`: D-213.

---

## Y-34 - `gen check --report`: a predicted, value-blind report from files and a contract

* **id**: Y-34
* **goal**: `configforge gen check --schema app.schema.json [--report out.json] [--unknown warn|reject] <file>...` reads every file as the program would, in order, and with `--report` writes the value-blind report of what the files would configure, marked `"predicted": true`, exiting 0 unless a file cannot be read (2); without `--report` it behaves as today.
* **why**: `docs/research/analysis-tool.md` section 5, "First: Option B" (a predicted report from files, and Doctor on it), with section 4's Option B for the mechanism. It reuses the report and Doctor's rules the project already has, for every format the library reads, with places. The research asks the tool to start by doing what the others do (yaml-language-server, Taplo, conftest); Y-36 gives that, Y-34 is what differs.
* **needs**: nothing
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Decided 1 (where the static mode lives)**, research section 5, question 1: **(a)** `configforge gen check --report`, then `doctor lint` (no new program; Doctor keeps its non-goal; `gen` is already the contract's program). **Rejected:** (b) a new `configforge check` (a third meaning of "check" beside `codec check` and `gen check`); (c) Doctor reads files itself (it breaks `docs/Doctor.md:853`, "Resolving configuration itself").
  * **Where.** `gen_check` (`tools/gen/contract.c:50-100`; its usage text `contract_usage`, 38-46). Its loop gains `--report <path>` and `--unknown warn|reject` (default `reject`, as today at `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`; with `warn` the call is `CF_UNKNOWN_WARN` (`lib/config/include/configforge/cf_define.h:318-322`), so unknown keys are candidates that CFD005 reports); the usage line gains both. An option given twice, or `--report` without a path, is `contract_usage()` (exit 2).
  * **With `--report`**, after `cf_builder_create`: the configuration is built with a builder that keeps going past a refused value when the unknown mode is `warn` (a refused value is a refused candidate in the report: research section 4, Option B). The report is written with `cf_report` (`lib/config/include/configforge/configforge.h:666`) from a `cf_report_info` of `CF_REPORT_BLIND` whose `profile` is NULL, `application` the schema's title when it has one, and `generated` NULL (a file made from the same inputs is the same bytes). The file is written whole or not at all (`cf_platform_write_file`, F-1a) when `--report` names a path, and to stdout when it is `-`.
  * **The mark.** `cf_report_info` gains a last member `cf_bool predicted;` (lib/config/include/configforge/cf_report.h:45-52; its `size` field makes a new last member an addition, the `CF_REPORT_INFO_INIT` at line 55 gains a `CF_FALSE`, `CF_REPORT_INFO_INIT` is `{sizeof(cf_report_info), CF_REPORT_BLIND, NULL, NULL, NULL, NULL, CF_FALSE}`); `cf_report_write` (`lib/config/src/*/report.c`; find `"profile"` with `grep -n '"profile"' lib/config/src/*/report.c`) writes `"predicted": true` beside it only when the field is true and the info's `size` covers it, else nothing (an older caller's report is byte for byte unchanged). The cf_report format version does not change (an added member).
  * **What exit codes mean.** 0 done (without `--report`: every file holds to the contract, as today; with `--report`: the report was written, whatever the findings are: Doctor judges them); 1 only without `--report` (a file does not hold); 2 unusable input (a file or the schema cannot be read, or the report cannot be written). A file in a program's own codec or repair (D-103) that `gen` cannot load is "cannot read", exit 2, never a pass (research section 4, Option B, "Rules out").
  * **D-214** ("A predicted report is the contract's program's, and Doctor judges it"): Question: how a repository checks its configuration files before a release without running the program. Options: recipes only (Y-36, always); a predicted report from `gen check` then Doctor (decided); a new `configforge check`; Doctor reading files (breaks `docs/Doctor.md:853`). Decision: `gen` resolves, Doctor judges; the report says `"predicted": true`; `--unknown warn` makes unknown keys candidates; exit 0 with `--report` unless a file cannot be read. Rejected: findings about the program's own codecs; a checker that touches the machine it runs on (research section 3, failure mode 6).
* **files**: `tools/gen/contract.c`, `lib/config/include/configforge/cf_report.h`, the report writer (found by the grep above), `tools/gen/tests/` (the gen tool's tests: `ls tools/gen` shows none: put the new cases in `tests/gen_check/` and add `add_test` lines in `tools/gen/CMakeLists.txt` after the existing gen tests: find them with `grep -n add_test tools/gen/CMakeLists.txt`), `tests/gen_check/*` (new: `app.schema.json`, `good.toml`, `bad.toml`, `bad.yaml`), `docs/Tools.md`, `docs/Doctor.md` (a paragraph); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-34 main`.
  2. Make the fixtures: a schema with `server.port` (integer, 1-65535), `log.level` (enum `debug|info|warn`), `db.password` (sensitive string), `old.key` (deprecated); `good.toml` (all four valid), `bad.toml` (`server.prot = 1`, `server.port = 70000`, `log.level = "loud"`, and `old.key = 1`), `bad.yaml` (the same keys in YAML, with `server.port` twice).
  3. Tests (shell-free, through `add_test` with `COMMAND` and `PASS_REGULAR_EXPRESSION` or the gen tool's existing test style): (a) `gen check --schema S good.toml` exits 0 and prints `ok: 1 file(s)`; (b) `gen check --schema S --report r.json --unknown warn bad.toml` exits 0, `r.json` contains `"predicted": true` and `"server.prot"` as a candidate whose place starts `bad.toml:1`; (c) `configforge doctor lint r.json` (after Y-35) is Y-35's test; here a `grep` of `r.json` for CFD-relevant data suffices: `"server.port"` has a refused candidate with `bad.toml:2`; (d) `--report -` writes the JSON to stdout; (e) a file that cannot be read exits 2 and writes no report; (f) two `--report` options exit 2; (g) `bad.yaml`'s duplicate key shows both places; (h) running the same command twice gives byte-identical reports (`cmp`); (i) the report contains no value text: `grep -c "70000" r.json` is 0 (value-blind).
  4. See it fail: `cmake --build --preset default --target gen 2>&1 | tail -n 5 && ctest --preset default -R gen_check 2>&1 | tail -n 20` (`--report` is a usage error today).
  5. Write the option parsing, the `predicted` member and the report writing. `ctest --preset default -R "gen|report|doctor" 2>&1 | tail -n 30` (every existing report test unchanged).
  6. See that the test can fail: write `"predicted": true` always behind a run-time-false condition; (a report of an older caller changes) the existing report goldens fail; put it back.
  7. D-214, docs, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-34: gen check --report, a predicted report from files (D-214)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_check|test_report"` exit 0.
* **stop if**: an unknown key under `CF_UNKNOWN_WARN` is not in the report as a candidate (the research asks a card to confirm it: report what `cf_report` writes, the card then adds the candidate to the report writer); a refused value under `warn` aborts `cf_builder_create` (report: the builder needs a mode that keeps going, which is a new public choice); the existing report goldens change; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`configforge gen check --report`: a predicted, value-blind report from files and a contract, for `doctor lint` (Y-34, D-214)." `docs/Decisions.md`: D-214. `docs/Roadmap.md`: the item "An analysis tool" under "From the maintainer, 2026-11-26" stays 🟨 until Y-37 and Y-40-m.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve your recommendations"): Decided 1 is **(a)**: `gen check --report`, then `doctor lint`; no new program.

## Y-35 - Doctor on a predicted report: the rules a running program is needed for are off and named; SARIF columns and fingerprints

* **id**: Y-35
* **goal**: `configforge doctor lint` on a report marked `"predicted": true` turns off the rules that need a running program, says which in its output, and its SARIF regions carry `startColumn` and each result a `partialFingerprints` entry.
* **why**: `docs/research/analysis-tool.md` section 5 ("Two small changes in Doctor come with it") and section 6, second row. Without it a predicted report makes false findings (CFD018 "set but never read", CFD008's default-only in production) and GitHub loses a finding's identity when lines move (sources [33], [34]).
* **needs**: Y-34
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Which rules are off** on a predicted report: CFD018 (set but never read: nothing ran), CFD008 (default-only in production: no profile is known), and CFD007's half about files readable by every user of the machine (the machine is not the program's: only its command-line half, which a predicted report cannot have either, so CFD007 is off whole). Everything else runs: CFD001, 004, 005, 006, 009, 010, 011, 013, 015, 016, 019. Each off rule is named once in the output as `note: CFD018 is off: the report is predicted from files, not observed`; in `--format sarif` the notes are in `runs[0].invocations[0].toolExecutionNotifications` (`level: "note"`, `message.text` the same sentence).
  * **Where**: `tools/doctor/engine.c`: the rule tables `g_lint_rules` (line 1867) are run by `doctor_lint_reports` (line 1880); the `report` struct (read at 209: `r->profile = text_at(&r->doc, "profile")`) gains `int predicted` read from `"predicted"`; `doctor_lint_reports` skips the three rules when it is set and prints the notes before the findings. A rule skipped this way never appears as a finding; a `--severity` or profile line naming it is ignored with the same note.
  * **SARIF** (`tools/doctor/doctor.c:130-152`): the `region` object is `{ "startLine": <line>, "startColumn": <col> }` when the place has a column: `doctor_place_of` (find with `grep -n doctor_place_of tools/doctor/*.c`) returns `path:line` today; it gains a third out-parameter `col` (0 when the place is `path:line` only, then `startColumn` is not written). The place `app.ini:2:1` already carries one (research section 5). Each result gains `"partialFingerprints": { "configforge/v1": "<fnv1a of rule id, key and file>" }` made with the library's own FNV-1a (`cf_fingerprint_step`, as `engine.c`'s CFD019 uses it), 16 lowercase hex digits; the line is not part of it, so a finding keeps its identity when lines move.
  * **D-215** ("A predicted report switches off what only a running program can say"): the three rules, the notes, `startColumn`, `partialFingerprints` over rule, key and file. Rejected: running the rules and labelling their findings; a fingerprint that includes the line.
* **files**: `tools/doctor/engine.c`, `tools/doctor/doctor.c`, `tools/doctor/engine.h`, the Doctor tests (`ls tools/doctor` has none: the tests are in `tests/doctor/`; find them with `grep -n "doctor" tests/CMakeLists.txt | head`), `docs/Doctor.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-35 main` (after Y-34).
  2. Tests: (a) lint of Y-34's `r.json` (bad.toml) prints the three notes and no CFD018, CFD008 or CFD007 finding, and does print CFD005 for `server.prot` with a nearest-name hint, CFD006 for `server.port`; (b) `--format sarif` of it has `startColumn` where the place has one, `partialFingerprints.configforge/v1` of 16 hex digits on every result, and the `toolExecutionNotifications` notes; (c) the same findings with the line of `bad.toml` moved by inserting a comment line at its top give the same `partialFingerprints` and a different `startLine`; (d) a report with no `"predicted"` member lints exactly as before (golden unchanged); (e) a place without a column writes no `startColumn`.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20` (the new cases fail: no notes, no columns).
  4. Write the changes. `ctest --preset default -R "doctor|gen_check" 2>&1 | tail -n 30`.
  5. See that the test can fail: run CFD018 on a predicted report behind a run-time-false condition; (a) fails; put it back.
  6. D-215, `docs/Doctor.md` (the rules table gains a column note, "off on a predicted report", at the three rules; section on SARIF gets the two fields), CHANGELOG. Format; codemap and apiref, last. Commit `Y-35: Doctor on a predicted report; SARIF columns and fingerprints (D-215)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: Y-34 is not landed; the SARIF golden tests compare whole files and a column or a fingerprint cannot be added without changing a golden of a report that is not predicted (the new fields appear in every SARIF run, so those goldens change: that is expected and each changed golden is listed in the commit message; stop only if a changed golden shows anything else); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor on a predicted report turns off the rules a running program is needed for and says which; SARIF results carry `startColumn` and `partialFingerprints` (Y-35, D-215)." `docs/Decisions.md`: D-215.

---

## Y-36 - Recipes: the contract's schema in yaml-language-server and Taplo, conftest over a report with places

* **id**: Y-36
* **goal**: the Guide has a page "Checking files in an editor and in CI" that shows how to use the schema the library already writes (`cf_schema`, `configforge gen schema`) in yaml-language-server and in Taplo, and how to run conftest over a report with each candidate's place copied into `_loc`; each recipe is run by a test.
* **why**: `docs/research/analysis-tool.md` section 5 ("Then, in this order": 2, **A**, the recipes) and section 4, Option A. The analysis tool starts by doing what the others do (maintainer, 2026-11-26): a user gets completion and hover for YAML, TOML and JSON, and policy rules over a report, with no new code.
* **needs**: nothing (the report example is clearer after Y-34)
* **size**: small (documentation and examples)
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Where**: `docs/Guide.md`, a new section after "11. The editor, the report and Doctor" (heading at line 1315) as a subsection `### Checking files before they ship`; examples in `examples/recipes/` (new): `app.schema.json` (written once with `configforge gen schema --schema <contract>` and committed), `app.yaml` with the modeline `# yaml-language-server: $schema=app.schema.json`, `app.toml` with `#:schema ./app.schema.json` (Taplo's directive), `.taplo.toml` with `[[rule]] include = ["*.toml"] schema.path = "app.schema.json"`, and `policy.rego` for conftest.
  * **conftest over a report**: the recipe converts a report with `jq` so that every candidate carries `_loc` from its `at` (`jq '.keys |= map(. + {_loc: .at})'`: check the report's real member names first with `configforge gen check --report` from Y-34 on a fixture, or with an existing report in `tests/`), then `conftest test --policy policy.rego report.json`; `policy.rego` holds one rule: `deny[msg] { ... }` "`pool.max` is at least `pool.min`" with `msg` formatted with `_loc`. The Guide says what conftest cannot see (a rule that errors passes: research section 3, failure mode 8: write the recipe's rule with a test input that must be denied).
  * **Tests**: the examples are run by the suite only when the tools are on the PATH: `ctest -R recipes` prints `skipped: yaml-language-server not found` etc. (the schema's own validity is checked with the library: `configforge gen check --schema app.schema.json app.toml app.yaml` exits 0 on the good files and 1 on `bad.toml`).
  * No decision number: documentation. The Guide says: relations between keys are a recipe here (research section 5, relations between keys: option (a), recipes first; **Rejected** for now: relations written in the contract itself); Y-30-m and Y-39-m then add a fixed set of them, and `pattern`, to the contract.
* **files**: `docs/Guide.md`, `examples/recipes/*` (new), `examples/CMakeLists.txt` (or `tests/CMakeLists.txt`: where the example harness lives, `grep -rn "example\." tests/CMakeLists.txt | head -3`); last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-36 main`.
  2. Write the harness lines first (`ctest -R recipes`): the schema check of the good and bad files; each external-tool step skipped when absent.
  3. See it fail: `ctest --preset default -R recipes 2>&1 | tail -n 20` (no such test, then no files).
  4. Write the examples and the Guide subsection; make `app.schema.json` with the library: `build/tools/gen/gen --schema <the contract the examples use> --out examples/recipes/app.schema.json` (the exact form is in `docs/Tools.md`).
  5. `ctest --preset default -R recipes`; then, where `conftest`, `taplo` and `yaml-language-server` are installed, run each by hand once and write the exact command lines into the Guide as run.
  6. See that the test can fail: change a value in `bad.toml` to a valid one; the "exits 1" case fails; put it back.
  7. CHANGELOG. Format; codemap and apiref, last. Commit `Y-36: recipes for editors and conftest from the contract's schema`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R recipes` exit 0 (skips are exits 0).
* **stop if**: the schema the library writes is refused by yaml-language-server or Taplo when run by hand (report the message: it is a finding about `cf_schema`, not a Guide fix); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide page and `examples/recipes/` on checking files with yaml-language-server, Taplo and conftest from the contract's schema (Y-36)." Roadmap: the "conftest over a report" line (`docs/Roadmap.md:938-941`) is marked 🟩 by this card.

---

* **Organiser's answers**:
  * 2026-11-27, the organiser: relations between keys are recipes here (the research's option (a)); Y-30-m (a fixed set of relations) and Y-39-m (`pattern`) are the open cards for the contract itself (the maintainer's standing approval, 2026-11-27).

## Y-37 - A secret written in a checked file: CFD022

* **id**: Y-37
* **goal**: `configforge doctor lint` on a predicted report reports CFD022, an error, for each key the contract marks `CF_SENSITIVE` or that is named like a secret (D-84) when a file given to `gen check` holds a value for it; the value is never printed.
* **why**: `docs/research/analysis-tool.md` section 5, question 3, and section 6 (fifth row). `docs/Doctor.md:430` puts "whether a file is committed to version control" out of CFD007's scope; in the files mode the files are the repository, and a secret in one is the one secret finding that is a fact, not a guess.
* **needs**: Y-35, C-8b (CFD021 is C-8b's, so this rule takes the next number)
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Decided 3: (a)** a new rule CFD022: a `CF_SENSITIVE` key, or one named like a secret (D-84), has a value in a file given to `gen check`; error; the value never printed. **Rejected:** (b) leave it to gitleaks (a pattern scanner guesses; the contract knows which keys are secret). The Guide still names gitleaks for secrets outside the contract.
  * **The rule** (`tools/doctor/engine.c`, a new `rule_secret_in_file` added to `g_lint_rules` (line 1867) after `rule_sensitive_source`; catalogue entry in `docs/Doctor.md` after CFD021, in the table at line 422): runs only on a predicted report (a report from a running program has CFD007 for its sources); for each key whose definition is sensitive (`sensitive` member of the report's key entry, as `rule_sensitive_source` reads it: copy its test) or whose name `cf_key_looks_secret` (the D-84 check; find its real name with `grep -rn "D-84" lib/*/include | head -3`) says is secret, and whose winning or any candidate comes from a file (an `at` of `path:line` that is not `argv`/`env`), a finding `error`, id `CFD022`, key, place, and the text `'%s' is a secret and holds a value in '%s'; keep secrets out of files that are checked in (give it by the environment, a secrets directory or a credentials source)`. No value, no digest, no length: the report is value-blind already, and the message never quotes the candidate's text (a test greps for it).
  * **Blind-safe**: yes (it reads presence and origin only), so the column in `docs/Doctor.md`'s table says `yes`; default severity `error`; the rule's profile defaults: none.
  * **D-216** ("A secret written in a checked file is a finding of its own"): the bullets above in one paragraph; Rejected: printing the value; a pattern scanner of values (gitleaks' job: research section 2).
* **files**: `tools/doctor/engine.c`, `docs/Doctor.md`, the Doctor tests, `tests/gen_check/` (a fixture `secret.toml` holding `db.password = "hunter2"`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-37 main` (after Y-35 and C-8b).
  2. Tests: (a) `gen check --schema S --report r.json secret.toml` then `doctor lint r.json` prints CFD022 for `db.password` with the place `secret.toml:1` and exit non-zero (the exit rule of `doctor lint` for an error finding: check with `ctest -R doctor`'s existing cases); (b) the output does not contain `hunter2` (`grep -c`); (c) a key named `api_token` that the contract does not mark sensitive but D-84's name check catches is reported; (d) the same file's secret given by the environment (a report from a running program, not predicted) is not CFD022 (CFD007 may speak); (e) with `--severity CFD022=warning` it is a warning.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule and the catalogue row. `ctest --preset default -R doctor`.
  5. See that the test can fail: report the value in the message behind a run-time-false condition; (b) fails; put it back.
  6. D-216, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-37: CFD022, a secret written in a checked file (D-216)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: C-8b has used a different number for its rule than CFD021 (take the next free number and say so in the commit); a report does not carry which candidate came from which source (the rule cannot tell a file from the environment): report it; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD022: a secret written in a file given to `gen check` (Y-37, D-216)." `docs/Decisions.md`: D-216. `docs/Roadmap.md`: "An analysis tool" 🟩 when Y-38 is done or closed.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve your recommendations"): Decided 3 is **(a)**: the rule CFD022 (a sensitive or secret-named key with a value in a checked file is an error; the value is never printed); the Guide still names gitleaks for secrets outside the contract.

## Y-38 - Defined but never read, over a set of reports: CFD023

* **id**: Y-38
* **goal**: `configforge doctor drift a.json b.json ...` (the command that reads a set of reports, `docs/Doctor.md` section 7e) reports CFD023, `info`, for a contract key that no report given read.
* **why**: `docs/research/analysis-tool.md` section 4, Option E (E2), section 5 question 4, section 6 (sixth row). CFD018 is "set but never read"; the other half, a definition the program stopped using, is the contract's dead code. One run cannot prove it (failure mode 12); a set of reports can.
* **needs**: C-8a, C-8b (the reports that record reads)
* **size**: small
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Decided 4: a rule over a set of reports, severity `info`** (the recommendation). **Rejected:** not at all (the contract's dead code stays invisible); a higher severity (a set of reports can still miss a way the program runs).
  * **The rule**: in `tools/doctor/engine.c`, over the set `drift` already reads (find the set command with `grep -n "drift" tools/doctor/doctor.c | head -5`): for each key in the contract of the first report (its `definitions`, i.e. keys with a default or required flag) such that no report in the set holds a read of it (the report member C-8a adds, which records reads; read C-8a's "decided" for the member's name and copy it), a finding `info`, id `CFD023`, key, and the text `'%s' is defined but no report read it (%d report(s) given); a run that never reads it cannot prove it is unused: give reports from every way the program runs`. It never fires with fewer than 3 reports (the text then says `needs at least 3 reports`: one note, no findings: one run cannot prove it, failure mode 12). A report that does not track reads (no member) makes the rule decline with a note `report <n> did not track reads`, no findings.
  * **Blind-safe**: yes; default severity `info`.
  * **D-217** ("Defined but never read needs a set of reports"): the bullets above; Rejected: a finding from one report; a count of reads.
* **files**: `tools/doctor/engine.c`, `tools/doctor/doctor.c`, `docs/Doctor.md` (the table after CFD022 and section 7e), the Doctor tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-38 main` (after C-8a, C-8b and Y-37).
  2. Tests with four hand-made reports (small JSON files in `tests/doctor/`): (a) key `old.knob` read in none of 3 reports: one CFD023 `info`; (b) read in one of them: no finding; (c) 2 reports: the `needs at least 3 reports` note and no finding; (d) one report without the reads member: the note and no finding; (e) a key read in all: nothing.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule. `ctest --preset default -R doctor`.
  5. See that the test can fail: report with 2 reports behind a run-time-false condition; (c) fails; put it back.
  6. D-217, docs, CHANGELOG, Roadmap. Format; codemap and apiref, last. Commit `Y-38: CFD023, defined but never read, over a set of reports (D-217)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: C-8a records no reads in a report; `drift` takes exactly two reports and cannot take a set (report: the command needs its own card); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD023: a definition that no report in a set read (Y-38, D-217)." `docs/Decisions.md`: D-217. `docs/Roadmap.md`: "An analysis tool" 🟩.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.
  * 2026-11-27, the organiser: question 4 is the recommendation, a rule over a set of reports, `info`; the 3-report floor is an option of the rule whose default is 3, by the answer above (the maintainer's standing approval, 2026-11-27).

## Y-39-m - Relations in the contract: `pattern`, beside Y-30-m's `if`/`then` and `dependentRequired`

* **id**: Y-39-m
* **goal**: the contract can say that a string value matches a pattern, in JSON Schema's word `pattern`, checked by the library and by every validator that reads the schema, with a bounded C89 regular-expression engine; the relations between keys (`if`/`then`, `dependentRequired`) are Y-30-m's, on the same C call.
* **why**: `docs/research/analysis-tool.md` section 4, Option D (D2) and section 5, question 2. Section 6 gives it no version: "after 2.3.0, no row yet".
* **needs**: Y-30-m (the C call `cf_builder_relation`), Y-36
* **size**: large
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided 2 (relations between keys): (b), split with Y-30-m**: the relations are Y-30-m's fixed set (decided once with that card's question D); this card adds `pattern` and its engine, behind `option(CF_WITH_PATTERN "the schema's pattern, by a bounded regular-expression engine" OFF)`, OFF by default, ON in the `full` and `server` profiles. Without the option a schema's `pattern` is refused by name, as today, and `CF_REL_MATCHES` is `CF_E_CAPABILITY`. Rego or CUE over the report (Y-36) stay the recipe for anything more. **Rejected:** (a) none in the library (a pattern is the commonest rule a schema states, and refusing the keyword makes such schemas unusable); a dependency (PCRE2, RE2: not C89, not `misra`); a backtracking engine (unbounded time on a hostile pattern).
  * **Decided (where the engine lives): `lib/text/src/regex.c`**, internal (`CF_INTERNAL_API`, names `cf_regex_*` in `cmake/configforge.internal.symbols`), not a new component: it is a text utility of the same kind as `lib/text/src/escape.c`, and a component of one file would be all directory. **Rejected:** a new `lib/regex/` (D-100 asks a directory per component, not a component per file).
  * **The engine**: a Thompson-style matcher of a subset with no backtracking blow-up: literals, classes (`[a-z]`, `[^...]`, `\d`, `\w`, `\s`), `.`, `*`, `+`, `?`, `{m,n}` with `n` at most 1000, `^`, `$`, alternation, groups; no back-references, no look-around (refused at compile with the column: `pattern '%s': %s is not supported at column %u`). It matches anywhere unless anchored, as JSON Schema says. A step budget per match whose default is 1,000,000 (D-220), set by `cf_builder_pattern_budget(cf_builder* b, unsigned long steps)` (0 unlimited); one more step is `CF_E_VALIDATE`, `'%s': the pattern needs more than %lu steps`. A compiled pattern lives in the builder's arena.
  * **The C side**: Y-30-m's enum gains `CF_REL_MATCHES = 5`; `cf_builder_relation(b, CF_REL_MATCHES, key, NULL, 0, pattern, err)` (the pattern is compiled at the call: a bad pattern is `CF_E_DEFINITION` there). A mismatch is `CF_E_VALIDATE` (D-223) with the key and the place: `'%s' at %s does not match the pattern '%s'`; a sensitive key's value is never quoted (the pattern is).
  * **D-218** ("A pattern is checked by a bounded engine of our own, OFF by default"): the bullets above; Rejected: none in the library; a dependency; backtracking.
* **files**: `lib/text/src/regex.c` (new), `lib/text/include/configforge/cf_text.h` (the internal declarations; `CF_INTERNAL_API`), `lib/text/CMakeLists.txt`, `lib/config/src/bridge/schema.c` (`k_str` gains `pattern` under the option, line ~318), the relation check of Y-30-m, `lib/text/tests/test_regex.c` (new; `ls lib/text/tests` for the file pattern), `lib/config/tests/test_relation.c`, `tests/fuzz/regex.c` (new) and its CMake entry, `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `cmake/configforge.symbols`, `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-218), `docs/Limits.md` (the budget and `{m,n}` bound), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-39-m main` (after Y-30-m).
  2. Tests first: `test_regex.c` (under `CF_WITH_PATTERN`, otherwise prints `skipped: no pattern`): a table of pattern, text, expected (match, no match, refused at compile with its column), at least 60 rows over every construct; `(a*)*b` on 10,000 `a`s ends inside the default budget with no match (linear time), and with the budget at 100 is refused; a back-reference `\1` is refused. In `test_relation.c`: a schema with `"pattern": "^[a-z]+$"` on `name`: `abc` passes, `aB1` is refused with the place; without the option the schema is refused by name (the existing behaviour, the test guarded the other way).
  3. See it fail: `cmake -S . -B build-pattern -DCF_WITH_PATTERN=ON && cmake --build build-pattern --target test_regex 2>&1 | tail -n 20`.
  4. Write `regex.c`. `ctest --test-dir build-pattern -R test_regex 2>&1 | tail -n 20`. Then the keyword in `schema.c`, `CF_REL_MATCHES`, `cf_builder_pattern_budget`. `ctest --test-dir build-pattern -R "test_regex|test_relation|test_schema|gate"`; the default build's `ctest --preset default` unchanged.
  5. See that the test can fail: drop the step count behind a run-time-false condition (never a constant); the budget case fails; put it back.
  6. The fuzz target `tests/fuzz/regex.c` (input split at the first NUL into a pattern and a text; never a crash, never more steps than the budget); an hour: `cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=3600 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` with `CF_WITH_PATTERN=ON`.
  7. `docs/Limits.md`, the Guide's relations paragraph (Y-30-m's) gains `pattern`, D-218, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-39-m: pattern in the contract, a bounded regular-expression engine, CF_WITH_PATTERN (D-218)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --test-dir build-pattern -R "test_regex|test_relation"` exit 0; a fuzz hour on the engine with no hang and no finding.
* **stop if**: Y-30-m's `cf_builder_relation` is not landed; the `misra` preset refuses the engine with a deviation `docs/Compliance.md` does not list (report the rule); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "the schema's `pattern`, checked by a bounded engine, `CF_WITH_PATTERN`, OFF by default (Y-39-m, D-218)"; `docs/Decisions.md`: D-218.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". Question 2 is (b), split: the relations are Y-30-m's (its question D, answered once for both), this card is `pattern` behind `CF_WITH_PATTERN`, OFF by default; the engine is in `lib/text`; the C call is Y-30-m's with `CF_REL_MATCHES`; version 2.3.0 kept (the maintainer's standing approval, 2026-11-27).

---

## Y-40-m - The editor's hover: the report's origins through the schema, with no server of ours

* **id**: Y-40-m
* **goal**: the editor shows, on hover over a key, the contract's description, default and range, and, from a report, what overrides it in production, through the language servers people already use.
* **why**: `docs/research/analysis-tool.md` section 5, "Then, in this order" 7 (**F**, the language server, last) and section 6 (last row, 2.3.0, large); the Roadmap's "Later, and large" item (`docs/Roadmap.md:946-948`). The research's reading of the field (yaml-language-server, Taplo, Even Better TOML) is that Y-36's recipes cover completion and hover from the schema already; the server adds what a schema cannot: the report's origins.
* **needs**: Y-34, Y-36
* **size**: medium
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided (whether to build a server): no server of ours.** The existing servers (yaml-language-server, Taplo / Even Better TOML) already show a schema's `description` and `markdownDescription` on hover (Y-36's recipes); what they cannot know is the report's origins. So `configforge gen hover --schema S --report r.json [--out S.hover.json]` writes a copy of the contract's schema whose every key carries a `markdownDescription`: its description, default, range or enum, sensitivity, and, from the report, which source and place set it and what it overrides (value-blind: the report holds no values, and a sensitive key shows only its source). The editor is pointed at `S.hover.json` by Y-36's recipe. A subcommand of `gen`, no new program, no dependency, no public C interface. **Rejected:** a C program speaking LSP over stdio in `tools/lsp/` (a protocol server, its JSON-RPC and its editor plugins for one hover line the existing servers already render; `docs/research/analysis-tool.md` section 5 asks for the second option's cost to be measured first, and this card is that option); never (the report's origins stay out of the editor).
  * **Where.** `tools/gen/contract.c` beside `gen_schema` (`int gen_hover(int argc, char** argv)`, usage `hover_usage` in `gen_schema`'s style, line ~2713), declared in `tools/gen/gen.h`, dispatched in `gen.c`'s `main` after `schema`. The schema is loaded with `cf_defset_schema_file`; each definition's description is replaced in a copy by the composed text; the result written with `cf_schema_write` (as `sd_write` does) plus a `markdownDescription` member per property (the writer gains it if it has none: an added member, written only when the text is set, so `gen schema`'s output is byte for byte unchanged). The report is read as `doctor` reads one (`grep -n "report" tools/doctor/doctor.c | head`). A key the report does not hold says `not set by any source in this report`. Exit 0 written, 2 unusable input (D-223: the file and the reason).
  * **D-219** ("The editor's hover comes from the schema, written with the report's origins"): the bullets above; Rejected: a language server of our own; never.
* **files**: `tools/gen/contract.c`, `tools/gen/gen.c`, `tools/gen/gen.h`, the schema writer (`grep -n "cf_schema_write" lib/config/src/*/*.c`), `tests/gen_hover/` (new: Y-34's `app.schema.json`, a report, the expected `app.hover.json`), `tools/gen/CMakeLists.txt` (the tests), `docs/Tools.md`, `docs/Guide.md` (Y-36's recipe gains the hover file); last: `CHANGELOG.md`, `docs/Decisions.md` (D-219), `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/Y-40-m main` (after Y-34 and Y-36).
  2. Tests (Y-34's `add_test` style): (a) `gen hover --schema app.schema.json --report r.json --out h.json` exits 0 and `h.json` equals the committed `app.hover.json` (`cmp`); (b) `server.port`'s `markdownDescription` names `site.toml:2` and the default it overrides; (c) `db.password`'s names its source and holds no value text; (d) a key not in the report says so; (e) `gen schema` on Y-34's fixtures is byte-identical to before (`cmp` against a golden committed as the card's first commit); (f) a missing report is exit 2.
  3. See it fail: `ctest --preset default -R gen_hover 2>&1 | tail -n 20` (no `hover` subcommand).
  4. Write `gen_hover` and the writer's `markdownDescription`. `ctest --preset default -R "gen_hover|gen_schema|gen_check"`.
  5. See that the test can fail: write the description without the report's origin behind a run-time-false condition; (b) fails; put it back.
  6. `docs/Tools.md`, the Guide's recipe, D-219, CHANGELOG, the Roadmap's "Later, and large" item (`docs/Roadmap.md:946-948`) marked 🟩 with one line: hover through the schema, no server of ours. Format; codemap last. Commit `Y-40-m: gen hover, the report's origins in the schema's hover text (D-219)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "gen_hover|gen_schema"` exit 0.
* **stop if**: `cf_schema_write` drops a keyword the input schema holds (Y-30-m's relations, `pattern`), so the hover copy would validate less than the contract (report: the writer needs them first); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added": "`configforge gen hover`: a schema whose hover text carries the report's origins, for yaml-language-server and Taplo (Y-40-m, D-219)"; `docs/Decisions.md`: D-219.
* **Organiser's answers**:
  * 2026-11-27, the organiser: opened from "parked". No language server of ours: the second option of the card (the existing servers through the schema), built as `gen hover`, since its cost is one subcommand and a writer member; size medium; version 2.3.0 kept (the maintainer's standing approval, 2026-11-27).

## D-228 - Extensions keep the formats standard (the maintainer, 2026-11-26)

Approved in a side session ("I approve"). Copied into `docs/Decisions.md` by Y-42, the first card that lands under it. Cards: Y-42 (include and import), Y-41-m (simple expressions, off by default). Next free id after them: Y-43.

1. The syntax always stays valid TOML, JSON or YAML: other tools read our files without an error.
2. Each extension is enabled by an option of each source, `cf_file_source_options.extensions` (`include`, `import`, `ref`, `expr`), and `--extensions` in `gen build`. `${key}` stays on by default (as today); `include` and `import` are off by default (a file that makes the library read other files is a choice of security).
3. A double extension, `app.cfx.toml`, turns the extensions on for that file while editors still see `.toml`; no new extensions such as `.tomlx`.

## Y-42 - Include and import, in standard syntax: `"$include"` and `"$import"`, a root, a depth, a loader that plugs in (D-228)

* **id**: Y-42
* **goal**: a file source whose options (or whose name, `app.cfx.toml`) turn the extensions on reads `"$include": "base.toml"` (the whole document) and `"$import": "shared.yaml#/database"` (a part, addressed by a JSON Pointer, RFC 6901) in the table that holds them, across formats, from the file's own directory and below only, to a depth an option sets (16 by default), with cycles always refused, provenance naming the real file, a changed included file noticed by `cf_changed` and the watchers, and the loader that reads a referenced document a part a program can replace; with the defaults nothing changes.
* **why**: D-228: a base file and per-environment overrides, or a shared fragment, are the commonest reason to leave one file; today a program copies them into one by hand or writes the merge itself. The syntax is a plain key of each format, so every other tool still reads the file; reading other files stays a choice (off by default), because a configuration that makes the library open files is a security decision (`docs/Safety.md`).
* **needs**: nothing
* **size**: large
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The syntax (D-228 point 1).** A scalar member named exactly `$include` or `$import`, with a text value, in any table (the root too, and a table that is an array element). TOML `"$include" = "base.toml"`, JSON `"$include": "base.toml"`, YAML `$include: base.yaml`, INI and `.properties` `$include = base.ini` in a section. Nothing else is recognised: a member that is not a scalar, or has a number or a boolean value, keeps today's handling (a key the key rules refuse, `cf_key_valid`: with the extension off the key is refused exactly as before; the test records the message). The value of `$include` is a path; the value of `$import` is `path#pointer` or a bare `path`, which is the same as `$include`. A `#` in an `$include` value is refused (`CF_E_SOURCE`, `'$include' takes a document; use '$import' for a part`).
  * **Where it lands.** The referenced document's keys are added at the point of the `$include` member, as if the document had been written there, under the table that holds the member (the root: at the top). Every key arrives at the including source's rank, in document order, and within one source the later occurrence wins (`lib/config/src/core/keyspace.c`, the sort of candidates, "within one source the later occurrence wins"): a key set after the member in the including file overrides the included one, a key set before it loses to it. The Guide says so in one sentence and shows the usual order (the member first). `${key}` references in an included document are read as in any document, after precedence (nothing new).
  * **The part (`$import`).** The pointer addresses the imported document by RFC 6901: `""` or a missing `#` is the whole document, `/a/b` the member `b` of `a`, `/tags/0` the first element of an array (the segment is the element's decimal index, as the flattener numbers it), `~1` is `/` and `~0` is `~`. The part must be a table or an array; a pointer to a scalar is refused (`CF_E_SOURCE`, `'<pointer>' in '<file>' is a value; $import takes a part that has keys`) and a pointer that matches nothing too (`'<pointer>' matches nothing in '<file>'`). The part's keys are added under the table that holds the member, without the pointer's own path (`#/database` from `[db]` gives `db.host`, not `db.database.host`). It is done in the flatten sink: `lib/config/src/bridge/include.c` (new) has a document sink that forwards only the events below the pointer's segments and drops that prefix, over the same decoder (`cf_flatten_decode`, `lib/config/include/configforge/cf_flatten.h`); a segment that holds a `.` or is not a valid key is refused with the key rules' own error.
  * **The hook.** In `fl_scalar` (`lib/config/src/bridge/flatten.c:237`) before `fl_enter_member`: when the member name is `$include` or `$import`, the value is text, and the flattener has an extension hook (a new member `const cf_flatten_hook* hook` of `cf_flatten`, NULL by default and under `CF_INTERNAL_API`), call `hook->member(ctx, name, text, path_so_far)`, which expands and returns (CF_FALSE stops the decode with the error it set); otherwise as today. The hook is installed by `cf_source_read_blob` (`lib/config/src/bridge/file.c:90`) only when the source's `extensions` has `CF_EXT_INCLUDE` or `CF_EXT_IMPORT` set for the member's kind; with both off no code of this card runs.
  * **Formats across.** The referenced document is read by its own name's codec (`cf_codec_by_path`), else by its content (D-73, as `file_load` does), the encoding as the source's `flags` say; a TOML file can include a JSON one. A format that cannot be read in this build is the error `cf_builder_file` gives for it.
  * **The options (D-220, K-11a, D-51).** In `lib/config/include/configforge/configforge.h`, after `cf_builder_file`:
    ```c
    #define CF_EXT_REF     1u  /* ${key}: on by default, as today */
    #define CF_EXT_INCLUDE 2u  /* "$include" */
    #define CF_EXT_IMPORT  4u  /* "$import" */
    #define CF_EXT_EXPR    8u  /* reserved for simple expressions (Y-41-m) */

    typedef struct cf_include_loader {
        size_t size;
        /* Read the document `ref`, named as written after `from` (the name of the including document), into `ar`: set `out` (bytes and the name places show), and `*name` to the real name. CF_FALSE with err set. */
        cf_bool (*load)(void* ctx, const char* from, const char* ref, cf_arena* ar, cf_blob* out, const char** name, cf_error* err);
        void* ctx;
    } cf_include_loader;

    typedef struct cf_file_source_options {
        size_t                   size;       /* sizeof(cf_file_source_options): cf_file_source_options_init sets it */
        const char*              format;     /* as cf_builder_file's: NULL is the name's extension, then the content; default NULL */
        unsigned                 flags;      /* as cf_builder_file's (CF_OPTIONAL, the encoding bits); default 0 */
        unsigned                 extensions; /* CF_EXT_* bits; default CF_EXT_REF: include and import off */
        size_t                   max_depth;  /* how deep includes and imports nest; 0 is unlimited, chosen explicitly; default 16 */
        const char*              root;       /* the directory the file loader may reach; default NULL: the source file's own directory (none for a document in memory) */
        const cf_include_loader* loader;     /* what reads a referenced document; default NULL: the file loader below */
    } cf_file_source_options;

    CF_API void cf_file_source_options_init(cf_file_source_options* o);
    CF_NODISCARD CF_API cf_bool cf_builder_file_with(cf_builder* b, const char* name, int rank,
                                                     const char* path,
                                                     const cf_file_source_options* o);
    CF_NODISCARD CF_API cf_bool cf_extensions_parse(const char* text, unsigned* bits, cf_error* err);
    ```
    `cf_builder_file_with(..., NULL)` is `cf_builder_file(b, name, rank, path, NULL, 0)`; `cf_builder_file` is `cf_builder_file_with` over the matching options and stays as it is (the double extension below applies to both). `cf_extensions_parse("include,import", &bits, err)` gives `CF_EXT_REF` plus the named ones (`ref` adds nothing, `all` is include and import, the names are lowercase, separated by commas, spaces ignored); an unknown name is `CF_E_DEFINITION`, `unknown extension '<name>': ref, include, import, expr`; `expr` is `CF_E_CAPABILITY`, `extension 'expr' is not built yet`. It is public so that `gen build --extensions` (Y-24) and every binding read the same names. `cf_builder_file_with` refuses (held until `cf_builder_create`, as the builder's other failures are) an `extensions` that clears `CF_EXT_REF` (`CF_E_CAPABILITY`, `extensions: references cannot be turned off yet`) or sets `CF_EXT_EXPR` (the same message as the parse's), and an options `size` below `offsetof(cf_file_source_options, flags)` (D-51).
  * **The double extension (D-228 point 3).** A path whose last two extensions are `.cfx.` and a format's (`app.cfx.toml`, `app.cfx.json`, `base.cfx.yaml`) gets `CF_EXT_INCLUDE | CF_EXT_IMPORT` added to its `extensions` (and `CF_EXT_EXPR` when Y-41-m builds), by `cf_builder_file`, `cf_builder_file_with` and the drop-in directory source for each of its files; the format is still the last extension's (`cf_codec_by_path` already reads `.toml` there). No new extension names. A referenced document inherits the including document's extensions and root; it does not need `.cfx.` in its own name.
  * **The file loader (the default, `cf_include_loader.load` NULL).** `ref` is joined to the directory of `from` and the result collapsed lexically (`.` and `..` segments); it must stay under `root` (the options' `root`, normalised the same way, else the directory of the top source file): a `ref` that is absolute (a leading `/`, a drive or a UNC prefix) or that leaves `root` is refused (`CF_E_SOURCE`, `'<ref>' is outside the directory this file may include from (<root>)`); a `ref` with a scheme (`https://`) is refused (`this build reads only files; a cf_include_loader can add others`). It reads the file with `cf_file_read_stable` (`lib/config/src/bridge/file.c:61`, the same stable read a source uses). A path a symbolic link makes point out of the directory is the operator's choice: the check is of the path as written, which `docs/Safety.md` says in those words. A document held in memory (`cf_builder_document`) has no directory: with `root` NULL, an include in it is refused (`CF_E_CAPABILITY`, `a document held in memory has no directory to include from; set root`).
  * **Depth, cycles, size.** The nesting depth is counted from the top document (a document is depth 0): deeper than `max_depth` is `CF_E_SOURCE`, `includes nest deeper than <n>: <chain>` (0 is unlimited). Cycles are always refused, whatever `max_depth` is: the stack of resolved names is kept and a name already on it is `CF_E_SOURCE`, `'<a>' includes itself: <a> -> <b> -> <a>`. The decode budget of the including document (`cf_flatten_budget`, D-82) is shared by everything it includes, so a file that includes itself many times over cannot make more than the budget.
  * **Provenance.** Each candidate's place is the included file's own `<file>:<line>`, so `cf_explain` and Doctor name the real file; the source's name is unchanged.
  * **Watching.** The file source records the files its last load read (the including file excluded: it is the source's `path`) in its state, and a new member appended to `cf_source_vtable` (D-51: `size` first, read only for a table that reaches it), `size_t (*extra_paths)(const cf_source* s, const char* const** list)`, NULL for every other source, returns them; the loop in `lib/config/src/core/config.c` that collects each source's `cf_source_path` (the line that stamps `watch_paths`) collects these too, with the watch arrays sized after the sources are loaded (the two `cf_arena_calloc` calls that size them now move after the load, or are sized by a first count; the worker finds which with `grep -n "watch_paths" lib/config/src/core/config.c`). `cf_changed`, `cf_watcher_open` and `cf_live` then notice a change of an included file. A `cf_derive` keeps the original's files as it keeps the original's own.
  * **Errors (D-223).** Every refusal above names the including file and the line of the member (`<file>:<line>: ...`) and the reference as written, with the codes given; an allocation that fails is `CF_E_MEMORY`. A sensitive key's value never enters a message (an included file's text is not quoted).
  * **Bindings.** The C++ wrapper (`cf::builder::file_with`) and the Rust wrapper (`Builder::file_with`) take an `extensions` string read by `cf_extensions_parse`, an optional `root` and a `max_depth`; one test each includes a file. Python follows with C-4c.
  * **D-228** is copied into `docs/Decisions.md` by this card as it stands above. **The new D-number**, the next free one: "A file includes another in its own syntax: `$include` and `$import`, off by default, under a root and a depth, through a loader that plugs in". Options: a directive of our own (`#include`, `<<:`) (rejected: not valid in all three formats); the including file's keys always winning (rejected: it needs a second pass and a rule for keys before and after; the document order is one rule the key space already has); a registry of loaders (rejected: no global state, D-101; the loader is an option).
  * **Rejected**: following symbolic links with a check (needs a platform call this card does not add); an include of a directory (the drop-in source does that); expressions here (Y-41-m).
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_flatten.h`, `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/include.c` (new; add to `lib/config/CMakeLists.txt` and `scripts/generate/amalgamate.py` as `serve.c` is in C-10a's list), `lib/config/src/bridge/flatten.c`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/dir.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_include.c` (new, in `lib/config/tests/CMakeLists.txt` after `test_file`), `bindings/cpp/` and `bindings/rust/` (the files `grep -rn "file(" bindings/cpp bindings/rust | head` names), `cmake/configforge.symbols` (generated), `cmake/configforge.internal.symbols` (generated); last: `docs/Guide.md` (a subsection "Files that include files" in the section on files), `docs/Safety.md` (a paragraph "Includes"), `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-42 main`. Write `lib/config/tests/test_include.c` (C89; files written with `spill()` from `tests/support/cf_fixture.h` into a directory under the working directory). Cases: (a) `app.toml` with `"$include" = "base.toml"` then `port = 9000`, base has `port = 1`, `host = "h"`, extensions `CF_EXT_REF | CF_EXT_INCLUDE`: `port` is 9000 and `host` is `h`; `cf_explain` of `host` names `base.toml:` with its line; with the member after `port`, base's wins; (b) the default options and a plain `cf_builder_file`: `app.toml` with the member is refused exactly as in the tree before the card (the test records the message and the same file with the extension on reads); (c) `app.cfx.toml` includes with no options; `app.toml` does not; (d) across formats: JSON including TOML and YAML; (e) `$include` in a table (`[db]` with `"$include" = "db.toml"` gives `db.host`); (f) `$import` of `shared.yaml#/database` into `[db]`, of `#/tags/0` (a scalar: refused), of `#/nope` (matches nothing), of `#/a~1b` (a member named `a/b` is refused by the key rules, with their message), of the whole document; (g) a ref `../x.toml` refused, `/etc/x.toml` refused, `sub/y.toml` read, `sub/../z.toml` read (stays under root), `https://h/x.toml` refused with the loader sentence; with `root` set to the parent directory `../x.toml` is read; (h) depth: a chain of 17 files refused with the default (`includes nest deeper than 16`), read with `max_depth = 20` and with 0; (i) a cycle `a` -> `b` -> `a` refused with the chain, also with `max_depth = 0`; (j) a program's own loader serving `mem:base` from memory; (k) a missing included file: the error names the including file and line and `CF_OPTIONAL` does not excuse it; (l) the sensitive case: an included file's key declared `CF_SENSITIVE` never shows its value in an error; (m) watch: `cf_changed` is false, then rewrite the included file (a stamp that differs: wait for a changed mtime as `test_watch.c` does) and it is true; (n) `cf_derive` keeps it; (o) the in-memory document refusal; (p) `cf_extensions_parse` of `include,import`, `all`, ` include , ref `, `expr` and `nope`; (q) a drop-in directory with `10-app.cfx.toml`; (r) the budget: a file including a 1 MB file 100 times is refused by the decode budget, not by memory.
  2. See it fail: `ctest --preset default -R test_include --output-on-failure | tail -n 30` (undeclared names).
  3. The options struct, `cf_file_source_options_init`, `cf_builder_file_with`, `cf_extensions_parse`, the `cf_builder_file` shortcut and the double extension. `ctest --preset default -R "test_include|test_file|test_dir"`: cases b, c, p, q and the existing tests pass.
  4. The flatten hook, `include.c` (the loader, the stack, the depth, `$include`), then `$import` (the filtering sink), then the vtable member and the watch. After each, `ctest --preset default -R "test_include|test_file|test_watch|test_live"`.
  5. See that the test can fail: let the containment check pass every path behind a run-time-false condition (a compared run-time value, never a constant: MSVC's C4127); case (g) fails; put it back.
  6. The bindings and their tests; the Guide subsection and the Safety paragraph; D-228 and the new D-number; CHANGELOG. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-42: include and import in standard syntax, off by default (D-228)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX (the `sanitize` preset runs `test_include`); ACCEPT-WINDOWS (path cases with `\` and a drive prefix are in the test under `#if` for the platform); ACCEPT-CI; `ctest --preset default -R "test_include|test_file|test_dir|test_watch|test_live"` exit 0; `gate.abi` shows only additions (the types are not symbols; the three functions, `cf_file_source_options_init`, `cf_builder_file_with` and `cf_extensions_parse`, are); `gate.options`, `gate.nodiscard`, `gate.guarantees` exit 0 (the new names carry their `Guarantee:` lines: `cf_file_source_options_init` `nothrow`, `cf_builder_file_with` `basic: the failure is held by the builder; cf_builder_create reports it`, `cf_extensions_parse` `strong`); every existing test and golden file unchanged.
* **stop if**: the flattener cannot call a hook before it builds the member's key without a change to the codec sinks' signatures (report which); the watch arrays cannot be sized after the load without a change to `cf_derive`'s inheritance of watched files (report); `F-2a` landed and defined `cf_file_source_options` with other members than these (then this card adds `extensions`, `max_depth`, `root` and `loader` to it and writes what it found under notes); the worker passes 60 turns after step 4's first part (include working): the card is split there, `$import` and the watch becoming a second card; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.1.0 — unreleased" (the heading is added above the newest one when it is not there), "### Added": "`\"$include\"` and `\"$import\"` in a file's own syntax, off by default (`cf_file_source_options.extensions`, or the double extension `app.cfx.toml`), under a root and a depth, with the loader a part that plugs in; `cf_builder_file_with`, `cf_extensions_parse` (Y-42, D-228)"; `docs/Decisions.md`: D-228 and the new D-number; `docs/Guide.md` and `docs/Safety.md` as the steps say; `docs/Roadmap.md`: the row for includes, if there is one (`grep -n "include" docs/Roadmap.md | head`).
* **Organiser's answers**:
  * `cf_file_source_options` and `cf_builder_file_with` are defined by this card (module 1, 2.1.0) and not by F-2a (D-225, module 5, 2.3.0): F-2a appends `writable` and `encoding` to the struct as it stands and does not define it again; F-2a's text says so (the maintainer's standing approval, 2026-11-27).
  * `--extensions` of `gen build` is Y-24's step: this card gives it `cf_extensions_parse` and the four names (the maintainer's standing approval, 2026-11-27).
  * Turning `${key}` references off per source, and `expr`, are not built here: D-228 names the bits, and both are refused with `CF_E_CAPABILITY` until a card builds them (Y-41-m for `expr`) (the maintainer's standing approval, 2026-11-27).
  * Position decides precedence inside a document (the later occurrence wins, as the key space already decides for one source), so no second pass and no new rule (the maintainer's standing approval, 2026-11-27).
  * The path check is lexical; symbolic links are not resolved, for want of a platform call, and `docs/Safety.md` says so (the maintainer's standing approval, 2026-11-27).

## Y-41-m - Simple expressions: `"${= 8000 + 80}"`, total, inside a string

* **id**: Y-41-m
* **goal**: with `CF_EXT_EXPR` on a source, a string value that holds `${= expression}` is evaluated: string concatenation and `+ - * /` on integers and reals, with no functions, loops or conditions, so that `"${= 8000 + 80}"` is the integer 8080 and `"http://${host}:${= ${base.port} + 80}/"` is a text, every file still valid TOML, JSON or YAML.
* **why**: D-228: a port derived from another, a timeout in milliseconds written as seconds times 1000: small arithmetic that people otherwise ask a templating language for. Total (it always ends) and tiny, because more power is CUE, Pkl, Jsonnet or Dhall as front ends (Y-31's recipe), never a language of our own. Off by default: a file is read as today unless the source turns `expr` on.
* **needs**: Y-42
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Where.** An extension of D-228 (point 2): the bit `CF_EXT_EXPR` that Y-42 reserves; `cf_file_source_options.extensions` and `cf_extensions_parse("expr")` accept it once this card builds it (they refuse it with `CF_E_CAPABILITY` before), and the double extension `app.cfx.toml` turns it on with include and import. Without the bit a `${=` is read as today (a reference to the key `=...`, refused by the key rules as it is now).
  * **The syntax.** `${=` then an expression then `}`, inside a string value, anywhere in it, any number of times. The expression: integer literals (decimal, optional leading `-`), real literals (a point or an exponent), text literals in single quotes (`'a'`, a quote doubled to write one), parentheses, binary `+ - * /` and unary `-`, with the usual precedence (`* /` above `+ -`, left to right). `${key}` references inside the expression are replaced by their values first, by the machinery that already resolves references after precedence (`CF_CAND_TEMPLATE`), so the expression sees the numbers or the text: `${= ${base.port} + 80}` with `base.port` 8000 is 8080.
  * **The types.** Integer and integer give integer (`/` on two integers is exact division and refused when it leaves a remainder: `7 / 2` is `CF_E_CONVERT`, `7.0 / 2` is 3.5); an integer and a real give a real; `+` on two texts concatenates; a text and a number is refused (`cannot add text and int`); every other operator on a text is refused. When the value is exactly one `${= ...}` and nothing else, the result has the expression's type (a number the key's definition can take as int or real, or a text); when there is other text around it, the result is text with each expression's result written as `cf_i64_to_text` or the shortest real (`cf_f64_to_text`) writes it.
  * **Total and bounded (D-220).** There is no way to loop, call or branch; the evaluation ends. An option `expr_max_ops` of `cf_file_source_options` (appended; default 256 operations and literals for one expression; 0 is unlimited) bounds the size of an expression; one more is `CF_E_CONVERT`, `an expression of more than <n> operations`. Integer overflow past 64 bits, division by zero and a real result that is not finite are `CF_E_CONVERT` (`integer overflow`, `division by zero`, `not a finite number`); a syntax error names the column in the value.
  * **Errors (D-223).** Each refusal has the key, the place (the file and line of the value) and a reason `CF_REASON_EXPRESSION` (`"expression"`, "an expression that cannot be evaluated"); a sensitive key's expression error does not quote the value or its operands.
  * **Rejected**: functions, conditions, loops, variables, a standard library (a language of our own: CUE, Pkl, Jsonnet and Dhall exist, and Y-31 shows how to use them); `%` and bit operators (not asked); evaluating at read time of the file instead of after references (a reference would not be usable in an expression).
* **files**: `lib/config/src/bridge/expr.c` (new), `lib/config/src/core/keyspace.c` or the reference expansion found by `grep -n "CF_CAND_TEMPLATE" lib/config/src/core/*.c`, `lib/config/include/configforge/configforge.h`, `lib/base/include/configforge/cf_reason.def`, `lib/config/tests/test_expr.c` (new), the bindings' option strings and one test each, `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-41-m main` (after Y-42). Test first: `test_expr.c` with `"${= 8000 + 80}"` (int 8080), `"${= 1 + 2 * 3}"` (7), `"${= (1 + 2) * 3}"` (9), `"${= 7 / 2}"` (refused), `"${= 7.0 / 2}"` (3.5), `"${= 'a' + 'b'}"` (`ab`), `"${= 'a' + 1}"` (refused), `"http://h:${= ${p} + 80}/"` (text), division by zero, overflow, an expression of 300 operations (refused with the default, read with `expr_max_ops` 0), the extension off (a `${=` as today), and `app.cfx.toml` on.
  2. See it fail: `ctest --preset default -R test_expr --output-on-failure | tail -n 20`.
  3. The evaluator (`expr.c`, a recursive-descent parser over the bounded text with an explicit depth limit of 32), the hook after reference expansion, the reason, the option member.
  4. Bindings; the Guide section "Simple expressions". Format; codemap and apiref last; ACCEPT; commit `Y-41-m: simple expressions in strings, off by default`.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `ctest --preset default -R "test_expr|test_include"` exit 0; every existing test unchanged.
* **stop if**: expansion of references cannot hand a number's text to the evaluator without a change to `CF_CAND_TEMPLATE`'s contract (report); a golden file changes with the extension off.
* **docs**: `CHANGELOG.md`, "### Added": "`${= ...}` simple expressions in strings, off by default (Y-41-m, D-228)"; `docs/Decisions.md`: the next free D-number, "Expressions are total and small: concatenation and four operators, bounded by an option".
* **Organiser's answers**:
  * Written under the maintainer's standing approval, 2026-11-27. It builds on Y-42's option and bit, and changes no default (the maintainer's standing approval, 2026-11-27).
  * 2026-11-27, the organiser: opened from "parked": the card is built as written, the extension off by default; version 2.1.0, Y-42's (the maintainer's standing approval, 2026-11-27).
