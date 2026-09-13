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
| D-03 | Snapshot ownership and string lifetime | ABI, hot path, watch | Open |
| D-04 | Definition representation in C | ABI, builder ergonomics | Open |
| D-05 | Validation failure behaviour | resolver, error model | Open |
| D-06 | Key naming, case and normalisation | mapping, every source | **Decided**, closed |
| D-07 | Allocation and freestanding model | core, all constrained targets | Open |
| D-08 | Error model shape | ABI, every API | Open |
| D-09 | Zero-config entry point | public API, principles | Open |
| D-10 | Value interpolation | resolver, provenance | Open |
| D-11 | Unknown-key behaviour | resolver, diagnostics | Open |
| D-12 | Profiles as a core concept | policy model, Doctor | Open |
| D-13 | Sensitivity and redaction | diagnostics, report artifact | Open |
| D-14 | Thread-safety contract | ABI, snapshot | Open |
| D-15 | Text encoding boundary | platform providers, ABI | Open |
| D-16 | Provenance report schema | Doctor, diagnostics, Report Replay | **Decided** |
| D-17 | Reference parser and dependency stance | build, portability | Open |
| D-18 | Project name availability | everything public | **Decided** |
| D-19 | **Implementation language and standard** | every other decision | **Decided** |
| D-20 | Location of the error surface | every read call site | Open |
| D-21 | Distribution form | adoption, build | Open |
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
| TOML | **deferred**, decide in Phase 5 | full TOML 1.0 is 2,500+ lines with dates, multiline and literal strings, inline tables and arrays of tables. `tomlc99` and its relatives are C99 and DOM-based, so vendoring means a C89 fork plus a DOM-to-events shim. A documented *subset* — no datetimes, no multiline — is about 800 lines and covers most hand-written configuration. Both routes stay open because the codec seam does not care. |
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
| TOML, XML, SQLite | deferred | D-23; a vendored parser or a linked dependency |
| Windows Registry | deferred | needs Windows to test |
| gsettings, macOS defaults | deferred | needs the platform to test |
| remote key-value stores | deferred | a non-goal until something asks |

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

**MSVC is weaker and says so.** `/W4 /WX` is not equivalent to the GCC and Clang set;
several of the classes below have no MSVC counterpart. Stating the gap is better than a
comment implying parity: a project that claims one standard and enforces two has the
weaker one.

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

## Closing Phase 0

Phase 0 ends when every entry above reads `Decided` or `Deferred` with a written
reason, and the specification has been updated to match.

At that point the specification is frozen. Any new section afterwards has to be paid
for with code.
