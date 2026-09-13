# Conventions

The house style. Most of these were paid for: the reason is given where
the reason is a mistake that was made.

## How this document stays true

A document of rules decays. It accumulates entries nobody re-reads,
contradictions nobody notices, and eventually a rule that was right once
and is wrong now. The mitigation is not discipline, which is the thing
that failed in the first place.

> **A rule the build can check should be a gate, not a sentence.**

So when something is learned here, the first question is whether it can
be enforced rather than written. Only what genuinely cannot be checked
stays as prose, and prose rules carry the incident that produced them —
so a later reader can judge whether the rule still applies instead of
obeying it because it is written down.

Rules below marked **[gate]** are enforced by the build; the sentence is
a pointer, not the authority. The rest are judgement, and judgement
should be re-examined.

A gate also has to be told when its measurement stops being valid. The
mutable-global check reads object sections, and AddressSanitizer moves
`const` data into `.data` to instrument it — so the check is skipped
under the sanitiser rather than left to fire on correct code. **A gate
that fires on correct code teaches people to ignore it**, which costs
more than not having it.

## Language

C89 is the baseline and `-std=c89 -pedantic -Werror` is the gate. Where
a toolchain offers more, the portability layer uses it — for
**capability and performance, never for behaviour**. Two code paths that
are never compared become two behaviours, which is why
`CF_FORCE_PORTABLE` runs the whole suite with every upgrade disabled and
compares the output byte for byte.

Consequences worth remembering because the compiler will remind you
anyway: declarations at the top of a block, no `//`, no designated
initializers, no trailing comma in an enum, no `long long` without
detection, no `snprintf`.

## Naming

```text
cf_        public functions and types
CF_        public macros and enumerators
cfi_       internal symbols with external linkage
static     everything else
```

**[gate]** A C library has no namespaces, so the prefix rule *is* the
namespace and `gate.symbols` enforces it: a leaked unprefixed symbol breaks somebody
else's build and is found by whoever links it, not whoever wrote it.

**[gate]** **Give file-local statics a file-specific prefix.** `js_quote` in the
JSON codec, `report_quote` in the report writer. Two `static` functions
with the same name in different files compile fine until the
amalgamation puts them in one translation unit — which is exactly how
`put_value` was found, and why `gate.amalgamation` now catches the next
one.

## Memory

**One arena per configuration.** Allocation is a pointer bump, release
is one call, and there is no per-object bookkeeping anywhere. Snapshots
are immutable and are freed whole, which is what an arena is for.

```text
cf_arena_new     grows by taking blocks from an allocator
cf_arena_fixed   works inside a caller's buffer and fails cleanly
```

Rules:

* **[gate]** Every allocation goes through `cf_allocator`. Nothing calls
  `malloc` outside `alloc.c`, and `gate.symbols` reads the built
  objects to make sure.
* **Text handed to the key space is copied**, so a caller's buffer need
  only survive the call. Text read from a binary image is **borrowed**,
  so the image must outlive the configuration — which is the point on a
  target where the image is a `const char[]` in flash.
* A source that needs real state gets it from the arena and tells the
  caller how much with `cf_source_*_state_size()`. **Never make the
  caller guess the size**: a 256-byte guess was refused by
  `cf_source_image_mem`, and the uninitialised `cf_source` that followed
  crashed.
* **[gate]** **No global mutable state.** `gate.symbols` fails on any
  symbol in `.data` or `.bss`. The test is the section rather than
  `nm`'s letter: under PIE a `const` struct holding pointers lands in
  `.data.rel.ro` and `nm` calls it `d`, so a gate reading the letter
  would fail on correct code and teach everyone to ignore it. A static
  pool inside a source file is global mutable state: not reentrant, not thread-safe, and a direct
  breach of the architectural principle. Simple sources keep state in an
  inline union inside `cf_source`; `ext` is owned, `ref` is borrowed and
  read-only.
* Alignment is checked, never assumed. A caller's `char[]` is usually
  not aligned, and placing a struct at an unaligned address is undefined
  on every target that cares.

## Errors

**One error site.** Everything that can go wrong is detected while the
configuration is being built and reported there. Once a `cf_config`
exists, reading is infallible for any key with a default or a
definition. The alternative — a status on every read — costs three lines
of ceremony per parameter and is why typed configuration libraries feel
heavy.

```c
cf_bool cf_error_set(cf_error* e, cf_code code, const char* key,
                     const char* source, const char* fmt, ...);
```

* Returns `CF_FALSE`, so a call site reads `return cf_error_set(...)`.
* Safe with a NULL error, so no call site needs a guard.
* The struct is caller-supplied and fixed-size: not a thread-local
  last-error, because global error state contradicts a stated principle;
  not an allocation, because the failure path has to work in the
  fixed-capacity profile, which is where allocation fails.
* A message names **the value, the bound and the place to fix it**.
  `server.port: 99999 is outside 1..65535` with `set at: argv[1]` is the
  standard to hold to.

Sources that can fail in one way return a boolean. A source that can
fail in a dozen — a file — keeps a `cf_error` and exposes it through
`cf_source_*_error()`, because replacing a parse error at
`app.toml:14:3` with "could not be read" throws away the line number the
reader needs.

## Project-wide idioms

**Measure or write.** Every renderer takes `(buf, cap)` and returns the
length it *would* have written, so a caller sizes a buffer by rendering
once into nothing. `cf_fmt`, `cf_sink`, `cf_explain`, `cf_inspect`,
`cf_report`, `cf_image_write` all behave this way. Truncation always
terminates and still reports the full length — an early return that
stops counting reports a length short by everything after the first
overflow, which is a bug that was shipped once here.

**`cf_fmt` is small on purpose.** `%s %c %d %i %u %zu %x %%` plus `%I`
(`cf_i64`), `%U` (`cf_u64`) and `%S` (pointer and length). **No width,
no precision, no flags.** Reaching for `%08x` produced a digest that was
the literal format string in every provenance report, and the test only
checked the prefix. If output needs alignment, build it.

**The `.def` X-macro lists.** Enum plus names in one place, because an
enum beside a hand-written switch drifts and the drift is silent. Used
for value types, error codes and candidate outcomes; **not** used for
the codec registry or the source list, where `#if` guards and non-flat
structure would make it less readable rather than more.

## Forbidden

Every locale-sensitive C library function, on every toolchain, at every
standard level:

| Instead of | Use | Because |
| ---------- | --- | ------- |
| `strtod`, `printf("%f")` | `cf_text_to_f64`, `cf_f64_to_text` | the locale's decimal separator |
| `tolower`, `toupper` | `cf_to_lower`, `cf_to_upper` | in a Turkish locale `toupper('i')` is `'İ'` |
| `strcoll` | `cf_cmp_bytes` | ordering must be reproducible |
| `isspace`, `isdigit` | `cf_is_space`, `cf_is_digit` | locale-dependent above ASCII |
| `sprintf` | `cf_fmt` | unbounded at any standard level |

`gate.symbols` checks the built library's undefined symbols against this
list, which works everywhere and does not need a generated locale.

Also absent by rule: variable-length arrays **[gate: `-Wvla`]**,
`complex`, `tgmath.h`, allocation on a failure path, and `#ifdef` on a
compiler or standard macro outside `cf_port.h` and `port.c`
**[gate: `gate.detection`]**.

## Tests

No framework. A test is a C89 program returning non-zero on failure,
with two helpers:

```c
static void check(int cond, const char* what);
static void has(const char* hay, const char* needle, const char* what);
```

A framework would be the library's first external dependency and the
tests are not complicated enough to need one.

**What a good test here looks like:**

* It asserts behaviour, not implementation. `cf_ks_digest` equal across
  two insertion orders, not "the array is sorted".
* Its `what` string says what was expected, so a failure reads as a
  sentence.
* A comment gives the reason when the reason is a decision. "Not
  booleans here. See D-24." earns its line; "check the port" does not.
* **A weak assertion is worse than none.** Checking that a digest starts
  with `fnv1a:` passed for a whole session while every digest was the
  literal `%08x%08x`. Assert the shape *and* that it distinguishes.
* **[gate]** Buffers over a kilobyte are `static`. The stack budget
  applies to tests too, and it has caught three of them.
* **[gate]** Test data that is a string literal must be a mutable array
  when it is handed to an API taking `char* const*`, because
  `-Wwrite-strings` is on and because the API matches `argv` so that callers never cast.

**Negative tests for gates.** A gate is proved by breaking the thing it
protects and watching it fail, then restoring. Do this once per gate and
say so.

## Working practice

Two rules earned during this work:

**Batch what verifies together.** Combine operations that are
independent and whose outcome one check can attribute. Keep them
separate when one can mask another's failure. End every batch with a
verification whose output names the part that failed — a list of gate
names, not `100% passed`.

**Undo narrowly.** `git checkout -- <directory>` reverted a whole
directory to fix two files and silently took a turn's worth of other
work with it. Revert files, not directories.

**Verify with `cmake -P scripts/verify.cmake`, not by hand.** Chaining the steps
manually got a failing tree committed once: `ctest | grep passed`
matched the word inside "98% tests passed" and the commit went through.
A checkpoint of a broken tree is worse than no checkpoint. The script exits non-zero on the first failure and prints that step's
output, so the failure is attributable without re-running anything.

It is written in CMake rather than in a shell on purpose. A shell script
covers half the platforms this library claims, and the other half would
need a batch file kept in step with it — which is the duplication the
hand-written Makefile was deleted to avoid. CMake is already required to
build, so the driver adds nothing.

**Zero a struct whole, not field by field.** Adding two fields to
`cf_defset` and initialising one of them segfaulted every caller that
built one on the stack. Field-by-field initialisation is correct exactly
until someone adds a field, and the compiler says nothing.

**Use the exit code, never the output.** Two commits here went in on a
broken tree because the verification was piped to `grep`, and `grep`
succeeds when it *finds* the word "FAILED". The only correct shape is

```sh
cmake -P scripts/verify.cmake > /dev/null 2>&1 && git commit ...
```

Reading the log is for diagnosing a failure, not for deciding whether
there was one.

**Read the failure, not the summary.** `ctest | grep 'Failed'` missed a
`SEGFAULT` line for one run, because the word ctest prints for a crash
is not the word it prints for a failed assertion. `scripts/verify.cmake`
shows the step's whole output for this reason.

**A static archive hides a missing definition.** A function declared in
a header, called from one file and defined nowhere links fine into
`libconfigforge.a` and stays hidden until something pulls that object
in. The amalgamation is one translation unit, so `gate.amalgamation`
finds it on the next run — which is a second reason that gate is worth
its second of build time.

**Patch the formatted text.** The build files are rewritten by
`cmake-format`, so a search-and-replace written against the shape a file
had before the formatter touched it silently matches nothing. Two edits
were lost that way. Assert that the patch target was found.

**Commit each verified unit.** Not each file and not each session: each
thing that is true on its own. `scripts/verify.sh` first, always.
