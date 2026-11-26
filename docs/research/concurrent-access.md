# Reads and writes at the same time, guarded by choice

Research for the Roadmap item of that name ("From the maintainer,
2026-11-26", `docs/Roadmap.md:1555-1562`). It is not a card. It says what
the library promises today, what other libraries and systems do, what
breaks, and what fits this library, and it ends with the cards it implies
and the choices that are the maintainer's.

Line numbers are those of the tree at e4fff61.

---

## 1. The question

### As the maintainer asked it

> "In the end, is there protection if read and write operations happen at
> the same time? I think it could depend on an option." (Turkish,
> translated.)

There are three questions in it, and they have different answers:

1. **Threads of one program.** One thread reads the configuration while
   another reloads it, or derives a changed one, or writes a value back.
2. **Processes and one file.** Two programs, or two copies of one
   program, write the same configuration file at once; or one writes
   while another reads.
3. **A reader and a half-written file.** A program reads a file while
   something (the library, an editor, a deployment tool) is writing it.

### As it stands in the library today

**Threads.** The contract is D-14 (`docs/Decisions.md:705-735`), written
into the public header (`lib/config/include/configforge/configforge.h:702-715`):

```text
Reads are safe concurrently. Building and releasing are the caller's to
synchronise.
```

A built configuration is an immutable snapshot, reference counted. A
change is a new snapshot (`cf_derive`, `docs/Decisions.md:1570-1579`), so
a thread reading the old one never sees it move. There is no lock on the
read path, by decision.

Reloads go through `cf_live` (`lib/config/include/configforge/cf_live.h:15-73`):

* `cf_live_get` may be called from any thread. It takes a reference to the
  current snapshot under a ticket lock built from the port's atomics
  (`lib/config/src/core/live.c:76-94`, `cf_live_get` at `live.c:740-751`).
  The lock covers a pointer copy and a reference count increment, nothing
  else.
* `live_swap` (`live.c:314-350`) replaces the pointer under the same lock;
  the old snapshot is released, and a reader that still holds it keeps it
  until its own `cf_config_free`.
* Every other `cf_live_*` call "belongs to one thread at a time, the one
  that drives the reloads" (`cf_live.h:69-73`). Two threads that both
  call `cf_live_poll` or `cf_live_reload` at once are outside the
  contract, and nothing detects it.
* A `CF_SINGLE_THREADED` build has no atomics and one thread
  (`lib/port/include/configforge/cf_port.h:319-345`).

One stale line: D-14's sequel (`docs/Decisions.md:2194-2196`) says the
lock is "a count taken from 0 to 1". The code replaced that with a ticket
lock after the one-count lock livelocked under tcc (`live.c:78-85`). The
decision text should say so.

So within one process, reads during a reload are already safe, without
any option: a reader holds a whole snapshot, old or new, never a mixture.

**Writes.** `cf_write` (`lib/config/src/core/config.c:2041-2090`) checks
the source's capability, the program's grant and the contract, then calls
the source's `write`. It takes no lock. Today one source can write: the
SQLite store (`lib/config/src/bridge/store.c`). Its write opens the
database, runs `BEGIN IMMEDIATE` and commits (`store.c:228-231`, `287`).
SQLite's own locks keep two writers from corrupting the store, but no busy
timeout is set anywhere in `lib/` (no `sqlite3_busy_timeout`, no
`PRAGMA busy_timeout`): a second writer that finds the store locked fails
at once with `SQLITE_BUSY`.

**Files.** The library writes no configuration file of its own yet. The
plan has two cards that will:

* **F-1a** (`docs/plan/F-files.md:31-72`): `cf_platform_write_file`
  replaces a file whole or not at all: a temporary in the same directory,
  `fsync`, `rename` (POSIX) or `MoveFileExW` with
  `MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH` (Windows). Its
  "Rejected" list includes "a lock file (one writer is the rule, D-128)".
* **F-2a** (`docs/plan/F-files.md:101-131`): an override file the library
  owns. `overrides_write` reads the file, applies the changes, emits the
  whole document and replaces the file through F-1a. Its "What a write
  does to a file" says: "two processes writing the same file lose one
  update (read, change, replace); no lock (rejected: a lock file is a
  second thing that a crash leaves behind, and D-127 already refuses to
  clean one)."

Until then, `examples/c/settings.c:66` writes with `fopen(..., "wb")` over
the old file: a crash or a reader at the wrong moment sees a truncated
file.

**Reading files.** `cf_blob_file` (`lib/serialize/src/blob.c:59`) reads a
file with `fread` into memory, once. Nothing checks that the file did not
change while it was read. On Windows the file is opened with `_wfopen`
(`lib/platform/src/win32/file.c:22`), which does not share delete access:
while it is open, a rename over it fails. Change detection compares
modification time and size and is documented as a hint
(`docs/Decisions.md:1689-1695`): it can miss a rewrite of the same length
inside one clock tick. A reload that fails to read or build keeps the
running snapshot (`cf_live.h:38-40`), and `settle` (`cf_live.h:161`)
waits some polls after a change is seen, so a burst of writes is read
once, at its end.

**Short answer to the maintainer, today:** threads are protected for
reads and for reloads through `cf_live`; processes writing one file are
not protected at all, because nothing writes a file yet; F-1a will stop
readers from ever seeing a half file that the library wrote, but not two
writers from losing each other's change.

---

## 2. What the world does

### POSIX `rename`

`rename()` replacing an existing name is atomic: "a link named new shall
remain visible to other threads throughout the renaming operation" and
refers to either the old or the new file [1]. It says nothing about
crashes or durability. This is what every "write a temporary, then
rename" scheme rests on, F-1a included.

### `flock`

Whole-file advisory locks, shared or exclusive, `LOCK_NB` to fail instead
of waiting [2]. The lock belongs to the open file description: two
separate `open()` calls in one process conflict, so it excludes threads
that each open the file. It is released when the last duplicate is
closed, or when the process dies. Not POSIX, but on Linux, the BSDs,
macOS and musl. On Linux it does not interact with `fcntl` locks; on NFS
since Linux 2.6.12 it is emulated with whole-file `fcntl` locks; on
SMB since Linux 5.5 it becomes effectively mandatory. Converting shared to
exclusive is not atomic.

### `fcntl` record locks, classic and OFD

Classic POSIX locks (`F_SETLK`, `F_SETLKW`) belong to the process [3]:
closing *any* descriptor of the file drops all of the process's locks on
it, threads of one process share them and cannot exclude each other, and
`fork` does not inherit them. Open file description locks
(`F_OFD_SETLK`, `F_OFD_SETLKW`; Linux 3.15) belong to the open file
description like `flock`, but keep byte ranges; they conflict with
classic locks even inside one process [3][4]. They are in POSIX.1-2024
(Issue 8) [5]. Mandatory locking is gone from Linux 5.15. On NFS a lock
can be lost after a partition; since Linux 3.12 later I/O then fails with
`EIO`, and nothing tells the program at the moment it happens [3].

### Windows `LockFileEx`

Byte-range locks, shared or exclusive, `LOCKFILE_FAIL_IMMEDIATELY` to
refuse instead of waiting; with an overlapped handle the wait can be
bounded through an event and a wait function [6]. Unlike POSIX they are
*mandatory* for the range: an exclusive lock "denies all other processes
both read and write access" to it, and a second handle of the same
process is also kept out. A lock does not stop access through a mapped
view. Locks are released at handle close or process exit, but "the time
it takes ... depends upon available system resources". Supported over
SMB 3.

### Windows rename: `MoveFileEx`, `ReplaceFile`, `FILE_RENAME_INFO`

`MoveFileExW` with `MOVEFILE_REPLACE_EXISTING` replaces the target's
contents; `MOVEFILE_WRITE_THROUGH` returns only once the move is on disk
[7]. The documentation does not call it atomic. `ReplaceFileW` keeps the
replaced file's attributes, ACLs and streams, but its error codes show it
is several steps: `ERROR_UNABLE_TO_MOVE_REPLACEMENT` without a backup
name leaves "the replaced file no longer exists" [8]. Renaming through
`SetFileInformationByHandle` with `FILE_RENAME_INFO` has a flags form
(`FileRenameInfoEx`) [9]; its POSIX-semantics flag lets a rename replace
a file that others hold open (Windows 10 and later; the reference page
lists the field but not the flag's effect, so a card measures it).

In practice a rename over a file that another process has open without
`FILE_SHARE_DELETE` fails with `ERROR_ACCESS_DENIED` or
`ERROR_SHARING_VIOLATION`. Go's own tool retries exactly these errors
("may be resolved by waiting"), for up to 2 seconds, because readers,
antivirus and indexers hold files briefly [10]. Google's `renameio`
exports nothing on Windows, saying it is "not possible to reliably write
files atomically on Windows" [11].

### git: lock files by existence

To change `foo`, git creates `foo.lock` with `O_CREAT|O_EXCL`, writes the
new content into it, and commits by renaming it over `foo`; rollback
deletes it [12]. "Readers do not block." The lock is the file's
existence, so a process killed without running its handlers leaves it
behind and every later writer fails until a human deletes it.
`hold_lock_file_for_update_timeout` retries with quadratic backoff for at
least `timeout_ms`; 0 tries once, -1 forever. The defaults are per lock:
`core.filesRefLockTimeout` 100 ms, `core.packedRefsTimeout` 1000 ms [13].

### SQLite

Readers take SHARED locks; a writer takes RESERVED while it prepares,
then PENDING (no new readers) and EXCLUSIVE to write, so readers cannot
starve it [14]. A crash mid-write leaves a "hot" journal that the next
reader rolls back. A lock that cannot be had returns `SQLITE_BUSY`; a
busy handler such as `sqlite3_busy_timeout(db, ms)` sleeps and retries
until `ms` have passed [15]. SQLite's own list of ways to corrupt a
database is the best catalogue of locking failures [16]: filesystems
with broken locks, NFS above all; classic POSIX locks dropped by a
`close()` elsewhere in the process; two copies of SQLite in one process;
processes using different lock protocols ("will not see each other's
locks"); a file renamed or unlinked while in use; and any process that
simply opens the file and overwrites it.

### systemd

`systemctl edit` opens the editor on temporary files, and writes them to
the real location only if the editor exits successfully; then it reloads,
as `daemon-reload` does [17]. Reading is a separate, explicit step:
`daemon-reload` reruns generators and reloads every unit file. A unit
file half written by another tool is read only when someone asks for a
reload.

### dconf / GSettings

Readers `mmap` one binary database (GVDB) and never talk to a service;
writes go over D-Bus to one writer service, which also sends the change
notifications [18][19]. There is one writer by construction, so there is
nothing to lock between writers. System defaults are keyfiles compiled by
`dconf update`, which notifies running programs over D-Bus; "locks" in
dconf are administrator locks on keys (mandatory settings), not
concurrency locks [18].

### Windows registry

The kernel's configuration manager owns the hives; programs never touch
the files. Writes from many processes are serialised by the kernel, one
call at a time. Changing several keys as one unit needs a transaction:
`RegCreateKeyTransacted` and the Kernel Transaction Manager; a
non-transacted operation on such a key rolls the transaction back [20].

### Apple: `NSUserDefaults` / `CFPreferences`

Since OS X 10.9 a daemon, `cfprefsd`, owns the preference files and
caches them; programs talk to it, and it writes asynchronously, so a
plist edited directly "is unlikely to have the expected results" [21].
Apple's older guidance says each process should choose when it accepts
new values, through `CFPreferencesAppSynchronize` as the "choke point",
rather than every process updating itself [22]. There is no transaction
across keys.

### etcd and Consul

Network stores, but they settle the lost-update question cleanly. etcd
gives every key a `mod_revision`; a transaction compares and then writes,
all comparisons applied atomically, and compare-and-swap is its stated
use [23]. Consul's KV `cas` writes only if the key's `ModifyIndex` still
matches (0: only if it does not exist); its locks are sessions, and a
plain write ignores them [24]. Both prefer an optimistic check to a lock
for single updates.

### libconfig

"Fully reentrant" but not thread-safe: two configurations in two
threads are fine, one configuration shared needs the caller's lock [25].
`config_write_file` writes the named file; an `fsync` option exists, off
by default; nothing says the write is atomic.

### Viper and koanf (Go)

Viper is not safe for concurrent use: "Concurrent reads and writes can
cause a panic" and the caller synchronises [26]. `SafeWriteConfig` only
refuses to overwrite an existing file. koanf's `Watch` reload "is not
goroutine safe if there are concurrent `*Get()` calls"; its own example
swaps in a new instance without a lock [27].

### Spring Cloud

A `@RefreshScope` bean is a lazy proxy over a cache; a refresh clears the
cache and the next call builds a new bean [28]. The scope "serializes
reads against refreshes for beans in that scope" with a lock, a cost on
every call that this library chose not to pay (D-14).

### Go's `renameio` and the ext4 lesson

`renameio` lists what "temporary, then rename" needs: the temporary on
the same filesystem, removed on every error but never after a successful
rename, and `fsync` before the rename, or a crash can leave a 0-byte file
[11]. That last point is the ext4 lesson of 2009: with delayed
allocation, files rewritten by rename without `fsync` came back empty
after a crash; ext4 added a heuristic, and the advice to programs stayed
"call `fsync`" [29]. Pillai et al. found 60 crash-consistency bugs in
eleven widely used systems, each depending on what a particular
filesystem persists and reorders [30].

### NFS

Close-to-open consistency: a client writes back on `close` and checks on
`open`; attributes are cached for 3 to 60 seconds by default, so another
client may see a change late [31]. Locks go through NLM by default;
`nolock` makes them local to one client, and `local_lock` chooses which
lock types stay local.

---

## 3. What breaks

Each case below is one a source shows, not one imagined.

1. **The lost update.** A read, a change, a replace: two writers both
   read version 1, and the second rename throws away the first change.
   The rename makes each file whole; it does nothing about this. F-2a
   says so (`docs/plan/F-files.md:114`). git takes a lock to stop it
   [12]; etcd and Consul compare a revision before they write [23][24].

2. **A lock on a file that rename replaces.** A lock belongs to the
   inode, not the name. A writer that locks `settings.json`, writes a
   temporary and renames it, leaves the next waiter holding a lock on an
   unlinked inode, while a third writer locks the new file. SQLite lists
   renaming or unlinking a locked database as a cause of corruption
   [16]. Either the lock lives on a file that is never replaced, or every
   writer re-checks after locking that the name still refers to the inode
   it locked.

3. **Classic `fcntl` locks are dropped by any `close`, and do not
   exclude threads.** A library that opens the same file elsewhere in
   the process silently drops the program's lock [3][4][16]. Two threads
   of one program taking the "same" lock both get it [3][4].

4. **Two lock protocols do not see each other.** On Linux `flock` and
   `fcntl` locks do not interact [2]; SQLite lists mixed protocols as a
   cause of corruption [16]. Every writer of one file must use the same
   call.

5. **Advisory means a writer that does not ask is not stopped.** An
   editor, `sed -i`, `echo >`, a deployment tool: none takes the lock.
   SQLite cannot defend against "any process" that overwrites the file
   [16]; on macOS `O_EXLOCK` is equally advisory [21]. No lock protects a
   reader from these writers. Only the reader's own checks can.

6. **A lock by existence outlives a crash.** git's `.lock` is removed by
   `atexit` and signal handlers, not by the kernel; `kill -9`, a power
   cut or an abort leaves it, and every later writer fails until it is
   deleted by hand [12]. This is the objection D-128 already makes, and
   it is right for this kind of lock. A kernel lock (`flock`, `fcntl`,
   `LockFileEx`) is released when the process dies [2][3][6]; the file
   that carries it can stay, empty and meaningless.

7. **The half-written file a reader parses.** A writer that writes in
   place (truncate, then write) exposes a moment when the file is empty
   or cut short. A JSON or XML document cut short fails to parse, and the
   reload keeps the running snapshot. A line-based format does not fail:
   an INI, `.properties`, `.env` or TOML file cut at a line end, or YAML
   cut between keys, is a valid document with keys missing, which become
   defaults unless the contract requires them. `renameio` exists to stop
   readers ever seeing "a half-written or 0-byte file" [11]; that only
   works for writers that use it.

8. **A rename without `fsync` can leave an empty file after a crash.**
   [11][29][30]. F-1a's step 3 is the `fsync`, and its directory sync
   after the rename is the second half.

9. **Windows: the replace fails while someone reads.** A target open
   without `FILE_SHARE_DELETE` makes `MoveFileExW` fail with access
   denied or a sharing violation. The library's own reader opens that way
   (`win32/file.c:22`), and so do antivirus scanners and indexers. Go
   retries these errors for 2 seconds [10]; `renameio` gives up on
   Windows [11]. F-1a, as written, fails the write on the first such
   error.

10. **Windows: `ReplaceFile` can leave the file under another name.**
    Its errors describe states in which the replaced file is gone or
    renamed [8]. F-1a's choice of `MoveFileExW` over it is right.

11. **Windows: locking the configuration file itself stops its
    readers.** `LockFileEx` is mandatory for the range: an exclusive lock
    denies reads to every other process and to the locker's own second
    handle [6]. A writer that locked `settings.json` would make every
    reload fail while it writes. The lock must be on another file.

12. **Network filesystems.** Locks may be emulated, local only
    (`nolock`), or lost after a partition with an `EIO` later and no
    signal [2][3][31]; attributes are cached for seconds, so a change
    detector on another client sees a change late [31]. SQLite names NFS
    as the usual place its locks fail [16]. No promise holds there that
    holds on a local disk.

13. **A change detector that misses a change.** Modification time and
    size miss a rewrite of the same length inside one tick
    (`docs/Decisions.md:1689-1695`). A check made only before a read
    cannot tell that the file changed during it.

14. **A wait with no bound, or a refusal with no wait.** git's `-1`
    waits forever; the store today refuses at once (`store.c:231`, no
    busy timeout [15]). A library waiting forever hangs its program on
    another program's crash or bug; a library that never waits fails a
    user's save because an indexer looked at the file.

15. **A spin lock held for long work.** `cf_live`'s ticket lock spins
    (`live.c:86-90`). It is right for a pointer copy. Used to keep two
    reload drivers apart, it would spin a CPU for the length of a
    rebuild, which reads files.

16. **Another user holding the lock.** In a directory others can write
    to (H-5), a lock file can be created first, or held forever, by
    someone else, or be a symbolic link. A bounded wait turns this into
    a refusal, not a hang; the lock file is opened without following
    links, as F-1a refuses links for the file itself.

---

## 4. What fits this library

The measures: the C89 core with platform calls only in `lib/platform/`
(D-100, D-106); no thread and no callback of the library's own (D-14,
the `cf_live` decision); one refusal, named, the same everywhere
(D-107); the platform tier internal (D-51, D-88), so a new public name is
a decision; and D-127/D-128 as the F cards draft them.

### Threads in one process

| Option | Cost | Needs | Rules out |
| ------ | ---- | ----- | --------- |
| T1. Keep D-14 as it is: reads free, reloads through `cf_live`, one driving thread | none | nothing | nothing |
| T2. A guard on the `cf_live` driving calls: a second driver at once gets a refusal | an atomic test-and-set per poll | a new `cf_live_result` value (public) | waiting (only a spin is available) |
| T3. A lock inside `cf_config` | a lock on every read | a mutex the port does not have | D-14 and the performance model |

T1 already answers the maintainer's question for threads. T2 is
reasonable, but it is the Roadmap's next item, "Threads and processes
made easier" (`docs/Roadmap.md:1585-1594`), and belongs to its research.
T3 contradicts D-14 and is not considered further.

### Processes writing one file

| Option | Cost | Needs | Rules out |
| ------ | ---- | ----- | --------- |
| P0. Nothing beyond F-1a (D-128 as drafted) | none | nothing | lost updates are silent |
| P1. Optimistic check: before the rename, read the file again and compare with the bytes the change was made on; when they differ, apply the change to the new bytes and try again, a few times, then refuse | one re-read of a small file | nothing public; a variant of F-1a's function | does not close the window between the check and the rename |
| P2. A kernel lock on a sidecar file, `.<base>.cf-lock` beside the file, around read, change, replace | one open, one lock call, one small file left in the directory | a platform lock (internal), an option to ask for it (public) | writers that do not take it are not stopped (case 5) |
| P3. A lock by existence, git's `.lock` | a stale lock after every crash | a human, or a cleaner D-127 refuses to have | robustness |
| P4. Lock the configuration file itself | none | nothing | Windows readers (case 11) and the rename (case 2) |
| P5. A writer service (dconf, `cfprefsd`, the registry) | a process | IPC | a library; this is a program's architecture |
| P6. The SQLite store | already built (D-42) | SQLite | a file a human edits |

P1 and P2 together are what git and etcd do between them: git locks,
etcd compares. P1 alone closes almost all of the window: the race is
then between a re-read and a rename, microseconds, instead of a whole
read, change and emit. P2 closes it for every writer that cooperates,
which includes every program built on this library, `configforge codec
set` (F-3i) and other copies of the same program.

**What P2 needs.** In the platform layer (internal tier), one lock call:

* POSIX: `flock(fd, LOCK_EX | LOCK_NB)` where it exists (Linux, the BSDs,
  macOS, musl): it excludes threads that open separately, survives an
  unrelated `close`, and dies with the process [2]. Where it does not,
  `F_OFD_SETLK` when the header defines it [3][5]; else classic
  `F_SETLK`, with its two faults written down (case 3). One protocol for
  each platform, never two (case 4).
* Windows: `LockFileEx` on the sidecar, `LOCKFILE_EXCLUSIVE_LOCK`,
  exclusive on one byte. Mandatory locking does not matter there: no one
  reads the sidecar.
* none: refused with `CF_E_CAPABILITY`.
* The sidecar is opened read-write, created if missing, never deleted
  (deleting it would bring back case 2), never followed through a
  symbolic link (`O_NOFOLLOW` where defined, `lstat` first as F-1a does).
  Its mode is the configuration file's own, so that whoever may write the
  file may lock it.

**Waiting.** C89 has no clock (`cf_live.h:59-60`), but the platform layer
does: `poll` with a timeout (`lib/platform/src/posix/posix.c:938`) and
`Sleep` (`lib/platform/src/win32/win32.c:443`). A bounded wait is a loop
of non-blocking attempts with growing sleeps, as git does [12], kept in
the platform layer. On Windows an overlapped `LockFileEx` with
`WaitForSingleObject` gives an exact bound [6]; the loop is simpler and
the same on both, and is enough.

**Reporting.** A lock still held at the end of the wait is a refusal of
its own kind: the program's reaction (try later, tell the user that
another program is saving) differs from a file that cannot be written.
`cf_code` has no such code (`lib/base/include/configforge/cf_code.def:25-33`);
reasons are for document refusals (D-107), so a reason does not fit.

### The store

The store already has locks; it lacks a wait. `sqlite3_busy_timeout`
(or `PRAGMA busy_timeout`) before `BEGIN IMMEDIATE` is one line [15], and
the same wait the program chooses for files should apply.

### A reader and a half-written file

| Option | Cost | Needs | Rules out |
| ------ | ---- | ----- | --------- |
| R1. Writers that never write in place: F-1a | already planned | nothing | the half file from the library's own writers |
| R2. A stable read: take the stamp, read, take it again; when it moved, read again, a few times, then refuse | one `stat` more per file read | nothing public | most reads of a file being written by anyone |
| R3. A shared lock taken by every reader and reload | a lock call per read | P2 | nothing that R1 does not already give: library writers never write in place, and other writers do not take the lock |
| R4. `settle` | exists | nothing | bursts |
| R5. Windows readers that share delete access (`CreateFileW` with `FILE_SHARE_DELETE`, then a C stream on it) | a different open in `win32/file.c` | nothing public | a reader blocking a writer's rename, if Windows lets the rename through (to be measured) |

R3 is the option the Roadmap item names ("a lock taken by a reload").
It buys nothing: with R1 a reader always opens a whole file, old or new,
and the writers that do write in place are exactly the ones that take no
lock. It would also make a reader wait for a writer, which D-14 refuses
on the read path.

---

## 5. Recommendation

### First: no new public interface

1. **Amend F-1a before it opens (Windows).** Step 5 (`MoveFileExW`)
   retries `ERROR_ACCESS_DENIED` and `ERROR_SHARING_VIOLATION` with a
   bounded wait, as Go does [10], instead of failing at the first; the
   test holds the target open with `_wfopen` and sees the write succeed
   once it is closed inside the bound, and fail after it. The bound is
   the one of point 3 below when that exists, a fixed 2 s until then.
   Without this, the library's own reload on one thread makes a save on
   another fail on Windows.
2. **Amend F-2a: the optimistic check (P1), always.** `overrides_write`
   keeps the bytes it read; just before the replace, a variant of F-1a's
   function reads the file again and compares. Different: the items are
   applied again to the new bytes, up to 3 times, then refused with
   "'%s' changed while it was written; try again". A change then wins key
   by key, not file by file: two programs that set different keys both
   keep their key. It needs no file, no option and no new name, so it
   does not touch D-128's objection, and it replaces "lose one update" in
   D-128 with "lose one only in a window of microseconds".
3. **Write the promise down now** (documentation only, straight to
   `main`): a Guide section "At the same time" with the three answers of
   section 1, the stale "0 to 1" line of `docs/Decisions.md:2194` fixed,
   and the cases a program must handle itself (other writers, network
   filesystems).

### After: the lock by choice

4. **The sidecar lock (P2), asked for by the program**, around read,
   change, replace in `overrides_write`, and around F-3i's edits when the
   CLI is told to. Default off, as the maintainer's "it could depend on
   an option" asks, and so that no file appears that the program did not
   ask for. With point 2 always on, the lock is what a program asks for
   when it needs the last microsecond closed too: several copies of one
   service writing one file, an installer and the program writing at
   once.
5. **The same wait for the store** (`sqlite3_busy_timeout`).
6. **A stable read (R2)** in the file source: the stamp before and after
   the read, up to 3 reads, then the source fails as an unreadable file
   does, which a reload already survives. It catches most in-place
   writers, which no lock can.

The public interface, sketched (names follow `cf_builder_writable` and
F-2a's option A):

```c
/* cf_builder_overrides `flags`: writers of this file take its lock, a
   file named .<name>.cf-lock beside it, kept, never removed. */
#define CF_FILE_LOCK 0x0100u

/* How long a write waits for another writer of the same file or store,
   in milliseconds, before it is refused with CF_E_BUSY. 0, the
   default: refused at once. No value waits forever. */
CF_API cf_bool cf_builder_write_wait(cf_builder* b, unsigned long wait_ms);
```

```c
/* cf_code.def, after CF_E_POLICY */
CF_CODE_X(CF_E_BUSY, "busy", "another writer holds it; try again")
```

Internal tier (`lib/platform/include/configforge/cf_platform_file.h`,
under `CF_INTERNAL_API`, after F-1a's names):

```c
typedef struct cf_platform_lock { long handle; } cf_platform_lock;
/* 1 with the lock held on `path`.cf-lock; 0 with err set: CF_E_BUSY
   after `wait_ms`, CF_E_SOURCE when the lock file cannot be opened. */
CF_API int  cf_platform_lock_take(cf_platform_lock* l, const char* path,
                                  unsigned long wait_ms, cf_error* err);
CF_API void cf_platform_lock_give(cf_platform_lock* l);
/* F-1a with the optimistic check: 0 and CF_E_BUSY when the file no
   longer holds `expect` (NULL, 0: it must not exist). */
CF_API int  cf_platform_write_file_if(const char* path, const void* data,
                                      size_t size, const void* expect,
                                      size_t expect_size, cf_error* err);
```

### Not to build

* **A lock on the read path or inside `cf_config`** (T3, R3): D-14 holds,
  and nothing would be gained.
* **A lock by existence** (P3): case 6.
* **Locking the configuration file itself** (P4): cases 2 and 11.
* **A writer service** (P5): a program's architecture, not a library's.
* **An unbounded wait**: case 14; a program that wants to wait longer
  calls again.
* **Promises on network filesystems**: the page says what may fail there
  (case 12) and the library does not detect the filesystem.
* **Transactions across files** (KTM, a journal of several files): one
  file is one unit; a program that needs several keys changed together
  puts them in one file, or uses the store.
* **A guard on `cf_live`'s driving calls** (T2): left to the research of
  "Threads and processes made easier".

### TO DECIDE

1. **Reopen D-128's "no lock file".** D-128, as drafted in F-2a, rejects
   a lock file because "a crash leaves it behind". That is true of a lock
   by existence (git's), not of a kernel lock on a kept file, which a
   crash releases. Options: **A** the sidecar lock, by the program's
   choice (`CF_FILE_LOCK`), default off; **B** the same, default on, a
   flag to turn it off; **C** no lock, the optimistic check only;
   **D** D-128 as drafted (neither). **Recommended: A, with the
   optimistic check always (point 2).** It is what the maintainer's
   "depend on an option" describes, it leaves no file in a directory
   unless asked, and C alone already removes almost all lost updates.
2. **The wait: where it is set and its default.** Options: **A**
   `cf_builder_write_wait(b, ms)` for the whole configuration, default 0;
   **B** the same, default a short bound (git uses 100 ms and 1000 ms
   [13]); **C** a wait argument on `cf_write` (a breaking change of a
   public signature, so 2.0.0). **Recommended: A.** A program chooses
   its own patience; 0 keeps today's store behaviour (refused at once)
   as the default, so nothing changes for a program that asks nothing.
   The Windows retry of point 1 is not this wait: it is part of making
   one write work at all, and has its own fixed bound.
3. **How "busy" is reported.** Options: **A** a new code `CF_E_BUSY`;
   **B** `CF_E_SOURCE` with a message only; **C** `CF_E_SOURCE` with a
   new reason (but reasons are document refusals, D-107). **Recommended:
   A**: the program acts on it differently, and a message is not
   something to test against. It adds a value to a public enum, inside
   1.x as an addition.

---

## 6. The cards it implies

Sizes as the cards use them; versions from `docs/Maintaining.md:99-114`
(2.2.0 is "files written back", where F-1a and F-2a are; F-1a's own
CHANGELOG line says 1.9.0, which the table does not, and the card should
be brought into line when it is read cold).

| Card | Title | Size | Version | Needs |
| ---- | ----- | ---- | ------- | ----- |
| docs | The Guide's "At the same time": what is guarded and what is not; D-14's sequel fixed | small | now, on `main` | nothing |
| F-1a amended | Windows: the replace retried for a bounded time while another handle holds the target; the test holds it open | small, inside F-1a | 2.2.0 | nothing |
| F-1c | `cf_platform_write_file_if`: the replace refused when the file no longer holds the bytes read | small | 2.2.0 | F-1a |
| F-2a amended | `overrides_write` re-reads and re-applies before it replaces (up to 3 times), then refuses | small, inside F-2a | 2.2.0 | F-1c |
| F-5a | `cf_platform_lock_take` and `_give`: `flock`, OFD or classic `fcntl`, `LockFileEx`, on a kept sidecar; a test that two opens in one process exclude each other on every platform | medium | 2.2.0 | TO DECIDE 1 |
| F-5b | `CF_FILE_LOCK`, `cf_builder_write_wait`, `CF_E_BUSY`; the lock around `overrides_write`; the store's busy timeout from the same wait | medium | 2.2.0 | F-5a, F-2a, TO DECIDE 2 and 3 |
| F-5c | A stable read: the stamp before and after a file is read, up to 3 reads | small | 2.2.0 | nothing |
| F-5d | Windows readers that share delete access, if a measurement shows a rename then succeeds over an open reader | small | 2.2.0 | F-1a; dropped if the measurement says no |
| (later) | A guard on `cf_live`'s driving calls | small | with "Threads and processes made easier" | that item's research |

---

## 7. Sources

1. The Open Group, POSIX.1-2024, `rename`:
   https://pubs.opengroup.org/onlinepubs/9799919799/functions/rename.html
2. Linux man-pages, `flock(2)`:
   https://man7.org/linux/man-pages/man2/flock.2.html
3. Linux man-pages, `fcntl_locking(2)`:
   https://man7.org/linux/man-pages/man2/fcntl_locking.2.html
4. J. Layton, "File-private POSIX locks", LWN, 2014:
   https://lwn.net/Articles/586904/
5. The Open Group, POSIX.1-2024, `fcntl` (OFD locks):
   https://pubs.opengroup.org/onlinepubs/9799919799/functions/fcntl.html
6. Microsoft, `LockFileEx`:
   https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-lockfileex
7. Microsoft, `MoveFileExW`:
   https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-movefileexw
8. Microsoft, `ReplaceFileW`:
   https://learn.microsoft.com/en-us/windows/win32/api/winbase/nf-winbase-replacefilew
9. Microsoft, `FILE_RENAME_INFO`:
   https://learn.microsoft.com/en-us/windows/win32/api/winbase/ns-winbase-file_rename_info
10. The Go project, `cmd/internal/robustio` (`robustio.go`,
    `robustio_windows.go`, `robustio_flaky.go`):
    https://github.com/golang/go/tree/master/src/cmd/internal/robustio
11. Google, `renameio`: https://github.com/google/renameio
12. git, `lockfile.h`:
    https://github.com/git/git/blob/master/lockfile.h
13. git, `core.filesRefLockTimeout` and `core.packedRefsTimeout`:
    https://git-scm.com/docs/git-config
14. SQLite, "File Locking And Concurrency In SQLite Version 3":
    https://www.sqlite.org/lockingv3.html
15. SQLite, `sqlite3_busy_timeout`:
    https://www.sqlite.org/c3ref/busy_timeout.html
16. SQLite, "How To Corrupt An SQLite Database File":
    https://www.sqlite.org/howtocorrupt.html
17. systemd, `systemctl(1)` (`edit`, `daemon-reload`):
    https://www.freedesktop.org/software/systemd/man/latest/systemctl.html
18. GNOME, dconf for system administrators:
    https://wiki.gnome.org/Projects/dconf/SystemAdministrators
19. `dconf-service(1)`:
    https://man.archlinux.org/man/dconf-service.1.en ; and
    https://wiki.gnome.org/Projects/dconf
20. Microsoft, `RegCreateKeyTransactedA`:
    https://learn.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regcreatekeytransacteda
21. H. Oakley, "Sticky preferences: why trashing or editing them may not
    change anything", 2017:
    https://eclecticlight.co/2017/07/06/sticky-preferences-why-trashing-or-editing-them-may-not-change-anything/ ;
    and the Cocoa-dev thread on cross-process settings:
    https://lists.apple.com/archives/cocoa-dev/2009/Aug/msg01603.html
22. Apple, "Preferences Best Practices" (archived):
    https://developer.apple.com/library/archive/documentation/CoreFoundation/Conceptual/CFPreferences/Concepts/BestPractices.html
23. etcd, "etcd API" (revisions, transactions):
    https://etcd.io/docs/v3.5/learning/api/
24. HashiCorp, Consul KV store API (`cas`, `acquire`):
    https://developer.hashicorp.com/consul/api-docs/kv
25. libconfig manual, "Multithreading Issues" and `config_write_file`:
    https://hyperrealm.github.io/libconfig/libconfig_manual.html
26. Viper, README (FAQ on concurrency, writing config files):
    https://github.com/spf13/viper
27. koanf, README (watching, reloading):
    https://github.com/knadh/koanf
28. Spring Cloud Commons, "Refresh Scope" and "Environment Changes":
    https://docs.spring.io/spring-cloud-commons/reference/spring-cloud-commons/application-context-services.html
29. J. Corbet, "ext4 and data loss", LWN, 2009:
    https://lwn.net/Articles/322823/
30. T. S. Pillai et al., "All File Systems Are Not Created Equal: On the
    Complexity of Crafting Crash-Consistent Applications", OSDI 2014:
    https://www.usenix.org/conference/osdi14/technical-sessions/presentation/pillai
31. Linux man-pages, `nfs(5)` ("Data and metadata coherence", `lock`,
    `local_lock`): https://man7.org/linux/man-pages/man5/nfs.5.html
