# Y - asked 2026-11-26: reading and writing at the same time, compression, an analysis tool, a configuration compiler, threads and processes

This group carries the maintainer's five ideas of 2026-11-26
(`docs/Roadmap.md`, "From the maintainer, 2026-11-26"). Each was
researched first, in `docs/research/`: `concurrent-access.md`,
`compression.md`, `analysis-tool.md`, `config-compiler.md`,
`threads-and-processes.md`. A card says which section of its document
holds the reasons; it does not repeat them. Each card is read cold against
the tree, by someone who did not write it, before it opens (docs/Plan.md,
"Read again after the layout changes"). Line numbers are those of the tree
at cbff5a9; a card that finds them moved finds the symbol by name, and a
symbol that is gone is a "stop if".

Decisions of this group are numbered from D-186 (each card names its own).
The maintainer's choices are marked **TO DECIDE**, each with its options
and the document's recommendation; every card around one is written so that
the answer is the only thing missing. A card whose TO DECIDE has no answer
stops at that step, with everything before it done and pushed.

The cards of "concurrent access" amend F-1a and F-2a (`docs/plan/F-files.md`,
not landed when this is written). They are cards of their own that need
those two to have landed, so that F-files.md is not edited. A card whose
text finds F-1a or F-2a changed meanwhile says so under its notes.

---

## Y-1 - The Guide's "At the same time": what is guarded and what is not

* **id**: Y-1
* **goal**: `docs/Guide.md` has a subsection "At the same time" that tells a program what is safe among threads, among processes and between a reader and a writer, and what it must handle itself; and D-14's sequel in `docs/Decisions.md` no longer says the live lock is "a count taken from 0 to 1".
* **why**: `docs/research/concurrent-access.md` section 5, point 3 ("Write the promise down now"). The library already gives most of the guarantees (a snapshot is immutable, D-14; F-1a never writes in place) and nobody can find them. Documentation only: straight to `main` (CLAUDE.md, "Habits").
* **needs**: nothing
* **size**: small
* **decided**:
  * **Where.** `docs/Guide.md`, at the end of "14. Threads" (heading at line 1671, one paragraph, 1673-1682): a new `### At the same time`. Plain, short sentences.
  * **The three answers**, each a short paragraph: (1) *Threads in one process*: reads are safe at once, with no option; a reader holds a whole snapshot, old or new, never a mixture (D-14); `cf_live_get` may be called from any thread, every other `cf_live_*` call belongs to one thread at a time, and two threads that both call `cf_live_poll` or `cf_live_reload` are outside the contract and nothing detects it. (2) *Processes writing one file*: a file the library writes is replaced whole or not at all (F-1a, D-127), so a reader sees the old file or the new one; two writers that read, change and replace at once lose one update unless Y-4 (a write re-reads and applies again) and Y-6 (a lock by choice) have landed. (3) *A reader and a half-written file*: the library's own writers never write in place; a program or editor that does can be read half-way; the stable read of Y-7 reads again when the stamp moved, and a reload that fails keeps what is serving.
  * **What the program handles itself** (a list): other programs that write in place; network filesystems (locks and rename may not be honoured; the library does not detect the filesystem); writers that do not take the lock; many keys that must change together across several files (put them in one file, or use the store).
  * **True on the day it lands.** A sentence about Y-4, Y-6 or Y-7 is written only if that card has landed (`git log --oneline --grep "Y-4\|Y-6\|Y-7"`); otherwise the Guide says what the tree does today.
  * **The fix in `docs/Decisions.md`** (lines 2194-2196, in the `cf_live` decision): "taken from 0 to 1" becomes a ticket lock: each taker draws a number and waits for its turn; the one-count lock it replaced livelocked under tcc (`lib/config/src/core/live.c:78-85`). Read `live.c:76-94` first; if the code says otherwise, the code wins.
  * No decision number.
* **files**: `docs/Guide.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch main && git pull`.
  2. Read `lib/config/src/core/live.c:70-100`, `docs/Decisions.md:2185-2200`, `docs/Guide.md:1671-1682`.
  3. Write the subsection and the Decisions fix.
  4. Check every name used: `grep -n "cf_live_get\|cf_live_poll\|cf_live_reload" lib/config/include/configforge/cf_live.h`.
  5. `ctest --preset default -R "gate.doc|gate.links" 2>&1 | tail -n 20` (list the document gates first with `ctest --preset default -N | grep -i "doc\|link"`).
  6. Commit `Guide: At the same time; D-14's sequel names the ticket lock` to `main`; push.
* **accept**: the document gates of step 5 exit 0. ACCEPT-WINDOWS and ACCEPT-CI are not needed for documents.
* **stop if**: section 14 has moved so that "at its end" is unclear; the code of `live.c` contradicts D-14 itself, not only its sequel: write it under notes and stop.
* **docs**: `docs/Guide.md`, `docs/Decisions.md` as above. No CHANGELOG entry. The Roadmap item "concurrent reads and writes by choice" is marked by Y-6.

---

## Y-2 - Windows: the replace retried for a bounded time while another handle holds the target

* **id**: Y-2
* **goal**: on Windows, `cf_platform_write_file` retries its final `MoveFileExW` on `ERROR_ACCESS_DENIED` and `ERROR_SHARING_VIOLATION` every 20 ms for at most 2000 ms, then fails with a message naming the wait; a test holds the target open and sees the write succeed when the handle is closed inside the bound and fail after it.
* **why**: `docs/research/concurrent-access.md` section 5, point 1 ("Amend F-1a before it opens"). On Windows a rename over a file another handle holds fails at once, so the library's own reload on one thread makes a save on another fail. Go's `renameio` retries the same way (source [10]).
* **needs**: F-1a
* **size**: small
* **decided**:
  * **Where.** `lib/platform/src/win32/file.c`, F-1a's step 5 (the `MoveFileExW(temp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` call, found by name). POSIX and none are unchanged.
  * **The loop.** `#define CF_WRITE_RETRY_MS 2000UL` and `#define CF_WRITE_RETRY_STEP_MS 20UL` at the top of the file. When the call fails with `GetLastError()` equal to `ERROR_ACCESS_DENIED` (5) or `ERROR_SHARING_VIOLATION` (32) and `waited < CF_WRITE_RETRY_MS` (`waited` counted by adding the step after each `Sleep(CF_WRITE_RETRY_STEP_MS)`, not read from a clock), sleep and call again. Any other error ends the loop at once. It is still one step for `cf_platform_write_steps()`; an injected failure at step 5 (`fail_at == 5`) returns before the call and does not retry.
  * **Wording** after the wait: `cannot write '%s': still in use by another program after 2000 ms (error <n>)`, `CF_E_SOURCE`, as F-1a's errors. The temporary is deleted as in F-1a.
  * **Fixed 2000 ms** until Y-6 adds `cf_builder_write_wait`; that wait is for the lock only. This retry is part of making one write work (research section 5, TO DECIDE 2). No public name.
  * **Rejected**: `ReplaceFileW` (it fails the same way on a held handle; research section 2, "Windows rename").
  * One sentence is added to D-127's text: "On Windows the final rename is retried for up to 2000 ms while another handle holds the target."
* **files**: `lib/platform/src/win32/file.c`, `lib/config/tests/test_writefile.c`, `docs/Decisions.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-2 main` (after F-1a has landed).
  2. In `test_writefile.c`, under `#if CF_PLATFORM_WIN32`, case (h) with `cf_wf.txt` holding `old` and opened by `cf_platform_fopen(path, "rb")` (no delete sharing): (h1) a thread from `CreateThread` sleeps 300 ms and `fclose`s the handle; `cf_platform_write_file(..., "new", 3, &err)` returns 1 and the file reads `new`; (h2) with the handle kept open the write returns 0 after at least 1900 ms and at most 4000 ms, `err.message` contains `still in use by another program after 2000 ms`, the file reads `old`, no `.cf_wf.txt.cf-tmp-0` remains; then `fclose`.
  3. See it fail: `cmake --build --preset default --target test_writefile && ctest --preset default -R test_writefile --output-on-failure 2>&1 | tail -n 30` on Windows: (h1) fails at once with `error 5` or `error 32`.
  4. Add the loop. `ctest --preset default -R test_writefile`.
  5. See that the test can fail: `if (fail_at == 0xFFFFu) waited = CF_WRITE_RETRY_MS;` before the loop (run-time-false, never a constant: MSVC negative-proof rule); (h1) fails; put it back.
  6. D-127's sentence; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake` and `cmake -P scripts/generate/apiref.cmake`, last. Commit `Y-2: the Windows replace waits for a handle that holds the target`.
  8. ACCEPT; push `work/Y-2`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX (case (h) is compiled out); ACCEPT-WINDOWS with `ctest --preset default -R test_writefile` exit 0 and case (h) run; ACCEPT-CI.
* **stop if**: F-1a is not landed or its Windows step 5 is not a single `MoveFileExW`; `gate.layering` refuses `CreateThread` in a test (use `Sleep` in a second process via `system` only if it fits in one line, else stop); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Fixed": "On Windows a file written by `cf_platform_write_file` waits up to 2 seconds for another handle to let go of the target (Y-2)." `docs/Decisions.md`: D-127's sentence. Roadmap: Y-6 marks the item.

---

## Y-3 - cf_platform_write_file_if: the replace refused when the file no longer holds the bytes read

* **id**: Y-3
* **goal**: `cf_platform_write_file_if(path, data, size, expect, expect_size, err)` does what `cf_platform_write_file` does but, just before the replace, reads the file again and, when it no longer holds exactly `expect` (with `expect == NULL`: when it now exists), writes nothing and returns 0 with `CF_E_SOURCE` and `'%s' changed while it was written`.
* **why**: `docs/research/concurrent-access.md` section 4 (P1) and section 5, point 2: the internal piece of the optimistic check (Y-4). `CF_E_BUSY` is Y-6's; this card does not need it.
* **needs**: F-1a
* **size**: small
* **decided**:
  * **Name and place.** In `lib/platform/include/configforge/cf_platform_file.h` under `#if CF_INTERNAL_API`, after F-1a's names, and one line in `cmake/configforge.internal.symbols` after F-1a's:
    ```c
    /* cf_platform_write_file, but first the file is read again: when it no longer
       holds exactly `expect` (NULL: it must not exist), nothing is written and the
       result is 0 with CF_E_SOURCE, "'<path>' changed while it was written". */
    CF_API int cf_platform_write_file_if(const char* path, const void* data, size_t size,
                                         const void* expect, size_t expect_size, cf_error* err);
    ```
  * **How.** In each platform's `file.c`, F-1a's body becomes `static int write_checked(path, data, size, fail_at, have_expect, expect, expect_size, err)`; `cf_platform_write_file_at` calls it with `have_expect == 0`, `cf_platform_write_file_if` with 1. The check is made after the temporary is closed (POSIX step 5, Windows step 4) and immediately before the rename step: read `path` with `cf_platform_fopen` and `fread` into a buffer of `expect_size + 1` bytes (more than `expect_size` bytes is already different); compare with `memcmp`. A missing file holds nothing (equal to `expect == NULL`). On a difference the temporary is removed and 0 returned. It is not a numbered step: `cf_platform_write_steps()` is unchanged.
  * **None**: returns 0 with `CF_E_CAPABILITY`, `this build has no files to write`.
  * The window between check and rename stays (microseconds); Y-6's lock closes it.
  * **Rejected**: comparing stamps (a rewrite in the same second with the same size would pass; research section 4, P1).
* **files**: `lib/platform/include/configforge/cf_platform_file.h`, `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/none/file.c`, `cmake/configforge.internal.symbols`, `lib/config/tests/test_writefile.c`; last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-3 main` (after F-1a has landed).
  2. In `test_writefile.c` add: (i) file `old`, `expect "old"`, writes `new`: returns 1, reads `new`; (j) `expect "ol"` (2 bytes): returns 0, message contains `changed while it was written`, file reads `old`, no temporary; (k) `expect "old"`, file holds `older`: returns 0; (l) no file, `expect NULL, 0`: returns 1; (m) file exists, `expect NULL, 0`: returns 0; (n) 3 MiB of `expect` equal to the file: returns 1.
  3. See it fail: `cmake --build --preset default --target test_writefile 2>&1 | tail -n 20` (undeclared name).
  4. Add the declaration, the three bodies, the symbols line. `ctest --preset default -R "test_writefile|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: skip the comparison behind a run-time-false condition (`if (fail_at == 0xFFFFu)`); (j) fails; put it back.
  6. CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-3: cf_platform_write_file_if, the replace refused when the file changed`.
  7. ACCEPT; push `work/Y-3`; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_writefile` exit 0 in `default` and `minimal`.
* **stop if**: F-1a's body has no point between closing the temporary and the rename; `gate.symbols` wants more than one line; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_platform_write_file_if` (internal tier): a file replaced only if it still holds the bytes the change was made on (Y-3)." No D-number (Y-4 holds the decision). Roadmap: Y-6.

---

## Y-4 - overrides_write re-reads and re-applies before it replaces

* **id**: Y-4
* **goal**: a write through `cf_builder_overrides` that finds the file changed between its read and its replace applies its items again to the new bytes, up to 3 times, then refuses with `CF_E_SOURCE`, `'<path>' changed while it was written; try again`; two programs that set different keys both keep their key.
* **why**: `docs/research/concurrent-access.md` section 5, point 2 (P1, always on). It replaces D-128's "lose one update" with "lose one only in a window of microseconds", and needs no file, no option and no new public name, so it does not touch D-128's objection to a lock file.
* **needs**: Y-3, F-2a
* **size**: small
* **decided**:
  * **Where.** `overrides_write` in `lib/config/src/bridge/file.c` (F-2a, found by name). F-2a's steps (2) to (6) become a loop of at most 3 turns: the bytes read in step 2 are kept (a missing file: `NULL, 0`); step 6 calls `cf_platform_write_file_if(path, buf, n, expect, expect_size, err)`. Result 1: done. Result 0 with `strstr(err->message, "changed while it was written") != NULL`: `cf_error_clear(err)` and turn again from step 2 on the new bytes (new arena, fresh `cf_keyspace`, every item applied again). Any other failure is returned at once. After the third change: `cf_error_set(err, CF_E_SOURCE, NULL, path, "'%s' changed while it was written; try again", path)`, return `CF_FALSE`.
  * **Change wins key by key**: items apply to the newest bytes; a key another program set stays unless an item sets it; an item's key always wins over the other writer's value for that key.
  * **A file the reader refuses** on a later turn is F-2a's refusal: `CF_E_SOURCE`, nothing written.
  * **A test seam without global state** (D-101): `cfi_overrides_write_hook(cf_source* s, void (*between)(void* ctx), void* ctx)` in `file.c`, declared under `#if CF_INTERNAL_API` in `cf_source.h` after `cf_source_file_overrides`, one line in `cmake/configforge.internal.symbols`; it sets two fields of the source's state, `between` being called once a turn after the read and before the replace.
  * **D-186** (after D-185; "An override write checks that the file is still the one it read"): Question: two programs write one override file at once; read, change, replace loses one update (D-128). Options: a lock file always (rejected: a file left in the directory unasked; research section 4, P2 and P3); no check (D-128 as drafted); compare and apply again (decided). Decision: `overrides_write` compares the bytes it read with the file just before the replace and, on a difference, applies its items to the new bytes, 3 times at most, then refuses with `'%s' changed while it was written; try again`; a window of microseconds between the check and the rename remains, which only a lock (Y-6, the program's choice) closes. Rejected: stamps instead of bytes; an unbounded retry; letting another writer's value beat an item's.
* **files**: `lib/config/src/bridge/file.c`, `lib/config/include/configforge/cf_source.h`, `cmake/configforge.internal.symbols`, `lib/config/tests/test_overrides.c`, `docs/Decisions.md`; last: `CHANGELOG.md`, `docs/Guide.md` (Y-1's subsection, one sentence), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-4 main` (after F-2a and Y-3 have landed).
  2. In `test_overrides.c` add: (l) the hook rewrites `cf_ov.json` with `{"a":1}` on the first turn only, while the write sets `b`: the file ends holding `a` and `b`; (m) the hook rewrites every turn: the write returns `CF_FALSE` with `changed while it was written; try again` after exactly 3 turns (the hook counts) and the file holds the hook's last bytes; (n) the hook sets the key the write sets, to another value: the write's value wins; (o) the hook writes `{ bad` on turn 2: `CF_E_SOURCE` with `cf_ov.json:1`, the file as the hook left it.
  3. See it fail: `cmake --build --preset default --target test_overrides 2>&1 | tail -n 20` (hook undeclared). Add the hook alone (no loop): (l) fails, `a` lost.
  4. Write the loop. `ctest --preset default -R "test_overrides|test_write|test_writefile"`.
  5. See that the test can fail: run the loop once (`if (fail_at == 0xFFFFu)`-style run-time-false guard on the turn count); (l) fails; put it back.
  6. D-186 into `docs/Decisions.md`; the Guide sentence; the CHANGELOG entry.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-4: an override write re-reads and applies again before it replaces (D-186)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_overrides` and `ctest --preset minimal -R test_overrides` exit 0.
* **stop if**: F-2a or Y-3 is not landed; F-2a's `overrides_write` keeps no copy of the bytes it read; the loop changes a result of F-2a's cases (a) to (k); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Changed": "An override file written by two programs at once keeps both programs' keys: a write re-reads the file before it replaces it and applies its change again, 3 times at most (Y-4, D-186)." `docs/Decisions.md`: D-186. Roadmap: Y-6.

---

## Y-5 - cf_platform_lock_take and _give: a lock on a kept sidecar file, on every platform (TO DECIDE)

* **id**: Y-5
* **goal**: `cf_platform_lock_take(l, path, wait_ms, err)` takes an exclusive lock held on a sidecar file `<dir>/.<base>.cf-lock` beside `path`, waiting at most `wait_ms`, and `cf_platform_lock_give(l)` lets it go; a test shows that two takes of the same path in one process exclude each other, on POSIX and on Windows, and that a take whose holder is gone succeeds at once.
* **why**: `docs/research/concurrent-access.md` section 4 (P2, "What P2 needs") and section 5, "After: the lock by choice", point 4. A kernel lock on a kept file is released by a crash, which D-128's objection to "a lock file" (a lock by existence) does not cover.
* **needs**: F-1a, and TO DECIDE 1 below answered
* **size**: medium
* **decided**:
  * **TO DECIDE 1 (reopen D-128's "no lock file")**, research section 5, TO DECIDE 1. Options: **A** a sidecar lock the program asks for (`CF_FILE_LOCK`, Y-6), default off; **B** the same, default on, a flag turns it off; **C** no lock, Y-4's optimistic check only; **D** D-128 as drafted (neither). **Recommended: A, with Y-4 always on.** It is what the maintainer's "it could depend on an option" describes, it leaves no file in a directory unless asked, and Y-4 alone already removes almost all lost updates. With C or D this card and Y-6 are closed unbuilt, and Y-4's text stands. The steps below are A's; for B only Y-6's default changes.
  * **Names** (`lib/platform/include/configforge/cf_platform_file.h`, under `#if CF_INTERNAL_API`, after Y-3's name; the three lines of `cmake/configforge.internal.symbols` after Y-3's):
    ```c
    typedef struct cf_platform_lock { long handle; } cf_platform_lock;
    /* 1 with the lock held on <dir>/.<base>.cf-lock; 0 with err set: CF_E_SOURCE, "'<path>' is locked by another writer after <wait_ms> ms"
       when the wait ran out (Y-6 changes the code to CF_E_BUSY), CF_E_SOURCE when the lock file cannot be opened. */
    CF_API int  cf_platform_lock_take(cf_platform_lock* l, const char* path, unsigned long wait_ms, cf_error* err);
    CF_API void cf_platform_lock_give(cf_platform_lock* l);
    ```
    `l->handle` is -1 when not held. `cf_platform_lock_give` on a lock not held does nothing.
  * **POSIX** (`posix/file.c`; `<sys/file.h>` added): the sidecar is opened `O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC` (each only where defined), after `lstat` refuses a symbolic link as F-1a does (`'%s' is a symbolic link`), mode the configuration file's own `st_mode & 0666` when it exists, else `0600`. **One protocol per build**, chosen at compile time and never two: `#if defined(LOCK_EX)` `flock(fd, LOCK_EX | LOCK_NB)` (Linux, the BSDs, macOS, musl); `#elif defined(F_OFD_SETLK)` an OFD lock `F_OFD_SETLK` with `struct flock` over the whole file (`l_len = 0`); `#else` classic `F_SETLK`, whose two faults (it dies when any descriptor on the file is closed by the process; threads share it) are written in a comment and in the Guide by Y-6. A refusal with `EWOULDBLOCK`, `EAGAIN` or `EACCES` is "held"; any other errno is `CF_E_SOURCE`, `cannot lock '%s': <strerror>`.
  * **Windows** (`win32/file.c`): `CreateFileW(sidecar, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL)`; `LockFileEx(h, LOCKFILE_EXCLUSIVE_LOCK | LOCKFILE_FAIL_IMMEDIATELY, 0, 1, 0, &ov)` on one byte; `ERROR_LOCK_VIOLATION` (33) is "held".
  * **The wait** is a loop of non-blocking attempts with sleeps that start at 1 ms and double to a ceiling of 50 ms (`poll(NULL, 0, ms)` on POSIX as `lib/platform/src/posix/posix.c:938` does, `Sleep(ms)` on Windows), the sum of the sleeps counted, not read from a clock; `wait_ms == 0` makes one attempt. Never longer than `wait_ms` (the last sleep is cut to what is left). No wait is unbounded (research section 5, "Not to build").
  * **None**: returns 0 with `CF_E_CAPABILITY`, `this build has no files to lock`.
  * **The sidecar is never deleted** (deleting it brings back the race of research section 3, case 2) and never read; its content is empty. `cf_platform_lock_give` closes the descriptor or handle (which releases the lock).
  * **D-187** is Y-6's text. This card adds none.
* **files**: `lib/platform/include/configforge/cf_platform_file.h`, `lib/platform/src/posix/file.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/none/file.c`, `cmake/configforge.internal.symbols`, `lib/config/tests/test_lock.c` (new), `lib/config/tests/CMakeLists.txt` (add `test_lock` after `test_writefile`); last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1. `git switch -c work/Y-5 main` (after F-1a, Y-3 landed).
  2. Write `test_lock.c`, C89, after `test_writefile.c`'s way (`"cf_test.h"`): (a) `take` on `cf_lk.txt` returns 1 and `.cf_lk.txt.cf-lock` exists; (b) a second `take` of the same path (a second `cf_platform_lock` in the same process, so a second open file description) with `wait_ms` 0 returns 0 at once and the message contains `locked by another writer`; (c) with `wait_ms` 200 it returns 0 after at least 150 ms and at most 1000 ms; (d) after `give` of the first, a take returns 1; (e) a take of a path in a directory that does not exist returns 0 with `cannot lock`; (f) POSIX only: a symbolic link named as the sidecar is refused; (g) `give` twice is harmless; (h) the sidecar still exists after `give`. A case with a second process (`system` of a helper) is not written: two open file descriptions in one process exclude each other with `flock` and OFD locks; with classic `F_SETLK` they do not, so (b) to (d) are `#if` out there and the test prints `classic fcntl: same-process exclusion not tested`.
  3. See it fail: `cmake --build --preset default --target test_lock 2>&1 | tail -n 20` (undeclared).
  4. Add declarations, bodies, symbols. `ctest --preset default -R "test_lock|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: make `take` skip the lock call behind a run-time-false condition (`wait_ms == 0xFFFFFFFFUL`); (b) fails; put it back.
  6. CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-5: cf_platform_lock_take and _give, a kernel lock on a kept sidecar file`.
  7. ACCEPT; push; ACCEPT-CI (macOS, FreeBSD and musl are the other protocols' proof).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_lock` exit 0 in `default` and `minimal`.
* **stop if**: no answer to TO DECIDE 1; a platform in CI has neither `flock` nor OFD locks and its classic-`fcntl` case fails for a reason other than (b) to (d): report it; `gate.layering` refuses the includes; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_platform_lock_take` and `_give` (internal tier): an exclusive lock on a sidecar file, `flock`, an OFD lock or `LockFileEx` (Y-5)." No D-number here (D-187 is Y-6's). Roadmap: Y-6.

---

## Y-6 - CF_FILE_LOCK, cf_builder_write_wait, CF_E_BUSY: a write that waits for another writer (TO DECIDE)

* **id**: Y-6
* **goal**: a program that passes `CF_FILE_LOCK` to `cf_builder_overrides` has every `cf_write` to that file take the sidecar lock around read, change and replace; `cf_builder_write_wait(b, ms)` sets how long a write waits for the lock (and the store for its own lock); a write that waited in vain returns `CF_E_BUSY`.
* **why**: `docs/research/concurrent-access.md` section 5, points 4 and 5 and "The public interface, sketched". The lock closes the microsecond window Y-4 leaves, for every writer that cooperates (several copies of one service, an installer and the program, `configforge codec set`). The store already has locks and lacks a wait (research section 4, "The store").
* **needs**: Y-5, Y-4, F-2a, and TO DECIDE 2 and 3 answered
* **size**: medium
* **decided**:
  * **TO DECIDE 2 (where the wait is set and its default)**, research section 5, TO DECIDE 2. Options: **A** `cf_builder_write_wait(b, ms)` for the whole configuration, default 0; **B** the same, default a short bound (git: 100 ms and 1000 ms); **C** a wait argument on `cf_write` (a breaking change of a public signature, so 2.0.0). **Recommended: A**: 0 keeps today's store behaviour (refused at once) so nothing changes for a program that asks nothing. The Windows retry of Y-2 is not this wait.
  * **TO DECIDE 3 (how "busy" is reported)**, research section 5, TO DECIDE 3. Options: **A** a new code `CF_E_BUSY`; **B** `CF_E_SOURCE` with a message only; **C** `CF_E_SOURCE` with a new reason (reasons are document refusals, D-107). **Recommended: A**: the program acts on it differently, and a message is not something to test against; it adds a value to a public enum, an addition inside 1.x. The steps below are A's and the default-0 wait of 2A; for 2B the default constant changes; for 3B or 3C the code changes at each place named below.
  * **The names**:
    ```c
    /* cf_source.h, in the enum of source flags after CF_ANY_WRITER (1u << 18): */
    CF_FILE_LOCK = 1u << 19, /* a writer of this file takes .<name>.cf-lock beside it, kept, never removed */
    /* configforge.h after cf_builder_writable (line 272): */
    CF_API cf_bool cf_builder_write_wait(cf_builder* b, unsigned long wait_ms);
    ```
    `cf_code.def` after `CF_E_POLICY` (line 33): `CF_CODE_X(CF_E_BUSY, "busy", "another writer holds it; try again")`. `CF_FILE_LOCK` is accepted only by `cf_builder_overrides` (F-2a's flag check refuses any bit it does not know, by name: add this one to its allowed set, not to `CF_FILE_SOURCE_FLAGS`, so `cf_builder_file` still refuses it: `'%s': CF_FILE_LOCK belongs to an overrides file`).
  * **The wait reaches the write** without a signature change: `cf_builder_write_wait` stores `b->write_wait_ms` (the builder struct, `lib/config/src/core/config.c:48`; default 0) and `cf_builder_create` passes it to each source that has a `write` through an internal `cf_source_set_wait(cf_source* s, unsigned long ms)` (declared in `cf_source.h` under `#if CF_INTERNAL_API`, one line in the internal symbols file); the file source's and the store's state each keep `wait_ms`. A derived configuration's stand-in (`derive_ghost`, `config.c:1580-1608`) copies it with `vt->write` and the path.
  * **The lock in `overrides_write`** (`lib/config/src/bridge/file.c`): when `st->flags` has `CF_FILE_LOCK`, `cf_platform_lock_take(&lk, path, st->wait_ms, err)` before F-2a's step 2 and `cf_platform_lock_give(&lk)` after the loop ends, on every path out. A failed take that was "held": the error code is changed to `CF_E_BUSY` with message `'%s' is being written by another program; waited %lu ms` (the platform message is replaced; a take that failed for any other reason keeps `CF_E_SOURCE`). The lock and Y-4's check are both on: the check stays even under the lock (a writer that does not take the lock).
  * **The store** (`lib/config/src/bridge/store.c`, the write at line 228): `sqlite3_busy_timeout(db, (int)st->wait_ms)` right after `sqlite3_open_v2`; a `SQLITE_BUSY` from `BEGIN IMMEDIATE` is `CF_E_BUSY` with `store '%s' is busy; waited %lu ms`. With `wait_ms` 0 SQLite's busy timeout is 0: refused at once, as today (the code changes from the store's `store_failed` code to `CF_E_BUSY` only for `SQLITE_BUSY`; check `store_failed` and keep its other codes).
  * **Contract**: `CF_E_BUSY` is added to the language bindings' tables where `CF_E_POLICY` is listed (`grep -rn "CF_E_POLICY" bindings docs include --include=* | head`; each place named by that grep gets one line); the C++, Rust and Python wrappers expose it as their error kind for "busy". If a gate demands more of the bindings, stop and say which.
  * **D-187** (after D-186; "A lock on a write is the program's choice, and a write that cannot get it says busy"): Question: two programs write one override file or store at once and the program wants the last microsecond closed (D-128 refused a lock file because "a crash leaves it behind"). Decision: that is true of a lock by existence, not of a kernel lock on a kept sidecar, which a crash releases; the sidecar lock is taken only when the program passes `CF_FILE_LOCK` (default off, so no file appears unasked); a write waits at most `cf_builder_write_wait` ms (default 0: refused at once) and then fails with `CF_E_BUSY`; the sidecar is never removed; the lock is on writes only, never on the read path or inside `cf_config` (D-14); Y-4's re-read stays on under the lock. Rejected: a lock by existence; locking the configuration file itself (Windows readers; the rename); a writer service; an unbounded wait; a lock on the read path; promises on network filesystems.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/include/configforge/configforge.h`, `lib/base/include/configforge/cf_code.def`, `lib/config/src/core/config.c`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/store.c`, `cmake/configforge.internal.symbols`, `lib/config/tests/test_overrides.c`, `lib/config/tests/test_store.c` (or the store's test file: `ls lib/config/tests | grep store`), the binding tables found by the grep above; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (Y-1's subsection: the lock, the wait, the busy code, the classic-`fcntl` faults), `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 2 and 3 (and Y-5's answer to 1 is the premise). `git switch -c work/Y-6 main` (after Y-4 and Y-5 landed).
  2. In `test_overrides.c` add: (p) `CF_FILE_LOCK` with a second `cf_platform_lock` already held on `cf_ov.json`: `cf_write` returns `CF_FALSE` with `CF_E_BUSY` at once and the file is as before; (q) the same with `cf_builder_write_wait(b, 200)`: `CF_E_BUSY` after at least 150 ms; (r) the holder gives the lock back from the test's `between` hook of Y-4 (so the hook doubles as a clock): the write succeeds; (s) no `CF_FILE_LOCK`: no `.cf_ov.json.cf-lock` is created; (t) `cf_builder_file(..., CF_FILE_LOCK)` fails `cf_builder_create` with `belongs to an overrides file`; (u) after the write `.cf_ov.json.cf-lock` still exists. In the store's test: a second connection holding `BEGIN IMMEDIATE`, `cf_write` to the store returns `CF_E_BUSY` with wait 0 and succeeds after the holder commits when the wait is long enough (a hook is not available: use a second `sqlite3` handle on the same file and commit before the call for the success case, and hold it for the failure case; do not time two threads).
  3. See it fail: `cmake --build --preset default --target test_overrides 2>&1 | tail -n 20` (undeclared names).
  4. Add `CF_E_BUSY`, the flag, `cf_builder_write_wait`, `cf_source_set_wait`, the lock and the store wait. `ctest --preset default -R "test_overrides|test_store|test_write|test_code|gate" 2>&1 | tail -n 30` (`test_code` or the test that counts `cf_code.def`'s entries: its count rises by one).
  5. See that the test can fail: skip the take behind a run-time-false condition (`st->wait_ms == 0xFFFFFFFFUL`); (p) fails; put it back.
  6. D-187, the Guide, the CHANGELOG, the Roadmap row (🟩, the heading of the item "Concurrent reads and writes, by choice" under "From the maintainer, 2026-11-26" in `docs/Roadmap.md`).
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-6: CF_FILE_LOCK, cf_builder_write_wait and CF_E_BUSY (D-187)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_overrides|test_store"` exit 0; `ctest --preset minimal` exit 0 (the store is absent there: its test says so).
* **stop if**: no answer to TO DECIDE 2 or 3; Y-4 or Y-5 is not landed; a gate wants the bindings wrapped beyond their error tables; `cf_code.def`'s count is checked by a file this card does not name (say which); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`CF_FILE_LOCK`, `cf_builder_write_wait` and `CF_E_BUSY`: a write to an override file or a store can wait for another writer, and says busy when it cannot (Y-6, D-187)." `docs/Decisions.md`: D-187. `docs/Roadmap.md`: "Concurrent reads and writes, by choice" 🟩.

---

## Y-7 - A stable read: the stamp before and after a file is read

* **id**: Y-7
* **goal**: a file source reads the file's stamp, reads the file, reads the stamp again, and when the two differ reads again, up to 3 reads; when the third differs the source fails as an unreadable file does (`CF_E_SOURCE`, `'%s' kept changing while it was read`), which a reload already survives.
* **why**: `docs/research/concurrent-access.md` section 4 ("A reader and a half-written file", R2) and section 5, point 6. It catches most in-place writers, which no lock can.
* **needs**: nothing
* **size**: small
* **decided**:
  * **Where.** `file_load` in `lib/config/src/bridge/file.c` (372-460), the branch that reads a path: `cf_blob_file(&blob, bytes, st->path, &st->err)` at line 421. Only that branch: environment variables and documents in memory have no file to stamp. The stamp is `cf_platform_stamp(path, &st0)` (`lib/platform/include/configforge/cf_platform.h:255`, `cf_stamp` at 245; false when it cannot be stamped, which is not an error): taken before and after `cf_blob_file`; "equal" is a byte comparison of the two `cf_stamp` structs with `memcmp` (check first that the struct has no padding that is left uninitialised: `memset` both to 0 before the call). When either call says false, the read stands as it did (no loop).
  * **The loop**: up to 3 reads (`#define FILE_STABLE_READS 3`); each turn frees the bytes (`cf_blob_arena_done(bytes, ar)` and a new `cf_blob_arena(ar)`, as the function's own error path does) before reading again. No sleep between reads (a retry that sleeps is the program's `settle`, R4, which exists). After the third unequal pair: `cf_error_set(&st->err, CF_E_SOURCE, NULL, st->path, "'%s' kept changing while it was read", st->path)` and return `CF_FALSE`; a `CF_OPTIONAL` source does not skip it (the file is there).
  * **Cost**: one `stat` more per file read (`cf_platform_stamp`), written in the card's CHANGELOG line. The stamp the source keeps for `cf_live` (the one after the read) is the second one, so a change during the read is seen at the next poll.
  * **Test without a clock or a thread** (D-101: no global state): `cfi_file_read_hook(cf_source* s, void (*between)(void* ctx), void* ctx)` in `file.c`, declared under `#if CF_INTERNAL_API` in `cf_source.h` after `cf_source_file`, one line in `cmake/configforge.internal.symbols`; `between` is called once per read, after the read and before the second stamp.
  * **D-188** (after D-187; "A file is read twice when it changes while it is read"): Question: a program or editor writes a file in place while the library reads it (the half-written read; F-1a makes the library's own writers safe, not others). Options: a shared lock on every read (rejected: others do not take it; research section 4, R3); settle (exists, bursts only); a stable read (decided). Decision: the stamp before and after the read, up to 3 reads, then the source fails as an unreadable file; no lock, no sleep. Rejected: a lock on the read path (D-14); a longer retry.
* **files**: `lib/config/src/bridge/file.c`, `lib/config/include/configforge/cf_source.h`, `cmake/configforge.internal.symbols`, `lib/config/tests/test_file.c`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (Y-1's subsection, one sentence), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-7 main`.
  2. In `test_file.c` (the file source's tests; find it with `ls lib/config/tests | grep test_file`) add: (a) the hook rewrites the file (a different size) on the first read only: the configuration holds the second content; (b) the hook rewrites on every read: `cf_builder_create` fails with `kept changing while it was read` after exactly 3 reads (the hook counts) and with `CF_OPTIONAL` the same; (c) the hook does nothing: one read (the hook counts 1); (d) an environment-variable source and a document source never call the hook.
  3. See it fail: `cmake --build --preset default --target test_file 2>&1 | tail -n 20` (hook undeclared); add the hook alone: (a) fails.
  4. Write the loop. `ctest --preset default -R "test_file|test_live|test_config" 2>&1 | tail -n 30` (live and reload tests must not change).
  5. See that the test can fail: loop once (run-time-false guard on the count); (a) fails; put it back.
  6. D-188, the Guide, the CHANGELOG.
  7. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-7: a file is read again when it changes while it is read (D-188)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_file` exit 0; `bench` is not gated here (one stat in a path that reads a file).
* **stop if**: `cf_platform_stamp` is not exact on a platform in CI (a case of the suite that rewrites within one tick and expects a difference fails): report which; the loop changes a result of `test_live`; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Changed": "A file source reads the file again, up to 3 times, when its stamp moved while it was read; one more `stat` a read (Y-7, D-188)." `docs/Decisions.md`: D-188. Roadmap: Y-6 marked it; no change here.

---

## Y-8 - Windows readers that share delete access, if a measurement shows it helps

* **id**: Y-8
* **goal**: first a measurement, written down: on Windows, a rename over a file that a reader holds open with `FILE_SHARE_DELETE` succeeds or it does not. If it succeeds, the file source opens its files with that sharing so that a reader never blocks a writer's rename; if it does not, the card closes with the measurement and no code change.
* **why**: `docs/research/concurrent-access.md` section 4 (R5) and section 6: "dropped if the measurement says no". Y-2's retry covers a reader that closes soon; this would cover one that is slow.
* **needs**: F-1a, Y-2
* **size**: small
* **decided**:
  * **The measurement** (step 2), a small C program under `bench/` is not needed; a test case does it. In `lib/config/tests/test_writefile.c`, under `#if CF_PLATFORM_WIN32`, case (p): open `cf_wf.txt` with `CreateFileW(..., GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, ..., OPEN_EXISTING, ...)`, call `cf_platform_write_file(..., "new", 3, &err)`: record whether it returns 1 within 100 ms (the case prints `share-delete: rename over an open reader: OK|BLOCKED` to stdout and asserts nothing about that outcome; it asserts that, when OK, the file reads `new` and the reader's handle still reads `old`).
  * **If OK**: `win32/file.c`'s `cf_platform_fopen` (line 34) for read modes (`"r"`, `"rb"`) opens with `CreateFileW` and `FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE`, then `_open_osfhandle` and `_fdopen` to a C stream, as a file opened by `_wfopen` would be; write modes unchanged. No new public name. D-188's text gains one sentence, "On Windows files are read with delete sharing so that a reader does not block a rename."
  * **If BLOCKED**: nothing changes; the case stays as a recorded fact (its output line is in the CHANGELOG-less commit message), and Y-2 is the answer.
  * **Not both**: the card never opens files for write with delete sharing.
* **files**: `lib/config/tests/test_writefile.c`, `lib/platform/src/win32/file.c` (only if OK), `docs/Decisions.md` (only if OK), `CHANGELOG.md` (only if OK).
* **steps**:
  1. `git switch -c work/Y-8 main` (after Y-2 landed).
  2. Add case (p). Run on Windows: `cmake --build --preset default --target test_writefile && ctest --preset default -R test_writefile --output-on-failure 2>&1 | tail -n 30`; read the `share-delete:` line.
  3. If BLOCKED: commit only case (p) with the message `Y-8: a rename over a reader that shares delete is BLOCKED on Windows; no change` and stop (card closed). If OK: continue.
  4. See it fail: add case (q): a `cf_platform_fopen(path, "rb")` reader held open, then `cf_platform_write_file` with the Y-2 wait set to its fixed 2000 ms: it must succeed in under 500 ms; it fails today (it waits 2000 ms and then refuses).
  5. Change `cf_platform_fopen` for read modes. `ctest --preset default -R "test_writefile|test_file" 2>&1 | tail -n 30`.
  6. See that the test can fail: leave the old open behind a run-time-false condition; (q) fails; put it back.
  7. D-188's sentence; the CHANGELOG entry. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-8: Windows reads share delete access so that a reader does not block a rename`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-WINDOWS with `ctest --preset default -R test_writefile` exit 0 and the `share-delete:` line recorded; ACCEPT-LINUX (compiled out); ACCEPT-CI.
* **stop if**: Y-2 is not landed; the measurement cannot be made (no Windows machine reachable: say so, leave the card open); `_fdopen` on the handle changes how `cf_blob_file` reads (a `test_file` case fails); three failures of the same step.
* **docs**: only if OK: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Changed": "On Windows files are read with delete sharing, so a reader does not block a writer's rename (Y-8, D-188)." The "later" item of the research (a guard on `cf_live`'s driving calls) is not a card: it goes to "Threads and processes made easier" (cards Y-36 to Y-42 below), the research's own decision.

---

## Y-9 - Compressed input by the program's own zlib: the D-103 recipe, a Guide section and an example

* **id**: Y-9
* **goal**: a program that links its own zlib can read a `.toml.gz` through ConfigForge today: the Guide has a section "Compressed input" showing a repair that inflates (D-103's composed codec) and `examples/c/inflate.c` builds and runs it under the suite.
* **why**: `docs/research/compression.md` section 5 ("A new interface ... Guide shows both") and section 6, first row: the recipe is what the library lacks a backend for, and it is also the answer before Y-10 to Y-13 exist.
* **needs**: nothing
* **size**: small
* **decided**:
  * **The recipe** is D-103's: a composed codec of a base codec and a repair (`CF_WITH_COMPOSE`, `cmake/ConfigForgeOptions.cmake:19`). The repair is a function the program supplies: it receives the compressed bytes and returns the text. The example inflates with the program's own `zlib` (`inflateInit2(&zs, 16 + MAX_WBITS)` for gzip), checking the output against a limit the program chooses (the example uses 1 MiB and says why: it is the program's bound, not the library's; Y-10's bound replaces it).
  * **Named only, read only, no shared bound** (research section 4.1): the Guide says so in one sentence each, and names Y-10 as the layer that detects and bounds.
  * **The example builds only when the program's zlib is there**: `examples/c/CMakeLists.txt` gets `find_package(ZLIB)`; with no zlib the example is skipped with a status line. `gate.dependencies` (`docs/Dependencies.md`, top) forbids a package the page does not name: add one row to `docs/Dependencies.md` section 2 (developing), "zlib | Zlib | the compressed-input example | skipped when absent", and say it is never linked into the library.
  * **Shape of the example**: reads `examples/c/data/app.toml.gz` (a 200-byte file made once with `gzip -9n` and committed; its text is a three-key TOML) through the composed codec, prints `server.port`; the suite's example harness compares the output (as `examples/c/settings.c`'s test does: find it with `grep -rn "example.settings" examples tests cmake | head -3`).
  * No decision number.
* **files**: `examples/c/inflate.c` (new), `examples/c/data/app.toml.gz` (new), `examples/c/CMakeLists.txt`, `docs/Guide.md` (section 8, "Formats": a subsection "Compressed input" at its end; find the end with `grep -n "^## 9" docs/Guide.md`), `docs/Dependencies.md`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-9 main`.
  2. Read the composed-codec example in the Guide (`grep -n "cf_compose\|repair" docs/Guide.md | head`) and copy its shape.
  3. Write the example's expected output into the harness first (the test), then the CMake wiring with `inflate.c` absent.
  4. See it fail: `cmake --preset default && cmake --build --preset default 2>&1 | tail -n 20 && ctest --preset default -R example.inflate 2>&1 | tail -n 20` fails (no such source).
  5. Write `inflate.c` and make `app.toml.gz` (`printf '[server]\nport = 8080\nhost = "localhost"\n' | gzip -9n > examples/c/data/app.toml.gz`). The test passes.
  6. See that the test can fail: change the expected port in the harness to 8081, see the test fail, put it back.
  7. Guide, Dependencies row, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-9: compressed input by the program's own zlib, a recipe and an example`.
  8. ACCEPT; push; ACCEPT-CI (a runner without zlib skips the example and passes).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the example is skipped when zlib is not found there: say so in the commit); ACCEPT-CI; `ctest --preset default -R "example.inflate|gate.dependencies"` exit 0.
* **stop if**: the composed codec's repair cannot receive binary bytes (D-103's interface takes text): report it, and Y-10's layer becomes the only route; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide section and `examples/c/inflate.c` on reading a gzip file through a composed codec with the program's own zlib (Y-9)." Roadmap: Y-15 marks "compression".

---

## Y-10 - The `cf_unpacker` interface: a byte layer under the codecs, its bounds, a NULL backend (TO DECIDE)

* **id**: Y-10
* **goal**: `cf_unpack_*` exists in `lib/serialize` (`cf_unpack_count`, `_at`, `_by_name`, `_detect`, `_limit`, `cf_unpack`, `cf_pack`, `cf_builder_unpacker`), with the bounds of research section 4.3, three new refusal reasons, and a test backend of the program's own (a repair that "decompresses" a run-length form), with no real compression library linked.
* **why**: `docs/research/compression.md` section 5, "First: reading gzip and zstd, as a byte layer" (option B): one bound against bombs instead of one per program, every reader unchanged. Sections 4.1 (placement) and 4.3 (bounds) hold the reasons.
* **needs**: Y-9, and TO DECIDE 1 to 6 below answered (default: the recommendations)
* **size**: large
* **decided**:
  * **TO DECIDE 1 (where compression sits).** (a) A composed codec, as the Roadmap says: no new interface, named only, no shared bound, read only. (b) A byte layer under the codecs beside D-108's recoding. **Recommended: (b)**, with (a) kept as Y-9's recipe. It changes the Roadmap's wording, so it is the maintainer's. With (a) this card and Y-11 to Y-15 are closed unbuilt and Y-9 stands.
  * **TO DECIDE 4 (the bounds' numbers).** `CF_UNPACK_FACTOR` 64, `CF_UNPACK_FLOOR` 1 MiB, `CF_UNPACK_MAX` 256 MiB, zstd `windowLogMax` 23. **Recommended as written**; each could become a builder option later if a user brings a file that needs more.
  * **TO DECIDE 5 (the interface).** One struct for both directions, `pack` NULL when a backend reads only. **Recommended**, as sketched below. The name `cf_unpacker` is the card's choice; `cf_compression` is the alternative; the card uses `cf_unpacker`.
  * **TO DECIDE 2, 3 and 6 are the later cards'** (Y-11, Y-13, and the release: this card's version is 1.10.0 by 6's recommendation).
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
  * **The bounds** (research section 4.3): constants in `lib/serialize/include/configforge/cf_serialize.h`, not build options; `scripts/generate/limits.cmake` writes them into `docs/Limits.md`'s generated part and a test holds the numbers. Output by input (factor and floor), an absolute ceiling, the decoder's memory (zstd's `windowLogMax` 23, written for Y-12). **One layer only**: unpacked bytes that start with a compression magic again are refused (`CF_REASON_NESTED`), never unpacked again; multi-member input is read by the backend in a loop.
  * **Reasons** (`lib/base/include/configforge/cf_reason.def`, after `CF_REASON_BUDGET`, line 25; the counting test and `docs/Limits.md` follow): `CF_REASON_UNPACK_BOMB` (`"unpack-bomb"`, "a compressed document that makes more than its bound"), `CF_REASON_UNPACK_CORRUPT` (`"unpack-corrupt"`, "a compressed document that does not decompress"), `CF_REASON_NESTED` (`"nested-compression"`, "compressed bytes inside compressed bytes"). Messages: `'<name>' (<codec>) would make more than <limit> bytes (the bound for <n> compressed bytes)`; `'<name>' (<codec>) is damaged: <backend's words>`; `'<name>' unpacks to compressed bytes again; one layer only`.
  * **Registry**: a static array of built-in unpackers (empty when no backend is built: `cf_unpack_count()` is 0), and the program's own through `cf_builder_unpacker` held in the builder (up to `CF_BUILDER_SOURCES` of them, `builder_fail` beyond with `no room for another unpacker`), consulted before the built-in ones. No global mutable state (D-101).
  * **D-189** (after D-188; "Compression is a byte layer under the codecs, bounded once"): Question: how a program reads `app.toml.gz` (the Roadmap proposed a composed codec). Options: composed codecs (named only, M x N, no shared bound, read only); a byte layer beside D-108's recoding (decided); a built-in inflate (later, Y-16). Decision: the layer, one interface `cf_unpacker` for both directions, chosen by the program's name, then the outer extension, then the magic (as D-73 does for formats), refusable per source; three bounds checked while decompressing, never from a header; one layer only; backends are optional, OFF by default; a compression the library lacks comes from the program's own backend or Y-9's recipe. Rejected: nested decompression; brotli and raw deflate as detected formats; archives; a vendored zstd or liblzma.
  * **D-190** (the bounds, "A decompressed document is bounded by its input and by a ceiling"): the three numbers of TO DECIDE 4, as constants, with the reasons of section 4.3.
* **files**: `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/unpack.c` (new; add to `SERIALIZE_SOURCES`, find it with `grep -n SERIALIZE_SOURCES lib/serialize/CMakeLists.txt`), `lib/base/include/configforge/cf_reason.def`, `lib/serialize/tests/test_unpack.c` (new, in `cf_unit_tests(...)` of `lib/serialize/tests/CMakeLists.txt`), `scripts/generate/limits.cmake`, `cmake/configforge.symbols` (the public names; find the file with `ls cmake | grep symbols`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Limits.md` (generated), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1 (the others default to their recommendations unless an answer says otherwise). `git switch -c work/Y-10 main`.
  2. Write `test_unpack.c`, C89, with a test backend `rle`: input `<byte><count>` pairs (count 1 to 255), magic `"RL"`, extension `.rl`. Cases: (a) `cf_unpack_count()` is 0 with no backend built; (b) `cf_builder_unpacker` then a file `cf_up.toml.rl` is detected by extension and unpacked; (c) detected by magic with a name that has no extension; (d) `cf_unpack_limit(0)` is 1 MiB, `cf_unpack_limit(1024)` is 64 * 1024 + 1 MiB, `cf_unpack_limit(SIZE_MAX / 2)` is `CF_UNPACK_MAX`; (e) a bomb (`"RL"` then 5000 pairs of `x 255`, 1.2 MB out for 10 KB in, under 64x + 1 MiB: passes; then 300000 pairs for 600 KB in making 76 MB: over 64 * 600 KB = 38 MB + 1 MiB: refused with reason `unpack-bomb` and the message above); (f) output that starts with `"RL"` again: `nested-compression`; (g) a truncated body: `unpack-corrupt`; (h) `cf_pack` with a NULL `pack`: `'rle' cannot pack`; (i) the program's own backend wins over a built-in of the same name.
  3. See it fail: `cmake --build --preset default --target test_unpack 2>&1 | tail -n 20` (undeclared names).
  4. Write the declarations, `unpack.c`, the reasons, the limits generator and the symbols. `ctest --preset default -R "test_unpack|gate|reason" 2>&1 | tail -n 30`.
  5. See that the test can fail: skip the ceiling check behind a run-time-false condition (`limit == 0xFFFFFFFFu`); (e) fails; put it back.
  6. D-189, D-190, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-10: cf_unpacker, a byte layer under the codecs, its bounds (D-189, D-190)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_unpack` exit 0 in `default` and `minimal` (the layer has no dependency); `gate.limits` or the test that holds `docs/Limits.md` exit 0.
* **stop if**: no answer to TO DECIDE 1; `gate.symbols`, the reason-count test or the API-surface gate wants a change this card does not name (say which); the interface needs `cf_builder` internals that `lib/serialize` may not include (`gate.layering`): report, the registry then moves to `lib/config`; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`cf_unpacker`, a layer that unpacks compressed bytes before a reader sees them, with one bound against bombs (Y-10, D-189, D-190)." `docs/Decisions.md`: D-189, D-190. Roadmap: Y-15 marks "compression".

---

## Y-11 - gzip through zlib: CF_WITH_ZLIB (TO DECIDE)

* **id**: Y-11
* **goal**: with `-DCF_WITH_ZLIB=ON`, `cf_unpack_by_name("gzip")` unpacks gzip, members in a loop, with the library's own header reading, CRC-32 and ISIZE checked, trailing bytes refused, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (reading gzip first), section 4.2 (the options) and 4.4 (licence and supply chain: zlib 1.2.13 or later, CVE-2022-37434).
* **needs**: Y-10, and TO DECIDE 2 below answered
* **size**: medium
* **decided**:
  * **TO DECIDE 2 (new optional dependencies).** Options: both zlib and libzstd; zlib only; none, with a built-in inflate (Y-16) instead. **Recommended: both** (this card and Y-12), both OFF by default, ON in the `full` and `server` profiles (`CMakePresets.json:123-137`), Y-16 later if an embedded user asks. With "none", Y-11 and Y-12 are closed unbuilt and Y-16 is brought forward.
  * **The option**: `option(CF_WITH_ZLIB "gzip through zlib (requires zlib >= 1.2.13; zlib-ng in compat mode also)" OFF)` in `cmake/ConfigForgeOptions.cmake` after `CF_WITH_SQLITE` (line 23); `cmake/ConfigForgeProfile.cmake:20` lists the option for profiles (add `CF_WITH_ZLIB:BOOL` as `CF_WITH_SQLITE:BOOL` is); `lib/serialize/CMakeLists.txt` finds zlib as `lib/config/CMakeLists.txt:62-68` finds sqlite3 and fails with `CF_WITH_ZLIB needs zlib: install it (zlib1g-dev, brew install zlib, vcpkg zlib)`. The profiles `minimal` and `misra` keep it OFF.
  * **The floor, checked three ways** (D-… "enforce version floors", memory note, and the sqlite3 floor's pattern in `lib/config/CMakeLists.txt`): at configure time (`ZLIB_VERSION_STRING` `VERSION_LESS 1.2.13` is a `FATAL_ERROR`), at build time (a `#if ZLIB_VERNUM < 0x12d0` `#error` in `gzip.c`), at run time (`zlibVersion()` compared digit by digit in `cf_unpack_by_name("gzip")`, NULL and a refusal `CF_E_CAPABILITY`, `the zlib in use is <v>; ConfigForge needs 1.2.13 or later`); zlib-ng in compat mode reports its zlib version and passes.
  * **gzip's header is read by the library's own code**, never `inflateGetHeader`: 10 fixed bytes (`1f 8b`, method 8, flags, MTIME, XFL, OS); FEXTRA (2-byte length, skipped by length), FNAME and FCOMMENT (skipped to a NUL, at most 4096 bytes: more is `unpack-corrupt`), FHCRC (2 bytes, skipped, not checked), reserved flag bits set is `unpack-corrupt`. Then raw inflate (`inflateInit2(&zs, -15)`) over the body with the output limit; then CRC-32 (zlib's `crc32`) and ISIZE (low 32 bits of the length) checked against the 8-byte trailer; another member starting with `1f 8b` is read in a loop (the limit counts all members together); other trailing bytes are `unpack-corrupt`, `trailing bytes after the last gzip member`.
  * **The backend**: `lib/serialize/src/gzip.c` (new, in `SERIALIZE_SOURCES` only under `CF_WITH_ZLIB`), a `static const cf_unpacker gzip_unpacker` with `name "gzip"`, `extensions ".gz"`, magic `1f 8b 08`, `unpack = gzip_unpack`, `pack = NULL` (Y-15 sets it), registered in Y-10's built-in array. The output is allocated from the arena in steps doubling from 4 KiB up to the limit; zlib's allocator hooks are the arena's (`zalloc`, `zfree` with the arena, so no `malloc` the program does not see).
  * **Fuzz target** `tests/fuzz/unpack_gzip.c` in the form of the existing ones (`ls tests/fuzz | head`; copy the smallest), input: any bytes through `cf_unpack`, never crashing, never beyond the limit; a seed corpus of three gzip files (one good, one with FEXTRA and FNAME, one two-member).
  * **Dependencies.md**: a row in section 1 "Using the library": `zlib | zlib licence (Zlib) | CF_WITH_ZLIB | gzip; >= 1.2.13 (CVE-2022-37434); never vendored`. The SBOM generator (`configforge.spdx.json`) lists it when ON: find where sqlite3 is added (`grep -rn sqlite cmake/*.cmake scripts | head -5`) and add zlib the same way.
  * **D-191** ("gzip is read by our header code over raw inflate"): the three bullets above in one paragraph; Rejected: `inflateGetHeader` (a second parser inside zlib), a vendored zlib, `gzopen` (a file API, no bound).
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/gzip.c` (new), `lib/serialize/src/unpack.c`, `lib/serialize/tests/test_gzip.c` (new), `lib/serialize/tests/data/` (the gzip fixtures), `tests/fuzz/unpack_gzip.c` (new) and its CMake entry, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE 2. `git switch -c work/Y-11 main`.
  2. Make fixtures with the system `gzip`: `good.gz`, `extra.gz` (FEXTRA and FNAME; made by a ten-line Python script `scripts/work/` is not needed: write the bytes by hand in the test as a C array), `two.gz` (`cat a.gz b.gz`), `badcrc.gz` (one flipped CRC byte), `trunc.gz` (cut at half), `bomb.gz` (`head -c 100000000 /dev/zero | gzip -9`, 97 KB: 1000x: refused).
  3. Write `test_gzip.c` (guarded by `CF_WITH_ZLIB`; compiled to a test that prints `skipped: no zlib` otherwise): each fixture's result, the bomb's `unpack-bomb`, the bad CRC's `unpack-corrupt`, trailing garbage `trailing bytes after the last gzip member`, a header with a FNAME of 5000 bytes `unpack-corrupt`, the floor's message when `zlibVersion()` is faked (a function pointer seam `cfi_gzip_version_fn` in `gzip.c`, not a global).
  4. See it fail: `cmake -S . -B build-zlib -DCF_WITH_ZLIB=ON && cmake --build build-zlib --target test_gzip 2>&1 | tail -n 20` (no `gzip.c`).
  5. Write `gzip.c` and the CMake changes. `ctest --test-dir build-zlib -R "test_gzip|test_unpack|gate" 2>&1 | tail -n 30` and the default build's `ctest --preset default` still green.
  6. See that the test can fail: skip the CRC compare behind a run-time-false condition; `badcrc.gz` passes wrongly and its case fails; put it back.
  7. Fuzz a minute: `cmake -DMODE=quick -DEXTRA="fuzz" -DFUZZ_SECONDS=60 -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake` (as P-3's step 4e does), with `CF_WITH_ZLIB=ON` in the fuzz preset.
  8. D-191, Dependencies row, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-11: gzip through zlib, CF_WITH_ZLIB (D-191)`.
  9. ACCEPT; push; ACCEPT-CI, whose `full` profile now builds with zlib.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (zlib from vcpkg, or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-zlib -R test_gzip` exit 0; the fuzz minute with no finding.
* **stop if**: no answer to TO DECIDE 2; `gate.dependencies` refuses the find call and the fix is more than one line of its allow-list; a gzip made by a tool of the CI image does not unpack (report the bytes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZLIB`: gzip through zlib, read by the library's own header code with CRC-32 and ISIZE checked (Y-11, D-191)." `docs/Decisions.md`: D-191. Roadmap: Y-15.

---

## Y-12 - zstd through libzstd: CF_WITH_ZSTD

* **id**: Y-12
* **goal**: with `-DCF_WITH_ZSTD=ON`, `cf_unpack_by_name("zstd")` unpacks zstd frames (several frames in a loop, skippable frames skipped) with `windowLogMax` 23 and the output limit, and a fuzz target on it.
* **why**: `docs/research/compression.md` section 5 (zstd beside gzip), section 4.2 and 4.4 (the zstd CVEs; the decoder's own memory bound).
* **needs**: Y-11 (its option wiring and fixtures' pattern; TO DECIDE 2 is answered there)
* **size**: medium
* **decided**:
  * **The option**: `CF_WITH_ZSTD` OFF by default, ON in `full` and `server`, wired exactly as Y-11 wires `CF_WITH_ZLIB`; the floor is the zstd release the research names in section 4.4 (read it: it is one number; the card's worker copies it into the three checks, configure, `#if ZSTD_VERSION_NUMBER < <n>` `#error`, and `ZSTD_versionNumber()` at run time).
  * **The backend** `lib/serialize/src/zstd.c`: `name "zstd"`, `extensions ".zst;.zstd"`, magic `28 b5 2f fd`, `pack = NULL`. Streaming decompression (`ZSTD_createDCtx`, `ZSTD_DCtx_setParameter(d, ZSTD_d_windowLogMax, 23)`, `ZSTD_decompressStream` into the arena in doubling steps up to the limit); a frame that asks for more window is refused with `unpack-bomb` and the size it asked for in the message (`'%s' (zstd) asks for a window of <n> bytes; the bound is 8388608`). Frames in a loop; a skippable frame (magic `5e 2a 4d 18` to `5f 2a 4d 18`) skipped by its length; other trailing bytes `unpack-corrupt`. The decompressed size in the frame header is never trusted: the limit is checked as bytes come out. zstd's allocator hooks are the arena's.
  * **Dictionaries are not here** (Y-18).
  * **Fuzz target** `tests/fuzz/unpack_zstd.c`, as Y-11's; seeds: one good frame, two frames, one with a skippable frame first.
  * **D-192** ("zstd is read with a window limit and a loop of frames"): the bullets in one paragraph. Rejected: a vendored libzstd (a large C99 surface under `misra`); dictionaries by default.
  * `docs/Dependencies.md` row: `libzstd | BSD-3-Clause (dual with GPL-2.0; the library uses the BSD one) | CF_WITH_ZSTD | zstd; never vendored`.
* **files**: `cmake/ConfigForgeOptions.cmake`, `cmake/ConfigForgeProfile.cmake`, `CMakePresets.json`, `lib/serialize/CMakeLists.txt`, `lib/serialize/src/zstd.c` (new), `lib/serialize/src/unpack.c`, `lib/serialize/tests/test_zstd.c` (new), `tests/fuzz/unpack_zstd.c` (new), `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-12 main` (after Y-11 landed).
  2. Fixtures with the system `zstd`: `good.zst`, `two.zst` (`cat`), `skip.zst` (a skippable frame written by hand as bytes `50 2a 4d 18 04 00 00 00 de ad be ef` then `good.zst`), `badframe.zst` (flipped byte), `bigwin.zst` (`zstd --long=27 -19` of 200 MB of a repeating text: a window above 2^23; if the host's `zstd` lacks `--long`, the fixture is skipped and the test says so), `bomb.zst` (`head -c 100000000 /dev/zero | zstd -19`).
  3. `test_zstd.c`: each fixture's result and the three refusals by reason.
  4. See it fail: `cmake -S . -B build-zstd -DCF_WITH_ZSTD=ON && cmake --build build-zstd --target test_zstd 2>&1 | tail -n 20`.
  5. Write the backend. `ctest --test-dir build-zstd -R "test_zstd|test_unpack|gate"`.
  6. See that the test can fail: set the window parameter to 27 behind a run-time-false condition; `bigwin.zst` is no longer refused; put it back.
  7. Fuzz a minute as Y-11 step 7. D-192, Dependencies, CHANGELOG. Format; codemap and apiref, last. Commit `Y-12: zstd through libzstd, CF_WITH_ZSTD (D-192)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (libzstd from vcpkg or the case skipped and said so); ACCEPT-CI; `ctest --test-dir build-zstd -R test_zstd` exit 0; a clean fuzz minute.
* **stop if**: Y-11 is not landed or TO DECIDE 2 chose zlib only; `ZSTD_d_windowLogMax` is not in the installed zstd (older than the floor): the floor was wrong, report; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`CF_WITH_ZSTD`: zstd frames through libzstd with a window limit (Y-12, D-192)." `docs/Decisions.md`: D-192. Roadmap: Y-15.

---

## Y-13 - The sources read through the layer: flags, extensions, provenance, bindings (TO DECIDE)

* **id**: Y-13
* **goal**: a file source, a drop-in directory source and the image file source unpack compressed bytes before a reader sees them: the program can name the compression (`CF_COMPRESSED_GZIP`, `CF_COMPRESSED_ZSTD`) or refuse it (`CF_COMPRESSED_NONE`); otherwise the outer extension and then the magic decide; the inner extension names the format; provenance reads `app.toml.gz (gzip)`; the C++, Rust and Python bindings pass the flags.
* **why**: `docs/research/compression.md` section 5 ("The layer is called from `cfi_source_text` ..., from the image file source, and from `configforge codec`" and "Provenance and errors name both").
* **needs**: Y-10, and TO DECIDE 3 below answered; Y-11 or Y-12 for a real backend (the tests use Y-10's `rle` backend where neither is built)
* **size**: medium
* **decided**:
  * **TO DECIDE 3 (detection by magic bytes when built in).** (a) On, refusable per source with `CF_COMPRESSED_NONE`. (b) Off unless the source names a compression or the outer extension does. **Recommended: (a)**: a magic is a declaration like a BOM, the bounds hold either way, and a program that built the backend in asked for it. (b) is the safer default for a program that never expects compressed input but links a build that has it. The steps are (a)'s; for (b) step 4's `by magic` branch is not written and the case (c) of step 2 expects the plain refusal.
  * **Flags** (`lib/config/include/configforge/cf_source.h`, source flags; beside the encoding value `CF_ENCODING_*` which takes bits 24-27, `lib/text/include/configforge/cf_encoding.h:27-28`): `CF_COMPRESSED_SHIFT 28`, `CF_COMPRESSED_MASK (3u << 28)`, values `CF_COMPRESSED_AUTO = 0` (the default), `CF_COMPRESSED_NONE = 1u << 28` (refuse compressed bytes: `'%s' is compressed and this source takes none`, reason `unpack-corrupt`'s sibling is not needed: `CF_E_SOURCE`, no reason), `CF_COMPRESSED_GZIP = 2u << 28`, `CF_COMPRESSED_ZSTD = 3u << 28`. They are accepted where `CF_ENCODING_MASK` is: `CF_FILE_SOURCE_FLAGS` and `CF_DROPIN_SOURCE_FLAGS` (`cf_source.h:249-255`) gain `CF_COMPRESSED_MASK`; other sources refuse them by name as they refuse any flag. If bits 28-29 are taken by a flag the worker finds (`grep -n "1u << 2[89]\|<< 28\|<< 29" lib/*/include/configforge/*.h`), stop and report.
  * **Where it is called**: `cfi_source_text` (`lib/config/src/bridge/file.c:292`) gains a first step before `cfi_source_recode`: when the flags and the name say the bytes are compressed, `cf_unpack` into the source's arena (the same arena as the text), `raw` then names the unpacked bytes; the codec was chosen from the path by the caller before this call (`file_named`), so the caller (`file_load` at 421-434 and `dir.c:178`) is changed to strip one outer extension when it is a known unpacker's (`app.toml.gz` names `toml`: `cf_codec_by_path` of the path without `.gz`). After unpacking, the recoding (D-108) and the detection by content go on as today. Nested compression is refused by Y-10's `nested-compression`.
  * **Provenance**: the source's `path` stays `app.toml.gz`; `cf_explain` and Doctor name `app.toml.gz (gzip)`: `file_state` gains `const char* unpacked` (the unpacker's name or NULL), shown by the code that shows `st->encoding` today (find with `grep -n "st->encoding" lib/config/src/bridge/file.c`); an error line and column are "in the decompressed text": the message is `app.toml.gz (gzip):3:5: ...` using the provenance's text as the file name for the reader's errors.
  * **Digest and fingerprint** are of the uncompressed bytes (the text the reader reads), so a file recompressed at another level has the same fingerprint.
  * **The image file source** (`cf_image_open` or the function that reads an image file: `grep -n "image" docs/Codemap.md | head`): the same call before the image header is read. The image bytes keep their own layout (nothing inside the format is compressed, research "Not to build").
  * **Bindings**: the C++, Rust and Python wrappers each pass the three new flag values where they pass `CF_ENCODING_*` (`grep -rn "CF_ENCODING_UTF16LE" bindings | head`); one test each that a flag reaches the call (with Y-10's backend registered through the C API, or skipped when no backend is built).
  * **D-193** ("A compressed file is read through the layer, by name, extension or magic"): the flags, the order (program's flag, outer extension, magic), `CF_COMPRESSED_NONE`, the digest of the uncompressed bytes, one layer.
* **files**: `lib/config/include/configforge/cf_source.h`, `lib/config/src/bridge/file.c`, `lib/config/src/bridge/dir.c`, the image file source (named by the grep above), `bindings/cpp/`, `bindings/rust/`, `bindings/python/` (the files the grep above names), `lib/config/tests/test_compressed.c` (new; add to `lib/config/tests/CMakeLists.txt` after `test_file`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md` (section 8's "Compressed input", replacing Y-9's recipe-only text with both), `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE 3 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-13 main` (after Y-10).
  2. Write `test_compressed.c` with Y-10's `rle` backend registered through `cf_builder_unpacker`: (a) `cf_ct.toml.rl` read by extension, `cf_explain` names `cf_ct.toml.rl (rle)`; (b) a file with no compression extension but the `RL` magic: read by magic (TO DECIDE 3a); (c) `CF_COMPRESSED_NONE` on the same file: refused with `is compressed and this source takes none`; (d) `CF_COMPRESSED_GZIP` on a file whose bytes are plain text: refused `is not gzip` (the flag names a compression, the bytes must be it); (e) a syntax error on line 2 of the unpacked text: the message names `cf_ct.toml.rl (rle):2:`; (f) the fingerprint of the unpacked file equals that of the same text in `cf_ct.toml`; (g) a drop-in directory holding `10-a.toml.rl` and `20-b.toml`: both read; (h) nested compression refused; (i) a flag the other sources do not know (`cf_builder_env_document` with `CF_COMPRESSED_GZIP`) is refused by name.
  3. See it fail: build `test_compressed` (the flags are undeclared).
  4. Write the flags, the call in `cfi_source_text`, the outer-extension strip, the provenance, the image source and the bindings. `ctest --preset default -R "test_compressed|test_file|test_dir|test_image|binding" 2>&1 | tail -n 30` (list the binding tests with `ctest --preset default -N | grep -i "py\|rust\|cpp"`).
  5. See that the test can fail: skip the unpack call behind a run-time-false condition; (a) fails; put it back.
  6. D-193, Guide, CHANGELOG. Format; codemap and apiref, last. Commit `Y-13: the file, directory and image sources read compressed bytes through the layer (D-193)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_compressed` exit 0 in `default` and `minimal`; with `-DCF_WITH_ZLIB=ON` a gzip case of the same test also passes.
* **stop if**: bits 28 and 29 are taken; the outer-extension strip changes a result of `test_file` or `test_dir` (a file named `x.json.gz` today is refused or read by content: say which); a binding gate asks for more than the flag values; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "File, directory and image sources read gzip and zstd files by flag, outer extension or magic; provenance names `app.toml.gz (gzip)` (Y-13, D-193)." `docs/Decisions.md`: D-193. Roadmap: Y-15.

---

## Y-14 - `configforge codec` through the layer, and a bomb corpus

* **id**: Y-14
* **goal**: `configforge codec detect`, `check` and `dump` read compressed files through the layer (`detect` says "gzip, then TOML by the extension"), and the test corpus holds the classic bombs.
* **why**: `docs/research/compression.md` section 5 ("`configforge codec` ... `detect` says ...") and section 6; the corpus is what holds the bounds against regressions.
* **needs**: Y-13
* **size**: small
* **decided**:
  * **Where**: `tools/codec/codec.c` (`detect` at line 76; find `check` and `dump` by name). Each calls `cf_unpack_detect` on the input as the source does; when it answers, `detect` prints `gzip, then TOML by the extension` (or `by the content`), `check` and `dump` read the unpacked bytes.
  * **A flag `--no-unpack`** on the three commands refuses compressed input: it sets `CF_COMPRESSED_NONE` as the sources do. No other flag.
  * **The corpus** `tests/conformance/bombs/` (new): `gzip-1000x.gz` (a 98 KB file making 100 MB), `gzip-nested.gz` (a gzip of a gzip), `gzip-truncated.gz`, `gzip-bad-crc.gz`, `zstd-window.zst` and `zstd-1000x.zst` (made with `zstd`; only when `CF_WITH_ZSTD`), each with a `.expect` line: the reason. A test `test_bombs` runs each through `cf_unpack` and compares the reason; a backend not built skips its files with a line saying so.
* **files**: `tools/codec/codec.c`, `tools/codec/tests/` (the codec tool's tests: find with `ls tools/codec`), `tests/conformance/bombs/*` (new), `tests/conformance/CMakeLists.txt` or the place the existing corpus is wired; last: `CHANGELOG.md`, `docs/Tools.md` (the codec page's three commands).
* **steps**:
  1. `git switch -c work/Y-14 main` (after Y-13).
  2. Write the tests first: `detect` of `app.toml.gz` prints the line above; `check` of the same exits 0; `check --no-unpack` exits non-zero with `takes none`; each corpus file's expectation.
  3. See it fail: run the codec tool's tests (`ctest --preset default -R "codec|bombs" 2>&1 | tail -n 20`).
  4. Write the tool's change and the corpus wiring. The tests pass in the build with zlib and in the default build (skipping).
  5. See that the test can fail: change one `.expect` reason, see `test_bombs` fail, put it back.
  6. CHANGELOG, `docs/Tools.md`. Format; codemap and apiref, last. Commit `Y-14: configforge codec reads compressed files; a corpus of bombs`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "codec|bombs"` exit 0.
* **stop if**: Y-13 is not landed; the codec tool links no part of `lib/config` and cannot reach `cf_unpack_detect` (report: it is `lib/serialize`'s, so it should); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`configforge codec detect`, `check` and `dump` read compressed files; a corpus of decompression bombs (Y-14)." No D-number. Roadmap: Y-15.

---

## Y-15 - Writing: cf_pack, deterministic gzip and zstd, `convert --compress`, a compressed image

* **id**: Y-15
* **goal**: `cf_pack` writes gzip and zstd over a finished buffer, byte for byte the same on every run (gzip with MTIME 0 and OS 255, a fixed level), `configforge codec convert --compress gzip|zstd` and `cf_image_write` use it, the digest and the provenance stay those of the uncompressed bytes, and the whole-or-nothing write of wave 6 writes the compressed bytes like any other.
* **why**: `docs/research/compression.md` section 5, "After: writing" (option C), and section 6, "Writing" (2.2.0). It marks the Roadmap item "compression" done.
* **needs**: Y-11, Y-12, Y-14, F-1b (the public file write)
* **size**: medium
* **decided**:
  * **gzip** (`gzip.c`, Y-11): `pack` sets `deflateInit2(&zs, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY)` and writes the 10-byte header itself (`1f 8b 08 00 00 00 00 00 <xfl> ff`: no name, MTIME 0, OS 255, XFL 2 for level 9, 4 for level 1, else 0), the deflate body, then CRC-32 and ISIZE; the default level is 6 (`level == 0` in `cf_pack` means "the default"; `1` to `9` are zlib's). Deterministic: the same input and level give the same bytes on any platform with zlib 1.2.13 or later and zlib-ng does not (zlib-ng's deflate output differs): the page and D-194 say determinism holds for one zlib implementation, and the test compares with a fixture only when `ZLIB_VERSION` is not zlib-ng's (`#ifdef ZLIBNG_VERSION`).
  * **zstd** (`zstd.c`, Y-12): `ZSTD_compressStream2` with `ZSTD_c_compressionLevel` (default 3; `level` 1 to 19), `ZSTD_c_checksumFlag` 1, `ZSTD_c_contentSizeFlag` 1, `ZSTD_c_nbWorkers` 0 (a thread count changes nothing in the bytes, but 0 keeps the library thread-free, D-14), no dictionary; one frame.
  * **The measure-or-write form**: `cf_pack` first emits to a sink that measures (`cf_sink`, as `cf_emit` does), then the caller allocates and calls again; the backend's `pack` supports it by running its encoder twice (research section 5: "Measure-or-write, like cf_sink").
  * **`configforge codec convert --compress gzip|zstd`** (`tools/codec/codec.c`): the output is the converted text packed; without the flag, an output named `.gz` or `.zst` packs by its outer extension and says so on stderr (`packing as gzip by the extension`).
  * **`cf_image_write`** (find with `grep -n cf_image_write docs/Codemap.md`): a path ending in a known outer extension is packed; `ImageFormat.md` gains one paragraph that the image's header and sections are those of the uncompressed bytes.
  * **The digest and fingerprint** are of the uncompressed bytes (Y-13), so a recompressed file has the same.
  * **Writing through F-1b's call** (`cf_emit_file`): an output path with a known outer extension is packed before the platform write; the file is replaced whole or not at all as any other.
  * **D-194** ("Compressed output is deterministic for one library and level"): the bullets above in one paragraph; Rejected: a name and a timestamp in the gzip header; threads in zstd; compression inside the image format.
* **files**: `lib/serialize/src/gzip.c`, `lib/serialize/src/zstd.c`, `lib/serialize/src/unpack.c`, `lib/config/src/bridge/emit.c` (`cf_emit_file`), the image writer, `tools/codec/codec.c`, `lib/serialize/tests/test_pack.c` (new), `tools/codec/tests/`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/ImageFormat.md`, `docs/Guide.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-15 main` (after the cards above).
  2. `test_pack.c`: (a) pack then `cf_unpack` returns the input for sizes 0, 1, 4096 and 3 MiB, both backends; (b) two packs of the same input and level are equal byte for byte; (c) a gzip pack has bytes 4 to 7 (MTIME) zero and byte 9 `ff` (the system `gzip -dc` accepts it: `system("gzip -dc cf_pk.gz > cf_pk.out")`, skipped when `system` fails); (d) the measure call returns the exact size the write needs; (e) the fingerprint of a configuration read from `cf_pk.toml.gz` equals the one read from `cf_pk.toml`; (f) `convert --compress gzip` of a JSON file gives a file `gzip -dc` turns back into the TOML text; (g) `cf_emit_file` to `x.toml.gz` reads back through Y-13.
  3. See it fail: build `test_pack` (`pack` is NULL, so `cf_pack` says `'gzip' cannot pack`).
  4. Write the packers and the three callers. `ctest --test-dir build-zlib ...` and `build-zstd` as before.
  5. See that the test can fail: write MTIME from a clock behind a run-time-false condition; (b) fails; put it back.
  6. D-194, docs, CHANGELOG, the Roadmap row (🟩 "compression"). Format; codemap and apiref, last. Commit `Y-15: cf_pack, deterministic gzip and zstd output (D-194)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX with `-DCF_WITH_ZLIB=ON -DCF_WITH_ZSTD=ON`; ACCEPT-WINDOWS; ACCEPT-CI; `ctest -R "test_pack|codec"` exit 0.
* **stop if**: F-1b or Y-14 is not landed; two packs differ on the same machine (a nondeterminism in the backend: report where); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`cf_pack`, `configforge codec convert --compress` and compressed images: gzip and zstd written deterministically (Y-15, D-194)." `docs/Decisions.md`: D-194. `docs/Roadmap.md`: the item "Compression" under "From the maintainer, 2026-11-26" 🟩.

---

## Y-16 - A built-in inflate for the embedded presets (parked: when a user asks)

* **id**: Y-16
* **goal**: a `CF_WITH_INFLATE` option adds a gzip reader with no dependency, written in C89 under the `misra` rules, differentially fuzzed against zlib, for the embedded presets that want a compressed image.
* **why**: `docs/research/compression.md` section 5, "Later" (option D) and section 4.5 (embedded and minimal profiles).
* **needs**: Y-11, Y-15; and a user's ask (status: parked, waits on the ask)
* **size**: large
* **decided**:
  * **TO DECIDE (whether to build it at all).** Options: build (a small inflate, about 400 lines, as `tinfl` or `puff.c` shows; research section 4.5); never (a program on a small target compresses nothing); use the program's own inflate through Y-9's recipe. **Recommended: wait for a user who asks**, then build.
  * **Shape if built**: `lib/serialize/src/inflate.c`, one function of the `cf_unpacker` interface under the name `"gzip"` when `CF_WITH_ZLIB` is OFF and `CF_WITH_INFLATE` is ON (never both: a configure error `CF_WITH_INFLATE and CF_WITH_ZLIB both provide gzip`); reads only; the bounds of Y-10 and the header/trailer code of Y-11 are shared (`gzip_header.c`, moved out of `gzip.c` by the card, with no behaviour change); fixed and dynamic Huffman blocks, stored blocks, a 32 KiB window in the arena; no allocation outside the arena; a differential fuzz target `tests/fuzz/inflate_diff.c` comparing against zlib on every input (the `yamldiff` pattern: `tests/fuzz/yamldiff*.c`), required to find no difference in an hour.
  * Its steps, accept and stop-if are written when a user asks, by the cold read that opens it; this card is parked until then (docs/Plan.md, "Waits on the maintainer").
* **files**: `lib/serialize/src/inflate.c`, `lib/serialize/src/gzip_header.c`, `tests/fuzz/inflate_diff.c`, `cmake/ConfigForgeOptions.cmake`, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-195), `docs/Dependencies.md`.
* **steps**:
  1. Wait for the ask; then the opening cold read writes the steps from this card's "decided" and the pattern of Y-11's steps (test first, see it fail, the differential fuzz hour).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI and an hour of the differential fuzz without a difference.
* **stop if**: no ask has come; the inflate cannot be held to the `misra` rules without a deviation the maintainer has not recorded.
* **docs**: when built: `CHANGELOG.md`, "## 2.2.0 — unreleased" or the version then open, "### Added"; `docs/Decisions.md`: D-195.

---

## Y-17 - xz and lz4 frames (parked: when a user asks)

* **id**: Y-17
* **goal**: `CF_WITH_LZMA` (xz through liblzma, refusing 5.6.0 and 5.6.1, with a `memlimit` of the order of zstd's window) and `CF_WITH_LZ4` (lz4 frames) as backends of the `cf_unpacker` interface, each OFF by default.
* **why**: `docs/research/compression.md` section 5, "Later" (E) and section 4.4 (the xz backdoor of 2024 and its refusal).
* **needs**: Y-12; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE (whether to build, and which of the two).** Options: xz only; lz4 only; both; neither. **Recommended: none until asked**; xz first when asked, because configuration archives from distributions are xz, and lz4 for a program whose own files are lz4.
  * **Shape if built**: as Y-12: `xz.c` (`name "xz"`, `.xz`, magic `fd 37 7a 58 5a 00`, `lzma_stream_decoder` with `memlimit` 8 MiB written as a constant in `cf_serialize.h` beside the others, `LZMA_CONCATENATED`, `lzma_version_number()` refusing `50060002` to `50060012`, i.e. 5.6.0 and 5.6.1, at run time as well as at configure time) and `lz4.c` (`.lz4`, magic `04 22 4d 18`, `LZ4F_decompress` with a frame-size check); fuzz target and fixtures for each; Dependencies rows.
  * The steps are written by the opening cold read, on Y-12's pattern.
* **files**: `lib/serialize/src/xz.c`, `lib/serialize/src/lz4.c`, tests, fuzz targets, `cmake/ConfigForgeOptions.cmake`, `docs/Dependencies.md`; last: `CHANGELOG.md`, `docs/Decisions.md` (D-196).
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from Y-12's.
* **accept**: as Y-12, for each backend built.
* **stop if**: no ask has come; the installed liblzma is 5.6.0 or 5.6.1 (the refusal fires: the card stops and says so).
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-196.

---

## Y-18 - zstd dictionaries on a source (parked: when a user asks)

* **id**: Y-18
* **goal**: a source can name a zstd dictionary (through `cf_unpacker`'s `data` field) so that small files compressed with one are read.
* **why**: `docs/research/compression.md` section 5, "Later" (F) and section 4.6 (dictionaries).
* **needs**: Y-13, Y-15; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE (whether to build at all, and the interface).** Options: a dictionary by the program's own `cf_unpacker` whose `data` is the dictionary (no new public name: the program makes a copy of the built-in zstd descriptor with its own `data`); a builder call `cf_builder_dictionary(b, source, bytes, n)`; never. **Recommended: the first**, only when someone asks; it needs nothing new.
  * **Shape if built**: `zstd.c` reads `self->data` as a dictionary (`ZSTD_DCtx_loadDictionary`), refuses a dictionary above 1 MiB (`CF_UNPACK_DICT_MAX`), and the frame's dictionary id is checked against the dictionary's (`'%s' needs dictionary <id>`); a frame that names a dictionary with none given is refused.
  * The steps are written by the opening cold read, on Y-12's pattern.
* **files**: `lib/serialize/src/zstd.c`, tests, the Guide's "Compressed input"; last: `CHANGELOG.md`, `docs/Decisions.md` (D-197).
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from Y-12's.
* **accept**: as Y-12.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-197.

---

## Y-19 - `gen check --report`: a predicted, value-blind report from files and a contract (TO DECIDE)

* **id**: Y-19
* **goal**: `configforge gen check --schema app.schema.json [--report out.json] [--unknown warn|reject] <file>...` reads every file as the program would, in order, and with `--report` writes the value-blind report of what the files would configure, marked `"predicted": true`, exiting 0 unless a file cannot be read (2); without `--report` it behaves as today.
* **why**: `docs/research/analysis-tool.md` section 5, "First: Option B" (a predicted report from files, and Doctor on it), with section 4's Option B for the mechanism. It reuses the report and Doctor's rules the project already has, for every format the library reads, with places. The research asks the tool to start by doing what the others do (yaml-language-server, Taplo, conftest); Y-21 gives that, Y-19 is what differs.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **TO DECIDE 1 (where the static mode lives)**, research section 5, TO DECIDE 1. Options: **(a)** `configforge gen check --report`, then `doctor lint` (no new program; Doctor keeps its non-goal; `gen` is already the contract's program); **(b)** a new `configforge check` (one step; a third meaning of "check" beside `codec check` and `gen check`); **(c)** Doctor reads files itself (rejected: it breaks `docs/Doctor.md:853`, "Resolving configuration itself"). **Recommended: (a).** The steps are (a)'s; for (b) the new program is a thin front over the same function and `tools/gen/CMakeLists.txt` gains a second executable name, which is the maintainer's to ask for; (c) is not offered.
  * **Where.** `gen_check` (`tools/gen/contract.c:50-100`; its usage text `contract_usage`, 38-46). Its loop gains `--report <path>` and `--unknown warn|reject` (default `reject`, as today at `cf_builder_unknown(b, CF_UNKNOWN_REJECT)`; with `warn` the call is `CF_UNKNOWN_WARN` (`lib/config/include/configforge/cf_define.h:318-322`), so unknown keys are candidates that CFD005 reports); the usage line gains both. An option given twice, or `--report` without a path, is `contract_usage()` (exit 2).
  * **With `--report`**, after `cf_builder_create`: the configuration is built with a builder that keeps going past a refused value when the unknown mode is `warn` (a refused value is a refused candidate in the report: research section 4, Option B). The report is written with `cf_report` (`lib/config/include/configforge/configforge.h:666`) from a `cf_report_info` of `CF_REPORT_BLIND` whose `profile` is NULL, `application` the schema's title when it has one, and `generated` NULL (a file made from the same inputs is the same bytes). The file is written whole or not at all (`cf_platform_write_file`, F-1a) when `--report` names a path, and to stdout when it is `-`.
  * **The mark.** `cf_report_info` gains a last member `cf_bool predicted;` (lib/config/include/configforge/cf_report.h:45-52; its `size` field makes a new last member an addition, the `CF_REPORT_INFO_INIT` at line 55 gains a `CF_FALSE`, `CF_REPORT_INFO_INIT` is `{sizeof(cf_report_info), CF_REPORT_BLIND, NULL, NULL, NULL, NULL, CF_FALSE}`); `cf_report_write` (`lib/config/src/*/report.c`; find `"profile"` with `grep -n '"profile"' lib/config/src/*/report.c`) writes `"predicted": true` beside it only when the field is true and the info's `size` covers it, else nothing (an older caller's report is byte for byte unchanged). The cf_report format version does not change (an added member).
  * **What exit codes mean.** 0 done (without `--report`: every file holds to the contract, as today; with `--report`: the report was written, whatever the findings are: Doctor judges them); 1 only without `--report` (a file does not hold); 2 unusable input (a file or the schema cannot be read, or the report cannot be written). A file in a program's own codec or repair (D-103) that `gen` cannot load is "cannot read", exit 2, never a pass (research section 4, Option B, "Rules out").
  * **D-198** ("A predicted report is the contract's program's, and Doctor judges it"): Question: how a repository checks its configuration files before a release without running the program. Options: recipes only (Y-21, always); a predicted report from `gen check` then Doctor (decided); a new `configforge check`; Doctor reading files (breaks `docs/Doctor.md:853`). Decision: `gen` resolves, Doctor judges; the report says `"predicted": true`; `--unknown warn` makes unknown keys candidates; exit 0 with `--report` unless a file cannot be read. Rejected: findings about the program's own codecs; a checker that touches the machine it runs on (research section 3, failure mode 6).
* **files**: `tools/gen/contract.c`, `lib/config/include/configforge/cf_report.h`, the report writer (found by the grep above), `tools/gen/tests/` (the gen tool's tests: `ls tools/gen` shows none: put the new cases in `tests/gen_check/` and add `add_test` lines in `tools/gen/CMakeLists.txt` after the existing gen tests: find them with `grep -n add_test tools/gen/CMakeLists.txt`), `tests/gen_check/*` (new: `app.schema.json`, `good.toml`, `bad.toml`, `bad.yaml`), `docs/Tools.md`, `docs/Doctor.md` (a paragraph); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-19 main`.
  2. Make the fixtures: a schema with `server.port` (integer, 1-65535), `log.level` (enum `debug|info|warn`), `db.password` (sensitive string), `old.key` (deprecated); `good.toml` (all four valid), `bad.toml` (`server.prot = 1`, `server.port = 70000`, `log.level = "loud"`, and `old.key = 1`), `bad.yaml` (the same keys in YAML, with `server.port` twice).
  3. Tests (shell-free, through `add_test` with `COMMAND` and `PASS_REGULAR_EXPRESSION` or the gen tool's existing test style): (a) `gen check --schema S good.toml` exits 0 and prints `ok: 1 file(s)`; (b) `gen check --schema S --report r.json --unknown warn bad.toml` exits 0, `r.json` contains `"predicted": true` and `"server.prot"` as a candidate whose place starts `bad.toml:1`; (c) `configforge doctor lint r.json` (after Y-20) is Y-20's test; here a `grep` of `r.json` for CFD-relevant data suffices: `"server.port"` has a refused candidate with `bad.toml:2`; (d) `--report -` writes the JSON to stdout; (e) a file that cannot be read exits 2 and writes no report; (f) two `--report` options exit 2; (g) `bad.yaml`'s duplicate key shows both places; (h) running the same command twice gives byte-identical reports (`cmp`); (i) the report contains no value text: `grep -c "70000" r.json` is 0 (value-blind).
  4. See it fail: `cmake --build --preset default --target gen 2>&1 | tail -n 5 && ctest --preset default -R gen_check 2>&1 | tail -n 20` (`--report` is a usage error today).
  5. Write the option parsing, the `predicted` member and the report writing. `ctest --preset default -R "gen|report|doctor" 2>&1 | tail -n 30` (every existing report test unchanged).
  6. See that the test can fail: write `"predicted": true` always behind a run-time-false condition; (a report of an older caller changes) the existing report goldens fail; put it back.
  7. D-198, docs, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-19: gen check --report, a predicted report from files (D-198)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_check|test_report"` exit 0.
* **stop if**: no answer to TO DECIDE 1; an unknown key under `CF_UNKNOWN_WARN` is not in the report as a candidate (the research asks a card to confirm it: report what `cf_report` writes, the card then adds the candidate to the report writer); a refused value under `warn` aborts `cf_builder_create` (report: the builder needs a mode that keeps going, which is a new public choice); the existing report goldens change; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`configforge gen check --report`: a predicted, value-blind report from files and a contract, for `doctor lint` (Y-19, D-198)." `docs/Decisions.md`: D-198. `docs/Roadmap.md`: the item "An analysis tool" under "From the maintainer, 2026-11-26" stays 🟨 until Y-22 and Y-25.

---

## Y-20 - Doctor on a predicted report: the rules a running program is needed for are off and named; SARIF columns and fingerprints

* **id**: Y-20
* **goal**: `configforge doctor lint` on a report marked `"predicted": true` turns off the rules that need a running program, says which in its output, and its SARIF regions carry `startColumn` and each result a `partialFingerprints` entry.
* **why**: `docs/research/analysis-tool.md` section 5 ("Two small changes in Doctor come with it") and section 6, second row. Without it a predicted report makes false findings (CFD018 "set but never read", CFD008's default-only in production) and GitHub loses a finding's identity when lines move (sources [33], [34]).
* **needs**: Y-19
* **size**: small
* **decided**:
  * **Which rules are off** on a predicted report: CFD018 (set but never read: nothing ran), CFD008 (default-only in production: no profile is known), and CFD007's half about files readable by every user of the machine (the machine is not the program's: only its command-line half, which a predicted report cannot have either, so CFD007 is off whole). Everything else runs: CFD001, 004, 005, 006, 009, 010, 011, 013, 015, 016, 019. Each off rule is named once in the output as `note: CFD018 is off: the report is predicted from files, not observed`; in `--format sarif` the notes are in `runs[0].invocations[0].toolExecutionNotifications` (`level: "note"`, `message.text` the same sentence).
  * **Where**: `tools/doctor/engine.c`: the rule tables `g_lint_rules` (line 1867) are run by `doctor_lint_reports` (line 1880); the `report` struct (read at 209: `r->profile = text_at(&r->doc, "profile")`) gains `int predicted` read from `"predicted"`; `doctor_lint_reports` skips the three rules when it is set and prints the notes before the findings. A rule skipped this way never appears as a finding; a `--severity` or profile line naming it is ignored with the same note.
  * **SARIF** (`tools/doctor/doctor.c:130-152`): the `region` object is `{ "startLine": <line>, "startColumn": <col> }` when the place has a column: `doctor_place_of` (find with `grep -n doctor_place_of tools/doctor/*.c`) returns `path:line` today; it gains a third out-parameter `col` (0 when the place is `path:line` only, then `startColumn` is not written). The place `app.ini:2:1` already carries one (research section 5). Each result gains `"partialFingerprints": { "configforge/v1": "<fnv1a of rule id, key and file>" }` made with the library's own FNV-1a (`cf_fingerprint_step`, as `engine.c`'s CFD019 uses it), 16 lowercase hex digits; the line is not part of it, so a finding keeps its identity when lines move.
  * **D-199** ("A predicted report switches off what only a running program can say"): the three rules, the notes, `startColumn`, `partialFingerprints` over rule, key and file. Rejected: running the rules and labelling their findings; a fingerprint that includes the line.
* **files**: `tools/doctor/engine.c`, `tools/doctor/doctor.c`, `tools/doctor/engine.h`, the Doctor tests (`ls tools/doctor` has none: the tests are in `tests/doctor/`; find them with `grep -n "doctor" tests/CMakeLists.txt | head`), `docs/Doctor.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-20 main` (after Y-19).
  2. Tests: (a) lint of Y-19's `r.json` (bad.toml) prints the three notes and no CFD018, CFD008 or CFD007 finding, and does print CFD005 for `server.prot` with a nearest-name hint, CFD006 for `server.port`; (b) `--format sarif` of it has `startColumn` where the place has one, `partialFingerprints.configforge/v1` of 16 hex digits on every result, and the `toolExecutionNotifications` notes; (c) the same findings with the line of `bad.toml` moved by inserting a comment line at its top give the same `partialFingerprints` and a different `startLine`; (d) a report with no `"predicted"` member lints exactly as before (golden unchanged); (e) a place without a column writes no `startColumn`.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20` (the new cases fail: no notes, no columns).
  4. Write the changes. `ctest --preset default -R "doctor|gen_check" 2>&1 | tail -n 30`.
  5. See that the test can fail: run CFD018 on a predicted report behind a run-time-false condition; (a) fails; put it back.
  6. D-199, `docs/Doctor.md` (the rules table gains a column note, "off on a predicted report", at the three rules; section on SARIF gets the two fields), CHANGELOG. Format; codemap and apiref, last. Commit `Y-20: Doctor on a predicted report; SARIF columns and fingerprints (D-199)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: Y-19 is not landed; the SARIF golden tests compare whole files and a column or a fingerprint cannot be added without changing a golden of a report that is not predicted (the new fields appear in every SARIF run, so those goldens change: that is expected and each changed golden is listed in the commit message; stop only if a changed golden shows anything else); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor on a predicted report turns off the rules a running program is needed for and says which; SARIF results carry `startColumn` and `partialFingerprints` (Y-20, D-199)." `docs/Decisions.md`: D-199.

---

## Y-21 - Recipes: the contract's schema in yaml-language-server and Taplo, conftest over a report with places

* **id**: Y-21
* **goal**: the Guide has a page "Checking files in an editor and in CI" that shows how to use the schema the library already writes (`cf_schema`, `configforge gen schema`) in yaml-language-server and in Taplo, and how to run conftest over a report with each candidate's place copied into `_loc`; each recipe is run by a test.
* **why**: `docs/research/analysis-tool.md` section 5 ("Then, in this order": 2, **A**, the recipes) and section 4, Option A. The analysis tool starts by doing what the others do (maintainer, 2026-11-26): a user gets completion and hover for YAML, TOML and JSON, and policy rules over a report, with no new code.
* **needs**: nothing (the report example is clearer after Y-19)
* **size**: small (documentation and examples)
* **decided**:
  * **Where**: `docs/Guide.md`, a new section after "11. The editor, the report and Doctor" (heading at line 1315) as a subsection `### Checking files before they ship`; examples in `examples/recipes/` (new): `app.schema.json` (written once with `configforge gen schema --schema <contract>` and committed), `app.yaml` with the modeline `# yaml-language-server: $schema=app.schema.json`, `app.toml` with `#:schema ./app.schema.json` (Taplo's directive), `.taplo.toml` with `[[rule]] include = ["*.toml"] schema.path = "app.schema.json"`, and `policy.rego` for conftest.
  * **conftest over a report**: the recipe converts a report with `jq` so that every candidate carries `_loc` from its `at` (`jq '.keys |= map(. + {_loc: .at})'`: check the report's real member names first with `configforge gen check --report` from Y-19 on a fixture, or with an existing report in `tests/`), then `conftest test --policy policy.rego report.json`; `policy.rego` holds one rule: `deny[msg] { ... }` "`pool.max` is at least `pool.min`" with `msg` formatted with `_loc`. The Guide says what conftest cannot see (a rule that errors passes: research section 3, failure mode 8: write the recipe's rule with a test input that must be denied).
  * **Tests**: the examples are run by the suite only when the tools are on the PATH: `ctest -R recipes` prints `skipped: yaml-language-server not found` etc. (the schema's own validity is checked with the library: `configforge gen check --schema app.schema.json app.toml app.yaml` exits 0 on the good files and 1 on `bad.toml`).
  * No decision number: documentation. The Guide says: relations between keys are a recipe here (research section 5, TO DECIDE 2 (a)); Y-24 is the parked card for doing them in the contract.
* **files**: `docs/Guide.md`, `examples/recipes/*` (new), `examples/CMakeLists.txt` (or `tests/CMakeLists.txt`: where the example harness lives, `grep -rn "example\." tests/CMakeLists.txt | head -3`); last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-21 main`.
  2. Write the harness lines first (`ctest -R recipes`): the schema check of the good and bad files; each external-tool step skipped when absent.
  3. See it fail: `ctest --preset default -R recipes 2>&1 | tail -n 20` (no such test, then no files).
  4. Write the examples and the Guide subsection; make `app.schema.json` with the library: `build/tools/gen/gen --schema <the contract the examples use> --out examples/recipes/app.schema.json` (the exact form is in `docs/Tools.md`).
  5. `ctest --preset default -R recipes`; then, where `conftest`, `taplo` and `yaml-language-server` are installed, run each by hand once and write the exact command lines into the Guide as run.
  6. See that the test can fail: change a value in `bad.toml` to a valid one; the "exits 1" case fails; put it back.
  7. CHANGELOG. Format; codemap and apiref, last. Commit `Y-21: recipes for editors and conftest from the contract's schema`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R recipes` exit 0 (skips are exits 0).
* **stop if**: the schema the library writes is refused by yaml-language-server or Taplo when run by hand (report the message: it is a finding about `cf_schema`, not a Guide fix); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide page and `examples/recipes/` on checking files with yaml-language-server, Taplo and conftest from the contract's schema (Y-21)." Roadmap: the "conftest over a report" line (`docs/Roadmap.md:938-941`) is marked 🟩 by this card.

---

## Y-22 - A secret written in a checked file: CFD022 (TO DECIDE)

* **id**: Y-22
* **goal**: `configforge doctor lint` on a predicted report reports CFD022, an error, for each key the contract marks `CF_SENSITIVE` or that is named like a secret (D-84) when a file given to `gen check` holds a value for it; the value is never printed.
* **why**: `docs/research/analysis-tool.md` section 5, TO DECIDE 3, and section 6 (fifth row). `docs/Doctor.md:430` puts "whether a file is committed to version control" out of CFD007's scope; in the files mode the files are the repository, and a secret in one is the one secret finding that is a fact, not a guess.
* **needs**: Y-20, C-8b (CFD021 is C-8b's, so this rule takes the next number)
* **size**: small
* **decided**:
  * **TO DECIDE 3.** **(a)** a new rule CFD022: a `CF_SENSITIVE` key, or one named like a secret (D-84), has a value in a file given to `gen check`; error; the value never printed (recommended); **(b)** leave it to gitleaks. With (b) this card is closed unbuilt, and the Guide's Y-21 page says "use gitleaks for secrets in files".
  * **The rule** (`tools/doctor/engine.c`, a new `rule_secret_in_file` added to `g_lint_rules` (line 1867) after `rule_sensitive_source`; catalogue entry in `docs/Doctor.md` after CFD021, in the table at line 422): runs only on a predicted report (a report from a running program has CFD007 for its sources); for each key whose definition is sensitive (`sensitive` member of the report's key entry, as `rule_sensitive_source` reads it: copy its test) or whose name `cf_key_looks_secret` (the D-84 check; find its real name with `grep -rn "D-84" lib/*/include | head -3`) says is secret, and whose winning or any candidate comes from a file (an `at` of `path:line` that is not `argv`/`env`), a finding `error`, id `CFD022`, key, place, and the text `'%s' is a secret and holds a value in '%s'; keep secrets out of files that are checked in (give it by the environment, a secrets directory or a credentials source)`. No value, no digest, no length: the report is value-blind already, and the message never quotes the candidate's text (a test greps for it).
  * **Blind-safe**: yes (it reads presence and origin only), so the column in `docs/Doctor.md`'s table says `yes`; default severity `error`; the rule's profile defaults: none.
  * **D-200** ("A secret written in a checked file is a finding of its own"): the bullets above in one paragraph; Rejected: printing the value; a pattern scanner of values (gitleaks' job: research section 2).
* **files**: `tools/doctor/engine.c`, `docs/Doctor.md`, the Doctor tests, `tests/gen_check/` (a fixture `secret.toml` holding `db.password = "hunter2"`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 3 (the default is (a): an answer is needed only to choose (b)). `git switch -c work/Y-22 main` (after Y-20 and C-8b).
  2. Tests: (a) `gen check --schema S --report r.json secret.toml` then `doctor lint r.json` prints CFD022 for `db.password` with the place `secret.toml:1` and exit non-zero (the exit rule of `doctor lint` for an error finding: check with `ctest -R doctor`'s existing cases); (b) the output does not contain `hunter2` (`grep -c`); (c) a key named `api_token` that the contract does not mark sensitive but D-84's name check catches is reported; (d) the same file's secret given by the environment (a report from a running program, not predicted) is not CFD022 (CFD007 may speak); (e) with `--severity CFD022=warning` it is a warning.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule and the catalogue row. `ctest --preset default -R doctor`.
  5. See that the test can fail: report the value in the message behind a run-time-false condition; (b) fails; put it back.
  6. D-200, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-22: CFD022, a secret written in a checked file (D-200)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: no answer to TO DECIDE 3; C-8b has used a different number for its rule than CFD021 (take the next free number and say so in the commit); a report does not carry which candidate came from which source (the rule cannot tell a file from the environment): report it; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD022: a secret written in a file given to `gen check` (Y-22, D-200)." `docs/Decisions.md`: D-200. `docs/Roadmap.md`: "An analysis tool" 🟩 when Y-23 is done or closed.

---

## Y-23 - Defined but never read, over a set of reports: CFD023 (TO DECIDE)

* **id**: Y-23
* **goal**: `configforge doctor drift a.json b.json ...` (the command that reads a set of reports, `docs/Doctor.md` section 7e) reports CFD023, `info`, for a contract key that no report given read.
* **why**: `docs/research/analysis-tool.md` section 4, Option E (E2), section 5 TO DECIDE 4, section 6 (sixth row). CFD018 is "set but never read"; the other half, a definition the program stopped using, is the contract's dead code. One run cannot prove it (failure mode 12); a set of reports can.
* **needs**: C-8a, C-8b (the reports that record reads)
* **size**: small
* **decided**:
  * **TO DECIDE 4.** A rule over a set of reports, `info` (recommended), or not at all. With "not at all" the card is closed unbuilt.
  * **The rule**: in `tools/doctor/engine.c`, over the set `drift` already reads (find the set command with `grep -n "drift" tools/doctor/doctor.c | head -5`): for each key in the contract of the first report (its `definitions`, i.e. keys with a default or required flag) such that no report in the set holds a read of it (the report member C-8a adds, which records reads; read C-8a's "decided" for the member's name and copy it), a finding `info`, id `CFD023`, key, and the text `'%s' is defined but no report read it (%d report(s) given); a run that never reads it cannot prove it is unused: give reports from every way the program runs`. It never fires with fewer than 3 reports (the text then says `needs at least 3 reports`: one note, no findings: one run cannot prove it, failure mode 12). A report that does not track reads (no member) makes the rule decline with a note `report <n> did not track reads`, no findings.
  * **Blind-safe**: yes; default severity `info`.
  * **D-201** ("Defined but never read needs a set of reports"): the bullets above; Rejected: a finding from one report; a count of reads.
* **files**: `tools/doctor/engine.c`, `tools/doctor/doctor.c`, `docs/Doctor.md` (the table after CFD022 and section 7e), the Doctor tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 4 (the default is the recommendation). `git switch -c work/Y-23 main` (after C-8a, C-8b and Y-22).
  2. Tests with four hand-made reports (small JSON files in `tests/doctor/`): (a) key `old.knob` read in none of 3 reports: one CFD023 `info`; (b) read in one of them: no finding; (c) 2 reports: the `needs at least 3 reports` note and no finding; (d) one report without the reads member: the note and no finding; (e) a key read in all: nothing.
  3. See it fail: `ctest --preset default -R doctor 2>&1 | tail -n 20`.
  4. Write the rule. `ctest --preset default -R doctor`.
  5. See that the test can fail: report with 2 reports behind a run-time-false condition; (c) fails; put it back.
  6. D-201, docs, CHANGELOG, Roadmap. Format; codemap and apiref, last. Commit `Y-23: CFD023, defined but never read, over a set of reports (D-201)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R doctor` exit 0.
* **stop if**: no answer to TO DECIDE 4; C-8a records no reads in a report; `drift` takes exactly two reports and cannot take a set (report: the command needs its own card); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "Doctor's CFD023: a definition that no report in a set read (Y-23, D-201)." `docs/Decisions.md`: D-201. `docs/Roadmap.md`: "An analysis tool" 🟩.

---

## Y-24 - Relations in the contract: `pattern`, `if`/`then`, `dependentRequired` (parked: TO DECIDE 2, when a user brings a relation the recipe cannot hold)

* **id**: Y-24
* **goal**: the contract holds relations between keys ("if `tls.enabled` then `tls.cert` is required", "a value matches a pattern") in JSON Schema's words, checked by the library and by every validator that reads the schema, with a bounded C89 regular-expression engine for `pattern`.
* **why**: `docs/research/analysis-tool.md` section 4, Option D (D2) and section 5, TO DECIDE 2 ("recommended only when a user brings a relation the recipe cannot hold"). Section 6 gives it no version: "after 2.3.0, no row yet".
* **needs**: Y-21, and a user's ask (status: parked, waits on TO DECIDE 2 answered (b))
* **size**: large
* **decided**:
  * **TO DECIDE 2 (relations between keys).** **(a)** none in the library: Rego or CUE over the report as a recipe (Y-21) (recommended first: no dependency, no new interface); **(b)** `pattern`, `if`/`then`, `dependentRequired` in the contract, and a C call for the same: a new public interface and a bounded regular-expression engine in C89 (recommended only when a user brings a relation the recipe cannot hold). **Recommended: (a) now; (b) only on a user's ask.** With (a) this card is closed unbuilt.
  * **Shape if (b)**: the contract's JSON Schema reader (`cf_builder_schema_file`; find with `grep -n cf_builder_schema_file docs/Codemap.md`) reads three more keywords; a bounded regular-expression engine of a subset with no backtracking blow-up (a Thompson-style matcher: literals, classes, `.`, `*`, `+`, `?`, `^`, `$`, alternation, groups; no back-references, no look-around; a step budget of 1,000,000 per match, then a refusal `CF_REASON_BUDGET`), in `lib/text/` or a new `lib/regex/` component (D-100: one directory per component; the maintainer chooses); a C call `cf_builder_relation(b, ...)` whose shape is the maintainer's second TO DECIDE at that time. The steps are written when it is asked for.
* **files**: `lib/regex/` or `lib/text/`, the schema reader, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-202), `docs/Limits.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps from this card's "decided" (tests first, see them fail, the regex engine with a fuzz target and the budget).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; a fuzz hour on the regex engine with no hang.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-202.

---

## Y-25 - The language server (parked: large, 2.3.0, waits on the analysis tool and a user)

* **id**: Y-25
* **goal**: a language server driven by the contract: a key's description, default and range on hover, and, from a report, what overrides it in production.
* **why**: `docs/research/analysis-tool.md` section 5, "Then, in this order" 7 (**F**, the language server, last) and section 6 (last row, 2.3.0, large); the Roadmap's "Later, and large" item (`docs/Roadmap.md:946-948`). The research's reading of the field (yaml-language-server, Taplo, Even Better TOML) is that Y-21's recipes cover completion and hover from the schema already; the server adds what a schema cannot: the report's origins.
* **needs**: Y-19, Y-21; and a user's ask (status: parked)
* **size**: large
* **decided**:
  * **TO DECIDE (whether to build it).** Options: build a server of our own (a C program speaking LSP over stdio, in `tools/lsp/`, with hover from the schema and the report); extend yaml-language-server and Taplo through the schema's `x-` keywords (no server of ours); never. **Recommended: not before Y-21 has been used, and then the second option's cost is measured first**; the maintainer decides. The steps are written when it is asked for.
* **files**: `tools/lsp/` (if built), tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-203), `docs/Tools.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "## 2.3.0 — unreleased", "### Added"; `docs/Decisions.md`: D-203.

---

## Y-26 - `configforge gen build`: sources by rank, a contract, a target, deterministic output (TO DECIDE)

* **id**: Y-26
* **goal**: `configforge gen build --schema S --source <file>=<rank> ... [--runtime env,cli] [--profile P] --target image|report|<format> [--out F]` reads the sources in rank order, builds the configuration through the library, and writes the target (`image` through `cf_image_write`, `report` through `cf_report`, a format name through `cf_emit`) to `--out` or stdout; the same inputs give the same bytes; exit 0 built, 1 a diagnostic of severity error (nothing written), 2 unusable input.
* **why**: `docs/research/config-compiler.md` section 5.1 item 1 and section 4.2 B and C: pieces that exist (`cf_image_write`, `cf_emit`, `cf_report`) become one command, which is the compiler the maintainer asked for. It is also the moment to stop the image freezing defaults (Y-28).
* **needs**: Y-19 (it shares the front half of `gen check`: the same function builds the configuration)
* **size**: medium
* **decided**:
  * **TO DECIDE A (where the compiler lives)**, research section 5.3 A. Options: **(1)** `configforge gen build`, a subcommand beside `gen check` and `gen schema`, with D-104's `gen --bake` spelled as its `--target c` (Y-29); **(2)** a fourth program `configforge build`, which reopens D-99's three programs; **(3)** only `gen --bake` as D-104 wrote it, the image left to C calls. **Recommended: (1).** The steps are (1)'s. With (3) this card and Y-27, Y-28 and Y-30 to Y-33 are closed unbuilt and Y-29 is written as `gen --bake` (its card says how).
  * **TO DECIDE E (the manifest's place)** is Y-31's; **F (defaults in the image)** is Y-28's; **B (a build file)** is Y-34's; **C (diagnostics)** is Y-30's; **D (rules across keys)** is Y-32's.
  * **Where.** A new `tools/gen/build.c` (`int gen_build(int argc, char** argv)`, declared in `tools/gen/gen.h`), listed in `tools/gen/CMakeLists.txt`; `gen.c`'s `main` (line 42-60) gets `if (argc >= 2 && strcmp(argv[1], "build") == 0) return gen_build(argc - 2, argv + 2);` before the `check` branch; `usage()` (gen.c:22-40) gains the `build` line. The front half is shared with `gen_check` (`tools/gen/contract.c:50-100`): the code that makes a builder from a schema and the files moves into one function `static cf_builder* gen_builder(const char* schema, int nsrc, const gen_source* src, cf_error* err)` in `tools/gen/gen.h`'s companion `common.c` (new; both commands call it), a change of structure with no behaviour change: `ctest -R gen_check` (Y-19's tests) must pass unchanged after step 4.
  * **Options.** `--schema <file>` once (required). `--source <file>=<rank>` repeatable: the rank is a name of `cf_define.h:345-348`'s ranks (`sys-file`, `user-file`, `env`, `cli`, ...: read the enum and accept exactly its names with `-` for `_`, lowercase; an unknown name is exit 2, `unknown rank '<x>'; ranks are <list>`); sources are applied in the order of their rank (the library does that: the command takes them in the order given). `--runtime <ranks>` (comma-separated, from the same names): the ranks the program will still have at run time, used only by Y-27's checks; accepted and stored here, `unused` until Y-27. `--profile <name>`: passed to `cf_report_info.profile` and, when the contract has profile-dependent definitions, to the builder as the profile (find the builder call with `grep -n "cf_builder_profile" lib/config/include/configforge/*.h`; if there is none the option only labels the report). `--target <t>`: `image`, `report`, or the name of a codec that writes (`cf_codec_by_name`, with `CF_CODEC_WRITE`); anything else is exit 2, `unknown target '<t>'; targets are image, report and the formats that can be written: <list>`. `--out <file>` (a path, or `-` for stdout; the default is stdout for `report` and a format and a refusal for `image` without `--out`: `an image is binary; give --out`). Written whole or not at all (`cf_platform_write_file`, F-1a).
  * **Targets**: `image` calls `cf_image_write(ks, sv, flags=0, buf, cap)` measure-then-write (`cf_image.h:54`; the key space and source view come from the built configuration: find the accessor with `grep -n "cf_config_keyspace\|cf_config_view" lib/config/include/configforge/*.h`; if none is public, the tool uses the internal tier (`CF_INTERNAL_API`) as `tools/doctor` does for reports); `report` calls `cf_report` with `CF_REPORT_BLIND`; a format calls `cf_emit(c, format, CF_EMIT_ALL, 0, ...)` as `cf_emit_file` does.
  * **Deterministic**: no clock, no host name, no environment read for the output (a `--source` of kind `env` is not offered: environment is a run-time rank, `--runtime env`). The report's `generated` is NULL. Two runs give identical bytes: a test compares them.
  * **Exit codes** as `gen check`: 0; 1 when the build refuses (`cf_builder_create` returns NULL: the first error is printed as `<file>:<line>:<col>: <message>` the way `gen check` prints it, `printf("%s\n", err.message)`), nothing written; 2 unusable input (a schema or file unreadable, a bad option, an unwritable `--out`).
  * **D-204** ("The compiler is `gen build`, and it is deterministic"): TO DECIDE A's (1) with the shape above; the targets are those that exist; `--target c` is Y-29's. Rejected: a fourth program; a configuration language of our own (research 4.2 A; [38]); text templates (section 3, item 4); unification in place of ranks (it removes the override every program needs from its command line); planning against live state; an input-addressed store; a plug-in protocol for targets beyond D-110's.
* **files**: `tools/gen/build.c` (new), `tools/gen/common.c` (new), `tools/gen/gen.c`, `tools/gen/gen.h`, `tools/gen/contract.c`, `tools/gen/CMakeLists.txt`, `tests/gen_build/*` (new fixtures), `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE A (the default is (1): an answer is needed only to choose (2) or (3)). `git switch -c work/Y-26 main` (after Y-19).
  2. Fixtures: Y-19's schema; `defaults.toml` (all keys), `site.toml` (overrides `server.port`), `bad.toml` (`server.port = 70000`).
  3. Tests: (a) `gen build --schema S --source defaults.toml=sys-file --source site.toml=user-file --target toml --out out.toml` exits 0 and `out.toml` holds `port = 9000` from site (the override wins by rank, in either order of the options); (b) `--target report` writes a blind report that `doctor lint` accepts and that names `site.toml` for `server.port`; (c) `--target image --out out.cfgb` then a C test program (`tests/gen_build/load_image.c`, built into the suite) opens it with `cf_image_open` and reads `server.port` = 9000, and `explain` names the source when the image was written with provenance (Y-28 adds the flag; here only the value); (d) two runs, `cmp` equal, for each of the three targets; (e) `bad.toml` exits 1 with `server.port` in the message and no output file; (f) an unknown rank, an unknown target, a missing `--schema`, `--target image` with no `--out`: each exit 2 with its message; (g) the compiled image's values equal those of a configuration built by `cf_build` with the same files in the same program (`load_image.c` builds both and compares every key: "compared with layer 0").
  4. Move the common front half; run `ctest --preset default -R gen_check` unchanged. See the new tests fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20` (no `build` subcommand).
  5. Write `build.c`. `ctest --preset default -R "gen_build|gen_check|image"`.
  6. See that the test can fail: let a later source lose by order of options behind a run-time-false condition; (a) fails; put it back.
  7. D-204, `docs/Tools.md`, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-26: configforge gen build (D-204)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gen_build|gen_check"` exit 0.
* **stop if**: no answer to TO DECIDE A; the key space and source view are not reachable by the tool without a new public function (report which: it is a new public interface and the maintainer's); `gen` links only part of the library and cannot reach `cf_image_write` (report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`configforge gen build`: sources by rank and a contract to an image, a report or a written format, deterministically (Y-26, D-204)." `docs/Decisions.md`: D-204. `docs/Roadmap.md`: "A configuration compiler" under "From the maintainer, 2026-11-26" stays 🟨 until Y-33.

---

## Y-27 - The open-key checks: what the compiler can prove about the keys left to run time

* **id**: Y-27
* **goal**: `gen build --runtime <ranks>` computes, for each key, which run-time ranks may still set it, and refuses (exit 1) a required key no source supplies and no run-time rank may supply, and a template whose referenced key is open; it lists the keys no run-time rank may change as sealed.
* **why**: `docs/research/config-compiler.md` section 4.2 E and section 6, second row: the compiler proves the contract can be met, which no other tool the research names does for a layered configuration.
* **needs**: Y-26
* **size**: small
* **decided**:
  * **Where**: `tools/gen/build.c`, a function `gen_open_keys(const cf_defset*, const char* runtime_ranks, diag*)` after the build. The inputs: each definition's `policy` (`cf_def.policy`, `lib/config/include/configforge/cf_define.h:345-348`: which ranks may set the key) and the rank names Y-26 parsed from `--runtime`.
  * **The three checks** (research 4.2 E): (1) a key that is required, has no value from the build-time sources, and whose policy allows none of the `--runtime` ranks: error `'<key>' is required, no source gives it and no run-time source (<ranks>) may: <key> can never be set`; (2) a template (`${other.key}` interpolation, as the library's reference feature) whose referenced key is open (no build-time value and a run-time rank may set it): error naming both keys, `'<key>' refers to '<other>', which is set at run time; a template cannot be computed at build time`; (3) a key whose build-time value no run-time rank may change: sealed, listed by Y-31's manifest and by `--target report`'s note `sealed: <key>` (a `notes` member of the report? no: the list goes only to the manifest and, without it, to stderr as `sealed: <key>` lines when `--verbose` is given; the option `--verbose` is added here).
  * Without `--runtime` none of the checks run (nothing is known to be open): a note `no --runtime given; open keys not checked` on stderr. Each check's severity is error; Y-30 turns them into `cf_diag` records.
  * **D-205** ("The compiler proves what the contract can be met with"): the three checks, the sealed list, and that they use the policy and the rank names only. Rejected: running the program; guessing ranks.
* **files**: `tools/gen/build.c`, `tests/gen_build/`, `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-27 main` (after Y-26).
  2. Fixtures: a contract with `server.port` required and policy allowing `cli` and `env` only; `tls.cert` required with policy `user-file` only; a template `url = "http://${server.host}:${server.port}"`.
  3. Tests: (a) `--runtime env,cli` with `server.port` not in the files: passes check (1); (b) `--runtime env` and `server.port` policy `cli` only: exit 1, message as above; (c) the template over the open `server.port`: exit 1 naming both keys; (d) the template over a key set in a file: passes; (e) no `--runtime`: the note and no check; (f) `--verbose` lists the sealed keys.
  4. See it fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20`.
  5. Write the checks. `ctest --preset default -R gen_build`.
  6. See that the test can fail: skip check (1) behind a run-time-false condition; (b) fails; put it back.
  7. D-205, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-27: gen build proves the open keys (D-205)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R gen_build` exit 0.
* **stop if**: the policy of a definition cannot be read by the tool (it is not in the public `cf_defset` accessors: report the accessor needed); the library has no templates (then check (2) is dropped and the card says so under notes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`gen build --runtime`: required keys proved to be settable, templates over open keys refused, sealed keys listed (Y-27, D-205)." `docs/Decisions.md`: D-205.

---

## Y-28 - `CF_IMAGE_CONFIGURED`: an image that holds only what a source configured, and defaults and secrets out unless asked

* **id**: Y-28
* **goal**: `cf_image_write` takes a new flag `CF_IMAGE_CONFIGURED` that writes only keys a source set (no defaults); `gen build --target image` leaves defaults out unless `--with-defaults` and refuses a `CF_SENSITIVE` value in the artifact unless `--allow-secrets`; the C call with the old flags writes what it always wrote.
* **why**: `docs/research/config-compiler.md` section 4.2 F and section 5.3 F: an artifact must not freeze defaults the next release of the program changes, and a full report or image is a credential-bearing file.
* **needs**: Y-26, and TO DECIDE F below
* **size**: small
* **decided**:
  * **TO DECIDE F (defaults in a compiled image).** (1) left out unless asked; (2) written, as today. **Recommended: (1)** for the command; the C call keeps today's behaviour unless the new flag is given, so 1.x callers see no change. With (2) the command writes defaults and `--with-defaults` is not offered; the flag still exists.
  * **The flag**: `cf_image.h:44-49`: `CF_IMAGE_CONFIGURED = 1u << 1`, "only what a source set: no defaults". An added enum value; the version of the format does not change (the image carries fewer keys). `cf_image_write` (`lib/config/src/*/image.c`: find with `grep -n "cf_image_write" lib/config/src/*/*.c`) skips a key whose winning candidate's source is the contract's default (the `CF_TRANSIENT`-like test `emit.c:36`'s `keep` uses for `CF_EMIT_CONFIGURED`: copy its test, do not write a second one).
  * **Secrets**: `gen build` refuses to write a `CF_SENSITIVE` key's value into the image, a report with `CF_REPORT_FULL`, or an emitted document, unless `--allow-secrets`: exit 1, `'<key>' is sensitive and would be written to the output; pass --allow-secrets to write it`. A blind report never holds a value, so it is not refused.
  * **D-206** ("An image keeps only what was configured, unless asked"): the flag, the two command options and their defaults. Rejected: stripping defaults silently from the existing flags.
* **files**: `lib/config/include/configforge/cf_image.h`, the image writer, `tools/gen/build.c`, `lib/config/tests/test_image.c` (the image test file: `ls lib/config/tests | grep image`), `tests/gen_build/`, `docs/ImageFormat.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE F (the default is (1)). `git switch -c work/Y-28 main` (after Y-26).
  2. Tests: in `test_image.c` (a) an image written with `CF_IMAGE_CONFIGURED` holds no key whose only candidate is a default and holds each configured key, and an image written with flags 0 is byte-identical to the one before this card (a golden: take it from `git stash`-free means: write the image on main first and commit it as `tests/data/image-v1.cfgb` as the first commit of the card); (b) `gen build --target image` without `--with-defaults` writes the configured image; with it, the full one; (c) a sensitive key set in a file and `--target toml`: exit 1 with the message; with `--allow-secrets` exit 0.
  3. See it fail: build `test_image` (the flag is undeclared) and `ctest -R gen_build`.
  4. Write the flag and the options. `ctest --preset default -R "test_image|gen_build"`.
  5. See that the test can fail: ignore the flag behind a run-time-false condition; (a) fails; put it back.
  6. D-206, `docs/ImageFormat.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-28: CF_IMAGE_CONFIGURED, defaults and secrets out of an artifact unless asked (D-206)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_image|gen_build"` exit 0; the golden image is unchanged by the card.
* **stop if**: a default's candidate cannot be told from a configured one in the key space (report: the key space needs a marker); an image test golden changes; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`CF_IMAGE_CONFIGURED`; `gen build` leaves defaults out of an image and refuses secrets unless asked (Y-28, D-206)." `docs/Decisions.md`: D-206.

---

## Y-29 - The baked configuration: `gen build --target c` and `cf_config_from_baked`

* **id**: Y-29
* **goal**: `configforge gen build --schema S --source ... --target c --out device_cfg.c` resolves a configuration at build time and writes it as C89 `static const` tables with provenance ("from device.toml line 12, at build time"); a program that links only that file and the baked reader reads the same values and gets the same `explain` text as layer 0, with no parser linked.
* **why**: `docs/research/config-compiler.md` section 4.2 C (baked C tables, "designed (D-104)", L) and section 6, fourth row; D-104, layer 1 for C (`docs/plan/A-architecture.md:258-270`): "no parser in the binary ... the field docs/Market.md found open". The card is not written in any other group's file (searched `docs/plan/*.md` for `cf_config_from_baked`: only the architecture page names it), so it is here, as the research's own list says it belongs to 1.9.0.
* **needs**: Y-26, Y-28
* **size**: large
* **decided**:
  * **The target**: `--target c` in `gen_build` (Y-26's target list gains `c`; the unknown-target message lists it). The output is one C89 file: a header comment with the schema's name, the sources and their digests, `static const cf_baked_entry <name>_entries[]` sorted by key (for binary search by the reader), and `const cf_baked <name> = { sizeof(cf_baked), 1, <count>, <name>_entries, <name>_strings }`; `--name <name>` (default `baked_config`) is the C identifier prefix, checked as a C identifier (exit 2 otherwise).
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
    `cf_config_from_baked()` of D-104's text is these functions (one reader, `get` and `explain` over the tables): the name is kept in D-207 as an alias note, not as a function: **TO DECIDE (the public shape)**: options: **(a)** the five functions above (a baked configuration is its own small type); **(b)** `cf_config_from_baked(const cf_baked*)` returning a `cf_config*` that works with the whole existing API (needs the config core, so the program links layer 0, which defeats "no reader linked"); **(c)** both, (b) as a `lib/config` convenience. **Recommended: (a)** (it is what "links no reader at all" needs; D-104 names the function as an idea, not a signature), (b) later if a program wants the full API over baked data.
  * **No parser linked**: `cf_baked.c` (new, `lib/baked/` as a component of its own, D-100, built as `libconfigforge_baked.a`, depends on `base` only: `lib/baked/CMakeLists.txt`; `gate.layering` is told it may include `cf_base` and nothing else: one line in the layering table, found with `grep -rn "serialize" scripts/work/layering* cmake/*layer* | head -3`) is C89, allocates nothing, calls nothing from `lib/serialize`. A test links a program against `libconfigforge_baked.a` only and checks with `nm` that no `cf_codec_`/`json`/`toml` symbol is in the link (`gate.baked_nolink`, an `add_test` running `nm --defined-only` over the test binary; skipped without `nm`).
  * **Compared with layer 0** (the test of the card): for every key of the example contract, `cf_baked_find` and `cf_config_get` agree on kind and value; `cf_baked_explain` and `cf_explain` agree on the winning source name and the origin, up to the suffix.
  * **Deterministic output**: no date; entries sorted; the same inputs give the same file. **Secrets**: refused unless `--allow-secrets` (Y-28's rule): a `CF_SENSITIVE` key's value is never baked by default; the entry is written with kind `CF_BAKED_ABSENT` and the origin kept, so `explain` says it was set and withheld.
  * **Arrays and lists**: a key with `.count` and indexed keys (`list.0`, `list.1`) is baked as separate entries, as the key space holds them; a key kind with no baked form (a duration or size, which `cf_value` may carry) is baked as its integer in the unit the value holds, with the kind it has. If the key space holds a kind this list does not name, stop (`stop if`).
  * **D-207** ("A baked configuration is a small type with its own reader, written by `gen build --target c`"): the target, the five functions, the no-parser rule, provenance at build time, secrets withheld. Rejected: a parser in the baked reader; C23 `#embed` (D-104's other kit, a separate thing: read at start-up).
* **files**: `tools/gen/build.c`, `tools/gen/baked.c` (new: the C emitter), `lib/baked/` (new: `CMakeLists.txt`, `include/configforge/cf_baked.h`, `src/baked.c`, `tests/CMakeLists.txt`, `tests/test_baked.c`), `CMakeLists.txt` (add the subdirectory, found by `grep -n add_subdirectory CMakeLists.txt`), `cmake/configforge.symbols`, `scripts/work/` layering table (named above), `tests/gen_build/baked_main.c` (new), `docs/Guide.md` (a section "A configuration baked into the program", after section 12 "Small targets"), `docs/Design.md` (one paragraph under D-104's layer 1); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to the TO DECIDE (the default is (a)). `git switch -c work/Y-29 main` (after Y-26 and Y-28).
  2. Write `lib/baked/tests/test_baked.c`, C89, with a hand-written `cf_baked` table of five entries (one of each kind, one with origin NULL): `cf_baked_find` hits and misses, the three getters, a getter of the wrong kind returns `CF_FALSE`, `cf_baked_explain` measure-or-write, a NULL table is refused.
  3. See it fail: `cmake --build --preset default --target test_baked 2>&1 | tail -n 20` (no component).
  4. Write the component, the symbols, the layering line. `ctest --preset default -R "test_baked|gate" 2>&1 | tail -n 30`.
  5. In `tests/gen_build/` write the end-to-end test: `gen build ... --target c --out baked_cfg.c`; `baked_main.c` includes the generated file, prints each key; a CMake `add_test` builds it with the generated file (`add_custom_command` from the `gen` target) and compares with `cf_config_get` over the same files in a second program (layer 0): the two programs' outputs are `diff`ed by the test. `gate.baked_nolink`.
  6. See it fail: run before the emitter exists (`ctest -R gen_build_baked`); then write `tools/gen/baked.c`; it passes.
  7. See that the test can fail: bake a value off by one behind a run-time-false condition in the emitter; the diff fails; put it back.
  8. D-207, Guide, Design paragraph, CHANGELOG. Format; codemap and apiref, last. Commit `Y-29: the baked configuration, gen build --target c and cf_baked (D-207)`.
  9. ACCEPT; push; ACCEPT-CI (the embedded targets' jobs, if they exist yet, build the baked example).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_baked|gen_build|gate.baked_nolink"` exit 0; `ctest --preset minimal -R test_baked` exit 0.
* **stop if**: no answer to the TO DECIDE; the key space holds a kind of value this card does not name; `gate.layering` or `gate.symbols` cannot take a new component without a rule this card does not write; the generated file is not valid C89 under `-std=c89 -pedantic` (a gate: add it to the test); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Added": "`gen build --target c` and `cf_baked`: a configuration resolved when the program is built, read with no parser linked (Y-29, D-207)." `docs/Decisions.md`: D-207. `docs/Roadmap.md`: the item "A baked configuration" (found by `grep -n "baked" docs/Roadmap.md`, line 1581 area) 🟩.

---

## Y-30 - `cf_builder_diagnostics`: every diagnostic with its place, and `gen build --json` and `gen check --all` (TO DECIDE)

* **id**: Y-30
* **goal**: a builder option `cf_builder_diagnostics(b, fn, user)` keeps going past the first refusal and hands every diagnostic (severity, code, reason, key, source, place, message) to `fn` during `cf_builder_create`, which still fails when any was an error; `gen build --json` and `gen check --all` print the whole list, shaped like `terraform validate -json`.
* **why**: `docs/research/config-compiler.md` section 4.2 D and section 5.1 item 2: a compiler needs the whole list, not the first error; the program itself can then list every problem at start-up. It is where the 1.8.0 row's "one diagnostic model" leads.
* **needs**: Y-26, and TO DECIDE C below answered
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
  * **D-208** ("Every diagnostic, by callback, experimental"): TO DECIDE C's (1), the shape, and that a failed build with a callback still fails.
* **files**: `lib/config/include/configforge/cf_diag.h` (new), `lib/config/src/core/config.c`, `cmake/configforge.symbols`, `tools/gen/build.c`, `tools/gen/contract.c`, `lib/config/tests/test_diag.c` (new), `tests/gen_build/`, bindings: none (experimental tier, not wrapped); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Tools.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE C (the default is (1)). `git switch -c work/Y-30 main` (after Y-26).
  2. `test_diag.c`: a builder with a schema and three bad files (a value out of range, an unknown key, an unreadable optional-not file): (a) without the callback `cf_builder_create` returns NULL with the first error only; (b) with it the callback is called three times, severities error, each with its key and `file:line` place; (c) the build still returns NULL; (d) a sensitive key's bad value does not appear in any message; (e) with all files good the callback is never called and the build succeeds; (f) a warning-only build (a deprecated key set) calls it with `CF_SEV_WARNING` and succeeds.
  3. See it fail: `cmake --build --preset default --target test_diag 2>&1 | tail -n 20`.
  4. Write the header, the builder option and the continuing. `ctest --preset default -R "test_diag|test_config|test_define|gate"`: every existing result unchanged.
  5. See that the test can fail: stop after the first diagnostic behind a run-time-false condition; (b) fails; put it back.
  6. `gen build --json` and `gen check --all`, with their tests (a golden JSON in `tests/gen_build/`).
  7. D-208, docs, CHANGELOG. Format; codemap and apiref, last. Commit `Y-30: cf_builder_diagnostics, every diagnostic with its place (D-208)`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_diag|gen_build"` exit 0; every `test_config*` result unchanged.
* **stop if**: no answer to TO DECIDE C; continuing past a refusal leaves the builder in a state that a later step reads wrongly (a crash or a wrong second diagnostic: report the case); the 1.8.0 "one diagnostic model" card changed `cf_error` so that `code`/`reason` differ from this card's fields (the card then follows the model's names, and says so under notes); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`cf_builder_diagnostics` (experimental): every diagnostic of a build with its place; `gen build --json` and `gen check --all` print them (Y-30, D-208)." `docs/Decisions.md`: D-208.

---

## Y-31 - The manifest: inputs and their digests, the contract's digest, version, profile, open keys, the output's digest

* **id**: Y-31
* **goal**: `gen build --manifest <file>` writes a small JSON beside the artifact: each input file and its SHA-256, the contract's SHA-256, the library's version, the profile, the open (run-time) and sealed keys, the output's SHA-256 and `SOURCE_DATE_EPOCH` when it is set; the same inputs give the same manifest.
* **why**: `docs/research/config-compiler.md` section 4.2 H and section 6 (sixth row): a build can say what it was made from, as a reproducible build does.
* **needs**: Y-26, Y-27, and M-34 (SHA-256 as a digest, D-184: "one digest a purpose")
* **size**: small
* **decided**:
  * **TO DECIDE E (the manifest's place)**, research section 5.3 E. Options: **(1)** a JSON file beside the artifact; **(2)** a section of the image. **Recommended: (1) now, (2) with 2.0.0's image header (Y-35).** The steps are (1)'s.
  * **Shape**: `{"manifest_version": 1, "configforge": "<cf_version()>", "profile": "<p>"|null, "inputs": [{"path": "<as given>", "rank": "<name>", "sha256": "<hex>"}], "contract": {"path": "<schema>", "sha256": "<hex>"}, "open": ["<key>"], "sealed": ["<key>"], "output": {"target": "<t>", "path": "<out>", "sha256": "<hex>"}, "source_date_epoch": <n>|null}`, keys in this order, lists sorted (inputs in the order given, then none re-sorted; `open`, `sealed` sorted), two-space indentation, a final newline. Paths are written as given (no absolute path is added: a build in another directory differs only by what the user typed). `SOURCE_DATE_EPOCH` is read once from the environment, the only environment variable the command reads; no other clock.
  * **SHA-256** from M-34's interface (`cf_sha256` or what D-184 names: read `docs/plan/M-maintainer.md` around line 736 for the function; if M-34 is not landed, this card does not open).
  * **D-209** ("The manifest is a JSON file beside the artifact"): the shape above. Rejected: the manifest inside the image now; a timestamp; absolute paths.
* **files**: `tools/gen/build.c`, `tools/gen/manifest.c` (new), `tests/gen_build/`, `docs/Tools.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-31 main` (after Y-27 and M-34).
  2. Tests: (a) the manifest of Y-26's fixture has the six members in order; (b) the `sha256` of each input equals `sha256sum` of the file (the test runs `sha256sum` when present, else compares against digests written into the test); (c) two runs are byte-identical; (d) with `SOURCE_DATE_EPOCH=1700000000` the member is that number, without it `null`; (e) `sealed` lists the keys Y-27 computes; (f) the `output.sha256` equals that of the artifact.
  3. See it fail: `ctest --preset default -R gen_build 2>&1 | tail -n 20`.
  4. Write `manifest.c` and the option. `ctest --preset default -R gen_build`.
  5. See that the test can fail: add the time of day to the manifest behind a run-time-false condition; (c) fails; put it back.
  6. D-209, `docs/Tools.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-31: the manifest of a build (D-209)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R gen_build` exit 0.
* **stop if**: M-34 is not landed; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "`gen build --manifest`: inputs and their SHA-256, the contract's digest, open and sealed keys, the output's digest (Y-31, D-209)." `docs/Decisions.md`: D-209.

---

## Y-32 - Rules across keys in the contract: a fixed declarative set (parked: TO DECIDE D, with Y-24)

* **id**: Y-32
* **goal**: the contract can say, with a fixed set of declarative rules and no expression language, that one key requires another (`dependentRequired`), that a key's value is at least another's (`minimum` by key), or that one of a set of keys is given (`oneOf`); the library checks them at run time, and therefore the compiler at build time.
* **why**: `docs/research/config-compiler.md` section 4.2 G (option 2), section 5.3 D and section 6 (seventh row): a compiler that cannot check "if `tls.enabled` then `tls.cert`" proves less than the program does at start-up.
* **needs**: Y-26; and TO DECIDE D answered with the analysis tool's TO DECIDE 2 (Y-24)
* **size**: medium
* **decided**:
  * **TO DECIDE D (rules across keys).** (1) none, recipes for OPA and CUE (Y-21, Y-33); (2) a fixed declarative set in the contract; (3) an expression language. **Recommended by this research: (2), decided with the analysis-tool research**, whose own TO DECIDE 2 recommends (a) none now and (b) only on a user's ask: the two documents ask the same question, so the maintainer answers once, for both. This card is (2)'s declarative set without a regular-expression engine (that is Y-24's `pattern`); if the answer is (1), this card and Y-24 are closed unbuilt; (3) is not offered.
  * **The set**: JSON Schema's `dependentRequired` and `if`/`then` over `const` and `enum` tests (not over patterns), plus `x-configforge-atleast: {"key": "pool.max", "other": "pool.min"}` for "at least another key" (JSON Schema has no relation between two values; the `x-` prefix keeps the schema valid for every other validator, which ignore it). Names and a call for the C contract (`cf_def` side: `cf_builder_relation(b, kind, key, other)`, experimental tier) are written when the answer is given; the card's steps follow the answer.
  * Run by the library when the configuration is built (so a program refuses at start-up what the compiler refused at build time), each violation a diagnostic with both keys and the place of the winning value of each.
  * **D-210** ("Relations between keys are a fixed declarative set"), written by the card when it opens.
* **files**: the schema reader and the contract's defset (`lib/config/src/core/`), `tools/gen/build.c`, tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md`.
* **steps**:
  1. Wait for the answer to TO DECIDE D; the opening cold read writes the steps from this card's "decided" on the pattern of Y-27's (the test first, see it fail, the rule, the negative proof).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no answer; the answer is (1) or (3).
* **docs**: when built: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added"; `docs/Decisions.md`: D-210.

---

## Y-33 - Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel and KCL as front ends

* **id**: Y-33
* **goal**: the Guide has a page "A language in front of ConfigForge" showing, for each of CUE, Pkl, Jsonnet, Dhall, Nickel and KCL, the command that exports its result to JSON or YAML and the ConfigForge command that takes it from there (`gen build --source out.json=user-file ...`); the JSON/YAML-producing ones are run by a test when the tool is on the PATH.
* **why**: `docs/research/config-compiler.md` section 4.2 J and section 5.1 item 4: the library has no configuration language and builds none (research 4.2 A); these languages are used in front of it.
* **needs**: Y-26
* **size**: small (documentation)
* **decided**:
  * **Where**: `docs/Guide.md`, a new section after Y-21's subsection; examples in `examples/frontends/` (new): `app.cue`, `app.pkl`, `app.jsonnet`, `app.dhall`, `app.ncl`, `app.k`, each defining the same three keys (`server.port`, `log.level`, `db.host`) and each with the export command in a comment line: `cue export app.cue`, `pkl eval -f json app.pkl`, `jsonnet app.jsonnet`, `dhall-to-json --file app.dhall`, `nickel export app.ncl`, `kcl run app.k --format json`. Each command's flags are checked against the tool's own `--help` where installed; where not, the Guide says `as the tool's documentation says` and links its page (source numbers in research section 7).
  * **Test**: `ctest -R frontends`: for each tool on the PATH, run its export into `fe_<tool>.json`, then `gen build --schema S --source fe_<tool>.json=user-file --target toml` and compare with one golden; the rest print `skipped: <tool> not found` and pass.
  * **What the Guide says it does not do**: no templates (research section 3, item 4), no language of our own, and that unification in these languages is theirs, not a rank of ours.
  * No decision number: documentation.
* **files**: `docs/Guide.md`, `examples/frontends/*` (new), the test wiring in `tests/CMakeLists.txt`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-33 main` (after Y-26).
  2. Write the harness and the golden first (the six files' outputs reduce to one JSON).
  3. See it fail: `ctest --preset default -R frontends 2>&1 | tail -n 20` (no such test).
  4. Write the six examples and the Guide section; run each tool that is installed by hand and copy the real command lines into the Guide.
  5. See that the test can fail: change a value in `app.jsonnet`; the comparison fails when `jsonnet` is installed (else this step is done on one tool that is: say which); put it back.
  6. CHANGELOG. Format; codemap and apiref, last. Commit `Y-33: recipes for CUE, Pkl, Jsonnet, Dhall, Nickel and KCL in front of gen build`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R frontends` exit 0 (skips count).
* **stop if**: none of the six tools can be run on any machine at hand (the recipes would be unrun: write which are unrun under notes and mark them so in the Guide); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide page and `examples/frontends/` on putting CUE, Pkl, Jsonnet, Dhall, Nickel or KCL in front of `gen build` (Y-33)." `docs/Roadmap.md`: "A configuration compiler" 🟩 when Y-29 to Y-31 are done.

---

## Y-34 - A build file read with the tool's own contract (parked: when asked)

* **id**: Y-34
* **goal**: `configforge gen build --file build.toml` reads the sources, ranks, runtime ranks, profile and targets from a file that is itself read by the library with a contract of the tool's own (so it has the same diagnostics, places and formats as any configuration), instead of from the command line.
* **why**: `docs/research/config-compiler.md` section 4.2 I and section 5.3 B (**recommended: (3), both, the file later**) and section 6 (`when asked`): the command line comes first (Y-26); a build file is for a repository with many targets.
* **needs**: Y-26, Y-31; and a user's ask (status: parked)
* **size**: medium
* **decided**:
  * **TO DECIDE B (how the build is described).** (1) the command line; (2) a build file read by the library with the tool's own contract; (3) both, the file later. **Recommended: (3)**: Y-26 is (1); this card is the file, built only when asked.
  * **Shape if built**: the tool's own contract, in `tools/gen/build.schema.json` (committed, and used by `gen build --file` to read its own input through `cf_builder_schema_file`): `schema` (path), `runtime` (list), `profile`, `[[source]]` (path, rank), `[[target]]` (name, kind, out, `with_defaults`, `allow_secrets`, `manifest`); one `gen build --file b.toml` builds every target; relative paths are relative to the file. Any format the library reads works for the build file. The steps are written when it is asked for.
* **files**: `tools/gen/build.c`, `tools/gen/build.schema.json`, tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-211), `docs/Tools.md`.
* **steps**:
  1. Wait for the ask; the opening cold read writes the steps on Y-26's pattern.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI.
* **stop if**: no ask has come.
* **docs**: when built: `CHANGELOG.md`, "## 1.10.0 — unreleased" or the open version, "### Added"; `docs/Decisions.md`: D-211.

---

## Y-35 - The manifest inside the image (2.0.0, with the image header's layout)

* **id**: Y-35
* **goal**: the image format version 1.1 (`CF_IMAGE_MINOR` 1) carries the manifest of Y-31 as a section, so an artifact says what it was made from without a file beside it.
* **why**: `docs/research/config-compiler.md` section 5.3 E (2) and section 6 (last row, 2.0.0, with the image header): the one place a manifest cannot be lost from its image.
* **needs**: Y-31 and the image header's layout (wave 8, version 2.0.0; the card that fixes it is in `docs/plan/M-maintainer.md`: find it with `grep -n "image header" docs/plan/M-maintainer.md | head`)
* **size**: small
* **decided**:
  * **Shape**: a new section kind in the image (`docs/ImageFormat.md`: the section table), `CF_IMAGE_SECTION_MANIFEST`, holding the manifest's JSON bytes as Y-31 writes them (the image's own digest excludes it, so the output digest in the manifest is of the image without the section: written in `ImageFormat.md`); `CF_IMAGE_MINOR` rises from 0 to 1 (`cf_image.h:42`); a reader of minor 0 ignores a section it does not know (check the reader's rule in `ImageFormat.md`; if it refuses, the card stops: that is a break and 2.0.0's list's). `cf_image_manifest(const cf_image*, const char** json, size_t* n)` reads it (a new public name, experimental tier); `gen build --manifest-in-image` writes it.
  * **D-212** ("The manifest may live in the image"): the section, the minor, the digest rule.
* **files**: `lib/config/include/configforge/cf_image.h`, the image reader and writer, `tools/gen/build.c`, `docs/ImageFormat.md`, tests; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-35 main` (after Y-31 and the image header card).
  2. Tests: (a) an image written with the manifest section reads back its JSON byte for byte; (b) an image of minor 0 (the golden of Y-28) opens unchanged; (c) a reader built to the minor-0 rules opens the minor-1 image (the section is skipped); (d) the output digest in the manifest equals the SHA-256 of the image without the section.
  3. See it fail: build the image test (`cf_image_manifest` undeclared).
  4. Write the section, the minor and the call. `ctest --preset default -R "image|gen_build"`.
  5. See that the test can fail: hash the whole image behind a run-time-false condition; (d) fails; put it back.
  6. D-212, `docs/ImageFormat.md`, CHANGELOG. Format; codemap and apiref, last. Commit `Y-35: the manifest inside the image (D-212)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "image|gen_build"` exit 0; the minor-0 golden unchanged.
* **stop if**: the reader of minor 0 refuses an unknown section; the image header card has not fixed the section table; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.0.0 — unreleased", "### Added": "The image format 1.1 carries a build's manifest; `cf_image_manifest` (Y-35, D-212)." `docs/Decisions.md`: D-212.

---

## Y-36 - Close-on-exec and non-inheritable, everywhere the library opens something

* **id**: Y-36
* **goal**: every descriptor and handle the library opens (files it reads or writes, the watcher's inotify and kqueue descriptors, the private-create open) is closed on `exec` on POSIX and not inheritable on Windows, and a test that `exec`s a child shows the child holds none of the library's descriptors.
* **why**: `docs/research/threads-and-processes.md` section 5, "First" point 2 (section 4, D, first bullet): a program that starts a child leaks its open configuration files and watcher descriptors into it, which for a secrets directory is a leak of a secret's file.
* **needs**: nothing
* **size**: small
* **decided**:
  * **POSIX opens, by name**: (1) `cf_platform_fopen` (`lib/platform/src/posix/file.c:17-19`: `fopen(path, mode)`): becomes `open(path, flags | O_CLOEXEC)` then `fdopen(fd, mode)`, where `flags` come from the mode (`r`/`rb`: `O_RDONLY`; `w`/`wb`: `O_WRONLY|O_CREAT|O_TRUNC` mode `0666`; `a`/`ab`: `O_WRONLY|O_CREAT|O_APPEND`; a `+` anywhere in the mode: `O_RDWR` instead of the first); any other mode string goes to plain `fopen` as today. Where `O_CLOEXEC` is not defined (`#ifndef O_CLOEXEC`: old systems), `fopen` and then `fcntl(fileno(f), F_SETFD, FD_CLOEXEC)` (a race the comment names: a thread that forks between the two). (2) `lib/platform/src/posix/posix.c:487` (`open(path, O_WRONLY | O_CREAT | O_EXCL, 0600)`, the private create): add `| O_CLOEXEC` under `#ifdef O_CLOEXEC`, as lines 1020-1021 already do. (3) the watcher: `inotify_init1(IN_NONBLOCK | IN_CLOEXEC)` at 839 already is; `kqueue()` at 1181 gets `fcntl(w->fd, F_SETFD, FD_CLOEXEC)` straight after (macOS and the BSDs have no `kqueue1`), and each `open(path, ...)` the kqueue watcher makes for `EVFILT_VNODE` has `O_CLOEXEC` (line 1028 has the flag; check the other opens with `grep -n "open(" lib/platform/src/posix/posix.c`: every one without `O_CLOEXEC` gets it).
  * **Windows**: `win32/file.c:34` `cf_platform_fopen` appends `N` to the mode for `_wfopen` (MSVC's "not inherited" flag; with MinGW's CRT `N` is accepted too); the handles `CreateFileW` makes in `win32/file.c` and `win32.c` pass `SECURITY_ATTRIBUTES` with `bInheritHandle` FALSE (the default when the pointer is NULL: check each call passes NULL or FALSE; none passes TRUE).
  * **The test** (`lib/config/tests/test_cloexec.c`, new): POSIX only (`#if CF_PLATFORM_POSIX`): the test builds a configuration from a file and a watcher (`cf_live` with `CF_LIVE_AUTO` or the platform watcher directly), keeps them open, then `fork`s and in the child `execl`s itself (`argv[0]` with `--list-fds`), which lists `/proc/self/fd` on Linux (`opendir`, `readlink`) or tries `fcntl(fd, F_GETFD)` for fd 3 to 255 elsewhere and prints the numbers that are open; the parent reads the child's output through a pipe (made with `O_CLOEXEC` on the parent's end only and `dup2` in the child) and fails when any descriptor above 2 other than the pipe's is listed. Windows: skipped, with the line `cloexec: not tested on Windows`.
  * **D-213** ("Nothing the library opens is inherited by a child"): the list above and the reason (a secret's file must not reach a child); the program that wants a descriptor in a child opens it itself. Rejected: closing every descriptor above 2 in the library (it is the program's to do between `fork` and `exec`).
* **files**: `lib/platform/src/posix/file.c`, `lib/platform/src/posix/posix.c`, `lib/platform/src/win32/file.c`, `lib/platform/src/win32/win32.c`, `lib/config/tests/test_cloexec.c` (new), `lib/config/tests/CMakeLists.txt` (after `test_writefile` or `test_file`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-36 main`.
  2. Write `test_cloexec.c` as decided (it has two roles through `argv`).
  3. See it fail: `cmake --build --preset default --target test_cloexec && ctest --preset default -R test_cloexec --output-on-failure 2>&1 | tail -n 20` (the file descriptors of the `fopen`'d file and the kqueue/inotify leak: on Linux the plain `fopen` descriptor is listed).
  4. Make the changes. `ctest --preset default -R "test_cloexec|test_file|test_live|gate" 2>&1 | tail -n 30`.
  5. See that the test can fail: drop `O_CLOEXEC` from the `fopen` replacement behind a run-time-false condition (`if (path[0] == '\x7f')`, never a constant); the test fails; put it back.
  6. D-213, CHANGELOG. `python scripts/work/work.py format`; codemap and apiref, last. Commit `Y-36: nothing the library opens is inherited by a child (D-213)`.
  7. ACCEPT; push; ACCEPT-CI (macOS and FreeBSD prove the kqueue path).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the test says it is skipped); ACCEPT-CI; `ctest --preset default -R test_cloexec` exit 0.
* **stop if**: `fdopen` is not available in a C89 platform file under a strict `-std=c89` build (it is POSIX: the platform layer may use POSIX calls under its guard; stop only if `gate.layering` refuses it); the test cannot tell its own pipe from a leak (report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Fixed": "Files and watcher descriptors the library opens are closed on `exec` and not inherited on Windows (Y-36, D-213)." `docs/Decisions.md`: D-213. Roadmap: Y-41 marks "threads and processes".

---

## Y-37 - A second `cf_live` driver refused, and D-48's lock sentence corrected

* **id**: Y-37
* **goal**: a second thread that calls `cf_live_poll`, `cf_live_reload`, `cf_live_wait`, `cf_live_approve`, `cf_live_rollback` or `cf_live_commit` while another is inside one of them gets `CF_LIVE_FAILED` with `err` set to `CF_E_POLICY`, `another thread is driving this cf_live; every call but cf_live_get and cf_live_generation belongs to one thread at a time`, and nothing in the live object changes; a test runs two drivers and sees one refused.
* **why**: `docs/research/threads-and-processes.md` section 5, "First" point 3, and `docs/research/concurrent-access.md` section 5 ("A guard on `cf_live`'s driving calls (T2): left to the research of Threads and processes made easier"). Today two drivers are outside the contract and nothing detects it (`lib/config/include/configforge/cf_live.h:69-73`).
* **needs**: nothing (Y-1 corrects the same D-48 sentence; if Y-1 has landed, step 5 below finds it done)
* **size**: small
* **decided**:
  * **The guard**: a member `cf_refcount driving;` in `struct cf_live` (`lib/config/src/core/live.c:22`, with the driving thread's own members), zero at open. `static cf_bool live_enter(cf_live* l, cf_error* err)`: `if (cf_ref_inc(&l->driving) != 1) { (void)cf_ref_dec(&l->driving); cf_error_set(err, CF_E_POLICY, NULL, NULL, "another thread is driving this cf_live; every call but cf_live_get and cf_live_generation belongs to one thread at a time"); return CF_FALSE; }` (the port's names: `cf_ref_inc`, `cf_ref_get` are used at `live.c:86-93`; the decrement is the port's `cf_ref_dec`: find it with `grep -n "cf_ref_dec" lib/port/include/configforge/cf_port.h`; use what it is called); `live_leave(l)` is `(void)cf_ref_dec(&l->driving)`. The six public calls (`cf_live_poll` 753, `cf_live_reload` 792, `cf_live_wait` 803, `cf_live_approve` 814, `cf_live_rollback` 839, `cf_live_commit` 861) call `live_enter` first (returning `CF_LIVE_FAILED` when it fails) and `live_leave` on every return path (each body is wrapped: the old body becomes a `static cf_live_result live_<name>_locked(...)` and the public function is `enter; r = locked(...); leave; return r;`). `cf_live_get`, `cf_live_generation`, `cf_live_status_get` and `cf_live_restarts` do not enter (they are the any-thread calls, under the ticket lock).
  * **A `CF_SINGLE_THREADED` build** (`cf_port.h:319-345`): the port's refcount is plain; the guard is a re-entrance guard there (a callback that calls back into a driving call is refused too: `cf_live_swap_fn` of Y-41 must not do it, and now cannot).
  * **A check callback or build function that calls a driving call** is refused by the same guard (`live_enter` fails: the caller is on the driving thread but inside a call): written in the page.
  * **D-48's sentence** (`docs/Decisions.md`, in the D-48 text at 2173-2200, the line that says the lock is "a count taken from 0 to 1" at about 2194-2196): replaced by the ticket-lock wording Y-1 uses; and a sentence is added: "A second driver is refused (Y-37, D-214)."
  * **D-214** ("A second driver of a `cf_live` is refused, not undefined"): the six calls, the one atomic, the message, the result. Rejected: a mutex that makes a second driver wait (it would hide a design fault and make a reload wait behind another thread's file reads); detecting by thread id (no portable thread id in C89).
* **files**: `lib/config/src/core/live.c`, `lib/config/include/configforge/cf_live.h` (the comment at 60-73: "the others belong to one thread at a time ... a second is refused with CF_E_POLICY"), `lib/config/tests/test_live_threads.c` (new; or the existing live test file: `ls lib/config/tests | grep live`), `docs/Decisions.md`, `docs/Guide.md` (Y-1's subsection: the refusal replaces "nothing detects it"); last: `CHANGELOG.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/Y-37 main`.
  2. Test (`test_live_threads.c`, guarded by `CF_HAVE_THREADS`-like macro the existing live tests use: find it with `grep -n "SINGLE_THREADED\|pthread" lib/config/tests/*.c | head -3`; a build without threads prints `skipped: no threads`): (a) a `build` callback that blocks on a flag the test controls; thread A calls `cf_live_reload` (blocked inside the build); thread B calls `cf_live_poll` and gets `CF_LIVE_FAILED` with the message above; release A; A returns `CF_LIVE_SWAPPED`; then B's next call works (the guard was released); (b) a `check` callback that calls `cf_live_poll` on the same `cf_live` gets `CF_LIVE_FAILED`; (c) `cf_live_get` and `cf_live_generation` work from a third thread while A is blocked; (d) after a driving call that returns `CF_LIVE_FAILED` from a failed build the guard is released (a following call is accepted).
  3. See it fail: `cmake --build --preset default --target test_live_threads && ctest --preset default -R test_live_threads 2>&1 | tail -n 20` (B is not refused: today it runs the build concurrently: the test may hang or fail; run it with a 20 s timeout `ctest --timeout 20`).
  4. Write the guard and the wrapping. `ctest --preset default -R "test_live|test_live_threads|reload"`.
  5. See that the test can fail: have `live_enter` accept always behind a run-time-false condition; (a) fails; put it back. Under ThreadSanitizer: `cmake --preset sanitize` has TSan? (`grep -n thread CMakePresets.json`): if it does, run the test there too.
  6. D-214, the D-48 sentence (skip if Y-1 did it: `grep -n "a count taken from 0 to 1" docs/Decisions.md` finds nothing), the header comment, the Guide, CHANGELOG. Format; codemap and apiref, last. Commit `Y-37: a second cf_live driver is refused (D-214)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "test_live"` exit 0 in `default` and, if the preset exists, under TSan.
* **stop if**: a live example (`examples/c/live.c`, `reload.c`) calls a driving call from two threads on purpose (report: the example is wrong by the contract); a binding's wrapper drives from two threads (report); `cf_ref_dec` does not exist in the port (use `cf_ref_inc` with a negative step if the port has one; else report); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Changed": "A second thread driving a `cf_live` at once gets `CF_LIVE_FAILED` with `CF_E_POLICY` instead of undefined behaviour (Y-37, D-214)." `docs/Decisions.md`: D-214 and the D-48 sentence. Roadmap: Y-41.

---

## Y-38 - Threads and processes in the Guide: four recipes, compiled in CI, with a `fork` test

* **id**: Y-38
* **goal**: the Guide has a page "Threads and processes" with four recipes on today's interface, each an example built by CI: a request handler that takes one snapshot per request; a `SIGHUP` handler that sets a flag and a loop that calls `cf_live_reload`; `fork` after building, and what the child may not touch; an image in a private file handed to an `exec`ed child; and the `fork` recipe has a test that a child reads a snapshot taken before it.
* **why**: `docs/research/threads-and-processes.md` section 5, "First" point 1 and section 6, first row. The maintainer asked for threads and processes to be made easier (section 1); the first answer is to say how it is done with what exists.
* **needs**: Y-36, Y-37 (the page says close-on-exec holds and a second driver is refused)
* **size**: medium
* **decided**:
  * **Where**: `docs/Guide.md`: a new section "15. Threads and processes" after "14. Threads" (heading at 1671; Y-1's subsection ends section 14), and section 14's first paragraph gains one sentence pointing to it. Examples in `examples/c/`: `threads_request.c`, `signal_reload.c`, `fork_child.c`, `image_child.c` (all new), each listed in `examples/c/CMakeLists.txt` with `add_test` (`example.<name>`), POSIX only where the call needs it (`#if CF_PLATFORM_POSIX` at the top and a `main` that prints `skipped: needs POSIX` otherwise).
  * **Recipe 1, one snapshot per request** (`threads_request.c`): the program builds a configuration, a `cf_live` in `CF_LIVE_MANUAL`; four worker threads (`pthread`, guarded) each loop 1000 requests, each request calling `cf_live_get(live)`, reading three keys from the one snapshot, `cf_config_free`; the main thread calls `cf_live_reload` 20 times with a file changed between. The output is a count of requests that saw a mixed snapshot (a snapshot whose `a` and `b` values, written together by the test as a pair, disagree): the example prints `mixed snapshots: 0` and the test requires it.
  * **Recipe 2, SIGHUP** (`signal_reload.c`): `sigaction` with a handler that sets a `volatile sig_atomic_t` only; the main loop sees the flag and calls `cf_live_reload`; the test sends `SIGHUP` to itself (`raise`) after rewriting the file and sees the new value. The page says: never call library functions in a signal handler (the library is not async-signal-safe; D-14).
  * **Recipe 3, `fork` after building** (`fork_child.c`): the parent builds, takes a snapshot `c`, `fork`s; the child reads `c` (the copy-on-write pages) and exits with the value as status; the parent reads it back with `waitpid` and the test requires equality. The page lists what the child may not touch (research section 4, C1, items 7 to 9): the parent's `cf_live` (its watcher descriptors and its ticket lock are the parent's; a reload in the child is refused in the page: "never reload in a forked child"), a `cf_builder` in use, any lock held by another thread at `fork` (the child sees it held forever); the safe pattern is to build, then fork, and have the child only read.
  * **Recipe 4, an image to an exec'd child** (`image_child.c`): the parent writes a private image file (`cf_image_write` to a file created with `cf_platform_create_private` or `mkstemp` `0600`, unlinked after the child maps it), execs itself with `--child <path>`; the child `cf_image_open`s it and prints a key. The test requires the child's output. The page names the cost (one copy) and that this is C2 of the research; a shared mapping is Y-42 and waits for a request.
  * **Each recipe's page text** is the example's code in full (not a fragment) with three lines of why; the page ends with a table "what to use when": threads in one process (recipe 1), many processes started by one (3 and 4), a reload on a signal (2), many programs reading one file (Y-1's page).
  * No decision number: documentation.
* **files**: `docs/Guide.md`, `examples/c/threads_request.c`, `examples/c/signal_reload.c`, `examples/c/fork_child.c`, `examples/c/image_child.c`, `examples/c/CMakeLists.txt`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/Y-38 main` (after Y-36, Y-37).
  2. Write the four `add_test` lines first, each with its `PASS_REGULAR_EXPRESSION` (`mixed snapshots: 0`, `reloaded: new`, `child read: 8080`, `child key: 8080`).
  3. See them fail: `ctest --preset default -R "example.(threads|signal|fork|image)" 2>&1 | tail -n 20` (the examples do not exist).
  4. Write each example (the simplest first: recipe 3, then 4, 2, 1) and make each test pass.
  5. See that a test can fail: in `threads_request.c` have the writer replace half a pair behind a run-time-false condition; the count is non-zero and the test fails; put it back. Run recipe 1 under ThreadSanitizer when the preset has it.
  6. Write the Guide section from the examples' code. `gate.doc` and the link gate pass.
  7. CHANGELOG. Format; codemap and apiref, last. Commit `Y-38: Guide, Threads and processes: four recipes built by CI`.
  8. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS (the POSIX examples print `skipped`: stated in the commit); ACCEPT-CI; `ctest --preset default -R "example.(threads|signal|fork|image)"` exit 0.
* **stop if**: a recipe cannot be written on today's interface without a library change (report which: the page then says so and the change is a card); `pthread` is not linkable in a preset that builds examples (guard by `Threads::Threads` found); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.10.0 — unreleased", "### Added": "A Guide section 'Threads and processes' and four examples built by CI: a snapshot per request, a reload on SIGHUP, `fork` after building, an image handed to a child (Y-38)." Roadmap: Y-41 marks "threads and processes".

---

## Y-39 - `cf_live_get` and `cf_config_free` measured under threads; `cf_live_generation` without the lock (TO DECIDE)

* **id**: Y-39
* **goal**: the benchmark's page has a table of `cf_live_get` plus `cf_config_free` per second on 1, 2, 4, ... threads up to twice the cores; `cf_live_generation` reads an atomic epoch without taking the ticket lock; the page says from the numbers whether the read path has a knee.
* **why**: `docs/research/threads-and-processes.md` section 5, "First" point 4 ("Measure before changing the read path") and TO DECIDE 1 ((b) now). If the numbers show no knee, `cf_live_reader` (Y-40) waits.
* **needs**: nothing (Y-37 changes `live.c` too: whichever lands second rebases)
* **size**: medium
* **decided**:
  * **TO DECIDE 1 (a new public interface for the read path)**, research section 5, TO DECIDE 1. (a) None: A1 (`cf_live_get` as it is), with the measurement on the benchmark's page. (b) A5 only: `cf_live_generation` lock-free, no new name. (c) A2: `cf_live_reader` as sketched (Y-40), A5 included. **Recommended: (b) now, as part of the measurement card; (c) when the measurement shows a knee or a binding asks for it.** Either way (c) is a borrowed pointer, which D-03 weighed; the maintainer says whether a borrow bounded by the worker's own next call is acceptable where one bounded by another thread's reload was not (that is Y-40's question). This card is (b); with (a) the generation change is not made.
  * **The measurement**: a new bench program `bench/threads.c` (in `bench/CMakeLists.txt`, built only when `CF_BUILD_BENCH`: find the option with `grep -n "option(CF_BUILD_BENCH\|add_executable" bench/CMakeLists.txt | head`), POSIX `pthread` and Windows `_beginthreadex`; for each thread count `t` in 1, 2, 4, ... up to `2 * cores` (`cf_platform_cpu_count` if it exists; else `sysconf(_SC_NPROCESSORS_ONLN)`, and 4 when unknown): `t` threads each call `cf_live_get(live)` then `cf_config_free(c)` in a tight loop for 2 seconds (a `cf_live` of a configuration of 100 keys); a driver thread calls `cf_live_reload` every 10 ms (two builds alternating, so swaps happen); output `threads,ops_per_second_total,ops_per_second_per_thread,swaps`. A "knee" is defined in the page: the first `t` at which per-thread throughput falls below half of the one-thread figure.
  * **Where the numbers go**: `docs/Performance.md`, a new section "The read path under threads" after "The libraries" (line 424): the machine (from "The machine", 403), the table, the knee or "no knee up to <n> threads", and the conclusion in one sentence (which of TO DECIDE 1's options the numbers support). The CI bench job does not run it (it is a scaling measurement, not an instruction count: `bench gate` is unchanged); the numbers are written by hand from a run on a named machine (CLAUDE.md: never from a laptop for a baseline, but this is a table of measurement not a gate, so the machine is named).
  * **`cf_live_generation` lock-free** (`lib/config/src/core/live.c:910-920`): the generation is read without the lock: the epoch is a `cf_refcount`-like atomic `cf_i32`? It is a `cf_u64` (`live.c:22-40`): the port has no 64-bit atomic in C89. Decision: the epoch is kept as two atomic 32-bit words `gen_lo`, `gen_hi` written under the lock in a seqlock pattern (`seq` incremented to odd, words written, `seq` to even, using the port's atomics) and read by `cf_live_generation` as: read `seq`, words, `seq` again, retry while odd or changed. The `cf_u64 generation` under the lock stays for the status. If the port has no atomic load of a plain word (check `cf_ref_get`'s implementation: it is an atomic load of the refcount's word), use `cf_ref_get` for each of the three words. A test (`test_live_threads.c` of Y-37, or a file of this card) hammers `cf_live_generation` from 3 threads while the driver swaps 10,000 times and checks that every value read is a value written (monotone per reader, never a torn mixture: the generations are written as `n * 2^32 + n`, so a torn read has `lo != hi` mod 2^32 pattern the test can see).
  * **D-215** ("The generation is read without the lock"): the seqlock, why (the generation is read by status pages and workers polling for a change, and must not contend with `cf_live_get`), what is not changed (`cf_live_get` keeps the ticket lock: D-48). Rejected: a 64-bit atomic (no C89 form); a lock-free `cf_live_get` (that is Y-40's question).
* **files**: `bench/threads.c` (new), `bench/CMakeLists.txt`, `lib/config/src/core/live.c`, `lib/config/tests/test_live_threads.c`, `docs/Performance.md`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if "Organiser's answers" holds no answer to TO DECIDE 1 (the default is (b)). `git switch -c work/Y-39 main`.
  2. Write the torn-read test (above) first; it passes on the unchanged lock-based function, which is its oracle.
  3. Write `bench/threads.c`; run it on the build host and on a second machine with more cores if one is at hand: `cmake --preset bench` (find the preset name with `grep -n bench CMakePresets.json | head -3`), `build/bench/threads`.
  4. See the test fail on a wrong seqlock: first write `cf_live_generation` without the retry loop (a plain read of the two words) and see the torn-read test fail on a 3-thread run (it may need up to 10 runs: loop it `for i in 1 2 3 4 5 6 7 8 9 10; do ctest -R live_threads || break; done`); then write the retry loop and see it pass.
  5. Write the table and conclusion in `docs/Performance.md` from the numbers measured.
  6. D-215, CHANGELOG. Format; codemap and apiref, last. Commit `Y-39: the read path measured under threads; cf_live_generation without the lock (D-215)`.
  7. ACCEPT; push; ACCEPT-CI (the TSan job, if present, runs the torn-read test).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live_threads` exit 0; `docs/Performance.md` has the table.
* **stop if**: no answer to TO DECIDE 1; the port has no atomic load usable for the words (report: the generation stays locked and the card keeps only the measurement); the measurement cannot be run on any machine at hand (the card stays open); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 1.9.0 — unreleased", "### Changed": "`cf_live_generation` no longer takes the lock; the read path is measured under threads on the benchmark's page (Y-39, D-215)." `docs/Decisions.md`: D-215. Roadmap: Y-41.

---

## Y-40 - `cf_live_reader`: a worker's handle on the current snapshot (TO DECIDE, only if Y-39's numbers or a binding ask)

* **id**: Y-40
* **goal**: `cf_live_reader_open`, `_get`, `_generation`, `_release` and `_close` exist: a worker opens a handle once and reads the current snapshot, borrowed until its next call, writing no shared memory when no swap happened; C++ has a non-copyable `live::reader`, Rust a `LocalReader` that is `Send` and not `Sync`; its test runs under ThreadSanitizer.
* **why**: `docs/research/threads-and-processes.md` section 5, "After" point 5 (A2, with A5 inside it) and TO DECIDE 1 (c). It answers the maintainer's "threads made easier": a worker that reads a thousand times a second should not take a lock and a reference count each time.
* **needs**: Y-39, Y-37; and TO DECIDE 1 answered (c) (status: parked until Y-39's table shows a knee or a binding asks)
* **size**: medium
* **decided**:
  * **TO DECIDE 1 (c)**, with the question the research asks the maintainer: is a borrow bounded by the worker's own next call acceptable where one bounded by another thread's reload was not (D-03)? **Recommended: yes, when the numbers show a knee or a binding asks.** If no, this card is closed unbuilt and the page says to use `cf_live_get` per request.
  * **The interface** (`lib/config/include/configforge/cf_live.h`, experimental tier, after `cf_live_generation` at line 234; `CF_NODISCARD` as the neighbours), as research section 5, item 5:
    ```c
    typedef struct cf_live_reader cf_live_reader;
    CF_NODISCARD CF_API cf_live_reader* cf_live_reader_open(cf_live* l, cf_error* err);
    CF_API void cf_live_reader_close(cf_live_reader* r);
    CF_API const struct cf_config* cf_live_reader_get(cf_live_reader* r);
    CF_API cf_u64 cf_live_reader_generation(const cf_live_reader* r);
    CF_API void cf_live_reader_release(cf_live_reader* r);
    ```
    `cf_live_reader_get`'s result is valid until the next call on this reader, `cf_live_reader_release` or `cf_live_reader_close`; when no swap happened since the last call no shared memory is written (it reads the epoch of Y-39 and compares with the one it holds). One thread at a time per reader; any number of readers; a reader must be closed before `cf_live_close` (closing the live object with a reader open is refused: a count of open readers in `struct cf_live`; `cf_live_close` returns after releasing nothing and sets `err` `CF_E_POLICY`, `<n> reader(s) still open`, if the project's close has an error channel: find `cf_live_close` in `live.c` and follow its signature; if it returns void, the refusal is a log line at error level and the live object is not freed: say so in the header).
  * **How**: each reader holds a reference to a snapshot (`cf_config_ref`) and the generation it came from; `_get` compares `cf_live_generation`'s lock-free epoch with its own, and only when it moved takes the ticket lock (one `cf_live_get`), releases the old reference and keeps the new one. The old snapshot is therefore freed by the reader that held it last: a reader that never calls again keeps one snapshot alive (the page says: call `_release` when a worker goes idle).
  * **Bindings**: C++ `live::reader` (move-only, in `lib/cxx/` or `bindings/cpp/`: find with `grep -rn "cf_live_get" bindings lib/cxx | head -3`) wrapping the five calls; Rust `LocalReader` (`bindings/rust/`) that is `Send` and not `Sync` (`PhantomData<*const ()>` as the project's other handles do, or `impl !Sync` via a `Cell`); Python: nothing (the GIL makes it moot; a free-threaded build is its own card).
  * **Test under ThreadSanitizer**: `lib/config/tests/test_live_reader.c`: 4 reader threads each open a reader and read 100,000 times while a driver swaps 1,000 times; every value read is a complete snapshot (the paired-keys test of Y-38 recipe 1); the preset `sanitize` or a `tsan` preset (`grep -n -i "tsan\|thread" CMakePresets.json`) runs it; no report.
  * **D-216** ("A worker's reader handle: a borrow bounded by the worker's own next call"): the five calls, the borrow's bound, the generation comparison, the close rule. Rejected: implicit thread-local caching (D-03's option 3: stays rejected; the reader is explicit, owned by the worker, and freed by no one else); liburcu or any RCU dependency (LGPL, thread registration, Linux first, for a section two instructions long); hazard pointers (the same handle with more code).
* **files**: `lib/config/include/configforge/cf_live.h`, `lib/config/src/core/live.c`, `cmake/configforge.symbols`, the C++ and Rust binding files, `lib/config/tests/test_live_reader.c` (new), `docs/Guide.md` (section 15: a recipe 5); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here unless TO DECIDE 1 is answered (c) in "Organiser's answers". `git switch -c work/Y-40 main` (after Y-39).
  2. Write `test_live_reader.c`; see it fail (undeclared names).
  3. Write the reader. `ctest --preset default -R "test_live_reader|test_live"` and under TSan.
  4. See that the test can fail: have `_get` return without checking the generation behind a run-time-false condition; the "latest value" check of the test (after a swap the next `_get` sees the new snapshot) fails; put it back.
  5. The two bindings with a test each; D-216, Guide recipe, CHANGELOG. Format; codemap and apiref, last. Commit `Y-40: cf_live_reader, a worker's handle on the current snapshot (D-216)`.
  6. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live_reader` exit 0, and under ThreadSanitizer with no report.
* **stop if**: TO DECIDE 1 is not (c); Y-39 is not landed; the experimental tier forbids a new handle type without a gate update this card does not name; three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.4.0 — unreleased", "### Added": "`cf_live_reader` (experimental) and the C++ and Rust readers over it (Y-40, D-216)." `docs/Decisions.md`: D-216. Roadmap: Y-41.

---

## Y-41 - `swapped` in `cf_live_options`: workers told of a swap (TO DECIDE)

* **id**: Y-41
* **goal**: `cf_live_options` has `swapped` and `swapped_user`: a callback called on the driving thread, inside `poll`, `reload`, `wait`, `approve`, `rollback` and `commit`, after readers can get the new snapshot, with the generation and both snapshots, for posting to the program's own workers.
* **why**: `docs/research/threads-and-processes.md` section 5, "After" point 6 (B2) and TO DECIDE 2; it pairs with 2.2.0's "a reload that names the keys it changed" (the callback gets both snapshots, and that card's diff can be asked of them).
* **needs**: Y-37 (the guard makes "no call back into `l`" enforceable)
* **size**: small
* **decided**:
  * **TO DECIDE 2 (workers told of a swap)**, research section 5. (a) Nothing: workers poll the generation (Y-39 makes that cheap). (b) `swapped` in `cf_live_options`. (c) A descriptor per reader. **Recommended: (b), with 2.2.0's changed keys.** With (a) the card is closed unbuilt; (c) is not offered (a descriptor per reader is a thread's worth of machinery the library refuses: D-35, D-48).
  * **The names** (`cf_live.h`, `cf_live_options` at about line 128, size-versioned by `size` as D-51 says; `CF_LIVE_OPTIONS_INIT` gains two NULLs at its end):
    ```c
    /* Called on the driving thread, inside poll, reload, wait, approve, rollback and commit,
       after readers can get `next`. `prev` may be NULL. For posting to the program's own
       workers; it must not call back into `l` except cf_live_get and cf_live_generation. */
    typedef void (*cf_live_swap_fn)(void* user, cf_u64 generation,
                                    const struct cf_config* prev, const struct cf_config* next);
    /* in cf_live_options, after `settle`: */
    cf_live_swap_fn swapped;
    void*           swapped_user;
    ```
    `cf_live_open_with` reads them only when `o->size` covers them (the project's pattern; copy the way `settle` is read).
  * **Where it is called**: in `live_swap` (`live.c:314-350`) after the pointer is replaced and the ticket lock released, once per swap, never during a rollback's swap-back?: a rollback is a swap (readers get a snapshot), so it calls it too, with `prev` the rejected one. The callback runs with the driving guard of Y-37 held, so a call back into a driving function is refused by it (documented).
  * **A callback that blocks** blocks the driver: the page says so (it is the program's own post; use a queue).
  * **D-217** ("Workers are told of a swap by a callback on the driving thread"): the names, where it is called, the rule. Rejected: a thread of the library's own that posts; a descriptor per reader; calling the callback under the lock.
* **files**: `lib/config/include/configforge/cf_live.h`, `lib/config/src/core/live.c`, `lib/config/tests/test_live.c` (the live test: `ls lib/config/tests | grep live`), bindings: none (a function pointer; Python and Rust wrappers expose it in the same card only if the project wraps `settle`: `grep -rn "settle" bindings | head -3`; copy what they do for `check`); last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Guide.md`, `docs/Roadmap.md`, `docs/Codemap.md`, `docs/API.md` (generated).
* **steps**:
  1. Stop here and say so if there is no answer to TO DECIDE 2 (the default is (b)). `git switch -c work/Y-41 main` (after Y-37).
  2. Tests in `test_live.c`: (a) a reload that swaps calls `swapped` once with `generation` equal to `cf_live_generation` and `prev != NULL`, `next` the snapshot `cf_live_get` now returns; (b) the first build (open) is not a swap (no call: `prev` would be NULL; decide: it is called once at open with `prev == NULL`? No: open calls nothing, the callback is for changes; `prev` may be NULL only when a rollback has nothing before it: written in the header as "may be NULL"); (c) a reload that finds nothing changed calls nothing; (d) a failed build calls nothing; (e) a rollback calls it with `prev` the rejected snapshot; (f) the callback calls `cf_live_poll`: refused (`CF_LIVE_FAILED`, Y-37's message), no deadlock; (g) an `o->size` from before this card (a struct of the old size) works with `swapped` unset.
  3. See it fail: `ctest --preset default -R test_live 2>&1 | tail -n 20` (the members do not exist).
  4. Write the members and the call. `ctest --preset default -R "test_live|reload"`.
  5. See that the test can fail: skip the call behind a run-time-false condition; (a) fails; put it back.
  6. D-217, the Guide (section 15: a line on it), CHANGELOG, the Roadmap row (🟩 "Threads and processes made easier" when Y-36 to Y-39 and this card are done or closed). Format; codemap and apiref, last. Commit `Y-41: a callback when a cf_live swaps (D-217)`.
  7. ACCEPT; push; ACCEPT-CI.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R test_live` exit 0.
* **stop if**: no answer to TO DECIDE 2; Y-37 is not landed; the ABI gate (`gate.abi` or `abi-compliance`) flags the options struct growth as a break (it must not: `size`-versioned); three failures of the same step.
* **docs**: `CHANGELOG.md`, "## 2.2.0 — unreleased", "### Added": "`swapped` in `cf_live_options`: a callback on the driving thread when readers get a new snapshot (Y-41, D-217)." `docs/Decisions.md`: D-217. `docs/Roadmap.md`: "Threads and processes made easier" under "From the maintainer, 2026-11-26" 🟩.

---

## Y-42 - A shared image across processes: `cf_image_share` (parked: TO DECIDE 3, only on a program's request)

* **id**: Y-42
* **goal**: a platform capability writes an image into an anonymous, read-only shared object and maps one it is given, with the seals checked on Linux and FreeBSD and a read-only descriptor or handle elsewhere, and `cf_image_share_create`, `_map`, `_close` and a source over the mapped share exist, behind `CF_WITH_IMAGE_SHARE`.
* **why**: `docs/research/threads-and-processes.md` section 5, "Later, if asked: a shared image (C3)" and TO DECIDE 3: many processes read one copy of a configuration in memory. Not before a program asks: recipe 4 of Y-38 does the same job with one copy, and a configuration is kilobytes (`docs/ImageFormat.md`, "Why it exists").
* **needs**: Y-38; and a program's request (status: parked)
* **size**: large
* **decided**:
  * **TO DECIDE 3 (a shared image across processes).** (a) Documentation only (C1 and C2: Y-38's recipes 3 and 4). (b) The platform capability and public type of the sketch below, behind a build option `CF_WITH_IMAGE_SHARE` (off in `minimal`, `misra` and the embedded profiles; on in `server`). **Recommended: (a) now; (b) only on a program's request.** With (a) this card is closed unbuilt. **TO DECIDE 4 (does an image carry the contract)**, research section 5: today it does not, so a child must be built with the same definitions; carrying them is a new image section and a minor version (`ImageFormat.md`: "fields only grow") and belongs to the compiler's research, not this one. **Recommended: no; the child's own contract decides.**
  * **The sketch** (experimental tier, `lib/config/include/configforge/cf_image_share.h`):
    ```c
    typedef struct cf_image_share {
        size_t size; long handle; const char* base; size_t n; void* reserved[4];
    } cf_image_share;
    CF_NODISCARD CF_API cf_bool cf_image_share_create(const struct cf_config* c, unsigned flags, cf_image_share* out, cf_error* err);
    CF_NODISCARD CF_API cf_bool cf_image_share_map(cf_image_share* s, long handle, cf_error* err);
    CF_API void cf_image_share_close(cf_image_share* s);
    CF_API cf_bool cf_source_image_share(cf_source* src, const char* name, int rank, const cf_image_share* s, void* state, size_t state_size);
    ```
  * **Mechanisms**: Linux `memfd_create` with `MFD_ALLOW_SEALING`, sealed `F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE | F_SEAL_SEAL` before the handle is handed out; `_map` refuses a descriptor whose `F_GET_SEALS` lack them; FreeBSD `shm_create_largepage`-free `memfd_create` (FreeBSD 13+) the same; other POSIX: `shm_open` of an unlinked name `0400`, mapped `PROT_READ`; Windows `CreateFileMappingW` with `PAGE_READONLY` after a `PAGE_READWRITE` fill and a duplicated read-only handle. Where none exists: `CF_E_CAPABILITY`, `this platform has no shared image; hand the child an image file (Y-38, recipe 4)`.
  * The steps are written when a program asks, on Y-36's and Y-12's pattern (platform capability, test with a child process, a negative proof, a gate for the layering).
* **files**: `lib/platform/` (the capability), `lib/config/` (the type and the source), tests; last: `CHANGELOG.md`, `docs/Decisions.md` (D-218), `docs/Guide.md`, `docs/ImageFormat.md`.
* **steps**:
  1. Wait for the request; the opening cold read writes the steps from this card's "decided".
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI (macOS and FreeBSD prove their paths).
* **stop if**: no request has come.
* **docs**: when built: `CHANGELOG.md`, "### Added" of the version then open; `docs/Decisions.md`: D-218.

