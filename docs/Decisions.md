# ConfigForge — Open Design Decisions

This is the **Phase 0** artifact. No implementation begins until every decision below
has a recorded status.

Each decision here shares one property: **making it late costs a rewrite.** They touch
the snapshot layout, the ABI surface, or the meaning of `explain()`. Ordinary design
questions are not in this document; they belong in the specification and can be
revised freely.

## How to Use This Document

Each entry has a fixed shape:

```text
Question      what has to be decided
Options       the real alternatives, not a list of everything imaginable
Trade-off     what each option costs
Recommendation the position argued for, with reasoning
Status        Open | Decided | Deferred
Consequence   what becomes fixed once it is decided
```

When a decision moves to `Decided`, record the date and the reasoning actually used,
including the reasoning that was rejected. A decision log without rejected reasoning
gets reopened every six months.

---

## Decision Index

| ID | Subject | Blocks | Status |
| -- | ------- | ------ | ------ |
| D-01 | Type set and composite support | value model, ABI, mapping | **Decided** |
| D-02 | Merge semantics for composites | resolver, snapshot layout | **Decided** |
| D-03 | Snapshot ownership and string lifetime | ABI, hot path, watch | **Decided**, closed |
| D-04 | Definition representation in C | ABI, builder ergonomics | **Decided**, closed |
| D-05 | Validation failure behaviour | resolver, error model | **Decided**, closed |
| D-06 | Key naming, case and normalisation | mapping, every source | **Decided**, closed |
| D-07 | Allocation and freestanding model | core, all constrained targets | **Decided**, closed |
| D-08 | Error model shape | ABI, every API | **Decided**, closed |
| D-09 | Zero-config entry point | public API, principles | **Decided**, closed |
| D-10 | Value interpolation | resolver, provenance | **Decided** |
| D-11 | Unknown-key behaviour | resolver, diagnostics | **Decided**, closed |
| D-12 | Profiles as a core concept | policy model, Doctor | Open |
| D-13 | Sensitivity and redaction | diagnostics, report artifact | **Decided**, closed |
| D-14 | Thread-safety contract | ABI, snapshot | **Decided** |
| D-15 | Text encoding boundary | platform providers, ABI | **Decided**, closed |
| D-16 | Provenance report schema | Doctor, diagnostics, Report Replay | **Decided** |
| D-17 | Reference parser and dependency stance | build, portability | **Decided**, closed |
| D-18 | Project name availability | everything public | **Decided** |
| D-19 | **Implementation language and standard** | every other decision | **Decided** |
| D-20 | Location of the error surface | every read call site | **Decided**, closed |
| D-21 | Distribution form | adoption, build | **Decided**, closed |
| D-22 | Opportunistic feature substitution policy | portability layer, determinism | **Decided** |
| D-23 | Format set and the parser seam | every source, build options | **Decided** |
| D-24 | Type inference for typeless sources | resolver, Tier 0 behaviour | **Decided** |
| D-25 | Floating-point text conversion algorithm | determinism, M0 size | **Decided** |
| D-26 | External dependency policy per format | build, portability | **Decided** |
| D-27 | Serialization layer as a separate library | packaging, reuse | **Decided** |
| D-28 | Binary serialization: whether, and what shape | freestanding, boot time | **Decided** |
| D-29 | Command line portability | every consumer | **Decided** |
| D-30 | Which sources exist, and which are deferred | scope | **Decided** |
| D-31 | The no-build-system path | distribution, maintenance | **Decided** |
| D-32 | The compiler warning standard | every build, every compiler | **Decided** |
| D-33 | Per-parameter policy: shape, and what a violation does | resolver, security | **Decided** |
| D-34 | What `set()` means on an immutable snapshot | ownership, provenance | **Decided** |
| D-35 | Change notification: what the library hands back | threading, contract | **Decided** |
| D-36 | Licence | distribution, every user | **Decided** |
| D-37 | Limits and the stack budget: whose trade it is | memory, embedded, every builder | **Decided** |
| D-38 | Leaving parts out: what a missing part does | size, embedded, every builder | **Decided** |
| D-39 | YAML: the subset, and who types a value | formats, provenance | **Decided** |
| D-40 | Where the version is written, and the commit a build is from | releases, packaging, provenance | **Decided** |
| D-41 | Doctor's rules as a library | Doctor, embedding | **Decided** |
| D-42 | Authorization for persistent writes | stores, policy, security | **Decided** |
| D-43 | A renamed key | definitions, provenance, Doctor | **Decided** |
| D-44 | The Rust binding | bindings, build, safety | **Decided** |
| D-45 | Build limits in the ABI | ABI, bindings, 1.0 | **Decided** |

**D-19 is closed.** It was the only decision that changed the shape of all the others,
and closing it settled D-01, D-02 and D-22 with it. The remaining entries can now be
decided independently.

---

## D-01 — Type Set and Composite Support

**Question.** Which value types exist, and does the model support arrays and nesting?

**Options.**

1. Flat key space, scalar types only, structure as a key naming convention.
2. Scalars plus arrays of scalars.
3. Scalars, arrays and nested tables as real composite values.

**Trade-off.** Option 3 is the most expressive and costs a recursive resolver, composite
merge semantics, composite lifetime management in C, composite accessors in the API, and
a provenance model in which a value's origin is a set of origins. Option 2 is the option
people regret, because the first thing anyone wants after an array of scalars is an array
of tables.

Option 1 looked like a compromise and turns out to be what the two best origin-tracking
implementations in the industry actually do. .NET's `IConfiguration` is a flat key space
with `:` separators and Spring's `PropertySource` is a flat map with `.` separators. Both
flatten nested YAML and JSON on load and reconstruct structure only at binding time, and
both get clean per-key provenance as a direct consequence.

**Decision.** Option 1.

```text
key space   sorted flat map, string key to typed value
types       CF_NULL, CF_BOOL, CF_INT (64-bit), CF_REAL (64-bit), CF_TEXT (UTF-8)
structure   naming convention: server.listeners.0.port
arrays      index keys plus a synthesised <key>.count
```

A 64-bit integer type is required; its absence is a build error rather than a silently
narrowed `CF_INT`, because a value model that varies by platform makes cross-platform
determinism impossible.

**Consequence.** Removes composite merge, composite accessors and composite lifetime
rules from the design entirely. Makes a C implementation reasonable rather than painful.
Makes the snapshot a sorted array that serialises directly into the provenance report.
Introduces the array truncation hazard handled in D-02. Costs one convenience: populating
a whole struct in a single call now belongs to an optional binding layer, exactly as it
does in .NET and Spring.

---

## D-02 — Resolution Granularity and Arrays

**Question.** With a flat key space, what is left of the merge problem?

**Answer.** For scalars, nothing. Every key resolves independently against the precedence
chain, and what would have been deep merge in a nested model is the default behaviour. An
override file supplies only the keys it changes and never restates a whole table.

Arrays are the exception, because length is a property of the whole array rather than of
any element. If a high-precedence source supplies two elements and a low-precedence source
supplies four, the last two survive as ghosts. This is a real and documented defect in
.NET's configuration binding, and inheriting the flat model means inheriting the hazard.

**Decision.**

```text
scalars   per-key resolution against the precedence chain
arrays    the winning source for <key>.count fixes the length
          index keys beyond that count are discarded during resolution
          discarded elements remain visible in explain() as rejected candidates
          an opt-in append mode exists per definition
```

**Consequence.** Provenance is per key and has one shape for every parameter in the
system, which is what allows the diagnostics output to be specified as a stable, testable
contract. The truncation rule requires array-aware sources to emit a count, and it becomes
a dedicated Doctor finding and a required test fixture.

---

## D-03 — Snapshot Ownership and String Lifetime

**Question.** When `get()` returns a string, who owns the memory, and what happens to
that memory when watch replaces the snapshot?

**Options.**

1. Copy out on every read. Safe, allocates on the hot path.
2. Borrowed pointer into the snapshot with an explicit acquire/release handle.
3. Thread-local current snapshot, borrowed pointers valid until the next reload on
   that thread.

**Trade-off.** Option 1 contradicts the stated performance goal. Option 3 is fast and
implicit, and it fails exactly when a value is captured across a reload boundary; the
failure is a dangling pointer in production, not a compile error. Option 2 requires
the user to hold a handle, which is one extra concept.

**Recommendation.** Option 2, with reference-counted snapshots and atomic pointer
replacement on reload, paired with a copy-out convenience call for Tier 0.

```text
cf_get_text(cfg, key, def)                    borrowed; valid until reload or free
cf_get_text_copy(cfg, key, buf, n, def)       copied; always safe
cf_snapshot_text(snap, key, def)              borrowed; valid while the handle is held
```

Three forms rather than two, because in C the safe option has to be explicit. Note that
without watch enabled there is exactly one snapshot for the life of the configuration, so
the first form has no hazard at all and Tier 0 can ignore the subject.

**Consequence.** Fixes the ABI shape (`cf_context_acquire` / `cf_snapshot_release`),
fixes what watch is allowed to do, and determines whether the destruction of an old
snapshot can be on the hot path. It also means a `Config`-level read cannot return a
`string_view`.

**Closed in implementation.** One arena per configuration, released whole. Text handed
to the key space is copied, so a caller's buffer need only survive the call; text read
from a binary image is borrowed, so the image must outlive the configuration - which is
the point on a target where the image is a `const char[]` in flash. Snapshots are
reference counted (D-14) so a reader can hold one while another is built.

---

## D-04 — Definition Representation in C

**Question.** How is `define<int>("server.port").defaultValue(8080).range(1,65535)`
expressed once a C ABI exists?

**Options.**

1. Opaque builder handle with one call per attribute, then a commit call.
2. A descriptor struct filled by the caller and registered in one call.
3. Definitions declared only through a declarative format, never programmatically.

**Trade-off.** Option 1 is easy to extend and produces long C call sequences and a
mutable-until-commit object with an ill-defined intermediate state. Option 2 is a
single call and freezes the attribute set into a struct layout, which is an ABI
compatibility problem the first time an attribute is added; the standard mitigation is
a size field and versioned struct. Option 3 removes programmatic definitions, which
breaks Tier 1.

**Recommendation.** A versioned, size-prefixed descriptor struct as the extensible
mechanism, plus convenience functions for the shapes people actually write.

```c
cf_def def;
cf_def_init(&def, CF_TYPE_INT, "server.port");   /* memset + size + type + key */
def.i.value = 8080;
def.i.min   = 1;
def.i.max   = 65535;
cf_define(b, &def, &err);

/* identical effect, for the common case */
cf_def_int(b, "server.port", 8080, 1, 65535, 0, &err);
```

C89 has no designated initializers, so a descriptor filled positionally would be fragile
and one filled purely by assignment would be verbose. `cf_def_init` followed by field
assignment is safe and explicit, and the convenience functions cover the ordinary cases.
Adding an attribute later means growing the struct and bumping `size`, never changing a
signature.

Option 1 is rejected explicitly rather than left open, because a partially built
definition is a state that then needs its own error semantics.

**Consequence.** Determines whether definitions are mutable after creation, and fixes
the extension mechanism for definition attributes for the life of the ABI.

**Closed in implementation.** A `cf_def` struct carrying its own `size`, so fields can
be added without changing a signature or a layout - `aliases` and `policy` were both
added that way after the struct shipped. Builder helpers cover the common shapes;
`cf_builder_define` takes the struct for everything else.

---

## D-05 — Validation Failure Behaviour

**Question.** A source provides a value that fails conversion or a validation rule.
Does resolution fail, or does it continue with the next source?

**Options.**

1. Strict: resolution fails.
2. Fallback: reject the value, continue down the precedence chain.
3. Per-parameter, with one as default.

**Trade-off.** Fallback as default means a malformed production environment variable
silently degrades to a development default, and the application starts in a state
nobody chose. It is also nearly undiagnosable without `explain()`. Strict as default
means a single bad value in a low-priority system file can prevent startup, which is
occasionally inconvenient.

**Recommendation.** Option 3, default Strict. Strict is consistent with the project's
central premise. Fallback stays available per parameter, and a rejected value remains
in the provenance chain with its rejection reason rather than being erased.

**Consequence.** Determines whether the resolver is fail-fast or collecting, and
determines whether rejected values are retained in the snapshot.

**Closed in implementation.** A candidate that fails its contract is marked rejected
with a reason and stays in the chain; the next candidate is selected, which is usually
the default. Construction fails only when nothing usable is left and the parameter is
required. The rejected value and the reason both appear in `explain()`, which is what
makes "why is this the default" answerable.

---

## D-06 — Key Naming, Case and Normalisation

**Question.** What is the canonical key form, and how do `server.port`,
`MYAPP_SERVER_PORT`, `--server-port` and `server_port` relate?

**Options.**

1. Canonical dotted lowercase; all source forms derived mechanically; comparison
   case-sensitive.
2. Case-insensitive comparison everywhere.
3. Explicit mapping only; no automatic derivation.

**Trade-off.** Case-insensitivity is Viper's most notorious defect: it silently
collapses distinct keys and behaves differently across sources. Fully explicit mapping
is unambiguous and adds a line of boilerplate per parameter, which is repetition. The
existing specification already states that defining `server.port` must not create
`SERVER_PORT`, which is a rule about *creation*, not about *lookup*, and the two are
worth keeping distinct.

**Decision.** Option 1 with mechanical derivation for lookup and an optional explicit
override per parameter. Case-sensitive comparison. Keys are ASCII
`[a-z0-9_]` per segment; anything else must be quoted. Derivation rules are fixed and
published:

```text
canonical       server.tls_min_version
environment     <PREFIX>_SERVER_TLS_MIN_VERSION
command line    --server-tls-min-version
file            server.tls_min_version
registry        Server\TlsMinVersion        (provider-specific, explicit)
```

**Consequence.** Fixes every source mapping, fixes whether two keys can collide, and
determines whether a key can be reconstructed from an environment variable name
(needed for unknown-key detection in D-11).

**Closed in implementation.** The escape turned out to be `__`, matching .NET, and the
natural single-underscore spelling is accepted as soon as a definition set exists: the
key list is what removes the ambiguity, so Tier 1 takes the spelling people actually
write while Tier 0 keeps only the one that can be enumerated.

Both are recorded when both are set, and the escaped one wins on order. Setting the
same parameter twice in two spellings then shows up in `explain()` as a shadowed
candidate rather than disappearing, because it is a mistake worth seeing.

The mapping lives in `cf_keymap`, separate from the lexers, precisely so the reverse
direction exists: given a key, ask a source what *it* would call it. Without that
separation the natural spelling could not have been added without editing the
environment parser.

---

## D-07 — Allocation and Freestanding Model

**Question.** Does the core allocate, and how does it behave with no heap?

**Options.**

1. Standard allocation, no constrained-target support.
2. Pluggable allocator.
3. Pluggable allocator plus a fixed-capacity mode backed by a caller-supplied arena.

**Trade-off.** Option 3 is the only one compatible with the stated bare-metal goal,
and it constrains the implementation permanently: no hidden `std::string`, no growth
assumptions, every container bounded, resolution able to fail on exhaustion. Adding
this later means rewriting every container in the core.

**Recommendation.** Option 3, with the cost split deliberately.

The **discipline** is adopted from the first commit: pluggable allocator, no locale
dependency, bounded internal containers, no hidden allocation. This is cheap to do
early and expensive to retrofit, because it means rewriting every container.

The **validation** is deferred. A freestanding CI target arrives in Phase 3 for the
constrained profile and in Phase 5 for true bare metal, when a consumer exists.
Claiming a target that nothing runs on is an unvalidated promise that constrains every
later decision, which is the same error as publishing an ABI with no binding.

**Consequence.** Fixes the internal container set, requires a locale-independent numeric
parser, forces every allocation through one interface, and makes the contiguous snapshot
layout in the specification a requirement rather than an optimisation.

**Closed in implementation.** Everything allocates through `cf_allocator`; `gate.symbols`
fails if any object but `alloc.c` references `malloc`. `cf_arena_fixed` works inside a
caller's buffer and fails cleanly when it runs out, and `CF_PLATFORM_NONE` reports every
platform capability as absent rather than pretending. The binary image (D-28) is the
artifact that makes a filesystem-free target work at all.

---

## D-08 — Error Model Shape

**Question.** What does an error look like at the ABI and in C++?

**Options.**

1. Integer result codes plus a thread-local last-error.
2. Result codes plus a caller-supplied error struct.
3. An allocated error object with a destroy call.

**Trade-off.** Thread-local last-error is familiar and is global state, which
contradicts a stated principle and behaves poorly under a fixed-capacity model. An
allocated error object allocates on the failure path, which conflicts with D-07.
A caller-supplied struct is verbose and allocation-free.

**Recommendation.** Option 2, with a fixed-size, size-prefixed error struct carrying
code, parameter key, source identifier and a bounded message buffer, passed as the last
parameter of every fallible function.

The C++03 wrapper stores it inside the `Config` handle so the check reads `if (!cfg)`.
C++03 has no `Result` type and adding one to the baseline header would be a template
exercise with no payoff; a throwing accessor is offered where exceptions are acceptable.
Other-language bindings map the struct onto their own idiom. The struct is the wire
format, not the ergonomics.

**Consequence.** Appears in every fallible ABI function signature and cannot be
changed afterwards.

**Closed in implementation.** A caller-supplied fixed-size `cf_error`: not a
thread-local last-error, because global error state contradicts a stated principle, and
not an allocation, because the failure path has to work in the fixed-capacity profile,
which is exactly where allocation fails. `cf_error_set` returns `CF_FALSE` so a call
site reads `return cf_error_set(...)`, and is safe with a NULL error so no call site
needs a guard.

---

## D-09 — Zero-Config Entry Point

**Question.** Can a program read configuration in one line, given that the
architectural principles forbid implicit global state?

**Options.**

1. No zero-config path; a context must always be constructed explicitly with sources.
2. `cf::defaults()` returning an ordinary explicit context with standard sources.
3. A true process-wide global with implicit initialisation.

**Trade-off.** Option 1 protects the principle and makes the smallest working program
noticeably longer than dotenv's, which affects whether anyone tries the library at
all. Option 3 violates a stated principle and creates initialisation-order problems.

**Recommendation.** Option 2. The tension is real and the resolution is precise: the
*object* stays explicit, only its *construction* is short. This should be written into
the specification as a worked example so that the principle is not later read as
forbidding convenience constructors.

**Consequence.** Defines Tier 0 and therefore the first page of the documentation.

**Closed in implementation.** `cf_app(name, argc, argv, &err)` wires the command line,
the environment with the derived prefix, and the definition defaults, and now also
expands `@file` response files and takes UTF-8 arguments on Windows. Tier 0 needs no
definitions at all.

---

## D-10 — Value Interpolation

**Question.** Are references such as `${OTHER_KEY}` or `${env:HOME}/cache` supported?

**Options.**

1. No.
2. Yes, resolved during resolution.
3. Deferred to a later phase, with syntax reserved now.

**Trade-off.** Interpolation is genuinely useful and it damages provenance: the origin
of an interpolated value is a set of origins plus a template, not a source. It also
introduces cycles, an evaluation order, and a question about whether interpolation
happens before or after precedence. Hydra and systemd both support it and both carry
the resulting complexity visibly.

**Recommendation.** Option 3. Reserve `${...}` as a syntax that is currently rejected
with a clear error rather than silently treated as a literal, so that adding it later
is not a breaking change. Do not implement it before the provenance model has proven
itself on the simple case.

**Consequence.** Determines whether a value's origin is a single source or a
composition, which is the difference between a simple and a recursive `explain()`.

**The reservation is enforced; the feature is still open.** For a long time this entry
recommended refusing `${` and nothing refused it, so a value such as `${HOME}/cache`
was taken literally - the exact outcome the recommendation existed to prevent. It is now
an invalid value under D-05: strict by default, with `explain()` showing the refusal for
a parameter that asked for fallback. `$${` is the literal. Three boundaries were drawn
while doing it, each because the alternative was wrong rather than inconvenient:

* only what somebody *wrote* is checked - files, the environment, the command line, a
  derived value. A program's default is its own code and changes with it.
* a value replayed from a report or loaded from an image is not checked again, because
  it was resolved once already; an escaped `$${` that became `${` would otherwise be
  refused on replay.
* the escape is undone once, at construction, so what a program reads and what a report
  records is what was meant.

**Decided: references to keys, after precedence.** The provenance model has proven
itself on the simple case - every source, codec and adapter since the reservation has
explained itself without exception - so the condition this entry set is met, and
`${key}` now means the value of another key. Four choices make it explainable rather than
a second resolver:

* **Precedence first, references after.** Every key's winner is chosen as if nothing
  referred to anything; then each winner with references has them replaced, a referenced
  key being finished first. The alternative - references inside resolution, so a
  reference could pick a candidate other than the winner - is what makes Hydra's and
  systemd's evaluation order something a user has to learn. Here a reference reads
  exactly what a program reading that key would get, and `explain()` of the referenced
  key is the whole story of the part it supplied.
* **The origin is the template and its keys.** The candidate keeps its text as written;
  `explain()` shows it (`selected, from "http://${server.host}:${server.port}"`) and the
  report records it as `written`, an added field. The parts explain themselves under
  their own keys, so `explain()` stays one level deep - the "recursive `explain()`" this
  entry worried about is a reader following names, not a tree the library prints.
* **Keys only.** `${env:HOME}` would be a second environment source with no name in
  `explain()`, and the environment is already a source; `${HOME}`, which names no key, is
  still refused, so the case the reservation was for - a directory called "${HOME}" - is
  still caught.
* **Typed after.** The result is text, typed by the contract like a value from the
  environment, so `workers = ${base.workers}` is an integer and a result the contract
  refuses falls back like any refused value.

Refused, naming the key: a reference to a key with no value, a cycle, and a key that is
not `CF_SENSITIVE` referring to one that is, because its value would show the secret - a
sensitive key may refer to another, and neither its value nor its template is shown.
Only the winner is expanded; a template that lost stays in the chain as written. A
replayed or derived value was expanded once and is not read again.

---

## D-11 — Unknown-Key Behaviour

**Question.** A source contains a key no definition describes. What happens?

**Options.**

1. Always ignore.
2. Always reject.
3. Per source instance: ignore, warn or reject.

**Trade-off.** Always rejecting breaks shared and system-wide files that legitimately
contain keys for other consumers. Always ignoring loses the single highest-value
diagnostic available: typo detection against the definition set.

**Recommendation.** Option 3, default ignore, with edit-distance matching in warn and
reject modes so that an unknown key is reported as a suggestion. This depends on D-06,
because reverse mapping from an environment variable name to a canonical key is only
possible if derivation is mechanical.

**Consequence.** Requires sources to enumerate their keys, which is a capability not
every source has. Feeds rule CFD005 in Doctor.

**Closed in implementation.** `CF_UNKNOWN_IGNORE`, `CF_UNKNOWN_WARN` and
`CF_UNKNOWN_REJECT`, with a Levenshtein suggestion on rejection - `did you mean
'server.port'?`. `cf_builder_open` exempts a prefix, because a plugin's settings cannot
be in the program's contract when the plugin is not in the program either; without that,
strict rejection and a dynamic key space were mutually exclusive.

---

## D-12 — Profiles as a Core Concept

**Question.** Are named environments (`dev`, `staging`, `prod`) a first-class concept
or just a policy layer?

**Options.**

1. Not a concept; users select files themselves.
2. First-class: an active profile selects sources and policy layers.
3. A conventional policy layer with no special support.

**Trade-off.** Spring and figment both make profiles first-class and both benefit from
it. Making profiles core adds a cross-cutting concept to every source and policy
decision. Doctor's comparison story is materially easier if a report carries a profile
name, and materially weaker if environments are only identified by filename.

**Recommendation.** Option 3 for the library, plus a required **profile label** field
in the provenance report artifact. This keeps the core small while giving Doctor what
it needs. Revisit only if the policy layer approach proves clumsy in real use.

**Consequence.** Determines whether `explain()` output mentions a profile, and shapes
the report schema in D-16.

---

## D-13 — Sensitivity and Redaction

**Question.** The specification's own example uses `database.password`. What does
`explain()` print?

**Options.**

1. Everything; diagnostics are a debugging tool.
2. A `sensitive` flag on the definition; value redacted in diagnostics and reports,
   origin preserved.
3. Heuristic redaction by key name.

**Trade-off.** Option 1 turns a diagnostic feature into a credential leak, especially
once reports are written to files or CI logs. Option 3 is a guess and will both
over-redact and miss.

**Recommendation.** Option 2. Redaction preserves presence, origin, length and a
salted hash, and omits the value. This is exactly what makes Doctor's value-blind mode
possible, so the two features share one mechanism. The security principles section
already commits to redaction; this decision makes it a definition attribute rather
than an aspiration.

**Consequence.** Adds a definition attribute (bearing on D-04's struct layout) and
fixes the redacted representation in the report schema.

**Closed in implementation.** `CF_SENSITIVE` redacts in `explain()`, `inspect()` and the
value-blind report, which keeps presence, origin, length and a digest - everything an
audit needs and nothing a leak would want. The digest distinguishes: there is a test that
a different secret gives a different one, written after a session in which every digest
was the literal format string and the test only checked the prefix.

---

## D-14 — Thread-Safety Contract

**Question.** What is guaranteed about concurrent use?

**Options.**

1. Nothing; callers synchronise.
2. Reads are safe concurrently; mutation and reload require external synchronisation.
3. Fully internally synchronised.

**Recommendation.** Option 2, stated explicitly. Concurrent reads are the entire point
of an immutable snapshot; full internal synchronisation would put a lock on the hot
path and contradict the performance model. The contract must be written down, because
an unstated threading contract is assumed to be option 3 by users and option 1 by
implementers.

**Consequence.** Determines which operations need atomics and whether a
single-threaded build can compile them out.

**Decided: option 2, and written into the public header rather than here.**

```text
Reads are safe concurrently.
Building and releasing are the caller's to synchronise.
```

Reads need nothing to be safe, because there is nothing to protect: a resolved snapshot
is immutable and a read is a binary search over a sorted array. Locking that path would
contradict the performance model and buy nothing.

**A snapshot is reference counted now, which is stronger than the contract requires.**
The type and the atomics already existed in the portability layer and were tested;
using them costs fifteen lines and gives the reload story a foundation. Retrofitting
ownership later is the same class of mistake as retrofitting the provenance chain —
possible, and it means rewriting the thing that was built without it.

Two details the implementation had to get right. The count starts at one explicitly:
`calloc` left it at zero, which is correct exactly as long as nobody takes a second
reference. And the count is decremented *before* the arena is released, with the result
read from a local — the configuration lives inside the arena it frees, so touching it
afterwards is a use-after-free of the object doing the freeing.

**The test states the contract where threads do not exist.** `test_threads` runs every
read path twice and compares the whole snapshot, provenance chain included, byte for
byte. That proves the property the contract rests on without needing a second thread,
and where pthreads are present it also runs eight readers over sixteen thousand rounds.

---

## D-15 — Text Encoding Boundary

**Question.** What encoding does the core use, and where does conversion happen?

**Recommendation.** UTF-8 internally on every platform, without exception. Conversion
lives inside the platform provider: the Windows provider converts at the Registry and
wide-character API boundary, and file providers validate and reject invalid UTF-8
rather than passing bytes through. The ABI takes and returns UTF-8 with explicit
lengths, never null-terminated wide strings.

**Consequence.** Every string in the ABI carries a length, which also serves the
embedded-string case in D-07.

**Closed in implementation.** UTF-8 throughout, validated nowhere and passed through
everywhere, with one exception that matters: `cf_platform_args` reads the wide command
line on Windows, because `argv` there arrives in the active code page and what the code
page dropped cannot be converted back. JSON `\u` escapes are decoded to UTF-8 including
surrogate pairs; a lone surrogate is refused.

---

## D-16 — Provenance Report Schema

**Decision.** JSON, versioned from v1, producible with values omitted, and readable by the
library itself because Report Replay consumes it.

```json
{
  "configforge_report": 1,
  "mode": "full",
  "profile": "production",
  "application": { "name": "myapp", "version": "2.3.1" },
  "library":     { "version": "0.1.0", "features": ["ini","json","toml"] },
  "generated":   "2026-09-11T08:14:22Z",
  "policy": {
    "precedence": ["cli", "env", "user-file", "system-file", "default"],
    "overrides":  { "db.password": ["env"] }
  },
  "keys": {
    "server.port": {
      "definition": {
        "type": "int", "required": false, "default": 8080,
        "min": 1, "max": 65535,
        "sensitive": false, "deprecated": null, "array_mode": null,
        "description": "Listening port"
      },
      "candidates": [
        { "source": "cli",       "present": false },
        { "source": "env",       "present": true,  "value": 8080,
          "at": "MYAPP_SERVER_PORT" },
        { "source": "user-file", "present": true,  "value": 3000,
          "at": "app.toml:14:3", "outcome": "shadowed" },
        { "source": "default",   "present": true,  "value": 8080 }
      ],
      "selected": { "source": "env", "reason": "highest precedence with a value" },
      "warnings": []
    },
    "db.password": {
      "definition": { "type": "text", "required": true, "sensitive": true },
      "candidates": [
        { "source": "env", "present": true, "redacted": true,
          "length": 24, "digest": "sha256:7f3a…" }
      ],
      "selected": { "source": "env", "reason": "only source with a value" },
      "warnings": []
    }
  },
  "unknown_keys": [
    { "key": "server.pot", "source": "user-file", "at": "app.toml:12:1",
      "nearest": "server.port", "distance": 1 }
  ]
}
```

**Rules that the schema enforces structurally rather than by inspection.**

1. `mode` is `"full"` or `"blind"` at the top level. A blind report may not contain a
   `value` field anywhere; a full report may. A reader can reject a mismatch without
   scanning the whole document, which matters because a full report is a
   credential-bearing artifact.
2. `library.features` records the compiled-in format set, so Doctor can distinguish "this
   source was unreadable by that build" from real drift.
3. Every candidate carries `present`, and a present candidate that did not win carries an
   `outcome`: `shadowed`, `rejected` with a reason, or `truncated` for discarded array
   elements. Nothing is dropped from the chain.
4. `at` is a free-form location string whose format is owned by the source kind:
   `file:line:col` for documents, the variable name for the environment, the argument index
   for the command line, the row id for a store.
5. Redaction preserves `length` and `digest` and omits `value`, which is what makes
   value-blind Doctor rules work at all.

**Consequence.** This artifact is the interface between the library and Doctor, between
ConfigForge and any foreign-ecosystem adapter, and — through Report Replay — a source
format the library must be able to read. That last point pulls a small JSON reader into
Phase 1. Changing the schema later breaks all three consumers.

---

## D-17 — Reference Parser and Dependency Stance

**Question.** The core has zero mandatory dependencies and is C89. What parses the file
source?

**Correction.** An earlier version of this entry proposed `toml++` as the default
provider. That was wrong and is recorded rather than quietly removed: `toml++` is a C++17
library and cannot be called from a C89 core. The option set below is the C one.

**Options.**

1. A built-in minimal parser written for this project.
2. A vendored C parser for a richer format.
3. An optional provider linking an external C parser.

**Trade-off.** The constrained and freestanding profiles need a parser with no allocation
requirement that fits in a small binary, which rules out most full-format parsers. The
hosted profile wants something richer than INI, because typed values and native arrays
are what make a hand-written configuration file pleasant.

**Decision.** All three, at different levels.

```text
built in    INI subset, C89, roughly 300 lines, no allocation beyond the arena
            sections map to key prefixes:  [server] port = 8080  ->  server.port
            available in every profile including freestanding
            ships inside the amalgamation, which is what keeps it dependency-free

vendored    JSON reader, C89, table-driven, no allocation
            not optional in practice: the provenance report and Report Replay both
            consume JSON, so this is a Phase 1 item for the hosted profile

optional    TOML provider, vendored C implementation
            hosted and constrained profiles; richer typing and native arrays
```

YAML is not promised at any level. Its specification is large, its C implementations are
heavyweight, and its type-inference rules are a well-known source of exactly the implicit
behaviour this project exists to prevent.

**Consequence.** The amalgamation stays dependency-free. Fixes what a bare-metal build can
read. Moves a small JSON reader into Phase 1, which the earlier plan had not accounted
for.

**Closed in implementation.** INI and JSON are written here - roughly 370 and 600 lines -
because the available C parsers all build a DOM and allocate, and this design needs an
event producer with exact locations and no allocation. TOML followed the same way, as the
subset D-23 described, in about 900 lines including its writer; it is on by default, being
as dependency-free as the other two. XML followed through expat, read only, behind
`CF_WITH_XML`: the seam took it without a change, as promised, and the mapping below is
the one built. The SQLite store followed through sqlite3 (D-26) as Design.md section 39
describes it: the table `configforge(key, type, value, meta)`, read as a source, and
`cf_store_set`, which writes in one transaction and refuses the whole change for one bad
key. A store is never created by reading it.

---

## D-23 — Format Set and the Parser Seam

**Question.** Which formats are supported, and how are they added without the codebase
growing a parser-shaped tumour?

**The risk.** Format count is where library projects die. Each format is a parser plus a
flattening convention plus optionally a writer plus fuzzing plus fixtures plus documented
edge cases. Five formats is not five times the work of one; it is more, because each brings
its own surprises and they interact with the key space.

**Decision.** Accept the format set, and make the seam narrow enough that the risk does not
materialise.

A parser is an **event producer**, never a source. It emits `begin_table`, `begin_array`,
`scalar`, `end` into a shared flattener that owns all key construction, array counting and
location formatting. A new format is an event producer with no key logic in it.

```text
Phase 1    INI subset (built in), JSON (built in, required by the report)
Phase 5    TOML, XML, SQLite, all behind CF_WITH_* compile-time options
```

A format compiled out fails **loudly** when a source needs it, never silently. The
compiled-in feature set is recorded in the provenance report so a restricted build is
distinguishable from a full one.

**Why SQLite is not just another format.** A store has no comments to preserve and no
formatting to lose, and it has transactions. That makes it the answer to the write problem
this specification otherwise defers: runtime-writable configuration targets a store, while
hand-maintained documents stay read-only.

**Why XML needs more care than the others.** XML has no canonical key mapping. The
convention — attributes and children in one namespace, collisions are errors, repeated
siblings become arrays, mixed content is an error — is declared and tested rather than
inferred, and XML is read-only.

**As built** (`src/serialize/xml.c`). The root element's name is in no key; its
attributes and children are the top level. Repeated siblings must be adjacent to be a
list, since a list scattered among other elements is one nobody reading the file sees as
one. A leaf's text is trimmed, as an INI value is, and every value is text for the
contract to type (D-24). A DOCTYPE is refused outright, which is also how entity
expansion and external references - the ways XML configuration is attacked - never
arise; namespaces are refused because a key has no room for a prefix. Knowing at the
first of two siblings that a second follows takes two passes over the text; the first
records a byte an element in expat's own memory, so the library still allocates nothing
outside its arenas. `test_xml` checks an XML document and the JSON a person would write
for it resolve to the same digest.

**Consequence.** Adds one internal interface and one shared flattener to Phase 1 that would
not otherwise exist. Pays for itself at the second format.

---

## D-24 — Type Inference for Typeless Sources

**Question.** INI, XML, environment variables and command-line arguments carry no types.
What type does a value get?

**Decision.**

```text
with a definition       the definition decides; no inference, ever
without a definition    "true"/"false" exactly -> BOOL
                        sign then digits only  -> INT when it fits in 64 bits
                        C-locale float form    -> REAL
                        everything else        -> TEXT
```

`yes`, `no`, `on`, `off`, `y`, `n` stay text. YAML 1.1 reads `NO` as boolean false, which
turns the country code for Norway into `false`. Leading zeros stay text, so `08` is not an
octal accident and the version string `1.10` does not become the number `1.1`.

The principle: **a value a human would have to stop and think about is left alone.**

**Consequence.** Tier 0 behaviour is predictable without a contract, and Tier 1 removes
inference entirely. Requires the conservative rule to be a documented, fixture-tested
contract rather than a parser detail.

---

## D-25 — Floating-Point Text Conversion Algorithm

**Question.** The library formats and parses `CF_REAL` itself, because `strtod` and
`printf("%f")` follow the active locale. What algorithm produces the digits?

**Why this is open rather than obvious.** Determinism is a stated requirement: the same
double must produce byte-identical text on every platform, or the golden fixtures and the
determinism hash are worthless. That rules out the cheap answer.

**Options.**

1. **Delegate and repair.** Call `sprintf("%.17g")`, then replace the locale's decimal
   separator. Roughly 40 lines. Rejected: `%g` output differs between platforms in exponent
   digit count and in rounding of the last digit, so determinism is lost — which is the one
   property the whole exercise is protecting.
2. **Scaled floating-point digit generation.** Normalise with a power-of-ten table and
   extract digits. Roughly 120 lines, and it accumulates error in the last places, so two
   platforms with different intermediate precision can disagree.
3. **Exact integer digit generation.** Decompose the double into mantissa and binary
   exponent, compute the exact decimal expansion with a small fixed-width big integer, emit
   17 significant digits in a canonical form. Roughly 300 lines, exact, identical
   everywhere, and always round-trips.
4. **Vendored shortest-representation algorithm.** A Ryu or Grisu port. Shortest output,
   exact, and roughly 1,000 lines with large tables, which is heavy for the freestanding
   profile.

**Decision.** Option 3, implemented, and it turned out to subsume option 4.

Exact integer digit generation using the Steele & White algorithm in the Burger & Dybvig
formulation produces not merely 17 correct digits but the *shortest* string that reads back
as the same double, because the algorithm tracks the gap to the neighbouring doubles and
stops as soon as the digits emitted identify the value uniquely. Option 4's benefit comes
free; its thousand lines of tables do not have to be paid for.

This matters more than it sounds. A 17-digit formatter writes `0.1` as
`0.10000000000000001`, which would then appear in configuration files, in `explain()`
output and in every provenance report. Shortest round-trip writes `0.1`.

```text
fixed-capacity big integer      64 limbs, 260 bytes per value, four values live
digit generation                exact, integer arithmetic only
formatting boundary             fixed for decimal exponent in [-4, 16), else scientific
parsing fast path               Clinger: exact when the significand fits in 53 bits
                                and the power of ten is exactly representable
parsing slow path               exact big-integer binary long division, 55 bits,
                                round half to even
NaN and infinity                rejected; a configuration value cannot be either,
                                and no data format this library writes spells them
```

The stack cost is the one real objection, at roughly one kilobyte during conversion. The
answer is `CF_WITH_REAL=OFF`, which compiles out `CF_REAL` entirely for targets that do not
need it.

**Consequence.** Implemented in `src/core/numeric.c`. Verified by round-trip over a value
set that includes the subnormal minimum, `DBL_MAX`, and the powers of ten at both extremes.
Integer conversion is exact and was never affected.

---

## D-26 — External Dependency Policy per Format

**Question.** The project wants few dependencies but real format support. Where is an
external library the right answer, and where is it an abdication?

**The test applied.** A dependency earns its place when the grammar is large enough that
writing it would be a project of its own, *and* an existing C library matches the
architecture. A dependency is wrong when the grammar is small, or when the available
libraries build a DOM while the design needs a streaming event producer.

| Format | Decision | Reasoning |
| ------ | -------- | --------- |
| INI subset | **own**, ~300 lines | trivial grammar; needed in the freestanding profile where no dependency can go; ships inside the amalgamation |
| JSON | **own**, ~700 lines | tiny grammar. Every popular C JSON library (cJSON, parson, jansson, yyjson) builds a DOM and allocates; the design needs a streaming producer with location tracking and no allocation. `jsmn` is streaming but is a tokenizer that does not unescape. None of them fits, and JSON is genuinely easy. |
| TOML | **own, the subset** (built; was deferred) | full TOML 1.0 is 2,500+ lines with dates, multiline and literal strings, inline tables and arrays of tables. `tomlc99` and its relatives are C99 and DOM-based, so vendoring means a C89 fork plus a DOM-to-events shim. A documented *subset* — no datetimes, no multiline — is about 800 lines and covers most hand-written configuration. Both routes stay open because the codec seam does not care. |
| XML | **expat** | the one clear case for an external library. expat is SAX, which is exactly the document event model, so the adapter is thin. It is C89-portable, MIT, and two decades mature. Writing an XML parser would be a mistake: entities, CDATA, namespaces and encoding declarations are where hand-written parsers go wrong. |
| SQLite | **sqlite3** | the dependency is the feature. Public domain, C89, distributed as a single amalgamated file, and the most portable C library in existence. It is also what makes transactional writes possible at all. |

**The rule that keeps this honest.** Every dependency is behind a `CF_WITH_*` option that
defaults to off, none is required by the default build, and the amalgamation contains only
the built-in codecs. A build with no options enabled links nothing but libc.

**Consequence.** The default build has zero external dependencies. XML and SQLite builds
have exactly one each. No dependency is ever silently pulled in.

---

## D-27 — Serialization Layer as a Separate Library

**Question.** The serialization layer — readers, writers, codecs, document events,
flattener — has no dependency on the configuration model. Should it be its own library?

**Trade-off.** It would be a usable standalone C89 component, and extracting it would serve
the project's own stated purpose of removing repeated code across projects. Against that: a
second version number, a second release process, a second set of documentation and a
dependency edge between two young repositories.

**Decision.** Keep it internal, with the boundary drawn as though it were separate.
`src/serialize` may not include anything from `src/core` beyond the portability layer, and
this is checked in CI rather than intended.

Extracting it later is then a directory move and a build file. Extracting it now buys
nothing that is needed today.

**Consequence.** Adds one CI gate. Preserves the option without paying for it.

---



---

## D-18 — Project Name Availability

**Decided.** `ConfigForge` is used. The name is not in conflict with anything the author
has encountered and describes the project accurately.

**Consequence.** Fixes the ABI symbol prefix `cf_`, the macro prefix `CF_`, the include
directory `configforge/`, the CMake package name `ConfigForge` and the namespace alias
`cf::`. All of these are now expensive to change, which is the point of having decided it
before the first public commit.

---

## D-19 — Implementation Language and Standard

**Decision.**

```text
core                 C89 baseline, opportunistic upgrades where detected
public C header      C89, compiles standalone under -std=c89 -pedantic
C++ header           C++03
optional C++ header  C++11 and later, additive
build                CMake 3.10, plus a plain Makefile
```

### Why C Rather Than C++

**Bindings.** A C ABI is the universal FFI target. Python `ctypes` and `cffi`, Rust
`bindgen`, C# P/Invoke, Java JNA, Go cgo, Node N-API and WASM consume it directly, none
needs a shim. A C++ core would require a C export layer to reach any of them, meaning the
C interface gets written anyway and two surfaces get maintained.

**No runtime dependency.** A C library links into a C project, a C++ project, a firmware
image and a WASM module without a C++ runtime, static initialisers or an unwinder.

**Maintainability fit.** The author works in C++03 and in C. A library its author cannot
comfortably maintain is worse than one written in a more ergonomic language.

The argument that previously favoured C++ was composite lifetime management, and D-01
dissolved it. With a flat key space there are no recursive values and no composite
ownership graph, so there is very little that RAII would have been protecting.

### Why C89 Rather Than C99

C89 is the widest target a portable library can have, and D-22's opportunistic strategy
recovers most of what C99 would have given. SQLite is the standing proof that the
combination yields a library that runs everywhere and still has a simple API.

Accepted costs, listed rather than minimised: declarations at the top of each block, no
`//` comments, no designated initializers, no flexible array members, no `snprintf`, no
`stdint.h`, no guaranteed `long long`, no `_Bool`. Two of those are not real costs, since
the bounded formatter has to exist regardless (the C library's numeric formatting is
locale-dependent and therefore unusable here) and `cf_bool` is preferable to `_Bool`
because an ABI type should not change size with the consumer's standard level.

### What C89 Does Not Buy

Toolchains with 16-bit `int`, no 64-bit integer type, or near and far pointer models stay
out of scope. Those are semantic limits, not syntactic ones, and C89 conformance does not
reach them. Recorded so that "maximum reach" does not quietly become an unbounded claim;
the support matrix in the specification is the bounded version of it.

**Consequence.** Fixes the language subset, the portability layer, the support matrix, the
repository layout, the test gates and the shape of every example in the specification.

---

## D-20 — Location of the Error Surface

**Question.** Where are configuration errors reported: at every read, or once at
construction?

**Options.**

1. Every read returns `Result<T, ConfigError>`.
2. Construction is fallible; reads are infallible for keys with a default or definition.
3. Reads throw.

**Trade-off.** Option 1 is the most defensible in isolation and costs three lines per
parameter at the call site, which is the single most common reason typed configuration
libraries are described as heavy. Option 3 is compact and unavailable in the core,
which cannot depend on exceptions. Option 2 requires that validation and conversion
happen eagerly during resolution, which the design already does, and that the snapshot
store converted values rather than raw strings.

**Recommendation.** Option 2, with `tryGet<T>()` and `require<T>()` available for the
cases that genuinely need them. Errors concentrate at one site; the hot path carries no
error checking at all.

**Consequence.** Requires eager conversion and eager validation during resolution, which
makes the snapshot larger and the cold path slower. Both are the correct side of that
trade for this design. Also interacts with D-05: strict validation is what makes
infallible reads truthful.

**Closed in implementation.** The whole error surface is at construction. Once a
`cf_config` exists, reading is infallible for any key with a default or a definition.
This is the largest single simplicity decision in the design: a status on every read
costs three lines of ceremony per parameter, which is why typed configuration libraries
feel heavy.

---

## D-21 — Distribution Form

**Question.** How does a developer obtain the library?

**Recommendation.** CMake package as the primary form, plus a CI-generated amalgamation
(`configforge.hpp` and `configforge.cpp`) for the hosted default configuration. System
packaging only after the ABI is frozen.

The amalgamation is a simplicity feature, not a convenience. Someone evaluating the
library should be able to try it by copying two files and should only meet the build
system after deciding to keep it. It is generated and tested from the real sources in
CI, never maintained by hand.

**Consequence.** Requires that the hosted default configuration have no mandatory
external dependency, which constrains D-17: the default file parser must either be
vendorable or the amalgamation ships with the built-in INI subset only.

**Closed in implementation.** Three forms. CMake with an installed package, checked by
`gate.install` from a project that has never heard of this source tree. An amalgamation -
two files, any compiler, no build system - generated from the real sources and compiled
and run by `gate.amalgamation`. And the binary image for a target that cannot compile
anything at all. The hand-written `Makefile` was deleted (D-31) because the amalgamation
is a better answer to the same question and is tested rather than asserted.

---

## D-22 — Opportunistic Feature Substitution Policy

**Question.** The baseline is C89 and better toolchains are common. When may the library
use what the toolchain offers?

**The risk.** Two code paths that are never compared become two behaviours. Determinism is
a stated requirement: if the C89 path and the C99 path format a number differently, golden
`explain()` fixtures fail on one platform and the determinism hash differs between two
builds of the same library. That is worse than never having used the standard function.

**Decision.**

> Opportunistic substitution is permitted for **capability and performance**.
> It is forbidden for **anything whose output is part of a contract**.

Permitted: fixed-width types, 64-bit integer detection, `inline`, compile-time assertions,
flexible array members, atomic reference counting, `_Generic` accessors, compiler
attributes, `snprintf` for `%s` and `%d` message assembly, `qsort` with a byte-wise
comparator.

Forbidden on every toolchain at every standard level: numeric parsing and formatting
(`strtod` and `printf("%f")` follow the active locale's decimal separator), case folding in
key normalisation (`tolower` and `toupper` are locale-dependent; in a Turkish locale
`toupper('i')` is `'İ'`), key ordering (`strcoll` is locale-dependent), and character
classification (`isspace` and `isdigit` are locale-dependent above ASCII).

The rule generalises cleanly: **every locale-sensitive C library function is forbidden.**
This is not caution about old compilers; it is the same bug class on a current one.

**Enforcement.** All detection lives in `cf_port.h` and `port.c`; no other file may test
`__STDC_VERSION__`, `_MSC_VER` or `__GNUC__`, checked by grep in CI. `CF_FORCE_PORTABLE=1`
disables every upgrade, and CI runs the full suite in both modes on the same toolchain,
asserting byte-identical golden output and an identical determinism hash.

**Consequence.** Adds roughly 600 lines of portability code to M0 and a second CI
configuration. Without the forced-portable gate, the fallback paths would be exercised only
on toolchains nobody runs daily and would rot silently.

---

## D-28 — Binary Serialization

**Question.** Should there be a binary format, and if so which one?

**What it would have to buy.** Not size and not speed of parsing: a configuration file
is kilobytes and is read once. The only thing a binary form can do that no text format
can is **load without parsing and without allocating**, which is what a target with no
filesystem, no heap and a boot-time budget actually needs. That is also where the
freestanding profile has had a hole all along: build-time configuration was described
as a source and had no artifact.

**Options.**

1. **TLV container.** Self-describing, streamable, familiar. For a flat key space it is
   a poor fit: the tag would have to be the key string, at which point it is a
   length-prefixed key-value list wearing a TLV costume. The idea survives where it
   actually belongs — as tag-plus-payload inside each value — without the container.
2. **CBOR or MessagePack.** Standard, with tooling. But both give a *document*, which
   JSON already gives; the gain is bytes, which is not the problem. A thousand lines for
   a third document codec whose advantage is size.
3. **An image of the snapshot.** The key space is already a sorted array plus a string
   arena, so the natural binary form is a direct image of that: a header, a sorted key
   index, a value array, one string blob. Loading is validate-and-point. No parse, no
   allocation, O(log n) lookup straight out of the bytes.

**Decision.** Option 3, as a format of its own rather than a codec, because it is not a
document and pretending otherwise would mean inventing a document shape that happens to
match the layout.

```text
magic          "CFGB", major, minor            refuse on a major mismatch
byte order     little endian, always           see below
layout         header | key index | values | strings
strings        NUL-terminated and length-carrying, as in memory
integrity      the determinism digest, verified on load
provenance     optional: the whole candidate chain, or only the winners
```

**Endianness is fixed rather than recorded.** An image that records its producer's byte
order and refuses the other one is an artifact that only loads on the machine that wrote
it, which defeats the purpose of having an artifact. Little endian always; a big-endian
host pays one conversion pass on load and gets identical semantics. The determinism
digest would be meaningless otherwise.

**Alignment is checked rather than assumed.** The loader verifies the base pointer and
the declared offsets; an unaligned buffer is copied rather than dereferenced, because
the alternative is undefined behaviour on every target that cares and silence on the
ones that do not.

**Provenance is optional and it is the interesting half.** A resolved-only image is what
a boot path wants. An image *with* the chain is what lets a device with no filesystem
still answer "why is this value what it is", which is the entire premise of the library
arriving somewhere it otherwise could not.

**Consequence.** Adds a format that is not a codec, so the file source needs a route
that bypasses the event model. Adds a build-time story the freestanding profile was
missing. Does not add CBOR: the codec seam accepts one later if a real need appears
rather than a suspected one.

---

## D-29 — Command Line Portability

**Question.** What does "parses correctly everywhere" mean for a command line?

**The real portability problem is not syntax.** On Windows `main`'s `argv` arrives in the
active ANSI code page, not UTF-8, so a configuration value containing any non-ASCII
character is already wrong before any parser sees it. Every syntax question below is
smaller than that one.

**Decisions.**

```text
--key=value        value                       everywhere
--key              true, when the key is a bool or has no definition
--key value        value, when a definition says the key takes one
--no-key           false, when the key is a bool
-k value           alias, when a definition declares it
@file              arguments read from a file, one per line
--                 stop
```

`--key value` was deliberately absent at Tier 0 and stays absent there: it cannot be
parsed correctly without knowing which options take a value, and a parser that guesses
eats positional arguments. A definition set answers exactly that question, so Tier 1
gains the form people expect. This is the same shape as D-06: a contract buys
capability, and its absence buys predictability rather than guesswork.

**`/key:value` is rejected.** It is the Windows convention and it collides with paths:
`/etc/app.conf` is a plausible positional argument and an implausible option. Supporting
both spellings would mean deciding which one `/tmp` is, and there is no good answer.

**`@file` is accepted** because the Windows command-line length limit is real and the
convention for working around it is universal. One argument per line, `#` comments,
recursion refused.

**UTF-8 is the library's problem, not the caller's.** `cf_cli_args()` returns UTF-8
arguments: on Windows it converts from `GetCommandLineW`, everywhere else it returns
`argv` unchanged. A caller that passes `argv` directly still works and still gets ASCII
right; the helper is what makes the non-ASCII case right without every program having to
know why.

**Consequence.** The command-line lexer gains a definition-aware mode, which means the
source has to see the definition set — the same wiring the environment source already
needed for D-06.

---

## D-30 — Which Sources Exist

**Question.** The design lists a dozen sources. Which are real, which are deferred, and
on what basis?

**The basis.** A source is built when it needs nothing this project does not already
have. A source is deferred when it needs a platform to test on or a dependency to link,
because an untested source is a claim rather than a feature.

| Source | State | Why |
| ------ | ----- | --- |
| defaults, memory | built | no bytes to read |
| environment, command line | built | flat lexers |
| file: INI, JSON | built | built-in codecs |
| provenance report replay | built | JSON plus the report schema |
| `.env` file | built | a flat lexer over a file; the seam already existed |
| directory of drop-in files | built | one rank, byte order, later wins |
| binary image | built | D-28 |
| TOML | built | the D-23 subset, own parser, on by default |
| XML, SQLite | built | D-23; a linked dependency each, off by default |
| directory of secrets, `.properties` | built | a file a key; a flat format |
| Windows Registry | built | a platform capability, tested on Windows (below) |
| gsettings, macOS defaults | deferred | needs the platform to test |
| remote key-value stores | deferred | a non-goal until something asks |

**The Registry is a platform capability, not an option.** It needs no dependency -
the API is in every Windows system - so there is no `CF_WITH_REGISTRY` to turn on, which
the specification had planned. The walk is in `src/platform/win32`, the only code that
knows Windows; everywhere else `cf_platform_registry_walk` fails, and the source with it,
*optional or not*, because a Registry source that read nothing on Linux would look
exactly like an empty key. REG_SZ is text the contract types rather than typed text,
because installers write numbers as strings; REG_EXPAND_SZ is not expanded, because
expanding it would be a second environment source with no name in `explain()`. Its test
writes its own keys with `reg.exe` and removes them, so it needs no platform macro.

**The drop-in directory is worth a note.** `conf.d` is how systemd, sudo, apt and a dozen
others let a package add configuration without editing a file it does not own. Each file
is a sub-source at its own rank in lexical order, so `10-base.ini` losing to `90-site.ini`
is precedence rather than a merge rule, and `explain()` names the file.

---

## D-31 — The No-Build-System Path

**Question.** The hand-written `Makefile` exists so that a toolchain without CMake can
still build the library. Is it still the right answer?

**What it cost.** Eight updates in step with `CMakeLists.txt`, each one a chance to
forget. It did catch one real defect — a feature macro visible in one translation unit
and not another — but the CMake build would have caught the same thing once the option
was declared properly.

**What replaced it.** The amalgamation: two files, no build system, any compiler,
`cc -std=c89 configforge.c app.c`. It is generated from the real sources, and the
amalgamation gate compiles and runs it on every commit, so the fallback is *tested*
rather than merely present. That is more than the Makefile ever was.

**Decision.** Delete the `Makefile`. The no-build-system path is the amalgamation, and it
is a better one: it needs no `make`, no shell, and no second list of source files to keep
in step.

**Consequence.** One fewer place to forget a file. The claim "works without CMake" is now
checked by a test instead of asserted by a file nobody runs.

---

## D-32 — The Compiler Warning Standard

**Question.** The warning set should be as close to common across compilers as it can be,
with per-compiler additions where it cannot. How is that kept true?

**What went wrong first.** The flags were written as one list under a branch testing for
`GNU` *or* `Clang`, so GCC-only flags were handed to clang as well. That would have
broken the first real clang build; clang-tidy's front end found it before anyone tried
one. The obvious repair — a hand-maintained table of which flag belongs to which
compiler — replaces one guess with a larger one, because the table is wrong the moment a
compiler adds or drops a flag and nothing says so.

**The measurement problem.** Writing that table honestly needs a way to ask a compiler
whether it knows a flag. Attempting it here failed: without clang installed, probing
through clang-tidy's front end reported every flag as accepted, including ones clang
demonstrably rejects. A standard built on a measurement that cannot be trusted is worse
than no standard.

**Decision.** Do not keep a table. Ask the compiler.

```text
one list        grouped by what each flag protects, not by which compiler has it
per compiler    a short list of additions that only one compiler offers
verification    check_c_compiler_flag, at configure time, with -Werror set so that
                a compiler which merely warns about an unknown -W flag still fails
                the check
unknown flag    skipped, and named in the configure output
```

The `-Werror` part matters: clang warns rather than errors on an unknown `-W` flag, so a
naive check passes for flags it does not support. Without it the mechanism would report
success exactly where it is needed.

**What this buys beyond correctness.** The measurement happens on the machine doing the
build, with the compiler doing the build. A flag a future compiler drops stops being
applied instead of breaking the build, and a flag a future compiler adds can be listed
before every supported compiler has it. The build prints the set it settled on, so the
answer to "what is this enforcing" is in the configure log rather than in a file someone
has to read.

**MSVC is weaker and says so.** `/W4 /WX` is not equivalent to the GCC and Clang set,
so the classes that do have an MSVC counterpart are enabled by number (`/w14242`,
`/w14311`, `/w14640` and the rest) and `/permissive-` stands in for `-pedantic`. It is
still not parity: the declaration class has no counterpart, and MSVC has no C89 mode at
all — `/std:c11` is the oldest it offers, so CMake's `C_STANDARD 90` is a silent no-op
there and the C89 conformance claim rests on the GCC and Clang jobs.

Naming the gap is the point. A project that claims one standard and enforces two has the
weaker one, and a silent no-op is how a standard stops being enforced without anyone
deciding to stop enforcing it.

| What it protects | GCC and Clang | MSVC |
| ---------------- | ------------- | ---- |
| Conformance | `-std=c89 -pedantic -Werror -Wall -Wextra` | `/W4 /WX` |
| Conversions | `-Wconversion -Wsign-conversion -Wdouble-promotion -Wcast-qual -Wcast-align -Wbad-function-cast` | partial, `/W4` |
| Shadowing and aliasing | `-Wshadow -Wwrite-strings -Wpointer-arith` | partial |
| Declarations | `-Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wold-style-definition -Wnested-externs -Wredundant-decls -Wundef` | none |
| Logic mistakes | `-Wnull-dereference -Wswitch-enum -Wvla -Wformat=2` | partial |
| Resources | `-Wstack-usage=` (GCC) | none |
| Compiler-specific | GCC: `-Wlogical-op -Wduplicated-cond -Wduplicated-branches -Wshift-overflow=2`; Clang: `-Wcomma -Wassign-enum -Wconditional-uninitialized -Wimplicit-fallthrough` | — |

**Consequence.** One place describes the standard, and the build discovers rather than
asserts which compiler implements which part of it. Adding a flag means adding it to the
list it belongs to; nothing has to be researched about who supports it.

---

## D-33 — Per-Parameter Policy

**Question.** The specification writes a per-parameter policy as
`database.password: Environment > Local`. What carries that, and what happens when a
source outside the list supplies the key anyway?

**Shape.** One field on the definition: the source names permitted to supply this key,
in decreasing precedence. One list does both jobs. A source not named may not set the
key, and the named ones are tried in that order rather than the global one — which is
how an operator-controlled setting survives a command line that tries to override it.
A separate "allowed set" and "ordering" would have been two fields that can disagree.

**A violation rejects the candidate; it does not fail construction.** Two arguments, and
the second is the one that settles it.

Refusing to start would let any user stop the program by passing a locked option, which
turns a policy into a denial of service — the opposite of what an operator lock is for.

And for the case the feature exists for, refusing is theatre. A password on the command
line is in `ps` the moment it is typed, whether or not this library goes on to use it.
The damage is done; the useful response is to say so loudly — in `explain()`, in the
provenance report, and at error severity in Doctor — rather than to refuse to run and
imply the secret is safe.

So a forbidden candidate becomes `rejected` with the policy in its reason, and stays in
the chain. `explain()` shows the value that was offered, where it came from, and why it
was not used. That is the whole premise of the library applied to its own security
feature.

**A policy naming no source this build has is an error.** The key could never be set by
anything, which is a typo rather than an intention. Checked only when *none* of the
named sources exists: naming a source that is present on some deployments and absent on
others is legitimate, and failing on that would make the feature unusable for the case
it is for.

**The core asks rather than taking the model.** The apply pass needs one thing from
outside itself — what a rank is called — and it takes a `cf_rank_names` callback instead
of a `cf_source_view`. The layering gate is what surfaced this: the first version
included `cf_source.h` from the core and the gate refused it. Same shape as the command
line's spec, and for the same reason: a test can answer the question with no sources
existing.

**A policy passes over the program's default.** The first version applied the list to
every candidate, the definition's default among them - and "default" is in nobody's
list, so a key locked to the environment, with nothing in the environment, had no value
at all rather than its default. A Windows example found it: `updatechannel` locked to
Group Policy, no policy set, and the program read the fallback it passed to the getter,
not the default it declared. A policy says who may *set* a key, and nobody sets a
default; it is what the key is when no permitted source says otherwise. So a default
candidate (`CF_CAND_DEFAULT`) is neither refused nor preferred: it wins only when no
permitted source has a value. A derived configuration keeps it a default, because its
inherited candidates now carry what they were (`cf_ks_add_candidate`).

**Consequence.** `cf_source_view` moved from `cf_explain.h` to `cf_source.h`, where a
view over sources belongs; having it in the diagnostics header meant nothing below could
name a source without including the top layer.

---

## D-34 — Setting a Value

**Question.** A snapshot is immutable. What does changing a setting at run time mean?

**Decision.** It derives another one. `cf_derive` returns a new configuration with the
change applied; the original stays valid and unchanged.

That is not a workaround for immutability, it is what immutability is for. A reader
holding the old snapshot keeps seeing a configuration that does not move underneath it,
and a thread reading while another sets needs no lock, because nothing is being written.
Mutating in place would have required exactly the lock the threading contract (D-14)
avoids, on the read path.

**The derived configuration is independent rather than layered.** It copies what it
needs, so neither has to outlive the other. Layering would have been cheaper — hold a
reference and fall through — and would have left the caller a lifetime to reason about
and every accessor a second path to walk. A pass over the key space is microseconds for
a configuration, and this library prefers the version with nothing to explain.

**Provenance survives, and grows.** The change arrives from a source of its own, ranked
above the command line because the user just did it deliberately, and the displaced
value stays in the chain. After a change, `explain()` says what it is now, what it was,
and where *that* came from — which is exactly the question somebody asks after changing
a setting. Replaying only the winners would have been simpler and would have made the
derived configuration explain itself *less* than the one it came from, which is the
wrong direction for this library to move in.

**The contract travels.** Definitions, open prefixes and the unknown-key policy are all
inherited: a derived configuration that accepted what the original refused would be a
different contract wearing the same name.

**A fixed arena has to supply its own.** A configuration built with `cf_build_in` has no
allocator to borrow, and falling back to `malloc` is the one thing a target with no heap
cannot survive. `cf_derive` refuses and names `cf_derive_in`.

**One thing the implementation got wrong first,** worth recording because the reasoning
sounded right: the override source was added before the replayed base, "so it takes the
rank above everything". Rank is explicit on the source, so the position looked
irrelevant — and was not. Deriving twice puts two candidates at the same override rank,
where the later one wins, and adding the new one first made the *inherited* override the
later of the two. A second change to the same key did nothing.

---

## D-35 — Change Notification

**Question.** A file this configuration was built from changes on disk. What does the
library do about it?

**The question that matters is not which API.** inotify, kqueue and
ReadDirectoryChangesW are three implementations of a detail. What decides the design is
what the library hands back, because that choice is what a caller cannot undo.

**Options.**

1. **A callback.** The library owns a thread and calls you. This forces a threading
   model on every caller, and calling back from the library's thread while the caller
   reads is exactly the race the threading contract (D-14) exists to avoid. Rejected.
2. **A pollable question.** The caller asks "has anything changed", and decides for
   itself when to act. No thread, no callback, nothing imposed.
3. **A handle to wait on**, for a program that already has an event loop.

**Decision: option 2, with option 3 reserved.** A library that forces a thread on its
caller has made a decision that belongs to the caller.

**Option 3, built.** `cf_watcher` hands over an inotify descriptor on Linux and an event
HANDLE on Windows, for a loop to wait on beside its own; `cf_watcher_wait` does the same
for a program with no loop. It is still not a callback and it owns no thread. Three
things decided its shape:

* **It watches directories, not files.** Editors save by writing a new file and renaming
  it over the old one, and a watch on the old file follows it into the bin. So a wake-up
  means "ask now", never "it changed" — a neighbour wakes it too — and `cf_changed`
  stays the answer. Drain, then ask; the other order can consume a wake-up for a change
  that arrived after the question.
* **One handle, not one per directory.** On Windows every directory's overlapped read
  completes into one manual-reset event. `FindFirstChangeNotification` was simpler and
  would have handed every caller a set of handles to wait on.
* **A directory that does not exist yet is watched through its nearest ancestor**, and
  draining moves the watch down as the levels appear. Otherwise an optional file under
  `~/.config/app/` on a machine that has never run the program could not wake anything
  when it finally arrived.

It lives beside the configuration rather than inside it: an immutable, shareable
snapshot holding a kernel object that reading mutates would have broken D-14. The
watcher copies the paths it needs, so either may be released first.

**kqueue, for macOS and the BSDs,** works differently in the one way that matters: it
watches what it has open, not a name. A directory's events say an entry was added,
removed or renamed and never that a file in it was rewritten in place, so each file is
watched as well as its directory; and a descriptor follows the file it was opened on, so
after an editor renames a new file over the old one it is watching the old one in the
bin. Every drain therefore closes everything and opens it again by name. A change in that
gap is not lost, for the same reason as everywhere else: drain, then ask.

It was written on a machine with no BSD and is tested two ways. `CF_WAKE_KQUEUE` builds
it on Linux against libkqueue, a kqueue emulation, and a CI job runs the unit tests that
way on every commit; the macOS job runs it against the real thing. The emulation found a
real bug — a descriptor's events must be deleted before the descriptor is closed, or the
next one to reuse the number inherits the old watch — and has two gaps of its own, each
written down where it bites: it does not report a directory created inside a watched
one, and its descriptor cannot be waited on with `poll`, which is why
`cf_watcher_wait` asks `kevent` instead.

**What building it found.** `cf_changed` stamped a drop-in directory by the directory's
own modification time, which moves when a file is added or removed and not when one is
edited in place — the commonest change such a directory gets. A directory is now stamped
by what it holds.

**And reload is a rebuild, not a mutation.** The caller runs its builder again and swaps
the result. This is where a conflict the design anticipated simply evaporated:

> *"With watch enabled, the pointer is valid until the next reload... This is the one
> genuine conflict between Tier 0 simplicity and Tier 4 capability."*

It is not a conflict any more, and nothing was done about it on purpose. Snapshots
became immutable and reference counted for other reasons — D-14 and D-34 — and a reload
that produces a *new* snapshot leaves the old one valid for as long as somebody holds
it. `cf_get_text` returns a pointer valid until *that* configuration is released, with
or without watching. Enabling watch changes no contract at all.

**What is actually detected, stated plainly.** `cf_changed` compares modification time
and size. That is cheap, portable and pollable, and it can miss a file rewritten inside
one clock tick with the same length. The exact answer is to rebuild and compare
`cf_digest`, which costs a re-read — most of the price of the reload you were about to
do anyway.

So `cf_changed` is documented as a *hint that is worth acting on*, not a guarantee.
Describing it as exact would be the more attractive sentence and the one that produces a
bug report about a change nobody saw.

---

## D-36 — Licence

**Question.** Under what terms may anyone use, ship and change this library?

**What decides it.** Who the users are. The design targets everything from a desktop
tool to a device with no filesystem and no heap (D-07, the freestanding profile), and
the distribution form most likely to travel is the single-file amalgamation, copied into
somebody else's tree (D-21, D-31). A licence that is fine for the first and a burden for
the second is the wrong one.

**Options.**

1. **MIT.** The most recognised permissive licence. Requires the notice in "all copies
   or substantial portions", which for a binary inside firmware is read by many legal
   departments as a notice in the product - a real obligation for a device maker.
2. **BSD-2-Clause.** The same shape, the binary-notice clause stated outright.
3. **Apache-2.0.** An explicit patent grant, which is valuable; incompatible with
   GPLv2-only projects, which excludes part of the C world this library is for, and
   heavier than a C library of this size needs to carry.
4. **MPL-2.0.** File-level copyleft. Changes to the library's files must be shared; asks
   more of an embedder than most will accept for a configuration library.
5. **zlib.** Permissive; no notice required in a binary distribution, the notice kept in
   every source distribution; altered versions marked as such. The licence of zlib,
   libpng and SDL - libraries that live inside other people's products.
6. **Boost (BSL-1.0).** Like zlib on binaries; less familiar outside C++.

**Decision: zlib.** It is the one whose obligations line up with how this library is
distributed. A firmware image built with it owes nobody a notice, so a device maker
carries no paperwork; anybody receiving the *source* - including the amalgamation on its
own - receives the licence, which is why the amalgamation generator puts it at the top of
both files and the install puts it beside the headers. It is GPL-compatible, so no C
project is excluded.

**What it does not give.** An explicit patent grant, which Apache-2.0 would. For a
configuration library with no novel algorithms that is judged the smaller risk than
excluding GPLv2-only users and burdening embedded ones.

**Third-party code** carries its own terms, and none of it is required: the default build
has no dependencies at all. `docs/Dependencies.md` lists every optional and build-time
one with its licence, so the obligations of a given build can be read off one page.

---

## D-37 — Limits and the Stack Budget

**Question.** How large may the library's fixed buffers be - the longest key, error
message, path, nesting - and who decides how much stack a function may take?

**What decides it.** Every limit is a buffer, and every buffer is stack. A 256-byte key
is 256 bytes in every frame that holds one; doubling it doubles those frames. The design
spans a desktop tool, which wants long keys and does not care about two kilobytes, and a
device whose whole thread has two kilobytes, which wants short keys and cares about
nothing else. Any fixed choice is wrong for one of them, and the library cannot know
which one it is being built for. The builder can.

**Options.**

1. **Fixed limits and a fixed budget**, as until now (256-byte keys, 2048 bytes a frame).
   Simple, and checked; but a builder who needs 1024-byte keys has to edit headers, and
   one who has one kilobyte of stack has no way to get there at all.
2. **Configurable limits, no budget.** The builder chooses; nothing tells them what the
   choice cost until a thread overflows on a device.
3. **Configurable limits and budget, the cost measured and reported.** The builder sets
   both; the build measures every frame with the compiler's own figures and checks them
   against the budget; when one does not fit it says which, by how much, and which
   option would remove it.

**Decision: option 3.** The trade is the builder's - the library's job is to make the
price legible, not to pick it. `CF_KEY_MAX`, `CF_MESSAGE_MAX`, `CF_PATH_MAX` and
`CF_MAX_DEPTH` are CMake cache variables, written into the generated
`cf_build_options.h` because they change the public structs (a `cf_error` holds a key
and a message), so a consumer cannot disagree with the library about them - the same
rule as `CF_WITH_REAL`. `CF_STACK_BUDGET` is bytes per function, default 2048, 0 to
measure without checking.

**How it is checked.** With `-fstack-usage`, which writes each function's frame to a
file, and `scripts/stack.cmake`, run after the library links. Not `-Wstack-usage`, which
stops at the first function over and says nothing about what to do; the script reports
all of them, attributes each to the `#if CF_WITH_*` it is compiled under, and lists the
largest frame each feature brings - so "what fits in a kilobyte" has an answer made of
options. Run by hand with another `BUDGET`, it answers "what if" without a rebuild.
Over budget, it removes the library it just linked, so the next build checks again
rather than reporting a success.

**What it does not cover.** MSVC writes no frame sizes, so there the budget is unchecked
and the build says so. A frame is one link of a call chain; the decoders recurse once per
level of nesting, so a thread needs room for `CF_MAX_DEPTH` of those as well, and that is
the nesting limit's to bound, not the frame budget's. The project's own tests, Doctor and
examples stay at the 2048 bytes they were written for, at the default limits; with other
limits their frames grow too, and they are not what ships.

**One limit for nesting.** JSON allowed 64 levels while the flattener held 32, so a
document between the two was accepted by one layer and refused by the next with a
less useful message. There is now one `CF_MAX_DEPTH` for every decoder and both
directions.

---

## D-38 — Leaving Parts Out

**Question.** Which parts of the library can a build leave out, and what happens to a
program that uses one that is missing?

**What decides it.** A configuration library is linked into everything from a server to
a sensor, and the sensor does not want an emitter, a report writer or a `.env` reader it
will never call. But the library's premise is that a value can always say where it came
from, and a build option that makes a source silently read nothing breaks that premise
in the worst possible way: an empty drop-in directory and an unreadable one look the
same.

**Options.**

1. **Formats only**, as until now. Everything else always built.
2. **Compile the part out entirely**, functions and all. Smallest; and a program that
   names one fails to link, so turning an option off is a breaking change for every
   program that mentions the function, even on a path it never takes.
3. **Compile the work out, keep the functions**, each failing by name.

**Decision: option 3.** `CF_WITH_DIR`, `CF_WITH_DOTENV`, `CF_WITH_EMIT`, `CF_WITH_IMAGE`
and `CF_WITH_REPORT`, all on by default. Off, a source still exists and its load fails
with "this build was compiled without CF_WITH_DIR" when the configuration is built - the
rule for a disabled format, applied to everything; a writer returns nothing and says why
where it can. The cost of a kept function is a few dozen bytes; the cost of a link error
is every program that has the call on any path. `cf_has_feature` lists what a build has,
so a provenance report from a reduced build is distinguishable from a full one.

**What it does not do.** Remove code a static link would remove anyway: `--gc-sections`
already drops a function nobody calls. The options matter for a shared library, the
amalgamation, and code reached through a table, as every codec's encoder is.

`tests/unit/test_features.c` checks each part in both directions - working when it is
on, refusing by name when it is off - and the `minimal` preset turns them all off, so
both directions run on every verify.

---

## D-39 — YAML

**Question.** What YAML does the library read, and how are its values typed?

**What decides it.** YAML is the format most configuration is written in and the one
whose reading most often surprises its writer. Under YAML 1.1 `country: no` is false and
`version: 3.10` is 3.1; under 1.2 neither is, and which one a reader follows is decided by
the library somebody picked, not by the file. Anchors and aliases let one value appear
under two keys, which is a value with two origins. Tags let the file override the type
the program declared.

**Options.**

1. **Full YAML, typed by the reader's schema.** The surprises above, inherited from
   whichever schema libyaml's caller implements.
2. **A subset, typed by the file** (as JSON and TOML are). Settles 1.1 against 1.2 by
   choosing one, which the writer of the file still has to know.
3. **A subset, typed by the contract.** A plain scalar is text until a definition types
   it, as an INI value is; a quoted one is text and says so.

**Decision: option 3**, read through libyaml (`CF_WITH_YAML`, off by default: it is the
one codec with a dependency) and written by hand. The library already has one spelling of
a boolean (D-24, `true` and `false` only), so `no` is the text "no" under either YAML
version and under a definition too. Null - `~`, `null`, or nothing - is the one plain
scalar typed by the file, because it is a value here as JSON's null is. Anchors, aliases,
tags, a second document, a key that is not a scalar and a top level that is not a mapping
are refused by name with what to write instead. Written YAML quotes every string, so it
reads back the same under 1.1 and 1.2, and quotes any key a reader could take for a
value.

**What it costs.** Without a contract, Tier 0 inference applies, and a plain `3.10` is the
number it looks like - quote what is meant as text. That is INI's behaviour, stated rather
than inherited.

---

## D-40 — Where the version is written, and the commit it was built from

**Question.** Where does the version live, and how does a build say which commit it is?

**What decides it.** The version was in four places - `cf_version.h`, `project()`, the
Conan recipe, the vcpkg manifest - and a gate compared two of them. Every release would
have been four edits, and the two unchecked copies are the ones a user installs. Two
builds of one version are not the same program either: the commit is what tells them
apart, and a provenance tool that cannot say where it came from is a poor advertisement.

**Options.**

1. **A `VERSION` file**, read by CMake and the recipe. Builds without CMake - the
   amalgamation, `cc` on the sources - need the version in C, so the header would be
   generated or copied, which is a second place again.
2. **The git tag as the source** (`git describe`). A tarball, a vendored copy and the
   staged tree verify builds have no tags; the version would depend on how the source
   arrived.
3. **The header as the source**, everything that can read it reading it, the one copy
   that cannot (`vcpkg.json`) written by a script and checked by a gate.

**Decision: option 3.** `CF_VERSION_MAJOR`, `_MINOR`, `_PATCH` in `cf_version.h` are
the only place the number is written by hand. CMake reads them before `project()`, the
Conan recipe in `set_version()`. `cmake -DTO=x.y.z -P scripts/version.cmake` rewrites
the header and the vcpkg manifest and opens a CHANGELOG section; `gate.version` fails
when any copy disagrees. A tag is `vX.Y.Z` with nothing else in it, and the release
workflow refuses one that is not the header's version.

The commit is build metadata, not version: `cf_build_id()` returns git's abbreviated
hash, with `-dirty` for uncommitted changes, written by the build on every build so it
follows a checkout without a reconfigure. A `git archive` carries it through
`export-subst`; a copy that is neither reports nothing rather than a guess. It is kept
out of `cf_version()`, so that two builds of a version still compare equal.

**What it costs.** The header's three numbers are parsed by regular expression in two
places (CMake, the recipe); the gate is what notices if its form changes. The provenance
report does not yet record the build id - adding it changes the report schema, and is
listed in the Roadmap.

---

## D-41 — Doctor's rules as a library

**Question.** Doctor was a program. A program cannot ask it anything without starting a
process and writing a report to disk. Should its rules be callable?

**What decides it.** The useful moment to find a production password on the command line
is before the service serves, not in a CI job afterwards; and a test that asserts "no
findings" should not need a file. Against that, Design.md and Doctor.md both draw a line:
Doctor reads reports and never resolves anything, and the configuration library should
not carry the rules that audit it.

**Options.**

1. **Rules in the configuration library.** One library, and every device that reads
   configuration carries an auditor it never runs.
2. **A library of its own**, with the program on top of it.
3. **Keep the program**, and let callers run it as a process.

**Decision: option 2.** `libconfigforge_doctor` (`cf_doctor.h`) holds the rules, the
baseline and the severities; `doctor` is it plus argument parsing and printing. A
configuration is linted by writing its full report in memory and reading that, so the
library still never resolves anything and sees exactly what `doctor lint` sees. What the
program kept in globals is a context, so two runs can happen at once; nothing in it
prints. Static, so the configuration library's is the one ABI to keep. The amalgamation
leaves it out by name.

**What it costs.** A context holds its baseline inline - 512 entries, about 140 KB - which
a program linting itself pays once, on the heap.

---

## D-42 — Authorization for persistent writes

**Question.** `cf_store_set` writes whatever it is given to whatever path it is given. A
running program that keeps a change - an admin page raising a log level - needs more:
which sources may be written, by whom, and which values. The first specification put it
as *policy check, capability check, writer*, and kept capability ("what can this source
do") apart from policy ("what may ConfigForge do with it"). How much of that belongs in
the library, and in what shape?

**Options.**

1. **Leave it to the caller.** `cf_store_set` is the API; the program checks what it
   likes. Every program then re-implements the contract, or does not, and a store fills
   with values that are rejected on every start.
2. **A policy layer of its own** - per-source and per-key write permissions, in the
   program or in a policy file, compiled and checked for conflicts. The specification's
   full model, and a second rule set beside the read side's, which can disagree with it.
3. **Three questions, each answered by something that already exists or is one call**:
   capability on the source, permission granted by the program, authorization by the
   read side's contract.

**Decision: option 3.** `cf_write(cfg, target, items, count, meta, &err)` asks, in order:

* **capability** - `write` on the source's vtable, NULL for every source but a store. A
  hand-written document has comments a writer would lose (Design.md, section 39); the
  environment and the command line belong to whoever started the process.
* **permission** - `cf_builder_writable(b, name)`. Every source is read only until the
  program grants it. A grant to a source that cannot be written, or to no source, fails
  `cf_builder_create`: the specification's "capability validation before the policy
  becomes effective", and a typo found at start rather than at the first write.
* **authorization** - the items become candidates from the target, at its rank, and the
  contract is applied to them by `cf_defs_apply`, the function that applies it when the
  store is read back. A value rejected there is refused here; a key whose per-parameter
  policy (D-33) leaves the store out is refused as `CF_E_POLICY`, because an operator's
  lock a program can write past is no lock. Required keys are not asked for - a write is
  a few keys, not a configuration - and a command (`CF_TRANSIENT`) is never written.

Any "no" refuses every item before one is written, and the error names which question
said it. The configuration does not change; it is a snapshot.

**Rejected: write permission per key** (option 2's core). It would be a list beside the
policy list, and the two would disagree; D-33's list already says which sources may
supply a key, and a source that may not supply it has no business storing it. If a key
needs to be readable from a store but never written by the program, that is a second
store, not granted.

**Kept: `cf_store_set`** as the unauthorised writer, for provisioning a store before any
configuration exists - an installer, a migration. It is documented as asking nothing.

**What it costs.** One member on `cf_source_vtable`, so a custom source written before
this does not compile until it says NULL - which `-Wmissing-field-initializers` was there
to make a decision rather than a default. Before 1.0, that is the time to add it.

---

## D-43 — A renamed key

**Question.** `cf_def` has had a `replaced_by` field since the definition model was
written, and nothing read it: a key renamed in version 2 left every file written for
version 1 configuring nothing. What should the old spelling do?

**Options.**

1. **Nothing**, the deprecation a warning and the rename the user's problem. Every file
   in the field breaks on upgrade, which is exactly what a deprecation exists to avoid.
2. **An alias**, the old name read as if it were the new one. Simple, and it erases the
   fact that the file says the old name - the one thing a person migrating needs to see.
3. **A copy at the same rank, marked.** Each value written under the old name is offered
   to the new key as well, `CF_CAND_RENAMED`, its place `app.ini:2:1 (as db.host)`.

**Decision: option 3.** Three rules make it predictable:

* **Precedence across sources is unchanged.** `--db.host` on the command line still beats
  `database.host` in a file: the person typing a flag meant it, whatever they called it.
* **Within one source the new spelling wins**, wherever each is written, by the sort
  order rather than by position: a half-updated file means its new line.
* **The old key keeps its values**, for code that has not moved yet; Doctor's CFD009
  still reports it as deprecated and still supplied, and now says what to rename it to.

A replacement needs `CF_DEPRECATED` and another valid key, refused at definition
otherwise. A derived configuration makes the copies again from the old key's inherited
values rather than inheriting them, so each value has one. The report records
`replaced_by` as an added field.

---

## D-44 — The Rust binding

**Question.** The owner values Rust as C and C++. How does a Rust program use this
library without the binding becoming a second implementation, or the build a download?

**Options.**

1. **A rewrite in Rust.** Two libraries to keep in step, and every promise - the same
   input refused with the same message - made twice.
2. **bindgen and the cc crate**, the usual way. Both are crates.io downloads, and this
   project's builds download nothing (AGENTS.md, "Safety").
3. **A hand-written FFI and a `build.rs` that runs CMake.**

**Decision: option 3.** `bindings/rust` declares the C functions it uses by hand
(`sys.rs`) and wraps them in a safe layer: a `Builder` that consumes itself into a
`Result<Config, Error>`, reads that cannot fail, `explain()`, `derive()`. Its `build.rs`
builds the library from the checkout with CMake - the one tool every build already
needs - in Release, so Windows links the same C runtime Rust does, and links it
statically. The crate has no dependencies, so `cargo build --offline` works.

**Three things the FFI must get right, and how each is held.** The C side borrows
strings - a source's name and path, a definition's lists - so every string passed in is
kept with the `Config` that may point at it. `cf_def` is mirrored as a `repr(C)` struct,
and `cf_def_init` writes `sizeof(cf_def)` into its first field: the binding compares it
with Rust's size and refuses a library built another way rather than misreading it.
And `Config` is `Send + Sync` because the snapshot is immutable and its count atomic
(D-14), not by assertion alone.

**What it found.** Its first run printed garbage where a derived change's source name
belonged: `cf_derive` kept the caller's string rather than copying it, which C callers
passing literals had never shown. Fixed in the library, not worked around in the
binding.

**Checked** by the examples: five of `examples/c` in `examples/rust`, run under cargo by
`-DCF_RUST=ON` and held to the same lines as their C twins; rustfmt and clippy clean.

---

## D-45 — Build limits in the ABI

**Question.** `cf_error` holds its key, source and message inline, in arrays sized by
`CF_KEY_MAX`, `CF_SOURCE_MAX` and `CF_MESSAGE_MAX` - build options (D-37). A program,
or a binding that mirrors the struct, compiled against other limits than the library's
reads the wrong bytes. Before 1.0 promises an ABI: are the limits part of it?

**Options.**

1. **Pointers instead of arrays**, so the struct's layout does not depend on a limit.
   Then an error needs memory to report a memory error, and `cf_error` on the stack, the
   one thing every caller relies on, stops being free.
2. **The limits are part of a build's ABI**, and nothing checks it.
3. **They are part of it, and the library says what they are.**

**Decision: option 3.** Two builds of the same version with different limits are
different ABIs, as two builds with different `CF_WITH_REAL` already are - which the
symbol list records (`gate.abi`'s `real` marks). `cf_build_limit("key")` and its
siblings return the build's values, so a consumer that cannot see the headers the
library was built with can ask: the Rust binding compares all three with the sizes it
mirrors, once, before the first call that fills an error, and refuses a library built
otherwise. `cf_error.size` stays what lets the struct grow at its end.

---

## D-46 — Reading a value without repeating its default

**Question.** A defined key has a default, and `cfg.get<int>("server.port", 8080)`
writes it a second time, where the two can drift. The owner asked for `cfg.server.port`:
the program names a key and gets its value, whatever source set it. What can each
language give?

**Options.**

1. **Member access on the configuration itself** (`cfg.server.port`). Standard C++ has
   no way to add members to a type at run time or from a file; it would need generated
   code, and generating into `cf::Config` makes one type mean different things in two
   programs linked together (an ODR violation).
2. **Code generation into the program's own namespace**, from the contract. ODR-safe,
   but a build step for every program and a second source of truth.
3. **Three run-time forms, each written once:** a declared `cf::Key<T>` that both
   defines the contract and reads it; a `cf::Setting` proxy named a part at a time
   (`cfg["server"]["port"]`) and typed where it is converted, with no fallback because the
   contract has one; and a `cf::Binding<S>` that fills the program's own struct, so the
   program writes `settings.server.port` on a type it owns.

**Decision: option 3**, in C++03, so every later standard has it too. The dot in
`settings.server.port` is the program's, which is the only place C++ allows it; the
Binding's field list is also the contract, so the default is written once. A Setting
converts to any type the wrapper reads and so cannot also be compared with `==`, which
the header says; `as<T>()` is the comparison. Generation (option 2) stays possible
for whoever wants it and is not in the library.

Found on the way: on LP64 systems under C++11 and later `cf_i64` is `long`, so the
wrapper's explicit specializations `get<long>` and `get<cf_i64>` were one specialization
twice. The header had compiled only as C++98, the one standard every example and test
built it with. The reads are now an overload set with `cf_i64`'s overload present only
where it is a type of its own, and a test builds the header as C++17 as well.

---

## D-47 — One C++ header, whatever the standard

**Question.** C++03 has no move, no `optional`, no `string_view`, no concepts; C++20
has all of them, ranges and `std::format`. The owner asked whether, since the
standards differ so much, there should be more than one interface - and then, once
there were three headers, whether one would be better: the compiler's standard
decides, from C++98 to the newest.

**Options.**

1. **The C++03 wrapper only.** Every compiler, and a modern program writes C++03.
2. **A header a standard** (`configforge.hpp`, `configforge17.hpp`,
   `configforge20.hpp`), each over the one before. Built first; the program has to
   know which to include, and a library compiled as C++17 that includes the C++17
   header forces it on everything that includes the library's own headers.
3. **One header, its parts turned on by the standard the including program is
   compiled as.**

**Decision: option 3**, at the owner's word, and the better one: a program includes
`configforge.hpp` and gets everything its standard offers, and moving to a newer
standard needs no change to an include. What made option 2 look safer is kept: under
every standard `cf::Config`, `cf::Builder` and the rest are the same classes - C++11
adds only members C++03 cannot spell (move, explicit bool, an initializer list), and
C++17 and C++20 add functions over them (`find`, `value`, `items`, `fields`, `lookup`)
and specializations of the standard's own templates (`enable_view`, `formatter`), which
change no layout and no meaning. So a library compiled as C++11 and a program
compiled as C++20 still pass a `cf::Config` between them.

A part turns on by what the standard library has, not only by `__cplusplus`
(`CF_CPP17`, `CF_CPP20`, `CF_CPP_EXPECTED`): GCC 12 says C++20 and has no `<format>`,
and a header that failed there would be worse than one offering C++17's part. MSVC's
`_MSVC_LANG` is read where `__cplusplus` is left at 199711L. `find<T>` reads through
`cf_try_*` and returns nothing for an integer `T` cannot represent - a value is the
configured one or absent, never a different number.

---

## D-48 — Reloading: who decides, and what cannot change live

**Question.** `cf_changed` and the watcher say that a file moved on. Every program then
writes the same loop - rebuild, check, swap - and the swap is where it goes wrong: a
reader on another thread takes the pointer as it is freed. The owner asked for
automatic, semi-automatic and manual reloading. And some values cannot change while the
program runs at all: a listening port, a pool's size.

**Options.**

1. **Leave it to the program**, as `examples/c/reload.c` does.
2. **A library thread that watches and calls back.** The library makes a threading
   decision that belongs to the program, which this library has refused since D-14.
3. **A helper with no thread of its own**, driven by the program's loop, in one of three
   modes, with a lock around the swap only.

**Decision: option 3**, `cf_live`. `CF_LIVE_MANUAL` reloads when asked,
`CF_LIVE_APPROVE` builds and holds a change until it is approved (an edit after it
replaces it rather than queueing behind it), `CF_LIVE_AUTO` swaps. A check, given the
running snapshot and the next, sees every one before readers can - which is where a
Doctor preflight goes. A failed build or check never replaces what is serving.
`cf_live_get` takes a reference under a lock made of the port's one atomic (a count
taken from 0 to 1), held for a pointer copy and an increment; the reloads belong to one
thread. A rebuild that finds the same values refreshes the file stamps without counting
a new generation.

**What cannot change live** is a definition flag, `CF_RESTART`. Refusing such a reload
would refuse the log level changed in the same edit; applying it would make readers
believe a port the socket is not bound to. Instead the running value is kept as the top
source, named `running`, so explain() shows the running value selected and the file's
new one shadowed - why the value is what it is stays answerable - and
`cf_live_restarts` lists the keys waiting for the restart. A `CF_RESTART` key that had
no value when the program started had nothing in use to keep and takes the new one.

---


One decision is open, because nothing has been built: profiles as a core concept
(D-12). Value interpolation (D-10), long reserved, is decided and built. Everything else is decided,
and where the implementation settled the question rather than this document, the section
says so and names what settled it.

That audit was worth doing on its own. Fourteen entries were still marked open while the
code they described had been written, tested and shipped — and a status column where
half the entries are wrong teaches a reader not to trust the column.

---

## Closing Phase 0

Phase 0 ends when every entry above reads `Decided` or `Deferred` with a written
reason, and the specification has been updated to match.

At that point the specification is frozen. Any new section afterwards has to be paid
for with code.
