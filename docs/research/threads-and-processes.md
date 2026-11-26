# Threads and processes made easier

Research for the Roadmap item of that name ("From the maintainer,
2026-11-26", `docs/Roadmap.md:1585-1594`). It is not a card. It says what
the library gives a program's threads and processes today, what other
libraries and runtimes give theirs, what breaks, and what fits this
library, and it ends with the cards it implies and the choices that are
the maintainer's.

Its neighbour, `docs/research/concurrent-access.md`, is about files: two
writers, a reader and a half-written file, locks on disk. This one is
about the program: its threads reading one configuration while it is
replaced, its child processes and workers, and how they hear of a
reload. Where the two meet (the `cf_live` driving thread, a guard on two
drivers) this one picks up what that one left here (its section 4,
"Threads in one process", option T2).

Line numbers are those of the tree at 091ee78.

---

## 1. The question

### As the maintainer asked it

> "There could be things that make it easier when work is done with many
> threads. Not that it gives a direct benefit, but threads and processes
> will surely be used." (Turkish, translated.)

The Roadmap item names four things to weigh: a configuration shared by
many threads and swapped on reload without a lock on the read path; a
child process handed the same configuration (an image in shared memory,
or a file descriptor); workers told of a reload; and, by implication,
where the library stops and the program begins.

### As it stands in the library today

**A snapshot never changes.** D-14 (`docs/Decisions.md:705-750`), in the
public header at `lib/config/include/configforge/configforge.h:702-720`:
"Reads are safe concurrently. Building and releasing are the caller's to
synchronise." A `cf_config` is immutable and reference counted; a change
is a new snapshot. The count is the port's atomic
(`lib/port/include/configforge/cf_port.h:319-360`: GCC `__atomic`,
`__sync`, MSVC `Interlocked`, or C11 `stdatomic`; a toolchain with none
must build `CF_SINGLE_THREADED`). `cf_config_free` decrements before it
frees, because the `cf_config` lives inside the arena it releases
(`lib/config/src/core/config.c:2186-2197`). D-03
(`docs/Decisions.md:261-280`) chose this - a reference the reader holds -
over "a thread-local current snapshot, borrowed pointers valid until the
next reload on that thread", because the latter "fails exactly when a
value is captured across a reload boundary".

**One configuration that reloads: `cf_live`** (D-48,
`docs/Decisions.md:2173-2200`; `lib/config/include/configforge/cf_live.h`).

* `cf_live_get` (`cf_live.h:182`, `lib/config/src/core/live.c:740-751`)
  may be called from any thread. It returns the current snapshot with a
  reference the caller releases. "One request sees one configuration."
* It takes that reference under a **ticket lock** made of the port's
  atomics (`live.c:76-94`): take a ticket, spin until it is served, copy
  the pointer, increment the snapshot's count, serve the next. It
  replaced a one-count lock that livelocked under tcc (commit 6020679).
  The spin has no pause and no yield. D-48's text still says "a count
  taken from 0 to 1" (noted as stale in concurrent-access.md too).
* `live_swap` (`live.c:314-350`) changes the pointer under the same
  lock and counts a generation; the old snapshot is released, and a
  reader still holding it keeps it.
* `cf_live_generation` (`cf_live.h:234`, `live.c:910-920`) also takes the
  lock to read a `cf_u64`. So a worker that wants to ask "has it changed
  since I looked?" pays the same lock as a `get`.
* Every other call "belongs to one thread at a time, the one that drives
  the reloads" (`cf_live.h:69-73`). Nothing detects a second driver.
* No thread and no callback of the library's own (D-35,
  `docs/Decisions.md:1612-1650`; D-48). The `check`, `health` and `log`
  functions of `cf_live_options` (`cf_live.h:131-162`) are called on the
  driving thread, inside `cf_live_poll`: the program's own thread.
* Bindings: Rust has `Live::reader()`, "Send + Sync + Clone, for the
  workers" (`bindings/rust/src/live.rs:14`); Python's `get` is "any
  thread" too (`bindings/python/src/configforge/_live.py:5`).
* Tests: `test_live.c:346-380` runs four reader threads against swaps
  under pthreads ("under ThreadSanitizer this is the test of the
  lock"). The benchmark measures a
  reload (`bench/bench.c:287-317`), not `cf_live_get` under contention.

**Watching.** `cf_watcher` hands over a descriptor or a HANDLE to wait on
(`configforge.h:827-865`): an inotify descriptor opened `IN_CLOEXEC`
(`lib/platform/src/posix/posix.c:839`), kqueue descriptors opened
`O_CLOEXEC` (`posix.c:1020`), a Windows event made with no security
attributes, so not inheritable (`lib/platform/src/win32/win32.c:520`). It
tells the *driving* thread a file moved; nothing tells *workers* that a
swap happened, except asking for the generation.

**The image** (`docs/ImageFormat.md`, `cf_image.h`, D-28). A resolved key
space laid out so that loading is validation and pointer arithmetic.
Every offset is relative to the start and a multiple of 8, every number
little endian and read byte by byte (`image.c` `get_u32`), so an image
is position-independent and host-independent. `cf_image_open`
(`cf_image.h:84`, `image.c:355-437`) validates the header, every bounds
and the digest, and then the image "is used in place. Nothing is copied,
nothing is allocated, and the buffer must outlive the view"
(`cf_image.h:58-63`). `cf_image_write` takes `cf_keys(c)` and
`cf_sources(c)`, so any built configuration can be written as an image.
`cf_source_image_mem` and `cf_source_image_file` (`cf_image.h:107-112`)
make one a source; the file form reads it into the key space's arena
(`image.c:681-706`). The image carries values and, optionally,
provenance; it does not carry the contract (types, `CF_RESTART`,
secret flags). Nothing maps one, shares one, or hands one to a child.

**Processes.** The library starts none and passes nothing to one.
Descriptors it opens briefly, while reading, are inheritable:
`cf_platform_fopen` calls `fopen` with the caller's mode and no `e`
(`lib/platform/src/posix/file.c:17-20`), `_wfopen` with no `N`
(`lib/platform/src/win32/file.c:35-42`), and
`cf_platform_create_private` opens with no `O_CLOEXEC`
(`posix.c:487`). A child that another thread starts in that window
inherits them.

**Short answer to the maintainer, today:** threads are served - any
thread reads any snapshot with no lock, and `cf_live_get` hands out the
current one under a lock held for two instructions. What is missing is a
read path that touches no shared cache line, a way for workers to hear
of a swap, anything for a child process, and the rules for `fork`
written down.

---

## 2. What the world does

### Read-copy-update: the Linux kernel and userspace-rcu

RCU splits an update into removal (publish the new pointer with
`rcu_assign_pointer`) and reclamation (free the old one after a *grace
period*, when every reader that might hold it has left its read-side
section: `synchronize_rcu`, or `call_rcu` to do it later). Readers mark
their section with `rcu_read_lock`/`rcu_read_unlock` and fetch with
`rcu_dereference`; in the kernel's non-preemptible model the read side
costs nothing [1]. Updaters still serialize among themselves.

userspace-rcu (liburcu, LGPL-2.1) brings it to programs in five flavors
[2]: the default (uses `sys_membarrier`, falls back to barriers), `mb`
(barriers on both sides), `signal` (needs a signal such as `SIGUSR1`),
`qsbr` (the fastest reads, but every reader thread must announce
quiescent states itself with `rcu_quiescent_state()`), and `bp` (no
registration, slower). Every flavor but `bp` needs each reader thread
registered. Linux on many CPUs is tested; FreeBSD and Cygwin less;
Darwin "needs more testing". `membarrier(2)` [16] is the Linux system
call that lets the writer pay for the readers' barriers.

### Hazard pointers

Michael's hazard pointers [3]: before using a shared pointer a reader
publishes it in a slot of its own ("hazard"), then checks that the
shared pointer still equals it; a writer that retires an object frees it
only when no slot names it. Unreclaimed memory is bounded by the number
of slots. C++26 standardizes them (`<hazard_pointer>`,
`make_hazard_pointer`, `protect`, `hazard_pointer_obj_base::retire`) from
folly's implementation [4]. The reader pays a store and a full fence per
protect; the writer pays a scan of every slot.

### Go: `sync/atomic.Value` and `atomic.Pointer`

`atomic.Value` gives "an atomic load and store of a consistently typed
value"; storing `nil` or a different type panics, and a `Value` must not
be copied after first use [5]. Its documentation's own example is a
configuration: a goroutine reloads every ten seconds and `Store`s; ten
workers `Load` for each request. `atomic.Pointer[T]` (Go 1.19) is the
typed form. Reclamation is the garbage collector's, which is why the
read side is one load. `context.Context` [6] is the other half of Go's
answer: a request carries what it needs down the call chain, so a
handler that took one configuration at the start keeps it to the end.

### Java: a volatile reference, and Spring's `@RefreshScope`

The Java memory model makes a write to a `volatile` field happen-before
every later read of it [7], so a reference to an immutable object,
swapped through a `volatile` field (or `AtomicReference`), is safely
published; the garbage collector reclaims the old one. Spring Cloud's
`@RefreshScope` [8] puts beans behind lazy proxies and caches their
targets; a refresh (`/actuator/refresh`, or an `EnvironmentChangeEvent`)
clears the cache, and each bean is built again at its next method call.
Holders of the old bean (its open connections, say) finish what they
were doing. The documentation's caveats: a removed property does not
disappear on refresh; `@RefreshScope` on a `@Configuration` class "might
lead to surprising behavior"; `HikariDataSource` is never refreshable by
default; and refresh is not supported in native images.

### Rust: `arc-swap`

`ArcSwap` holds an `Arc<T>` that can be swapped atomically, for
"read-mostly data" such as configuration [9]. `load` returns a `Guard`
that does not touch the shared reference count in the common case: each
thread has a few fast "debt" slots (eight, "at the time of writing"),
and when they run out it takes a slower path. Its limitations page [10]:
holding many guards at once is slow; guards are "not meant to be stored
in data structures or kept across async yield points". Its `Cache`
reads faster still "at the cost of delayed reclamation": a cache keeps
the last `Arc` it saw and only checks a counter until it changes.

### .NET: `IOptions`, `IOptionsSnapshot`, `IOptionsMonitor`

Three lifetimes for one bound settings object [11]: `IOptions` is read
once and never changes; `IOptionsSnapshot` is computed once per request
(scoped) and stays fixed for it; `IOptionsMonitor` is a singleton whose
`CurrentValue` is the latest and whose `OnChange` notifies. Change
notification works only for file-based providers, and in containers and
on network shares the documentation says to set
`DOTNET_USE_POLLING_FILE_WATCHER`, which polls every four seconds.

### Envoy: a main thread, workers, and thread-local slots

Envoy is one process: a main thread for configuration, signals and
administration, and worker threads that each own their connections [12].
Configuration reaches workers through thread-local storage: the main
thread allocates a slot, and on a change "posts a closure to every
worker"; each worker updates its own copy at a moment of its choosing,
and reads it with no lock. Across processes, **hot restart** [13]: the new
process starts and initializes fully, then asks the old one over a Unix
socket for copies of its listening sockets, starts listening, and tells
the old one to drain (`--drain-time-s`) and then to exit
(`--parent-shutdown-time-s`). Connections are not transferred. Not
supported on Windows.

### nginx, Apache, HAProxy: masters and workers

nginx on `SIGHUP` [14]: the master checks the syntax, opens log files and
new listening sockets, starts new workers with the new configuration,
and asks the old workers to close their listening sockets and finish
their clients. If the new configuration fails, the master "rolls back
changes and continues to work with old configuration". Workers never
reload in place: a configuration lives as long as a process. On `USR2`
the master starts a new binary that inherits the listening sockets (the
source passes their numbers in the `NGINX` environment variable,
`src/core/nginx.c`). Apache's graceful restart [15] does the same with
its children. HAProxy's seamless reload [17] has the new process fetch
the old one's listening sockets over its stats socket (`-x`, `expose-fd
listeners`), with `SCM_RIGHTS`.

### PostgreSQL: `SIGHUP` and the postmaster

The postmaster re-reads `postgresql.conf` on `SIGHUP` (`pg_ctl reload`,
`pg_reload_conf()`) and "propagates this signal to all currently running
server processes, so that existing sessions also adopt the new values
(this will happen after they complete any currently-executing client
command)" [18]. Each backend re-reads the file itself, at a point where
it is safe. Parameters that can only be set at start are ignored until
a restart, and invalid settings are "ignored (but logged)";
`pg_file_settings` shows what was not applied and why. This is the
library's `CF_RESTART` and "a failed reload keeps the running one",
already, in a process model.

### Passing descriptors: `SCM_RIGHTS`, systemd, memfd

`SCM_RIGHTS` [19] sends open descriptors over a Unix socket; "what is
being passed is a reference to an open file description", like a `dup`
into the receiver. If the receiver's control buffer is too small, the
excess descriptors are closed and `MSG_CTRUNC` is set; at most 253 a
message; a stream socket must carry at least one byte of data with them.

systemd's socket activation hands descriptors to a service at start
(`$LISTEN_FDS`, `$LISTEN_PID`, `$LISTEN_FDNAMES`, `sd_listen_fds`) [20],
and its **file descriptor store** [21] keeps descriptors a service
uploads (`FDSTORE=1`, `FDNAME=`, up to `FileDescriptorStoreMax=`) and
gives them back at the next start. It suggests storing "a memfd holding
serialized service state". Stored descriptors that report `POLLHUP` are
dropped; a new version of the service should remove names it does not
know.

`memfd_create` [22] (Linux 3.17, glibc 2.27) makes an anonymous file in
memory. With `MFD_ALLOW_SEALING`, `fcntl(F_ADD_SEALS)` can forbid
writing (`F_SEAL_WRITE`, or `F_SEAL_FUTURE_WRITE` which keeps existing
mappings), shrinking, growing, and further sealing. The documented use
is exactly the hand-off: one process fills and seals, another receives
the descriptor (over a socket, by `fork`, or through `/proc/<pid>/fd`),
checks the seals with `F_GET_SEALS`, and maps it, with no need to defend
against a peer that changes or shrinks it under the mapping.

### `fork` and copy-on-write

After `fork` the child has one thread, "the one that called fork()",
and a copy of the parent's memory "including the states of mutexes,
condition variables, and other pthreads objects"; until it calls
`execve` it may call only async-signal-safe functions [23]. Pages are
shared copy-on-write. "A fork() in the road" [24] argues `fork` is a
poor fit for multithreaded programs for these reasons. Instagram's
Python servers found copy-on-write defeated by reference counts written
into shared pages, and froze their collector before forking [25].

### Windows: file mappings and handle inheritance

`CreateFileMapping(INVALID_HANDLE_VALUE, ...)` makes a section backed by
the paging file; `MapViewOfFile` maps it; a name lets another process
open it, and a `Global\` name needs `SeCreateGlobalPrivilege` [26]. A
handle marked inheritable reaches a child started with
`bInheritHandles = TRUE`; but inheritability is the handle's, not the
call's, so in a threaded program one thread's handles leak into another
thread's child. Vista's `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` names
exactly the handles a child gets [27]. The CRT's `fopen` makes
inheritable handles unless the mode has `N` [28]; POSIX `open` has the
same race without `O_CLOEXEC`, which is why the flag exists [29].

### Telling workers: eventfd, condition variables, a counter

An `eventfd` [30] is a counter a loop can wait on; one read consumes it,
so it wakes one waiter, not all (a broadcast needs one per worker, or a
pipe each). A condition variable broadcasts, but it is a pthreads or
Win32 object and needs a mutex. A generation counter read at a safe
point (a request boundary, the top of a loop) needs neither: arc-swap's
`Cache`, Envoy's posted closures, PostgreSQL's "after the current
command" and .NET's per-request snapshot are all this shape.

---

## 3. What breaks

1. **A pointer and a count are two operations.** A reader that loads
   the current pointer and then increments the count it points to can
   do the second after a writer swapped and freed it. This is the whole
   problem RCU, hazard pointers and arc-swap's debts solve [1][3][9];
   `cf_live` solves it with a lock around both (`live.c:740-751`).
   Anything lock-free here must solve it again, or avoid the shared
   count.
2. **A shared count is a shared cache line.** Every `cf_live_get` and
   `cf_config_free` writes the snapshot's count, and every `get` writes
   two lock counters: under many cores these lines bounce between caches
   even when nothing changes. RCU and arc-swap exist because a
   reader-side write does not scale [1][9]. Not yet measured here: the
   benchmark does not time `cf_live_get` under threads.
3. **A ticket lock under preemption.** Tickets are served in order, so a
   waiter that is descheduled when its turn comes stalls every waiter
   behind it, not only the holder [31]. The kernel uses ticket locks with
   preemption off; a userspace spin cannot turn preemption off. With
   more reader threads than cores, `cf_live_get` can wait a time slice.
   The section is two instructions, so it is rare; it is not impossible.
4. **A borrowed pointer outlives its guard.** D-03 rejected thread-local
   borrowed pointers for this [D-03]; arc-swap warns guards must not be
   kept "across async yield points" [10]. A cache that releases the old
   snapshot at the next `get` makes a string taken before that `get` a
   dangling pointer after it.
5. **Delayed reclamation.** A per-thread cache keeps the last snapshot
   until its thread looks again; an idle worker pins an old one forever
   [9]. With QSBR, a reader that never announces a quiescent state
   blocks every grace period [2].
6. **Registration.** liburcu's flavors (but `bp`) need every reader
   thread registered, and `qsbr` needs every reader's cooperation [2]; a
   library cannot know a program's threads, and C89 has no
   thread-local storage or thread-exit hook to do it behind its back.
7. **`fork` with a lock held.** A child forked while another thread
   holds `cf_live`'s ticket lock inherits the lock "held" by a thread
   that does not exist in it, and its first `cf_live_get` spins forever
   [23]. The same for any allocator lock.
8. **`fork` and a shared watcher.** An inotify descriptor survives
   `fork` (`IN_CLOEXEC` closes it only at `exec`): parent and child read
   one queue, and a child that drains it eats the parent's wake-up.
   kqueue descriptors are not inherited by `fork` children, so BSD and
   macOS differ from Linux here.
9. **Copy-on-write undone by counts.** Instagram's lesson [25]: a count
   written into a shared page copies that page into every child. Here
   the count lives in the `cf_config` at the head of its arena
   (`config.c:2186-2197`), so it is one page a child, not the whole
   configuration; still, a "shared" configuration is not entirely
   shared once children take references.
10. **Shared memory that changes after it was checked.** `cf_image_open`
    checks every offset once; if a peer can still write the bytes, a
    later read follows an offset that was never checked (a double
    fetch), and if it can shrink the object, a read past the new end is
    `SIGBUS` [22]. A read-only descriptor of a memfd is not enough on
    its own: the same file can be opened again through
    `/proc/<pid>/fd`, the path `memfd_create(2)` itself names; seals are
    what hold. A regular file mapped while a writer rewrites it in place
    has the same fault; one replaced by `rename` (F-1a's write) leaves
    the mapped inode intact.
11. **Descriptors inherited by accident.** A descriptor or handle open
    without close-on-exec, or inheritable, reaches any child another
    thread starts at that moment [27][29]; the library's own `fopen`,
    `_wfopen` and `open` in `cf_platform_create_private` are such
    windows (section 1).
12. **Descriptor passing loses descriptors quietly.** A control buffer
    too small, or a receiver at its descriptor limit, closes the excess
    and sets `MSG_CTRUNC` [19]; a receiver that does not check reads a
    message with no descriptor in it.
13. **Signal handlers.** A reload started from a `SIGHUP` handler is not
    async-signal-safe: `cf_live_reload` allocates, opens files and takes
    a spin lock the interrupted thread may hold. Only a flag or a write
    to a pipe belongs in the handler [32].
14. **Workers that disagree.** When each process re-reads files itself
    (PostgreSQL [18], a worker running its own `cf_live`), workers
    briefly hold different generations, and a burst of writes can leave
    them on different *contents* if they read at different moments. A
    master that builds once and hands the result out (nginx [14], Envoy
    [12]) does not have this.
15. **What cannot change live, across processes.** nginx and Envoy
    restart workers to change a listening socket [13][14]; PostgreSQL
    marks such parameters and waits for a restart [18]. A child handed
    an image cannot tell a `CF_RESTART` key from another, because the
    image does not carry the contract (section 1).
16. **Secrets in shared memory.** A named Windows mapping is open to any
    process that can open the name [26]; an image carries every value,
    secrets included. An anonymous object reached only by an inherited
    or passed handle is not.

---

## 4. What fits this library

The measures: the C89 core, with atomics only through the port
(`cf_port.h:319-360`) and operating-system calls only in `lib/platform/`
(D-100, D-106: a newer C only per file, named); no thread and no callback
from a thread of the library's (D-14, D-35, D-48: functions it calls run
on the driving thread, inside `cf_live_poll`); a reference the reader
holds (D-03); options that grow through a sized struct (D-51); a new
public name is a decision (D-88).

### A. The read path

| Option | Reader cost | Needs | Rules out |
| ------ | ----------- | ----- | --------- |
| A1. Keep `cf_live_get` as it is | 3 shared writes + 1 on free, a spin | nothing | nothing |
| A2. A reader handle that caches a snapshot, checked by an epoch | 1 shared *read* when nothing changed | `cf_live_reader` (public); an `i32` epoch beside `generation` | a pointer kept past the next `get` on the same handle |
| A3. Hazard pointers inside `cf_live` | 1 store + fence per get, no shared write | a slot per reader thread: a handle anyway, or TLS | nothing more than A2, at more code |
| A4. liburcu | near zero | an LGPL dependency, thread registration, Linux first | Windows, the `minimal` and `misra` profiles |
| A5. A lock-free `cf_live_generation` only | 1 shared read | the epoch; no new name | nothing; the program builds the cache itself |

A2 is arc-swap's `Cache` and Envoy's slot made explicit. A worker opens
a reader once and keeps it (in its own struct, its own thread-local,
wherever it likes). The reader holds one reference to the snapshot it
last saw and the epoch it saw it at. A `get` loads `cf_live`'s epoch
(`cf_ref_get`, an atomic load the port already has); if it is the one
the reader holds, it returns the held snapshot, borrowed, with no write
anywhere. Only when the epoch moved does it call `cf_live_get` (the lock,
once a swap) and release the old one. `live_swap` and the rollback add
one `cf_ref_inc` of the epoch under the lock they already take.

What it costs: about 120 lines of C89 and a test; no new atomic kind,
no new C version, nothing in the platform layer. What it rules out: the
borrowed pointer lives until the next `get` *on that reader* or its
close - not until a reload, which cannot free it. That is narrower than
the option D-03 rejected (a reload on another thread cannot pull it
away), but it is a borrowed pointer, and a worker that keeps a string
from one request into the next after calling `get` again holds a freed
one. One reader is one thread's at a time; the bindings must say so
(Rust: a type that is `Send` and not `Sync`; C++: not copyable). An idle
reader pins the snapshot it last saw (item 5): `cf_live_reader_release`
lets a worker going idle drop it. The epoch is 32 bits and compared for
equality; a reader would have to miss exactly 2^32 swaps to be fooled.

A3 needs the same handle as A2 (C89 has no thread-local storage to hide
a slot in) and then a scan on every swap; it buys nothing A2 does not,
since the driving thread already polls and could free retired snapshots
there. A4 fails D-100's layering and the profiles. A5 is the minimum: it
lets a program write A2 itself, and it is half of A2 anyway.

### B. Telling workers

| Option | Cost | Needs | Rules out |
| ------ | ---- | ----- | --------- |
| B1. Nothing: workers read the generation at a safe point | none (cheap with A2/A5) | A5 | a sleeping worker learns only when it wakes |
| B2. A `swapped` function in `cf_live_options`, on the driving thread | a call after each swap and rollback | two members of `cf_live_options` (D-51) | nothing: it is the program's thread |
| B3. A descriptor that becomes readable on a swap | an `eventfd`/pipe/event per reader | platform code, one per reader (eventfd wakes one) | `none` platform; it is a second watcher |
| B4. Wait for a generation (`futex`, `WaitOnAddress`, condvar) | a blocking primitive | the platform's threads | C89 portability without a platform tier |

B2 fits D-35 and D-48 exactly: the library already calls `check`,
`health` and `log` there, and Envoy's "post a closure to every worker"
[12] is what a program's `swapped` function does with its own loops.
B1 with A2 covers a worker that serves requests: it sees the new
generation at its next request, PostgreSQL's "after the current command"
[18]. B3 and B4 bring threads or descriptors into the platform layer for
a worker that sleeps on nothing else; no user has asked for that.

### C. A configuration handed to another process

| Option | Cost | Needs | Rules out |
| ------ | ---- | ----- | --------- |
| C1. `fork` after building: the child reads what the parent built | none | rules written down, a test | using the parent's `cf_live` in the child |
| C2. An image in a private file, its path to the child | one write, one read and copy | nothing new: `cf_image_write`, `cf_platform_create_private`, `cf_source_image_file` | sharing pages; it is a copy |
| C3. An image in a sealed memfd (Linux), a read-only `shm` descriptor or section handle elsewhere, mapped read-only by the child | one write, one map; pages shared | `lib/platform/` code; a public "shared image" type and a source that maps it | the `none` platform; a child that is not the program's |
| C4. A service of the library's that hands images over a socket | a protocol | a server | D-35's "no thread of the library's" |

C1 works today and only needs to be said: a `cf_config` taken before
`fork` is valid in the child and reads with no lock; its count is
written one page at a time (item 9). The parent's `cf_live` is not the
child's: its lock may be held by a thread the child does not have (item
7), and on Linux its watcher descriptor is shared with the parent (item
8). The child opens its own `cf_live` if it reloads at all.

C2 works today too, and is the portable floor: the parent writes
`cf_image_write(cf_keys(c), cf_sources(c), ...)` into a file created
`0600` (`cf_platform_create_private`), passes the path, and the child
builds with the same contract and `cf_source_image_file` as its source.
The child checks every value against its own definitions, so
`CF_RESTART` and secret flags are the contract's, not the image's (item
15); it parses no text.

C3 is the zero-copy form the image was designed for: page-aligned,
offsets relative, little endian read byte by byte, validated once.
Validated once is only sound if nobody can change the bytes after (item
10): on Linux a memfd sealed `F_SEAL_WRITE | F_SEAL_SHRINK |
F_SEAL_GROW | F_SEAL_SEAL`, and the child refuses one without them; on
FreeBSD 13+ the same `memfd_create`; on macOS and other BSDs a
`shm_open` object unlinked at once, handed over by a descriptor opened
read-only (the parent is trusted, the child cannot write); on Windows an
unnamed section, filled, and a duplicate handle with `FILE_MAP_READ`
only, inherited through `PROC_THREAD_ATTRIBUTE_HANDLE_LIST` [27] (never
a name: item 16). Where none of these is known, it falls back to C2.
How the number reaches the child (an environment variable, as
`LISTEN_FDS` [20] and nginx's `NGINX` [14] do; `SCM_RIGHTS` [19]; the
systemd store [21]) is the program's, and the library only reads a
number it is given.

C4 is a server inside a library, and nginx, Envoy and systemd show that
the process model is the program's to choose.

### D. Hygiene the library owes either way

* Every descriptor and handle the library opens is close-on-exec and
  not inheritable: `fopen` with `e` where the C library has it (glibc,
  the BSDs), `_wfopen` with `N`, `O_CLOEXEC` in
  `cf_platform_create_private` (item 11). Cost: three lines and a test
  that a child sees none.
* T2 of concurrent-access.md: a second driving thread at once is
  refused, not left to corrupt `pending`. One atomic test-and-set in
  `cf_live_poll`, `cf_live_reload`, `cf_live_wait` and the approval
  calls; a refusal with a named reason (D-107).

### What must stay the program's

Starting threads and processes and how many; the event loop; signal
handlers (the library gives `cf_live_reload` to call *after* the handler
set a flag); when a worker picks up a new generation (the request
boundary is the program's to know); draining old workers and restarting
for a `CF_RESTART` key; the transport that carries a descriptor to a
child; who may receive a configuration with secrets in it. nginx,
Envoy, PostgreSQL and systemd each made these choices differently [12]
[14][18][21], and each is right for its program.

---

## 5. Recommendation

### First: say it, and close the leaks (no new public interface)

1. **A Guide page, "Threads and processes"**, with four recipes that
   work on today's interface: a request handler that takes one snapshot
   per request; a `SIGHUP` handler that sets a flag and a loop that
   calls `cf_live_reload` (item 13); `fork` after building, and what the
   child may not touch (C1, items 7-9); an image in a private file
   handed to an `exec`ed child (C2). Each recipe is an example compiled
   in CI, the `fork` one with a test that a child reads a snapshot taken
   before it.
2. **Close-on-exec everywhere the library opens something** (D, first
   bullet), with a test that `exec`s a child and lists its descriptors.
3. **The second driver refused** (concurrent-access.md's T2), with
   D-48's stale sentence about the lock corrected in the same card.
4. **Measure before changing the read path**: `cf_live_get` and
   `cf_config_free` on 1, 2, 4, ... threads up to twice the cores, on
   the benchmark's page. If the numbers show no knee, A2 waits.

### After: the reader handle and the swap function

5. **`cf_live_reader`** (A2), with A5 inside it, if step 4 shows the
   cost or a binding asks. The sketch:

   ```c
   typedef struct cf_live_reader cf_live_reader;

   /* A worker's handle on `l`: open once, keep, close before cf_live_close.
      One thread at a time per reader; any number of readers. */
   CF_NODISCARD CF_API cf_live_reader* cf_live_reader_open(cf_live* l, cf_error* err);
   CF_API void cf_live_reader_close(cf_live_reader* r);

   /* The current snapshot, borrowed: valid until the next call on this
      reader, cf_live_reader_release or cf_live_reader_close. When no swap
      happened since the last call, no shared memory is written. */
   CF_API const struct cf_config* cf_live_reader_get(cf_live_reader* r);

   /* The generation of the snapshot the reader holds. */
   CF_API cf_u64 cf_live_reader_generation(const cf_live_reader* r);

   /* Drop the held snapshot, for a worker going idle. */
   CF_API void cf_live_reader_release(cf_live_reader* r);
   ```

   `cf_live_generation` stops taking the lock in the same card (an
   atomic read of the epoch, the `cf_u64` kept under the lock for the
   status). C++ gets a non-copyable `live::reader`, Rust a `LocalReader`
   that is `Send` and not `Sync`, Python nothing (the GIL makes it moot;
   a free-threaded build is its own card).

6. **`swapped` in `cf_live_options`** (B2), size-versioned (D-51):

   ```c
   /* Called on the driving thread, inside poll, reload, wait, approve,
      rollback and commit, after readers can get `next`. `prev` may be
      NULL. For posting to the program's own workers; it must not call
      back into `l` except cf_live_get and cf_live_generation. */
   typedef void (*cf_live_swap_fn)(void* user, cf_u64 generation,
                                   const struct cf_config* prev,
                                   const struct cf_config* next);
   /* in cf_live_options, after `settle`: */
   cf_live_swap_fn swapped;
   void*           swapped_user;
   ```

   It pairs with 2.2.0's "a reload that names the keys it changed": the
   function gets both snapshots, and that card's diff can be asked of
   them.

### Later, if asked: a shared image (C3)

7. A platform capability that writes an image into an anonymous,
   read-only shared object and maps one it is given, with the seals
   checked on Linux and FreeBSD and a read-only descriptor or handle
   elsewhere; C2 where none. A sketch, for the decision only:

   ```c
   typedef struct cf_image_share {
       size_t size;      /* sizeof, D-51                        */
       long   handle;    /* a descriptor, or a HANDLE's value   */
       const char* base; /* the mapping, once mapped            */
       size_t n;
       void*  reserved[4];
   } cf_image_share;

   /* Parent: the image of `c` in a sealed, read-only shared object. */
   CF_NODISCARD CF_API cf_bool cf_image_share_create(const struct cf_config* c,
                                                     unsigned flags,
                                                     cf_image_share* out, cf_error* err);
   /* Child: map `handle` read-only, refuse it unsealed or writable,
      then cf_image_open. */
   CF_NODISCARD CF_API cf_bool cf_image_share_map(cf_image_share* s, long handle,
                                                  cf_error* err);
   CF_API void cf_image_share_close(cf_image_share* s);
   /* A source over a mapped share, as cf_source_image_mem. */
   CF_API cf_bool cf_source_image_share(cf_source* src, const char* name, int rank,
                                        const cf_image_share* s, void* state,
                                        size_t state_size);
   ```

   Not before a program asks: C2 does the same job with one copy, and
   a configuration is kilobytes (`docs/ImageFormat.md`, "Why it exists").

### Not to build

* **A thread, a server or a callback of the library's** (C4, B3 as a
  thread): D-35 and D-48 hold.
* **liburcu or any RCU dependency** (A4): LGPL, thread registration, and
  Linux first, for a section that is two instructions long.
* **Hazard pointers** (A3): the same handle as A2 and more code.
* **Implicit thread-local caching** (D-03's option 3): it stays rejected;
  A2 is explicit, owned by the worker, and freed by no one else.
* **Reloading the parent's `cf_live` in a forked child**: refused in the
  documentation, not made to work.
* **A configuration in named shared memory**: secrets by name (item 16).

### TO DECIDE

1. **A new public interface for the read path.**
   (a) None: A1, with the measurement on the benchmark's page.
   (b) A5 only: `cf_live_generation` lock-free, no new name.
   (c) A2: `cf_live_reader` as sketched, A5 included.
   *Recommendation: (b) now, as part of the measurement card; (c) when
   the measurement shows a knee or a binding asks for it.* Either way
   it is a borrowed pointer, which D-03 weighed: the maintainer says
   whether a borrow bounded by the worker's own next call is acceptable
   where one bounded by another thread's reload was not.
2. **Workers told of a swap.**
   (a) Nothing: the generation. (b) `swapped` in `cf_live_options`.
   (c) A descriptor per reader.
   *Recommendation: (b), with 2.2.0's changed keys.*
3. **A shared image across processes.**
   (a) Documentation only (C1, C2). (b) The platform capability and
   public type of step 7, behind a build option `CF_WITH_IMAGE_SHARE`
   (off in `minimal`, `misra`, the embedded profiles; on in `server`).
   *Recommendation: (a) now; (b) only on a program's request.*
4. **Does an image carry the contract?** Today it does not, so a child
   must be built with the same definitions. Carrying them is a new
   image section and a minor version (`ImageFormat.md`: "fields only
   grow"); it belongs to the configuration compiler's research, not
   here. *Recommendation: no; the child's own contract decides.*

---

## 6. The cards it implies

| Card | Size | Version |
| ---- | ---- | ------- |
| Threads and processes in the Guide: four recipes, compiled in CI, a `fork` test | M | 1.10.0 |
| Close-on-exec and non-inheritable everywhere the library opens a file, with a child that lists its descriptors | S | 1.9.0 |
| A second `cf_live` driver refused (concurrent-access.md T2), D-48's lock sentence corrected | S | 1.9.0 |
| `cf_live_get` and `cf_config_free` under 1 to 2x-cores threads on the benchmark's page; `cf_live_generation` without the lock (TO DECIDE 1b) | M | 1.8.0 |
| `cf_live_reader`, its test under ThreadSanitizer, C++ and Rust wrappers (TO DECIDE 1c) | M | 2.4.0 |
| `swapped` in `cf_live_options` (TO DECIDE 2) | S | 2.2.0 |
| A shared image: memfd and seals, `shm_open`, a read-only section, the source (TO DECIDE 3b) | L | not placed: on request |

---

## 7. Sources

1. Linux kernel, "What is RCU? - Fundamental Concepts":
   https://www.kernel.org/doc/html/latest/RCU/whatisRCU.html
2. userspace-rcu (liburcu): https://liburcu.org/
3. M. M. Michael, "Hazard Pointers: Safe Memory Reclamation for
   Lock-Free Objects", IEEE TPDS 15(6), 2004:
   https://doi.org/10.1109/TPDS.2004.8
4. P2530R3, "Hazard Pointers for C++26":
   https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2023/p2530r3.pdf
5. Go, `sync/atomic` (`Value`, `Pointer`):
   https://pkg.go.dev/sync/atomic#Value
6. Go, `context`: https://pkg.go.dev/context
7. The Java Language Specification, chapter 17, "Threads and Locks",
   17.4.5 happens-before:
   https://docs.oracle.com/javase/specs/jls/se21/html/jls-17.html#jls-17.4.5
8. Spring Cloud Commons, "Spring Cloud Context: Application Context
   Services" (`@RefreshScope`):
   https://docs.spring.io/spring-cloud-commons/reference/spring-cloud-commons/application-context-services.html
9. arc-swap: https://docs.rs/arc-swap/latest/arc_swap/
10. arc-swap, "Limitations and common pitfalls":
    https://docs.rs/arc-swap/latest/arc_swap/docs/limitations/index.html
11. .NET, "Options pattern":
    https://learn.microsoft.com/en-us/dotnet/core/extensions/options
12. Envoy, "Threading model":
    https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/intro/threading_model
13. Envoy, "Hot restart":
    https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/operations/hot_restart
14. nginx, "Controlling nginx": https://nginx.org/en/docs/control.html ;
    inherited sockets in the source:
    https://github.com/nginx/nginx/blob/master/src/core/nginx.c
15. Apache HTTP Server 2.4, "Stopping and Restarting":
    https://httpd.apache.org/docs/2.4/stopping.html
16. `membarrier(2)`: https://man7.org/linux/man-pages/man2/membarrier.2.html
17. HAProxy, Management Guide (seamless reload, `-x`, `expose-fd
    listeners`): https://docs.haproxy.org/2.8/management.html
18. PostgreSQL, "Setting Parameters":
    https://www.postgresql.org/docs/current/config-setting.html
19. `unix(7)`, `SCM_RIGHTS`:
    https://man7.org/linux/man-pages/man7/unix.7.html
20. systemd, `sd_listen_fds(3)`:
    https://www.freedesktop.org/software/systemd/man/latest/sd_listen_fds.html
21. systemd, "The File Descriptor Store":
    https://systemd.io/FILE_DESCRIPTOR_STORE/
22. `memfd_create(2)`:
    https://man7.org/linux/man-pages/man2/memfd_create.2.html
23. `fork(2)`: https://man7.org/linux/man-pages/man2/fork.2.html
24. A. Baumann, J. Appavoo, O. Krieger, T. Roscoe, "A fork() in the
    road", HotOS 2019:
    https://www.microsoft.com/en-us/research/publication/a-fork-in-the-road/
25. Instagram Engineering, "Copy-on-write friendly Python garbage
    collection":
    https://instagram-engineering.com/copy-on-write-friendly-python-garbage-collection-ad6ed5233ddf
26. Microsoft, "Creating Named Shared Memory":
    https://learn.microsoft.com/en-us/windows/win32/memory/creating-named-shared-memory
27. R. Chen, "Programmatically controlling which handles are inherited
    by new processes in Win32":
    https://devblogs.microsoft.com/oldnewthing/20111216-00/?p=8873
28. Microsoft, `fopen`, `_wfopen` (the `N` mode):
    https://learn.microsoft.com/en-us/cpp/c-runtime-library/reference/fopen-wfopen
29. `open(2)`, `O_CLOEXEC`:
    https://man7.org/linux/man-pages/man2/open.2.html
30. `eventfd(2)`: https://man7.org/linux/man-pages/man2/eventfd.2.html
31. J. Corbet, "Ticket spinlocks", LWN, 2008:
    https://lwn.net/Articles/267968/
32. `signal-safety(7)`:
    https://man7.org/linux/man-pages/man7/signal-safety.7.html
