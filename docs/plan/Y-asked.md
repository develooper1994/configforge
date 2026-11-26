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

A **TO DECIDE** of a research stays a TO DECIDE here, in the card that
needs it, with its options and the research's recommendation; every card
around it is written so that an answer is the only thing missing. A card
whose TO DECIDE has no answer under "Organiser's answers" stops at that
step, with everything before it done and pushed.

Decisions of this group: D-186 (a write checks the file is still what it
was read from, Y-2), D-187 (the lock a program asks for, Y-3), D-188
(how long a write waits, and "busy", Y-4), D-189 (a file is read twice
and compared, Y-5), D-190 (every descriptor the library keeps is close-on-exec, Y-8),
D-191 (a second driver of a `cf_live` is refused, Y-9), D-192 (the read path is
measured before it is changed, Y-10), D-193 (a worker's reader handle, Y-11),
D-194 (workers told of a swap, Y-12), D-195 (a shared image, Y-13-m, written
when it opens).

Compression (`docs/research/compression.md`), Y-14 to Y-23-m: D-196 and
D-197 (a byte layer under the codecs, its bounds, Y-15), D-198 (gzip, Y-16),
D-199 (zstd, Y-17), D-200 (the sources read through the layer, Y-18), D-201
(writing, deterministic, Y-20), D-202 to D-204 (the parked built-in inflate,
xz and lz4, zstd dictionaries; written when they open). The configuration
compiler (`docs/research/config-compiler.md`), Y-24 to Y-33: D-205 to D-210
(Y-24 to Y-29), D-211 (relations across keys, Y-30-m), D-212 (a build file,
Y-32-m), D-213 (the manifest in the image, Y-33). The analysis tool
(`docs/research/analysis-tool.md`), Y-34 to Y-40-m: D-214 and D-215 (a
predicted report and Doctor on it, Y-34 and Y-35), D-216 and D-217
(CFD022, CFD023), D-218 and D-219 (relations in the contract, the language
server; written when they open). The analysis tool builds on C-8a and C-8b
(`docs/plan/C-change-2.md`), which it does not repeat.

---

## Approved by the maintainer, 2026-11-26 (relayed from a side session: "I approve")

These two decisions replace Y-15's TO DECIDE 1, 4 and 5. **Y-15 to Y-20,
and Y-21-m to Y-23-m where they touch it, do not open until they are
rewritten to follow D-221** (the next organiser's first step: Y-15 is
split into Y-15a and Y-15b as point 7 says; each rewritten card carries
this answer under "Organiser's answers"). Both are copied into
`docs/Decisions.md` by the first card that lands under them.

* **D-220 - Parts that compose, options with defaults.** Every later
  design is made of atomic parts, each simple and usable alone, that
  compose into something rich. A limit is an option with a default
  value, never a fixed number ("in a system's design I always prefer an
  option and a default value to a hard limit", the maintainer). A layer
  is made only when a second real user of it exists: no empty layers.
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
* **goal**: `cf_platform_write_file_if(path, data, size, expect, expect_size, err)` replaces a file as F-1a's `cf_platform_write_file` does, but only when the file still holds exactly `expect` (or is absent when `expect` is NULL), checked after the temporary is complete and just before it is put in place, and returns -1 and leaves the file as the other writer left it when it does not; and `overrides_write` (F-2a, as amended) uses it to apply its change again to the new bytes, up to 3 times.
* **why**: two programs that read, change and replace one file lose one update, silently (D-128 as F-2a drafted it). The research recommends the optimistic check always, with no option and no new public name (`docs/research/concurrent-access.md:511-529`, "First: no new public interface", point 2): the race then lasts from a re-read to a rename, microseconds, instead of a whole read, change and emit, and a change wins key by key, not file by file. It is what etcd does between its locks and git's, and what a lock would still leave open for writers that do not take it.
* **needs**: F-1a
* **size**: small (one function in each of three files, the call in F-2a's card)
* **decided**:
  * **Names** (internal tier, `lib/platform/include/configforge/cf_platform_file.h`, under `#if CF_INTERNAL_API`, after F-1a's three names):
    ```c
    /* 1 when `path` holds exactly `size` bytes of `data`. -1 when the file no
       longer holds `expect` (`expect_size` bytes; NULL and 0: the file must not
       exist), with `err` set (CF_E_SOURCE, "'%s' changed while it was written")
       and the file as the other writer left it. 0 with `err` set for any other
       failure, as cf_platform_write_file. */
    CF_API int cf_platform_write_file_if(const char* path, const void* data, size_t size,
                                         const void* expect, size_t expect_size, cf_error* err);
    /* The same, calling `between(user)` once, after the temporary is complete
       and before the comparison: for the test, which changes the file there. No
       global state (D-101). NULL: no call. */
    CF_API int cf_platform_write_file_if_hook(const char* path, const void* data, size_t size,
                                              const void* expect, size_t expect_size,
                                              void (*between)(void* user), void* user,
                                              cf_error* err);
    ```
    `cf_platform_write_file_if` is `..._if_hook(..., NULL, NULL, err)`. Both are in the three `file.c` files (the serialization library carries them: `lib/platform/CMakeLists.txt:35-36`), and in the three lines each of `cmake/configforge.internal.symbols`, in that file's order.
  * **No code `CF_E_BUSY` yet.** That code arrives with Y-4; until then the refusal is `CF_E_SOURCE` and the return value -1 is what a caller tells it by. Y-4 changes the final refusal of the retries to `CF_E_BUSY`.
  * **Where the check goes.** F-1a's core is one static function in each `file.c` that does the steps and takes `fail_at`; this card gives it `expect`, `expect_size`, `between` and `user`. The check is after the last step before the rename (POSIX: after step 5, `close`; Windows: after step 4, `CloseHandle`) and is not an injectable step (`cf_platform_write_steps()` does not change). It is: call `between` if given; then read `path` whole with `cf_platform_fopen(path, "rb")` and `fread`, at most `expect_size + 1` bytes: equal when the read count is `expect_size` and `memcmp` is 0. When `expect` is NULL: the file must be missing (POSIX: `lstat` fails with `ENOENT`; Windows: `GetFileAttributesW` is `INVALID_FILE_ATTRIBUTES` with `ERROR_FILE_NOT_FOUND` or `ERROR_PATH_NOT_FOUND`), and any other answer is "changed". A file that cannot be read when `expect` is not NULL is "changed". On "changed" the temporary is removed as a failed step removes it and -1 is returned.
  * **Not decided here, and not needed:** an equal size and equal bytes with a different modification time is "unchanged": the check is of what the writer is about to destroy, which is bytes.
  * **D-186** (copy into `docs/Decisions.md` after the last D-number, titled "A write checks that the file is still what it was read from"): Question: two programs that read, change and replace one file lose one update. Decision: the check is of bytes, after the temporary is complete and just before the rename; a mismatch returns -1, the caller applies its change again to the new bytes up to 3 times and then refuses with `'%s' changed while it was written; try again`; no option, no new public name, no file. Rejected: a stamp (a hint that can miss a rewrite of the same length inside one clock tick, `docs/Decisions.md:1689-1695`); unbounded retries (a file two programs rewrite forever never settles; the third refusal is the answer); a lock for this (Y-3 is for the program that asks for one). Held to: `test_writefile` cases (h) to (l).
* **files**: `lib/platform/include/configforge/cf_platform_file.h`, `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/none/file.c`, `lib/config/tests/test_writefile.c`, `cmake/configforge.internal.symbols`; and, only when F-2a has landed without its amendment, `lib/config/src/bridge/file.c`, `lib/config/include/configforge/cf_source.h` and `lib/config/tests/test_overrides.c` (step 6); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-2 main` (after F-1a has landed; `grep -n cf_platform_write_file_at lib/platform/include/configforge/cf_platform_file.h` must find it, else stop). If F-2a has landed too, step 6 is done in this card; if not, step 6 is already part of F-2a's amended card and is skipped here.
  2. In `lib/config/tests/test_writefile.c`, after F-1a's case (g), add C89 cases, each in a file `cf_wi.txt` written by `fopen "wb"` first unless said: (h) `expect` equal: `cf_platform_write_file_if(f, "new", 3, "old", 3, &err)` is 1 and the file reads `new`; (i) `expect` different: the file holds `other`, the call with `expect` `old` returns -1, `err.code` is `CF_E_SOURCE`, `err.message` contains `changed while it was written`, the file still reads `other`, and `fopen(".cf_wi.txt.cf-tmp-0", "rb")` is NULL; (j) `expect` NULL, 0 with the file present returns -1, and with the file absent returns 1; (k) `expect` `""`, 0 against an empty file returns 1 (note: size 0 with a non-NULL pointer means "must exist and be empty") and against an absent file returns -1; (l) the race: `cf_platform_write_file_if_hook(f, "new", 3, "old", 3, between, f, &err)` where `between` rewrites `f` with `fopen "wb"` to `mine`: returns -1, the file reads `mine`, no temporary is left. Also (m) a failed step (the loop of F-1a's case (d) with `_if`) leaves the old file and no temporary. Run in the build directory.
  3. See it fail: `cmake --build --preset default --target test_writefile 2>&1 | tail -n 20` (undeclared `cf_platform_write_file_if`).
  4. Add the declarations, the check in the three core functions, the two entry points in each, and the symbol lines. `ctest --preset default -R "test_writefile|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: in `posix/file.c` make the comparison always equal (behind `if (fail_at == 0xFFFFu)`, a run-time-false condition, never a constant); case (i) and (l) fail; put it back. (On Windows run the same with `win32/file.c`.)
  6. F-2a's amendment (written into F-2a's own section of `docs/plan/F-files.md`: its "The optimistic check" bullet, step 2's cases (l) and (m), step 5): when F-2a has not landed, F-2a carries it and this step is already done by that card (say so in the report); when F-2a landed without it, this card carries it out as F-2a's section now reads: `overrides_write` keeps the bytes it read, calls `cf_platform_write_file_if`, applies again on -1 up to 3 times, and `test_overrides.c` gains F-2a's cases (l) and (m), which use `cfi_overrides_write`'s `between` hook.
  7. D-186 into `docs/Decisions.md`; the CHANGELOG entry.
  8. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-2: cf_platform_write_file_if, the replace refused when the file changed (D-186)`.
  9. ACCEPT; push `work/Y-2`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (cases (h) to (m) run on both); ACCEPT-CI; `ctest --preset default -R test_writefile` exit 0 in `default` and `minimal`.
* **stop if**: F-1a is not landed; `gate.symbols` or `gate.layering` refuses the names and the fix is not a line of `cmake/configforge.internal.symbols`; case (l) cannot be made without a thread or a second process (the hook must be enough: report which part is not); F-2a has landed and its `overrides_write` is not shaped as F-2a's "decided" says (report the difference, change nothing in it); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added" (the heading is added above the newest one when it is not there): "`overrides_write` applies its change again when the file changed while it was written, up to 3 times, then refuses: two programs that set different keys of one override file both keep their key (Y-2, D-186)". `docs/Decisions.md`: D-186. Roadmap: nothing until Y-5.

---

## Y-3 - `cf_platform_lock_take` and `_give`: a kernel lock on a kept sidecar file (F-5a in the research) (TO DECIDE)

* **id**: Y-3
* **goal**: `cf_platform_lock_take(l, path, wait_ms, err)` holds an exclusive lock on the file `<dir>/.<base>.cf-lock` beside `path` (`flock`, an open-file-description lock or a classic `fcntl` lock on POSIX, `LockFileEx` on Windows), waiting up to `wait_ms` milliseconds in a loop of non-blocking attempts, and `cf_platform_lock_give(l)` releases it; a test shows two takes of one path in one process exclude each other on every platform.
* **why**: the optimistic check of Y-2 leaves a window of microseconds, and a program that runs several copies of one service writing one file, or an installer and the program at once, may want it closed too. The Roadmap item asks for "an advisory lock on the file written (`flock`/`fcntl`, `LockFileEx`)" as an option the program asks for (`docs/Roadmap.md:1555-1561`); the research settles which call on which platform (`docs/research/concurrent-access.md:433-483`, "What P2 needs").
* **needs**: F-1a (the sidecar's name follows the temporary's), TO DECIDE 1
* **size**: medium (three platform files of the same shape, one test)
* **decided**:
  * **TO DECIDE 1 (reopen D-128's "no lock file").** D-128, as F-2a drafts it, rejects a lock file because "a crash leaves it behind" (`docs/plan/F-files.md`, F-2a's "What a write does to a file"). That is true of a lock by existence (git's `.lock`), not of a kernel lock on a kept file, which a crash releases. Options: **A** the sidecar lock, by the program's choice (`CF_FILE_LOCK`, Y-4), default off; **B** the same, default on, a flag to turn it off; **C** no lock, the optimistic check of Y-2 only; **D** D-128 as drafted (neither). **Recommended: A, with the optimistic check always.** It is what the maintainer's "it could depend on an option" describes, it leaves no file in a directory unless asked, and C alone already removes almost all lost updates. The steps below are A's and B's (the lock call is the same; B only changes Y-4's default); for C or D the maintainer says so under "Organiser's answers", this card and Y-4 are closed with no change, and Y-5 and Y-6 go on.
  * **Where.** The lock is in the full platform files, not in `file.c`: `lib/platform/src/posix/posix.c`, `win32/win32.c`, `none/none.c` (the serialization library does not take locks; the config library does, `lib/platform/CMakeLists.txt:35-36` names what the serialization library carries). Declared in `lib/platform/include/configforge/cf_platform.h` under `#if CF_INTERNAL_API`, after `cf_platform_stamp` (line 255-256), with its own comment:
    ```c
    typedef struct cf_platform_lock { long handle; } cf_platform_lock;
    /* 1 with the lock held on `path`.cf-lock; 0 with err set: CF_E_SOURCE
       "'%s' is busy: another writer holds its lock" after `wait_ms`
       (Y-4 makes it CF_E_BUSY), CF_E_SOURCE "cannot lock '%s': %s" when the
       lock file cannot be opened. */
    CF_API int  cf_platform_lock_take(cf_platform_lock* l, const char* path,
                                      unsigned long wait_ms, cf_error* err);
    /* Releases and closes; does nothing for a lock not held. The lock file is kept. */
    CF_API void cf_platform_lock_give(cf_platform_lock* l);
    ```
    `handle` is a file descriptor on POSIX, `(long)(INT_PTR)` of the `HANDLE` on Windows, -1 when not held; `cf_platform_lock_take` sets it, `give` sets it to -1.
  * **POSIX.** The sidecar is `<dir>/.<base>.cf-lock` (`<dir>` as F-1a's temporary: the path up to the last `/`, `.` when there is none). `lstat` first: a symbolic link is refused (`'%s' is a symbolic link`), a non-regular file is refused (`'%s' is not a regular file`), a missing one is fine; then `open(O_RDWR | O_CREAT | O_CLOEXEC where defined | O_NOFOLLOW where defined, mode)` with mode the configuration file's own `st_mode & 0666` when it exists and `0666` otherwise (the umask applies; whoever may write the file may lock it). The lock call, **one protocol per platform, chosen at build time, never two**: `flock(fd, LOCK_EX | LOCK_NB)` when `LOCK_EX` is defined (Linux, the BSDs, macOS, musl); else `fcntl(fd, F_OFD_SETLK, &fl)` when `F_OFD_SETLK` is defined; else `fcntl(fd, F_SETLK, &fl)` with `fl` the whole file, whose two faults (closing any descriptor of the file drops the process's locks; threads of one process do not exclude each other) are written into the function's header comment and into D-187. A refusal because it is held (`EWOULDBLOCK`, `EAGAIN`, and `EACCES` for `F_SETLK`) is "busy"; every other failure is "cannot lock". `EINTR` retries at once.
  * **Windows.** `CreateFileW` on the sidecar (the wide name through `win32_wide`, `win32.c:101`) with `GENERIC_READ | GENERIC_WRITE`, `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`, `OPEN_ALWAYS`; then `LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)` on one byte at offset 0; `ERROR_LOCK_VIOLATION` is "busy". No one reads the sidecar, so a mandatory lock on it costs nothing.
  * **none.** `cf_platform_lock_take` returns 0 with `CF_E_CAPABILITY`, `this build has no files to lock`; `give` does nothing.
  * **The wait.** Not an operating-system wait: a loop in the platform layer of non-blocking attempts with growing sleeps, as git does: attempt, then sleep 1, 2, 4, ... ms (each at most 100 ms and never past the bound), until `wait_ms` is spent; `wait_ms` 0 is one attempt. POSIX sleeps with `poll(NULL, 0, ms)` (`posix.c:938` is the file's use of `poll`), Windows with `Sleep` (`win32.c:443`'s use). The time spent is the sum of the sleeps (no clock), so the bound is a lower bound on the wall time and an upper bound on the sleeping; a program that wants an exact deadline calls again. No wait is unbounded: the type is `unsigned long` milliseconds and a value above 3600000 is taken as 3600000.
  * **The sidecar is kept and never removed** (removing it would bring back the rename-over-a-lock case: a second program opens the old file, the first removes it and a third creates a new one, and two hold "the" lock). It is never followed through a symbolic link.
  * **D-187** (copy into `docs/Decisions.md` after D-186, titled "A write may take a kernel lock on a kept file, when the program asks"): Question: should a write be able to exclude another writer at all, and with what. Options: the four of TO DECIDE 1. Decision (for A; B, C or D as the answer says): the call per platform, the sidecar's name, mode and kept status, the wait as a loop, one protocol per platform, "busy" distinct from "cannot lock". Rejected: a lock by existence (a stale lock after every crash, and D-127 refuses to clean one); locking the configuration file itself (a Windows reader would be refused and the rename would fail over a locked file); a lock on the read path or inside `cf_config` (D-14 holds, and with writers that never write in place a reader always opens a whole file); a writer service; an unbounded wait. D-128's "Rejected" list: in `docs/Decisions.md`, its words "a lock file" become "a lock file by existence (D-187 holds the kernel lock a program asks for)". Held to: `test_lock`.
* **files**: `lib/platform/include/configforge/cf_platform.h`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/win32.c`, `lib/platform/src/none/none.c`, `lib/config/tests/test_lock.c` (new), `lib/config/tests/CMakeLists.txt` (after `test_writefile`), `cmake/configforge.internal.symbols` (the two names), `lib/platform/README.md` (one line); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1. `git switch -c work/Y-3 main` (after F-1a has landed, and Y-2 when it is open: they touch different files).
  2. Write `lib/config/tests/test_lock.c`, C89, `#include <configforge/cf_platform.h>`, `"cf_test.h"`, in the build directory, file name `cf_lk.txt`. Cases: (a) `take(&a, "cf_lk.txt", 0, &err)` is 1 and `.cf_lk.txt.cf-lock` exists; (b) a second `take(&b, "cf_lk.txt", 0, &err2)` in the same process returns 0, `err2.message` contains `is busy`; (c) after `give(&a)` the second `take` succeeds, and `give(&b)`; (d) the wait: with `a` held, `take(&b, ..., 1500, &err)` returns 0 and `time(NULL)` has moved by at least 1 between before and after (C89's `time()` is the only wall clock the platform layer offers a test; `clock()` is CPU time and cannot see a sleep; a 1.5 s wait always crosses a second); (e) POSIX only (`#if CF_PLATFORM_POSIX`): `remove(".cf_lk.txt.cf-lock")`, then `system("ln -s cf_other .cf_lk.txt.cf-lock")` (skip the case when it returns non-zero) and `take` is refused with `is a symbolic link`, `cf_other` not created; (f) `give` of a lock never taken, twice, is harmless; (g) the sidecar is still there after `give` and the file `cf_lk.txt` is untouched (not created). On a build with no files (`CF_PLATFORM_NONE`) the test checks only `CF_E_CAPABILITY`. Add `test_lock` to the list.
  3. See it fail: `cmake --build --preset default --target test_lock 2>&1 | tail -n 20` (undeclared `cf_platform_lock_take`).
  4. Add the declarations, the three implementations, the symbol lines and the README line. `ctest --preset default -R "test_lock|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: in `posix.c` (or `win32.c` on Windows) make `take` return 1 without the lock call, behind a run-time-false condition (`if (wait_ms != 0xFFFFFFFFul)` selects the broken path, never a constant: the negative-proof rule for MSVC); case (b) fails; put it back.
  6. D-187 and D-128's edit into `docs/Decisions.md`; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-3: cf_platform_lock_take and _give, a kernel lock on a kept sidecar (D-187)`.
  8. ACCEPT; push `work/Y-3`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI (macOS and FreeBSD jobs included: `flock` is exercised there, and the BSD runner is where an `F_OFD_SETLK`-less path would show); `ctest --preset default -R test_lock` exit 0 in `default` and `minimal`.
* **stop if**: no answer to TO DECIDE 1; a CI target defines neither `LOCK_EX`, `F_OFD_SETLK` nor `F_SETLK` (report which); case (b) passes on a target where the protocol is classic `F_SETLK` (threads of one process share classic locks: the test is then `#if`-excluded there and the report says on which target); `gate.layering` refuses `posix.c`'s new includes (`<sys/file.h>`, `<fcntl.h>`).
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_platform_lock_take` and `_give` (internal tier): a kernel lock on a kept sidecar file, waiting up to a bound (Y-3, D-187)". `docs/Decisions.md`: D-187 and the edit of D-128. Roadmap: nothing until Y-5.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve", after the comparison of docs/research/concurrent-access.md section 2), written as **D-227**, answering Y-3's TO DECIDE 1 and Y-4's TO DECIDE 2 and 3:
  1. The lock is a part that plugs in: `cf_locker` (its name; `take(path, wait_ms, err)`; `give`), as `cf_packer` and `cf_unpacker` are. Built in: `cf_lock_none` (the default) and `cf_lock_sidecar` (Y-3's kernel lock on `<dir>/.<base>.cf-lock`: `flock`, OFD or `fcntl` on POSIX, `LockFileEx` on Windows; released by a crash). A program may plug in its own (a service it asks, a database lock, an embedded mutex).
  2. Y-2's optimistic check is always on; its retry count is an option, 3 by default.
  3. The wait is an option of each source in `cf_file_source_options` (`wait_ms`, 0 by default, as SQLite's), with a default for the whole configuration by `cf_builder_write_wait(b, ms)`; a `cf_locker` may carry its own suggested wait (as git's locks do). The SQLite store uses the same wait (`sqlite3_busy_timeout`).
  4. A lock that is busy, or retries spent, return `CF_E_BUSY` with its place and reason (D-223).
  5. The lock is chosen by `cf_file_source_options.lock` (`cf_lock_none` by default). D-128's "no lock file" holds for the default; the sidecar exists only when it is chosen.
  Y-3 and Y-4 are rewritten to D-227 before they open.

## Y-4 - `CF_FILE_LOCK`, `cf_builder_write_wait`, `CF_E_BUSY`: the lock around `overrides_write`, and the store's wait (F-5b in the research) (TO DECIDE)

* **id**: Y-4
* **goal**: a program that adds an override file with `CF_FILE_LOCK` has every `cf_write` to it taken under Y-3's lock, waiting as long as `cf_builder_write_wait` says before it is refused with the new code `CF_E_BUSY`; the SQLite store waits the same time (`sqlite3_busy_timeout`) before it is refused with `CF_E_BUSY`; and Y-2's exhausted retries are `CF_E_BUSY` too.
* **why**: the Roadmap asks, as an option the program asks for, for "what the library does when the lock is held (wait with a bound, or refuse)" (`docs/Roadmap.md:1559-1561`). The store has locks of its own and no wait: no `sqlite3_busy_timeout` and no `PRAGMA busy_timeout` is set anywhere in `lib/`, so a second writer that finds the store locked fails at once with `SQLITE_BUSY` (`lib/config/src/bridge/store.c:231`, `BEGIN IMMEDIATE`). The program's reaction to "another writer holds it" (try later, tell the user that another program is saving) differs from a file that cannot be written, so it gets a code of its own (`docs/research/concurrent-access.md:469-476`).
* **needs**: Y-3, F-2a, TO DECIDE 2 and 3
* **size**: medium (two public names, a code, three call sites, one test)
* **decided**:
  * **TO DECIDE 2 (the wait: where it is set and its default).** Options: **A** `cf_builder_write_wait(b, ms)` for the whole configuration, default 0; **B** the same, default a short bound (git uses 100 ms and 1000 ms); **C** a wait argument on `cf_write` (a breaking change of a public signature, so 2.0.0). **Recommended: A.** A program chooses its own patience; 0 keeps today's store behaviour (refused at once) as the default, so nothing changes for a program that asks nothing. The Windows retry of F-1a is not this wait: it is part of making one write work at all, and has its own fixed bound (F-1a, amended). The steps are A's.
  * **TO DECIDE 3 (how "busy" is reported).** Options: **A** a new code `CF_E_BUSY`; **B** `CF_E_SOURCE` with a message only; **C** `CF_E_SOURCE` with a new reason (but reasons are document refusals, D-107). **Recommended: A**: the program acts on it differently, and a message is not something to test against. It adds a value to a public enum, inside 1.x as an addition. The steps are A's; for B or C the maintainer says so under "Organiser's answers", `CF_E_BUSY` is not added, every `CF_E_BUSY` below is `CF_E_SOURCE` with the same message, and the card goes on.
  * **The public interface** (`lib/config/include/configforge/configforge.h`; `CF_FILE_LOCK` after `cf_builder_overrides`'s declaration that F-2a adds, `cf_builder_write_wait` after `cf_builder_writable`, line 272):
    ```c
    /* cf_builder_overrides `flags`: writers of this file take its lock, a file
       named .<name>.cf-lock beside it, kept, never removed. */
    #define CF_FILE_LOCK 0x0100u

    /* How long a write waits for another writer of the same file or store, in
       milliseconds, before it is refused with CF_E_BUSY. 0, the default: refused
       at once. No value waits forever (above one hour it is one hour). Returns
       CF_FALSE with the builder's error set on a NULL builder. */
    CF_API cf_bool cf_builder_write_wait(cf_builder* b, unsigned long wait_ms);
    ```
    `CF_FILE_LOCK` must not collide with the bits `cf_builder_file`'s flags already use: `grep -n "0x0[0-9a-f]*u" lib/config/include/configforge/configforge.h | head -20` (`CF_ENCODING_MASK` is among them, `file.c:482`); the card takes the next free bit of the `0x0100u` family, and says in the commit which. The flag is meaningful to `cf_builder_overrides` only: `cf_builder_file` with it set is refused, `CF_FILE_LOCK is for an overrides file`.
  * **The code.** `lib/base/include/configforge/cf_code.def`: a new last entry, so that no existing value moves: `CF_E_MEMORY` becomes a `CF_CODE_X` line and the new line is the `CF_CODE_LAST_X`: `CF_CODE_LAST_X(CF_E_BUSY, "busy", "another writer holds it; try again")`. Its value is 10 (`CF_E_MEMORY` stays 9: `bindings/rust/src/sys.rs:49` has `CF_E_POLICY` as 8). The language bindings read the name through `cf_code_name`; add `CF_E_BUSY` to Rust's `sys.rs` only if the file lists every code (it lists three: no change). `docs/API.md`'s code table is generated.
  * **How the wait travels.** A source gets a field: `unsigned long write_wait_ms;` appended to `struct cf_source` (`lib/config/include/configforge/cf_source.h:90-110`, after `u`); `memset(s, 0, ...)` in the builder (`config.c:1595`) already zeroes it. The builder keeps `write_wait_ms` (next to `nwritable`, `config.c:79-80`), `cf_builder_write_wait` sets it, `cf_config_create` copies it into every `c->sources[k]` (the loop at `config.c:1304-1335`), and `derive_onto` copies the base's (next to the grant loop, `config.c:1660-1671`, which needs the base's value: add `unsigned long write_wait_ms;` to `struct cf_config` beside `writable`, `config.c:137`, set at create). `overrides_write` and `store_write` read `s->write_wait_ms`.
  * **The lock around `overrides_write`** (F-2a's steps): when the source's flags carry `CF_FILE_LOCK` (kept in `file_state`, `file.c:471-477`'s state), `cf_platform_lock_take(&l, path, s->write_wait_ms, err)` before step (1), `cf_platform_lock_give(&l)` after step (6) and on every failure after the take. A take that fails because the lock is held is `CF_E_BUSY`, message `'%s' is busy: another writer holds its lock; try again`, `source` the path; a take that cannot open the lock file stays `CF_E_SOURCE`. The stand-in `derive_ghost` makes for a derived configuration (`config.c:1580-1608`) carries `vt->write` and the path only, and has no `file_state`, so the flag cannot live there: `struct cf_source` gets a second appended field, `cf_bool write_lock;`, set when `cf_builder_overrides` adds the source (from `CF_FILE_LOCK`) and copied by `derive_ghost` (`s->write_lock = o->write_lock;` after `s->rank`, line 1599). `overrides_write` reads `s->write_lock` and `s->write_wait_ms`, never `file_state`.
  * **The store's wait.** `cf_store_set` keeps its public signature (`cf_source.h:489`, called by `test_store.c`). A new `cf_store_set_wait(path, wait_ms, items, count, meta, err)` next to it, internal (`#if CF_INTERNAL_API`, symbol line in `cmake/configforge.internal.symbols`), is the body; `cf_store_set` calls it with 0; `store_write` (`store.c:332`) calls it with `s->write_wait_ms`. Inside it, after `sqlite3_open_v2` succeeds and before `store_schema` is executed (`store.c:228-231`), `sqlite3_busy_timeout(db, (int)min(wait_ms, 2147483647))`. When `store_failed` finds `sqlite3_errcode(db) == SQLITE_BUSY` (or `SQLITE_LOCKED`), the error is `CF_E_BUSY`, message `writing the store '%s': another writer holds it; try again`; any other failure is as it was (`store.c:187-190`).
  * **Y-2's retries.** `overrides_write`'s third refusal (`'%s' changed while it was written; try again`) is now `CF_E_BUSY` (same message). Y-2's tests and F-2a's case (l) that look at `err.code` change from `CF_E_SOURCE` to `CF_E_BUSY`.
  * **D-188** (after D-187, "A write waits as long as the program says, and is refused as busy"): Question: what the library does when another writer holds the file or the store. Decision: the interface above; default 0 (refused at once); the wait is a bounded loop in the platform layer and SQLite's own busy handler for the store; one code, `CF_E_BUSY`, for a held lock, a busy store and the exhausted retries; the code is last so that no value moves. Rejected: an unbounded wait (a program that wants to wait longer calls again); a wait argument on `cf_write` (breaks the signature before 2.0.0); a default wait (a program that asked nothing sees nothing new); `CF_E_SOURCE` with a reason (reasons are document refusals, D-107). Held to: `test_busy`.
* **files**: `lib/base/include/configforge/cf_code.def`, `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_source.h`, `lib/config/src/core/config.c`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/store.c`, `lib/config/tests/test_busy.c` (new), `lib/config/tests/CMakeLists.txt`, `cmake/configforge.symbols` (the public name), `cmake/configforge.internal.symbols` (`cf_store_set_wait`), the tests of Y-2 and F-2a that look at the code; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (Y-1's section gains the option), `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 2 and 3, or Y-3's TO DECIDE 1 was answered C or D (this card is then closed with no change). `git switch -c work/Y-4 main` (after Y-3, F-2a and Y-2 have landed).
  2. Write `lib/config/tests/test_busy.c`, C89, after `test_overrides.c`'s way of building a configuration. Cases: (a) a file `cf_bz.json` added with `CF_FILE_LOCK`, the lock taken by the test's own `cf_platform_lock_take` on `cf_bz.json`, `cf_builder_write_wait(b, 0)`: `cf_write` returns `CF_FALSE`, `err.code` is `CF_E_BUSY`, the message contains `is busy`, and the file is as it was; (b) the same with the lock given first: the write succeeds; (c) `cf_builder_write_wait(b, 1500)` with the lock held: refused, and `time(NULL)` has moved by at least 1 (as Y-3's case (d)); (d) without `CF_FILE_LOCK` the write succeeds with the lock held (the lock is the program's choice); (e) `cf_builder_file` with `CF_FILE_LOCK` fails `cf_builder_create` with `is for an overrides file`; (f) after the write, the sidecar `.cf_bz.json.cf-lock` exists and the file reads as written; (g) `cf_derive`d configuration writes with the same wait and lock; (h) `#if CF_WITH_SQLITE`: open a second connection to `cf_bz.db`, `BEGIN EXCLUSIVE` on it, `cf_write` to the store with wait 0 returns `CF_E_BUSY` at once and with wait 1500 returns it with `time(NULL)` moved by at least 1, and after `COMMIT` of the second connection the write succeeds; (i) `cf_code_name(CF_E_BUSY)` is `busy`; (j) `cf_builder_write_wait(NULL, 5)` is `CF_FALSE`. Add `test_busy` to the list.
  3. See it fail: `cmake --build --preset default --target test_busy 2>&1 | tail -n 20` (undeclared `CF_FILE_LOCK`).
  4. Add the code, the two fields, the builder call, the flag, `cf_store_set_wait`, and the call sites. `ctest --preset default -R "test_busy|test_overrides|test_store|test_write|test_writefile|test_lock|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: make `store_write` pass 0 instead of `s->write_wait_ms` (behind `if (count != 0xFFFFu)` a run-time-false test, not a constant); case (h)'s wait half fails; put it back. Then make `overrides_write` skip the take (same kind of condition); case (a) fails; put it back.
  6. D-188 into `docs/Decisions.md`; Y-1's section in `docs/Guide.md` gains a paragraph "When the program asks": `CF_FILE_LOCK` and `cf_builder_write_wait`, what `CF_E_BUSY` means, that the lock is advisory (a writer that does not take it is not stopped) and that its file is kept; the CHANGELOG entries.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-4: CF_FILE_LOCK, cf_builder_write_wait and CF_E_BUSY (D-188)`.
  8. ACCEPT; push `work/Y-4`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_busy|test_overrides|test_store|test_write"` exit 0, and `ctest --preset minimal -R test_busy` exit 0 (it builds, and says what is absent); the language bindings' own tests still pass in CI (`cf_code_name` carries the new code).
* **stop if**: no answer to TO DECIDE 2 and 3; `gate.interface` or `gate.abi` refuses the two appended `cf_source` fields (the struct is read by sources outside the tree: report, change nothing); `sqlite3_busy_timeout` is not in the bundled SQLite (`grep -rn busy_timeout` the vendored header; report); the grant loop of `derive_onto` cannot read the base's wait (the field is not on `cf_config`'s path: report).
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`CF_FILE_LOCK` for an override file, `cf_builder_write_wait`, and the new code `CF_E_BUSY`: a write waits for another writer as long as the program says, then is refused as busy; the store waits the same time (Y-4, D-188)"; "### Changed": "an override file's refused retries are `CF_E_BUSY`, not `CF_E_SOURCE` (Y-4)". `docs/Decisions.md`: D-188. Roadmap: nothing until Y-5.

---
* **Organiser's answers** (2026-11-26): TO DECIDE 2 and 3 are answered by D-227, written under Y-3's "Organiser's answers"; this card is rewritten to it before it opens.

## Y-5 - A stable read: a file read twice and compared, up to 3 reads (F-5c in the research)

* **id**: Y-5
* **goal**: the file source reads a file only after taking its stamp, and trusts the bytes only when the stamp taken again after the read is the same; when it moved, it reads again, up to 3 reads in all, and then fails as an unreadable file does (a reload then keeps the running snapshot); and the Roadmap item "Reads and writes at the same time, guarded by choice" is marked done.
* **why**: a reader of a file that something else (an editor, a deployment tool) is writing in place can read half of it, and no lock can stop that, because the writer does not take one (`docs/research/concurrent-access.md:490-507`, option R2). Taking the stamp before and after costs one `stat` more per file read, adds no public name, and catches most in-place writers. `cf_blob_file` reads with `fread` once and nothing checks that the file did not change while it was read (`lib/serialize/src/blob.c:59-110`).
* **needs**: nothing
* **size**: small
* **decided**:
  * **Where.** Not in `cf_blob_file` (the serialization library has no stamp: `cf_platform_stamp` is in `posix.c`, `win32.c` and `none.c`, outside what the serialization library carries). In the file source, `lib/config/src/bridge/file.c`, at the one call that reads a file whole: `file_load`'s `cf_blob_file(&blob, bytes, st->path, &st->err)` (line 416). A static helper is not testable alone: the card adds an internal function, declared under `#if CF_INTERNAL_API` in `lib/config/include/configforge/cf_source.h` beside `cfi_source_text` (line 277), with its symbol line in `cmake/configforge.internal.symbols`:
    ```c
    /* cf_blob_file, but the file is stamped before and after the read and read
       again when the stamp moved (mtime, size or presence), up to 3 reads in all;
       CF_E_SOURCE "'%s' changed while it was read" when all 3 saw it move.
       `between(user)`, when not NULL, is called after each stamp and before the
       read: for the test, which changes the file there. A file that cannot be
       opened fails as cf_blob_file's does, with no retry (an absent file is not a
       file being written). */
    CF_API cf_bool cfi_file_read_stable(cf_blob* b, cf_arena* ar, const char* path,
                                        void (*between)(void* user), void* user,
                                        cf_error* err);
    ```
    `file_load` calls it with `NULL, NULL`.
  * **The comparison.** `cf_stamp` (`cf_platform.h:245-249`): equal when `mtime`, `size` and `present` are all equal. A stamp that `cf_platform_stamp` cannot take (returns `CF_FALSE`) makes the read plain: one read, no comparison (a platform with no stamp is the `none` build, which has no files). Each re-read allocates its bytes from the same arena `ar`; the abandoned reads are zeroed when the arena is (a file may hold a secret, `blob.c:50-55`'s `cf_blob_arena_done`). The 3 is a constant, not an option.
  * **A hint, said so.** The stamp is a hint (`docs/Decisions.md:1689-1695`): a rewrite of the same length inside one clock tick passes it. The page says "catches most in-place writers", not all.
  * **D-189** (after D-188, "A file is read twice and compared"): Question: a reader of a file being written in place. Options: writers that never write in place (F-1a, already the library's); a stable read; a shared lock on every read (rejected: it buys nothing, since library writers never write in place and other writers take no lock, and it would make a reader wait for a writer, which D-14 refuses on the read path); readers that share delete access (Y-6). Decision: the stable read, 3 reads, then the source fails as an unreadable file does; which `cf_live` already survives. Held to: `test_stable`.
  * **The Roadmap item.** After Y-2, Y-3, Y-4 and this card have landed (or Y-3 and Y-4 are closed by TO DECIDE 1's answer C or D), the item "Reads and writes at the same time, guarded by choice." in `docs/Roadmap.md` (heading at line 1555 of the tree at cbff5a9; find it by its words) is marked 🟩 by this card, with one line added under it: what was built and where (Y-2, Y-3, Y-4, Y-5, Y-6, D-186 to D-189). When an earlier card of this topic is still open, this card marks nothing and says so in its report.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/file.c`, `lib/config/tests/test_stable.c` (new), `lib/config/tests/CMakeLists.txt`, `cmake/configforge.internal.symbols`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`, `docs/Guide.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-5 main`.
  2. Write `lib/config/tests/test_stable.c`, C89, `"cf_test.h"`, in the build directory, file `cf_st.json`. Cases: (a) a stable file: `cfi_file_read_stable(&b, ar, "cf_st.json", NULL, NULL, &err)` returns `CF_TRUE`, bytes equal the file; (b) one change: `between` rewrites the file with a different length the first time it is called: the call returns `CF_TRUE` and the bytes are the new ones (2 reads); (c) the file moves at every call of `between` (a counter makes each rewrite a different length): the call returns `CF_FALSE`, `err.code` is `CF_E_SOURCE`, the message contains `changed while it was read`, and `between` was called 3 times; (d) a missing file fails as `cf_blob_file` does (`cannot open`), `between` called 0 or 1 times and no retry; (e) a configuration built with `cf_builder_file` on a file that is stable reads it as before (one build). Use arenas from `cf_arena_create` (as `test_file.c` does; `grep -n cf_arena_create lib/config/tests/test_file.c | head -3`). Add `test_stable` to the list.
  3. See it fail: `cmake --build --preset default --target test_stable 2>&1 | tail -n 20` (undeclared `cfi_file_read_stable`).
  4. Add the declaration, the function, the symbol line, and the call in `file_load`. `ctest --preset default -R "test_stable|test_file|test_watch|test_live|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: make the loop stop after one read (`tries = (fail_at_zero) ? 3 : 1` with a run-time-false condition, never a constant); case (b) fails; put it back.
  6. D-189 into `docs/Decisions.md`; the Guide's section "At the same time" (Y-1), third paragraph, gains the sentence "The library's own reader takes the file's stamp before and after reading and reads again, up to three times, when it moved"; the CHANGELOG entry; the Roadmap row as decided.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-5: a stable read, a file read twice and compared (D-189)`.
  8. ACCEPT; push `work/Y-5`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_stable|test_file|test_live|test_watch"` exit 0; `bench` unchanged: the quick benchmark's file-reading cells (`scripts/bench/run.py --scope quick`) are not gated on it, and the one extra `stat` is not measured here.
* **stop if**: `cf_platform_stamp` of a file written within one clock tick cannot tell two lengths apart (it compares `size`, so it can; if case (b) is flaky, report the platform); `gate.symbols` refuses the name; an existing test of `file_load`'s error text changes.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Changed": "the file source reads a file only when its stamp is the same after the read as before, and reads again up to 3 times when it moved (Y-5, D-189)". `docs/Decisions.md`: D-189. `docs/Roadmap.md`: "**Reads and writes at the same time, guarded by choice.**" marked 🟩, when the cards above have landed.

---

## Y-6 - Windows readers that share delete access, if a measurement shows a rename then succeeds over an open reader (F-5d in the research)

* **id**: Y-6
* **goal**: on Windows, `cf_platform_fopen` opens a file with `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE` through `CreateFileW` and a C stream on the handle, if and only if a measurement in this card shows that `MoveFileExW` then replaces a file that a reader still has open; otherwise nothing changes and the card is closed with the measurement written down.
* **why**: `cf_platform_fopen` opens with `_wfopen` (`lib/platform/src/win32/file.c:36-44`), which does not share delete access: while a reader has the file open, F-1a's rename over it fails. F-1a's Windows retry (amended) waits for the reader to close, but a reader that is slow or stuck blocks the write for the whole bound. Opening with `FILE_SHARE_DELETE` is what lets a rename through on POSIX-like terms; whether `MoveFileExW(MOVEFILE_REPLACE_EXISTING)` over a file opened that way succeeds is what the research could not settle from documents (`docs/research/concurrent-access.md:168-188`, and option R5, "to be measured").
* **needs**: F-1a
* **size**: small
* **decided**:
  * **Not inheritable, always.** Y-8 opens files with `_wfopen`'s `N` mode flag; if this card replaces that call it keeps the property: `CreateFileW` with NULL security attributes is not inheritable, and `_open_osfhandle` is given `_O_RDONLY | _O_NOINHERIT`.
  * **A measurement first, and it decides.** The card runs the measurement from a throwaway C file in the scratch directory (not added to the tree), compiled with `cl` in the Visual Studio developer prompt, on Windows: `CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)`, keeping the handle open; then, from the same process, `MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`; the result and `GetLastError()` are written under the card's "notes" together with the Windows build (`ver`). Also the same with `_wfopen(path, L"rb")` as the control (it must fail, `ERROR_ACCESS_DENIED` or `ERROR_SHARING_VIOLATION`; if it succeeds the control is wrong: stop).
  * **If the rename succeeds** with the share-delete handle open: `cf_platform_fopen` on Windows becomes `CreateFileW` with the three share flags and `OPEN_EXISTING` (mode `r`) or `CREATE_ALWAYS` (mode `w`), `_open_osfhandle`, `_wfdopen`; the mode strings the library uses are `rb`, `wb` and `r` (`grep -rn "cf_platform_fopen" lib tools --include=*.c | sed 's/.*cf_platform_fopen//' | sort -u | head` lists them: any other string is a "stop if"). A reader that opens a file this way no longer blocks a writer's rename. The retry of F-1a stays (a reader that opened the file by other means still blocks it).
  * **If it fails**: no code. The card is closed, the measurement is under "notes", the research's R5 is written as rejected in a line of `docs/Decisions.md` under D-189 ("Windows readers that share delete access: measured, a rename over one still fails, 2026-xx-xx, build ..."), and the Roadmap is not touched.
  * **No new public name; no D-number of its own** (D-189 holds it).
* **files**: `lib/platform/src/win32/file.c` (only when the measurement says so), `lib/config/tests/test_writefile.c` (a Windows-only case (n)), `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/Y-6 main` (after F-1a has landed). On a machine that is not Windows, stop: this card is Windows-only and is carried by the organiser's Windows machine.
  2. Measure as decided and write the numbers under "notes" in this card's section of `docs/plan/Y-asked.md`.
  3. If the rename fails: write the line of `docs/Decisions.md`, commit it as `Y-6: Windows readers that share delete access, measured and rejected`, push to `main` (documents), and stop; the card is done.
  4. If the rename succeeds, write the test first: `test_writefile.c` case (n), `#if CF_PLATFORM_WIN32`: open the target with `cf_platform_fopen(path, "rb")`, keep it open, call `cf_platform_write_file(path, "new", 3, &err)`, expect 1 at once (not after the retry bound: measure the call and fail over 500 ms), close the reader, expect the file to read `new`.
  5. See it fail: `ctest --preset default -R test_writefile` on Windows fails case (n) (the write waits out the bound and fails, or takes more than 500 ms).
  6. Change `cf_platform_fopen` as decided; `ctest --preset default -R "test_writefile|test_file|test_serialize"` passes.
  7. See that the test can fail: change the share flags back to `FILE_SHARE_READ` behind a run-time-false condition (`if (mode[0] == 'z')`); case (n) fails; put it back.
  8. `python scripts/work/work.py format`; codemap last. Commit `Y-6: cf_platform_fopen shares delete access on Windows`.
  9. ACCEPT; push `work/Y-6`; ACCEPT-CI.
* **accept**: ACCEPT-WINDOWS with `test_writefile` case (n) when the measurement says yes, and the measurement under "notes" in both cases; ACCEPT-LINUX and ACCEPT-CI when code changed (the change is Windows-only, and both must still be green); `ctest --preset default -R "test_writefile|test_file"` exit 0.
* **stop if**: the control (`_wfopen`) does not fail; a mode string other than `rb`, `wb` or `r` is passed to `cf_platform_fopen` anywhere; a path that `win32f_wide` accepts is not accepted by `CreateFileW` (a long path with no `\\?\` prefix: report); the measurement is not available because the machine is not Windows.
* **docs**: when the code changed: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Changed": "on Windows a file the library reads is opened with delete sharing, so a writer's rename over it no longer waits for the reader (Y-6)". When it did not: only the line in `docs/Decisions.md`. No Roadmap row.

---

## Y-7 - "Threads and processes" in the Guide: four recipes, compiled in CI, a `fork` test

* **id**: Y-7
* **goal**: `docs/Guide.md` has a section "Threads and processes made easier" with four recipes that work on today's interface, each an example under `examples/c/` that CI builds and runs, and the `fork` one has a test that a child reads a snapshot taken before it.
* **why**: threads are served by D-14 and `cf_live`, but nothing shows a program how to share a configuration with request handlers, reload on `SIGHUP`, `fork` after building, or hand an image to a child (`docs/research/threads-and-processes.md:539-556`, "First", point 1); the rules for `fork` and signals are written nowhere (its section 3, items 7-9 and 13). The Roadmap item is "Threads and processes made easier" (`docs/Roadmap.md`, by its words).
* **needs**: Y-1 (its "At the same time" is the section this one follows), Y-8 (recipe 4 hands a file to an `exec`ed child that must not leak descriptors)
* **size**: medium (four small examples, one test, one Guide section)
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
* **goal**: every descriptor or handle the library keeps open while it runs (a file opened by `cf_platform_fopen`, the watcher's `inotify` or `kqueue` descriptor and the files it watches, a private file being created) is close-on-exec on POSIX and not inheritable on Windows, and a test `exec`s a child that lists its open descriptors and finds none of them.
* **why**: a child that another thread starts while the library has a file open inherits the descriptor: `cf_platform_fopen` calls `fopen` with the caller's mode and no `e` (`lib/platform/src/posix/file.c:19`) and `_wfopen` with no `N` (`win32/file.c:41`), and `cf_platform_create_private` opens with no `O_CLOEXEC` (`lib/platform/src/posix/posix.c:487`); the `kqueue()` descriptor is made without it too (`posix.c:1181`) (`docs/research/threads-and-processes.md:512-523`, item 11 of its section 3). A secret file read by a library should not reach a stranger's child.
* **needs**: nothing
* **size**: small
* **decided**:
  * **POSIX `cf_platform_fopen`** (`posix/file.c`, C89 plus `open`, `fdopen`, `fcntl`): `fopen(path, mode)` becomes `open` then `fdopen`. The mode string maps as C says: first letter `r` is `O_RDONLY`, `w` is `O_WRONLY | O_CREAT | O_TRUNC`, `a` is `O_WRONLY | O_CREAT | O_APPEND`; a `+` makes it `O_RDWR`; `b` is ignored; any other first letter returns NULL. The permission is `0666` (the umask applies, as `fopen` gave). Flags carry `O_CLOEXEC` where it is defined; where it is not, `fcntl(fd, F_SETFD, FD_CLOEXEC)` right after the `open` (a window of two calls remains and the file's comment says so). `fdopen` failing closes `fd` and returns NULL. Only `"rb"` is passed anywhere today (`grep -rn "cf_platform_fopen" lib tools --include=*.c`); a caller found with another mode string is handled by the mapping above and is not a stop.
  * **`none/file.c`** is plain `fopen` and stays (no descriptors the library can mark).
  * **Windows `cf_platform_fopen`**: the mode string gets `N` appended (`_wfopen(wpath, wmode)` with `wmode` one character longer: the `wchar_t wmode[8]` already has room; refuse a mode longer than 6 characters); the C runtime then opens the file `_O_NOINHERIT`. Y-6 may later replace this call; its card keeps the property.
  * **`cf_platform_create_private`** (POSIX, `posix.c:487`): `open(path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600)`, with `O_CLOEXEC` under `#ifdef O_CLOEXEC` as `posix.c:1020-1022` does, and `fcntl(fd, F_SETFD, FD_CLOEXEC)` otherwise. The Windows twin already passes `bInheritHandle = FALSE` (`win32.c:211`): no change.
  * **The `kqueue` descriptor** (`posix.c:1181`): after `kqueue()` succeeds, `fcntl(w->fd, F_SETFD, FD_CLOEXEC)`; a failure closes it and fails the watcher as `kqueue` failing does. (`kqueue` has no flags argument on macOS; FreeBSD's `kqueue1` is not used so that one path serves both.) The files it watches are already `O_CLOEXEC` (`posix.c:1020-1022`) and the `inotify` descriptor is `IN_CLOEXEC` (`posix.c:839`): no change. The Windows watcher's directory handle and event are made with NULL security attributes (`win32.c:420-424`, `520`), which is not inheritable: no change.
  * **Directory scans** (`opendir`, `posix.c:118`, `593`, `1081`): not changed. The C library's `opendir` marks its descriptor close-on-exec on glibc, musl and the BSDs, and the descriptor lives inside one scan that closes it before returning; the page says so. The test cannot observe it (a scan is too short to list from a child) and D-190 says "held by the C library".
  * **The test.** `lib/config/tests/test_cloexec.c`, `"cf_test.h"`, C89, run in the build directory, one binary that is its own child: with `argc >= 3 && strcmp(argv[1], "--fds") == 0` it lists, into the file `argv[2]`, the numbers of the descriptors 3 to 255 that are open (`fcntl(fd, F_GETFD) != -1` on POSIX; on Windows `_get_osfhandle(fd) != -1` for 3 to 255 after `_set_invalid_parameter_handler` with an empty handler and `_CrtSetReportMode(_CRT_ASSERT, 0)`), one per line, then returns 0. The parent: opens `cf_cx.txt` with `cf_platform_fopen(..., "rb")` and keeps it open; builds a `cf_config` with a file source and opens `cf_watcher_open` on it (`configforge.h:827-865`; skip the watcher case with a printed line when `cf_watcher_open` returns NULL, `CF_E_CAPABILITY`); records the numbers of those descriptors (`fileno` of the first; `cf_watcher_fd` of the second); runs the child (POSIX: `system("\"<argv[0]>\" --fds cf_fds.txt")`; Windows: `_spawnl(_P_WAIT, argv[0], argv[0], "--fds", "cf_fds.txt", NULL)`, which passes C-runtime descriptors that are not `N`); reads `cf_fds.txt` and checks that none of the recorded numbers is in it. A control: a descriptor the test itself opens with plain `fopen` (POSIX; Windows `_wfopen` with no `N`) and `fileno`s IS in the list (so the child's list works; when the control is not found the test fails with `the child's list is empty: the control descriptor was not seen`). `cf_platform_create_private` is exercised by a call with a new file name and checked by reading the source (`grep -n "O_CLOEXEC" lib/platform/src/posix/posix.c` shows the line): the descriptor is closed before the function returns, so no child can see it.
  * **D-190** (copy into `docs/Decisions.md` after D-189, titled "Every descriptor the library keeps is close-on-exec"): Question: a child started by another thread while the library has a file open inherits it. Decision: the four changes above; the directory scan held by the C library; the test with a control. Rejected: `fopen` with `e` (not in the macOS C library until recently, and a mode string the C standard does not know); `O_CLOEXEC` alone where `fcntl` would do (a platform without the flag gets the two-call form and a comment, not a refusal). Held to: `test_cloexec`.
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

## Y-10 - `cf_live_get` and `cf_config_free` under threads on the benchmark's page; `cf_live_generation` without the lock (TO DECIDE 1b)

* **id**: Y-10
* **goal**: `cf_bench` times the `cf_live_get` plus `cf_config_free` pair on 1, 2, 4 and so on up to twice the machine's cores, and `docs/Performance.md` shows the numbers in a part "Readers under threads"; and, when TO DECIDE 1b is answered yes, `cf_live_generation` reads an atomic instead of taking the lock.
* **why**: the read path takes three shared writes on `get` and one on free, under a ticket lock with no pause and no yield (`lib/config/src/core/live.c:86-93`), and nobody has measured it under threads: the benchmark times a reload (`bench/bench.c:287-317`), not `cf_live_get` under contention (`docs/research/threads-and-processes.md:313-337`, items 2 and 3). A worker that asks "has it changed since I looked?" through `cf_live_generation` pays the same lock as a `get` (`live.c:910-920`). The numbers decide whether the reader handle of Y-11 is built.
* **needs**: nothing; TO DECIDE 1b for the second half
* **size**: medium (a benchmark with threads, a page part, one small change in `live.c`)
* **decided**:
  * **TO DECIDE 1 (a new public interface for the read path).** (a) None: the current `cf_live_get`, with the measurement on the benchmark's page. (b) A5 only: `cf_live_generation` lock-free, no new name. (c) `cf_live_reader` as sketched (Y-11), A5 included. **Recommendation: (b) now, as part of this measurement card; (c) when the measurement shows a knee or a binding asks for it.** Either way (c) is a borrowed pointer, which D-03 weighed: the maintainer says whether a borrow bounded by the worker's own next call is acceptable where one bounded by another thread's reload was not. This card is (a), plus (b) when the answer is (b) or (c); for (a) the maintainer says so under "Organiser's answers" and step 5 is skipped.
  * **The measurement.** A new cell in `bench/bench.c` beside `run_reload`: `run_live_get(threads)`. It opens a `cf_live` (as `run_reload`, `bench.c:293-300`) on a small configuration, starts `threads` threads (pthreads where `<pthread.h>` exists, `_beginthreadex` on Windows, behind one small shim in the file; a build with neither prints `live-get: skipped (no threads)`) that each do 1,000,000 times `cfg = cf_live_get(live); cf_config_free(cfg);` after a start barrier, and reports, per `threads` in `1, 2, 4, ...` up to twice the cores (`sysconf(_SC_NPROCESSORS_ONLN)`, `GetSystemInfo`), the pairs per second for all threads together and the nanoseconds per pair per thread, in the line format `run_reload` uses (label `live-get/<threads>`). Also the control, a thread count of 1 with `cf_config_ref`/`cf_config_free` alone (no lock), label `ref/1`, so the lock's own cost shows. A knee is where the total pairs per second at 2 threads is under 1.2 times that at 1, or falls from one count to the next; the page says whether one appears.
  * **The page.** `docs/Performance.md` gains "Readers under threads" with a table of the cells and one paragraph: the machine (cores, OS, compiler), the numbers, and "no knee, so a reader handle waits" or "a knee at N, so Y-11 is the next card". The page's static block is generated and gated (`cmake/gates/GatePerfpage.cmake`): this part is hand-written below it and must not be inside the generated block; run `ctest --preset default -R gate.perfpage` after writing it and follow its message. The numbers come from one Linux run in WSL from a copy off `/mnt` and are not gated (bench-gate gates instruction counts of `--scope quick` cells; this cell is wall-clock and is not added to it).
  * **The lock-free generation (when TO DECIDE 1 says b or c).** A `cf_refcount gen32;` in `struct cf_live`, initialised beside the ticket counters (`live.c:706-707`) and incremented in `live_swap` next to `l->generation += quiet ? 0u : 1u;` (`live.c:331`) with the same condition and still under the lock (so `gen32` equals `generation` modulo 2^32). `cf_live_generation` returns `(cf_u64)(cf_u32)cf_ref_get(&l->gen32)` and takes no lock. The `cf_u64 generation` stays for `cf_live_status_get`. The header comment of `cf_live_generation` (`cf_live.h:234`) says: "reads an atomic and takes no lock; compare it for equality: it counts swaps modulo 2^32". Rejected: a 64-bit atomic (the port has `i32` ones); changing the status field.
  * **D-192** (after D-191, "The read path is measured before it is changed"): Question: is the lock on `cf_live_get` a cost, and what does a worker pay to ask for the generation. Decision: the measurement is on the page; `cf_live_generation` is lock-free (for answer b or c); `cf_live_get` is unchanged; a reader handle is Y-11 and waits for the page's numbers or a binding's request. Rejected: hazard pointers and liburcu (the research's A3 and A4: the same handle as Y-11 at more code, or an LGPL dependency that fails `minimal` and `misra`); a pause or yield in the spin (not measured to matter; the page's numbers decide). Held to: `test_live`'s new generation case and the page.
* **files**: `bench/bench.c`, `docs/Performance.md`, `lib/config/src/core/live.c` and `lib/config/include/configforge/cf_live.h` (step 5), `lib/config/tests/test_live.c`, `CHANGELOG.md`, `docs/Decisions.md`; last: `docs/Codemap.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1. `git switch -c work/Y-10 main`.
  2. In `test_live.c` add `test_generation_lockfree` (for (b) and (c)): after `cf_live_open` the generation is 1 or what `cf_live_generation` returned before; a reload that changes a value makes it one more; a reload that finds the same values leaves it; a `CF_LIVE_APPROVE` live: pending leaves it, `cf_live_approve` adds one; a rollback leaves it (quiet swap); and four pthread readers (`#if` as the file's other thread tests) call `cf_live_generation` 100000 times each during 20 reloads and every value they see is at least the one seen before it by the same thread (monotonic). It passes before the change (it checks behaviour, not the lock): it is the oracle.
  3. Add `run_live_get` and the `ref/1` control to `bench/bench.c`, and the command-line way the other cells are asked for (`grep -n "run_reload" bench/bench.c` shows how it is selected; use the same, label `live-get`). Build `cf_bench` (`cmake --build --preset default --target cf_bench`) and run it: `build/bench/cf_bench live-get 2>&1 | tail -n 20` prints one line per thread count.
  4. See the cell is real: run it with `threads` forced to 1 and then to the largest, and see the two totals differ (a cell that prints equal numbers for both did not run its threads: fix before going on). Write "Readers under threads" into `docs/Performance.md` from one Linux run; `ctest --preset default -R gate.perfpage`.
  5. For (b) or (c): add `gen32`, change `cf_live_generation`, the header comment. `ctest --preset default -R "test_live|gate" 2>&1 | tail -n 20`; `ctest --preset tsan -R test_live`.
  6. See that the test can fail: make `live_swap` skip the `gen32` increment behind a run-time-false condition (`if (quiet == 0xFFu)`, never a constant); `test_generation_lockfree` fails; put it back.
  7. D-192 into `docs/Decisions.md`; the CHANGELOG entry.
  8. `python scripts/work/work.py format`; codemap last. Commit `Y-10: cf_live_get under threads on the page; cf_live_generation without the lock (D-192)`.
  9. ACCEPT; push `work/Y-10`; ACCEPT-CI (the `bench-gate` job included: it must not move).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI, bench-gate and `tsan` included; `ctest --preset default -R "test_live|gate.perfpage"` exit 0; `docs/Performance.md` holds the part with numbers from a real run.
* **stop if**: no answer to TO DECIDE 1; the bench's thread shim cannot be written without a new dependency (a build without pthreads prints the skip: that is not a stop); bench-gate moves on a cell (the card changed something it must not: revert and report); the machine has one core (the table has one row: write it, and say the knee cannot be seen).
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "the benchmark times `cf_live_get` under threads, and the page shows readers under 1 to twice-the-cores threads (Y-10)"; and, for (b) or (c), "### Changed": "`cf_live_generation` reads an atomic and takes no lock (Y-10, D-192)". `docs/Decisions.md`: D-192. Roadmap: marked by Y-12.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.

## Y-11 - `cf_live_reader`: a worker's handle that caches a snapshot, its test under ThreadSanitizer, C++ and Rust wrappers (TO DECIDE)

* **id**: Y-11
* **goal**: `cf_live_reader_open`, `_get`, `_generation`, `_release` and `_close` give a worker one borrowed snapshot that costs one shared read per call when nothing was swapped, with a test under ThreadSanitizer and wrappers in C++ and Rust.
* **why**: `cf_live_get` writes three shared counters and the snapshot's count on every call, and a shared write does not scale with cores (`docs/research/threads-and-processes.md:413-450`, option A2; arc-swap's `Cache`, Envoy's slot). Y-10 measures whether this matters; this card is for the case where it does or a binding asks.
* **needs**: Y-10 (its numbers and its answer to TO DECIDE 1), Y-9, TO DECIDE 1c
* **size**: medium (about 120 lines of C, two wrappers, a test)
* **decided**:
  * **TO DECIDE 1c (a new public interface for the read path).** Options: (a) none, Y-10's measurement alone, this card closed with no change; (b) Y-10's lock-free generation alone, this card closed; (c) `cf_live_reader` as sketched below, with Y-10's lock-free generation inside it. **Recommendation: (c) when Y-10's page shows a knee or a binding asks for it; (b) until then.** It is a borrowed pointer, bounded by the worker's own next call, which D-03 did not weigh: the maintainer says whether that is acceptable. For (a) or (b) this card is closed under "Organiser's answers" with no change.
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
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1c, or the answer is (a) or (b). `git switch -c work/Y-11 main` (after Y-10 and Y-9 have landed).
  2. Write `test_reader.c`, C89, `"cf_test.h"`, linked with `Threads::Threads` (guarded as `test_live` is, `lib/config/tests/CMakeLists.txt:289-298`). Cases: (a) one reader: `get` twice with no swap returns the same pointer and `cf_live_reader_generation` is unchanged; (b) after a reload that changes `server.port` the next `get` returns a different pointer with the new value and the generation is one more; (c) a rollback (probation) also changes the pointer: after `cf_live_rollback` the reader's `get` shows the rolled-back value; (d) `release` then `get` returns a snapshot again; (e) the old snapshot is not freed while another thread holds it by `cf_live_get` (a `cf_config_ref` count check through `cf_get_int` after the reader moved on); (f) `open(NULL, &err)` is NULL with `CF_E_CAPABILITY`; (g) four threads, each with its own reader, read `server.port` 100000 times while the main thread reloads 50 times with a different port each time; every value read is one the build returned and no thread reads a port older than one it read before. The same file is run under `ctest --preset tsan`.
  3. See it fail: `cmake --build --preset default --target test_reader 2>&1 | tail -n 15` (undeclared `cf_live_reader_open`).
  4. Add the epoch, `live_get_epoch`, the five functions, the declarations and the symbol lines. `ctest --preset default -R "test_reader|test_live|gate"`; `ctest --preset tsan -R "test_reader|test_live"`.
  5. See that the test can fail: make `live_swap` skip the epoch increment for a quiet swap behind a run-time-false condition (`if (keep == 0xFFu)`); case (c) fails; put it back.
  6. C++ wrapper and its test; Rust wrapper and its test (`cargo test` in `bindings/rust`, `cargo clippy -- -D warnings`, `cargo fmt --check`).
  7. D-193 into `docs/Decisions.md`; Guide; CHANGELOG.
  8. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-11: cf_live_reader, a worker's cached snapshot (D-193)`.
  9. ACCEPT; push `work/Y-11`; ACCEPT-CI (tsan, Rust, C++ jobs included).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_reader|test_live"` exit 0 and `ctest --preset tsan -R "test_reader|test_live"` exit 0; the benchmark of Y-10 re-run with a `reader/<threads>` cell shows the reader's cost at the thread counts of the page (written into "Readers under threads" in the same commit).
* **stop if**: no answer to TO DECIDE 1c, or the answer is (a) or (b); Y-10's page shows no knee and nobody asked (the card then waits: report); `gate.abi` or `gate.symbols` refuses the names; ThreadSanitizer reports a race in case (g) (a design fault of the epoch: report with the report's first lines, change nothing); a binding's gate demands the Python wrapper.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_live_reader`: a worker's handle that caches the live snapshot and costs one shared read when nothing was swapped; C++ `LiveReader`, Rust `LocalReader` (Y-11, D-193)". `docs/Decisions.md`: D-193. Roadmap: marked by Y-12.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "I approve"): TO DECIDE 1c is **(c)**: `cf_live_reader`, its borrowed pointer valid until the same thread's next call, a part beside `cf_live_get` (which stays the safe default, unchanged). It is gated by measurement: this card opens only when Y-10's page shows a knee with the number of cores, or a binding asks for it; until then (b), Y-10's generation read without the lock. Held to by its ThreadSanitizer test, and by lifetimes in the C++ and Rust wrappers.

## Y-12 - `swapped` in `cf_live_options`: workers told of a swap

* **id**: Y-12
* **goal**: `cf_live_options` has two members, `swapped` and `swapped_user`, and the live calls `swapped(user, generation, prev, next)` on the driving thread after every swap and rollback, once readers can get `next`; and the Roadmap item "Threads and processes made easier" is marked done.
* **why**: the library tells the driving thread a file moved (`cf_watcher`) and a program learns of a swap only by asking for the generation; a program with workers posts a closure to each (Envoy's way) from the one place that knows. The function runs on the program's own thread, as `check`, `health` and `log` do (D-35, D-48), so it adds no thread of the library's (`docs/research/threads-and-processes.md:452-468`, option B2, and 580-606).
* **needs**: TO DECIDE 2
* **size**: small
* **decided**:
  * **TO DECIDE 2 (workers told of a swap).** Options: (a) nothing: the generation, read at a safe point; (b) `swapped` in `cf_live_options`; (c) a descriptor per reader that becomes readable on a swap. **Recommendation: (b), with 2.2.0's changed keys.** For (a) or (c) the maintainer says so under "Organiser's answers"; for (a) this card is closed with no change (and the Roadmap mark is made by the organiser); (c) is a new card, not this one.
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
  * **The Roadmap item.** After this card has landed with Y-7 to Y-10 landed (Y-11 and Y-13-m may be open or parked), "**Threads and processes made easier.**" in `docs/Roadmap.md` (by its words) is marked 🟩 with one line under it: what was built (Y-7 to Y-12, D-190 to D-194) and that the reader handle (Y-11) and the shared image (Y-13-m) are on request. When one of Y-7 to Y-10 is still open, this card marks nothing and says so.
  * **D-194** (after D-193, "Workers are told of a swap by a function on the driving thread"): Question: how a worker learns of a reload. Options: the generation at a safe point; a function the program supplies (decided); a descriptor per reader. Decision: `swapped`, called on the driving thread from `live_swap` after readers can get the new snapshot, before the old is released; it posts to the program's own workers and calls back only `cf_live_get` and `cf_live_generation`. Rejected: a descriptor per reader (platform code per reader, a second watcher, `eventfd` wakes one); waiting on a generation (a blocking primitive the C89 core has not); a thread of the library's (D-35). Held to: `test_live`'s new case.
* **files**: `lib/config/include/configforge/cf_live.h`, `lib/config/src/core/live.c`, `lib/config/tests/test_live.c`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 15's closing list gains it), `docs/Roadmap.md`; last: `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 2. `git switch -c work/Y-12 main` (after Y-9 has landed).
  2. In `test_live.c` add `test_swapped`: a `swapped` that records into a struct (`calls`, last `generation`, last `prev`, last `next`, and for each call `cf_get_int(next, "server.port", 0)` and, when `prev != NULL`, of `prev`); a live opened with it in `CF_LIVE_MANUAL`; a changing reload: `calls` is 1, `next`'s port is the new one, `prev`'s is the old, `generation` equals `cf_live_generation(live)` called inside the callback (it is allowed); a reload that finds the same values: `calls` stays 1; `CF_LIVE_APPROVE` mode: a change builds pending with no call, `cf_live_approve` makes one; probation: a swap, then `cf_live_rollback`: `calls` goes up by one, `generation` is unchanged by the rollback, `next` shows the old value again; a live opened with a `size` below the new members (`sizeof(cf_live_options)` minus them, as the 1.0 callers pass) never calls it; a `swapped` that calls `cf_live_reload` on its live gets `CF_LIVE_FAILED` with `CF_E_POLICY` (Y-9). Add it to the test's main.
  3. See it fail: `cmake --build --preset default --target test_live 2>&1 | tail -n 15` (no member `swapped`).
  4. Add the type, the members, the initialiser, the copy in `cf_live_open_with` and the call in `live_swap`. `ctest --preset default -R "test_live|test_watch|gate" 2>&1 | tail -n 25`; `ctest --preset tsan -R test_live`.
  5. See that the test can fail: skip the call for the rollback's swap behind a run-time-false condition (`if (quiet == 0xFFu)` selecting a skip), the rollback case fails; put it back.
  6. D-194 into `docs/Decisions.md`; Guide; CHANGELOG; the Roadmap row as decided.
  7. `python scripts/work/work.py format`; codemap and apiref last. Commit `Y-12: swapped in cf_live_options, workers told of a swap (D-194)`.
  8. ACCEPT; push `work/Y-12`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live` exit 0 and `ctest --preset tsan -R test_live` exit 0; `gate.abi` and `gate.interface` green (the struct grows by its size rule).
* **stop if**: no answer to TO DECIDE 2; `cf_live_options` is used by a binding that copies the struct by value at a fixed size (report which: a size rule is then broken); a rollback's swap does not go through `live_swap` (read `live_rollback` first and report where it swaps); Y-9 is not landed (the last check of step 2 needs it: drop that check and say so).
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`swapped` in `cf_live_options`: a function called on the driving thread after every swap and rollback, for posting to a program's own workers (Y-12, D-194)". `docs/Decisions.md`: D-194. `docs/Roadmap.md`: "**Threads and processes made easier.**" marked 🟩 as decided.

---

## Y-13-m - A shared image across processes: memfd and seals, `shm_open`, a read-only section, the source (TO DECIDE 3b)

* **id**: Y-13-m
* **goal**: a parent process writes a configuration's image into an anonymous, sealed, read-only shared object; a child it hands the descriptor or handle to maps it read-only, refuses one that is not sealed or is writable, and reads it in place as a source, on Linux, FreeBSD, macOS and Windows, with the private file of the Guide's recipe 4 on every other platform.
* **why**: the image was designed to be used in place (`docs/ImageFormat.md`, "Why it exists"; `lib/config/include/configforge/cf_image.h:58-63`) and a child can only read it from a private file today (a copy). Sharing pages is the zero-copy form; it is worth its platform code only when a program asks (`docs/research/threads-and-processes.md:607-640`, section 5, "Later, if asked"). It waits on the maintainer's answer to TO DECIDE 3 and, after that, on a program that asks.
* **needs**: Y-7 (recipe 4 is the portable floor this card falls back to), Y-8, TO DECIDE 3b (the maintainer's)
* **size**: large (a platform capability in three files, a public type, a source, a build option; split by the organiser into Y-13a, the platform capability and its tests, and Y-13b, the public type, the source and the Guide, if the first alone passes ~600 lines)
* **decided**:
  * **PARKED, on the maintainer.** This card is an agent card whose start is the maintainer's answer to TO DECIDE 3 and a program's request: **TO DECIDE 3.** (a) Documentation only: Y-7's recipes 3 and 4 are all the library gives (C1 and C2 of the research); (b) the platform capability and the public type below, behind a build option `CF_WITH_IMAGE_SHARE` (off in `minimal`, `misra` and the embedded profiles; on in `server`). **Recommendation: (a) now; (b) only on a program's request**: C2 does the same job with one copy, and a configuration is kilobytes. With (a), this card is closed with no change and is not opened.
  * **TO DECIDE 4 (does an image carry the contract).** Today it does not: a child must be built with the same definitions. Carrying them is a new image section and a minor version (`docs/ImageFormat.md`: "fields only grow"); it belongs to the configuration compiler's research. **Recommendation: no**; the child's own contract decides. This card does not carry the contract.
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
    (the research names the last `cf_source_image_share`; the card uses `cf_image_share_source` to keep the prefix of the type: the maintainer may rename at the answer.) `flags` is 0 now and refused otherwise.
  * **The platform capability** (internal, `lib/platform/include/configforge/cf_platform.h`, `CF_INTERNAL_API`): `cf_platform_share_create(const void* bytes, size_t n, long* handle, cf_error*)` and `cf_platform_share_map(long handle, const char** base, size_t* n, cf_error*)`, `cf_platform_share_unmap(const char* base, size_t n)`. **Linux and FreeBSD 13+:** `memfd_create("configforge", MFD_CLOEXEC | MFD_ALLOW_SEALING)`, `ftruncate`, `write`, then `fcntl(fd, F_ADD_SEALS, F_SEAL_WRITE | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_SEAL)`; the child maps with `mmap(PROT_READ, MAP_SHARED)` and checks `fcntl(fd, F_GET_SEALS)` has all four, refusing otherwise (`'%s' is not a sealed image`): a read-only descriptor alone is not enough, since `/proc/<pid>/fd` reopens it (the research's item 10). **macOS and other BSDs** (no seals exist): `shm_open` of a random name with `O_CREAT | O_EXCL | O_RDWR`, `ftruncate`, `write`; then the same name opened a second time with `O_RDONLY`; then `shm_unlink` of the name at once. The read-only descriptor is the one handed over, the writable one is closed, so the child can only map `PROT_READ` (the parent is trusted: the child is given no way to write). **Windows:** an unnamed section (`CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, ...)`), filled through `MapViewOfFile(FILE_MAP_WRITE)`, then `DuplicateHandle` with `FILE_MAP_READ` access only; the child's `MapViewOfFile(FILE_MAP_READ)`; handed over by `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` in the program's own `CreateProcess` (never a name: item 16). **None, and any target where none of these is known:** `CF_E_CAPABILITY`, `this platform cannot share an image; write it to a private file (cf_platform_create_private) and pass the path`. How the number reaches the child (an environment variable as `LISTEN_FDS` and nginx's `NGINX` do, `SCM_RIGHTS`, the systemd store) is the program's: the library reads a number it is given and starts no process.
  * **Not decided here, left to the card that opens this one:** the exact `shm_open` name scheme and the errno wording (the opening card writes them with the code and the cold read checks them); the split into Y-13a and Y-13b.
  * **D-195** (after D-194, "A shared image is a sealed read-only object handed over by the program"): written when the card opens, from the answer to TO DECIDE 3 and 4.
* **files**: `lib/config/include/configforge/cf_image_share.h` (new), `lib/config/src/bridge/image_share.c` (new), `lib/platform/include/configforge/cf_platform.h`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/win32.c`, `lib/platform/src/none/none.c`, `lib/config/tests/test_share.c` (new), `lib/config/tests/CMakeLists.txt`, `CMakeLists.txt` and `cmake/profiles/*.cmake` (the option), `cmake/configforge.symbols`, `cmake/configforge.internal.symbols`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. Stop here: the maintainer's answer to TO DECIDE 3 is "(b)" and a program has asked, written under "Organiser's answers". Without both, nothing is done.
  2. The opening organiser has the card read cold against the tree (docs/Plan.md, "Read again after the layout changes"), splits it into Y-13a and Y-13b if it exceeds ~600 lines, and writes the open details above, the D-195 text and the exact steps as numbered steps with commands; this card is then replaced by those two. No agent starts from this text alone.
* **accept**: when opened: ACCEPT-LINUX (Linux and the `sanitize` preset), ACCEPT-WINDOWS, ACCEPT-CI with the macOS and FreeBSD jobs, a test that a child maps the share and reads the parent's keys, and that an unsealed or writable object is refused.
* **stop if**: TO DECIDE 3 has no answer "(b)", or no program has asked; the card is opened without its cold read.
* **docs**: when opened: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_image_share`: a configuration's image in a sealed read-only shared object, mapped by a child process (`CF_WITH_IMAGE_SHARE`) (Y-13-m, D-195)". `docs/Decisions.md`: D-195. Roadmap: nothing (the item is marked by Y-12; this card is on request).

---

## Y-14 - Compressed input by the program's own zlib: the D-103 recipe, a Guide section and an example

* **id**: Y-14
* **goal**: a program that links its own zlib can read a `.toml.gz` through ConfigForge today: the Guide has a section "Compressed input" showing a repair that inflates (D-103's composed codec) and `examples/c/inflate.c` builds and runs it under the suite.
* **why**: `docs/research/compression.md` section 5 ("A new interface ... Guide shows both") and section 6, first row: the recipe is what the library lacks a backend for, and it is also the answer before Y-15 to Y-18 exist.
* **needs**: nothing
* **size**: small
* **decided**:
  * **The recipe** is D-103's: a composed codec of a base codec and a repair (`CF_WITH_COMPOSE`, `cmake/ConfigForgeOptions.cmake:19`). The repair is a function the program supplies: it receives the compressed bytes and returns the text. The example inflates with the program's own `zlib` (`inflateInit2(&zs, 16 + MAX_WBITS)` for gzip), checking the output against a limit the program chooses (the example uses 1 MiB and says why: it is the program's bound, not the library's; Y-15's bound replaces it).
  * **Named only, read only, no shared bound** (research section 4.1): the Guide says so in one sentence each, and names Y-15 as the layer that detects and bounds.
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
* **stop if**: the composed codec's repair cannot receive binary bytes (D-103's interface takes text): report it, and Y-15's layer becomes the only route; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide section and `examples/c/inflate.c` on reading a gzip file through a composed codec with the program's own zlib (Y-14)." Roadmap: Y-20 marks "compression".

---

## Y-15 - The `cf_unpacker` interface: a byte layer under the codecs, its bounds, a NULL backend (TO DECIDE)

* **id**: Y-15
* **goal**: `cf_unpack_*` exists in `lib/serialize` (`cf_unpack_count`, `_at`, `_by_name`, `_detect`, `_limit`, `cf_unpack`, `cf_pack`, `cf_builder_unpacker`), with the bounds of research section 4.3, three new refusal reasons, and a test backend of the program's own (a repair that "decompresses" a run-length form), with no real compression library linked.
* **why**: `docs/research/compression.md` section 5, "First: reading gzip and zstd, as a byte layer" (option B): one bound against bombs instead of one per program, every reader unchanged. Sections 4.1 (placement) and 4.3 (bounds) hold the reasons.
* **needs**: Y-14, and TO DECIDE 1 to 6 below answered (default: the recommendations)
* **size**: large
* **decided**:
  * **TO DECIDE 1 (where compression sits).** (a) A composed codec, as the Roadmap says: no new interface, named only, no shared bound, read only. (b) A byte layer under the codecs beside D-108's recoding. **Recommended: (b)**, with (a) kept as Y-14's recipe. It changes the Roadmap's wording, so it is the maintainer's. With (a) this card and Y-16 to Y-20 are closed unbuilt and Y-14 stands.
  * **TO DECIDE 4 (the bounds' numbers).** `CF_UNPACK_FACTOR` 64, `CF_UNPACK_FLOOR` 1 MiB, `CF_UNPACK_MAX` 256 MiB, zstd `windowLogMax` 23. **Recommended as written**; each could become a builder option later if a user brings a file that needs more.
  * **TO DECIDE 5 (the interface).** One struct for both directions, `pack` NULL when a backend reads only. **Recommended**, as sketched below. The name `cf_unpacker` is the card's choice; `cf_compression` is the alternative; the card uses `cf_unpacker`.
  * **TO DECIDE 2, 3 and 6 are the later cards'** (Y-16, Y-18, and the release: this card's version is 1.10.0 by 6's recommendation).
  * **The interface**, in `lib/serialize/include/configforge/cf_serialize.h` (after the `cf_blob` declarations near line 63; `size` first as D-51 does), as in research section 5:
    ```c
    typedef struct cf_unpacker {
        size_t               size;       /* sizeof(cf_unpacker) */
        const char*          name;       /* "gzip", "zstd" */
        const char*          extensions; /* ".gz", ".zst;.zstd" */
        const unsigned char* magic;      /* NULL: by name only */
        size_t               magic_len;
        cf_bool (*unpack)(const struct cf_unpacker* self, const cf_blob* in, cf_arena* ar,
                          size_t limit, cf_blob* out, cf_error* err);
        cf_bool (*pack)(const struct cf_unpacker* self, const char* in, size_t n, int level,
                        cf_sink* out, cf_error* err); /* NULL: reads only; measure-or-write like cf_sink */
        const void* data;                /* a dictionary, a program's state */
    } cf_unpacker;
    CF_API size_t             cf_unpack_count(void);
    CF_API const cf_unpacker* cf_unpack_at(size_t i);
    CF_API const cf_unpacker* cf_unpack_by_name(const char* name);
    CF_API const cf_unpacker* cf_unpack_detect(const cf_blob* in, const char* path); /* extension, then magic; NULL when neither */
    CF_API size_t             cf_unpack_limit(size_t n);
    CF_API cf_bool cf_unpack(const cf_unpacker* u, const cf_blob* in, cf_arena* ar, cf_blob* out, cf_error* err);
    CF_API cf_bool cf_pack(const cf_unpacker* u, const char* in, size_t n, int level, cf_sink* out, cf_error* err);
    CF_API cf_bool cf_builder_unpacker(cf_builder* b, const cf_unpacker* u); /* a program's own, looked at before the built-in ones */
    ```
    `cf_unpack_limit(n)` is `min(CF_UNPACK_MAX, CF_UNPACK_FACTOR * n + CF_UNPACK_FLOOR)` with overflow checked (`n > (CF_UNPACK_MAX - CF_UNPACK_FLOOR) / CF_UNPACK_FACTOR` gives `CF_UNPACK_MAX`). `cf_unpack` calls the backend with that limit; a backend that returns more is refused. `cf_unpack` refuses a NULL `unpack` (`CF_E_CAPABILITY`, `'%s' cannot unpack`), and `cf_pack` a NULL `pack` (`'%s' cannot pack`).
  * **The bounds** (research section 4.3): constants in `lib/serialize/include/configforge/cf_serialize.h`, not build options; `scripts/generate/limits.cmake` writes them into `docs/Limits.md`'s generated part and a test holds the numbers. Output by input (factor and floor), an absolute ceiling, the decoder's memory (zstd's `windowLogMax` 23, written for Y-17). **One layer only**: unpacked bytes that start with a compression magic again are refused (`CF_REASON_NESTED`), never unpacked again; multi-member input is read by the backend in a loop.
  * **Reasons** (`lib/base/include/configforge/cf_reason.def`, after `CF_REASON_BUDGET`, line 25; the counting test and `docs/Limits.md` follow): `CF_REASON_UNPACK_BOMB` (`"unpack-bomb"`, "a compressed document that makes more than its bound"), `CF_REASON_UNPACK_CORRUPT` (`"unpack-corrupt"`, "a compressed document that does not decompress"), `CF_REASON_NESTED` (`"nested-compression"`, "compressed bytes inside compressed bytes"). Messages: `'<name>' (<codec>) would make more than <limit> bytes (the bound for <n> compressed bytes)`; `'<name>' (<codec>) is damaged: <backend's words>`; `'<name>' unpacks to compressed bytes again; one layer only`.
  * **Registry**: a static array of built-in unpackers (empty when no backend is built: `cf_unpack_count()` is 0), and the program's own through `cf_builder_unpacker` held in the builder (up to `CF_BUILDER_SOURCES` of them, `builder_fail` beyond with `no room for another unpacker`), consulted before the built-in ones. No global mutable state (D-101).
  * **D-196** ("Compression is a byte layer under the codecs, bounded once"): Question: how a program reads `app.toml.gz` (the Roadmap proposed a composed codec). Options: composed codecs (named only, M x N, no shared bound, read only); a byte layer beside D-108's recoding (decided); a built-in inflate (later, Y-21-m). Decision: the layer, one interface `cf_unpacker` for both directions, chosen by the program's name, then the outer extension, then the magic (as D-73 does for formats), refusable per source; three bounds checked while decompressing, never from a header; one layer only; backends are optional, OFF by default; a compression the library lacks comes from the program's own backend or Y-14's recipe. Rejected: nested decompression; brotli and raw deflate as detected formats; archives; a vendored zstd or liblzma.
  * **D-197** (the bounds, "A decompressed document is bounded by its input and by a ceiling"): the three numbers of TO DECIDE 4, as constants, with the reasons of section 4.3.
* **files**: `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/unpack.c` (new; add to `SERIALIZE_SOURCES`, find it with `grep -n SERIALIZE_SOURCES lib/serialize/CMakeLists.txt`), `lib/base/include/configforge/cf_reason.def`, `lib/serialize/tests/test_unpack.c` (new, in `cf_unit_tests(...)` of `lib/serialize/tests/CMakeLists.txt`), `scripts/generate/limits.cmake`, `cmake/configforge.symbols` (the public names; find the file with `ls cmake | grep symbols`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Limits.md` (generated), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1 (the others default to their recommendations unless an answer says otherwise). `git switch -c work/Y-15 main`.
  2. Write `test_unpack.c`, C89, with a test backend `rle`: input `<byte><count>` pairs (count 1 to 255), magic `"RL"`, extension `.rl`. Cases: (a) `cf_unpack_count()` is 0 with no backend built; (b) `cf_builder_unpacker` then a file `cf_up.toml.rl` is detected by extension and unpacked; (c) detected by magic with a name that has no extension; (d) `cf_unpack_limit(0)` is 1 MiB, `cf_unpack_limit(1024)` is 64 * 1024 + 1 MiB, `cf_unpack_limit(SIZE_MAX / 2)` is `CF_UNPACK_MAX`; (e) a bomb (`"RL"` then 5000 pairs of `x 255`, 1.2 MB out for 10 KB in, under 64x + 1 MiB: passes; then 300000 pairs for 600 KB in making 76 MB: over 64 * 600 KB = 38 MB + 1 MiB: refused with reason `unpack-bomb` and the message above); (f) output that starts with `"RL"` again: `nested-compression`; (g) a truncated body: `unpack-corrupt`; (h) `cf_pack` with a NULL `pack`: `'rle' cannot pack`; (i) the program's own backend wins over a built-in of the same name.
  3. See it fail: `cmake --build --preset default --target test_unpack 2>&1 | tail -n 20` (undeclared names).
  4. Write the declarations, `unpack.c`, the reasons, the limits generator and the symbols. `ctest --preset default -R "test_unpack|gate|reason" 2>&1 | tail -n 30`.
  5. See that the test can fail: skip the ceiling check behind a run-time-false condition (`limit == 0xFFFFFFFFu`); (e) fails; put it back.
  6. D-196, D-197, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-15: cf_unpacker, a byte layer under the codecs, its bounds (D-196, D-197)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_unpack` exit 0 in `default` and `minimal` (the layer has no dependency); `gate.limits` or the test that holds `docs/Limits.md` exit 0.
* **stop if**: no answer to TO DECIDE 1; `gate.symbols`, the reason-count test or the API-surface gate wants a change this card does not name (say which); the interface needs `cf_builder` internals that `lib/serialize` may not include (`gate.layering`): report, the registry then moves to `lib/config`; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`cf_unpacker`, a layer that unpacks compressed bytes before a reader sees them, with one bound against bombs (Y-15, D-196, D-197)." `docs/Decisions.md`: D-196, D-197. Roadmap: Y-20 marks "compression".

---

## Y-16 - gzip through zlib: CF_WITH_ZLIB (TO DECIDE)

* **id**: Y-16
* **goal**: with `-DCF_WITH_ZLIB=ON`, `cf_unpack_by_name("gzip")` unpacks gzip, members in a loop, with the library's own header reading, CRC-32 and ISIZE checked, trailing bytes refused, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (reading gzip first), section 4.2 (the options) and 4.4 (licence and supply chain: zlib 1.2.13 or later, CVE-2022-37434).
* **needs**: Y-15, and TO DECIDE 2 below answered
* **size**: medium
* **decided**:
  * **TO DECIDE 2 (new optional dependencies).** Options: both zlib and libzstd; zlib only; none, with a built-in inflate (Y-21-m) instead. **Recommended: both** (this card and Y-17), both OFF by default, ON in the `full` and `server` profiles (`CMakePresets.json:123-137`), Y-21-m later if an embedded user asks. With "none", Y-16 and Y-17 are closed unbuilt and Y-21-m is brought forward.
  * **The option**: `option(CF_WITH_ZLIB "gzip through zlib (requires zlib >= 1.2.13; zlib-ng in compat mode also)" OFF)` in `cmake/ConfigForgeOptions.cmake` after `CF_WITH_SQLITE` (line 23); `cmake/ConfigForgeProfile.cmake:20` lists the option for profiles (add `CF_WITH_ZLIB:BOOL` as `CF_WITH_SQLITE:BOOL` is); `lib/serialize/CMakeLists.txt` finds zlib as `lib/config/CMakeLists.txt:62-68` finds sqlite3 and fails with `CF_WITH_ZLIB needs zlib: install it (zlib1g-dev, brew install zlib, vcpkg zlib)`. The profiles `minimal` and `misra` keep it OFF.
  * **The floor, checked three ways** (D-… "enforce version floors", memory note, and the sqlite3 floor's pattern in `lib/config/CMakeLists.txt`): at configure time (`ZLIB_VERSION_STRING` `VERSION_LESS 1.2.13` is a `FATAL_ERROR`), at build time (a `#if ZLIB_VERNUM < 0x12d0` `#error` in `gzip.c`), at run time (`zlibVersion()` compared digit by digit in `cf_unpack_by_name("gzip")`, NULL and a refusal `CF_E_CAPABILITY`, `the zlib in use is <v>; ConfigForge needs 1.2.13 or later`); zlib-ng in compat mode reports its zlib version and passes.
  * **gzip's header is read by the library's own code**, never `inflateGetHeader`: 10 fixed bytes (`1f 8b`, method 8, flags, MTIME, XFL, OS); FEXTRA (2-byte length, skipped by length), FNAME and FCOMMENT (skipped to a NUL, at most 4096 bytes: more is `unpack-corrupt`), FHCRC (2 bytes, skipped, not checked), reserved flag bits set is `unpack-corrupt`. Then raw inflate (`inflateInit2(&zs, -15)`) over the body with the output limit; then CRC-32 (zlib's `crc32`) and ISIZE (low 32 bits of the length) checked against the 8-byte trailer; another member starting with `1f 8b` is read in a loop (the limit counts all members together); other trailing bytes are `unpack-corrupt`, `trailing bytes after the last gzip member`.
  * **The backend**: `lib/serialize/src/gzip.c` (new, in `SERIALIZE_SOURCES` only under `CF_WITH_ZLIB`), a `static const cf_unpacker gzip_unpacker` with `name "gzip"`, `extensions ".gz"`, magic `1f 8b 08`, `unpack = gzip_unpack`, `pack = NULL` (Y-20 sets it), registered in Y-15's built-in array. The output is allocated from the arena in steps doubling from 4 KiB up to the limit; zlib's allocator hooks are the arena's (`zalloc`, `zfree` with the arena, so no `malloc` the program does not see).
  * **Fuzz target** `tests/fuzz/unpack_gzip.c` in the form of the existing ones (`ls tests/fuzz | head`; copy the smallest), input: any bytes through `cf_unpack`, never crashing, never beyond the limit; a seed corpus of three gzip files (one good, one with FEXTRA and FNAME, one two-member).
  * **Dependencies.md**: a row in section 1 "Using the library": `zlib | zlib licence (Zlib) | CF_WITH_ZLIB | gzip; >= 1.2.13 (CVE-2022-37434); never vendored`. The SBOM generator (`configforge.spdx.json`) lists it when ON: find where sqlite3 is added (`grep -rn sqlite cmake/*.cmake scripts | head -5`) and add zlib the same way.
  * **D-198** ("gzip is read by our header code over raw inflate"): the three bullets above in one paragraph; Rejected: `inflateGetHeader` (a second parser inside zlib), a vendored zlib, `gzopen` (a file API, no bound).
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/gzip.c` (new), `lib/serialize/src/unpack.c`, `lib/serialize/tests/test_gzip.c` (new), `lib/serialize/tests/data/` (the gzip fixtures), `tests/fuzz/unpack_gzip.c` (new) and its CMake entry, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE 2. `git switch -c work/Y-16 main`.
  2. Make fixtures with the system `gzip`: `good.gz`, `extra.gz` (FEXTRA and FNAME; made by a ten-line Python script `scripts/work/` is not needed: write the bytes by hand in the test as a C array), `two.gz` (`cat a.gz b.gz`), `badcrc.gz` (one flipped CRC byte), `trunc.gz` (cut at half), `bomb.gz` (`head -c 100000000 /dev/zero | gzip -9`, 97 KB: 1000x: refused).
  3. Write `test_gzip.c` (guarded by `CF_WITH_ZLIB`; compiled to a test that prints `skipped: no zlib` otherwise): each fixture's result, the bomb's `unpack-bomb`, the bad CRC's `unpack-corrupt`, trailing garbage `trailing bytes after the last gzip member`, a header with a FNAME of 5000 bytes `unpack-corrupt`, the floor's message when `zlibVersion()` is faked (a function pointer seam `cfi_gzip_version_fn` in `gzip.c`, not a global).
  4. See it fail: `cmake -S . -B build-zlib -DCF_WITH_ZLIB=ON && cmake --build build-zlib --target test_gzip 2>&1 | tail -n 20` (no `gzip.c`).
  5. Write `gzip.c` and the CMake changes. `ctest --test-dir build-zlib -R "test_gzip|test_unpack|gate" 2>&1 | tail -n 30` and the default build's `ctest --preset default` still green.
  6. See that the test can fail: skip the CRC compare behind a run-time-false condition; `badcrc.gz` passes wrongly and its case fails; put it back.
  7. Fuzz a minute: `cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` (as P-3's step 4e does), with `CF_WITH_ZLIB=ON` in the fuzz preset.
  8. D-198, Dependencies row, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-16: gzip through zlib, CF_WITH_ZLIB (D-198)`.
  9. ACCEPT; push; ACCEPT-CI, whose `full` profile now builds with zlib.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (zlib from vcpkg, or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-zlib -R test_gzip` exit 0; the fuzz minute with no finding.
* **stop if**: no answer to TO DECIDE 2; `gate.dependencies` refuses the find call and the fix is more than one line of its allow-list; a gzip made by a tool of the CI image does not unpack (report the bytes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZLIB`: gzip through zlib, read by the library's own header code with CRC-32 and ISIZE checked (Y-16, D-198)." `docs/Decisions.md`: D-198. Roadmap: Y-20.

---

## Y-17 - zstd through libzstd: CF_WITH_ZSTD

* **id**: Y-17
* **goal**: with `-DCF_WITH_ZSTD=ON`, `cf_unpack_by_name("zstd")` unpacks zstd frames (several frames in a loop, skippable frames skipped) with `windowLogMax` 23 and the output limit, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (zstd beside gzip), section 4.2 and 4.4 (the zstd CVEs; the decoder's own memory bound).
* **needs**: Y-16 (its option wiring and fixtures' pattern; TO DECIDE 2 is answered there)
* **size**: medium
* **decided**:
  * **The option**: `CF_WITH_ZSTD` OFF by default, ON in `full` and `server`, wired exactly as Y-16 wires `CF_WITH_ZLIB`; the floor is the zstd release the research names in section 4.4 (read it: it is one number; the card's worker copies it into the three checks, configure, `#if ZSTD_VERSION_NUMBER < <n>` `#error`, and `ZSTD_versionNumber()` at run time).
  * **The backend** `lib/serialize/src/zstd.c`: `name "zstd"`, `extensions ".zst;.zstd"`, magic `28 b5 2f fd`, `pack = NULL`. Streaming decompression (`ZSTD_createDCtx`, `ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, 23)`, `ZSTD_decompressStream` into the arena in doubling steps up to the limit); a frame that asks for more window is refused with `unpack-bomb` and the size it asked for in the message (`'%s' (zstd) asks for a window of <n> bytes; the bound is 8388608`). Frames in a loop; a skippable frame (magic `5e 2a 4d 18` to `5f 2a 4d 18`) skipped by its length; other trailing bytes `unpack-corrupt`. The decompressed size in the frame header is never trusted: the limit is checked as bytes come out. zstd's allocator hooks are the arena's.
  * **Dictionaries are not here** (Y-23-m).
  * **Fuzz target** `tests/fuzz/unpack_zstd.c`, as Y-16's; seeds: one good frame, two frames, one with a skippable frame first.
  * **D-199** ("zstd is read with a window limit and a loop of frames"): the bullets in one paragraph. Rejected: a vendored libzstd (a large C99 surface under `misra`); dictionaries by default.
  * `docs/Dependencies.md` row: `libzstd | BSD-3-Clause (dual with GPL-2.0; the library uses the BSD one) | CF_WITH_ZSTD | zstd; never vendored`.
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/zstd.c` (new), `lib/serialize/src/unpack.c`, `lib/serialize/tests/test_zstd.c` (new), `tests/fuzz/unpack_zstd.c` (new), `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
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
* **stop if**: Y-16 is not landed or TO DECIDE 2 chose zlib only; `ZSTD_d_windowLogMax` is not in the installed zstd (older than the floor): the floor was wrong, report; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZSTD`: zstd frames through libzstd with a window limit (Y-17, D-199)." `docs/Decisions.md`: D-199. Roadmap: Y-20.

---

## Y-18 - The sources read through the layer: flags, extensions, provenance, bindings (TO DECIDE)

* **id**: Y-18
* **goal**: a file source, a drop-in directory source and the image file source unpack compressed bytes before a reader sees them: the program can name the compression (`CF_COMPRESSED_GZIP`, `CF_COMPRESSED_ZSTD`) or refuse it (`CF_COMPRESSED_NONE`); otherwise the outer extension and then the magic decide; the inner extension names the format; provenance reads `app.toml.gz (gzip)`; the C++, Rust and Python bindings pass the flags.
* **why**: `docs/research/compression.md` section 5 ("The layer is called from `cfi_source_text` ..., from the image file source, and from `configforge codec`" and "Provenance and errors name both").
* **needs**: Y-15, and TO DECIDE 3 below answered; Y-16 or Y-17 for a real backend (the tests use Y-15's `rle` backend where neither is built)
* **size**: medium
* **decided**:
  * **TO DECIDE 3 (detection by magic bytes when built in).** (a) On, refusable per source with `CF_COMPRESSED_NONE`. (b) Off unless the source names a compression or the outer extension does. **Recommended: (a)**: a magic is a declaration like a BOM, the bounds hold either way, and a program that built the backend in asked for it. (b) is the safer default for a program that never expects compressed input but links a build that has it. The steps are (a)'s; for (b) step 4's `by magic` branch is not written and the case (c) of step 2 expects the plain refusal.
  * **Flags** (`lib/config/include/configforge/cf_source.h`, source flags; beside the encoding value `CF_ENCODING_*` which takes bits 24-27, `lib/text/include/configforge/cf_encoding.h:27-28`): `CF_COMPRESSED_SHIFT 28`, `CF_COMPRESSED_MASK (3u << 28)`, values `CF_COMPRESSED_AUTO = 0` (the default), `CF_COMPRESSED_NONE = 1u << 28` (refuse compressed bytes: `'%s' is compressed and this source takes none`, reason `unpack-corrupt`'s sibling is not needed: `CF_E_SOURCE`, no reason), `CF_COMPRESSED_GZIP = 2u << 28`, `CF_COMPRESSED_ZSTD = 3u << 28`. They are accepted where `CF_ENCODING_MASK` is: `CF_FILE_SOURCE_FLAGS` and `CF_DROPIN_SOURCE_FLAGS` (`cf_source.h:249-255`) gain `CF_COMPRESSED_MASK`; other sources refuse them by name as they refuse any flag. If bits 28-29 are taken by a flag the worker finds (`grep -n "1u << 2[89]\|<< 28\|<< 29" lib/*/include/configforge/*.h`), stop and report.
  * **Where it is called**: `cfi_source_text` (`lib/config/src/bridge/file.c:292`) gains a first step before `cfi_source_recode`: when the flags and the name say the bytes are compressed, `cf_unpack` into the source's arena (the same arena as the text), `raw` then names the unpacked bytes; the codec was chosen from the path by the caller before this call (`file_named`), so the caller (`file_load` at 421-434 and `dir.c:178`) is changed to strip one outer extension when it is a known unpacker's (`app.toml.gz` names `toml`: `cf_codec_by_path` of the path without `.gz`). After unpacking, the recoding (D-108) and the detection by content go on as today. Nested compression is refused by Y-15's `nested-compression`.
  * **Provenance**: the source's `path` stays `app.toml.gz`; `cf_explain` and Doctor name `app.toml.gz (gzip)`: `file_state` gains `const char* unpacked` (the unpacker's name or NULL), shown by the code that shows `st->encoding` today (find with `grep -n "st->encoding" lib/config/src/bridge/file.c`); an error line and column are "in the decompressed text": the message is `app.toml.gz (gzip):3:5: ...` using the provenance's text as the file name for the reader's errors.
  * **Digest and fingerprint** are of the uncompressed bytes (the text the reader reads), so a file recompressed at another level has the same fingerprint.
  * **The image file source** (`cf_image_open` or the function that reads an image file: `grep -n "image" docs/Codemap.md | head`): the same call before the image header is read. The image bytes keep their own layout (nothing inside the format is compressed, research "Not to build").
  * **Bindings**: the C++, Rust and Python wrappers each pass the three new flag values where they pass `CF_ENCODING_*` (`grep -rn "CF_ENCODING_UTF16LE" bindings | head`); one test each that a flag reaches the call (with Y-15's backend registered through the C API, or skipped when no backend is built).
  * **D-200** ("A compressed file is read through the layer, by name, extension or magic"): the flags, the order (program's flag, outer extension, magic), `CF_COMPRESSED_NONE`, the digest of the uncompressed bytes, one layer.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/dir.c`, the image file source (named by the grep above), `bindings/cpp/`, `bindings/rust/`, `bindings/python/` (the files the grep above names), `lib/config/tests/test_compressed.c` (new; add to `lib/config/tests/CMakeLists.txt` after `test_file`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 8's "Compressed input", replacing Y-14's recipe-only text with both), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE 3 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-18 main` (after Y-15).
  2. Write `test_compressed.c` with Y-15's `rle` backend registered through `cf_builder_unpacker`: (a) `cf_ct.toml.rl` read by extension, `cf_explain` names `cf_ct.toml.rl (rle)`; (b) a file with no compression extension but the `RL` magic: read by magic (TO DECIDE 3a); (c) `CF_COMPRESSED_NONE` on the same file: refused with `is compressed and this source takes none`; (d) `CF_COMPRESSED_GZIP` on a file whose bytes are plain text: refused `is not gzip` (the flag names a compression, the bytes must be it); (e) a syntax error on line 2 of the unpacked text: the message names `cf_ct.toml.rl (rle):2:`; (f) the fingerprint of the unpacked file equals that of the same text in `cf_ct.toml`; (g) a drop-in directory holding `10-a.toml.rl` and `20-b.toml`: both read; (h) nested compression refused; (i) a flag the other sources do not know (`cf_builder_env_document` with `CF_COMPRESSED_GZIP`) is refused by name.
  3. See it fail: build `test_compressed` (the flags are undeclared).
  4. Write the flags, the call in `cfi_source_text`, the outer-extension strip, the provenance, the image source and the bindings. `ctest --preset default -R "test_compressed|test_file|test_dir|test_image|binding" 2>&1 | tail -n 30` (list the binding tests with `ctest --preset default -N | grep -i "py\|rust\|cpp"`).
  5. See that the test can fail: skip the unpack call behind a run-time-false condition; (a) fails; put it back.
  6. D-200, Guide, CHANGELOG. Format; codemap and apiref, last. Commit `Y-18: the file, directory and image sources read compressed bytes through the layer (D-200)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_compressed` exit 0 in `default` and `minimal`; with `-DCF_WITH_ZLIB=ON` a gzip case of the same test also passes.
* **stop if**: bits 28 and 29 are taken; the outer-extension strip changes a result of `test_file` or `test_dir` (a file named `x.json.gz` today is refused or read by content: say which); a binding gate asks for more than the flag values; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "File, directory and image sources read gzip and zstd files by flag, outer extension or magic; provenance names `app.toml.gz (gzip)` (Y-18, D-200)." `docs/Decisions.md`: D-200. Roadmap: Y-20.

---

## Y-19 - `configforge codec` through the layer, and a bomb corpus

* **id**: Y-19
* **goal**: `configforge codec detect`, `check` and `dump` read compressed files through the layer (`detect` says "gzip, then TOML by the extension"), and the test corpus holds the classic bombs.
* **why**: `docs/research/compression.md` section 5 ("`configforge codec` ... `detect` says ...") and section 6; the corpus is what holds the bounds against regressions.
* **needs**: Y-18
* **size**: small
* **decided**:
  * **Where**: `tools/codec/codec.c` (`detect` at line 76; find `check` and `dump` by name). Each calls `cf_unpack_detect` on the input as the source does; when it answers, `detect` prints `gzip, then TOML by the extension` (or `by the content`), `check` and `dump` read the unpacked bytes.
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
* **decided**:
  * **gzip** (`gzip.c`, Y-16): `pack` sets `deflateInit2(&zs, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)` and writes the 10-byte header itself (`1f 8b 08 00 00 00 00 00 <xfl> ff`: no name, MTIME 0, OS 255, XFL 2 for level 9, 4 for level 1, else 0), the deflate body, then CRC-32 and ISIZE; the default level is 6 (`level == 0` in `cf_pack` means "the default"; `1` to `9` are zlib's). Deterministic: the same input and level give the same bytes on any platform with zlib 1.2.13 or later and zlib-ng does not (zlib-ng's deflate output differs): the page and D-201 say determinism holds for one zlib implementation, and the test compares with a fixture only when `ZLIB_VERSION` is not zlib-ng's (`#ifdef ZLIBNG_VERSION`).
  * **zstd** (`zstd.c`, Y-17): `ZSTD_compressStream2` with `ZSTD_c_compressionLevel` (default 3; `level` 1 to 19), `ZSTD_c_checksumFlag` 1, `ZSTD_c_contentSizeFlag` 1, `ZSTD_c_nbWorkers` 0 (a thread count changes nothing in the bytes, but 0 keeps the library thread-free, D-14), no dictionary; one frame.
  * **The measure-or-write form**: `cf_pack` first emits to a sink that measures (`cf_sink`, as `cf_emit` does), then the caller allocates and calls again; the backend's `pack` supports it by running its encoder twice (research section 5: "Measure-or-write, like cf_sink").
  * **`configforge codec convert --compress gzip|zstd`** (`tools/codec/codec.c`): the output is the converted text packed; without the flag, an output named `.gz` or `.zst` packs by its outer extension and says so on stderr (`packing as gzip by the extension`).
  * **`cf_image_write`** (find with `grep -n cf_image_write docs/Codemap.md`): a path ending in a known outer extension is packed; `ImageFormat.md` gains one paragraph that the image's header and sections are those of the uncompressed bytes.
  * **The digest and fingerprint** are of the uncompressed bytes (Y-18), so a recompressed file has the same.
  * **Writing through F-1b's call** (`cf_emit_file`): an output path with a known outer extension is packed before the platform write; the file is replaced whole or not at all as any other.
  * **D-201** ("Compressed output is deterministic for one library and level"): the bullets above in one paragraph; Rejected: a name and a timestamp in the gzip header; threads in zstd; compression inside the image format.
* **files**: `lib/serialize/src/gzip.c`, `lib/serialize/src/zstd.c`, `lib/serialize/src/unpack.c`, `lib/config/src/bridge/emit.c` (`cf_emit_file`), the image writer, `tools/codec/codec.c`, `lib/serialize/tests/test_pack.c` (new), `tools/codec/tests/`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/ImageFormat.md`, `docs/Guide.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-20 main` (after the cards above).
  2. `test_pack.c`: (a) pack then `cf_unpack` returns the input for sizes 0, 1, 4096 and 3 MiB, both backends; (b) two packs of the same input and level are equal byte for byte; (c) a gzip pack has bytes 4 to 7 (MTIME) zero and byte 9 `ff` (the system `gzip -dc` accepts it: `system("gzip -dc cf_pk.gz > cf_pk.out")`, skipped when `system` fails); (d) the measure call returns the exact size the write needs; (e) the fingerprint of a configuration read from `cf_pk.toml.gz` equals the one read from `cf_pk.toml`; (f) `convert --compress gzip` of a JSON file gives a file `gzip -dc` turns back into the TOML text; (g) `cf_emit_file` to `x.toml.gz` reads back through Y-18.
  3. See it fail: build `test_pack` (`pack` is NULL, so `cf_pack` says `'gzip' cannot pack`).
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
* **needs**: Y-16, Y-20; and a user's ask (status: parked, waits on the ask)
* **size**: large
* **decided**:
  * **TO DECIDE (whether to build it at all).** Options: build (a small inflate, about 400 lines, as `tinfl` or `puff.c` shows; research section 4.5); never (a program on a small target compresses nothing); use the program's own inflate through Y-14's recipe. **Recommended: wait for a user who asks**, then build.
  * **Shape if built**: `lib/serialize/src/inflate.c`, one function of the `cf_unpacker` interface under the name `"gzip"` when `CF_WITH_ZLIB` is OFF and `CF_WITH_INFLATE` is ON (never both: a configure error `CF_WITH_INFLATE and CF_WITH_ZLIB both provide gzip`); reads only; the bounds of Y-15 and the header/trailer code of Y-16 are shared (`gzip_header.c`, moved out of `gzip.c` by the card, with no behaviour change); fixed and dynamic Huffman blocks, stored blocks, a 32 KiB window in the arena; no allocation outside the arena; a differential fuzz target `tests/fuzz/inflate_diff.c` comparing against zlib on every input (the `yamldiff` pattern: `tests/fuzz/yamldiff*.c`), required to find no difference in an hour.
  * Its steps, accept and stop-if are written when a user asks, by the cold read that opens it; this card is parked until then (docs/Plan.md, "Waits on the maintainer").
* **files**: `lib/serialize/src/inflate.c`, `lib/serialize/src/gzip_header.c`, `tests/fuzz/inflate_diff.c`, `cmake/ConfigForgeOptions.cmake`, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-202), `docs/Dependencies.md`.
* **steps**:
  1. Wait for the ask; then the opening cold read writes the steps from this card's "decided" and the pattern of Y-16's steps (test first, see it fail, the differential fuzz hour).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI and an hour of the differential fuzz without a difference.
* **stop if**: no ask has come; the inflate cannot be held to the `misra` rules without a deviation the maintainer has not recorded.
* **docs**: when built: `CHANGELOG.md`, "## 2.2.0 — unreleased" or the version then open, "### Added"; `docs/Decisions.md`: D-202.

---

## Y-22-m - xz and lz4 frames

* **id**: Y-22-m
* **goal**: `CF_WITH_LZMA` (xz through liblzma, refusing 5.6.0 and 5.6.1, with a `memlimit` of the order of zstd's window) and `CF_WITH_LZ4` (lz4 frames) as backends of the `cf_unpacker` interface, each OFF by default.
* **why**: `docs/research/compression.md` section 5, "Later" (E) and section 4.4 (the xz backdoor of 2024 and its refusal).
* **needs**: Y-17; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE (whether to build, and which of the two).** Options: xz only; lz4 only; both; neither. **Recommended: none until asked**; xz first when asked, because configuration archives from distributions are xz, and lz4 for a program whose own files are lz4.
  * **Shape if built**: as Y-17: `xz.c` (`name "xz"`, `.xz`, magic `fd 37 7a 58 5a 00`, `lzma_stream_decoder` with `memlimit` 8 MiB written as a constant in `cf_serialize.h` beside the others, `LZMA_CONCATENATED`, `lzma_version_number()` refusing `50060002` to `50060012`, i.e. 5.6.0 and 5.6.1, at run time as well as at configure time) and `lz4.c` (`.lz4`, magic `04 22 4d 18`, `LZ4F_decompress` with a frame-size check); fuzz target and fixtures for each; Dependencies rows.
  * The steps are written by the opening cold read, on Y-17's pattern.
* **files**: `lib/serialize/src/xz.c`, `lib/serialize/src/lz4.c`, tests, fuzz targets, `cmake/ConfigForgeOptions.cmake`, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-203).
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from Y-17's.
* **accept**: as Y-17, for each backend built.
* **stop if**: no ask has come; the installed liblzma is 5.6.0 or 5.6.1 (the refusal fires: the card stops and says so).
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-203.

---

## Y-23-m - zstd dictionaries on a source

* **id**: Y-23-m
* **goal**: a source can name a zstd dictionary (through `cf_unpacker`'s `data` field) so that small files compressed with one are read.
* **why**: `docs/research/compression.md` section 5, "Later" (F) and section 4.6 (dictionaries).
* **needs**: Y-18, Y-20; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE (whether to build at all, and the interface).** Options: a dictionary by the program's own `cf_unpacker` whose `data` is the dictionary (no new public name: the program makes a copy of the built-in zstd descriptor with its own `data`); a builder call `cf_builder_dictionary(b, source, bytes, n)`; never. **Recommended: the first**, only when someone asks; it needs nothing new.
  * **Shape if built**: `zstd.c` reads `self->data` as a dictionary (`ZSTD_DCtx_loadDictionary`), refuses a dictionary above 1 MiB (`CF_UNPACK_DICT_MAX`), and the frame's dictionary id is checked against the dictionary's (`'%s' needs dictionary <id>`); a frame that names a dictionary with none given is refused.
  * The steps are written by the opening cold read, on Y-17's pattern.
* **files**: `lib/serialize/src/zstd.c`, tests, the Guide's "Compressed input"; last: `CHANGELOG.md`, `docs/Decisions.md` (D-204).
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from Y-17's.
* **accept**: as Y-17.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-204.

---

## Y-24 - `configforge gen build`: sources by rank, a contract, a target, deterministic output (TO DECIDE)

* **id**: Y-24
* **goal**: `configforge gen build --schema S --source <file>=<rank> ... [--runtime env,cli] [--profile P] --target image|report|<format> [--out F]` reads the sources in rank order, builds the configuration through the library, and writes the target (`image` through `cf_image_write`, `report` through `cf_report`, a format name through `cf_emit`) to `--out` or stdout; the same inputs give the same bytes; exit 0 built, 1 a diagnostic of severity error (nothing written), 2 unusable input.
* **why**: `docs/research/config-compiler.md` section 5.1 item 1 and section 4.2 B and C: pieces that exist (`cf_image_write`, `cf_emit`, `cf_report`) become one command, which is the compiler the maintainer asked for. It is also the moment to stop the image freezing defaults (Y-26).
* **needs**: Y-34 (it shares the front half of `gen check`: the same function builds the configuration)
* **size**: medium
* **decided**:
  * **TO DECIDE A (where the compiler lives)**, research section 5.3 A. Options: **(1)** `configforge gen build`, a subcommand beside `gen check` and `gen schema`, with D-104's `gen --bake` spelled as its `--target c` (Y-27); **(2)** a fourth program `configforge build`, which reopens D-99's three programs; **(3)** only `gen --bake` as D-104 wrote it, the image left to C calls. **Recommended: (1).** The steps are (1)'s. With (3) this card and Y-25, Y-26 and Y-28 to Y-31 are closed unbuilt and Y-27 is written as `gen --bake` (its card says how).
  * **TO DECIDE E (the manifest's place)** is Y-29's; **F (defaults in the image)** is Y-26's; **B (a build file)** is Y-32-m's; **C (diagnostics)** is Y-28's; **D (rules across keys)** is Y-30-m's.
  * **Where.** A new `tools/gen/build.c` (`int gen_build(int argc, char** argv)`, declared in `tools/gen/gen.h`), listed in `tools/gen/CMakeLists.txt`; `gen.c`'s `main` (line 42-60) gets `if (argc >= 2 && strcmp(argv[1], "build") == 0) return gen_build(argc - 2, argv + 2);` before the `check` branch; `usage()` (gen.c:22-40) gains the `build` line. The front half is shared with `gen_check` (`tools/gen/contract.c:50-100`): the code that makes a builder from a schema and the files moves into one function `static cf_builder* gen_builder(const char* schema, int nsrc, const gen_source* src, cf_error* err)` in `tools/gen/gen.h`'s companion `common.c` (new; both commands call it), a change of structure with no behaviour change: `ctest -R gen_check` (Y-34's tests) must pass unchanged after step 4.
  * **Options.** `--schema <file>` once (required). `--source <file>=<rank>` repeatable: the rank is a name of `cf_define.h:345-348`'s ranks (`sys-file`, `user-file`, `env`, `cli`, ...: read the enum and accept exactly its names with `-` for `_`, lowercase; an unknown name is exit 2, `unknown rank '<x>'; ranks are <list>`); sources are applied in the order of their rank (the library does that: the command takes them in the order given). `--runtime <ranks>` (comma-separated, from the same names): the ranks the program will still have at run time, used only by Y-25's checks; accepted and stored here, `unused` until Y-25. `--profile <name>`: passed to `cf_report_info.profile` and, when the contract has profile-dependent definitions, to the builder as the profile (find the builder call with `grep -n "cf_builder_profile" lib/config/include/configforge/*.h`; if there is none the option only labels the report). `--target <t>`: `image`, `report`, or the name of a codec that writes (`cf_codec_by_name`, with `CF_CODEC_WRITE`); anything else is exit 2, `unknown target '<t>'; targets are image, report and the formats that can be written: <list>`. `--out <file>` (a path, or `-` for stdout; the default is stdout for `report` and a format and a refusal for `image` without `--out`: `an image is binary; give --out`). Written whole or not at all (`cf_platform_write_file`, F-1a).
  * **Targets**: `image` calls `cf_image_write(ks, sv, flags=0, buf, cap)` measure-then-write (`cf_image.h:54`; the key space and source view come from the built configuration: find the accessor with `grep -n "cf_config_keyspace\|cf_config_view" lib/config/include/configforge/*.h`; if none is public, the tool uses the internal tier (`CF_INTERNAL_API`) as `tools/doctor` does for reports); `report` calls `cf_report` with `CF_REPORT_BLIND`; a format calls `cf_emit(c, format, CF_EMIT_ALL, 0, ...)` as `cf_emit_file` does.
  * **Deterministic**: no clock, no host name, no environment read for the output (a `--source` of kind `env` is not offered: environment is a run-time rank, `--runtime env`). The report's `generated` is NULL. Two runs give identical bytes: a test compares them.
  * **Exit codes** as `gen check`: 0; 1 when the build refuses (`cf_builder_create` returns NULL: the first error is printed as `<file>:<line>:<col>: <message>` the way `gen check` prints it, `printf("%s\n", err.message)`), nothing written; 2 unusable input (a schema or file unreadable, a bad option, an unwritable `--out`).
  * **D-205** ("The compiler is `gen build`, and it is deterministic"): TO DECIDE A's (1) with the shape above; the targets are those that exist; `--target c` is Y-27's. Rejected: a fourth program; a configuration language of our own (research 4.2 A; [38]); text templates (section 3, item 4); unification in place of ranks (it removes the override every program needs from its command line); planning against live state; an input-addressed store; a plug-in protocol for targets beyond D-110's.
* **files**: `tools/gen/build.c` (new), `tools/gen/common.c` (new), `tools/gen/gen.c`, `tools/gen/gen.h`, `tools/gen/contract.c`, `tools/gen/CMakeLists.txt`, `tests/gen_build/*` (new fixtures), `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE A (the default is (1): an answer is needed only to choose (2) or (3)). `git switch -c work/Y-24 main` (after Y-34).
  2. Fixtures: Y-34's schema; `defaults.toml` (all keys), `site.toml` (overrides `server.port`), `bad.toml` (`server.port = 70000`).
  3. Tests: (a) `gen build --schema S --source defaults.toml=sys-file --source site.toml=user-file --target toml --out out.toml` exits 0 and `out.toml` holds `port = 9000` from site (the override wins by rank, in either order of the options); (b) `--target report` writes a blind report that `doctor lint` accepts and that names `site.toml` for `server.port`; (c) `--target image --out out.cfgb` then a C test program (`tests/gen_build/load_image.c`, built into the suite) opens it with `cf_image_open` and reads `server.port` = 9000, and `explain` names the source when the image was written with provenance (Y-26 adds the flag; here only the value); (d) two runs, `cmp` equal, for each of the three targets; (e) `bad.toml` exits 1 with `server.port` in the message and no output file; (f) an unknown rank, an unknown target, a missing `--schema`, `--target image` with no `--out`: each exit 2 with its message; (g) the compiled image's values equal those of a configuration built by `cf_build` with the same files in the same program (`load_image.c` builds both and compares every key: "compared with layer 0").
  4. Move the common front half; run `ctest --preset default -R gen_check` unchanged. See the new tests fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20` (no `build` subcommand).
  5. Write `build.c`. `ctest --preset default -R "gen_build|gen_check|image"`.
  6. See that the test can fail: let a later source lose by order of options behind a run-time-false condition; (a) fails; put it back.
  7. D-205, `docs/Tools.md`, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-24: configforge gen build (D-205)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_build|gen_check"` exit 0.
* **stop if**: no answer to TO DECIDE A; the key space and source view are not reachable by the tool without a new public function (report which: it is a new public interface and the maintainer's); `gen` links only part of the library and cannot reach `cf_image_write` (report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`configforge gen build`: sources by rank and a contract to an image, a report or a written format, deterministically (Y-24, D-205)." `docs/Decisions.md`: D-205. `docs/Roadmap.md`: "A configuration compiler" under "From the maintainer, 2026-11-26" stays 🟨 until Y-31.

---

## Y-25 - The open-key checks: what the compiler can prove about the keys left to run time

* **id**: Y-25
* **goal**: `gen build --runtime <ranks>` computes, for each key, which run-time ranks may still set it, and refuses (exit 1) a required key no source supplies and no run-time rank may supply, and a template whose referenced key is open; it lists the keys no run-time rank may change as sealed.
* **why**: `docs/research/config-compiler.md` section 4.2 E and section 6, second row: the compiler proves the contract can be met, which no other tool the research names does for a layered configuration.
* **needs**: Y-24
* **size**: small
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
* **needs**: Y-24, and TO DECIDE F below
* **size**: small
* **decided**:
  * **TO DECIDE F (defaults in a compiled image).** (1) left out unless asked; (2) written, as today. **Recommended: (1)** for the command; the C call keeps today's behaviour unless the new flag is given, so 1.x callers see no change. With (2) the command writes defaults and `--with-defaults` is not offered; the flag still exists.
  * **The flag**: `cf_image.h:44-49`: `CF_IMAGE_CONFIGURED = 1u << 1`, "only what a source set: no defaults". An added enum value; the version of the format does not change (the image carries fewer keys). `cf_image_write` (`lib/config/src/*/image.c`: find with `grep -n "cf_image_write" lib/config/src/*/*.c`) skips a key whose winning candidate's source is the contract's default (the `CF_TRANSIENT`-like test `emit.c:36`'s `keep` uses for `CF_EMIT_CONFIGURED`: copy its test, do not write a second one).
  * **Secrets**: `gen build` refuses to write a `CF_SENSITIVE` key's value into the image, a report with `CF_REPORT_FULL`, or an emitted document, unless `--allow-secrets`: exit 1, `'<key>' is sensitive and would be written to the output; pass --allow-secrets to write it`. A blind report never holds a value, so it is not refused.
  * **D-207** ("An image keeps only what was configured, unless asked"): the flag, the two command options and their defaults. Rejected: stripping defaults silently from the existing flags.
* **files**: `lib/config/include/configforge/cf_image.h`, the image writer, `tools/gen/build.c`, `lib/config/tests/test_image.c` (the image test file: `ls lib/config/tests | grep image`), `tests/gen_build/`, `docs/ImageFormat.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE F (the default is (1)). `git switch -c work/Y-26 main` (after Y-24).
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

## Y-27 - The baked configuration: `gen build --target c` and `cf_config_from_baked`

* **id**: Y-27
* **goal**: `configforge gen build --schema S --source ... --target c --out device_cfg.c` resolves a configuration at build time and writes it as C89 `static const` tables with provenance ("from device.toml line 12, at build time"); a program that links only that file and the baked reader reads the same values and gets the same `explain` text as layer 0, with no parser linked.
* **why**: `docs/research/config-compiler.md` section 4.2 C (baked C tables, "designed (D-104)", L) and section 6, fourth row; D-104, layer 1 for C (`docs/plan/A-architecture.md:258-270`): "no parser in the binary ... the field docs/Market.md found open". The card is not written in any other group's file (searched `docs/plan/*.md` for `cf_config_from_baked`: only the architecture page names it), so it is here, as the research's own list says it belongs to 1.9.0.
* **needs**: Y-24, Y-26
* **size**: large
* **decided**:
  * **The target**: `--target c` in `gen_build` (Y-24's target list gains `c`; the unknown-target message lists it). The output is one C89 file: a header comment with the schema's name, the sources and their digests, `static const cf_baked_entry <name>_entries[]` sorted by key (for binary search by the reader), and `const cf_baked <name> = { sizeof(cf_baked), 1, <count>, <name>_entries, <name>_strings }`; `--name <name>` (default `baked_config`) is the C identifier prefix, checked as a C identifier (exit 2 otherwise).
  * **The shapes** (declared in a new public header `lib/config/include/configforge/cf_baked.h`, experimental tier (D-89); `size` first, D-51):
    ```c
    typedef struct cf_baked_entry {
        const char* key;      /* NUL-terminated */
        unsigned char kind;   /* CF_BAKED_BOOL, _INT, _FLOAT, _STR (from cf_value's kinds) */
        long long   i;        /* bool, int */
        double      f;
        const char* s;        /* string, NUL-terminated */
        const char* origin;   /* "device.toml:12" at build time, or NULL */
        const char* source;   /* the source's name */
    } cf_baked_entry;
    typedef struct cf_baked { size_t size; unsigned version; size_t count; const cf_baked_entry* entries; } cf_baked;
    CF_API const cf_baked_entry* cf_baked_find(const cf_baked* b, const char* key);        /* NULL when absent */
    CF_API cf_bool cf_baked_get_int(const cf_baked* b, const char* key, long long* out);
    CF_API cf_bool cf_baked_get_bool(const cf_baked* b, const char* key, cf_bool* out);
    CF_API cf_bool cf_baked_get_str(const cf_baked* b, const char* key, const char** out);
    CF_API size_t  cf_baked_explain(const cf_baked* b, const char* key, char* buf, size_t cap); /* measure-or-write; the text of layer 0's explain for a baked winner, ending "(baked at build time)" */
    ```
    `cf_config_from_baked()` of D-104's text is these functions (one reader, `get` and `explain` over the tables): the name is kept in D-208 as an alias note, not as a function: **TO DECIDE (the public shape)**: options: **(a)** the five functions above (a baked configuration is its own small type); **(b)** `cf_config_from_baked(const cf_baked*)` returning a `cf_config*` that works with the whole existing API (needs the config core, so the program links layer 0, which defeats "no reader linked"); **(c)** both, (b) as a `lib/config` convenience. **Recommended: (a)** (it is what "links no reader at all" needs; D-104 names the function as an idea, not a signature), (b) later if a program wants the full API over baked data.
  * **No parser linked**: `cf_baked.c` (new, `lib/baked/` as a component of its own, D-100, built as `libconfigforge_baked.a`, depends on `base` only: `lib/baked/CMakeLists.txt`; `gate.layering` is told it may include `cf_base` and nothing else: one line in the layering table, found with `grep -rn "serialize" scripts/work/layering* cmake/*layer* | head -3`) is C89, allocates nothing, calls nothing from `lib/serialize`. A test links a program against `libconfigforge_baked.a` only and checks with `nm` that no `cf_codec_`/`json`/`toml` symbol is in the link (`gate.baked_nolink`, an `add_test` running `nm --defined-only` over the test binary; skipped without `nm`).
  * **Compared with layer 0** (the test of the card): for every key of the example contract, `cf_baked_find` and `cf_config_get` agree on kind and value; `cf_baked_explain` and `cf_explain` agree on the winning source name and the origin, up to the suffix.
  * **Deterministic output**: no date; entries sorted; the same inputs give the same file. **Secrets**: refused unless `--allow-secrets` (Y-26's rule): a `CF_SENSITIVE` key's value is never baked by default; the entry is written with kind `CF_BAKED_ABSENT` and the origin kept, so `explain` says it was set and withheld.
  * **Arrays and lists**: a key with `.count` and indexed keys (`list.0`, `list.1`) is baked as separate entries, as the key space holds them; a key kind with no baked form (a duration or size, which `cf_value` may carry) is baked as its integer in the unit the value holds, with the kind it has. If the key space holds a kind this list does not name, stop (`stop if`).
  * **D-208** ("A baked configuration is a small type with its own reader, written by `gen build --target c`"): the target, the five functions, the no-parser rule, provenance at build time, secrets withheld. Rejected: a parser in the baked reader; C23 `#embed` (D-104's other kit, a separate thing: read at start-up).
* **files**: `tools/gen/build.c`, `tools/gen/baked.c` (new: the C emitter), `lib/baked/` (new: `CMakeLists.txt`, `include/configforge/cf_baked.h`, `src/baked.c`, `tests/CMakeLists.txt`, `tests/test_baked.c`), `CMakeLists.txt` (add the subdirectory, found by `grep -n add_subdirectory CMakeLists.txt`), `cmake/configforge.symbols`, `scripts/work/` layering table (named above), `tests/gen_build/baked_main.c` (new), `docs/Guide.md` (a section "A configuration baked into the program", after section 12 "Small targets"), `docs/Design.md` (one paragraph under D-104's layer 1); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE (the default is (a)). `git switch -c work/Y-27 main` (after Y-24 and Y-26).
  2. Write `lib/baked/tests/test_baked.c`, C89, with a hand-written `cf_baked` table of five entries (one of each kind, one with origin NULL): `cf_baked_find` hits and misses, the three getters, a getter of the wrong kind returns `CF_FALSE`, `cf_baked_explain` measure-or-write, a NULL table is refused.
  3. See it fail: `cmake --build --preset default --target test_baked 2>&1 | tail -n 20` (no component).
  4. Write the component, the symbols, the layering line. `ctest --preset default -R "test_baked|gate" 2>&1 | tail -n 30`.
  5. In `tests/gen_build/` write the end-to-end test: `gen build ... --target c --out baked_cfg.c`; `baked_main.c` includes the generated file, prints each key; a CMake `add_test` builds it with the generated file (`add_custom_command` from the `gen` target) and compares with `cf_config_get` over the same files in a second program (layer 0): the two programs' outputs are `diff`ed by the test. `gate.baked_nolink`.
  6. See it fail: run before the emitter exists (`ctest -R gen_build_baked`); then write `tools/gen/baked.c`; it passes.
  7. See that the test can fail: bake a value off by one behind a run-time-false condition in the emitter; the diff fails; put it back.
  8. D-208, Guide, Design paragraph, CHANGELOG. Format; codemap and apiref, last. Commit `Y-27: the baked configuration, gen build --target c and cf_baked (D-208)`.
  9. ACCEPT; push; ACCEPT-CI (the embedded targets' jobs, if they exist yet, build the baked example).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_baked|gen_build|gate.baked_nolink"` exit 0; `ctest --preset minimal -R test_baked` exit 0.
* **stop if**: no answer to the TO DECIDE; the key space holds a kind of value this card does not name; `gate.layering` or `gate.symbols` cannot take a new component without a rule this card does not write; the generated file is not valid C89 under `-std=c89 -pedantic` (a gate: add it to the test); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`gen build --target c` and `cf_baked`: a configuration resolved when the program is built, read with no parser linked (Y-27, D-208)." `docs/Decisions.md`: D-208. `docs/Roadmap.md`: the item "A baked configuration" (found by `grep -n "baked" docs/Roadmap.md`, line 1581 area) 🟩.

---

## Y-28 - `cf_builder_diagnostics`: every diagnostic with its place, and `gen build --json` and `gen check --all` (TO DECIDE)

* **id**: Y-28
* **goal**: a builder option `cf_builder_diagnostics(b, fn, user)` keeps going past the first refusal and hands every diagnostic (severity, code, reason, key, source, place, message) to `fn` during `cf_builder_create`, which still fails when any was an error; `gen build --json` and `gen check --all` print the whole list, shaped like `terraform validate -json`.
* **why**: `docs/research/config-compiler.md` section 4.2 D and section 5.1 item 2: a compiler needs the whole list, not the first error; the program itself can then list every problem at start-up. It is where "one diagnostic model" (docs/Maintaining.md, the 1.8.0 row; what remains is K-6a and K-6b in docs/plan/K-correctness.md) leads.
* **needs**: Y-24, and TO DECIDE C below answered
* **size**: medium
* **decided**:
  * **TO DECIDE C (a public diagnostics interface).** **(1)** `cf_builder_diagnostics` in the experimental tier (D-89); **(2)** inside the tool only, by building with every key as `CF_INVALID_FALLBACK` and walking the key space for rejected candidates (`cf_keyspace.h:131`, `143-152`): no new interface, but it misses errors that are not a candidate's (an unreadable file, an unknown key under reject) and holds a second reading of what is an error. **Recommended: (1)**, so a program and the compiler report the same list. With (2) the tool does the walk and the three public names below are not added.
  * **The names** (experimental tier, `lib/config/include/configforge/cf_diag.h`, new; `size` first, D-51), as research section 5.2:
    ```c
    typedef enum cf_severity { CF_SEV_ERROR = 0, CF_SEV_WARNING, CF_SEV_NOTE } cf_severity;
    typedef struct cf_diag {
        size_t size; cf_severity severity; cf_code code; int reason; /* CF_REASON_* (D-107) */
        const char* key; const char* source; const char* at; const char* message;
    } cf_diag;
    typedef void (*cf_diag_fn)(const cf_diag* d, void* user);
    CF_API cf_bool cf_builder_diagnostics(cf_builder* b, cf_diag_fn fn, void* user);
    ```
    `key` NULL when none; `at` is `file:line:col` as `explain` writes it, or NULL; a sensitive key's value never appears in `message` (a test greps for it).
  * **Where**: `lib/config/src/core/config.c`: the builder struct (line 48) holds `fn` and `user`; where the builder stops at the first strict refusal (find each `builder_fail` call that ends `cf_builder_create`, with `grep -n "builder_fail\|return NULL" lib/config/src/core/config.c | head -40`), with `fn` set it records the diagnostic and continues when the refusal is local to one key or one source (a value out of range under strict; an unreadable source; an unknown key under reject), and stops where continuing is meaningless (out of memory, a schema that cannot be read). After the build, `cf_builder_create` returns NULL if any diagnostic had severity error, with the first as its `err`, as today.
  * **`gen build --json`**: prints `{"valid": false, "error_count": n, "warning_count": m, "diagnostics": [{"severity": "error", "summary": <message>, "detail": null, "key": ..., "source": ..., "range": {"filename": ..., "start": {"line": l, "column": c}}}]}` (the shape of `terraform validate -json`, research source [22]) to stdout; exit 1 when `valid` is false. **`gen check --all`** prints the same list (as text without `--json`: one `<place>: <severity>: <message>` line each). Default `gen check` stays at the first error (exit codes unchanged).
  * **D-209** ("Every diagnostic, by callback, experimental"): TO DECIDE C's (1), the shape, and that a failed build with a callback still fails.
* **files**: `lib/config/include/configforge/cf_diag.h` (new), `lib/config/src/core/config.c`, `cmake/configforge.symbols`, `tools/gen/build.c`, `tools/gen/contract.c`, `lib/config/tests/test_diag.c` (new), `tests/gen_build/`, bindings: none (experimental tier, not wrapped); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE C (the default is (1)). `git switch -c work/Y-28 main` (after Y-24).
  2. `test_diag.c`: a builder with a schema and three bad files (a value out of range, an unknown key, an unreadable optional-not file): (a) without the callback `cf_builder_create` returns NULL with the first error only; (b) with it the callback is called three times, severities error, each with its key and `file:line` place; (c) the build still returns NULL; (d) a sensitive key's bad value does not appear in any message; (e) with all files good the callback is never called and the build succeeds; (f) a warning-only build (a deprecated key set) calls it with `CF_SEV_WARNING` and succeeds.
  3. See it fail: `cmake --build --preset default --target test_diag 2>&1 | tail -n 20`.
  4. Write the header, the builder option and the continuing. `ctest --preset default -R "test_diag|test_config|test_define|gate"`: every existing result unchanged.
  5. See that the test can fail: stop after the first diagnostic behind a run-time-false condition; (b) fails; put it back.
  6. `gen build --json` and `gen check --all`, with their tests (a golden JSON in `tests/gen_build/`).
  7. D-209, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-28: cf_builder_diagnostics, every diagnostic with its place (D-209)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_diag|gen_build"` exit 0; every `test_config*` result unchanged.
* **stop if**: no answer to TO DECIDE C; continuing past a refusal leaves the builder in a state that a later step reads wrongly (a crash or a wrong second diagnostic: report the case); K-6a or K-6b (what remains of "one diagnostic model") changed `cf_error` so that `code`/`reason` differ from this card's fields (the card then follows the model's names, and says so under notes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`cf_builder_diagnostics` (experimental): every diagnostic of a build with its place; `gen build --json` and `gen check --all` print them (Y-28, D-209)." `docs/Decisions.md`: D-209.

---

## Y-29 - The manifest: inputs and their digests, the contract's digest, version, profile, open keys, the output's digest

* **id**: Y-29
* **goal**: `gen build --manifest <file>` writes a small JSON beside the artifact: each input file and its SHA-256, the contract's SHA-256, the library's version, the profile, the open (run-time) and sealed keys, the output's SHA-256 and `SOURCE_DATE_EPOCH` when it is set; the same inputs give the same manifest.
* **why**: `docs/research/config-compiler.md` section 4.2 H and section 6 (sixth row): a build can say what it was made from, as a reproducible build does.
* **needs**: Y-24, Y-25, and M-34 (SHA-256 as a digest, D-184: "one digest a purpose")
* **size**: small
* **decided**:
  * **TO DECIDE E (the manifest's place)**, research section 5.3 E. Options: **(1)** a JSON file beside the artifact; **(2)** a section of the image. **Recommended: (1) now, (2) with 2.0.0's image header (Y-33).** The steps are (1)'s.
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

## Y-30-m - Rules across keys in the contract: a fixed declarative set (TO DECIDE D)

* **id**: Y-30-m
* **goal**: the contract can say, with a fixed set of declarative rules and no expression language, that one key requires another (`dependentRequired`), that a key's value is at least another's (`minimum` by key), or that one of a set of keys is given (`oneOf`); the library checks them at run time, and therefore the compiler at build time.
* **why**: `docs/research/config-compiler.md` section 4.2 G (option 2), section 5.3 D and section 6 (seventh row): a compiler that cannot check "if `tls.enabled` then `tls.cert`" proves less than the program does at start-up.
* **needs**: Y-24; and TO DECIDE D answered with the analysis tool's TO DECIDE 2 (Y-39-m)
* **size**: medium
* **decided**:
  * **TO DECIDE D (rules across keys).** (1) none, recipes for OPA and CUE (Y-36, Y-31); (2) a fixed declarative set in the contract; (3) an expression language. **Recommended by this research: (2), decided with the analysis-tool research**, whose own TO DECIDE 2 recommends (a) none now and (b) only on a user's ask: the two documents ask the same question, so the maintainer answers once, for both. This card is (2)'s declarative set without a regular-expression engine (that is Y-39-m's `pattern`); if the answer is (1), this card and Y-39-m are closed unbuilt; (3) is not offered.
  * **The set**: JSON Schema's `dependentRequired` and `if`/`then` over `const` and `enum` tests (not over patterns), plus `x-configforge-atleast: {"key": "pool.max", "other": "pool.min"}` for "at least another key" (JSON Schema has no relation between two values; the `x-` prefix keeps the schema valid for every other validator, which ignore it). Names and a call for the C contract (`cf_def` side: `cf_builder_relation(b, kind, key, other)`, experimental tier) are written when the answer is given; the card's steps follow the answer.
  * Run by the library when the configuration is built (so a program refuses at start-up what the compiler refused at build time), each violation a diagnostic with both keys and the place of the winning value of each.
  * **D-211** ("Relations between keys are a fixed declarative set"), written by the card when it opens.
* **files**: the schema reader and the contract's defset (`lib/config/src/core/`), `tools/gen/build.c`, tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md`.
* **steps**:
  1. Wait for the answer to TO DECIDE D; the opening cold read writes the steps from this card's "decided" on the pattern of Y-25's (the test first, see it fail, the rule, the negative proof).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no answer; the answer is (1) or (3).
* **docs**: when built: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added"; `docs/Decisions.md`: D-211.

---

## Y-31 - Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel and KCL as front ends

* **id**: Y-31
* **goal**: the Guide has a page "A language in front of ConfigForge" showing, for each of CUE, Pkl, Jsonnet, Dhall, Nickel and KCL, the command that exports its result to JSON or YAML and the ConfigForge command that takes it from there (`gen build --source out.json=user-file ...`); the JSON/YAML-producing ones are run by a test when the tool is on the PATH.
* **why**: `docs/research/config-compiler.md` section 4.2 J and section 5.1 item 4: the library has no configuration language and builds none (research 4.2 A); these languages are used in front of it.
* **needs**: Y-24
* **size**: small (documentation)
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
* **why**: `docs/research/config-compiler.md` section 4.2 I and section 5.3 B (**recommended: (3), both, the file later**) and section 6 (`when asked`): the command line comes first (Y-24); a build file is for a repository with many targets.
* **needs**: Y-24, Y-29; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE B (how the build is described).** (1) the command line; (2) a build file read by the library with the tool's own contract; (3) both, the file later. **Recommended: (3)**: Y-24 is (1); this card is the file, built only when asked.
  * **Shape if built**: the tool's own contract, in `tools/gen/build.schema.json` (committed, and used by `gen build --file` to read its own input through `cf_builder_schema_file`): `schema` (path), `runtime` (list), `profile`, `[[source]]` (path, rank), `[[target]]` (name, kind, out, `with_defaults`, `allow_secrets`, `manifest`); one `gen build --file b.toml` builds every target; relative paths are relative to the file. Any format the library reads works for the build file. The steps are written when it is asked for.
* **files**: `tools/gen/build.c`, `tools/gen/build.schema.json`, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-212), `docs/Tools.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps on Y-24's pattern.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "## 1.10.0 — unreleased" or the open version, "### Added"; `docs/Decisions.md`: D-212.

---

## Y-33 - The manifest inside the image (2.0.0, with the image header's layout)

* **id**: Y-33
* **goal**: the image format version 1.1 (`CF_IMAGE_MINOR` 1) carries the manifest of Y-29 as a section, so an artifact says what it was made from without a file beside it.
* **why**: `docs/research/config-compiler.md` section 5.3 E (2) and section 6 (last row, 2.0.0, with the image header): the one place a manifest cannot be lost from its image.
* **needs**: Y-29 and the image header's layout (wave 8, version 2.0.0; the card that fixes it is in `docs/plan/M-maintainer.md`: find it with `grep -n "image header" docs/plan/M-maintainer.md | head`)
* **size**: small
* **decided**:
  * **Shape**: a new section kind in the image (`docs/ImageFormat.md`: the section table), `CF_IMAGE_SECTION_MANIFEST`, holding the manifest's JSON bytes as Y-29 writes them (the image's own digest excludes it, so the output digest in the manifest is of the image without the section: written in `ImageFormat.md`); `CF_IMAGE_MINOR` rises from 0 to 1 (`cf_image.h:42`); a reader of minor 0 ignores a section it does not know (check the reader's rule in `ImageFormat.md`; if it refuses, the card stops: that is a break and 2.0.0's list's). `cf_image_manifest(const cf_image*, const char** json, size_t* n)` reads it (a new public name, experimental tier); `gen build --manifest-in-image` writes it.
  * **D-213** ("The manifest may live in the image"): the section, the minor, the digest rule.
* **files**: `lib/config/include/configforge/cf_image.h`, the image reader and writer, `tools/gen/build.c`, `docs/ImageFormat.md`, tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-33 main` (after Y-29 and the image header card).
  2. Tests: (a) an image written with the manifest section reads back its JSON byte for byte; (b) an image of minor 0 (the golden of Y-26) opens unchanged; (c) a reader built to the minor-0 rules opens the minor-1 image (the section is skipped); (d) the output digest in the manifest equals the SHA-256 of the image without the section.
  3. See it fail: build the image test (`cf_image_manifest` undeclared).
  4. Write the section, the minor and the call. `ctest --preset default -R "image|gen_build"`.
  5. See that the test can fail: hash the whole image behind a run-time-false condition; (d) fails; put it back.
  6. D-213, `docs/ImageFormat.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-33: the manifest inside the image (D-213)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "image|gen_build"` exit 0; the minor-0 golden unchanged.
* **stop if**: the reader of minor 0 refuses an unknown section; the image header card has not fixed the section table; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "The image format 1.1 carries a build's manifest; `cf_image_manifest` (Y-33, D-213)." `docs/Decisions.md`: D-213.

---

## Y-34 - `gen check --report`: a predicted, value-blind report from files and a contract (TO DECIDE)

* **id**: Y-34
* **goal**: `configforge gen check --schema app.schema.json [--report out.json] [--unknown warn|reject] <file>...` reads every file as the program would, in order, and with `--report` writes the value-blind report of what the files would configure, marked `"predicted": true`, exiting 0 unless a file cannot be read (2); without `--report` it behaves as today.
* **why**: `docs/research/analysis-tool.md` section 5, "First: Option B" (a predicted report from files, and Doctor on it), with section 4's Option B for the mechanism. It reuses the report and Doctor's rules the project already has, for every format the library reads, with places. The research asks the tool to start by doing what the others do (yaml-language-server, Taplo, conftest); Y-36 gives that, Y-34 is what differs.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **TO DECIDE 1 (where the static mode lives)**, research section 5, TO DECIDE 1. Options: **(a)** `configforge gen check --report`, then `doctor lint` (no new program; Doctor keeps its non-goal; `gen` is already the contract's program); **(b)** a new `configforge check` (one step; a third meaning of "check" beside `codec check` and `gen check`); **(c)** Doctor reads files itself (rejected: it breaks `docs/Doctor.md:853`, "Resolving configuration itself"). **Recommended: (a).** The steps are (a)'s; for (b) the new program is a thin front over the same function and `tools/gen/CMakeLists.txt` gains a second executable name, which is the maintainer's to ask for; (c) is not offered.
  * **Where.** `gen_check` (`tools/gen/contract.c:50-100`; its usage text `contract_usage`, 38-46). Its loop gains `--report <path>` and `--unknown warn|reject` (default `reject`, as today at `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`; with `warn` the call is `CF_UNKNOWN_WARN` (`lib/config/include/configforge/cf_define.h:318-322`), so unknown keys are candidates that CFD005 reports); the usage line gains both. An option given twice, or `--report` without a path, is `contract_usage()` (exit 2).
  * **With `--report`**, after `cf_builder_create`: the configuration is built with a builder that keeps going past a refused value when the unknown mode is `warn` (a refused value is a refused candidate in the report: research section 4, Option B). The report is written with `cf_report` (`lib/config/include/configforge/configforge.h:666`) from a `cf_report_info` of `CF_REPORT_BLIND` whose `profile` is NULL, `application` the schema's title when it has one, and `generated` NULL (a file made from the same inputs is the same bytes). The file is written whole or not at all (`cf_platform_write_file`, F-1a) when `--report` names a path, and to stdout when it is `-`.
  * **The mark.** `cf_report_info` gains a last member `cf_bool predicted;` (lib/config/include/configforge/cf_report.h:45-52; its `size` field makes a new last member an addition, the `CF_REPORT_INFO_INIT` at line 55 gains a `CF_FALSE`, `CF_REPORT_INFO_INIT` is `{sizeof(cf_report_info), CF_REPORT_BLIND, NULL, NULL, NULL, NULL, CF_FALSE}`); `cf_report_write` (`lib/config/src/*/report.c`; find `"profile"` with `grep -n '"profile"' lib/config/src/*/report.c`) writes `"predicted": true` beside it only when the field is true and the info's `size` covers it, else nothing (an older caller's report is byte for byte unchanged). The cf_report format version does not change (an added member).
  * **What exit codes mean.** 0 done (without `--report`: every file holds to the contract, as today; with `--report`: the report was written, whatever the findings are: Doctor judges them); 1 only without `--report` (a file does not hold); 2 unusable input (a file or the schema cannot be read, or the report cannot be written). A file in a program's own codec or repair (D-103) that `gen` cannot load is "cannot read", exit 2, never a pass (research section 4, Option B, "Rules out").
  * **D-214** ("A predicted report is the contract's program's, and Doctor judges it"): Question: how a repository checks its configuration files before a release without running the program. Options: recipes only (Y-36, always); a predicted report from `gen check` then Doctor (decided); a new `configforge check`; Doctor reading files (breaks `docs/Doctor.md:853`). Decision: `gen` resolves, Doctor judges; the report says `"predicted": true`; `--unknown warn` makes unknown keys candidates; exit 0 with `--report` unless a file cannot be read. Rejected: findings about the program's own codecs; a checker that touches the machine it runs on (research section 3, failure mode 6).
* **files**: `tools/gen/contract.c`, `lib/config/include/configforge/cf_report.h`, the report writer (found by the grep above), `tools/gen/tests/` (the gen tool's tests: `ls tools/gen` shows none: put the new cases in `tests/gen_check/` and add `add_test` lines in `tools/gen/CMakeLists.txt` after the existing gen tests: find them with `grep -n add_test tools/gen/CMakeLists.txt`), `tests/gen_check/*` (new: `app.schema.json`, `good.toml`, `bad.toml`, `bad.yaml`), `docs/Tools.md`, `docs/Doctor.md` (a paragraph); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-34 main`.
  2. Make the fixtures: a schema with `server.port` (integer, 1-65535), `log.level` (enum `debug|info|warn`), `db.password` (sensitive string), `old.key` (deprecated); `good.toml` (all four valid), `bad.toml` (`server.prot = 1`, `server.port = 70000`, `log.level = "loud"`, and `old.key = 1`), `bad.yaml` (the same keys in YAML, with `server.port` twice).
  3. Tests (shell-free, through `add_test` with `COMMAND` and `PASS_REGULAR_EXPRESSION` or the gen tool's existing test style): (a) `gen check --schema S good.toml` exits 0 and prints `ok: 1 file(s)`; (b) `gen check --schema S --report r.json --unknown warn bad.toml` exits 0, `r.json` contains `"predicted": true` and `"server.prot"` as a candidate whose place starts `bad.toml:1`; (c) `configforge doctor lint r.json` (after Y-35) is Y-35's test; here a `grep` of `r.json` for CFD-relevant data suffices: `"server.port"` has a refused candidate with `bad.toml:2`; (d) `--report -` writes the JSON to stdout; (e) a file that cannot be read exits 2 and writes no report; (f) two `--report` options exit 2; (g) `bad.yaml`'s duplicate key shows both places; (h) running the same command twice gives byte-identical reports (`cmp`); (i) the report contains no value text: `grep -c "70000" r.json` is 0 (value-blind).
  4. See it fail: `cmake --build --preset default --target gen 2>&1 | tail -n 5 && ctest --preset default -R gen_check 2>&1 | tail -n 20` (`--report` is a usage error today).
  5. Write the option parsing, the `predicted` member and the report writing. `ctest --preset default -R "gen|report|doctor" 2>&1 | tail -n 30` (every existing report test unchanged).
  6. See that the test can fail: write `"predicted": true` always behind a run-time-false condition; (a report of an older caller changes) the existing report goldens fail; put it back.
  7. D-214, docs, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-34: gen check --report, a predicted report from files (D-214)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_check|test_report"` exit 0.
* **stop if**: no answer to TO DECIDE 1; an unknown key under `CF_UNKNOWN_WARN` is not in the report as a candidate (the research asks a card to confirm it: report what `cf_report` writes, the card then adds the candidate to the report writer); a refused value under `warn` aborts `cf_builder_create` (report: the builder needs a mode that keeps going, which is a new public choice); the existing report goldens change; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`configforge gen check --report`: a predicted, value-blind report from files and a contract, for `doctor lint` (Y-34, D-214)." `docs/Decisions.md`: D-214. `docs/Roadmap.md`: the item "An analysis tool" under "From the maintainer, 2026-11-26" stays 🟨 until Y-37 and Y-40-m.

---

## Y-35 - Doctor on a predicted report: the rules a running program is needed for are off and named; SARIF columns and fingerprints

* **id**: Y-35
* **goal**: `configforge doctor lint` on a report marked `"predicted": true` turns off the rules that need a running program, says which in its output, and its SARIF regions carry `startColumn` and each result a `partialFingerprints` entry.
* **why**: `docs/research/analysis-tool.md` section 5 ("Two small changes in Doctor come with it") and section 6, second row. Without it a predicted report makes false findings (CFD018 "set but never read", CFD008's default-only in production) and GitHub loses a finding's identity when lines move (sources [33], [34]).
* **needs**: Y-34
* **size**: small
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
* **decided**:
  * **Where**: `docs/Guide.md`, a new section after "11. The editor, the report and Doctor" (heading at line 1315) as a subsection `### Checking files before they ship`; examples in `examples/recipes/` (new): `app.schema.json` (written once with `configforge gen schema --schema <contract>` and committed), `app.yaml` with the modeline `# yaml-language-server: $schema=app.schema.json`, `app.toml` with `#:schema ./app.schema.json` (Taplo's directive), `.taplo.toml` with `[[rule]] include = ["*.toml"] schema.path = "app.schema.json"`, and `policy.rego` for conftest.
  * **conftest over a report**: the recipe converts a report with `jq` so that every candidate carries `_loc` from its `at` (`jq '.keys |= map(. + {_loc: .at})'`: check the report's real member names first with `configforge gen check --report` from Y-34 on a fixture, or with an existing report in `tests/`), then `conftest test --policy policy.rego report.json`; `policy.rego` holds one rule: `deny[msg] { ... }` "`pool.max` is at least `pool.min`" with `msg` formatted with `_loc`. The Guide says what conftest cannot see (a rule that errors passes: research section 3, failure mode 8: write the recipe's rule with a test input that must be denied).
  * **Tests**: the examples are run by the suite only when the tools are on the PATH: `ctest -R recipes` prints `skipped: yaml-language-server not found` etc. (the schema's own validity is checked with the library: `configforge gen check --schema app.schema.json app.toml app.yaml` exits 0 on the good files and 1 on `bad.toml`).
  * No decision number: documentation. The Guide says: relations between keys are a recipe here (research section 5, TO DECIDE 2 (a)); Y-39-m is the parked card for doing them in the contract.
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

## Y-37 - A secret written in a checked file: CFD022 (TO DECIDE)

* **id**: Y-37
* **goal**: `configforge doctor lint` on a predicted report reports CFD022, an error, for each key the contract marks `CF_SENSITIVE` or that is named like a secret (D-84) when a file given to `gen check` holds a value for it; the value is never printed.
* **why**: `docs/research/analysis-tool.md` section 5, TO DECIDE 3, and section 6 (fifth row). `docs/Doctor.md:430` puts "whether a file is committed to version control" out of CFD007's scope; in the files mode the files are the repository, and a secret in one is the one secret finding that is a fact, not a guess.
* **needs**: Y-35, C-8b (CFD021 is C-8b's, so this rule takes the next number)
* **size**: small
* **decided**:
  * **TO DECIDE 3.** **(a)** a new rule CFD022: a `CF_SENSITIVE` key, or one named like a secret (D-84), has a value in a file given to `gen check`; error; the value never printed (recommended); **(b)** leave it to gitleaks. With (b) this card is closed unbuilt, and the Guide's Y-36 page says "use gitleaks for secrets in files".
  * **The rule** (`tools/doctor/engine.c`, a new `rule_secret_in_file` added to `g_lint_rules` (line 1867) after `rule_sensitive_source`; catalogue entry in `docs/Doctor.md` after CFD021, in the table at line 422): runs only on a predicted report (a report from a running program has CFD007 for its sources); for each key whose definition is sensitive (`sensitive` member of the report's key entry, as `rule_sensitive_source` reads it: copy its test) or whose name `cf_key_looks_secret` (the D-84 check; find its real name with `grep -rn "D-84" lib/*/include | head -3`) says is secret, and whose winning or any candidate comes from a file (an `at` of `path:line` that is not `argv`/`env`), a finding `error`, id `CFD022`, key, place, and the text `'%s' is a secret and holds a value in '%s'; keep secrets out of files that are checked in (give it by the environment, a secrets directory or a credentials source)`. No value, no digest, no length: the report is value-blind already, and the message never quotes the candidate's text (a test greps for it).
  * **Blind-safe**: yes (it reads presence and origin only), so the column in `docs/Doctor.md`'s table says `yes`; default severity `error`; the rule's profile defaults: none.
  * **D-216** ("A secret written in a checked file is a finding of its own"): the bullets above in one paragraph; Rejected: printing the value; a pattern scanner of values (gitleaks' job: research section 2).
* **files**: `tools/doctor/engine.c`, `docs/Doctor.md`, the Doctor tests, `tests/gen_check/` (a fixture `secret.toml` holding `db.password = "hunter2"`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 3 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-37 main` (after Y-35 and C-8b).
  2. Tests: (a) `gen check --schema S --report r.json secret.toml` then `doctor lint r.json` prints CFD022 for `db.password` with the place `secret.toml:1` and exit non-zero (the exit rule of `doctor lint` for an error finding: check with `ctest -R doctor`'s existing cases); (b) the output does not contain `hunter2` (`grep -c`); (c) a key named `api_token` that the contract does not mark sensitive but D-84's name check catches is reported; (d) the same file's secret given by the environment (a report from a running program, not predicted) is not CFD022 (CFD007 may speak); (e) with `--severity CFD022=warning` it is a warning.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule and the catalogue row. `ctest --preset default -R doctor`.
  5. See that the test can fail: report the value in the message behind a run-time-false condition; (b) fails; put it back.
  6. D-216, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-37: CFD022, a secret written in a checked file (D-216)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: no answer to TO DECIDE 3; C-8b has used a different number for its rule than CFD021 (take the next free number and say so in the commit); a report does not carry which candidate came from which source (the rule cannot tell a file from the environment): report it; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD022: a secret written in a file given to `gen check` (Y-37, D-216)." `docs/Decisions.md`: D-216. `docs/Roadmap.md`: "An analysis tool" 🟩 when Y-38 is done or closed.

---

## Y-38 - Defined but never read, over a set of reports: CFD023 (TO DECIDE)

* **id**: Y-38
* **goal**: `configforge doctor drift a.json b.json ...` (the command that reads a set of reports, `docs/Doctor.md` section 7e) reports CFD023, `info`, for a contract key that no report given read.
* **why**: `docs/research/analysis-tool.md` section 4, Option E (E2), section 5 TO DECIDE 4, section 6 (sixth row). CFD018 is "set but never read"; the other half, a definition the program stopped using, is the contract's dead code. One run cannot prove it (failure mode 12); a set of reports can.
* **needs**: C-8a, C-8b (the reports that record reads)
* **size**: small
* **decided**:
  * **TO DECIDE 4.** A rule over a set of reports, `info` (recommended), or not at all. With "not at all" the card is closed unbuilt.
  * **The rule**: in `tools/doctor/engine.c`, over the set `drift` already reads (find the set command with `grep -n "drift" tools/doctor/doctor.c | head -5`): for each key in the contract of the first report (its `definitions`, i.e. keys with a default or required flag) such that no report in the set holds a read of it (the report member C-8a adds, which records reads; read C-8a's "decided" for the member's name and copy it), a finding `info`, id `CFD023`, key, and the text `'%s' is defined but no report read it (%d report(s) given); a run that never reads it cannot prove it is unused: give reports from every way the program runs`. It never fires with fewer than 3 reports (the text then says `needs at least 3 reports`: one note, no findings: one run cannot prove it, failure mode 12). A report that does not track reads (no member) makes the rule decline with a note `report <n> did not track reads`, no findings.
  * **Blind-safe**: yes; default severity `info`.
  * **D-217** ("Defined but never read needs a set of reports"): the bullets above; Rejected: a finding from one report; a count of reads.
* **files**: `tools/doctor/engine.c`, `tools/doctor/doctor.c`, `docs/Doctor.md` (the table after CFD022 and section 7e), the Doctor tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 4 (the default is the recommendation). `git switch -c work/Y-38 main` (after C-8a, C-8b and Y-37).
  2. Tests with four hand-made reports (small JSON files in `tests/doctor/`): (a) key `old.knob` read in none of 3 reports: one CFD023 `info`; (b) read in one of them: no finding; (c) 2 reports: the `needs at least 3 reports` note and no finding; (d) one report without the reads member: the note and no finding; (e) a key read in all: nothing.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule. `ctest --preset default -R doctor`.
  5. See that the test can fail: report with 2 reports behind a run-time-false condition; (c) fails; put it back.
  6. D-217, docs, CHANGELOG, Roadmap. Format; codemap and apiref, last. Commit `Y-38: CFD023, defined but never read, over a set of reports (D-217)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: no answer to TO DECIDE 4; C-8a records no reads in a report; `drift` takes exactly two reports and cannot take a set (report: the command needs its own card); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD023: a definition that no report in a set read (Y-38, D-217)." `docs/Decisions.md`: D-217. `docs/Roadmap.md`: "An analysis tool" 🟩.

---
* **Organiser's answers** (2026-11-26, the maintainer, relayed from a side session: "apply the best recommendations wherever a recommendation will do"): the card's recommendation, adapted to D-220: a number the recommendation fixes as a hard limit is an option whose default is that number.

## Y-39-m - Relations in the contract: `pattern`, `if`/`then`, `dependentRequired` (TO DECIDE 2)

* **id**: Y-39-m
* **goal**: the contract holds relations between keys ("if `tls.enabled` then `tls.cert` is required", "a value matches a pattern") in JSON Schema's words, checked by the library and by every validator that reads the schema, with a bounded C89 regular-expression engine for `pattern`.
* **why**: `docs/research/analysis-tool.md` section 4, Option D (D2) and section 5, TO DECIDE 2 ("recommended only when a user brings a relation the recipe cannot hold"). Section 6 gives it no version: "after 2.3.0, no row yet".
* **needs**: Y-36, and a user's ask (status: parked, waits on TO DECIDE 2 answered (b))
* **size**: large
* **decided**:
  * **TO DECIDE 2 (relations between keys).** **(a)** none in the library: Rego or CUE over the report as a recipe (Y-36) (recommended first: no dependency, no new interface); **(b)** `pattern`, `if`/`then`, `dependentRequired` in the contract, and a C call for the same: a new public interface and a bounded regular-expression engine in C89 (recommended only when a user brings a relation the recipe cannot hold). **Recommended: (a) now; (b) only on a user's ask.** With (a) this card is closed unbuilt.
  * **Shape if (b)**: the contract's JSON Schema reader (`cf_builder_schema_file`; find with `grep -n cf_builder_schema_file docs/Codemap.md`) reads three more keywords; a bounded regular-expression engine of a subset with no backtracking blow-up (a Thompson-style matcher: literals, classes, `.`, `*`, `+`, `?`, `^`, `$`, alternation, groups; no back-references, no look-around; a step budget of 1,000,000 per match, then a refusal `CF_REASON_BUDGET`), in `lib/text/` or a new `lib/regex/` component (D-100: one directory per component; the maintainer chooses); a C call `cf_builder_relation(b, ...)` whose shape is the maintainer's second TO DECIDE at that time. The steps are written when it is asked for.
* **files**: `lib/regex/` or `lib/text/`, the schema reader, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-218), `docs/Limits.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from this card's "decided" (tests first, see them fail, the regex engine with a fuzz target and the budget).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; a fuzz hour on the regex engine with no hang.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-218.

---

## Y-40-m - The language server

* **id**: Y-40-m
* **goal**: a language server driven by the contract: a key's description, default and range on hover, and, from a report, what overrides it in production.
* **why**: `docs/research/analysis-tool.md` section 5, "Then, in this order" 7 (**F**, the language server, last) and section 6 (last row, 2.3.0, large); the Roadmap's "Later, and large" item (`docs/Roadmap.md:946-948`). The research's reading of the field (yaml-language-server, Taplo, Even Better TOML) is that Y-36's recipes cover completion and hover from the schema already; the server adds what a schema cannot: the report's origins.
* **needs**: Y-34, Y-36; and a user's ask (status: parked: large, 2.3.0, waits on the analysis tool and a user)
* **size**: large
* **decided**:
  * **TO DECIDE (whether to build it).** Options: build a server of our own (a C program speaking LSP over stdio, in `tools/lsp/`, with hover from the schema and the report); extend yaml-language-server and Taplo through the schema's `x-` keywords (no server of ours); never. **Recommended: not before Y-36 has been used, and then the second option's cost is measured first**; the maintainer decides. The steps are written when it is asked for.
* **files**: `tools/lsp/` (if built), tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-219), `docs/Tools.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added"; `docs/Decisions.md`: D-219.
