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
| D-60 | Arrays in INI are PHP's `name[]` and `name[i]` | formats, INI | **Decided** |
| D-61 | Sizing: limits by build, a file's bytes transient, a stream for 2.0 | memory, readers, embedded | **Decided** |
| D-62 | Each reader held to its format's standard, by the standard's suite | formats, TOML, JSON, dependencies | **Decided** |
| D-63 | YAML's anchors, aliases and core tags are read | formats, YAML, provenance | **Decided** |
| D-64 | Definitions read from a JSON Schema, the reverse of `cf_schema` | definitions, schema | **Decided** |
| D-65 | CI and the development environment on Ubuntu 26.04 LTS, together | build, CI, environment | **Decided** |
| D-66 | JSON5, and with it JSONC, a mode of the JSON reader | formats, JSON | **Decided** |
| D-67 | Doctor reads files against a schema, and writes the schema as C | Doctor, schema, definitions | **Decided** |
| D-68 | A format of one's own, registered on the builder | formats, codecs, conformance | **Decided** |
| D-69 | A YAML parser of the library's own, on by default | formats, YAML, dependencies | **Decided** |
| D-70 | The Rust crate builds with cargo and the cc crate | Rust, build, dependencies | **Decided** |
| D-71 | C++98 to C++14 deprecated; C++17 from 2.0 | C++, compatibility | Replaced by D-72 |
| D-72 | Every C++ standard from C++98, cumulatively | C++, compatibility | **Decided** |
| D-73 | The format from the name, then the content, by several strategies | formats, codecs | **Decided** |
| D-74 | One `define`, the type given or inferred | definitions, bindings | **Decided** |
| D-75 | Checked at compile time where the language can, at run time always | definitions, C++, Rust | **Decided** |
| D-76 | Predict sizes and measure the result | file input, key space | **Decided** |
| D-77 | A reference's expansion is bounded, by a constant and not a build limit | interpolation, resolver | **Decided** |
| D-78 | A directory that cannot be read whole is an error, optional or not | drop-in, secrets, platform | **Decided** |
| D-79 | Response files (`@file`) are opt-in | command line, security | **Decided** |
| D-80 | systemd's credentials, a builder of their own | sources, secrets | **Decided** |
| D-81 | A reader's name table is seeded a decode | formats, security | **Decided** |
| D-82 | What one document may make is bounded, in the flattener | formats, security | **Decided** |
| D-83 | A directory others can write to is not read as secrets | secrets, drop-in, platform, security | **Decided** |
| D-84 | An undefined key named like a secret is kept as one | secrets, security, reports | **Decided** |
| D-85 | A secret's digest, keyed | secrets, security, reports | **Decided** |
| D-86 | Doctor's action shows no value unless asked | doctor, security, reports | **Decided** |
| D-87 | macOS's user defaults, an optional source over the flattener | sources, platform | **Decided** |
| D-88 | What 1.x promises, made smaller: the internal tier fenced | api, abi | **Decided** |
| D-89 | The experimental tier | api, abi | **Decided** |
| D-90 | The serialization library, a target of its own | serialize, build, abi | **Decided** |
| D-91 | CBOR, the JSON model in binary | formats | **Decided** |
| D-95 | Code for each binding from a contract | tools, contract, C++, Rust, Python | **Decided** |
| D-96 | A contract drafted from a sample | tools, contract | **Decided** |
| D-97 | What kind of format a codec is, told to the program | formats | **Decided** |
| D-99 | Three programs behind one front door | tools | **Decided** |
| D-100 | One directory per component | architecture | **Decided** |
| D-101 | The vector library, not ConfigForge's | architecture | **Decided** |
| D-102 | One copy of each text and number helper | architecture | **Decided** |
| D-103 | Reading what is almost a format | architecture | **Decided** |
| D-104 | Work moved to build time, in layers | architecture | **Decided** |
| D-105 | Build profiles by name | architecture | **Decided** |
| D-106 | A newer C only where a file needs it, and said so | architecture | **Decided** |
| D-107 | One refusal, the same in every format | architecture | **Decided** |
| D-108 | Every text is UTF-8 inside; another encoding only by name | architecture | **Decided** |
| D-109 | What a format can hold, and every pair tested | formats, tests | **Decided** |
| D-110 | Generators outside the tree | generators | **Decided** |
| D-111 | The comparison with other libraries: one set of flags, the same work, and a history kept on its own branch | benchmark, CI | **Decided** |
| D-112 | The MISRA record is plain text of file and count, three kinds, and the gate fails open nowhere | compliance, CI | **Decided** |
| D-113 | A unit on an int definition: `cf_def.unit`, an open `cf_unit`, and the text read in the unit's base | definitions, `cf_numeric.h`, the contract | **Decided** |
| D-121 | A read that finds no definition, or another type, is recorded and reported | configuration, diagnostics | **Decided** |
| D-122 | A migration is a function on the deprecated key's definition, run on each value as it is copied | definitions, migration | **Decided** |
| D-222 | A unit is an open value: a name, a parser, a formatter and its own options; a program defines its own | `cf_numeric.h`, `cf_def.unit` | **Decided** |
| D-230 | Options with defaults are one convention: `size` first, `_init`, NULL the defaults; a fixed limit a public call reaches is an option with its old value as default | public options structs, `gate.options` | **Decided** |
| D-231 | The platform is a part: `none` is built and tried; what it cannot give answers `CF_E_CAPABILITY` | platform, build options, CI | **Decided** |
| D-232 | Every public function states one failure guarantee; the library never ends the program | public headers, `docs/Guarantees.md`, `gate.guarantees` | **Decided** |
| D-238 | Thread, signal and allocation claims beside the guarantee, the weakest by default | public headers, `docs/Guarantees.md`, `gate.guarantees` | **Decided** |
| D-239 | The library writes a system error's text from its own table | `lib/platform/src/errno_text.c`, `gate.cert` | **Decided** |

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
          index keys at or beyond that count are discarded during resolution
          discarded elements remain visible in explain() as truncated candidates
          an opt-in append mode per scalar-list definition (see below)
```

**Consequence.** Provenance is per key and has one shape for every parameter in the
system, which is what allows the diagnostics output to be specified as a stable, testable
contract. The truncation rule requires array-aware sources to emit a count, and it becomes
a dedicated Doctor finding and a required test fixture.

**As built** (`src/core/validate.c`, between policy and interpolation). The length is
`<array>.count` when its winning value is an integer of zero or more, from whichever
source wins it, the default included. An element is a key `<array>.<n>` with `n` all
digits, compared as a number (`007` is 7). One whose `n` reaches the length has every
candidate marked `truncated` - "past the end: hosts.count is 1, from cli" - and no value,
so it is not read, listed, written or in the digest, while `explain()` and the report
keep it. That holds for an element from any source, including one that outranks the
source of the count: the count decides, whoever supplied the element. Within the length
keys still resolve one at a time, so one source's `hosts.0` and another's `hosts.1` can
make an array neither wrote whole; a source that means to replace a list supplies its
count with it. A count that is not an integer, is negative, or is a template when the
pass runs fixes no length. A group with any member besides indexes and `count` is a
table and is left alone, and arrays inside arrays are cut outermost first.

**Declared scalar lists.** A sized extension to `cf_def` adds REPLACE
and APPEND, a separator and copied typed defaults. Existing descriptors,
including incomplete extensions and old tail padding, remain scalars.
The element model stays flat; immutable count/item projections supply
the contract without a mutable lookup cache. Declared indices are
canonical decimals and the selected list must be dense.

APPEND concatenates each permitted source's bounded contribution from
low to high precedence, or in policy order. A count-free source patches
final indices. Plans are collected before keys move; original keys,
locations and insertion order survive, and aggregate counts are marked
as synthesized. Snapshot derivation replays original coordinates, not
already shifted keys. Resolved images and Report Replay are append
bases rather than extra raw contributions. References run after layout;
append counts must therefore be literal integers.

The flat-source forms share one sink. Environment and dotenv values
split at the declared separator; repeated command-line flags are
individual elements. An empty environment value means an empty list,
while an empty CLI argument is an empty element. Natural environment
names are accepted only when unambiguous; canonical escaped names win.

Reload holds an entire restart-only list as typed resolved values.
Ordinary text overrides were rejected as a solution: they would append
the list again and reinterpret literal reference text. Held list
members are pinned ahead of source policy and remain pinned in derived
views; this preserves the running value rather than accepting a new
source assignment.

Writes use final indices at the facade and source-local indices at the
callback. Existing contributing elements can be mapped exactly.
Blindly forwarding indices would update the wrong element; treating an
aggregate count as a local count would change the wrong length. Those
structural changes, and contributor deletions that could create holes,
are refused rather than guessed. Count-free stores can set and remove
final-index patches. Complete source-local structural updates remain
provisioning operations, outside `cf_write`.

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
a different secret gives a different one, written after many commits in which every digest
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
build                CMake 3.10; the no-build-system path is the amalgamation (D-31)
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
file, and `scripts/check/stack.cmake`, run after the library links. Not `-Wstack-usage`, which
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
directions. It is the document and `CF_MAX_DEPTH - 1` levels inside it, and a writer
writes that much and no more: the JSON encoder had written one level past what its own
reader took, the TOML encoder had counted only the inline levels of an array, and the
TOML reader had held a key or a header to eight parts, so tables nested nine deep and
more were written and could not be read back. The emitter now refuses the level after the
last before any format sees it, with the same error for all of them.

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

`lib/config/tests/test_features.c` checks each part in both directions - working when it is
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
are refused by name with what to write instead (anchors, aliases and the core schema's
tags are read since D-63). Written YAML quotes every string, so it
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
Conan recipe in `set_version()`. `cmake -DTO=x.y.z -P scripts/release/version.cmake` rewrites
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

**Question.** Rust matters here as much as C and C++. How does a Rust program use this
library without the binding becoming a second implementation, or the build a download?

**Options.**

1. **A rewrite in Rust.** Two libraries to keep in step, and every promise - the same
   input refused with the same message - made twice.
2. **bindgen and the cc crate**, the usual way. Both are crates.io downloads, and this
   project's builds download nothing (CONTRIBUTING.md, "Safety").
3. **A hand-written FFI and a `build.rs` that runs CMake.**

**Decision: option 3.** `bindings/rust` declares the C functions it uses by hand
(`sys.rs`) and wraps them in a safe layer: a `Builder` that consumes itself into a
`Result<Config, Error>`, reads that cannot fail, `explain()`, `derive()`. Its `build.rs`
builds the library from the checkout with CMake - the one tool every build already
needs - in Release, so Windows links the same C runtime Rust does, and links it
statically. The crate has no dependencies, so `cargo build --offline` works.
Superseded for the build by D-70: cargo with the `cc` crate, not CMake.

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
writes it a second time, where the two can drift. Wanted: `cfg.server.port`, where
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
has all of them, ranges and `std::format`. Since the standards differ so much, should
there be more than one interface - and once there were three headers, would one be
better, the compiler's standard deciding, from C++98 to the newest?

**Options.**

1. **The C++03 wrapper only.** Every compiler, and a modern program writes C++03.
2. **A header a standard** (`configforge.hpp`, `configforge17.hpp`,
   `configforge20.hpp`), each over the one before. Built first; the program has to
   know which to include, and a library compiled as C++17 that includes the C++17
   header forces it on everything that includes the library's own headers.
3. **One header, its parts turned on by the standard the including program is
   compiled as.**

**Decision: option 3**, the better one: a program includes
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
reader on another thread takes the pointer as it is freed. Programs want automatic,
semi-automatic and manual reloading. And some values cannot change while the
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

## D-49 — Python reaches the library through ctypes

**Question.** Python had Doctor (over its SARIF) and not the library. How should a
Python program call it?

**Options.**

1. **A CPython extension module** in C. Fast, but one binary per Python version and
   platform, and a compiler on every machine that installs from source.
2. **cffi or pybind11.** A dependency to download, which nothing else here has.
3. **ctypes over the shared library.** The standard library only; one package for every
   Python 3.8 and later; the price is that the struct layouts are written again in
   Python.

**Decision: option 3.** A configuration is read at start-up and on a reload, not in an
inner loop, so ctypes' call overhead costs nothing a program would notice. The layouts
are the ones the Rust binding mirrors, and the same check guards them: the library's
limits (`cf_build_limit`, D-45) are compared at import and a library built otherwise is
refused by name. The package is pure Python and finds the library at
`$CONFIGFORGE_LIB`, beside itself, or on the platform's search path, so a wheel that
carries the library is a packaging step, not a code change. A test of it runs only
against a shared build, so `verify` gained `EXTRA=shared`, the local twin of CI's
shared job.

---

## D-50 — Platforms in tiers, and a verify that is fast enough to run every time

**Question.** A full verify with every extra step took hours on the development
workstation and held everything else up: Linux and Windows could not run at once,
every preset was rebuilt from nothing, and a hung test held the run until someone
noticed. Wanted: tiers - GCC, Clang and MSVC first, the rest after - and faster work
without a larger disk.

**Options.**

1. **Everything on every commit**, as before. Exhaustive, and so slow it was skipped.
2. **Only the default preset on every commit.** Fast, and the sanitizers, `-fanalyzer`
   and the `-O2`/`-Os` stack budgets - which found real bugs - would run rarely.
3. **Tiers, and the same steps made cheaper.**

**Decision: option 3.**

* **Tier 1, every commit:** GCC through the five presets (sanitizers, the analyzer, the
  portable and minimal optimisations), Clang, and MSVC (the quick run on Windows):
  `verify -DTIER=1` and the Windows quick run, started together.
* **Tier 2, before a release and with any unit that touches what it covers:** the thread
  sanitizer, the shared build with the Python binding, YAML/XML/SQLite, musl, TinyCC and
  pcc, libkqueue, clang-tidy, fuzzing, coverage, FreeBSD's real kqueue
  (`scripts/buildenv/freebsd.sh`), the static Alpine build at `-O3`, and the bindings'
  own tools (cargo test, clippy, rustfmt): `verify -DTIER=2` and the rest by name.
* **Tier 3, where the hardware is:** macOS, through its CI job when Actions runs.

What makes it fast: Ninja and ccache where they exist (the staged copy sits at the same
path every run, so a unit recompiles what it changed); a private copy of the index, so
the Linux and Windows runs start together instead of one failing on `index.lock`; and a
per-test timeout (`TEST_TIMEOUT`, 900 s), so a hang fails in minutes. ccache is capped
at 5 GB. A tier-2 failure found at release time is fixed like any other; what tiers
change is when it is looked for, not whether.

---

## D-51 — How the interface grows in 1.x

**Question.** From 1.0 a program built against one 1.x must run against every later 1.x.
A review of the interface before the freeze (2026-10-30) found that the promise in
`cf_error.h` - "a newer library sees an older caller's smaller struct and writes only
what fits" - was not kept anywhere: nothing read `cf_error.size`, the library copied
whole structs of its own size into callers', `cf_def_init` cleared the library's
`sizeof` over the caller's storage, `cf_builder_define` refused any descriptor smaller
than the library's, and `cf_live_status_get` accepted only its own exact size. Each is
harmless while every size is equal, which is exactly why it would have gone unnoticed
until 1.1.

**Decision.** A struct the caller allocates and the library fills or reads carries its
size first, and:

* **1.0's fields are the floor.** They are always written and read, whatever `size`
  says - so a `cf_error` declared without `CF_ERROR_INIT` still works - and a size below
  1.0's is refused where the struct is an input.
* **A field added in 1.x is touched only where `size` reaches it.** The library keeps,
  for each struct, the offset where 1.0 ended.
* **Never a whole-struct copy into the caller's memory.** `cf_error_copy` copies field by
  field; `cf_live_status_get` copies as far as both structs reach.
* **The caller's size travels with the call that initialises.** `cf_def_init` is a
  macro over `cf_def_init_sized(d, sizeof(cf_def), ...)`, so the size is the one the
  caller was compiled with, not the library's.
* **No growable struct inside another.** `cf_live_status` held a `cf_error`, so any
  field after it would sit where the error's version put it; the error moved to its own
  call, `cf_live_last_error`.

The same review found the rest of what 1.x would have been stuck with, decided here:

* **What a program implements carries a size too**: `cf_source_vtable`, `cf_codec` and
  `cf_doc_sink` begin with `size_t size`, so 1.x adds a member and calls it only for a
  table that has it. `cf_builder_source` and `cf_builder_file_codec` refuse one without,
  by name; `cf_builder_source` copies a source's 1.0 members one by one.
* **What a program fills for a call that only reads it carries a size too** - a second
  pass (2026-11-01) found two the first had missed: `cf_report_info`, which `cf_report`
  reads, and `cf_doctor_gate`, which `cf_doctor_live_check` reads. Each begins with
  `size_t size` and has an `_INIT` macro, `CF_REPORT_INFO_INIT` and
  `CF_DOCTOR_GATE_INIT`; `NULL` is still the defaults. A report has no error to give,
  so an `info` below 1.0's size writes nothing - the empty string, as for a library
  without `CF_WITH_REPORT` - and a gate below it refuses every snapshot, naming
  `CF_DOCTOR_GATE_INIT`. Done before 1.0 because adding a member to either later would
  have been an ABI break.
* **Flags cannot be confused**: definition flags take bits 0 to 15 and source flags 16
  and up. `CF_OPTIONAL` and `CF_REQUIRED` were both bit 0 and reached the same
  `unsigned flags`, so `cf_builder_file(..., CF_REQUIRED)` meant "optional" without a
  word. A bit a function does not know is refused by name, so a flag from a later 1.x
  is never silently ignored by an earlier library.
* **A function that will want more arguments takes a struct**: `cf_live_open_with`
  and `cf_live_options`, which also give build and check a `user` each.
  `cf_builder_replay` gained the flags every other file-like source has.
* **Structs the library fills in the caller's memory keep room**: `cf_iter` and
  `cf_image` end in `void* reserved[4]`.
* **An internal tier**: `cf_platform.h`, `cf_flatten.h`, `cf_numeric.h`, the
  definition set and every `cf_ks_` function but those a source's load uses are exported
  for Doctor, the bindings and the tests, and outside the 1.x promise; each header says
  so. Hiding them would break Doctor against a shared library.
* **The bindings' own surfaces**: Rust's `sys` is hidden and outside the crate's semver,
  what can grow is `#[non_exhaustive]`, and a NUL inside a string is an error, not a
  panic. Python's `Config` is a `collections.abc.Mapping`; its methods are the whole
  set for 1.x, since a method added later would hide a key read as an attribute, and
  what 1.x adds comes as module functions. Both refuse a single-threaded library, whose
  snapshots their threads would share.
* **Exceptions stay out of C**: the C++ `Live` catches what a build or a check throws;
  a build that throws is a build that failed.

Rejected: a version number instead of a size. A size says the same and also says how
far it is safe to write, which a version would need a table to say.

---

## D-52 — What is not an error goes to a log the program gives

**Question.** An error stops a configuration and is reported once, in a `cf_error`.
`CF_UNKNOWN_WARN` existed and warned nobody: it counted. A deprecated key in use, a
value refused and fallen back from, a reload that failed at three in the morning - each
deserved a line in the program's log, and the library had no way to give one.

**Options.**

1. **Write to stderr.** A library that prints is a library a daemon, a GUI and a test
   suite all have to silence.
2. **A list of warnings on the configuration**, read after create. Nothing to say
   where a reload's warning goes, and a reader has to poll for what happened.
3. **A sink the program gives**, called with structured events.

**Decision: option 3.** `cf_log_fn` receives a `cf_log_event` - sized (D-51), with a
level whose numbers are Python's `logging` levels, a stable event name, the key, the
source, the code and a sentence. Events are made from the resolved key space after
create, so the resolver itself has no side effects, and a derived configuration logs
where its original did. `cf_live` adds each reload's result and sets its sink on every
builder its build function returns. The library never writes anywhere on its own, and a
secret's value never reaches a message: a refusal's reason is left out for a sensitive
key, though the reasons are written to carry no value anyway.

---

## D-53 — C89 inside, the program's own standard at the interface

**Question.** C has versions too: should the library use what a later one offers where
it is supported? The library is C89 so that it builds with every
compiler in the matrix, pcc and TinyCC among them; a program compiled as C11 or C23
should not be held to C89 by that.

**Decision.** As the C++ header does for C++ (D-47): the core stays C89, and the headers
add what the standard the program is compiled as can say, to the same declarations and
the same ABI.

* **C11**: `cf_get(cfg, key, &out)`, a `_Generic` over `int`, `long`, `long long`,
  `bool`, `double` and `const char*` - typed by where the value goes, refusing an integer
  the target cannot hold rather than truncating it. And `<stdatomic.h>` as an atomics
  path for a compiler that is neither GCC-like nor MSVC.
* **C23 and C++17**: `[[nodiscard]]` on the calls whose result is the only way to know
  they failed - create, derive, try, open, write. Not GCC's `warn_unused_result`, which
  a `(void)` cast cannot silence, so a call ignored on purpose could not say so.
* **Any standard**: `CF_STATIC_ASSERT` holds the library to what D-51 rests on - every
  sized struct begins with its size, and the integer widths are the interface's.

The header gate compiles every public header standalone as C89, C11 and C23, where the
compiler knows them.

---

## D-54 — Create, update, delete: one edit, one transaction

**Question.** What accessor layer makes CRUD easy? Reading was
already easy; the rest was `cf_write` over an array of items built by hand, with no way
to delete - a key written to a store stayed there, shadowing the file under it for good.

**Options.**

1. **A key handle with set and delete** (`cf_key_set(cfg, "a.b", ...)`), each call a
   write. Several calls are several transactions, and the contract sees each value
   alone: a change that is only valid as a whole could not be made.
2. **An edit gathered and committed whole.**

**Decision: option 2.** `cf_edit_begin` on a snapshot and a writable source;
`cf_edit_set_*` and `cf_edit_unset` gather the change, copying every string; commit is
`cf_write` over it, so an edit is checked by exactly the rules a direct write is, and the
store writes it in one transaction. Deleting is a `cf_write_item` with
`op = CF_WRITE_DELETE`; since items are often filled member by member, every `op` other
than `CF_WRITE_SET` and `CF_WRITE_DELETE` is refused, so an uninitialised one fails
instead of deleting. A snapshot never changes; `cf_live_commit` commits and reloads, so
an administrator's change reaches readers the way an edited file does, through the
build, the check and the mode.

---

## D-55 — WebAssembly as a tier-2 target

**Question.** Should WebAssembly be a target - a configuration library in an edge
runtime, a plugin host, a browser - and what would it take?

**Decision.** wasm32-wasi (WASI preview 1), through clang's own target and wasi-libc,
as distributions ship them, with no SDK to download. Preview 1 has no threads, so the
build is `CF_SINGLE_THREADED` and says so (D-14); it has no file notification, so there
is no change handle and `cf_changed` polls, as on any platform without one. Tests run
under Node's WASI, from the distribution like every other tool, through a runner that
maps the test's working directory and the repository. It is tier 2 (D-50): built and
run before a release.

It paid for itself on the first run: the only 32-bit target in the matrix found an image
header whose record count wrapped `size_t` and read past the image - on every 32-bit
platform, never on 64-bit.

---

## D-56 — What 1.x promises, and the check that holds it

**Question.** From 1.0 a program built against one 1.x release must keep working with a
later one, without a rebuild. What exactly is promised, and what keeps the promise
other than care?

**Decision.** Semantic versioning, over the public headers' C interface: within one
major version, a later release links and runs where an earlier one did. Breaking, and
therefore only in a major release: a function, macro or enumerator removed or renamed;
a parameter, return or enumerator value changed; a struct's member moved, removed or
retyped; a meaning changed for input that was accepted before. Not breaking: a new
function, enumerator, flag bit or error code; a member appended to a sized struct (D-51),
which a caller built against the earlier, smaller size never sees; a refusal of input
that was always documented as wrong. The internal tier D-51 names (`cf_platform_*`,
`cf_ks_*`, `cf_flatten*`) is outside the promise, and so are the text of messages and
the order of findings. The C++ header and the bindings are source interfaces over the
same C ABI and follow its version.

The check is `verify -DEXTRA=abi`, in tier 2 and so before every release: the shared
library built from the last release tag and from the tree, with debug information, and
compared by libabigail's `abidiff` against the public headers. An incompatible change
within one major version fails the run; across a major version, and before 1.0, the
report is printed and the run goes on. A change abidiff calls compatible - a function
added - is printed either way, so the release notes can name it.

---


## D-57 — Written configurations preserve their values

**Question.** The 1.0.0 release left three known round-trip defects:
long JSON/TOML strings, literal references on write, and names that are
both scalars and containers. Which fixes preserve the library's model?

**Alternatives.** Refusing long values on write would hide the reader's
accidental limit by removing useful output. A larger fixed buffer would
only move it and spend stack. Replacing the built-in readers with a new
dependency would change the default build without fixing its contract.
Instead, the reader measures a string with its existing grammar and
asks the document sink for storage. D-51's size permits an optional
`reserve_text` callback; the flattener supplies geometrically growing,
reusable arena storage. Missing storage and exhausted storage produce
different explicit errors. The codec owns no allocator, and a fixed
arena never falls back to the heap.

An encoder is also a producer of `cf_doc_sink`, so it must not write a
new, larger struct into an old caller's object. It fills and advertises
only the 1.0 prefix; the new callback is for reader-owned sinks.

For literal `${`, refusing values would reject valid defaults, and
escaping in the codecs would change raw document and report semantics.
`cf_emit` instead wraps the encoder and inserts one dollar per literal
opening. A backwards move over only the stored bytes preserves every
truncated prefix without allocating; the original value supplies the
full measured length.

A value and a container with one name cannot be represented portably
without inventing a format convention. Refuse the collision at the
shared document walk, after applying the selection. Flat configurations
remain valid, and writing only one side remains valid. No parser is
asked to accept duplicate member names.

**Checks.** Round trips include kilobyte strings, raw and escaped
characters, fixed-arena exhaustion, old and partially sized sinks,
encoder output-prefix canaries, literal references through construction,
every output capacity, and filtered collisions.

---

## D-58 — Language packages carry the native library

**Decision.** Use each language's packaging tools over the existing
CMake build, without introducing runtime dependencies or maintaining a
second native implementation.

A pure Python wheel that requires a separately located shared library
is not a standalone installation. scikit-build-core builds that library
and installs it beside the ctypes package, under the filename the
loader already searches. The root `pyproject.toml` keeps both source
trees inside the source distribution; a manifest below `bindings/`
would leave the C sources outside its archive. The wheel uses
`py3-none` and a platform tag: ctypes does not use the CPython extension
ABI. Only the Python install component enters the wheel, not a CMake
SDK or Doctor executable. `CONFIGFORGE_LIB` remains an explicit override.

Cargo cannot include sources or examples outside its package directory.
Checking in a vendored copy would create another tree to keep current.
Instead, `scripts/release/package_rust.cmake` stages the crate, native CMake
sources, licence and examples together before `cargo package`.
`build.rs` prefers those packaged sources and otherwise builds from the
checkout. It refuses missing sources instead of linking a system
library that might have different limits. Cargo stays dependency-free
and offline. Packaging is not registry publication.

**Checks.** Build a Python source distribution and its wheel, install
without dependencies, verify that the loaded library is inside the
installed package, then run the binding suite there. Cargo verifies its
archive and tests the unpacked crate without the original checkout.
The shared CI jobs perform both checks on Linux, Windows and macOS.

The package check uses an isolated Python environment. CI must install
into a virtual environment too: a runner whose system site-packages is
read-only makes pip use the user site, which Python's `-I` correctly
excludes. Do not weaken the isolation to accommodate that installation.

## D-59 — The documentation site reads the source documents

**Decision.** Build with MkDocs, its bundled Read the Docs theme and search, and
Pygments for build-time highlighting. A small file hook retains the
repository-relative layout, including the root README and contributor
documents. No copied Markdown tree, parallel API reference, hosted
search, remote fonts or browser-side highlighter is needed.

A separate documentation source would eventually disagree with the
repository. A larger theme adds dependencies without improving the
contract here: readers need navigation, searchable API names and links
that work. Browser testing rejected the default theme: its colour
toggle assumes highlight.js stylesheets exist, and its scroll tracking
fails on this repository's numbered heading anchors. The bundled
alternative needs neither a theme fork nor changed source anchors.
Wide tables scroll within the page, and long names wrap on narrow screens.

The site follows `main`, including unreleased changes, and says so on
its homepage. Its workflow builds pull requests but only deploys
`main`; publishing documentation does not publish a library release.

**Checks.** A strict build checks links and anchors after the generated
API gate. `scripts/check/check_docs.py` requires the rendered entry points and
searchable API symbols; an empty-index negative proof confirms it
refuses missing search data. Browser checks exercise actual search,
its result links and narrow-screen navigation under the deployment
path prefix. Documentation dependencies are tested in a clean
environment, not one retaining an earlier theme's packages.

## D-61 — Sizing: limits by build, a file's bytes transient, a stream for 2.0

**Question.** Every limit is a fixed buffer today (`CF_KEY_MAX` and its
siblings, D-37): a key, a message, a path, a depth. Three modes were
asked for - full-dynamic (no limit but memory), semi-dynamic (buffers on
the stack that grow from the allocator up to a cap) and manual (fixed) -
and, with them, how much of a file is held: the whole file (dump), the
file only while it is read (transient dump), or a few kilobytes at a
time (stream). Which, and when?

**Decision.** Three parts, each where it can be done honestly.

1. **Manual sizing is the mode, by build.** The limits are build options
   already, each measured (D-37, Maintaining.md "Measured"), and a
   profile sets them: `minimal` runs with 64-byte keys and a 1 KB stack
   budget. A semi- or full-dynamic key or message cannot reach the
   interface in 1.x: `cf_error` holds its `key` and `message` as arrays,
   and every program that declares one has their size compiled in.
   Growing them is a 2.0 change, with the dynamic modes, decided there
   and not in a 1.x release.
2. **A file's bytes are transient, now.** A file was read whole into the
   configuration's own arena and stayed as long as the configuration did,
   though the key space copies every value it keeps and nothing reads the
   bytes again. They are read into an arena of their own, from the same
   allocator, and wiped and freed once the reader is done; the steady
   footprint loses the file. A fixed arena - a program that gave the
   library its whole memory - keeps the bytes in it, as before, since
   there is nowhere else to put them.
3. **A stream is 2.0's.** Feeding a reader in chunks would bound the peak
   rather than the steady footprint: what a device without room for the
   whole file needs. It changes `cf_codec`'s `decode` from a blob to a
   source of chunks, an interface change, and every reader that looks
   back - TOML measures a string and reads it again, INI looks back at an
   earlier array run, YAML's implicit keys look ahead along a line - is
   reworked. It is the first item of 2.0, with the sizing modes.

**Held to.** Tier 1's sanitised builds read every file through the
transient arena, so a value left pointing into the freed bytes is an
error there, not a guess; the bytes are wiped before they are freed, so
a secret in a file does not outlive its reading in freed memory, as
secrets are kept out of it elsewhere (cf_arena_wipe).

---

## D-75 — Checked at compile time where the language can, at run time always

**Question.** A key's name, its range and its default against the range
are refused by the library at `cf_builder_create`: a mistake waits for
the program to run. Most keys are written as constants. Can the
compiler refuse them instead, without a second rule that drifts from
the library's?

**Decision.** The same mistake is caught in every build, and earlier in
the ones that can. The library's check at create stays, in every
language and standard; the compiler's is added where the language has a
constant evaluation, by the library's own rule (`cf_key_valid`, with the
build's `CF_KEY_MAX`): a canonical name, a range whose low end is not
above its high end, a default within its range (no range when the two
ends are equal, as in `cf_builder_int`).

* **Rust**: `Key::new` and `range` are `const fn`; a `const` `Key` that
  breaks the rule does not compile.
* **C++98**: nothing new; `cf::key_is_valid` exists as an ordinary
  function, the same rule.
* **C++11**: `cf::key_is_valid` and `Key<T>`'s constructors are
  `constexpr` (a recursion; a loop from C++14's relaxed `constexpr`). A
  `constexpr` Key that breaks the rule is a compile error, and the error
  names which: the failed check calls a function that is not
  `constexpr` - `not_a_canonical_key_dotted_lowercase_ascii`,
  `a_range_whose_low_end_is_above_its_high_end`,
  `a_default_outside_its_own_range` - and at run time that function
  returns its argument, leaving the refusal to the library.
* **C++17**: `cf::field` and `cf::fields` are `constexpr`, so a field
  list of number and bool keys can be a constant, checked the same way.
* **C++20**: `constinit` asks for the check on a Key the program may
  still assign.
* **C11**: `CF_DEFINE_INT` and `CF_DEFINE_TEXT` take a literal key and
  constant bounds, and `_Static_assert` refuses an empty or overlong key, a
  range upside down, a default outside its range, a text default outside
  its length bounds and a key both required and defaulted, each by the
  library's words. A key's characters and a real's range are not C11
  constant expressions and are checked at create; `CF_DEFINE` is as it
  was.
* **doctor gen**: the contract it writes checks each key by
  `cf::key_is_valid` from C++11, and from C11 a key's length, an int
  default against its range and a text default against its length, through
  C11's own checks; as C89 it is what it was.

A Key that is neither `constexpr` nor `constinit`, a name known only at
run time, a `Key<std::string>` (std::string is no constant before
C++20), a `Setting` - a name put together at run time - and `Binding`'s
fields are checked at create, as before. Asking for the check is the
program's choice: a check that turned every `static const` Key into a
compile error would change what a C++11 program that builds already
says (D-72).

**Held to.** `test_wrapper_access` compares `cf::key_is_valid` with
`cf_key_valid` key by key, and is compiled as C++98, 11, 14 and 17; the
C++17 and C++20 tests declare constant field lists and a `constinit`
key, and C11's seven; the generated contract compiled as C89, C11 and
C++11, and one from a schema whose text default is longer than its
`maxLength`, which must not compile as C11 or C++11. Each mistake is a program that must not compile
(`cmake/ConfigForgeMustNotCompile.cmake`), under every standard from
C++11 the compiler has; its test passes only when the build fails with
the check's name in the output, so a source that fails for another
reason does not pass.

---

## D-74 — One `define`, the type given or inferred

**Question.** A program declares a key with `cf_builder_int`,
`cf_builder_text` and their siblings, and the bindings mirror them
(`define_int`, `define_text`). Can one `define` do, with the type given
as a parameter or understood from the default?

**Decision.** The work is done once, in C, and every binding passes
through it: `cf_builder_key(b, key, type, dflt, min, max, flags)`. The
default and the bounds are text, read by the rules a file's value is;
the type is a parameter, or `CF_TYPE_NULL` for "the default's text
decides" - `true`/`false` a bool, an integer an int, a decimal a real,
anything else text. A default or bound not of the type, half a range,
a range on a bool or text, and a key with neither a type nor a default
are refused by the library, with the key, and `cf_builder_create`
reports them. Each binding only turns its value into text and names the
type, so the rules and their errors are the same everywhere. The typed
calls stay (D-56). Cumulative, as D-72 is for C++:

* **C89**: `cf_builder_key` itself.
* **C11**: `CF_DEFINE(b, key, dflt)`, a `_Generic` macro over the
  default's C type to the typed calls. C11's `true` and `false` are ints,
  so a bool default is written `(bool)false`. Not under C89.
* **C++98**: `Builder::define(key, dflt)`, overloaded on `bool`, `int`,
  `cf_i64`, `double` and `const char*`/`std::string`, with a range for
  the numbers. The overload is chosen at compile time and calls the typed
  function, so nothing is turned into text and back; `Key<T>` stays for
  a key read in more than one place. A `long` default is written as
  `cf_i64` or `int`, since `long` converts to both.
* **Python**: `Builder.define(key, default, type=None, min=, max=,
  flags=)`, through `cf_builder_key`; the library refuses, at `create`.
* **Rust**: `Builder::def(key, dflt)` and `def_with(key, dflt, range,
  flags)`, the range of the default's type, through `cf_builder_key`;
  `key` was taken, by a declared `Key`.

Inference only from a default the program wrote, never from a value a
file supplies: a contract that changed type because a file did would
not be one.

**Held to.** Each spelling's tests declare the same keys as the typed
calls and compare the two configurations' `cf_schema` output, which
must be byte for byte the same; the C, Python and Rust tests check that
each refusal comes from the library as a definition error.

---

## D-73 — The format from the name, then the content, by several strategies

**Question.** A file's format comes from its extension or is named;
otherwise reading it fails. Can the library tell the format itself, and
how, without the guessing the codec registry has always refused - a
format sniffed from a few bytes, which fails confusingly the first time
a file is renamed?

**Decision.** Several strategies, each simple, tried in turn; the first
that answers decides, and each says why it answered:

1. the format the program names;
2. the name: the format its extension gives - the builder's own codecs
   (D-68), then the built-in ones - is tried first, whatever else
   follows. If it reads the file, it is the format. If it does not, the
   file is refused, as now, and the error adds the format the content
   looks like, as a hint: a renamed file is the commonest cause;
3. the content, when the name gives no format (no extension, or one no
   format claims; that was an error before, so no file that was read is
   read differently):
   * **a look at the first bytes** orders the candidates, cheaply: `{`
     or `[` puts the JSON family first, `<` XML, a `[section]` line INI
     and TOML, a `key:` line YAML, a `key =` line TOML and INI;
   * **reading** decides, in that order: each codec reads the bytes into
     a sink that keeps only a hash of the events, and a codec that
     accepts them and yields at least one keyed value is a candidate. A
     strict format (the builder's own, JSON, JSON5, TOML, XML) is taken
     at once; a loose one (YAML, INI) only when no other loose format
     reads the file into different events - otherwise the file is
     refused as ambiguous, naming both. `.properties`, whose grammar
     accepts nearly any text, is never chosen from content.

The look only orders, and never decides: a wrong first guess costs one
more reading, and the reading is what is trusted.

**Every source of bytes, not only files.** Detection works on bytes
(`cf_codec_detect` takes a `cf_blob` and an optional name to take the
hint from), so it serves whatever delivers a document:

* a file, a drop-in directory, and standard input or a pipe (whose name
  is no hint, so the content decides);
* **a document the program holds in memory** - fetched from a server,
  embedded in the binary, read from a database - through a new source,
  `cf_builder_document(b, name, rank, bytes, n, format, flags)`: the
  format named, or NULL for the name's hint and then the content. Its
  places in `explain()` are the document's name and line;
* **a whole document in one environment variable or option** (`APP_CONFIG
  ='{"server": {"port": 8080}}'`), opt-in per variable, read through the
  same detection.

Sources of key and value pairs - the environment, the command line, a
program's pairs, the registry, a secrets directory, a store - carry no
format: there is nothing to detect, and their text is typed by the
definitions, as now.

The choice is shown, not hidden: the source's error names the formats
tried, `cf_source_file_format` says which was taken and how (named,
extension, content), and Doctor's `explain` prints it beside the file.
`cf_codec_detect` is public, for a program that reads bytes of its own.

**Held to.** Tests per format with the extension removed; a file that
two loose formats read differently, refused; an empty file, refused;
and the conformance suites' valid files, renamed without extension, each
read as their own format.

---

## D-72 — Every C++ standard from C++98, cumulatively

**Question.** D-71 deprecated C++98 to C++14 and planned C++17 for 2.0.
The request it answered was read as "modern C++ only"; it was
"modern C++ as well". Which is it to be?

**Decision.** Cumulative, for good. C++98 is the floor and is never
dropped, in 1.x or 2.0; each later standard adds to what the one before
has and changes none of it, so a program that moves from C++03 to C++23
keeps every line it wrote and gains the newer spellings beside them.
The header says which layer the including program has as
`CF_CXX_LEVEL` (98, 11, 17, 20 or 23), by what the library offers and
not only by what the compiler claims (D-47). D-71's warning and
`CF_CXX_LEGACY` are gone; the examples are C++98 again, since an example
that builds at the floor builds everywhere above it. A new facility goes
into the lowest layer that can say it, and a layer above adds only what
that layer's language makes possible: `std::optional` returns at 17,
concepts at 20, `std::expected` at 23, and the C++98 spelling of each
stays beside it.

**Held to.** The header gate compiles the header standalone with
`-pedantic -Werror` as C++98, 11, 14, 17, 20 and 23, and fails a level
that reports itself above the standard it was compiled as. The C++98
consumer and wrapper tests are C++98 on purpose.

---

## D-71 — C++98 to C++14 deprecated; C++17 from 2.0

**Replaced by D-72** before any release carried it.

**Question.** One header serves every standard from C++98 (D-47), and
modern C++ only was asked for. Can 1.x drop the old standards?

**Decision.** Not in 1.x, where a program that builds must go on
building (D-56); deprecated, said, and dropped in 2.0. Below C++17 the
header says at compile time that C++98 to C++14 are deprecated and 2.0
requires C++17, once per translation unit, as a warning (GCC, Clang) or
a message (MSVC); a program that knows defines `CF_CXX_LEGACY`. The
examples are C++17 now, as new code should be; the C++98 tests and the
C++98 consumer stay until 2.0, saying `CF_CXX_LEGACY`, and the header
gate checks both that every standard compiles and that C++11 without
the macro is told. In 2.0 the C++03 core goes, and `std::optional`,
`std::string_view` and structured bindings simplify what is left; C++20
modules come where the compilers in the matrix build them.

---

## D-69 — A YAML parser of the library's own, on by default

**Question.** INI, JSON, TOML and JSON5 are read here, streaming,
allocation-free, every value with its place; YAML alone needed a
dependency, so it was off by default, could not run in a fixed arena,
and its places came from someone else's events. Can YAML be read here
too, to the same standard?

**Decision.** A YAML 1.2 parser in `src/serialize/yamlparse.c`, by
recursive descent as the specification is written: block structure by
indentation, flow structure by brackets, a node taken as a key when
": " follows it on its line. It hands its events to the same mapping
the libfyaml and libyaml fronts feed (`yaml.c`), so anchors, the core
schema's tags and the refusals of what a configuration has no place for
are one implementation, whichever parser reads. It allocates nothing of
its own: a scalar is decoded into one buffer the length of the document,
which the sink lends, and its recursion is bounded by the nesting limit.

**Held to.** yaml-test-suite twice over. Apart from the library, its
events against the suite's own (`test.event`): all 308 valid cases event
for event, all 94 invalid ones refused. Through the library, the same
floors as libfyaml: 122 of 122 valid documents read, 94 of 94 invalid
refused, 110 of 110 read to the suite's values. Fuzzed with the others.

**What changes.** `CF_WITH_YAML` is on by default and `CF_YAML_BACKEND`
is `auto`, which takes the built-in parser; `libfyaml` and `libyaml`
remain for a build that wants a dependency's reading, and `builtin`
names this one. A default build still links nothing but the C library.

---

## D-70 — The Rust crate builds with cargo and the cc crate

**Question.** D-44 had the crate's `build.rs` run CMake, so that the
build downloaded nothing. But a Rust crate that needs CMake is not built
the way Rust users build crates: cargo should be enough. Does the
no-download rule outweigh that?

**Decision.** No. `build.rs` compiles the library's C sources with the
`cc` crate - Rust's own way to build C, which finds the C compiler, MSVC
included, and picks the C runtime Rust links - and links them
statically. Every source file guards itself, by its option and its
platform, so all of them are compiled, as the amalgamation does, with
`cf_build_options.h`'s defaults, which `sys.rs`'s struct layouts follow.
CMake is no longer needed to build the crate.

**What it costs.** One build dependency, `cc`, with its own two
(`find-msvc-tools`, `shlex`), from crates.io, pinned with their
checksums in `Cargo.lock`: cargo runs with `--locked`, and offline once
they are in its cache. Nothing is added to what a program using the
crate links.

---

## D-68 — A format of one's own, registered on the builder

**Question.** A caller's `cf_codec` reaches one file through
`cf_builder_file_codec` and nothing else: its extension does not choose
it, a named format does not find it, a drop-in directory does not read
it, and `cf_emit` cannot write it. Should it go in the codec table, and
how does its author know it reads its format correctly?

**Decision.** Registered on the builder, `cf_builder_codec(b, codec)`,
not in a table for the whole process. A process-wide table that code
adds to at run time is global mutable state, which this library has
none of, and two parts of one program registering one name would clash
where nobody could see it; on the builder, the clash is refused when it
is made. A registered codec comes ahead of the built-in ones, for the
files and directories added after it, by extension or by the name of a
format given; the configuration keeps the list, so `cf_emit` and
`cf_config_codec` find it, and a derived configuration reads with the
same codecs. Up to `CF_BUILDER_CODECS` (8).

**Measured as the built-in ones are.** `conform --codec <library>
<symbol>` reads the suite's documents through a codec a shared library
exports, and `conformance.py --codec` scores it on its format's suite
against that format's floors. A shared build's tests load a module of
two codecs, one reading through the library's JSON reader. A static
library is not built position-independent, so the module is built where
the library is shared, which is also how a program and its plugin share
one copy of it.

---

## D-67 — Doctor reads files against a schema, and writes the schema as C

**Question.** Since D-64 a JSON Schema can be the contract a program
reads. Can a file be checked against it without the program - in CI,
in a pre-commit hook - and can a build without the JSON reader have the
same contract without writing it a second time?

**Decision.** Two Doctor commands, both from the library's own reading
of the schema, so neither has rules of its own. `doctor check --schema S
F...` builds a configuration from the files as a program would, unknown
keys refused outside a prefix the schema opens, and reports the first
refusal (exit 1) or `ok` (exit 0); a schema it cannot read is exit 2.
`doctor gen --schema S` writes one C89 function that defines every key
with `cf_def` and `cf_builder_define` and opens the schema's prefixes,
so an embedded build has the contract from the same file. A hook,
`configforge-check`, runs the first on each staged file.

**Held to.** The generated function and the schema write the same JSON
Schema back through `cf_schema`, and refuse the same files; the test
compiles the function with the project's warnings, and fails when the
generator drops a description. The checks name the key and the reason.

**Two choices.** A required key is required of the files checked: a
required key may come from the environment, and a check of files alone
cannot know it will, so the files to check are the ones that together
make a configuration. And only the first refusal is reported, as the
builder reports it, rather than a second engine collecting all of them.

---

## D-66 — JSON5, and with it JSONC, a mode of the JSON reader

**Question.** Which other formats are worth reading for configuration,
without overdoing it? JSON with comments and trailing commas is what
`tsconfig.json`, VS Code's settings and Deno's configuration are
written in. Is it a format of its own, and how is it read?

**Decision.** JSON5 (json5.org, 1.0.0), a format with a specification
and an official suite, read as a mode of the JSON reader rather than by
a second one: one scanner, a flag, the grammar's additions where JSON's
would refuse - comments, trailing commas, identifier and single-quoted
names and strings, `\v`, `\0`, `\xHH`, any other character escaping
itself, line continuations, hexadecimal and signed numbers, a leading
or trailing point, Infinity and NaN, and ECMAScript's white space and
line terminators past ASCII. A JSONC file is JSON5, so `.json5` and
`.jsonc` both choose it; `.json` stays JSON, and nothing JSON refused
before is read now. Written, a configuration is JSON, which every JSON5
reader reads.

**What it is held to.** json5-tests, pinned by commit, with the same
floors as the others: 83 of 83 valid documents read, 31 of 31 invalid
refused, and every valid one read to the values the `json5` package
(PyPI, pinned in `setup.sh`) reads. Fuzzed with the other readers.

**Two choices.** Infinity and NaN are their text, typed by the
definition, as TOML's inf and nan are (D-62): the library holds no value
that is not finite, so a key defined as real refuses one by name. And a
name's characters past ASCII are ECMAScript's identifier characters
taken as "not white space", since its letter and mark categories would
need Unicode's tables in the library; ASCII is exact.

**Weighed and not taken.** HCL (a language, with expressions), HOCON
(the JVM's), plist (with native OS sources), KDL, RON, CUE, Dhall and
Pkl (young or narrow). With JSON5 the format set is enough.

---

## D-65 — CI and the development environment on Ubuntu 26.04 LTS, together

**Question.** The environment (`setup.sh`, the Containerfile, the WSL
distribution) and CI ran Ubuntu 24.04, whose libfyaml (0.8) hangs on
malformed input. Move to 26.04, and how?

**Decision.** Both at once - an environment that is not CI's is one where
"it passed here" says nothing - after a survey of every package in an
`ubuntu:26.04` container, and a tier 2 run, from nothing, on a new WSL
distribution made beside the old one. CI names `ubuntu-26.04` in every
job, because `ubuntu-latest` still means 24.04 and would move under the
project on GitHub's schedule rather than its own.

**What changed with it.** The compiler runtime follows clang's version
(21 on 26.04). libfyaml is the archive's 0.9.4, so the script that built
0.9.6 from a tarball is gone, and the floor of 0.9.1 the build enforces
is the only place a version is written; verify asks the build rather
than looking for a file. libkqueue left the archive in 26.04, so
`EXTRA=kqueue` is skipped on it, and CI's kqueue job stays on 24.04 -
which also keeps the previous LTS, and GCC 13, in the matrix. The
release's Linux build is static musl in Alpine, so the runner's glibc
never reaches a user.

**What it found.** New compilers are new analysers: GCC 15's, cppcheck
2.19 and clang-tidy 21 found six things the old ones did not, among them
a write before a buffer in an example and an index that a negative count
made overflow. Each was fixed in the code (the commit lists them); none
was silenced.

---

## D-64 — Definitions read from a JSON Schema, the reverse of `cf_schema`

**Question.** A program whose contract already exists as a JSON Schema
(for an editor, or a service that validates the same file) writes it a
second time as `cf_builder_*` calls, and the two drift. Read the schema?

**Decision.** `cf_defset_schema` and `cf_builder_schema(_file)` read a
JSON Schema (draft 2020-12 or 2019-09) as definitions, the inverse of
`cf_schema_write`: what that writes reads back as the same definitions,
and writes out again byte for byte (test_schema's round trip). The
keywords that map are the ones the writer writes, and a few with an
exact meaning here: `required` (`CF_REQUIRED`), an integer's
`exclusiveMinimum`/`exclusiveMaximum` (one in), `title` (a description
when there is none). Annotations - `$comment`, `examples`, `format`,
`readOnly` - are read and change nothing, as a schema reader would
treat them.

**Everything else is refused by name and place**, never dropped:
`$ref` and `$defs`, the combinators, `pattern`, `const`, a type that is
a list, a number's exclusive bounds, a list of lists or of tables, an
older draft. A constraint the program believes it declared and the
library silently does not enforce is the worst outcome a contract can
have. The writer was changed to match: in a closed schema an opened
prefix now says `"additionalProperties": true` (JSON Schema's default,
so the meaning is the same), which is what the reader opens.

**How.** The schema is read by this library's own JSON reader (held to
RFC 8259, D-62) into a small tree in the definitions' arena, then
walked. In the bridge layer, since it joins a reader and the
definitions; a build without JSON gets `CF_E_CAPABILITY`.

---

## D-63 — YAML's anchors, aliases and core tags are read

**Question.** D-39 refused anchors and aliases, because "a value used
twice has no single origin to explain", and every tag, because "a
value's type is its definition's to decide". Now each format follows its
own standard. Do those reasons hold?

**Anchors and aliases.** They do not. An aliased value is written once:
its origin is the anchor's line, and explain() can name it exactly. What
D-39 feared is a value under two keys, which a configuration has anyway
whenever two keys are set to the same thing. So the reader records an
anchored node's events in the sink's workspace (`reserve_work`, D-62)
and an alias replays them, every value with the place it was written.
An alias of an alias is expanded once; an alias inside its own anchor is
refused; and expansion is bounded at 64 events per event read (with a
floor), so a "billion laughs" is refused in linear time, by name. A sink
that lends no workspace refuses an anchor, saying so.

Events alone were not enough, and 1.5.0 adds the second bound: the bytes
an expansion copies are held to 64 times the document plus 1 MiB, with
the same error. One large scalar aliased thousands of times is few
events and gigabytes of copy, so a megabyte of input could still be made
to cost more memory than any limit on events would show. A name anchored
again takes over its entry rather than lengthening the lookup chain.

**Tags.** The core schema's - `!!str`, `!!int`, `!!float`, `!!bool`,
`!!null`, `!!map`, `!!seq` - say what YAML 1.2 says the value is, and
honouring them is reading YAML; a tagged value that is not its type is
refused. `!!str` is quoted text, exactly as `"..."` is, so a definition
still decides as D-39 has it. The non-specific `!` is text. Any other tag
names a type of the writing application's, which a configuration cannot
honour, and is refused by name.

**Still refused, as the configuration model has it:** a top level that
is not a mapping, a second document, a key that is not a scalar, an
empty key. The conformance run counts these apart from the grammar's.

---

## D-76 — Predict sizes and measure the result

Use the reported file size to reserve the input buffer and estimate the key
space from the input or the previous generation. Measure each proposed
optimisation separately. The [roadmap](Roadmap.md) records which estimates
were kept and which were left after measurement.

## D-77 — A reference's expansion is bounded, by a constant and not a build limit

**Question.** `${}` expansion (D-10) had no bound: two dozen lines, each
naming the one before it twice, make gigabytes, and on a 32-bit `size_t`
the measured size could wrap, so the copy overran the heap. What bounds
it, and where is the bound written?

**Options.**

1. No bound: the defect.
2. A multiple of the document's size, as YAML's aliases have (D-63).
3. An entry in `cf_build_limit` (D-45).
4. A fixed constant beside the other limits.

**Decision.** Option 4: `CF_EXPAND_MAX`, 1 MiB, in `cf_define.h`. A value
that expands past it is refused by name. Option 2 was rejected because
references draw on every source, so there is no single document to take a
multiple of. Option 3 was rejected because those limits size stack
buffers and are checked by the gates and the bindings; this one sizes no
buffer, and a build option would put a number into the ABI that nothing
needs to know. 1 MiB is far above any value a configuration holds, and a
program that needs more has a value that is not configuration.

All the expansions of one configuration together are bounded too, by
`CF_EXPAND_TOTAL_MAX`, 16 times `CF_EXPAND_MAX`, and the key at which the
total is passed is refused by name. A bound on each key alone was not
enough: twenty doubling lines make a value just under 1 MiB, and ten
thousand short lines each naming it make 10 GiB with no key over the
limit; the total is a multiple of the per-key bound, not of a document,
for the reason option 2 was rejected.

**Found on the way.** A reference to a key whose own expansion was
refused, with nothing to fall back to, was read through NULL, a crash. It
is refused instead, with the same error.

**Compatibility.** A configuration with a value that expanded to more than
1 MiB, or with expansions adding up to more than 16 MiB, is now refused.
None is known.

---

## D-78 — A directory that cannot be read whole is an error, optional or not

**Question.** A drop-in or secrets directory was listed up to 256 files;
past that the listing stopped at whatever the disk handed over first, so
a `99-override.ini` could vanish with no word. A FIFO in it hung
`cf_create`, a reload and the kqueue watcher. What does a directory that
cannot be read whole do?

**Options.**

1. Read what fits and carry on, as it was.
2. Read what fits and log the rest (D-52).
3. An error, and `CF_OPTIONAL` excuses it as it excuses a missing
   directory.
4. An error that `CF_OPTIONAL` does not excuse.

**Decision.** Option 4. `cf_platform_dir_list` returns -2 for more files
than the limit, a name too long to carry, or an entry that is not a file,
and the error names the directory and the limit, or the entry. Options 1
and 2 give a configuration that differs from the files on disk, in the
one case, the override, where the difference matters most. Option 3 was
rejected because `CF_OPTIONAL` means "the directory may be absent", not
"it may be half read".

Subdirectories are skipped on every platform, as Windows already did.
Anything else that is not a file - a FIFO, a socket, a link to nothing -
fails by name. Links are followed, because a Kubernetes mount is made of
links, and refusing them would break the commonest place these
directories are used.

**Compatibility.** A directory with more than 256 files, which read
partly before, is an error. That is the change, and the reason for it.

---

## D-79 — Response files (`@file`) are opt-in

**Question.** `cf_app` and `cf_app_builder` passed the command line through
`cf_cli_expand`, so any argument beginning with `@` was opened as a file of
options. A program that takes a positional argument from outside - a name,
a path, a user's text - therefore let that argument inject configuration
and, by whether an error came back, tell which files exist. Should
expansion be on by default?

**Options.**

1. Keep the default. Rejected: an untrusted argument injects configuration
   and tells which files exist, in a library whose point is that the
   command line is the highest-ranked source.
2. Remove the feature. Rejected: programs use it, and the Windows
   command-line length limit it works around is real.
3. Opt-in, one call on the builder.

**Decision.** Option 3. `cf_app` and `cf_app_builder` leave an argument
that starts with `@` alone: the command-line source treats it as any
positional - it skips it, and no file is opened. `cf_builder_response_files`
(1.5) turns expansion on for a builder made by `cf_app_builder`. The
builder keeps the command line it was given, after the UTF-8 step, and which
of its sources reads it; the call runs `cf_cli_expand` over it and gives that
source the expanded vector. A response file that cannot be read fails the
builder, as it did, and `cf_builder_create` reports it. Calling it twice
changes nothing; on a builder with no command line (from `cf_build`) it
returns false and fails nothing. `cf_app` itself never expands: a program
that wants the feature starts from `cf_app_builder`. C++ has `responseFiles()`,
Python `response_files()` and Rust `response_files()`. Nothing existing
changes signature, and the symbol list only gains a line.

**Compatibility.** A change of default inside 1.x, made on purpose: a
program that relied on `@file` through `cf_app` or `cf_app_builder` now gets
the argument as a positional, and its options are not read. It must use
`cf_app_builder`, call `cf_builder_response_files`, and then
`cf_builder_create`. Nothing in the repository's examples or in Doctor
relied on it.

**Held to.** `test_lex` runs twice with a real `@` argument (the library
reads the process's own command line on Windows, so an invented argv would
be ignored there): with the file present, the default leaves its key unset
and the opt-in sets it; with the file absent, the default fails nothing and
the opt-in fails the builder with an error naming it. The same two runs in
`test_wrapper_write` (C++98), a child process in the Python and Rust tests,
and a builder with no command line returns false and fails nothing in all
four.

---

## D-80 — systemd's credentials, a builder of their own

**Question.** systemd hands a service its credentials (`LoadCredential=`,
`SetCredential=`, `ImportCredential=`) as files in the directory
`$CREDENTIALS_DIRECTORY` names, one file a credential, which is exactly
what the secrets directory reads. A program could pass `getenv`'s answer
to `cf_builder_secrets` already. Is a source of its own worth an entry in
the interface, and what does it do where the variable is not set?

**Options.**

1. Nothing new: the program reads the variable and names the directory.
2. A flag on `cf_builder_secrets` that makes `path` a variable's name.
3. A builder of its own, `cf_builder_credentials(b, name, rank, prefix,
   flags)`, over the secrets directory's reading.

**Decision.** Option 3. With option 1 every value says it came from
`/run/credentials/app.service/db.password` and nothing says systemd put
it there; a report read on another machine cannot tell a credential from
a file somebody dropped into a directory, and saying where a value came
from is this library's one promise. Option 2 makes one argument mean a
path or a variable by a bit beside it, and still needs the provenance to
change with the bit. So the reading is the secrets directory's - one code
path in `secrets.c`, names, the trailing newline, D-78's limits and
CF_SENSITIVE left to the contract all unchanged - under a vtable of its
own, kind `credentials`, with each value placed as `systemd credential
<id> (<file>)`. The variable is looked up at every load, as
`cf_builder_env_document` looks up its own, so a reload sees what it
holds then.

**Unset.** An unset or empty variable is an error that names it; with
`CF_OPTIONAL` the source reads nothing, as a missing optional file does,
so one program runs under systemd and from a shell alike. A variable that
is set must name a directory that can be read whole, `CF_OPTIONAL` or
not: systemd creates the directory before it starts the service, so one
that is not there is a mistake, and optional means "maybe not run with
credentials", not "credentials maybe lost".

**No `#if` for the operating system.** The variable is the protocol, and
it can be set anywhere - by a test, by a container's entry point, by a
supervisor that copies systemd's convention. Reading it through the
platform's environment (`cf_platform_environ`) works on every platform,
and gating the source to Linux would make one program's configuration
depend on where it was compiled for no gain. The one option that does
apply is the secrets directory's own: under `CF_WITH_DIR`, refused by
name without it (D-38).

**Compatibility.** Additive: one function, one line in the symbol list.

---

## D-81 — A reader's name table is seeded a decode

**Question.** The TOML, YAML, INI and XML readers keep small hash tables
of names taken from the document being read (what TOML has defined, YAML's
anchors, the arrays an INI section has had, the names at one XML level).
They hashed with FNV-1a from its fixed offset basis, which anyone can
compute against, so whoever writes the document can choose names that all
land in one bucket and make reading take quadratic time. How is the hash
to differ from one decode to the next?

**Options.**

1. No seed. Rejected: it is the defect; chosen names collide.
2. A bound on the chain length that falls back to a scan. Rejected: a
   lookup is still linear in the worst case, and a document can reach it.
3. A random source from the platform layer. Rejected: a reader of bytes
   would then need the operating system, and the core has none.
4. The address of the table and of a local variable, mixed.

**Decision.** Option 4. A seed is computed once when a decode's table is
set up and stored in it; every hash of a name in that decode starts from it
in place of the offset basis, so inserts and lookups agree and behaviour
is unchanged. It is the two addresses with the second multiplied (both are
often on the stack and would cancel), folded so the random high bits reach
the low bits a bucket index uses, multiplied by the FNV prime and folded to
the hash's width; 0 becomes the offset basis. It is free, differs from run
to run where the system randomises addresses, and where it does not (no
address randomisation) it is the same every run - no worse than before,
and the comment says so. It is a defence in depth against a chosen
document, not a guarantee: an attacker who learns the addresses learns the
seed.

Nothing observable depends on the seed: none of the four tables is walked
in bucket order; each is only asked for a name, and keys, errors, which
duplicate is reported and the order of output come from the document.

**Compatibility.** None; no signature, output or message changes.

**Held to.** Each reader's test reads one document twice through two
arenas at different addresses and compares keys, values, places and order;
a TOML document of 20,000 keys builds. With the seed helper returning a
constant the readers' tests pass; with it returning a different value each
call they fail, so every call site uses the stored seed.

---

## D-82 — What one document may make is bounded, in the flattener

**Question.** A document can cost a multiple of itself: aliases that
replay a block, separators that split into elements, empty arrays that
each leave a key. YAML bounded its aliases (D-63); nothing else bounded
anything, so a small JSON, TOML, INI, XML or `.properties` document, and
every program's own codec, could make far more than its size of keys and
values. Where is the bound?

**Options.**

1. A bound in each reader. Rejected: every new reader would need its own,
   and a program's codec would have none.
2. A bound on the arena's bytes. Rejected: the key space's arena holds
   other sources' keys and the reader's work space, so the count would
   depend on what was read before.
3. One bound in the flattener, where every document passes.
4. None.

**Decision.** Option 3. As the flattener adds each key it counts the key's
length, its text's length and 64 bytes for the entry and candidate that
hold it; a document may make 64 times its own size and 1 MiB more
(`CF_DECODE_FACTOR`, `CF_DECODE_FLOOR`, `CF_DECODE_KEY_COST`), and past it
the document is refused as `CF_E_SOURCE`, by a message that names the
limit, whatever its format. 64 is YAML's rule for its aliases, made
everyone's; the floor lets a small document reuse a block freely; the key
cost makes the one count bound keys as well as bytes, since a key cannot
cost less than it. These are constants and not build options: they size no
buffer and no frame. YAML's own limits stay: they count events, which bound
time where no key is made, and they name the line and column of the alias;
the flattener's bound is the floor under every reader, YAML included, and
where YAML's trips first that is the right message. The environment, the
command line, a `.env` file and the Registry are not under it: none nests
or repeats, so none can make more than a few times its size.

**Compatibility.** A document making more than 64 times its size and 1 MiB
is refused; none is known. `cf_flatten` is internal (D-51) and gains three
fields, and three functions are added: `cf_decode_budget`,
`cf_flatten_budget` and `cf_flatten_decode`.

**Held to.** `test_budget` reads a small document of each format under a
small budget and sees it refused by name, and a YAML document of thirty
aliases of a 2000-key block refused; `test_yaml` takes either byte bound
for the large anchor.

---

## D-83 — A directory others can write to is not read as secrets

**Question.** A value from `/run/secrets/db.password` explains itself as
that file. If a user other than the directory's owner and root could
have put the file there, or changed it, the explanation names the wrong
author: the configuration says "the secrets directory" and means
"whoever could write to it". Should a secrets directory be checked for
who may write to it, how strictly, and what of a drop-in directory?

**Options.**

1. Nothing: a directory is read whoever can write to it, as before.
   Rejected: it is the defect.
2. sudo's rule: refuse a directory, or a file in it, that its group or
   others can write, and an owner other than root or the program's user.
   Rejected: on Ubuntu and most Linux systems a user's umask is 002 and
   each user has a group of their own, so every directory a developer
   makes would be refused; and Docker Compose bind-mounts a developer's
   file into a container that runs as root, which an owner rule would
   refuse for an owner nobody else can impersonate.
3. The rule decided below, checked in the listing's own pass.
4. The same rule checked by a second pass after the listing. Rejected: a
   file changed between the two would be read unchecked.

**Decision.** Option 3. `cf_platform_dir_list_trusted` lists the
directory and, in the same pass that already stats every entry, holds it
to three rules:

1. The directory is refused when it is not sticky and others may write
   to it, or its group may and that group is not the private group of
   the user who owns it - the group of that user's entry in the password
   database (`getpwuid_r`).
2. In a sticky directory they may write to - the one kind rule 1 lets
   through - each entry as `lstat` sees it (the link, for a link) must
   belong to root, to the directory's owner or to the user the program
   runs as: there nobody can replace another's entry, so an entry of one
   of those is theirs.
3. Each file as `stat` sees it - what a link names - is refused when
   others may write to it, or its group may and the group is not its
   owner's own.

The answer on groups is the maintainer's (2026-11-12): a group may
write only when it is the private group of the user the directory
belongs to, so what umask 002 makes is read, and a directory a team's
group shares is not. Links are followed, as D-78 decided, and the
directories on the way to a link's target are not checked. Kubernetes
mounts a secret as a sticky, world-writable directory of root's (mode
1777) holding root's links to root's files (0644): rule 2 lets it
through and a file another user drops in is caught. With a pod's
`fsGroup` the same directory belongs to that group and the group may
write (mode 3777): in a sticky directory a group's write is no more
than others' already is, so it too is rule 2's to judge and not rule
1's to refuse. Docker's
`/run/secrets` (0755, files 0444) and systemd's credentials directory
(the service user's, 0500) pass as they are. A password-database entry
too long for the buffer (512 bytes) is treated as not the user's own
group: the directory is refused, and `CF_ANY_WRITER` reads it. A refusal is `CF_E_SOURCE`,
names the directory or the file, its mode or its owner, and is not
excused by `CF_OPTIONAL`, which is for a directory that is not there.

`cf_builder_secrets` and `cf_builder_credentials` hold to the rule by
default; `CF_ANY_WRITER` reads such a directory all the same, the
program saying who may write to it is known and accepted. A drop-in
directory (`cf_builder_dir`) is held to it only with `CF_TRUSTED_DIR`,
and the two flags together fail the builder. Every other source refuses
both flags as it refuses any flag it does not know.

**Windows.** Who may write there is an access control list, not a mode,
and an answer read from one by a rule written for modes would be wrong
both ways, so the platform does not say: `cf_platform_has_trusted_dir`
is false there and in a build with no operating system, and the trusted
listing is the plain one. A secrets directory is read unchecked, as
before - asked for nothing, it is not refused for what the platform
cannot say - and a drop-in directory that asks `CF_TRUSTED_DIR` is
refused as `CF_E_CAPABILITY`.

**Compatibility.** A secrets or credentials directory others can write
to, or holding a file they can, is refused where it was read;
`CF_ANY_WRITER` reads it as before. A drop-in directory is unchanged
unless `CF_TRUSTED_DIR`. Two functions and two flags are added.

**Held to.** `test_trusted` reads a secrets directory of each shape -
0755 with a 0644 file, 0777 with and without `CF_ANY_WRITER`, 01777 of
the program's own file, a 0666 file, 0775 of the owner's own group - a
0777 drop-in directory with and without `CF_TRUSTED_DIR`, and, run as
root, a stranger's file in a sticky directory; on Windows, a drop-in
directory asking `CF_TRUSTED_DIR` is refused for what the platform
cannot say. The whole suite passes under umask 002.

---

## D-84 — An undefined key named like a secret is kept as one

**Question.** Nothing can know that `db.pasword` is a secret: nobody
defined it. It is the commonest way a secret is written where nothing
knows it is one, and every output then shows it in clear - the blind
report, `explain()`, a diff, a malformed reference's reason. What should
a report show of an undefined key that may be a secret?

**Options.**

1. Nothing changes; the gap is written in Safety.md as the one place a
   secret is shown.
2. A key with no definition whose nearest defined key, at the distance
   "did you mean" uses (`cf_defset_nearest`: a third of the key, 1 to 4
   edits), is `CF_SENSITIVE` is treated as that secret. A plain key one
   edit from a secret's name is hidden too.
3. Every undefined key's value is hidden whenever the contract has any
   sensitive key. Rejected: a typo of a plain key loses its value from
   every report, which is where a person looks to see what was mistyped.

**Decision.** Option 2 (the maintainer's answer, 2026-11-12). Its value
is not shown in a blind report, `explain()`, a diff, a refusal's reason
or Doctor's output, and the fingerprint takes its digest. `cf_defs_apply`
decides once, in its branch for an entry with no definition, and marks
every candidate `CF_CAND_SECRET_LIKE`; a key under an open prefix is a
plugin's, not a typo, and is never marked. `cfi_defset_nearest_secret`
compares only the secrets, so an undefined key costs as many comparisons
as there are secrets. `cfi_entry_secret` is the one predicate `explain`,
`diff`, the fingerprint and the report read. A report writes
`"like": "<the secret's key>"` beside `"definition": null` - a key name,
not a value; schema 1 holds, as an added field. Doctor reads it: such a
key is sensitive there, so its output hides the value and compares by
digest. An unknown key's refusal under `CF_UNKNOWN_REJECT` names keys
only and is unchanged.

**Compatibility.** A blind report, an explanation and a diff of such a
key show a digest or nothing where they showed its value; a fingerprint
of a configuration holding one changes once.

**Held to.** `test_secretlike` (report, explanation, diff, fingerprint,
a malformed reference, an open prefix), a scenario row in `test_marker`,
and `doctor.secrets` on a blind report of the same.

---

## D-85 — A secret's digest, keyed

**Question.** A blind report, a full one and the fingerprint say "this
secret changed" with a digest, and the digest is an unsalted FNV-1a. Over
a four-digit PIN it is a lookup: all ten thousand guesses are tried in a
millisecond against a digest read from a report or a log line. Can a
digest still tell that a secret changed without being testable against
guesses?

**Options.**

1. A key the program gives its builder (`cf_builder_digest_key`); without
   one, FNV-1a as before. Nothing changes for a program that does
   nothing; reports made under one key still compare; the gap stays open
   for a program that gives none, and the Guide says so.
2. A key made on first use and kept in a file per host. Every digest and
   fingerprint changes on upgrade, two hosts' reports no longer compare,
   and the library would write a file it never wrote. Left for 2.0, when
   fingerprints may change once.
3. A key from an environment variable. No code change for a deployment
   that sets it, but the key then sits in the environment the library's
   own documents tell programs to keep secrets out of.
4. A slow unkeyed hash. Rejected: a PIN is still ten thousand tries.

**Decision.** Option 1 (the maintainer's answer, 2026-11-12): the key comes
only from the program, and FNV-1a stays the default. The function is
SipHash-2-4 (Aumasson and Bernstein, 2012): a keyed function made for
short inputs, 64 bits as the digest was, and a few dozen lines of C89 in
`keyspace.c` beside `fnv1a`. A keyed digest is written
`sip24:<id>:<16 hex digits>`, 31 characters, where the id is the top 32
bits of SipHash-2-4 of `configforge digest key id` under the key, in
eight hex digits: it tells two keys apart and, being the output of a
keyed function, says nothing that helps find the key. `n` must be 16;
anything else fails the builder with `CF_E_POLICY`. The key is copied
into the configuration's arena, so it is wiped with it, kept in
`cf_defset`, and a derived configuration keeps it. One function,
`cf_ks_secret_digest_text`, makes a secret's digest text for every
caller: a report (`"digest"` stays a string) and the fingerprint
(`cf_defset_fingerprint`). A full report now carries a secret's `"digest"`
beside its `"value"`, an added field, so that Doctor never has to make a
digest it has no key for. The fingerprint keeps its form, `fnv1a:` and 16
hex digits, over the selected values with a secret entering by its
digest's text: keyed, it differs from an unkeyed one and is the same for
every configuration of one deployment under one key.

**Compatibility.** Without a key nothing changes. With one, a secret's
digest and the fingerprint change once, report schema stays 1, and a
Doctor from before reads a keyed digest as an opaque string: two keyed
alike compare correctly, a keyed and an unkeyed one read as "changed",
the cautious answer (H-8b teaches Doctor to say "cannot be compared").
A full report holds a `"digest"` where it held none, which also lets
Doctor compare a secret in a full report with the same secret in a blind
one.

**Held to.** Doctor compares two digests only when their kinds - the text
up to the last `:`, `fnv1a` or `sip24:<id>` - are equal; otherwise `diff`,
`history` and `blame` report CFD020, "whether it changed cannot be
told", naming the two kinds and neither digest. `doctor.keyed` builds
blind reports of one secret under key A (two values), key B and no key,
and checks that A against A is CFD014, that A against B and either
against the unkeyed one is CFD020 and never CFD014, and that a keyed
report's lint raises no CFD019.

**Held to.** `test_digestkey` (the reference vectors, the form, one key
and another, no key, the fingerprint, a derived configuration, a full
report, the refusal), and a second pass of `test_marker` with a key given.

---

## D-86 — Doctor's action shows no value unless asked

**Question.** A full report holds the value of every key, and the GitHub
Action posts what Doctor says to a pull request, its job summary and its
log. Anyone who can read the pull request then reads the values: a URL
with a token in it, an internal host name. A secret's was never shown;
should a plain key's be?

**Options.**

1. As it was: plain values are shown everywhere.
2. Redact every value always. A developer on their own machine loses the
   one thing that makes `explain` and `diff` quick to read.
3. A flag, `--redact-all`, and the action passes it unless asked not to.
4. A flag, off everywhere, the action included. Safe only for a workflow
   that knows to set it.

**Decision.** Option 3. `dump`, `explain`, `lint`, `diff`, `history` and
`blame` take `--redact-all`, and `cf_doctor_redact_all` is the library's
side: no value of any key is shown, as if each were a secret. A finding
still says that a value changed, was refused or came from where; a plain
key's reason is replaced by its kind ("range", "type"), since the
library's reason can quote the value. Comparisons still use the values a
full report carries: a key is not made sensitive by the flag, so CFD019
and CFD007 say what they said, and secrets are still compared by digest.
The action's new input `show-values` (default `false`) decides whether
it passes the flag. The pre-commit hook is not changed: it runs on the
developer's own machine and posts nothing.

**Compatibility.** An existing workflow's comments, summaries and logs
lose their plain values on upgrade; `show-values: true` brings them
back. Doctor on the command line and the library behave as before
unless asked.

## D-87 — macOS's user defaults, an optional source over the flattener

**Question.** A macOS program keeps its settings in the user defaults
(CFPreferences), written by `defaults write <domain>`, and a person
looking for where a value came from looks at `defaults <domain> <key>`.
How does ConfigForge read them so that explain() names that place, and
which of CFPreferences' scopes does it read?

**Options.**

1. No source: the program copies the values into a file or the
   environment. The origin is lost.
2. The Registry's shape without the flattener: a source that adds each
   key itself, repeating the dictionary and array handling every codec
   shares, and in time disagreeing with it.
3. The platform walk delivers document events (dictionary, array, value,
   end) and the shared flattener makes the keys, as for a JSON file.
4. Reading the plist files under `~/Library/Preferences`. `cfprefsd`
   caches and writes them late, they are often binary, and managed
   profiles live elsewhere: the file is not the setting.

CFPreferences keeps a domain's values in up to four scopes: the current
user on any host, the current user on this host, any user, and a managed
profile. Read (a) the first only; (b) the merged view
`CFPreferencesCopyAppValue` gives, whose precedence is CFPreferences' and
not the program's ranks, and which still misses keys only a managed
profile sets; or (c) (a) now, and later a scope prefix in `domain`
(`"currentHost:..."`, `"anyUser:..."`, `"managed:..."`), each scope its
own source at its own rank.

**Decision.** Option 3, over the platform walk of W3-F2a, built only
under `CF_WITH_PREFERENCES`, off by default everywhere, macOS included:
on, every static consumer on macOS must link `-framework
CoreFoundation`, which a 1.x upgrade must not impose. Scope (c), the
maintainer's answer of 2026-11-16: the current user on any host is read
now, what `defaults read <domain>` shows; the prefixes are a later card,
so precedence between scopes stays the program's and explain() shows it.
Names are lowered, and a name that is still not a key is an error naming
it. A dictionary is a section and an array a list; a string is text the
contract types (`defaults write` writes strings unless told `-int`), a
number an int or a real, a boolean a bool, and a date or data is refused
by name. The place is "defaults <domain> <path>", names joined by '/'
and an array's member by its index. A domain with no values is an error,
and nothing with `CF_OPTIONAL`; off macOS, or without the option, it
fails by name even when optional, since reading nothing would look like
an empty domain. Not watched: `cf_changed` does not see a change, as for
the Registry.

**Compatibility.** Additive: one symbol, `cf_builder_preferences`.
Nothing changes unless the option is on.

**Held to.** CI's `macos-preferences` job builds with
`CF_WITH_PREFERENCES=ON` on macOS and runs the whole suite. Its
`test_preferences` imports a plist into a test domain with `defaults
import`, reads it back through `cf_builder_preferences` and deletes it:
a dictionary member is a section's key (`Server/Port` is `server.port`,
9443), an array a list, a boolean a bool, an integer past 32 bits an
int, a real a real, and explain() names `defaults <domain> Server/Port`.
A date is refused, a name no key can carry is refused with `CF_E_KEY`, a
domain with no values is an error unless `CF_OPTIONAL`. Every other job
runs the same test's other half: the source fails by name, optional or
not. On macOS `posix.c` alone is compiled as C99 when the option is on,
since CoreFoundation's headers are not C89, and the framework is linked
publicly and named in pkg-config.

## D-88 — What 1.x promises, made smaller: the internal tier fenced

**Question.** D-51's internal tier was a sentence in each header: 87 of
the 369 exported functions are not promised, yet a program that called
one (`cf_ks_resolve`, say) compiled, linked, and would break in a later
minor release. How is the edge of the promise made something a compiler
sees?

**Options.**

1. Leave it as it was.
2. Headers that are not installed. Doctor is built against the
   installed package, the internal functions share headers with promised
   ones, and the amalgamation is one header.
3. A macro a program does not define, `CF_INTERNAL_API`.
4. Hide the symbols. Doctor links the shared library, and renaming them
   (`cfi_`) is a removal.

**Decision.** Option 3. `cf_port.h` defines `CF_INTERNAL_API` to 0 after
`CF_API`, so `#if CF_INTERNAL_API` passes `-Wundef`; only functions are
fenced, never types or macros, so each header still compiles alone. It
is 1 for the library, Doctor, the tests, the benchmark, the CBMC proofs,
the amalgamation's build and the Rust crate; not for the examples,
`tests/consumer` or a program. Fenced: the key space's `cf_ks_*` but
`cf_ks_arena` and `cf_ks_add*`; `cf_flatten.h`; `cf_numeric.h`;
`cf_platform.h` but `cf_platform_name` and the `cf_platform_has_*`
queries; the definition set's `cf_defset_*` and `cf_def_*`;
`cf_ks_explain`, `cf_ks_inspect` and their redacted forms; `cf_ref_*`,
`cf_cmp_*` and `cf_siphash24`. The maintainer's answer (2026-11-16):
`cf_fmt`, `cf_vfmt`, `cf_fmt_append`, `cf_platform_name` and the eight
`cf_platform_has_*` queries, twelve functions the examples call, join the
promise.
`gate.abi` keeps two lists, the promised one and
`cmake/configforge.internal.symbols` ("no version at all"), and fails a
name in both; `gate.headers` compiles each header again with the fence
open. Held to by `unit.internal_fenced` (`lib/config/tests/fenced.c` must not
compile, naming `cf_ks_count`) and the same check in `gate.install`
against the installed headers.

**Compatibility.** A program that called an internal function no longer
compiles; it links and runs as before, since nothing exported changed.
It may define `CF_INTERNAL_API` to 1 and keep those calls, outside the
1.x promise.

## D-89 — The experimental tier

**Question.** With the internal tier fenced (D-88), every exported
function a program can call is promised for the rest of 1.x from the
release it appears in. A new function's shape is often only right after
its first users; how can one come in without being promised at once?

**Options.**

1. No tier: a function is promised from its first release.
2. A header of its own for experimental functions. A function would
   move between headers when it is promoted.
3. A tier with no version in it. Nothing could say when a function is
   due to be promoted or removed.
4. A marker carrying the release the function came in.

**Decision.** Option 4. `CF_EXPERIMENTAL(major, minor)`, defined in
`cf_port.h` as `CF_API`, marks a function outside the 1.x promise for
the minor release it comes in only. `gate.abi` keeps
`cmake/configforge.experimental.symbols` ("name major.minor", and
" real" under `CF_WITH_REAL`) and fails once the version has moved past
a name: the card that opens the next minor makes it `CF_API` or removes
it. A name in two lists fails too. The bindings expose no experimental
function: they follow the C once it is settled. `docs/API.md` begins
with what 1.x promises (`## What 1.x promises`), and an experimental
function is marked there with the release it came in. A declaration is
read on one line, as `CF_API`'s are.

**Settled in 1.9.0 (I-2).** The thirteen functions of 1.8 were promoted, none removed.
`cf_format_limits` and `cf_usage` kept their signatures; a further limit or an option comes
as a new function.

**Compatibility.** Additive; no function is experimental yet.

## D-90 — The serialization library, a target of its own

**Question.** D-27 kept the serialization layer inside ConfigForge
"until needed". The readers and writers - a C89 YAML 1.2 reader that
passes yaml-test-suite, JSON5, TOML, CBOR, TLV - have value without a
configuration, and every format the Roadmap adds next would otherwise
enter ConfigForge's 1.x promise. When does the layer become a library?

**Options.**

1. As it was: one library.
2. A target of its own in this repository, before the first new format.
3. A repository of its own now, before it has users of its own.

**Decision.** Option 2. `configforge_serialize` is a CMake target
(exported `ConfigForge::configforge_serialize`), built from
`src/serialize` and the leaves it calls - the platform's file opener
(`cf_platform_file.h`) among them, S-3's three cuts having left it no
other tie - with the headers `cmake/ConfigForgeSerialize.cmake` lists;
`gate.layering` holds it to that list. Its sources are compiled twice,
into it and into `libconfigforge`, rather than shared as objects, so
each library keeps its own export flags; `libconfigforge` carries all of
it, so a program links one or the other, never both. Its version is
ConfigForge's, a checked copy (`cf_serialize_version.h`,
`cf_serialize_version()`; `gate.version` and `version.cmake` cover it).
`gate.abi.serialize` keeps its symbol list
(`cmake/configforge_serialize.symbols`). There is one amalgamation.
The name, `configforge_serialize` (the family `configforge_doctor`
began), was chosen on 2026-11-16 while the maintainer was away and
confirmed by the maintainer on 2026-11-17. Held to by `unit.serialize_alone`, a program linked against
the serialization library alone, and `gate.abi.serialize`.

It is installed as the CMake component `serialize` of the package
`ConfigForge` and as `configforge_serialize.pc`, whose static link line
carries neither SQLite nor `libconfigforge`; Conan's recipe has the two
components. `gate.install` builds a consumer of it from the CMake
package and from pkg-config.

**Compatibility.** Nothing a 1.6 program sees changes; two functions
are added.

## D-91 — CBOR, the JSON model in binary

**Question.** A device or a service that keeps its settings in CBOR (RFC
8949) has them copied into JSON to be read, and the copy is what explain()
then names. Can the library read the CBOR itself, and what of CBOR does a
configuration hold?

**Options.**

1. No CBOR. Rejected: the copy is exactly what explain() should not name.
2. CBOR through a codec the program supplies (D-68). Rejected for the
   common case: every program that wants it would write the same page of
   grammar, and the well-formedness rules are the part that is easy to get
   wrong.
3. A built-in reader.

**Decision.** Option 3, `CF_WITH_CBOR`, on by default as JSON is: no
dependency, a few hundred lines, in every preset (the minimal one, with
`CF_WITH_REAL` off, refuses floats by name). The data model is JSON's: a
map with text keys at the top; maps, arrays, integers, floats of half,
single and double precision, text strings, `false`, `true` and `null`
inside. Infinity and NaN are the text `inf`, `-inf` and `nan` with
`textual` set, typed by the contract, as TOML's are (D-62). Where CBOR
holds more than a configuration does:

* **Byte strings** are refused, as a value and as a key. A configuration
  has no type for bytes, and reading them as hex or base64 would be
  choosing an encoding nobody asked for.
* **Tags**: 55799 (self-described CBOR) is read past wherever it is; 0 (a
  date and time), 1 (seconds since the epoch) and 32 (a URI) are read as
  their content, checked against the tag; every other tag is refused by its
  number before its content is read, because read past, tag 4 would be an
  array where a number was written and tag 2's bignums would change
  meaning silently.
* **Indefinite lengths** are read, for maps, arrays, text strings and map
  keys: streaming encoders write nothing else, and Appendix A has them. A
  string's chunks are joined; each must be a definite-length text string
  and UTF-8 by itself (section 3.2.3).
* **Map keys that are not text** are refused, naming what the key is.
* **Duplicate keys** are read, the later winning and both kept as
  candidates, exactly as the JSON and INI readers do: explain() shows both
  places and Doctor's CFD001 names the earlier one. Refusing them, which
  section 5.6 permits, would make CBOR the one reader that refuses what
  JSON accepts, and needs a table of names per map for a check the library
  already makes, visibly, one level up.
* **`undefined` and the simple values other than false, true and null**
  are refused.
* **Integers outside the 64-bit signed range** are refused; `-1 - n` with
  `n` the greatest is the least int64 and is read.
* **The top** is a map after any number of 55799 tags, and nothing may
  follow it: a CBOR sequence (RFC 8742) is another format. No byte order
  mark is skipped: `EF` is a CBOR simple value.

Everything RFC 8949 makes not well-formed is refused: reserved additional
information, a two-byte simple value below 32, an indefinite length on
major type 0, 1 or 6, a break outside an indefinite-length item, a chunk of
the wrong type, an odd map before its break, an item cut short. A head
longer than it needs to be is well-formed and read. A length, an array's
item count and a map's pair count are checked against the bytes left
before the item is read, so a count of four billion costs nothing, and
nesting is bounded by `CF_MAX_DEPTH` with a loop for tags, so no document
costs more than that many frames.

Places are `<file>, byte <n>`, the offset counted from 0 of the first byte
of the item (the first of its tags), as `dd skip=` and `cmp` count: the
place a hex dump of the real file shows it, so `server.port` says
`settings.cbor, byte 14`.

**Writing.** `cf_emit(c, "cbor", ...)` writes what the reader reads back
to the same keys, types and values, through the same event interface as
every encoder. Maps and arrays have an **indefinite length** (`BF ... FF`,
`9F ... FF`): the events do not say how many members a container has
until its end, and the sink measures or writes in one pass, so a count
cannot be written before the members without a second walk or moving
bytes already written; indefinite length is well-formed CBOR that every
decoder reads. Rejected: a second walk to count each container, which
doubles the work for what no reader needs. Text strings and numbers are
definite and in **preferred serialization** (section 4.1): an integer in
its shortest head; a real as the shortest of half, single and double
precision that holds it exactly; NaN as `F9 7E 00` and the infinities as
`F9 7C 00` and `F9 FC 00`. It is **not deterministic encoding** (section
4.2.1): the walk writes a table's values before its tables, not keys in
byte order, and sorting them would be a second order for no reader here.

A literal `${` stays literal, as the text formats keep it. `cf_emit`
doubles each `${` in a text value after a text encoder has written it,
which is wrong for CBOR: a text string's length comes before its bytes,
and the doubled text would no longer match it. So a codec that writes
bytes says so, `CF_CODEC_BINARY`, and its state begins with a
`cf_encoder_head` whose `CF_ENCODE_ESCAPE_REFS` flag `cf_emit` sets after
the encoder function has zeroed it; the encoder doubles each `${` itself,
the length counting it, and `cf_emit` does nothing after it. `cf_doc_emit`
alone sets no flag and escapes nothing, which is what a conversion needs:
a reader's text is the text as written. Rejected: an escaped copy made by
`cf_emit` before the encoder sees the value (no memory to make it in, and
a bound on it would be a new refusal); an encoder that always escapes (a
conversion would then double what the source wrote once).

**From content.** A document with no name to give its format - fetched
from a server, held in an environment variable, a drop-in file with no
extension - is read as CBOR when its bytes are (D-73). The top is a map,
so the first byte is a map's head, `A0` to `BB` or `BF`, or the
self-describe tag `D9 D9 F7`. `A0` to `BF` are UTF-8 continuation bytes,
which begin no character, and `D9` is never followed by `D9` in UTF-8, so
no text that a text format here reads begins so: the first look sends
these bytes to CBOR first and sends no text there. The reading decides,
not the look, so a binary file that is not CBOR is still refused; and CBOR
is strict, taken at once when it reads.

**Compatibility.** Additive: an option on by default, and three public
functions that serve it and the readers after it - `cf_text_is_utf8`
(RFC 3629) and, under `CF_WITH_REAL`, `cf_f64_bits`, `cf_f64_from_bits`,
`cf_f64_from_f16_bits` and `cf_f64_from_f32_bits`. The image reader's own
copies of the first two are gone. Writing adds `CF_CODEC_BINARY` and
`cf_encoder_head`, which a codec of one's own may use or ignore.

**Held to.** The reader's own test, `test_cbor`, covers each decision and
each refusal above, with the documents written as hex, and the option off.
RFC 8949's Appendix A, through the vectors of `cbor/test-vectors` at the
pinned commit, each read as the member of a map: 68 of 68 read, the 13
others refused by name for the model (byte strings, tags 2, 3, 23 and 24,
integers past 64 bits, an integer key, `undefined`, simple values); Appendix F.1's 94 malformed
items, all refused; and 55 of 55 read to the values the vectors state.
The repository follows RFC 7049's Appendix A, which lists `simple(24)`,
`f8 18`; RFC 8949 calls it malformed (F.1, and Appendix G.1 says why the
example was removed), so the run scores it among the malformed items.
Detection from content scores 4 of the 68 (`detect-cbor`): the others
are lone values, which are no configuration, as most of JSON's suite is. The reader is fuzzed with the others - `test_fuzz` from a corpus
that holds each kind of item it takes, and the libFuzzer harness
`fuzz.cbor` - and held to the same bound on what an input may cost (D-82).

---

## D-92 — A codec carries its own data

**Question.** A codec made at run time from a description - a TLV layout
(D-93), or a binding's codec written in Rust or Python - has to reach that
description when it reads. `decode(in, out, err)` has no argument to reach
it by, and this library has no global to keep it in. How does a codec
carry data of its own?

**Options.**

1. A context argument added to `decode`.
2. A table of layouts for the whole process, looked up by the codec's
   name.
3. No codec for TLV: a source of its own, `cf_builder_tlv`, that holds
   the layout.
4. Two members at the end of `cf_codec` - a decoder that is handed the
   codec, and a pointer the library never reads - and one function that
   every reading goes through.

**Decision.** Option 4. Option 1 breaks every codec in and out of the
library. Option 2 is global mutable state, which D-68 kept out by giving
each builder its own codecs. Option 3 reads files only: a layout would
reach no drop-in directory, no document in memory, no conformance run's
`--codec`, and no writer later. So `cf_codec` gains, after
`encoder_state_size`, `decode_self(self, in, out, err)` and `const void*
data`; `cf_codec_decode` is the one way the library reads through a codec
it did not choose by name - a file, a document, a directory's file, the
trial reads of detection, the conformance reader - and takes
`decode_self` when the codec's size reaches it and it is set, else
`decode`. `cf_codec_can_read` says whether a codec reads: `CF_CODEC_READ`
and a decoder of either kind. The places that call the built-in JSON
reader by name keep `json->decode`: JSON has one.

**1.0's size is the floor** (D-51). A codec compiled against 1.4 is
smaller and goes on working: `cf_builder_codec` and
`cf_builder_file_codec` take any size from `offsetof(cf_codec,
decode_self)` up, and nothing past a codec's own size is read.

**Named only.** `CF_CODEC_NAMED_ONLY` keeps a codec from being chosen
from content (D-73): its name and its extensions still choose it. A
layout reads bytes no other format would refuse - a run of tags and
lengths is nearly any binary - so a guess at them would be taken, and a
codec of the builder's own is tried first and taken at once when it
reads. No built-in codec has the flag; `.properties` stays out of
detection by not being listed.

**Compatibility.** Additive: two members at the end, two functions, one
flag. A 1.4 codec is read as before. A 1.5 codec passed to 1.4 is refused
only when it has no `decode`, since 1.4 asks for one.

---

## D-93 — TLV, a codec from a layout

**Question.** Tag, length, value has a standard only as ASN.1's BER; a
device's own `.cfg` of two-byte tags and lengths follows no document but
its maker's. Such a file is configuration, and today it reaches a program
through code nobody else can read and no value of which can say where it
came from. How does this library read it?

**Options.**

1. Nothing: each program writes a codec for its device.
2. One TLV codec, for BER only.
3. A layout the program gives - the widths, the byte order, the end, and
   each tag's key and type - and a codec the library makes of it.

**Decision.** Option 3. Option 1 is what programs do now, and each such
codec is a page of offsets that explains nothing. Option 2 leaves out
every device that is not BER. A layout is one table, the whole of what
the program writes, and the library does the reading, the bounds and the
places.

* **The layout.** `cf_tlv_layout` carries its size and starts from
  `CF_TLV_LAYOUT_INIT`, so a later 1.x can add to it (D-51). `cf_tlv_tag`
  is an array's element and cannot grow, so it is fixed for 1.x, as
  `cf_pair` is; its `more` is NULL, and a library that finds it set
  refuses the tag rather than ignore what it does not know.
  `cf_tlv_codec_sized` carries the size of the `cf_codec` it fills, as
  `cf_def_init` carries its own, and refuses one from before 1.5.
* **Ownership.** The program owns the layout, its tables and its keys;
  the codec keeps the layout's pointer in `data` (D-92) and copies
  nothing, as `cf_builder_codec` copies no codec. One codec may serve many
  builders. A layout gone before its configuration is read is the
  program's mistake, as a codec on the stack is.
* **Checked whole.** The name is a format's (one key segment); the kind,
  the widths, the byte order, the flags and the unknown policy are known
  values; every table is in ascending order of tag with none twice - the
  order binary search needs and the way a duplicate is found - and each
  tag fits the width, is not the end tag, has a valid key unless it is
  skipped or a record, and a width its type can have. The first mistake
  is named, `CF_E_DEFINITION`, before any document is read.
* **The types.** Text (trailing NULs dropped, as a C string in a fixed
  field ends; a NUL left inside refused; UTF-8 checked); signed and
  unsigned integers of 1 to 8 bytes in the layout's order or the tag's
  own, built byte by byte so no host order matters, an unsigned one past
  the largest `cf_i64` refused; a bool of one byte, 0 or 1; a real of 4 or
  8 bytes, Infinity and NaN their text as CBOR's are; a record, skipped or
  refused. A tag with `CF_TLV_SKIP` is passed over: the layout knows it
  and chose not to read it.
* **The end.** With `CF_TLV_END`, the end tag ends the top level - with
  no length, or a length of 0 under `CF_TLV_END_LENGTH` - and nothing may
  follow it. Without it, the document ends with its last byte, which must
  end an element.
* **Places and limits.** Each value is `<file>, byte <n>`, n the offset
  of its tag, which is what a person looks for in a hex dump. A length is
  checked against what is left before its value is read.
* **In the bridge.** `src/bridge/tlv.c`, not beside the other readers: a
  layout's keys are checked by `cf_key_valid`, which the serialization
  layer may not reach (the layering gate).

* **Nests.** A nest's value is a TLV sequence of the same kind, read with
  its entry's tags as a table under its key; the end tag is not looked
  for inside, where the length bounds it. The document is level 1 and each
  nest one more, to `CF_MAX_DEPTH`. A nest whose tags are its own table,
  or one around it, is a recursive layout: checked once, read as deep as
  the document goes and no deeper. A nest with no tags reads every member
  as unknown.
* **Lists.** A tag flagged `CF_TLV_LIST` is an array under its key, an
  element for each occurrence. The occurrences must be one run: a later
  run is refused, naming the byte where the list ended, as INI's `name[]`
  lines are (D-60) - a second run would restart the count. Without the
  flag a repeated tag is two values for one key, the later winning and
  both kept as candidates, as two lines of an INI file are.
* **BCD and hex.** BCD is two digits a byte, the high nibble first, an
  integer of at most 18 digits; leading zeros are digits, as EMV's amounts
  have them. Hex is any bytes as lowercase hexadecimal text, written into
  the sink's text storage, or for a value under 128 bytes into the
  reader's own.
* **An unknown tag**, by the layout's `unknown`: refused, the default - a
  value nobody mapped is one nobody can explain, and a layout out of date
  should say so the first time the device's file outgrows it; skipped,
  which the layout's author chose; or kept under `tag-` and the tag's
  digits in lowercase (`tag-0099`), as hex. Rejected: a fixed prefix such
  as `unknown.`, which would move the key when the layout later names the
  tag, where `tag-0099` already says what it is.

* **BER-TLV** (`CF_TLV_BER`), as ISO/IEC 7816-4 and EMV use ASN.1's
  BER. A tag is its identifier bytes as one big-endian number - `5A`,
  `9F02`, `BF0C` - the way EMV writes it, so its class and constructed bit
  are part of it and need no field of their own; at most 4 bytes. A
  length is X.690's: the short form under `80`, `81` to `84` bytes
  following, longer than needed or not, as BER allows. The indefinite
  length, `80`, is refused: ISO/IEC 7816-4 and EMV do not use it, and its
  end would be found only by reading. A constructed tag's value is more
  TLV, so its entry is a nest, and a nest's tag is constructed: both are
  checked when the codec is made. An unknown constructed tag kept is a
  table of unknowns. With `CF_TLV_PADDING`, a `00` or `FF` where a tag
  would begin is passed over, before and between elements, as ISO/IEC
  7816-4 allows. The widths and the byte order are not read: the kind
  decides.

Writing is wave 6's: what a layout skips must be kept to write the file
back.

**Held to.** `test_tlv`'s three layouts - a device's fixed one, a
network one with nests, a list, BCD and hex, and an EMV record in
BER-TLV - with every document written by the test from its layout and
every refusal above by its words; the fuzzers' two, one of fixed widths
with a tag of each type and unknown tags kept, one BER-TLV with padding,
in `test_fuzz` and as the libFuzzer harnesses `fuzz.tlv` and `fuzz.ber`,
under the bound on what an input may cost (D-82); and
`examples/c/tlv.c`, which the suite runs.

**Compatibility.** Additive: one function, one option on by default
(`CF_WITH_TLV`; without it the function refuses by name, D-38).

## D-94 — Converting between formats

**Question.** A configuration moved from INI to TOML by hand, or by a
script that drops what it does not understand, arrives with values
missing and nothing to say so. Can a document be written in another
format, and what is said about what the target cannot hold?

**Options.**

1. Through `cf_emit`: read the file into a configuration, write it out.
2. The decoder's events straight into the target's encoder.
3. Through the key space and the encoder (`cf_doc_emit_where`).
4. The events recorded in a tree, and replayed into the encoder.

**Decision.** Option 4, in `configforge codec convert <file> --to
<format>`, and `configforge codec format <file>`, which is the same with
the document's own format as the target. Option 1 resolves a `${` and
doubles it: a document is converted, not a configuration. Option 2 breaks
INI and TOML: a JSON document's later root key lands inside an INI
section, and a TOML array after a sub-table lands inside that sub-table,
the two faults `src/bridge/flatten.c` records. Option 3 is not in the
serialization library, which the program links alone, and refuses every
name that is not a lowercase key.

The tree is built in an arena from the decoder's events. A table that
meets an earlier table of its name is that table reopened (INI's repeated
sections); anything else replaces the earlier member, which is left out -
the later one wins, as the file means it. Members are found by a hash of
the parent and the name, so a table of many costs no more per member than
one of few. The tree is bounded by D-82's rule, written in the tool: a
node costs its name, its text and 64 bytes, and a document may cost 64
times its size and 1 MiB more; past it the run is refused, exit 2. The
replay writes a table's values, then its arrays, then its tables, and an
array's members in order.

What the target cannot hold is found before anything is written, in three
passes, and each is a line on stderr naming its path and left out with
everything under it: a name an encoder that writes names as they are
would write unquoted (not for JSON, JSON5, CBOR, whose encoders write a
name as a string, nor YAML, whose encoder quotes one); an array the
target's caps cannot write (`cf_emit`'s rule); and a value the encoder
itself refuses (TOML's null, INI's line break), found by replaying into
an encoder that writes nowhere. Two notes are said once and are not
findings, since they are about the formats and would fail `--strict` for
every file out of TOML: a format that can hold comments, which no reader
keeps; and an untyped source written to a typed target as text. A target
with no types written values that were numbers, booleans or nulls as
their text, and that is a finding. `--strict` fails (exit 1) on any
finding and writes nothing, an `--out` file that existed being left
alone. Exit codes: 0 converted; 1 `--strict` and a finding, or a format
that cannot be told; 2 unusable input or options; 3 the encoder refused
what the check let through, a bug.

`${` is written as it was read, neither expanded nor doubled, in binary
encoders too. Comments are said once and lost: no reader keeps them.

**Compatibility.** Additive: new commands of `configforge-codec`;
nothing else changes.

## D-95 — Code for each binding from a contract

**Question.** The contract is written once, in a schema. A C++ program
that reads `cfg.get("server.port", 8080)` writes the key and its default
out a second time by hand, which is the repetition a contract exists to
end. How is the contract read in each language with its own types, and
how are the languages added without the tool knowing each by name?

**Options.**

1. Each binding's own defining calls: `cf::Builder::define(Key)` for C++,
   which carry a type, a default and a range, and lose descriptions,
   enumerations, lengths, lists, aliases, policies, deprecations and the
   opened prefixes.
2. The library's C calls, written whole (what `--lang c` writes), and for
   each language what reads: names for the keys and a struct filled from
   them.
3. The schema itself, read at run time by a JSON reader the program links.

**Decision.** Option 2, for C and C++; Rust and Python take option 3's
schema without a reader of their own (below). The C++ header holds the C body -
every line of it compiles as C++ - in `inline bool define(cf_builder*)`,
so the contract is the schema's, whole, in C++ as in C, and no JSON
reader is in the build. The Keys and the Binding only read: a `cf::Key`
names a key and carries its default (the schema's, or the type's zero),
no range and no flags, so that nothing is defined twice and nothing is
defined by a Key. `Contract::defineInto` lets
`cf::Builder().define(N::Contract())` take it. Rejected: option 1, which
loses most of what a schema says; and `constexpr` Keys under C++11, how
the generated file is checked at compile time being D-75's.

The languages are a table in `tools/gen/contract.c` - a name, the
extension of its file, the check of the name `--name` gives it, the words
it reserves and a writer - and `--lang` finds its entry there; nothing
else names a language, and a name not in it is refused with the names that
are. Rust and Python each add an entry and change nothing around it. A
generator outside the tree is a later decision.

The names are made from the keys, by one rule for the languages that need
them. A key's identifier is its text with `.` and `-` as `_`, `k_` before
a digit and `_` after a word the language reserves; two keys of one
identifier are refused, by name, rather than written as code that does not
compile. A struct is named from its top segment in CamelCase, and is
`Table` after one equal to the top struct's name. The shape is two levels:
a key of one segment is a field of the top struct, a key of two or more a
field of its top segment's struct, named by the rest of the key - because
`cf::Binding` reaches a field through at most two member pointers, and a
third level would be a struct the Binding could not fill. The order is the
definition set's, a struct where its first key comes. What has no field -
a list, whose elements are keys of their own, and a key of a type the
language does not read - is a comment where its field would be, never
dropped quietly.

**Rust.** C and C++ define the contract with the library's C calls, which
Rust cannot make without the crate's hidden `sys`; the crate's own
defining calls (`settings!`'s `define`) carry a type, a default, a range
and flags and drop descriptions, enumerations, lengths, lists, open
prefixes and, with no `f64` `Field`, reals. So the generated module
embeds the schema as a string and gives it to a new `Builder::schema`
(`cf_builder_schema`, 1.2's), which the library reads as the schema file:
the whole contract, by the library's one rule set (D-67), and the `Key`
constants and the `settings!` struct only read. Rejected: the binding's
defining calls, which lose most of what a schema says, named on stderr as
the file is written; and the same calls after teaching the crate
everything a `cf_def` holds, a feature of the crate larger than a
generator. The schema is one string, its lines kept as lines, `\` and
`"` escaped and the other control bytes `\u{..}`; the file has plain
comments and no inner attribute, because `include!` refuses one (`//!`
included), and the generated `contract` and `SCHEMA` are reserved with
the language's words. The struct is flat, a field for each key by its
whole identifier, `settings!` having no nesting; a real and a list are a
comment where the constant would be, until the crate reads reals. The
module's test is two halves, each where its tools are: a golden file,
`tests/gen/contract.rs`, which `gen.rust` holds the program's output to
byte for byte (and `--out` writes in binary, so Windows writes the same
bytes), and the Rust crate's own integration test, which `include!`s that
file and runs it under cargo. A golden file shows a change of the writer
as a diff; the cargo test shows that the file is Rust that works.

**Python.** The same answer as Rust's: the module embeds the schema as one
literal a line, and `contract(b)` gives it to `Builder.schema`
(`cf_builder_schema`, new in the package), so the contract is the schema's,
whole, by the library's one rule set; the dataclasses only read. They are the
shape of the C++ header's - a class for each top segment, defined before the
class that uses it, deeper keys flattened into it, a list or a type Python
does not read a comment - and each field is a `cf.setting` or `cf.section`,
so `cf.read(cfg, Settings)` fills it, and the program does not also call
`cf.define`, which would define each key a second time. What `setting` takes
beyond the key and the default is for whoever reads the file: `range` the
schema's, `doc` its description, `flags` the definition's, of which
`SENSITIVE` keeps the field out of the dataclass's repr. A real is a `float`,
unlike Rust's, because `cf_get_real` is in the package. The text is a
Python literal with `"`, `\` and the control bytes escaped, the file UTF-8,
which a module's source is. `--name` is the top class (`Settings`), checked as
an identifier and not a word the language reserves, nor `contract`, `cf`,
`dataclass`, `SCHEMA` or the type names `bool`, `float`, `int` and `str`, which
a field of the same name would shadow in its class's annotations. The module's
test imports the generated file by its path and reads a configuration through
it; `typing.get_type_hints` finds the classes in the module's own globals, as
`from __future__ import annotations` needs.

**Compatibility.** Additive. `--lang c` is the default and writes, byte
for byte, what `configforge gen` wrote; `--name` is, for C, the function
as it was, for Rust and Python, `Settings`.

---

## D-96 — A contract drafted from a sample

**Question.** A contract is what lets a value be refused with its key and
its reason instead of read wrong, and a program with years of
configuration files and no contract has none to start from. Writing one by
hand from a file it already has is the part nobody does. Can the tool
draft it, and what may it claim to know?

**Options.**

1. Types only: a key for every value the file holds, its type from the
   value, every key optional, no default, no range, an empty description.
2. Types and the sample's values as defaults.
3. A contract inferred from several files: a key optional where some
   file lacks it, a range from the values seen.

**Decision.** Option 1. `configforge gen schema --from <file>` reads the
file as a configuration (`--as` names its format, else the name and then
the content decide, D-73) and writes the JSON Schema through the
library's own writer, so the draft reads back as the definitions it was
made of. No default: a sample's value is one deployment's choice, not
the program's, and written as a default every other deployment would
inherit it without a word. No range and no `required`, for the same
reason - one file shows what a value was, not what it may be - and
every key is optional, which is what `cf_schema_write` writes anyway.
The description is written empty, to be filled in. The schema is closed,
so `configforge gen check` refuses a misspelt key against it.

Text a format has no types for - INI, YAML's plain scalars,
`.properties` - is typed by the library's Tier 0 inference (D-24), the
rule that reads it when no definition exists, so INI's `port = 8080` is
an integer: the draft says what the library would have read.

What a contract has no type for is left out, and said on stderr with its
key, never dropped quietly: a null (the sample does not show its type),
an array of tables, an array of arrays, an array of mixed types, an
empty array (its element type is not shown), and an array of nulls. The
exit code is still 0: the lines are findings about the sample, not
failures of the command. Rejected: guessing a type for them (a wrong one
refuses a good file), and making the command fail (a first draft from a
real file would be refused for a null in it).

Option 2 is rejected above. Option 3 is a different command: it needs
several files and a rule for which values are a range and which are an
accident, and a first draft from one file is the case with no such
answer. A later card may add it; this one does not close the way.

The draft ends with a line on stderr saying it is a draft: what it has
and has not, and that it is to be reviewed before it is the contract.

**Compatibility.** Additive: a new command of `configforge-gen`; Doctor
never had it, so there is no alias.

---

## D-97 — What kind of format a codec is, told to the program

**Question.** Formats now differ in ways a program cares about - text or
binary, readable on their own or only with a layout, typed or all text -
and the page that lists them (Formats.md) is written by hand. How does a
program learn what kind a format is, and how does the page stay true?

**Options.**

1. A separate registry of descriptions, one record a format.
2. The flags the codec already has (`cf_codec.caps`), one more for "needs
   a schema", and two functions over them.

**Decision.** Option 2. The flags are where `cf_emit` and the loader
already ask what a codec can do, so they are true or the library
misbehaves; a second table of descriptions is a second place to be wrong
and would differ from the codecs the first time one changed.

* `CF_CODEC_SCHEMA`: the bytes cannot be read without a schema or layout
  the program gives. Text is the absence of `CF_CODEC_BINARY`;
  self-describing is the absence of `CF_CODEC_SCHEMA`. A TLV codec made
  from a layout is `CF_CODEC_READ | TYPED | ARRAYS | BINARY | SCHEMA |
  NAMED_ONLY`; no built-in codec has SCHEMA.
* `cf_codec_select(all_of, none_of, out, cap)`: the built-in codecs with
  every bit of `all_of` and none of `none_of`, in registry order, at most
  `cap` written and the full count returned, as `cf_report` does.
* `cf_codec_kind(c, buf, cap)`: the kind as text in a fixed order and
  wording ("binary, self-describing, typed, arrays, read and written"),
  the length needed returned as snprintf does. It works for a codec of
  the program's own and for a layout's.
* The table in Formats.md is `doctor formats --format markdown`, between
  markers, and a test holds the page to it. A build without some formats
  prints fewer rows, each of which the page must hold.

Rejected: a registry of descriptions, above.

**Compatibility.** Additive: one flag bit, two functions, one Doctor
command. A program's own codec that sets no SCHEMA is self-describing,
which is what it was assumed to be. The C++, Rust and Python bindings
come later.

---

## D-99 — Three programs behind one front door

**Question.** Doctor had gathered three kinds of command: reading
reports, generating code from a contract, and converting documents.
Each has its own users and its own promise. How are they offered
without one program carrying all three?

**Options.**

1. Doctor keeps all three.
2. Three programs, each found by its own name.
3. Three programs behind one front door, as git has it.

**Decision.** Option 3. `configforge <name>` runs `configforge-<name>`,
found beside the front door itself, then on the `PATH`, never in the
current directory; a name is 1 to 64 bytes of `a-z`, `0-9` and `-`, not
starting with `-`. The front door exits with the program's own status,
or 2 when it refuses a name, finds nothing or cannot start one (126 and
127 are not used). It is not built where there are no processes (WASI,
`CF_PLATFORM=none`). Doctor's program is renamed `configforge-doctor`;
every tool is built into `tools/bin` and installed into `bin/`.
`configforge codec` is the serialization library's program, linked to
`configforge_serialize` alone, with `formats` moved from Doctor, `detect`
(a file's format and how it was told) and `check` (whether each file is
well formed). Its name was the one open question: `configforge-codec`
run as `configforge codec <command>`, over `configforge-serialize`
(longer to type on every command line) and one program per command
(five programs on the `PATH`, and `configforge check` would be this
tool's while `configforge gen check` is the contract's); the maintainer
chose it on 2026-11-17. Rejected:
a process-starting function in the library (the library starts no
program), `_wspawnv` on Windows, a table of known commands, and dropping
the name `doctor` at once.

**Compatibility.** `doctor` is `configforge-doctor`. In 1.7.x the name
`doctor` still runs it and says so on stderr, and the release archives
carry both; it goes in 1.8.0 (T-5). `configforge gen` took Doctor's
`check` and its C writer (`gen --schema`), and `configforge codec` took
`formats`; `doctor check`, `doctor gen` and `doctor formats` still run
them in 1.7.x and say so on stderr. The aliases and the old name went in
1.8.0: Doctor answers `check`, `gen` and `formats` as it answers any
command it does not know (usage, exit 2), no program is built, installed
or shipped as `doctor`, and the Python module no longer looks for one.

## D-100 — One directory per component

Today `src/` holds six layer directories and `include/configforge/`
holds every public header side by side. From 1.8.0 each component is a
directory of its own under `lib/`, with its own public headers, sources
and build file:

```text
lib/port/        cf_port.h, the build options: which compiler, which limits
lib/lanes/       the vector library (D-101); depends on nothing of ConfigForge
lib/numeric/     numbers read and written (D-102)
lib/base/        what every component shares: alloc, error, value
lib/text/        UTF-8, escapes, character classes, locations (D-102)
lib/platform/    the operating system: posix, win32, none
lib/serialize/   the formats, codecs and document events
lib/config/      the configuration library: core, sources, bridge, diag
lib/cxx/         configforge.hpp and the compile-time layer (D-104)
```

* Each component is `lib/<name>/include/configforge/*.h` (its public
  headers), `lib/<name>/src/` (its sources and internal headers) and
  `lib/<name>/CMakeLists.txt`. `lib/config/src/` keeps today's four
  layers as subdirectories (`core/`, `sources/`, `bridge/`, `diag/`),
  since their rules are already held by `gate.layering`.
* **The installed headers do not move.** Every component installs into
  the same `include/configforge/`, so `#include <configforge/cf_x.h>`
  keeps working and nothing breaks for a program. The move is of the
  source tree only. `lib/lanes/` is the exception: it installs its own
  `include/lanes/`, as a library that is not ConfigForge's.
* **What may use what** is a table in `cmake/ConfigForgeLayers.cmake`,
  read by `gate.layering`: an include from one component into another
  that the table does not allow fails. The table:

  | Component | May use |
  | --------- | ------- |
  | port | - |
  | lanes | - (its own portability header, not cf_port.h) |
  | numeric | port, lanes |
  | base | port, numeric (`cf_value.h` holds numbers) |
  | text | port, numeric, base, lanes |
  | platform | port, numeric, base |
  | serialize | port, numeric, base, text, lanes, platform's file opener (`cf_platform_file.h`) |
  | config | everything above |
  | cxx | config and everything it uses |

* The libraries stay what they are: `configforge_serialize` is made of
  port, numeric, base, text, lanes, the platform's file opener and
  serialize; `configforge` of all of them. A component is a directory
  and a build file that adds its sources to those two targets, as
  `src/<layer>/CMakeLists.txt` does today, not a target of its own:
  the libraries, their symbol lists, the version macros and `gate.abi`
  are unchanged by the move, and `gate.layering` is what fails a build
  that breaks the table.
* Tools that need every public header in one directory (abidiff, the
  CBMC proofs, `check_include_file` tests) read a staged copy the
  configure step makes, `${CMAKE_BINARY_DIR}/staged/include/`.
* Unit tests move into their component (below, "Each component is
  laid out as a project of its own").
* **Each component is laid out as a project of its own** (the
  maintainer, 2026-11-19: so that taking one out later is a copy, not
  a rewrite). `lib/<c>/` holds `CMakeLists.txt`, `include/`, `src/`,
  `tests/` and `README.md`:
  * the build file declares the component with one call,
    `cf_component(<name> DEPENDS <components> SOURCES <files>)`
    (`cmake/ConfigForgeComponent.cmake`); the DEPENDS lists are the
    layering table above, written once, where the component is, and
    `gate.layering` reads them;
  * `cmake -S lib/<c>` builds that component alone, with what it
    depends on, as a static library `cf_<c>`, and runs its tests;
    `gate.components` does that for every component, which proves at
    every change that each one can be taken out. Inside the whole
    project the sources still go into the two libraries, since CMake
    3.10 (the project's floor) cannot link object libraries, and the
    symbol lists stay as they are;
  * its unit tests live in its `tests/`; the tests that cross
    components (consumer, conformance, fuzz, doctor, gen, codec,
    proofs, front) stay in the top-level `tests/`, and the shared test
    helpers (`cf_test.h`, `cf_fixture.h`, `cf_kv_codec.h`, the data)
    move to `tests/support/`;
  * its `README.md` says what it is, what it depends on, its public
    headers, and its version: `lanes` and `serialize` have their own,
    the rest follow ConfigForge's.
  A-1 moves the files; A-1b makes each component a project.
* Rejected: top-level directories per component (they mix with `docs/`,
  `tools/` and `tests/`); moving the installed headers into
  `configforge/<component>/` (a break for every program, for no gain to
  it); a CMake target per component (each would need its own symbol
  list and export rules, for nothing `gate.layering` does not already
  hold).

**Status.** Decided, 2026-11-19.

Built: A-1 (824181f), A-1b.

A-1b: the table is the components' DEPENDS lists, one in each
`lib/<c>/CMakeLists.txt`; `cmake/ConfigForgeLayers.cmake` is gone, and
`gate.layering` reads the declarations the build writes at configure time.
A dependency may be spelled `<component>:<header>` (serialize's
`platform:cf_platform_file.h`): the build needs the component, and the
source may include that header of it and no other.

## D-101 — The vector library, not ConfigForge's

The word-at-a-time and SIMD work (docs/plan/P-word-at-a-time.md) is a
library of its own, usable by any C program, and ConfigForge is one of
its users. Its name is **lanes**: prefix `lanes_`, directory
`lib/lanes/`, headers under `include/lanes/` (the maintainer,
2026-11-19; `wordlane` and `swarmi` were the other names weighed).
Short, and it says what a vector is.

Checked on 2026-11-19 (docs/Roadmap.md, the SWAR and SIMD item): no
library found combines a C89 SWAR tier, a scalar reference per
operation, C99 SIMD tiers and run-time choice behind one C API. That is
what this one is.

* **Two levels.** (1) **Span operations**, the API a program calls: a
  function over a byte span, such as "the index of the first byte of
  this set". (2) **The lane vocabulary**, used inside each backend:
  portable vector types and operations by lane width,
  `lanes_<op>_<type><count>` (`lanes_add_u32x4`, `lanes_eq_u8x64`), a
  512-bit register seen as 64 of 8 bits, 32 of 16, 16 of 32 or 8 of 64.
  The choice of instruction set is made per span operation, never per
  lane operation: choosing per instruction costs more than it saves,
  which is why Highway also chooses per function.
* **The span operations of the first release**, exactly what the
  readers need and nothing more: `find_byte`, `find_any` (a set of up to
  256 bytes, made once by `lanes_set_init`), `find_lt` (the first byte
  below a value), `ascii_run` (the leading bytes below 0x80),
  `utf8_valid` (the length of the valid UTF-8 prefix), `count_byte`,
  and `digits8` (eight ASCII digits to their value, or "not eight
  digits"). A further operation is added when a user needs it, with its
  scalar form first.
* **Backends.** `scalar` (the reference, C89, always built), `swar32`
  and `swar64` (C89, words read with `memcpy`), `sse2`, `sse42`,
  `avx2`, `avx512` (x86), `neon`, `sve` (Arm). Each SIMD backend is its
  own C99 file compiled with its own flags (`-mavx2` and the like;
  none for MSVC), so the rest of the library stays C89 and a program is
  never built for a processor it may not have.
* **No global mutable state.** A backend is a constant table of
  function pointers, `const lanes_ops*`. The program asks for one and
  keeps it: `lanes_scalar()` always; `lanes_by_isa(LANES_ISA_AVX2)`,
  NULL when it was not built or this processor lacks it (manual
  choice, the first stage); `lanes_best()`, the best built backend this
  processor runs, in the order AVX-512, AVX2, SSE4.2, SSE2 on x86, SVE,
  NEON on Arm, then SWAR at the word width, then scalar (the automatic
  stage). Nothing is detected or chosen behind the caller's back, so
  two threads can never race over a choice.
* **Detection.** At build time from the compiler's macros (architecture,
  byte order, what it can emit). At run time `cpuid` and `xgetbv` on x86
  (the operating system must save the wider registers), the auxiliary
  vector (`getauxval`) on Linux Arm, `IsProcessorFeaturePresent` on
  Windows, NEON assumed on AArch64. Byte order matters only where a
  SWAR mask becomes an index; that step has a form per byte order,
  checked once by `lanes_by_isa` against a known word.
* **The build option** `CF_LANES_BACKENDS` chooses what is compiled:
  `scalar` (the reference alone), `c89` (scalar and SWAR, nothing that
  needs C99), or `all` (every backend the compiler can build, the
  default). The `misra` profile builds `scalar` (no word read through
  `memcpy`, MISRA C Rule 21.15); `minimal` and the embedded profiles
  build `c89`. Until 2.4.0 adds the others, every value builds `scalar`.
* **Every backend is tested against `scalar`**: every operation, every
  length from 0 to four vectors plus a tail, every starting offset,
  poison bytes after the end that must never be read (sanitizers), and
  a fuzz target comparing every backend the machine runs. CI forces each
  backend in turn (`LANES_FORCE=<name>`); a backend the runner cannot
  run is built and listed as "built only".
* **ConfigForge's use.** In 1.8.0 the readers call `text` and `numeric`,
  which call `lanes_scalar()` only: the abstraction is in place, the
  speed is not yet (2.4.0 adds the other backends and the program's
  choice, `cf_builder_lanes`). The `misra` and `minimal` profiles build
  `scalar` alone.
* **Later, and not ConfigForge's to build**: layers for other kinds of
  data over the same lanes (bytes: hex, base64, checksums; arrays:
  reductions, prefix sums, sorting; images and N-dimensional data:
  strided views, conversions). Named here so the lanes API does not
  shut them out; none is planned.
* It moves to a repository of its own (`git subtree split`) when a
  second project uses it, or at its first release, whichever is first.

**Status.** Decided, 2026-11-19.

Built: A-2 (da0dd49); the word-at-a-time backend, `lanes_swar()`: P-2 (the commit is in docs/Plan.md).

## D-102 — One copy of each text and number helper

The readers each carry their own UTF-8 checks, escape decoders, number
readers and whitespace loops (the inventory is under "The helpers
today", below). Each becomes one function in `lib/text/` or
`lib/numeric/`, and a difference between formats becomes a parameter,
never a second copy.

* `lib/text/` is internal (not in the 1.x promise): `cf_text_*`
  functions for UTF-8 (validate, decode one code point, encode one),
  escape decoding by family (the `\u` family of JSON, JSON5 and TOML;
  YAML's; XML's entities; INI and `.properties`' backslashes), character
  classes as one 256-entry table, whitespace and comment skips by
  syntax, and the count of lines and columns behind a `cf_loc`.
* `lib/numeric/` keeps the public `cf_numeric.h` as it is and gains the
  readers' own number code behind it, one integer reader (bases,
  underscores, signs, overflow) and one real reader and writer.
* Both call `lanes` for their scans (D-101), so P-3's "the scans P-1
  found" becomes "the few functions in `lib/text/`", and an
  optimization made once reaches every format.
* XML's entities stay expat's and INI has no escapes, so neither is a
  family of the escape decoder (A-7 says why).
* Each helper is moved with a test that runs every reader's old input
  through it first; a reader is switched only when the conformance
  suites pass on it unchanged.

**Status.** Decided, 2026-11-19.

Built, in part: A-3 (520baa2) put the UTF-8 decoder and encoder, the hex
digit and the table hash in `lib/text/` and compiled lanes into both
libraries. A-7 added the escape decoder per family, the writers' escaper
and the `${` functions in `lib/text/`. A-8 added shared blanks, comments,
line breaks, locations and refusal messages. The number reader has no
card yet.

## D-103 — Reading what is almost a format

A document that is nearly JSON - `{{a:1}}`, `{a:{2}}`, a sender's own
habits - is read by composing codecs, not by loosening the built-in
ones. Four levels, of which three exist:

1. **A dialect with a standard** is a codec of its own name, as JSON5
   and JSONC are (D-66): `{a:1, b:2}` is JSON5 today.
2. **A whole format of the program's own** is a `cf_codec` the program
   fills and registers with `cf_builder_codec` (the Guide, "Your own
   format"). Exists.
3. **A codec composed from a codec** (experimental, `cf_codec_compose`): a base
   codec, a name, and either or both of
   * a **repair**, `cf_bool (*)(void* ctx, const cf_blob* in, cf_blob*
     out, cf_error* err)`, run on the bytes before the base reads them
     (`{{a:1}}` to `{a:1}`);
   * a **filter**, which wraps the `cf_doc_sink` the base writes into
     and may rename, drop, merge or retype what passes
     (`{a:{2}}` to `a = 2`), the way a transforming serializer does in
     other languages.
   The composed codec is a `cf_codec` built in the caller's storage on
   the existing `decode_self` and `data` members (D-92), so nothing in
   the pipeline changes. It carries `CF_CODEC_NAMED_ONLY`: it is never
   chosen from content. Its name is the one the program gives
   (`json+repairs`), and every value read through it says so in its
   provenance, so `explain()` shows that a value passed through the
   program's repair.
4. **Recovery inside a lexer** (a callback on an unexpected token) is
   rejected for now: it would make every lexer's internals a promise.
   Reopened only if a user brings a case levels 1 to 3 cannot read.

Two readers disagreeing about one document is an attack (request
smuggling is the web's form of it), so no composed codec exists until
the program makes one, and it is never chosen from content. The build
option `CF_WITH_COMPOSE` defaults ON; the `misra` profile sets it OFF.
Its repair and filter are the program's code, named in every report.

**Status.** Decided, 2026-11-19.

Built: A-9. The examples are in the Guide, “Reading what is almost a format”.

## D-104 — Work moved to build time, in layers

The core stays C89 and is always there. Work that can be done when the
program is built is done then, by whatever the compiler offers, without
the program's source changing with the compiler:

* **Layer 0, the C89 core**: reads, merges and explains at run time.
  Its ABI is the 1.x promise.
* **Layer 1, build-time kits**, each for one language and level, each
  doing its work before the program runs and never calling layer 0:
  * **C, any standard**: X-macro tables and `configforge gen`'s output.
    New: **a baked configuration** - `configforge gen --bake <file>
    --schema S` resolves a configuration when the program is built and
    writes it as C89 `static const` tables, provenance included ("from
    device.toml line 12, at build time"). The program then links no
    reader at all; `cf_config_from_baked()` gives the same `get` and
    `explain` over the tables. For firmware whose configuration is fixed
    at build time this is no parser in the binary, and it is the field
    docs/Market.md found open.
  * **C23**: `#embed` of the file into the program, read at start-up,
    where baking is not wanted.
  * **C++**, header-only, by feature rather than by standard number
    (feature-test macros such as `__cpp_consteval`, never `__cplusplus`
    alone): **tier 11** key names hashed at compile time and checked
    with `static_assert`; **tier 17** a constant key table and a perfect
    hash; **tier 20** a JSON or TOML text parsed at compile time into a
    constant table, and a key name that is not in the contract a compile
    error; **tier 26** `#embed` and static reflection, a struct bound to
    its contract with no generated code.
  * **Rust**: `const fn` and a derive macro, in the Rust binding.
* **Layer 2, the facade**: `configforge.hpp` (in `lib/cxx/`) offers one
  call, such as `cf::load<Settings>(source)`, and does at compile time
  what the compiler's tier allows and the rest through layer 0. The
  program's source is the same under C++11 and C++26; only when the
  work happens changes.
* **The rule that keeps the layers honest**: every result a build-time
  path produces is compared, in the tests, with what layer 0 produces
  from the same input. A build-time result that differs is a bug in
  the build-time path.
* Layers 1 and 2 are header-only or generated, so they have no ABI and
  may follow the newest standards freely; the C ABI promise is not
  touched by them. MISRA C++:2023 applies to `lib/cxx/` (docs/Compliance.md).

**Status.** Decided, 2026-11-19.

Built: later milestones.

## D-105 — Build profiles by name

A profile is a named set of options in `cmake/profiles/<name>.cmake`,
one `set(<option> <value> CACHE ...)` a line, chosen by
`-DCF_PROFILE=<name>` or by `-DCF_PROFILE_FILE=<path>` for one of the
program's own. The profiles: `full`, `default`, `minimal`, `misra`,
`embedded-json`, `embedded-cbor`, `server`. An option given on the
command line wins over the profile. The configure step prints the
profile, each option that differs from it, and the estimated size from
the README's table. The profile's name is written into
`cf_build_options.h`, `cf_build_profile()` and the SBOM's options (not
the build id, which stays a bare commit id: tests and the Python
binding pin its form). Every shipped profile is built
in CI. ("Profile" here is a build's; the platform's file locations
"with profiles" in the Roadmap are a program's, a different thing.)

**Status.** Decided, 2026-11-19.

Built: A-11.

## D-106 — A newer C only where a file needs it, and said so

C89 stays the floor. A file that needs more - a dependency's header
(libfyaml's C11, CoreFoundation's C99), a SIMD backend (C99) - says so
through one helper, `cf_source_needs_c(<file> <standard> REASON "<why>")`
in `cmake/ConfigForgeCVersion.cmake`, which sets that file's standard,
prints a configure-time warning naming the file, the standard and the
reason, writes it into the SBOM's options, and records it in a list that
`gate.cversion` compares with the files actually compiled above C89. A
file above C89 that is not on the list fails; so does an entry with no
reason. The `minimal` and `misra` profiles refuse every entry, by name,
at configure time. Code already written in C89 is never rewritten to a
newer standard unless it has to be.

**Status.** Decided, 2026-11-19.

Built: A-11.

## D-107 — One refusal, the same in every format

`cf_error` is already the one error of the library (a code, the key,
the source, a message). What it lacks is the place and the reason in a
form a program can test rather than read:

* a `cf_loc where` member (line and column; `cf_loc` has no byte
  offset, and the binary formats keep their place in the message),
  added through `size` as D-51 adds members, so 1.x callers are
  unaffected;
* a `reason`, from a new list `cf_reason.def` beside `cf_code.def`, of
  what was refused, the same name in every reader: `CF_REASON_UTF8`,
  `CF_REASON_DEPTH`, `CF_REASON_NUMBER_RANGE`, `CF_REASON_DUPLICATE_KEY`,
  `CF_REASON_TRUNCATED` and the rest the inventory finds. The code
  stays coarse; the reason is what a test of "the same bad input is
  refused the same way in every format" compares.
* Additive in 1.x. A reader that today puts the place only in the
  message keeps the message as it is and fills `where` too.

**Status.** Decided, 2026-11-19.

Built: A-10. `where` holds line and column; the byte offset D-107 first
named stays in the message until D-138, which builds it.

## D-138 — A refusal's byte offset (K-6a)

D-107 named the byte offset and left it in the message. D-223 gives `cf_error` its place, so the offset joins it once, as a number a program reads:

* `size_t offset;` after `reason` in `cf_error`, `(size_t)-1` when the reader has none (`CF_ERROR_INIT` and `cf_error_set` set it so). Added through `size` as D-51 adds members: written and copied only where `size` reaches it.
* `CF_EXPERIMENTAL(2, 0) cf_error_place_at(e, reason, line, col, offset)` fills it; `cf_error_place` is the same with no offset. Internally `cf_text_fail_at` is `cf_text_fail` with an offset.
* The offset is the index, from 0 from the first byte of the input (a byte order mark included), of the byte the column names: the same pointer as `col` in the text readers, the item's start (the number its message names) in CBOR and TLV, expat's byte index in XML. A refusal from the sink, and YAML's document and alias events, have none: `(size_t)-1`.
* Experimental in 2.0, not 1.9 as the card said: the tree is 2.0.0 and `gate.abi` refuses a name experimental since 1.9 (D-89).
* Rejected: the offset inside `cf_loc` (every `cf_loc` grows: an ABI change in 1.x); `cf_error_offset()` parsing the message (a message is not an interface).

## D-108 — Every text is UTF-8 inside; another encoding only by name

Decided by the maintainer on 2026-11-19, after weighing what others do.

**What others do.** Refuse: Rust (`str::from_utf8`, serde_json),
simdjson, yyjson by default, Jackson, libyaml, Python's files
(`errors='strict'` by default), PostgreSQL. Replace with U+FFFD:
browsers (the WHATWG Encoding standard), Go's `encoding/json` v1,
Java's `new String(bytes, UTF_8)`. Pass through: RapidJSON by default.
Escape without loss: Python's `surrogateescape` (PEP 383), Rust's WTF-8
for Windows paths. Go's `json/v2` moved from replacing to refusing by
default. JSONTestSuite leaves invalid UTF-8 to the implementation (its
`i_` cases), so refusing costs no conformance.

**Today** TOML, CBOR and TLV refuse invalid UTF-8; JSON, JSON5, the
built-in YAML reader, INI, `.properties` and dotenv pass the bytes on
("The helpers today").

**The rule.**

1. Inside the library every text is valid UTF-8.
2. A reader refuses bytes that are not, with the line and column of the
   first one. Nothing is replaced and nothing is guessed.
3. Another encoding is read only when the program names it: a
   `CF_ENCODING_*` value in the source's flags, or its name in the
   tools. The bytes are recoded to UTF-8 as they are read, and the
   source says so ("read as windows-1254").
4. A byte-order mark is a declaration, not a guess: UTF-8's is skipped,
   as today; UTF-16's, in either byte order, makes the file UTF-16,
   recoded; UTF-32's is refused by name.
5. A guess is allowed in one place only: a refusal's message may say
   which encoding the bytes look like and how to read them so. It
   never changes a value.
6. A file is converted for good, once, by `configforge codec recode`:
   its bytes recoded and nothing else changed, for a person to review
   as a diff.

**Exceptions, each for a reason.** XML declares its own encoding in the
document (`<?xml ... encoding="..."?>`) and expat reads by that
declaration: a declaration, so kept (`CF_CODEC_SELF_ENCODED`). CBOR's
and TLV's text strings are already checked. The contents of secrets
and credentials directories are opaque bytes the library neither parses
nor prints, so they are not checked; a value type for bytes, which
would hold them properly, is weighed for 2.0.0.

**Why not guess.** Windows-1252 and Windows-1254 differ only at 0xD0,
0xDD, 0xDE, 0xF0, 0xFD and 0xFE, which are exactly Ğ, İ, Ş, ğ, ı and ş.
A short value cannot tell them apart, and a wrong guess gives a wrong
value that looks right (`Þükrü`).

**Why not U+FFFD.** Different bytes become one text: two keys or two
secrets collide, a digest no longer matches the file, a file written
back has changed. Two readers of one file disagree, which is an attack
(Bishop Fox, "An Exploration of JSON Interoperability Vulnerabilities",
2021). Lenient decoding has been exploited before: the overlong `/` of
MS00-078.

**When.** All of it in 1.8.0 (A-4 to A-6), under "### Changed" with
this number: an input refused that was accepted, which
docs/Maintaining.md allows in a minor. One step, not a warning first:
the refusal names the one setting that reads the file again, so a
warning period would only keep an inconsistent and attackable
behaviour longer, while the users who would be broken are still few.

**Status.** Decided, 2026-11-19.

Built: A-4 (the reading by name and by byte-order mark), A-5 (the
refusal of bytes that are not UTF-8, with their place and a hint), A-6
(`configforge codec recode`: a file converted once, for review, and
`--check`).

---

## D-109 — What a format can hold, and every pair tested

**Question.** What a conversion loses should be said once and tested, and
it was written in three places that did not know each other: `g_formats`
in `lib/config/tests/test_roundtrip.c`, W5-V1's three passes and each
encoder's refusals. Two losses fell between them: an empty table is
written as nothing by TOML and INI, and a null was counted as text for
YAML, which writes and reads it as a null.

**Options.**

1. More `CF_CODEC_*` caps. Caps say what a codec does with the pipeline
   (read, write, typed, arrays); these say what the format's syntax can
   carry, and a codec a program registers has no such word.
2. A member added to `cf_codec`: a codec of the program's own would have to
   fill it, and the size guard of `cf_codec_has_self` would grow a case.
3. A table kept inside the tools: the converter and the test would each
   keep a copy, which is the fault this ends.
4. A second word of bits kept with the formats, beside each format's name,
   extensions and option (`cf_format_info`), known for a format a build left
   out.

**Decision.** Option 4. `cf_format_holds(format, &holds)` (experimental in
1.8) gives the bits `CF_HOLDS_*` of the six formats that have a writer:
integers, reals, booleans, null, arrays of values, empty arrays, arrays of
tables, arrays of arrays, empty tables, names beyond letters digits `_` and
`-`, an empty name, line breaks and NUL bytes in a text; and
`CF_REFUSES_NULL` for a format that lacks null and refuses it (TOML)
instead of writing the text `null` (INI). A type a format lacks is written
as the value's text, which the reader gives back; anything else is left out
by the converter, which names it. Not bits: NaN and the infinities (the
library holds no real that is not finite; they are text in every format), a
mixed-type array (all six hold it), the order of keys (the converter writes a
table's values, then its arrays, then its tables, so no pair keeps it and
the comparison ignores it), a key that is not text (no reader gives one).

What differs by number and not by kind is `cf_format_limits(format,
&max_path, &max_depth)`, a second experimental function: a format never
writes what its own reader refuses, and the readers disagree. Nesting
below the document: JSON, JSON5, YAML and CBOR 31, XML 30 (its leaf is an element too), TOML 32, INI none
in its reader. A joined path of names to a table, array or value: INI and
TOML `CF_KEY_MAX - 2` (a length both the writer and the reader take, and
not the first one refused: TOML's reader limits the dotted key of a line
relative to its header, so no one number is exact), none for the others.
Found by the first run of the pair test, which saw TOML's reader refuse a
path its writer wrote; `test_holds` reads each depth at the limit and one
past it.

The converter and the test read this one table: `convert_core.c` writes its
passes for a target the table knows from the bits and the numbers (a target
it does not know keeps W5-V1's, by its caps) and keeps the encoder trial as
the net. The test runs through the core and not the program (some 800
documents, 36 pairs, three hops would be sixty thousand starts), and its
oracle, `ch_lower` in `tests/codec/chain.h`, is written from the bits and
the numbers alone: what the table says a format writes of what the source
gave. Three kinds of document: the edge documents built in the test (every
type, the extremes of numbers, texts, names, arrays, empty tables, paths
at the boundary), the project's own fixtures, and the conformance suites'
valid documents, which are an `EXTRA` since they are fetched with git. Each
hop is held to the oracle and a loss is never quiet: when what was read
differs from what was written, the hop reported a finding or counted a
value as text; a pair whose target holds everything its source holds loses
nothing; writing twice is the same bytes. `libFuzzer` takes a random
document through a random chain of the six. The tables of what each format
holds and what each pair loses are generated by `configforge codec pairs`
into `docs/Formats.md` and held to the program.

Settled in the converter and the test, not in the readers: the YAML reader
says a second, empty document table at the end of every file, which the
converter's tree and the test's take as the same table reopened (the
reader's own fix is on the Roadmap). Writing a YAML file twice is stable
from its first rewrite, not from its first write: a typed source's `1` is
`"1"` once it has been read as text.

**Compatibility.** A new experimental function pair and enum values;
`configforge codec convert` names more findings than W5-V1 said (an empty
table, a name that is empty, a path too long, nesting too deep) and counts a
YAML null no longer as text.

## D-110 — Generators outside the tree

**Question.** `configforge gen` writes the contract in the languages the
project ships (D-95). A team that wants it in Go, Zig or a schema of its own
should not have to change this project. How does a language the project does
not ship reach the program?

**Options.**

1. A plug-in loaded into the program: a shared library with a function the
   program calls.
2. A program found by name, as git finds its subcommands:
   `configforge-gen-<lang>`, handed the contract on its input.
3. A configuration file that names the generators.
4. The library's own schema writer (`cf_schema_write`) as the hand-over.

**Decision.** Option 2.

1. A plug-in is built against this project's headers, loaded into its
   process, written in a language that can export a C function, and its
   crash is the program's. A program is none of those, and any language
   writes one.
2. A configuration file is a place to be wrong in and a thing to find; the
   name on the PATH is the registration, and the installed tools are found
   the same way (D-99).
3. The schema is JSON Schema, where a generator would walk `$defs`, `type`
   unions and nesting to find a key: the work the contract's JSON ends. The
   definitions as C structs in a binary form would be read by C alone.

**The order of a `--lang`.** (1) A name the table holds is its writer, and
nothing outside the tree is looked at: `configforge-gen-c` on the PATH never
replaces C. (2) Else the name must be 1 to 64 bytes of `a-z`, `0-9` and `-`,
not starting with `-` (D-99's rule for a command), so `../x` and `Fake` are
never run. (3) Else the program is looked for beside this program, then in
each directory of the PATH, never the current directory and never an empty
entry; the first regular file that may be run is the one. Not found is exit 2
and the table's names and where it looked. (4) Else the schema is read, and a
schema that cannot be is exit 2 with nothing started.

**The JSON** (`gen_contract_json`, `--lang contract-json`): version 1 of
`configforge-contract`, the schema's name and text, the `--name` given, the
opened prefixes and a definition for each key in the library's order. Every
member of a definition is always present and an absent value is `null`, so a
generator never tests for a missing key and a missing key is always a version
mismatch. Integers are their exact 64-bit digits and a real always has a `.`
or an exponent, so a reader never takes one for the other. Not in it: the
schema's closedness (the C writer does not write it either), the checks a
language makes of its own, and the library's version (a golden file would change
with every release). `contract-json` is an entry of the table like the others,
so what is piped to a generator is what an author sees and what the golden file
holds: one writer.

**What the program is given.** The JSON on its standard input, written whole
from memory and then closed; on its command line every argument `gen` was given
but `--lang` and `--schema` and their values, in order and unchanged. `--name`
and `--out` are the generator's to check; its standard output and error are
`gen`'s own, and a file `--out` names is the generator's. The generator's exit
code passes through. A program that cannot be started, or was stopped by a
signal (Windows: ended with a code above 255), is exit 2 with the path. A write
to a pipe the generator closed is dropped, so a generator that does not read
its input is not a failure of `gen`.

**Starting it.** POSIX: `posix_spawn` with file actions, not `fork` and
`_exit(127)`: a program that cannot be executed is then the call's own error,
and no exit code of a generator is mistaken for one. Windows: `CreateProcessW`
with a quoted command line, and `.exe` only, not PATHEXT: `.cmd` and `.bat` run
through `cmd.exe` and its quoting, where the 2024 batch-file argument
injections lie. A platform that cannot run another program says so.

**Why a pipe.** A temporary file is a name, a race and a file left behind;
handing over the schema's path would make the generator read JSON Schema;
capturing the generator's output and writing `--out` here would shut out a
generator that writes more than one file or a binary one, and a large output
would sit in memory.

**Compatibility.** A new behaviour of a `--lang` that was an error. `--lang`
absent still writes C; a name that is in the table is still its writer.

**Status.** Decided and built: W5-G4.

## D-111 — The comparison with other libraries: one set of flags, the same work, and a history kept on its own branch

**Question.** How is "faster than the others" made a claim that can be checked,
and how is a change that makes it false seen before it is merged? A reader that
is faster than it was last month is not shown to be fast; one that is faster
than what a user would otherwise pick is (Roadmap, "The aim: beat the others at
everything, and say where it does").

**Options.**

1. The numbers each library publishes. Rejected: different documents, flags
   and work, so the figures cannot be set beside one another.
2. A comparison built by each library's own build files. Rejected: each
   applies its own flags (`-O3`, `-march=native`, a sanitizer), and a library
   built with them is not compared with one built without.
3. One harness: every library's sources compiled by this project, under one
   flag set; every adapter given the same bytes, as files, and doing the same
   work, one fold over every value; validation shown by probes, not assumed;
   each version pinned by commit (decided).
4. None. Rejected: P-2, P-3 and the SWAR and SIMD backend would have nothing to
   be held to.

And for the history of the numbers:

1. A data file in the documentation site. Rejected: every deploy replaces the
   whole site, so a file kept in it has to be carried from the last build by
   something that is itself a store.
2. Workflow artifacts. Rejected: they expire (90 days).
3. A database or a service. Rejected: a dependency for a page.
4. A branch of the repository, `bench-history`, an orphan (decided): every
   point is a commit, with the date, the diff and an author a reader can audit;
   the docs build reads it with a plain checkout and no token; and main's
   history stays free of a bot's commits.

**Decision.** Option 3, and a branch.

* The documents are made by `cf_bench --emit-docs`, deterministic (a fixed
  seed mixed with the document's number), and the comparison reads files, so a
  library and a compiler are given the same bytes. The 1 MiB lines are
  committed (`bench/docs-1mib.fnv`) and `bench.docs` regenerates them: a
  generator that drifts, or differs between compilers, is seen. It found one on
  the first run: an argument evaluation order that GCC and Clang took
  differently.
* The libraries are fetched at a commit (`scripts/bench/fetch.cmake`), not by
  FetchContent, which fetches at configure time and brings in each library's
  flags, and not by submodules, which would put eleven libraries in every clone
  of a project whose default build has no dependency. They are built by
  `bench/compare/`, a CMake project the top-level never adds, with
  `CF_BENCH_FLAGS` and nothing else.
* Each adapter has a strict mode (every check the library offers that
  ConfigForge makes) and a loose one (its own default); the headline rows are
  strict. Eight probe documents per format say what each reader refuses, and
  the gap is printed on the page, not hidden.
* **The gate counts instructions and never wall time**: a shared runner varies
  by tens of percent in time and almost not at all in instructions. A format's
  4 MiB read (16 took 15 minutes under callgrind, over the budget) that costs more than 110% of
  `bench/baseline/instructions.json` fails the push. The baseline is raised
  only in a commit whose message says what made the code cost more.
* Every push's numbers, from the quick run (ConfigForge alone), are appended
  to `bench-history`; the full comparison, every library and document, runs
  weekly. `docs/Performance.md` draws each format's line over time, with the
  commit of each point, and shows where ConfigForge is behind first.

**Consequence.** A library enters the round by an adapter, a
`libs/<name>.cmake`, an entry in `settings.json` and its probe lines, and
leaves by removing them. XML cannot be a win: ConfigForge's reader is expat
(D-23) with the layer's own handlers on top, so its row is an overhead of the
layer and is said to be.

**Status.** Built: P-1b.

## D-112 — The MISRA record is plain text of file and count, three kinds, and the gate fails open nowhere

**Question.** `docs/Compliance.md` says the library is checked against
MISRA C:2012 on every commit, and nothing checked it. How does a free checker
(cppcheck's MISRA addon) become a gate that keeps the claim true while the
library changes, without six thousand edits to quiet it on the first day, and
without a green that means "the tool did not run"?

**Options.**

1. Fix every report, then fail on any. Rejected: a judgement per site (the
   allocator's own `malloc`, an octal mode, a codec descriptor), and a commit
   that changes code six thousand times to quiet a checker is how a gate loses
   the trust it was made for.
2. Inline suppressions with a comment each. Rejected: a comment that hides a
   report is a record the gate cannot count, and it sits where a reader of the
   code, not of the standard, finds it.
3. A record of every report, the gate failing on a report the record lacks and
   on a record line that matches no report (decided): a ratchet, which a later
   card lowers by fixing.

**Decision.** Option 3.

* **The record** is `docs/compliance/deviations.txt`, plain text, because the
  gate is CMake and a Markdown table needs a Markdown reader. One line a
  record: `rule | file | count | kind | reason`, a `tool | cppcheck | N.M`
  line (exactly one, the first), and `ignore | rule | reason` lines. A reason
  is at least 12 characters, is not `TODO`, and holds no `;`, `[` or `]`.
* **File and count, not file and line.** A line number moves with every edit
  and every format pass, so each card that touched a file would rewrite the
  record. With a count, a new report in a file that has the rule fails (the
  count is higher), a fix fails until the line is lowered (the record no longer
  matches), and a report moved between files fails twice. `-DWRITE` makes the
  update one command, and the diff is what is reviewed.
* **Three kinds.** `deviation`: decided and permanent, the reason naming its
  row in `docs/Compliance.md` or a D-number, refused in an `include/`
  directory (a deviation in a public header is one every user adopts). `tool`:
  the code complies and the addon is wrong, the reason saying why. `backlog`:
  a real report no one has decided on, counted and printed, never growing.
  The gate does not enforce MISRA's classes (mandatory, required, advisory):
  the licensed rule list is not in the repository and a wrong number in a gate
  is a false claim; a reviewer checks them.
* **Ignored rules**: 2.3, 2.4, 2.5 and 8.7, rules about a whole program, judged
  by the integrator (`docs/Compliance.md`, "Rules about a whole program"). A
  fifth showing up after a cppcheck update is an `ignore` line with its
  reason, not a `backlog`.
* **The threshold is zero on the first run**: nothing is fixed, every report
  is recorded (6,337 in 613 lines at cppcheck 2.19), and each later card that
  takes a rule lowers its lines.
* **The gate fails open nowhere.** A canary (a file with a `goto`) must come
  back as 15.1, or the gate fails: without `--enable=style` or python the
  addon prints nothing, which looks exactly like a clean library. Without
  cppcheck the gate is skipped locally with a status line and fails when `CI`
  is `true`. The record names the cppcheck version (major.minor) its counts
  were made under; another version is a skip locally and a failure in CI, and
  the remedy is `-DWRITE` and a reviewed diff. No rule text and no rule
  category is committed (MISRA's texts are copyright), and a
  `cppcheck-suppress` of a MISRA rule under `lib/` fails the gate: the record
  is the only place. A `misra-config` note is counted and printed: "no
  violation" does not cover those places.

**Consequence.** The claim in `docs/Compliance.md` is true of what the addon
can see; the directives and the rules no tool checks stay on the review
checklist, and "follows" is not "compliant" until a qualified tool has run.
The matrix of every rule, the compliance report and the public headers' zero
are later cards.

**Status.** Built: N-19.

## D-113 — A unit on an int definition

**Question.** The `30s` and `512MiB` of a configuration are held without a new
`cf_type`, which would change the ABI.

**Decision.** `cf_def` gains `unit`, a pointer to an open `cf_unit` (D-222):
its name, its parser, its formatter and its own options; the built-in duration
and size are reached through `cf_unit_duration_get()` and `cf_unit_size_get()`
and a program defines its own; an int definition with a unit accepts the
unit's grammar and keeps the int in the unit's base; the range is in the base;
the provenance keeps the text as written; a typed source's string is converted
for such a key and for no other.

**Held to.** `lib/numeric/tests/test_numeric.c` for the grammar and
`lib/config/tests/test_units.c` for the contract.

**Choices.** A fraction is read only when whole in the base; a month and a year
are refused by default; a bare `K` is IEC by default; the suffixes of a
duration decrease. The two built-in units are functions, not data (the card's
own fallback, taken at once): a shared library exports data differently on each
platform (a DLL needs `dllimport`), and `gate.abi` lists functions. The grammar
is read exactly, in integer arithmetic (a number of 192 bits), so a fraction
needs no floating point and `minimal` builds the same tests. A bare number is
what `cf_text_to_i64` takes, so `+30` is 30 and `+30s` is refused. A month is
`mo`, a year `y`, and in ISO 8601 `P1M` and `P1Y`; with `calendar` off all four
say "is ambiguous: use days".

**Weighed and not taken.** A closed enum (the maintainer chose the open
struct), a new `cf_type` (ABI), rounding, locale-dependent spellings.

**Status.** Built: C-2a. The schema keyword, the report and `cf_emit` are
C-2b.

## D-222 — A unit is an open value: a name, a parser, a formatter and its own options (the maintainer, 2026-11-26)

The maintainer approved an open struct instead of a closed enum: "every unit is
a `cf_unit` value; a program may add its own unit with its own table of
abbreviations".

1. `cf_unit` is a struct: its name; `parse` (text to number, with a refusal
   reason); `format` (number to text, for `explain` and for writing); the
   unit's own default options (D-220). `cf_def` gains `const cf_unit *unit;` as
   its last field; NULL is no unit; a caller built before it gets NULL by the
   size rule (D-51, `DEF_SIZE_UNIT`).
2. Built-in values for the duration and the size; a program defines its own
   unit with its own table of abbreviations. The parser is a part usable alone
   (`cf_unit_parse`); configuration binds to it only through `cf_def.unit`.
3. Notation, in the maintainer's order (the languages first, then world
   standards, else explicit, with a table and the most telling abbreviation).
   Duration: Go's `time.ParseDuration` forms (`1h30m`, `300ms`, `1.5h`); C++
   `std::chrono`'s names `h`, `min`, `s`, `ms`, `us` and `µs`, `ns` (minutes as
   `m` or `min`); `d` and `w` as Rust's humantime; ISO 8601 `PT1H30M` also read.
   Months and years refused by default ("is ambiguous: use days"), allowed by
   an option. The base unit ms by default; an option for s or us. Size: IEC
   80000-13 `KiB`, `MiB`, `GiB` (1024^n) and SI `kB`, `MB` (1000^n); bare `K`,
   `M`, `G` read as IEC by default (as Kubernetes and the JVM read them), an
   option to change it. A bare number with no unit is the base unit by
   default, an option to refuse it.
4. Every abbreviation read is listed in a table in the documents
   (`docs/Guide.md`, "Durations and sizes").

**Status.** Built: C-2a (the unit and the field); C-2b (the schema, the report,
`cf_emit`).

## D-224 — The file layer: options, a writer, a replace, and an emit on top

**Question.** `examples/c/settings.c` writes a program's settings with
`fopen "wb"` over the old file: a crash or a full disk leaves neither the old
file nor the new one, and wave 6 (F-3) will write users' files. What does the
library offer for writing a file, and what does it let a program choose?

**Options.** A: one `cf_emit_file` that does everything, with the options of
a file on it. B: the file layer alone, the program calling `cf_emit` itself.
C: the layers, each usable without the one above (decided by the maintainer,
2026-11-26: "option C is better"; "do the design without departing from the
standards: the compiler's or the platform's rules must be usable").

**Decision.** Option C.

1. **One options struct**, `cf_file_options`, initialised by
   `cf_file_options_init(&o)` (C89, D-230's shape: `size` first); a NULL
   `options` anywhere is the defaults. Every setting is an option with a
   default (D-220).
2. **Three layers.** `cf_file_writer`: streaming, in place, for a program
   that writes continuously (records, logs). `cf_file_replace(path, data, size,
   options, err)`: a temporary file, then one atomic replace, usable alone and
   knowing nothing of configuration (D-127). `cf_emit_file(c, path, format,
   what, rank, options, err)` (F-1b): `cf_emit`, then `cf_file_replace`, with no
   logic of its own. The first two are in `lib/platform` (the platform is a
   part, D-100, D-220), in `cf_file.h`; the third is `cf_emit.h`'s.
3. **Two settings, independent, taken from the standards.** Buffering is
   C89's `setvbuf`: `CF_BUF_PLATFORM` (the default: `setvbuf` is not called),
   `CF_BUF_FULL` (`_IOFBF`), `CF_BUF_LINE` (`_IOLBF`), `CF_BUF_NONE`
   (`_IONBF`), and a `buffer_size` (0: `BUFSIZ`; above it, a buffer taken from
   `cf_default_allocator()` and released at the close). Durability is the
   platform's call (`fsync`, `FlushFileBuffers`): `CF_SYNC_PLATFORM` (nothing
   forced), `CF_SYNC_COMMIT` (once, before the file is closed; for a replace,
   before the rename), `CF_SYNC_EVERY` (after `sync_bytes` bytes or `sync_ms`
   milliseconds, whichever comes first, checked at each write; a zero trigger
   never fires; the tail is synced at the close), `CF_SYNC_EACH` (after every
   write, chosen knowingly).
4. **The defaults are the compiler's and the platform's, with one exception**
   the maintainer chose: `cf_file_replace`, and so `cf_emit_file`, defaults to
   `CF_SYNC_COMMIT` (one sync a save, so that "whole or not at all" holds
   across a power loss, as SQLite and Git do); `cf_file_writer` defaults to
   `CF_SYNC_PLATFORM`. **`CF_SYNC_DEFAULT`** is how that is written (it is not
   in the maintainer's list): it is the value `cf_file_options_init` sets and
   what NULL means, resolved by each layer to its own, so that NULL and an
   initialised struct are the same. A write that is one `fwrite` takes
   `CF_SYNC_EVERY` and `CF_SYNC_EACH` as `CF_SYNC_COMMIT`.
5. **The Windows retry of the rename is an option**, `retry_ms`, 2 s by default
   (D-127); `append` is the writer's.
6. **A clock for `sync_ms`**: `cf_platform_now_ms`, internal, a monotonic
   millisecond counter (`clock_gettime(CLOCK_MONOTONIC)`, `GetTickCount64`).
   Where there is none it returns `(unsigned long)-1` and a writer that asks for
   `CF_SYNC_EVERY` with `sync_ms` refuses by name (`CF_E_CAPABILITY`).
7. **The `none` platform** has no files to write: every call of the layer but
   `cf_file_options_init` fails with `CF_E_CAPABILITY`, `this build has no
   files to write`. The calls exist (D-220), so a program built for it links.

Rejected: one `cf_emit_file` with the file's options on it (a program that
writes records would have to go through configuration); the file layer alone,
leaving every program to join `cf_emit` to it; a default of `CF_SYNC_PLATFORM`
for the replace.

**Status.** Built: F-1a (the layer); F-1b (`cf_emit_file`).

## D-122 — A migration is a function on the deprecated key's definition, run on each value as it is copied

**Question.** `replaced_by` moves a value to a new name and converts
nothing. A file written for version 1 says `timeout_s = 30` and the key now
counts milliseconds. Where does the conversion live, and when does it run?

**Options.**

1. On the new key's definition. Rejected: a key renamed twice across
   versions has one conversion for each old name.
2. On the old key's definition, run on the winning value after resolution.
   Rejected: only the winner would be converted, and a lower source's value
   would reach the new key in the wrong unit.
3. On the old key's definition, run on every value of the old key as the
   rename copies it, before precedence chooses (decided).

**Decision.** Option 3. `cf_def.migrate` and `migrate_user`, for a
deprecated scalar key with `replaced_by`; `in` is the value as the old key
declares it, `out` the new key's type, a failure a reason. The copy's place
says `(from <old>, converted)`. A refusal fails the build with the old key
and the line, and never shows a sensitive value. A value that holds a
reference is refused, not passed on unconverted.

**Consequence.** `cf_def` grows by two members (D-51). Splitting one key
in two is not covered; it is the maintainer's to ask for.

## D-127 — A file is replaced whole or not at all

**Question.** How does `cf_file_replace` make a file hold exactly the bytes
given, so that a crash, a full disk or a reader at the wrong moment never
leaves half a file?

**Decision.** By a temporary file in the same directory, written, synced once
and put in place by one rename; and at every step that can fail, the old file
is intact and no temporary is left.

* **The core** is `cf_platform_write_file_with(path, data, size, options,
  fail_at, on_retry, user, err)`, internal, which `cf_file_replace` calls with
  `fail_at` 0 and no hook; each platform's `file.c` holds one static
  `file_core`. `cf_platform_write_steps()` says how many steps can fail
  (POSIX 6, Windows 5, none 0), and `fail_at` fails step *k* as if that system
  call had failed (`injected failure at step <k>`): no global state (D-101).
* **POSIX**, after a check of what is there (`lstat`: a symbolic link is
  refused, `'%s' is a symbolic link; write the file it points to`; what is not
  a regular file is refused; no file is fine): **1** `open` the temporary
  `<dir>/.<name>.cf-tmp-<k>` with `O_CREAT|O_EXCL` (and `O_CLOEXEC`, or
  `FD_CLOEXEC` after it, D-190), `k` from 0 to 99, the next on `EEXIST`; mode
  `0666` for a new file (the umask applies, as `fopen` gave) and `0600` for a
  replacement; **2** every byte, and `fflush`; **3** `fsync`; **4** `fchmod` to
  the original's mode; **5** `fclose`, its result checked; **6** `rename`. After
  the rename, not a step and only when the sync is not `CF_SYNC_PLATFORM`, the
  directory is opened, synced and closed, every failure ignored: the file is in
  place, and reporting a failure then would tell the caller that nothing changed.
* **Windows**, after `GetFileAttributesW` (a directory or a read-only file is
  refused): **1** `CreateFileW` the temporary with `CREATE_NEW` and no
  inheritance, into a stream; **2** write and `fflush`; **3** `FlushFileBuffers`;
  **4** `fclose`; **5** `MoveFileExW(REPLACE_EXISTING | WRITE_THROUGH)`, retried
  (as Go's `renameio` does) while it fails with `ERROR_ACCESS_DENIED` or
  `ERROR_SHARING_VIOLATION`, which a reader's open handle without delete
  sharing gives: the sleeps start at 10 ms, double to at most 200, and their
  sum is bound by `retry_ms` (2000 by default; a sum, no clock). Without it
  the library's own reload on one thread makes a save on another fail. No
  attribute or ACL is copied: the new file takes the directory's.
* **The temporary of a crash** is never read or removed by a later write: its
  name is taken and the next `k` is used (it may be another process's live one).
* **Errors** (D-223) name the place and the reason: `cannot write '%s': <why>`,
  the reason being `strerror(errno)`, or on Windows `error <GetLastError()>`;
  the checks before step 1 have their own wording.

Rejected: following a symbolic link (the rename would replace the link, and
writing through it needs `realpath`, which C89 and Windows do not share: the
caller names the target); a lock file by existence (one writer is the rule, D-128;
D-187 holds the kernel lock a program chooses); cleaning an old temporary (it may be
another process's live one); a fixed retry bound (D-220: it is `retry_ms`); a
default of `CF_SYNC_PLATFORM` for `cf_file_replace`.

**Status.** Built: F-1a. Held to: `test_writefile` fails each step.

## D-223 — One error model (the maintainer, 2026-11-26, relayed from a side session: "I like the error model"; "the core uses the C89 standard")

It answers K-6a's and K-6b's TO DECIDE and Y-28's TO DECIDE C, and K-9c
follows it, so that `cf_error` changes shape once. K-6a, K-6b and Y-28
are rewritten to it before they open; each carries it under
"Organiser's answers". The core stays C89 (D-72): a newer standard's
feature is only a macro that is empty under C89.

1. **Errors cannot be ignored.** Every public function that returns a
   failure (a `cf_bool` or a status; a create, open, try or write) is
   marked `CF_NODISCARD` (`cf_port.h` defines it: `[[nodiscard]]` under
   C23 and C++17, empty under C89; GCC's `warn_unused_result` is not
   used). Today 16 of about 139 carry it: K-9c marks the rest, adds a
   gate that refuses an unmarked one, and CI's C23 build treats the
   warning as an error.
2. **`cf_error` carries** its code, its place (source, file, line,
   column, key), a reason text and a chain of causes ("could not read,
   because permission denied"). A sensitive key's value never enters the
   text. A plain C89 struct; the chain lives in the arena or a linked
   list.
3. **Every format and component returns the same `cf_error`.**
4. **Options with defaults (D-220):** by default the first error stops
   the work and is returned; an option collects every error (the
   compiler's case, Y-28); an option `cf_error_handler`, a function
   pointer, for logging or an embedded indicator.
5. **Held to:** the `CF_NODISCARD` gate; cppcheck and clang-tidy's
   `bugprone-unused-return-value` on the library itself; K-9b's injected
   faults also check that each failure returns the right code.

## D-230 — Options with defaults are one convention: `size` first, `_init`, NULL the defaults; a fixed limit a public call reaches is an option with its old value as default

**Question.** D-220 says a limit is an option with a default value, never a
fixed number. One options struct was public (`cf_live_options`), with a
macro initializer and no function a binding can call, and the planned ones
(`cf_file_options`, `cf_number_options`, `cf_serve_options`) would each have
invented the shape again; two limits a public call reaches
(`CF_MAX_OPEN_PREFIXES`, `CF_MAX_SOURCES`) were fixed numbers.

**Decision.**

* **One shape**, written in `docs/Architecture.md`, "Options with defaults":
  `size` first (D-51); `cf_<x>_options_init(&o)` writes `size` and the
  defaults, a NULL `o` does nothing; a NULL `const cf_<x>_options*` is the
  defaults, except where the call cannot run without a member
  (`cf_live_open_with` needs `build`); a member's `0` means "unlimited" or
  "off" only where its comment says so; a struct that has a macro
  initializer keeps it. `gate.options` (`scripts/check/options.py`) refuses a
  public options struct that lacks it, and a `CF_MAX_*` macro with no line in
  `scripts/check/options.txt`.
* **`CF_MAX_OPEN_PREFIXES`** is the default of
  `cf_limit_options.max_open_prefixes` (`cf_builder_limits`; 0 is unlimited).
  The macro stays: a program may use it.
* **`CF_MAX_SOURCES`** is not an option: it sized two counters of one
  renderer, which are now filled a window of sources at a time, so the
  summary names every source; the macro is the window.
* **`CF_MAX_DEPTH` and the other build limits** stay build options (D-37);
  the reader options of modules 3 and 6 add `max_depth` when they add the
  struct.

Rejected: renaming `CF_MAX_OPEN_PREFIXES`; making NULL the defaults for
`cf_live_open_with`; a per-call `max_sources`, which would make an
explanation depend on a number nobody should choose; one options struct for
every limit of every call.

**Status.** Built: K-11a.

## D-186 — A write checks that the file is still what it was read from

**Question.** Two programs that read, change and replace one file lose one
update, silently. What does the replace do about it?

**Decision.** The check is of bytes, after the temporary is complete and just
before the rename (`cf_platform_write_file_if`, internal): the file must still
hold exactly what the caller read (or be absent, when it read none). A mismatch
returns -1 with `'%s' changed while it was written`, and the file stays as the
other writer left it. The caller applies its change again to the new bytes up to
`write_attempts` times (an option of the source, 3 by default, D-227) and then
refuses with `'%s' changed while it was written; try again`. No option, no new
public name, no file: the race lasts from a re-read to a rename, microseconds,
and a change wins key by key, not file by file.

Rejected: a stamp (a hint that can miss a rewrite of the same length inside one
clock tick, as `cf_changed` can); unbounded retries (a file two programs rewrite
forever never settles; the third refusal is the answer); a lock for this (Y-3 is
for the program that asks for one).

Held to: `test_writefile` cases (l) to (q).

**Status.** Built: Y-2 (the check; `overrides_write` uses it in F-2a).

## D-187 — A write may take a lock that plugs in: none by default, a kernel lock on a kept file when the program chooses it

**Question.** Should a write be able to exclude another writer at all, and with
what?

**Options.** No lock, the optimistic check only (D-186, what the default does);
a lock by existence; a kernel lock on a kept sidecar file; a locker the program
fills in.

**Decision (D-227).** `cf_locker` (cf_lock.h): `take(path, wait_ms, err)` and
`give`, a part that plugs in as `cf_packer` does. `cf_lock_none()` is the
default and takes nothing; `cf_lock_sidecar()` takes an exclusive kernel lock
on `<dir>/.<base>.cf-lock` beside the file; a program may fill its own (a
service it asks, a database lock, an embedded mutex). The call per platform,
one protocol chosen when the library is built and never two: `flock` where
`LOCK_EX` exists, else `F_OFD_SETLK`, else classic `F_SETLK` (whose two faults:
closing any descriptor of the file drops the process's locks, and threads of
one process do not exclude each other), and `LockFileEx` on Windows. The
sidecar is opened read-write with the file's own mode (`0666` when it does not
exist), never followed through a link, kept and never removed, and released by
a crash. The wait is a loop of non-blocking attempts with sleeps of 1, 2, 4 ...
ms (at most 100 each), bounded by the `wait_ms` the program gives (0: one
attempt). A lock that is held is `CF_E_BUSY` (`'%s' is busy: another writer
holds its lock; try again`), distinct from "cannot lock" (`CF_E_SOURCE`). Where
no call exists (WASI, `none`) the sidecar's `take` fails `CF_E_CAPABILITY`.

Rejected: a lock by existence (a stale lock after every crash, and D-127
refuses to clean one); locking the configuration file itself (a Windows reader
would be refused and the rename would fail over a locked file); a lock on the
read path or inside `cf_config` (D-14 holds, and with writers that never write
in place a reader always opens a whole file); a writer service; an unbounded
wait.

Held to: `test_lock`.

**Status.** Built: Y-3 (the lockers; Y-4 chooses one by `cf_file_source_options.lock`).

---
## D-189 (Y-6 part) — Windows readers that share delete access: measured, a rename over one still fails

Option R5 of `docs/research/concurrent-access.md` (open reads with `FILE_SHARE_DELETE`) was measured on 2026-11-27, Windows 11 build 10.0.26300.10075, `cl` of Visual Studio 18: `MoveFileExW(MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` over a file held open by `CreateFileW` with read, write and delete sharing fails with `ERROR_ACCESS_DENIED` (5), exactly as over a `_wfopen` reader (the control, also 5); with no reader open it succeeds. Rejected: `cf_platform_fopen` is unchanged. The `retry_ms` wait of F-1a stays the answer. The rest of D-189 is Y-5's:

**Question.** A reader of a file that something else (an editor, a deployment tool) is writing in place can read half of it, and no lock can stop that, because the writer takes none. What does the reader do?

**Options.** Writers that never write in place (F-1a, already the library's); a stable read; a shared lock on every read (rejected: it buys nothing, since library writers never write in place and other writers take no lock, and it would make a reader wait for a writer, which D-14 refuses on the read path); readers that share delete access (Y-6, measured and rejected above).

**Decision.** The stable read. The file source takes the file's stamp (`mtime`, `size`, presence), reads, and takes it again; when it moved it reads again, up to `read_attempts` reads in all (an option, 3 by default, D-220), then fails as an unreadable file does (`'%s' changed while it was read`), which `cf_live` already survives: a reload keeps the running snapshot. The code is `cf_file_read_stable` (internal), called by the file source; `cf_blob_file` is unchanged. It costs one `stat` more per file read and adds no public name. The stamp is a hint, as `cf_changed`'s is (a rewrite of the same length inside one clock tick passes it), so this catches most in-place writers, not all. A platform that cannot stamp reads once.

Held to: `test_stable`.

---

## D-190 (Y-8) — Every descriptor the library keeps is close-on-exec

**Question.** A child started by another thread while the library has a file open inherits the descriptor, and a secret file read by a library should not reach a stranger's child.

**Decision.** `cf_platform_fopen` on POSIX is `open` then `fdopen`, the mode string mapped as C says (`r`, `w`, `a`, a `+` for read and write, `b` ignored, anything else refused), close-on-exec from the open (`O_CLOEXEC`, or `fcntl` with `FD_CLOEXEC` right after where the flag is not declared: a window of two calls remains, and the file says so). On Windows the mode gets an `N` (`_wfopen`), so the C runtime opens it not inheritable. `cf_platform_create_private` opens with `O_CLOEXEC` or the `fcntl` form. The `kqueue` descriptor is marked with `fcntl` after `kqueue()`. F-1a's writer and temporary and Y-3's sidecar lock are born close-on-exec; the watcher's `inotify` descriptor and the files it watches already were. A directory scan is not changed: the C library's `opendir` marks its descriptor and the scan closes it before it returns.

**Rejected.** `fopen` with `e` (a mode string the C standard does not know, and new to the macOS C library); `O_CLOEXEC` alone where `fcntl` would do (a platform without the flag gets the two-call form, not a refusal).

Held to: `test_cloexec`: a child that lists its descriptors finds none of a `cf_platform_fopen` file, a watcher, a file writer and a sidecar lock, and finds a control opened with plain `fopen`.

---

## D-231 — The platform is a part: `none` is built and tried; what it cannot give answers `CF_E_CAPABILITY`

**Question.** D-220 says a feature the target platform cannot provide is
either not compiled, its use a link error, or `CF_E_CAPABILITY` where the
call must exist. The `none` platform existed and nothing built it: `auto`
chose `posix` for a bare-metal toolchain, `CF_NO_STDIO` and `CF_NO_STDLIB`
were macros no build option set, and no test ran on `none`.

**Decision.**

* **`auto` knows a bare-metal system**: `CMAKE_SYSTEM_NAME` `Generic` is
  `none`; WASI has a C library with files and stays `posix`. The Cortex-M job
  also says `-DCF_PLATFORM=none`.
* **Two options for a C library that is not there**: `CF_NO_STDIO` (files
  cannot be opened) and `CF_NO_STDLIB` (no default allocator), each a public
  compile definition, both refused unless the platform is `none`.
* **`cf_platform_has_files`** is the query for files, like the other `has_`
  queries, and `cf_blob_file` asks it before it opens: a build with no files
  answers `CF_E_CAPABILITY` ("this build has no files to read"), not
  `CF_E_SOURCE` "cannot open".
* **Built and tried**: the preset `none` (the host's C library for newlib,
  the whole suite; a test that needs what `none` cannot give asks the
  platform and skips) and the preset `none-bare` (`CF_NO_STDIO` and
  `CF_NO_STDLIB`, one test, `tests/platform/freestanding.c`), in one CI job.
* **The allocator** stays as it was: `cf_default_allocator()` is NULL under
  `CF_NO_STDLIB`, a call with an error channel answers `CF_E_CAPABILITY`,
  `cf_build(NULL)` returns NULL, and `cf_build_in` is the path.
* **One table** of each feature against each platform, in
  `docs/Architecture.md`, generated from `docs/platforms.txt`; `gate.platforms`
  refuses a platform function or query with no row or no definition on a
  platform. The table is the document D-220 asks for.

Rejected: a CI build with `-ffreestanding` and no host C library (the
Cortex-M cross build is that proof); excluding tests by a regex in the
`none` preset; failing the configure for a `Generic` system without
`CF_PLATFORM`; a link error for the file functions on `none`.

**Status.** Built: K-11b, K-11c.

## D-232 — Every public function states one failure guarantee; the library never ends the program

**Question.** A library inside a controller or a daemon must say what is left
when a call fails, and must never decide to stop the process (the maintainer,
2026-11-26, "exception safety is to be added"). No header said it, and the
second rule held by luck only.

**Decision.** Three words, written as one line `Guarantee: <word>.` in the
comment above each public declaration (or above a run of declarations with no
blank line between them): `nothrow` (cannot fail), `strong` (on failure the
arguments are as they were; a `cf_error` and the caller's output buffers are
not counted) and `basic` (on failure nothing leaks and every object stays
valid, its contents may have changed, and the comment says which). A function
that can leave an object invalid or leak is a defect, never a fourth word.
`scripts/check/guarantees.py` (`gate.guarantees`) holds that each public name
(`cmake/configforge.symbols`, experimental included) has a line, that no
`CF_NODISCARD` function claims `nothrow`, that the table in
`docs/Guarantees.md` is what the headers say, and that no file of `lib/*/src`
or `lib/*/include` calls `abort`, `exit`, `_Exit`, `quick_exit`, `longjmp`,
`siglongjmp`, `setjmp`, `sigsetjmp`, `assert`, `raise` or `__builtin_trap`
(exceptions: `scripts/check/nonlocal.txt`, empty). Every public name has its
line since K-9g; the list of names not yet annotated is gone, and a new
public name without a line is refused. An allocation that fails returns `CF_E_MEMORY` (held at run time by
K-9b).

Rejected: a macro on each declaration (it changes every line and the compiler
checks nothing by it); the words in prose only (a promise nothing reads
drifts); a fourth word for "may leave the object unusable" (it is a defect);
guarantees for the internal names (outside the promise); one word per header.

**Status.** Built: K-9a.

## D-233 — MC/DC is measured with GCC and held by a floor

**Question.** The tool that measures MC/DC (which DO-178C level A and ISO 26262
ASIL D ask for), and what is held.

**Options.** (1) GCC 14 or later, `-fcondition-coverage` (masking MC/DC), read
from `gcov`'s JSON; (2) Clang 18 or later, `-fcoverage-mcdc`, read from
`llvm-cov`; (3) both.

**Decision.** Option 1. GCC is already the coverage compiler (`--coverage`,
`CF_COVERAGE`), `ubuntu-26.04` and the development distro carry GCC 15, and one
number from one tool is what a ratchet needs. Clang's MC/DC limits a decision to
six conditions and would be a second report to agree with. The build is the
`mcdc` preset (`CF_COVERAGE`, `CF_COVERAGE_MCDC`; configuring it under another
compiler, or under GCC older than 14, is an error). `scripts/check/mcdc.py`
runs `gcov --json-format --stdout --conditions` on each `.gcda` and sums
`lines[].conditions[]`: `count` outcomes, `covered` of them covered, in
`lib/*/src/`, a file built into two targets merged by the larger `covered`.
Held: a floor `MCDC_MIN` in `scripts/verify.cmake` (`-DEXTRA=mcdc`), the
percent of condition outcomes covered, set to the measured value rounded down
to a whole percent, raised by any card that adds tests, never lowered without
the maintainer. 100% is not claimed: the per-file report
(`build-mcdc/mcdc.txt`, CI's `mcdc-report` artifact) shows where it is not.

Rejected: Clang's report (a second number); object-code coverage, the extra
that level A asks of the object code (outside this tool).

**Tests.** `check.mcdc`.

**Status.** Built: L-25.

## D-236 — The CERT C matrix is generated, and says what no tool sees

**Question.** Which rules of the SEI CERT C Coding Standard is the library
held to, and by what? clang-tidy's `cert-*` checks run as errors
(`.clang-tidy`), but only some rules have a check; a rule no tool sees looks
the same as a rule held, unless a page says which is which.

**Decision.** `docs/compliance/Cert.md` is generated from one list,
`docs/compliance/cert.txt`: a line a rule, `<rule id> | <checked by> |
<note>`, the id alone (`ARR30-C`) and never the standard's title, which is the
publisher's text (D-152 decides the same for MISRA). Checked by is one or more
of `clang-tidy <check>`, `cppcheck <id>`, `compiler <flag>`, `cbmc`, `eva`,
`design D-<n>`, `review` (the note says what a person reads at a release) and
`n/a` (the library uses no such facility; the note starts with the `grep` that
shows it, and prints nothing). The list is built from `clang-tidy
--list-checks -checks=-*,cert-*` and the standard's public rule index, the C rules
only (122 of them), never from memory. `scripts/generate/cert_matrix.cmake`
writes the page and refuses an empty field, an unknown kind, a duplicate id,
an id not ending in `-C`, and an `n/a` row with no grep; `gate.cert`
regenerates and compares, and when clang-tidy is on the path checks that each
`clang-tidy` cell is a check `clang-tidy --list-checks` lists in the
repository's own `.clang-tidy`, so a check the file disables or a name that
does not exist fails.

Rejected: a page by hand (it drifts, as the limits did); nothing beyond
`.clang-tidy` (a rule no tool sees would look held). The recommendations of the
standard are a card of their own; a defect found while writing a row is a card
of its own too: the matrix records two (`strerror`'s shared buffer, CON33-C,
and `cf_block`'s `char data[1]`, DCL38-C).

**Tests.** `check.cert`, `gate.cert`.

**Status.** Built: L-28.

## D-237 — Out of memory returns `CF_E_MEMORY` on every path, proved by an exhaustive allocation sweep

**Question.** D-223 point 5 asks that each failure return the right code, and
D-232 that a failed call leave a stated state. Nothing proved either for memory
that runs out: `test_oom.c` swept one builder run, the mapped store and the
write, and no codec on its own, the edit, the image, the live reload, the file
layer or `cf_derive`.

**Decision.** One failing allocator and one sweep, `tests/support/cf_oom.h`
(SQLite's test allocator): a case is a function that does the work through an
allocator it is given; the sweep runs it once with everything allowed, which
counts the injection points, then once for each with that allocation and every
one after it refused. Each failure must be `CF_E_MEMORY` with a message and
leave no block outstanding (the allocator's live count, and LeakSanitizer in the
`sanitize` preset); the run that is allowed everything must give the same digest
as the first. A `strong` function's case also checks the state is as it was.
`scripts/check/oom.py` (`gate.oom`) refuses a public load or write name
(`_create`, `_load`, `_decode`, `_write`, `_commit`, `_replace`, `_derive`,
`_reload`, `_emit`, `_open`, `_save`; guarantee `strong` or `basic`) that has no
line in `lib/config/tests/oom_paths.txt`: a case, an excuse (it allocates
nothing), or `known:` for a defect not yet fixed (2.0.0 does not release while
one remains).

Two places took the default allocator and not the caller's, and the sweep could
not reach them: `cf_edit_begin` now takes the allocator of the configuration it
edits, and `cf_file_writer_open` takes `cf_file_options.allocator` (appended;
NULL is `cf_default_allocator()`, as before). What the sweep found and fixed: a
source or a document that ran out of room was reported as `CF_E_SOURCE`
("could not be read", "key is not valid"); the arena now remembers a refusal
(`cfi_arena_refused`) and the loader reports `CF_E_MEMORY`.

Rejected: a hook in `cf_default_allocator()` or a linker interposer (a global
mutable seam in the library, D-101); injecting I/O failures (a different card);
a random schedule (the sweep is exhaustive and so reproducible).

**Compatibility.** None for a program that passes no allocator; a program that
built a configuration with its own now has the edit's memory from it too.

**Status.** Built: K-9b.

## D-238 — Thread, signal and allocation claims beside the guarantee, the weakest by default

**Question.** "There are many safety notions: thread-safe, signal-safe,
exception-safety ... which of them can the library provide?" (the maintainer,
2026-11-27). Failure safety was held (D-232), memory (D-237), stack and
hostile input by gates, and the coarse thread contract was written (D-14,
`test_threads`); a claim per function about threads, signal handlers,
interrupts and real-time use was missing.

**Decision.** Three lines may stand beside `Guarantee:` in a public
declaration's comment, each a word and a full stop: `Thread:` `safe` (any
number of threads on the same objects, the readers of a built `cf_config`),
`distinct objects` or `unsafe`; `Signal:` `safe` or `unsafe`; `Alloc:` `none`
or `may`. A function with no such line is `distinct objects`, `unsafe`, `may`,
so no name needs a line to be honest and one is written only where it is
proved. Interrupt and real-time claims follow from `Signal: safe` with
`Alloc: none` and `gate.stack`, and have no words of their own.
`scripts/check/guarantees.py` (`gate.guarantees`) refuses a word it does not
know, and holds `Signal: safe` and `Alloc: none` against the compiler's call
data (GCC 10+, `-fcallgraph-info`, written beside the objects of `configforge`
and `configforge_serialize`): a reached allocator (`malloc` and kin, the arena
allocators) or a call through a pointer is refused for both, and stdio, locks
and the names of `scripts/check/notsignal.txt` for `Signal: safe`, each naming
the path. A call through a pointer is refused because the library's allocator
is one (`cf_error_clear` releases an owned block through it, so it claims
neither word). Without call data (another compiler, `CF_LTO`) the check says
it skipped and passes. `docs/Guarantees.md` lists the three columns.

Rejected: a line on every name (three times K-9d to K-9g's work for the same
truth); a per-function annotation macro (the compiler checks nothing by it);
trusting an indirect call (it hides the allocator); `Thread: safe` proved by
the call data (a race needs a race detector; `test_threads` holds the readers).

**Status.** Built: K-9h.

## D-157 — The Cortex-M numbers come from a cross build of the library alone

The Cortex-M build is held to numbers although no device or emulator runs the
tests. Three ways were weighed: (1) build and run under qemu-system-arm; (2)
cross-build the static library only (no linking of a program) and gate its
sizes and frames; (3) wait for hardware.

**Decision.** Option 2. The numbers the Roadmap asks for (`.text`, stack) are
properties of the compiled objects, and a library that is not linked needs no
board support package. `cmake/toolchains/arm-none-eabi.cmake` builds for a
Cortex-M4, soft float (a Cortex-M0 or M3 has no FPU, so the numbers are the
pessimistic ones), with the `none` platform and one thread; the CI job
`cortex-m` builds the `minimal` profile and fails unless the build printed
`stack: largest frame <n> bytes (<function>), within 1024`. The target's status
in `docs/targets.txt` is "built only", never "tested".

Rejected: option 1 (a board model and semihosting to maintain for no number
the build does not already give) and option 3 (L-19-m records hardware when it
exists).

**Compatibility.** None.

**Status.** Built: L-18a.

## D-60 — Arrays in INI are the ones INI files already have

**Question.** INI had no arrays here: a configuration with one could not
be written as INI, and a file with `ports[] = 80` was refused. C-style
arrays were wanted, and every format follows its own standard. What is
INI's array?

**Options.**

1. None: refuse, as before.
2. A brace list, `ports = {80, 443}` - C's initialiser.
3. PHP's `parse_ini_file`: `ports[] = 80` appends, `ports[1] = 8443` sets
   an element.
4. Repeated keys, as systemd does.

**Decision.** Option 3. INI has no formal standard; of the readers that
have arrays, PHP's is the one most INI files with arrays are written
for, and its `name[i]` is C's element assignment. Option 2 was built
first and dropped: no INI reader knows it, so it would have been a
syntax made up here - exactly what following each format's standard
rules out. Option 4 cannot be told from a mistake: a key set twice is an
error elsewhere in this library (CFD001 reports one).

Underneath, both forms are the flat `name.i` and `name.count` every other
format's array is, so explain(), reports and Doctor see no difference.
The `name[]` lines of one array go together; a second run of them later
in the section would restart the count and overwrite the first, so it is
refused with both lines named. The reader stays allocation-free: it
finds an earlier run by looking back at the bytes it already holds.

**What INI cannot hold** - an array of tables, an array of arrays, an
empty array - has no PHP form either, and `cf_emit` refuses it by name
before writing anything (`CF_CODEC_SCALAR_ARRAYS`, beside
`CF_CODEC_ARRAYS`). A table is a section, as before. A value in braces is
text, as it always was.

**Compatibility.** Additive: a key with `[` in it was refused before,
and no file that read before reads differently.

---

## D-62 — Each reader held to its format's standard, by the standard's suite

**Question.** The readers were written to subsets (D-23) and tested
against cases written here. Every format should follow its own
standard, starting from the best-known practice and improving from
there. How is "follows the standard" decided, and is a reader written
here still the right one?

**Method.** The practice the mature implementations share: the
specification's grammar is what a reader implements (RFC 8259, the TOML
1.0.0 ABNF); the standard's own suite is the bar (JSONTestSuite,
toml-test), every valid document accepted and every invalid one refused;
a floor that only rises (`tests/conformance/floor.txt`, `verify
-DEXTRA=conformance`, docs/Conformance.md); and differential tests and
fuzzing for what a suite does not reach. Measured first, the JSON reader
accepted numbers RFC 8259 forbids (`01`, `1.`, `-`); fixed, it scores
95/95 and 188/188. TOML refused 47 valid documents and accepted 115
invalid ones; the number grammar, the define-once rules below, and the
rules for control characters and UTF-8 took the invalid ones accepted
from 115 to none.

**Keep the readers written here, or adopt one?** For TOML the candidate
was tomlc17, which passes toml-test in full. It builds a tree with no
positions in it, and a value whose file, line and column explain() cannot
name is the one thing this library exists not to produce; it also
allocates, which the fixed-arena profile forbids. So the reader here is
completed against the suite instead, and the dependency contract below
stays the way to swap one in later should that change. For YAML, whose
parser was always a dependency, the measured comparison of libyaml (YAML
1.1) and libfyaml (1.2) is below.

**Values without a type here.** TOML has dates, times, `inf` and `nan`;
the library has no date type and keeps every real finite, since a range
check means nothing against NaN. Refusing them refused valid TOML;
adding a date type would be a 2.0 change to every binding. So each is
checked against its grammar - a date against its month and leap years,
a time with seconds, an offset in range - and read as its text, as
written and untyped, the way an INI value is: a definition decides, and
one that declares a real refuses `inf` by name. toml-test then reads all
208 valid documents to the values it states.

**The dependency contract.** A format is a `cf_codec`; a reader is an
event producer into a `cf_doc_sink`, whatever implements it. A bundled
library is behind its codec and nothing else sees it, so replacing one is
a change to one file and its CMake option, and the suite says whether the
replacement is better.

**Define-once, without the reader owning memory.** TOML forbids defining
a key or a table twice, and an event reader cannot ask its sink what was
defined. The reader keeps a record of definitions - a tree of names
under a hash, O(1) a key - in memory the sink lends through a new,
optional `cf_doc_sink.reserve_work` (D-51 sizing: a sink compiled before
it never has it called). The loader lends it from the configuration's
arena, so a fixed arena bounds it with everything else; a sink without
it reads as 1.1 did, the later key winning, rather than refusing
documents it used to accept.

**YAML, measured.** On yaml-test-suite (its data branch, pinned), the
libyaml reader accepted 100 of the 133 valid documents a configuration
can hold, refused 87 of 94 invalid ones, and read 90 of 93 to the values
the suite states; 175 more valid documents are refused by the
configuration model, by name (D-39, D-63), and counted apart. Every one
of libyaml's misses is its parser's - YAML 1.1, unmaintained for years:
`?foo` as a plain scalar, a literal block's last line of spaces, a `:`
in an anchor's name. libfyaml 0.8, on the same suite, accepts 308/308,
refuses 94/94 and writes the suite's event stream exactly for 308/308.
It is MIT-licensed, but has no MSVC support, where libyaml is
everywhere.

So a libfyaml backend was written behind the same codec - the event
mapping shared, the parser's half of yaml.c the only difference - and
through it the reader scored 122/122 valid, 94/94 invalid and 110/110
values. Then the unit fuzzer hung: a 149-byte input of random bytes
beginning with `%` sends libfyaml 0.8's `fy_parser_parse` into a loop it
never leaves, and libfyaml's own `fy-testsuite` hangs on it too. It is a
known bug - libfyaml issue 101, malformed UTF-8 in a directive, fixed by
7f7be36 and first released in 0.9.1 (December 2025) - that Ubuntu 24.04
still ships.

**So libfyaml, from 0.9.1, is the YAML parser where it builds.** The
backend is `CF_YAML_BACKEND`: `auto` takes libfyaml when one of 0.9.1 or
later is installed and libyaml otherwise; `libfyaml` and `libyaml` name
one. The least version, 0.9.1, is enforced rather than noted, in three
places from one value in `src/serialize/CMakeLists.txt`: the build reads
the version from the pkg-config file beside the library (the header
carries none) and refuses an older libfyaml, or one whose version it
cannot read, by name; `yaml.c` asks `fy_library_version()` before each
document, because 0.8 and 0.9 share the soname `libfyaml.so.0` and a
program built against 0.9.6 loads Ubuntu's 0.8 just as well - checked:
that program, given 0.8, refuses with the version it found instead of
hanging; and `configforge.pc` carries `Requires.private: libfyaml >=
0.9.1`, so pkg-config refuses an older one to a static consumer. With 0.9.6 the reader scores
122/122 valid, 94/94 invalid and 110/110 values, the reproducer is
refused (`tests/conformance/data/libfyaml-0.8-hang.yaml.bin`, read by
test_yaml under a timeout), and the fuzzer passes. libyaml stays the
backend for MSVC and for a C89-only compiler: libfyaml's header needs
C11, so yaml.c alone is compiled as C11 in a libfyaml build, its header
included as a system one (this library's warnings are for its own code).
Ubuntu 26.04 ships 0.9.4, which setup.sh and CI's libfyaml job install
since the move to 26.04 (before it, a script built 0.9.6 from its
checked release tarball); verify scores both backends against their
floors, and skips libfyaml with the build's own reason where it refuses
the one installed.

Making the C99 build work found two defects in libfyaml's pre-C11
fallback for atomics, still in 1.0.0-beta2 and master: the guard reads
`__STDC__NO_ATOMICS__` for C11's `__STDC_NO_ATOMICS__`, so pcc includes
a `<stdatomic.h>` it does not have, and `atomic_compare_exchange_strong`
assigns `*__res` to a `bool`, which tcc refuses. Both reproduced with
tcc and pcc, fixed by one line each, and reported upstream as libfyaml
issue 506. The contract held throughout: the parser's half of yaml.c and
its build option are the whole difference.

**Compatibility.** A TOML document that defines something twice was
read before, the later key winning, and is refused now. TOML always
called such a document invalid; this library documented reading it as a
departure, in toml.c, and the departure is withdrawn. Of D-56's terms it
is closest to a refusal of input documented as wrong, and since a file
that loaded under 1.1 can stop loading, the changelog names it among the
changes to look at before upgrading.

---

One decision is open, because nothing has been built: profiles as a core concept
(D-12). Value interpolation (D-10), long reserved, is decided and built. Everything else is decided,
and where the implementation settled the question rather than this document, the section
says so and names what settled it.

That audit was worth doing on its own. Fourteen entries were still marked open while the
code they described had been written, tested and shipped — and a status column where
half the entries are wrong teaches a reader not to trust the column.

---

## D-175 — Sizing is a mode of the builder

`MANUAL` is the build's limits, as before, and the only mode of a `minimal` build; `SEMI` grows a buffer from the allocator up to the program's cap; `DYNAMIC` has no cap but memory. The mode is stored with the definitions and kept by a derived configuration. What each mode lets grow is D-176.

---

## Closing Phase 0

Phase 0 ends when every entry above reads `Decided` or `Deferred` with a written
reason, and the specification has been updated to match.

At that point the specification is frozen. Any new section afterwards has to be paid
for with code.

---

## D-176 — Sizing grows what the allocator holds, and `cf_error` owns its text

Under `SEMI` and `DYNAMIC` the readers' scratch and the key space's keys follow the mode's cap; `cf_error`'s texts are owned and freed by `cf_error_clear`, so a long message round-trips. `MANUAL` is unchanged. Rejected: bigger stack buffers, a second error struct, arrays kept beside a long-message pointer.

M-11a's part: the loader tells a reader how long a key it may build through a new last member of `cf_doc_sink`, `key_limit` (0, the zeroed default, is the build's `CF_KEY_MAX - 1`, so a sink of one's own, and `MANUAL`, read as before); the JSON, TOML, INI and YAML readers grow their key buffers from `reserve_work`, doubled, and refuse a key past the limit with the reason "budget". The key space and the loader check a key with `cfi_key_valid_n(key, len, limit)`; the public `cf_key_valid` keeps the build's limit. A cap below the build's key limit leaves the build's, so a mode never refuses what `MANUAL` takes; a value is held to the cap.

M-11b's part: `cf_error`'s `key`, `source` and `message` are `const char*`, "" when unset (NULL only in a struct of zeros before its first use). A text that fits the old arrays lives in an inline store (`cfi_key`, `cfi_source`, `cfi_message`, in the place the arrays had, so that they stay 1.0's region and are written whatever `size` says); a longer one goes to one block each from `cf_default_allocator()`, held in `cfi_heap`, the one member added behind `offset`: an error whose `size` does not reach it (the Rust and Python bindings' views, which end at `reason`) keeps every text inline and cut, as before. `cf_error_clear` frees the blocks; `cf_error_copy` copies a text into the destination's own store, so a copy never points into its source; a build with `CF_FIXED_LIMITS`, a target with no allocator and a failed allocation cut the text at the old array's size. `cf_error_set` is unchanged (key at `CF_KEY_MAX - 1`, source at `CF_SOURCE_MAX - 1`, message at `CF_MESSAGE_MAX - 1`); `cf_error_set_long(e, code, key, source, cap, fmt, ...)` allows each text `cap` bytes, 0 for none. The key space's list-append errors and the emitter's key-bearing ones call it under `SEMI` and `DYNAMIC` and keep `cf_error_set` under `MANUAL`. A `cf_error` is not copied by value (a cleared one may be, once, to start another: its texts are literals); the C++ `Error` has a copy constructor, an assignment and a destructor for it. The builder, the live reader and the key space's refusal release or avoid a block an arena would not.
---

## D-177 — Image major 2: a 128-byte header with the build's limits and id

The 12 reserved bytes could not hold them; offsets stay 32-bit; a major 1 image loads (`build_id` NULL); a major 2 image from a build with longer keys is refused naming both limits. Rejected: 64-bit offsets, a section table. Bytes 104-111 are kept for the manifest.

## D-178 — 2.0 breaks once, and says so

The breaks are those D-176 and D-177 name, listed in `docs/Migrating-2.0.md` with before and after; `gate.abi`'s baseline is the 2.0 interface; nothing else breaks in 2.0.

---

## D-239 — The library writes a system error's text from its own table

**Question.** `strerror` may return a buffer shared by every thread (CERT
CON33-C), and D-14 lets two threads load distinct configurations at once;
which text does the library put in an error?

**Decision.** `cfi_errno_text(code, buf, cap)` in `lib/platform/src/errno_text.c`:
a fixed English table for the codes the library meets (`ENOENT`, `EACCES`,
`EEXIST`, `ENOTDIR`, `EISDIR`, `ENOSPC`, `EROFS`, `EMFILE`, `ENAMETOOLONG`,
`EINVAL`, `EIO`, `ENOMEM`, `EPERM`, `EBUSY`, `EXDEV`, `ELOOP`, each under
`#ifdef` of its name), `error <n>` for any other. It holds no state, lock or
allocator, so it is `Thread: safe`, `Signal: safe`, `Alloc: none`. A message
is the same on every platform and in every locale, which a test can compare.

Rejected: `strerror_r` with feature macros (two signatures, XSI and GNU, to
test on every libc); a lock around `strerror` (a lock in a reader).

**Status.** Built: K-9k.

## D-121 — A read that finds no definition, or another type, is recorded and reported

**Question.** `cf_get_int(cfg, "server.prot", 8080)` returns 8080. Reading
is infallible (Design.md, "One Error Site"), so nothing tells the program,
the operator or Doctor that the key is a typo or has another type. How is
it noticed without making a read fail, take a lock or log from any thread?

**Options.**

1. Make a read of an undefined key an error. Rejected: it breaks the rule
   that reads do not fail, and every program that reads a key its
   contract leaves to a default.
2. Log it. Rejected: a read takes no lock and may be on any thread
   (D-14), and a log line a read at a time is the noise a report avoids.
3. Record it in a small fixed table beside the read coverage, opt-in, and
   say it in the report (decided).

**Decision.** Option 3: `cf_builder_track_misreads`, `cf_misreads` and
`cf_misread_at`, and a `"misreads"` array in the report. A misread is a
typed read of a key the contract does not define (and is not under an
open prefix), or whose value, or definition when it has no value, has
another type; `cf_has` and `cf_type_of` are probes, not reads. Sixteen
distinct (key, kind, wanted) entries, claimed without a lock and
published when whole; the rest are lost. No value is recorded.

**Consequence.** Doctor's CFD021 (C-8b) reads the array.

**Status.** Built: C-8a.
