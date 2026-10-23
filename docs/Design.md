# ConfigForge

**ConfigForge** is a cross-platform configuration resolution and management framework designed to make application configuration easy to consume without making configuration behavior implicit.

It provides a unified abstraction over environment variables, command-line arguments, configuration files, build-time values, OS-native configuration systems, runtime state, and user-defined configuration sources.

The project is designed around a simple principle:

> **Make configuration easy without making configuration behavior implicit.**

ConfigForge should provide strong defaults for common applications while remaining deterministic, explicit, extensible, and suitable for performance-sensitive systems ranging from embedded devices to desktop and server applications.

---

---

## Document Set

| Document | Purpose |
| -------- | ------- |
| `ConfigForge.md` | Architecture and specification. Long-lived. |
| `Decisions.md` | Open design decisions that must be closed before Phase 1. Retired once closed. |
| `Doctor.md` | `ConfigForge Doctor`, the pre-deployment configuration analysis tool built on the provenance report. |

## 1. Motivation

Application configuration is usually distributed across several unrelated mechanisms:

* Environment variables
* Command-line arguments
* Multiple configuration files
* User-level configuration
* System-level configuration
* OS-native configuration stores
* Build-time definitions
* Runtime configuration
* Application defaults

Each mechanism has its own API, precedence rules, data representation, validation behavior, and write semantics.

Applications therefore tend to accumulate configuration-specific code such as:

* manual environment variable parsing
* command-line parsing
* configuration file loading
* precedence handling
* type conversion
* validation
* fallback logic
* runtime overrides
* file writing
* reload/watch logic
* platform-specific configuration access
* debugging code explaining where a value came from

ConfigForge aims to provide a single configuration model for these concerns while keeping the underlying sources independent.

---

### 1.1 Why This Project Exists

ConfigForge is not a commercial product and is not expected to generate revenue.
There is no paid market for a C/C++ configuration library and this document does not
pretend otherwise.

The project exists for three reasons.

**1. Removing repeated code.** Every non-trivial application in this ecosystem will
otherwise reimplement the same plumbing: environment parsing, argument parsing,
precedence, type conversion, validation, fallback, file writing, reload, and ad-hoc
debug printing. That plumbing is the actual cost being removed, and it is paid once
per project for as long as the habit continues.

**2. Answering a question nobody answers in C/C++.** "Why is this value what it is?"
is answered by .NET (`ConfigurationRoot.GetDebugView()`), Spring Boot
(`/actuator/env` with origin tracking), Rust (`figment`), and Git
(`git config --list --show-origin --show-scope`). It is not answered by any widely
used C or C++ library. The gap is real; its commercial value is not.

**3. Being infrastructure worth showing.** A small, portable, fully specified library
with one distinctive capability is a stronger artifact than a large unfinished one.

---

### 1.2 Success Criteria

The project succeeds if, in a new application:

* configuration code is a declaration block rather than a module,
* no configuration helper is copied in from a previous project,
* and `explain()` removes the need to add temporary debug printing.

These are the criteria. Download counts and stars are not.

---

### 1.3 Adoption Rule

ConfigForge must not become a build-time requirement of any other project in this
ecosystem before it reaches a stable release.

Other projects read plain configuration by default. ConfigForge integration is an
optional adapter behind a project-local interface. Coupling an unfinished library to
an unfinished application blocks both of them, and the failure of either becomes the
failure of both.

---

## 2. Design Goals

### 2.1 Simplicity

The common case should require very little code.

A typical application should be able to define sources and simply ask:

```c
cf_i64 port = cf_get_int(cfg, "server.port", 8080);
```

Simplicity here is a measurable requirement rather than an aspiration. The following
budgets are binding on the design:

| Budget | Limit |
| ------ | ----- |
| Lines needed to read three parameters from defaults, file, environment and CLI | 10 |
| Mandatory third-party dependencies of the core | 0 |
| Headers to include for the C++ API | 1 |
| Policy required to obtain the default precedence | none |
| Setup required before `explain()` works | none |
| Concepts a reader must learn before the first working program | 1 (`get`) |

#### Usage Tiers

Complexity must be opt-in. Each tier must be usable without knowing that the next one
exists, and no tier may be a prerequisite of the one below it.

```text
Tier 0 — Read
    cf_config* cfg = cf_app("myapp", argc, argv, &err);
    cf_i64     port = cf_get_int(cfg, "server.port", 8080);

Tier 1 — Contract
    cf_builder_int(b, "server.port", 8080, 1, 65535, 0);

Tier 2 — Explicit sources
    cf_builder* b = cf_build(NULL);
    cf_builder_file(b, "file", CF_RANK_USER_FILE, "app.ini", NULL, 0);
    cf_builder_env (b, "env",  CF_RANK_ENV, "MYAPP_", cf_environ());
    cf_builder_cli (b, "cli",  CF_RANK_CLI, argc, argv);

    cf::Config cfg = cf::Builder()                       // the same, C++03
                         .file("file", CF_RANK_USER_FILE, "app.ini")
                         .env("env", CF_RANK_ENV, "MYAPP_", cf_environ())
                         .cli("cli", CF_RANK_CLI, argc, argv)
                         .create();

Tier 3 — Policy
    per-parameter precedence and source permissions (cf_def.policy)

Tier 4 — Extension
    custom sources, cf_derive, cf_emit, watching, snapshot images
```

`tests/unit/test_usage.c` is this list as a program: the smallest real use
of each tier, compiled and run by the suite, so the tiers here cannot name
an interface that does not exist. They once did - Tier 1 and Tier 2 above
showed functions from an early draft that the library never had.

`cf_app()` returns an ordinary object with the conventional sources and the
conventional precedence for a named application. It is a convenience constructor, not a
hidden global. The principle "no implicit global state" is preserved: the object is
explicit, only its construction is short.

The tier boundary is enforced in the documentation, not only in the API. The words
snapshot, policy, capability, materializer, provider and resolver do not appear before
Tier 3. A concept that a Tier 0 user can encounter is a concept that a Tier 0 user has
to learn.

The tier model is also the documentation structure. A reader who never leaves Tier 0
should never encounter the words policy, materializer or capability.


The application should not need to manually implement resolution logic.

### 2.2 Deterministic Resolution

Given the same:

* definitions
* source values
* capabilities
* policies
* runtime state

the resolver must produce the same result.

Implicit behavior based on registration order, filesystem discovery order, or object creation order should not affect resolution.

### 2.3 Explicit Mutation

Reading configuration must never implicitly modify a source.

Runtime mutation and persistent source mutation are separate operations.

```text
get()
    → read resolved state

set()
    → modify runtime state

write()
    → explicitly modify selected source(s)

propagate()
    → explicitly apply runtime state to policy-selected source(s)
```

### 2.4 Performance

Convenience must not require sacrificing runtime performance.

Configuration loading, parsing, policy compilation, and resolution should be separated from the hot path.

The intended runtime path is:

```text
Source Data
    ↓
Policy
    ↓
Resolver
    ↓
Immutable Snapshot
    ↓
Fast get()
```

Repeated `get()` operations should not perform file I/O, configuration parsing, or policy compilation.

### 2.5 Extensibility

Users should be able to add their own configuration sources without modifying ConfigForge.

Examples include:

* Remote configuration
* Database-backed configuration
* Device-specific configuration
* Company-specific configuration systems
* Custom embedded-system storage
* Application-specific configuration services

### 2.6 Cross-platform Design

The core should not depend on a particular operating system.

Platform-specific functionality should be implemented as optional providers.

### 2.7 Static Linking

The complete library must be statically linkable.

Optional providers and features should not become mandatory dependencies.

### 2.8 No Unnecessary Requirements

ConfigForge should provide useful defaults and should not force users to configure concepts they do not need.

An advanced configuration system must not require advanced configuration to use it.

---

## 3. Public API Surface

The C API is the API. The C++ header is a convenience layer written over it, not a
separate design. This section fixes both surfaces before any machinery is described, so
that every later decision can be checked against them.

### The Smallest Complete Program, C

```c
#include <configforge/configforge.h>

int main(int argc, char** argv)
{
    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    cf_i64      port;
    const char* host;
    int         tls;

    cfg = cf_app("myapp", argc, argv, &err);
    if (cfg == NULL) { fputs(err.message, stderr); return 1; }

    port = cf_get_int (cfg, "server.port", 8080);
    host = cf_get_text(cfg, "server.host", "0.0.0.0");
    tls  = cf_get_bool(cfg, "tls.enabled", 0);

    cf_config_free(cfg);
    return 0;
}
```

Declarations sit at the top of the block because the baseline is C89. That is the cost of
the language decision, shown rather than hidden, and at the call site it is the whole of it.

`cf_app` installs the conventional sources
and the conventional precedence for a named application:

```text
command line  >  environment (MYAPP_)  >  user file  >  system file  >  built-in default
```

File locations follow platform convention: XDG on Linux, `%APPDATA%` and
`%PROGRAMDATA%` on Windows, `~/Library/Application Support` on macOS.

The error is a caller-supplied struct rather than a thread-local last-error, because
global error state contradicts a stated principle and breaks under the fixed-capacity
profile.

### The Same Program, C++03

```cpp
#include <configforge/configforge.hpp>

int main(int argc, char** argv)
{
    cf::Config cfg = cf::app("myapp", argc, argv);
    if (!cfg) { std::fputs(cfg.error().message(), stderr); return 1; }

    int         port = cfg.get<int>("server.port", 8080);
    std::string host = cfg.get<std::string>("server.host", "0.0.0.0");
    bool        tls  = cfg.get<bool>("tls.enabled", false);
}
```

Every construct here is valid C++03. `cf::Config` is a reference-counted handle, so it
is copyable and cheap without move semantics, and a factory function can return it by
value. No `auto`, no smart pointer, no `string_view` appears in the baseline header.

A separate optional header adds `auto`-friendly returns, `std::string_view` and
`std::optional` for consumers on C++11 or later. It is additive and never required, and
the baseline header never depends on it.

### One Error Site

**Reads do not fail.**

Everything that can go wrong — an unreadable file, a malformed value, a range
violation, a policy conflict — is detected while the configuration is being built and
reported at exactly one place. Once a configuration object exists, reading is
infallible for any key with a default or a definition.

```text
construction   fallible, checked once
reading        infallible
```

This is the largest single simplicity decision in the design. The alternative, where
every read returns a status, costs three lines of ceremony per parameter and is the
main reason typed configuration libraries are described as heavy.

Two checked forms exist for the cases that need them:

```c
int     found = cf_try_int(cfg, "server.port", &out);   /* genuinely optional keys */
cf_i64  port  = cf_require_int(cfg, "server.port", &err);
```

Reading an undefined key with no default is a programming error rather than a runtime
condition, and is reported as one.

### Adding a Contract

```c
cf_builder* b = cf_app_builder("myapp", argc, argv);

cf_def_int (b, "server.port").value(8080).range(1, 65535);   /* C++ form */
cf_def_int_v(b, "server.port", 8080, 1, 65535);              /* C form   */
cf_def_text (b, "db.password", NULL, CF_REQUIRED | CF_SENSITIVE);

cf_config* cfg = cf_builder_create(b, &err);
```

In C the definition is a single call per parameter with a descriptor, because a chained
builder in C means an opaque handle with an ill-defined intermediate state. In C++03 the
fluent form is available over the same descriptor:

```cpp
cf::Config cfg = cf::app("myapp", argc, argv)
                   .def<int>("server.port").value(8080).range(1, 65535)
                   .def<std::string>("db.password").required().sensitive()
                   .create();
```

This is Tier 1, and it enables typo detection, contract-aware `explain()` output, and
most of the Doctor rule set.

### Explaining a Value

```c
char buf[1024];
cf_explain(cfg, "server.port", buf, sizeof buf);
fputs(buf, stdout);
```

No setup, no flag, no separate object, no build option. The caller supplies the buffer,
consistent with the allocation rules in the language section.

### Optional C11 Convenience

Where C11 is available, a `_Generic` dispatch collapses the accessors to one name:

```c
cf_i64 port = cf_get(cfg, "server.port", (cf_i64)8080);
```

The header tests `__STDC_VERSION__` and provides the typed accessors unconditionally, so
a C89 or C99 consumer loses convenience and never capability.

### String Lifetime at Tier 0

`cf_get_text` returns a pointer owned by the configuration object and valid until that
object is destroyed. Simple and correct as long as reload is not enabled.

With watch enabled, the pointer is valid until the next reload, and a program holding
values across reloads must use the snapshot handle described later. This is the one
genuine conflict between Tier 0 simplicity and Tier 4 capability, and the resolution is
that **enabling watch is what changes the contract**, never an invisible default.

### What Tier 0 Never Mentions

Snapshot, policy, capability, materializer, provider, resolver, effective policy,
propagation. All real, all necessary, none present in the first page of documentation or
in the programs above. Complexity is reachable, not unavoidable.

---

## 4. Core Conceptual Model

ConfigForge is built around several distinct responsibilities.

```text
Definition
    ↓
Source
    ↓
Policy
    ↓
Resolver
    ↓
Snapshot
    ↓
Application
```

Mutation follows a separate path:

```text
Application
    ↓
set()
    ↓
Runtime State

Application
    ↓
write()/propagate()
    ↓
Materializer
    ↓
Writer
    ↓
Source
```

The major abstractions are intentionally separated because they answer different questions.

| Abstraction       | Responsibility                                                     |
| ----------------- | ------------------------------------------------------------------ |
| `Definition`      | What is this configuration parameter?                              |
| `Value`           | What typed value does it contain?                                  |
| `Source`          | Where can configuration data come from or go to?                   |
| `SourceProvider`  | How is a type of source implemented?                               |
| `SourceInstance`  | Which concrete source is being used?                               |
| `Capability`      | What can a source technically do?                                  |
| `Policy`          | What is ConfigForge allowed to do and which source has precedence? |
| `PolicyLayer`     | Which group of policy rules belongs together?                      |
| `PolicySource`    | Where did a policy come from?                                      |
| `PolicyCompiler`  | How are policy sources combined into an effective policy?          |
| `EffectivePolicy` | The immutable policy actually used by the resolver                 |
| `Resolver`        | How are source values converted into resolved configuration state? |
| `Snapshot`        | Immutable resolved configuration state                             |
| `ConfigContext`   | The complete configuration world and its lifecycle                 |
| `Config`          | Application-facing configuration API                               |
| `Writer`          | Writes data to one source                                          |
| `Materializer`    | Applies configuration state to selected sources                    |
| `WatchManager`    | Detects source changes and triggers reload/resolution              |
| `Diagnostics`     | Explains and inspects configuration behavior                       |

---

## 5. Definitions

A definition describes the application's configuration contract.

For example:

```cpp
config.define<int>("server.port")
      .defaultValue(8080)
      .range(1, 65535);
```

A definition may contain:

* Name
* Type
* Default value
* Validation rules
* Description
* Metadata
* Aliases
* Optional source mappings
* Sensitivity, controlling redaction in diagnostics and reports
* Deprecation status and replacement key
* Merge mode, for composite parameters
* Invalid-value behaviour

A definition does **not** automatically create or modify values in every source.

For example, defining:

```text
server.port
```

must not automatically create:

```text
SERVER_PORT
```

in the environment, write a configuration file, modify an OS setting, or create a build variable.

Definition and source mapping are separate concepts.

---

## 6. Value Model

The value model determines the snapshot layout, the size of the C API, the mapping
rules for flat sources, and whether the core is writable in C at all. It is fixed
first.

### The Key Space Is Flat

ConfigForge stores a **sorted flat map from string key to typed value**. There are no
nested containers in the core. Structure is a naming convention on the key, not a shape
in the data.

```text
server.host                =  "0.0.0.0"
server.listeners.0.port    =  8080
server.listeners.0.tls     =  true
server.listeners.1.port    =  8443
server.listeners.count     =  2
```

This is not a simplification invented here. It is what the two best origin-tracking
implementations in the industry actually do: .NET's `IConfiguration` is a flat
`string → string` space with `:` separators, and Spring's `PropertySource` is a flat
map with `.` separators. Both flatten nested YAML and JSON at load time and reconstruct
structure only at binding time. Both then get clean per-key provenance as a consequence.

The benefits compound through the whole design:

* the core has no recursive data structure, which is what makes a C implementation
  reasonable rather than painful
* merge semantics for composites disappear, because every leaf resolves independently
* the C API needs no composite accessors
* the snapshot is a sorted array and serialises directly into the provenance report
* the inspection matrix in the diagnostics section is already flat
* flat-source mapping to environment variables and command-line arguments stops being a
  compromise and becomes the natural representation

### Types

Five, deliberately:

```text
CF_NULL     explicitly null where the format supports it
CF_BOOL
CF_INT      64-bit signed
CF_REAL     64-bit floating point
CF_TEXT     UTF-8, length-carrying
```

Narrower types such as `int`, `unsigned`, `float` are conversions applied at the API
boundary with range checking, not distinct storage types. This keeps the accessor set to
one function per storage kind rather than one per C type.

### Absence

Absence is modelled explicitly and is distinct from emptiness:

```text
absent      no source contains the key
empty       present, zero length
null        present, explicitly null
```

A key with no value and no default is an error at construction. Conflating these three
states is a recurring defect in comparable libraries.

### Arrays and the Truncation Rule

Arrays are index keys plus a synthesised count. Sources that natively understand arrays
(TOML, JSON) emit both; flat sources supply them by convention.

This model has one known hazard, and it must be handled rather than discovered. If a
high-precedence source supplies a two-element array and a low-precedence source supplies
four elements, the last two elements survive as ghosts. This is a real and documented
defect in .NET's configuration binding.

**Rule.** The winning source for `<key>.count` determines the array length. Index keys
beyond that count are dropped during resolution, remain visible in `explain()` as
discarded candidates, and are reported by Doctor as a distinct finding.

Two further forms exist for arrays of scalars in flat sources:

```text
MYAPP_TAGS = "a,b,c"          separator form; separator declared in the definition
--tag a --tag b --tag c       repeated form
```

Both expand to index keys plus a count during source reading, so the resolver sees only
one representation.

### Addressing

```text
server.port
server.listeners.0.port
logging."my.logger".level
```

Segments are separated by `.`. Indices are ordinary segments consisting of digits. A
segment containing a literal dot is quotable.

### What Is Given Up

An application that wants a whole struct in one call does not get it from the core. That
belongs to a binding layer, exactly as it does in .NET and Spring, and can be added
later as an optional component over the flat space without touching the core.

The trade is deliberate: a recursive value model would buy one convenience function and
would cost a recursive resolver, composite merge semantics, composite lifetime
management in C, composite accessors in the API, and a non-flat provenance model. That
is a poor exchange.

### 64-Bit Integers

`CF_INT` is 64-bit. Targets with no 64-bit integer type are outside the support matrix
rather than served by a degraded value model; see the target environments section. The
`cf_i64` typedef is resolved by feature detection in the public header so that the API
is identical across toolchains.

---

## 7. Sources

A `Source` provides configuration data to ConfigForge and may optionally accept configuration data from ConfigForge.

Typical sources include:

### Standard Sources

* Default
* Memory
* Environment
* Command Line
* File

### Optional Sources

* Build-time configuration
* OS-native settings
* User configuration
* System configuration

### Custom Sources

Users may implement their own providers.

A source has an identity, capabilities, and source-specific implementation.

---

## 8. Source Type vs Source Instance

A source provider describes how a source type works.

For example:

```text
FileSource
```

may have multiple instances:

```text
config.toml
config.local.toml
config.production.toml
```

These are separate source instances and can have independent policies.

This distinction is important because configuration systems commonly contain multiple files with different purposes and priorities.

For example:

```text
CLI
    >
Environment
    >
Local File
    >
Production File
    >
Project File
    >
Default
```

Source registration order must not implicitly determine this precedence.

---

## 9. Source Capabilities

A source exposes its technical capabilities.

Initial capability set:

```text
READ
WRITE
CREATE
DELETE
WATCH
ENUMERATE
ATOMIC_WRITE
```

Capabilities describe what the underlying source can technically perform.

For example:

```text
FileSource
    READ
    WRITE
    CREATE
    DELETE
    WATCH
```

A read-only source may expose:

```text
READ
```

A source's capabilities cannot be increased by policy.

If a policy requests:

```text
WRITE
```

for a source that only supports:

```text
READ
```

the policy is invalid.

Capability validation should happen before the policy becomes effective.

The validation mechanism should also be available explicitly so applications can validate configuration before execution.

---

## 10. Policy

Policy determines how ConfigForge is allowed to use sources.

Policy may control:

* Read precedence
* Source permissions
* Write targets
* Parameter-specific restrictions
* Source-specific behavior
* Propagation targets
* Overrides

For example:

```text
Read precedence:

CLI
>
Environment
>
Local File
>
Project File
>
Default
```

and:

```text
Environment:
    READ

Local File:
    READ WRITE

Project File:
    READ
```

Capability and policy are intentionally different:

```text
Capability
    = What can this source technically do?

Policy
    = What may ConfigForge do with it?
```

---

## 11. Parameter-Level Policy

Policies can apply globally or to individual parameters.

For example:

```text
Global:
    CLI > Environment > Local > Project > Default
```

while:

```text
database.password:
    Environment > Local
```

The parameter-level policy overrides or specializes the inherited global policy.

Users should not need to define policy for every parameter.

Policy inheritance should provide sensible defaults.

---

## 12. Policy Sources and Layers

Policies can originate from several places:

```text
Built-in Defaults
Programmatic Policy
Declarative DSL
Runtime Overrides
```

These are represented internally as policy sources/layers and compiled into one effective policy.

A default precedence model is:

```text
Runtime Override
    >
DSL Policy
    >
Programmatic Policy
    >
Built-in Default
```

Therefore, when a DSL policy and a programmatic policy conflict, the DSL policy wins.

This behavior must be deterministic and explainable.

---

## 13. Multiple Policy Files

ConfigForge supports multiple declarative policy files.

For example:

```text
base.policy.toml
project.policy.toml
deployment.policy.toml
```

Policy files are represented as policy layers.

A layer ordering can be explicitly defined:

```text
base
    <
project
    <
deployment
```

Policies that do not conflict may be merged.

If two policies at the same effective precedence conflict and no explicit precedence exists, ConfigForge should report a policy conflict rather than silently selecting one.

The following behaviors should not be used as implicit conflict resolution:

* Last loaded wins
* Alphabetical file order
* Registration order
* Filesystem order

---

## 14. Policy DSL

ConfigForge supports a declarative policy format in addition to its programmatic policy API.

The initial DSL target is **TOML**.

Programmatic APIs remain the primary API for applications.

The DSL exists for cases where policy should be:

* externally managed
* deployment-specific
* editable without recompiling
* shared across environments
* maintained separately from application code

The DSL should be an optional provider and must not become a mandatory dependency of the core.

---

## 15. Policy Compilation

Policy sources are compiled before resolution.

```text
Policy Sources
    ↓
Policy Layers
    ↓
Policy Compiler
    ↓
Capability Validation
    ↓
Conflict Validation
    ↓
Effective Policy
```

`EffectivePolicy` is immutable and can safely be used by the resolver.

This prevents the resolver from needing to understand whether a policy originated from:

* C++
* TOML
* defaults
* runtime overrides

---

## 16. Resolver

The resolver is responsible for deterministic configuration resolution.

Conceptually:

```text
Definitions
+
Source Values
+
Effective Policy
+
Runtime State
    ↓
Resolver
    ↓
Resolved State
    ↓
Snapshot
```

The resolver must not:

* modify sources
* write files
* parse policy files
* perform application logic
* implicitly watch sources

Its responsibility is limited to producing the correct resolved configuration state.

---

## 17. Resolution Granularity

Because the key space is flat, there is no composite merge problem. Every key resolves
independently against the precedence chain, and what would be "deep merge" in a nested
model is simply the default behaviour.

```text
app.toml      server.host = "0.0.0.0"
              server.port = 3000

environment   MYAPP_SERVER_PORT = 8080

resolved      server.host = "0.0.0.0"   from app.toml
              server.port = 8080        from environment
```

An override file supplies only the keys it changes. It never has to restate a whole
table, which is exactly the repetition this project exists to remove.

### Arrays

Arrays are the one place where per-key independence is not sufficient, because array
length is a property of the whole array rather than of any single element. The rule is
in the value model section: the winning source for `<key>.count` fixes the length, and
index keys beyond it are discarded rather than left as ghosts.

`MergeMode::Append` is available per definition for the case where an override should
extend rather than replace a list. It is opt-in, because appending silently is the more
surprising of the two behaviours.

### Consequence for Provenance

Provenance is per key. There is no composite whose origin is a set of origins, and
`explain()` therefore has a single shape for every parameter in the system. This is the
main reason the diagnostics output can be specified as a stable, testable contract.

---

## 18. Snapshot

A `Snapshot` represents an immutable resolved configuration state.

```text
Sources
    ↓
Resolver
    ↓
Snapshot
```

Applications can obtain a snapshot:

```cpp
auto snapshot = config.snapshot();
```

and repeatedly access it without accessing the underlying sources.

This is particularly useful for hot loops and concurrent applications.

A source change should produce a new snapshot rather than mutating an existing snapshot.

```text
Snapshot #1
     │
     │ source changes
     ↓
Snapshot #2
```

Existing users of Snapshot #1 can continue using it safely.

---

## 19. Snapshot Ownership and Lifetime

A library that promises an allocation-free hot path and hot reload at the same time must
state who owns returned memory. This cannot be deferred, because it is an interface
decision and correcting it later is a breaking change.

### Model

Snapshots are reference-counted and immutable. Reload builds a new snapshot and
atomically replaces the configuration's current pointer. Existing holders keep seeing the
snapshot they acquired. The pattern is read-copy-update.

### Three Read Forms

```c
/* borrowed; valid until the next reload, or until cf_config_free */
const char* host = cf_get_text(cfg, "server.host", "0.0.0.0");

/* copied into caller storage; always safe, no lifetime rule to remember */
char buf[64];
cf_get_text_copy(cfg, "server.host", buf, sizeof buf, "0.0.0.0");

/* borrowed; valid for exactly as long as the snapshot handle is held */
cf_snapshot* s = cf_snapshot_acquire(cfg);
const char*  h = cf_snapshot_text(s, "server.host", "0.0.0.0");
cf_snapshot_release(s);
```

Tier 0 uses the first, and the second once reload is enabled. The third exists so that
the performance goal is not paid for by every caller.

### Rules

1. A borrowed pointer is valid exactly as long as the snapshot that produced it.
2. Without watch enabled there is one snapshot for the lifetime of the configuration, so
   the first form carries no lifetime hazard at all. This is why Tier 0 can ignore the
   subject entirely.
3. Acquiring a snapshot is a single atomic increment and is the only synchronisation on
   the hot path.
4. A released snapshot with no remaining holders is destroyed off the hot path.
5. Reload never mutates a live snapshot.

### Storage Layout

Because the key space is flat and sorted, a snapshot is one contiguous block: a sorted
key index, a value array and a string arena, allocated together.

Three consequences follow, all useful:

* the fixed-capacity profile becomes natural rather than bolted on, since the whole
  snapshot is a single arena allocation
* the determinism hash is a hash over that block rather than a traversal
* lookup is a binary search over the key index with no pointer chasing

The C++03 wrapper puts the snapshot handle in a RAII type so acquire and release are not
manual. That is a convenience of the wrapper, not a requirement of the model.

---

## 20. ConfigContext

A `ConfigContext` represents one complete configuration world.

It owns or coordinates:

* Source instances
* Definitions
* Effective policy
* Resolver
* Current snapshot
* Runtime state
* Diagnostics
* Watch management

A context provides isolation.

For example:

```text
ConfigContext A
    Sources A
    Policy A
    Snapshot A

ConfigContext B
    Sources B
    Policy B
    Snapshot B
```

Changes in A must not implicitly modify B.

There is no implicit global configuration state and there is no "last Config instance wins" behavior.

---

## 21. Config

`Config` is the application-facing API over a `ConfigContext`.

Typical operations include:

```cpp
config.get<T>("key");
config.set("key", value);
config.write("key", value, source);
config.snapshot();
config.watch("key", callback);
config.reload();
config.explain("key");
```

The exact syntax may evolve, but the semantics should remain stable.

---

## 22. Multiple Config Instances

Multiple `Config` objects must be supported.

By default they should be isolated through their contexts.

If applications explicitly want shared configuration state, multiple facades may reference the same `ConfigContext`.

```text
             ConfigContext
             /            \
         Config A       Config B
```

Sharing must be explicit.

ConfigForge must never rely on global mutable configuration state to make multiple instances interact.

---

## 23. Reading Configuration

The common operation is:

```cpp
auto port = config.get<int>("server.port");
```

This uses:

```text
Current Snapshot
```

and therefore should be cheap.

The normal `get()` operation performs policy-based resolution through the current configuration state.

---

## 24. Scoped Reads

Applications may sometimes want to restrict resolution to selected sources.

For example:

```cpp
config.get<int>(
    "server.port",
    Sources{Environment, ProjectFile}
);
```

This does not mutate the global policy.

It performs a scoped resolution using the selected sources.

Raw source access remains the responsibility of the source API:

```text
config.get()
    → resolved configuration

source.read()
    → source-specific raw data
```

---

## 25. Runtime `set()`

`set()` modifies runtime configuration state.

```cpp
config.set("server.port", 9000);
```

The default behavior is:

```text
Runtime State
    ↓
New Snapshot
```

It does not automatically modify:

* Environment variables
* Configuration files
* OS-native settings
* Build configuration
* Other sources

This makes `set()` safe and predictable.

---

## 26. Explicit Persistent Writes

Persistent source mutation is explicit.

```cpp
config.write(
    "server.port",
    9000,
    SourceId{"config.local"}
);
```

The operation performs:

```text
Policy Check
    ↓
Capability Check
    ↓
Writer
    ↓
Selected Source
```

`WRITE` capability and policy permissions must both allow the operation.

---

## 27. Automatic Propagation

ConfigForge also supports optional automatic propagation.

A user may explicitly request:

```text
set()
    +
Propagation
```

For example:

```cpp
config.set(
    "server.port",
    9000,
    SetMode::Propagate
);
```

In this mode ConfigForge can:

```text
Runtime State
    ↓
Materializer
    ↓
Effective Policy
    ↓
Writable Target Sources
    ↓
Writer
```

This operation may be slower than a runtime-only `set()`.

That is intentional.

The user explicitly accepts the additional work in exchange for convenience.

Automatic propagation must never be the default behavior of `set()`.

---

## 28. Propagation Targets

Propagation can use policy-selected targets or explicit targets.

Conceptually:

```text
Propagation::PolicyTargets
Propagation::SelectedSources
Propagation::AllWritable
```

An explicit all-writable operation may update every source permitted by capability and policy.

Read-only sources must not be modified.

If propagation partially fails, ConfigForge should report the individual results rather than pretending that the operation was globally atomic.

A propagation result may contain:

```text
Successful Sources
Failed Sources
Skipped Sources
Errors
```

ConfigForge does not promise cross-source transactions in the initial architecture.

---

## 29. Materializer

The `Materializer` is responsible for applying configuration state to one or more sources.

It is deliberately separated from `Writer`.

```text
Materializer
    ↓
Target Selection
    ↓
Policy
    ↓
Writer(s)
    ↓
Source(s)
```

`Writer` answers:

> How do I write to this source?

`Materializer` answers:

> How should this configuration state be applied to the selected sources?

This separation becomes important when propagation targets multiple sources.

---

## 30. Watch and Reload

Sources may optionally support `WATCH`.

ConfigForge should not require every source to implement watching.

For sources that support it:

```text
Source Change
    ↓
WatchManager
    ↓
Reload
    ↓
Resolution
    ↓
New Snapshot
    ↓
Notification
```

Watching must not implicitly trigger writes.

A watched value changing should not automatically propagate that value to other sources.

---

## 31. Selective Reload

Reloading should be possible at different scopes.

Conceptually:

```text
reload()
reload(source)
reload(key)
```

The implementation may optimize these operations using source-level and parameter-level invalidation.

A source change should not require rebuilding unrelated configuration state when an incremental strategy can safely be used.

---

## 32. Performance Model

ConfigForge should distinguish between the cold path and the hot path.

### Cold Path

May include:

* Source initialization
* File I/O
* Parsing
* Policy compilation
* Validation
* Resolution
* Snapshot creation

### Hot Path

Should ideally be:

```text
snapshot.get()
```

with:

* no file I/O
* no parser invocation
* no policy compilation
* no source discovery
* minimal allocation
* minimal synchronization

Performance should be treated as a design requirement rather than a later optimization.

---

## 33. Diagnostics

One of ConfigForge's major usability features is explaining why a value has a particular result.

For example:

```cpp
config.explain("server.port");
```

could report:

```text
Parameter: server.port

Sources:
  CLI          → not present
  Environment  → 8080
  Local File   → 3000
  Project File → not present
  Default      → 80

Policy:
  CLI > Environment > Local File > Project File > Default

Selected:
  Environment

Reason:
  Environment has higher precedence than Local File.
```

The purpose is to answer:

> **Why is this value what it is?**

---

## 34. Configuration Inspection

ConfigForge should provide a matrix-oriented inspection API.

Conceptually:

```text
Parameter       CLI   ENV   Local   Project   Default   Selected
----------------------------------------------------------------
server.port     -     8080  3000    -         80        ENV
server.host     -     -     -       localhost localhost Project
tls.enabled     false -     true    -         true      CLI
```

Inspection should be available programmatically and, where useful, through human-readable diagnostic output.

---

## 35. Error Handling

Errors are concentrated at configuration construction rather than spread across every
read. Once a configuration object exists, reading is infallible for any key with a
default or a definition, so the categories below are almost entirely construction-time
categories.

### The Model

A caller-supplied error struct. Never a thread-local last-error, because global error
state contradicts a stated principle; never an allocation, because the failure path must
work under the fixed-capacity profile.

```c
typedef struct cf_error {
    size_t size;                /* sizeof(cf_error), for versioning   */
    int    code;                /* cf_error_code                      */
    char   key[CF_KEY_MAX];     /* parameter, when applicable         */
    char   source[64];          /* source instance identifier         */
    char   message[256];        /* human-readable, already formatted  */
} cf_error;

#define CF_ERROR_INIT { sizeof(cf_error), 0, {0}, {0}, {0} }
```

Fixed size, stack-allocatable, usable in every build profile. The `size` field is what
allows fields to be added after 1.0 without breaking the interface.

Functions that can fail take `cf_error*` as their last parameter and return `NULL` or a
failure code. Functions that cannot fail do not take it, so the signature itself
documents fallibility.

### Categories

* Invalid definition
* Type conversion failure
* Validation failure
* Source failure
* Unsupported capability
* Policy violation
* Allocation or capacity exhaustion

### Language Layers

The C++03 wrapper stores the error inside the `Config` handle, so the check is `if (!cfg)`
followed by `cfg.error()`. A throwing accessor is available where exceptions are
acceptable; nothing in the core depends on it.

Bindings in other languages map the struct onto their own idiom: a Python exception, a
Rust `Result`, a Go `error`. The struct is the wire format, not the ergonomics.

---

## 36. Validation and Failure Semantics

When a source supplies a value that fails conversion or validation there are two
possible behaviours, and they cannot both be the default.

```text
Strict     resolution fails; the error names the parameter, the source and the rule
Fallback   the value is rejected and resolution continues with the next source
```

**The default is Strict.**

A silent fallback would mean that a malformed production environment variable quietly
degrades to a development default, and the application starts in a state nobody
chose. That is exactly the implicit behaviour this project exists to prevent.

Fallback remains available per parameter:

```cpp
config.define<int>("server.port")
      .range(1, 65535)
      .onInvalid(OnInvalid::Fallback);
```

When fallback is enabled, the rejected value and the reason for its rejection stay in
the provenance chain and appear in `explain()` output. A rejected value is never
erased.

### Unknown Keys

A source may contain keys that no definition describes.

```text
Ignore   default; foreign keys in shared or system-wide files are normal
Warn     reported through diagnostics
Reject   resolution fails
```

The mode is set per source instance, because a file owned by the application and a
system-wide file deserve different treatment.

In `Warn` and `Reject` modes an unknown key is matched against defined keys by edit
distance, so a typo is reported as a suggestion rather than as a bare rejection:

```text
Unknown key: server.pot
    did you mean: server.port
    source:       app.toml, line 12
```

This single behaviour removes a large share of real-world configuration incidents, and
it is available only because the definition set is a contract rather than
documentation.

---

## 37. Serialization Layer

Reading a configuration file and writing a provenance report are the same problem seen
from two sides: bytes to a key space, and a key space to bytes. They are solved once, in a
layer that knows nothing about configuration semantics.

```text
bytes  <->  reader / writer      byte source and sink
            codec                format grammar, both directions
            document events      begin_table, begin_array, scalar, end
            flattener            events <-> flat key space
```

### The Codec Interface

```c
typedef struct cf_codec {
    const char*   name;          /* "json", "ini", "toml", "xml"          */
    const char*   extensions;    /* ".json"                               */
    cf_codec_caps caps;          /* read, write, types, comments, locations */
    int (*decode)(cf_reader* in,  cf_doc_sink* out, cf_error* err);
    int (*encode)(cf_doc_walk* in, cf_writer* out, cf_error* err);
} cf_codec;
```

A codec is registered at build time. `decode` parses and emits document events; `encode`
consumes them and writes bytes. Neither direction contains any key-construction logic,
because the flattener owns that and is shared.

### Document Events

```c
typedef struct cf_doc_sink {
    int (*begin_table)(void* ctx, const char* name, size_t len);
    int (*begin_array)(void* ctx, const char* name, size_t len);
    int (*scalar)     (void* ctx, const cf_raw* v, const cf_loc* at);
    int (*end)        (void* ctx);
    void* ctx;
} cf_doc_sink;
```

What this buys:

* key construction is identical across every format by construction, not by convention
* array count synthesis happens in exactly one place
* location tracking is uniform, so `explain()` reports `app.toml:14:3` whatever parsed it
* a new format is an event producer with no key logic in it at all
* reading and writing a format are the same 400 lines seen twice, not two subsystems

### Four Users, One Layer

```text
configuration file    codec.decode -> events -> flattener -> key space
written overrides     key space -> walk -> events -> codec.encode
provenance report     key space -> walk -> events -> JSON encode
Report Replay         JSON decode -> events -> flattener -> key space
```

The report is not a special case of anything. It is the JSON codec applied to the same
walk that writes a configuration file, which is why the report format cost almost nothing
once the layer existed.

### Should It Be a Separate Library?

It could be. The layer has no dependency on the configuration model and would be a usable
standalone C89 serialization component.

**Recommendation: keep it internal for now, with the boundary drawn as if it were
separate.** `src/serialize` may not include anything from `src/core` beyond the
portability layer, checked in CI. Extracting it later is then a directory move plus a
build file. Extracting it now buys nothing and costs a second version number, a second
release process and a second set of documentation.

---

## 38. Source Kinds

Sources divide by what they have to do with the bytes, and the division
is what the code is organised around.

| Kind | Members | The work |
| ---- | ------- | -------- |
| **Direct** | memory pairs, definition defaults, a replayed report | no bytes to read |
| **Flat** | environment, command line, later `.env` and a registry | already key and value; lexing and naming |
| **Hierarchical** | JSON, TOML, XML, INI | a document; parsing, then flattening |

The hierarchical group had a seam from the start: a parser is an event
producer and never touches a key space. The flat group did not, and each
of its sources reached into the key space itself. That put the decision
"what does a candidate look like" in three files, and it meant every
lexer needed an arena and a key space before it could be tested at all.

### The entry sink

A flat source now produces entries into a `cf_entry_sink`. Recording
them into a key space is one implementation; a test's implementation is
an array.

```text
cf_lex_env  --+
cf_lex_cli  --+--> cf_entry_sink --> cf_keysink --> key space
codec -> events -> flattener --+
```

The lexers take no arena, no key space and no source object. Everything
they need is an argument, which is the whole point: the lexer tests
exercise both of them against an array and never allocate.

### Key mapping

How a source spells a key is separate from lexing it, and the separation
buys the direction that only matters once a contract exists: given a
key, ask a source what *it* would call that key.

```text
server.port
  command line   --server.port          a command line can carry a dot
  environment    MYAPP_SERVER__PORT     enumerable
  environment    MYAPP_SERVER_PORT      natural; needs a key list
```

This is what closed D-06. Without a definition set only the escaped
spelling can be enumerated, because `MYAPP_A_B_C` could be four
different keys. With one the ambiguity is gone and Tier 1 accepts the
spelling people actually write. Neither could have been added without
the mapping being a value of its own.

---

## 39. Format Providers

| Format | Reads | Writes | Carries types | Notes |
| ------ | ----- | ------ | ------------- | ----- |
| INI subset | yes | whole file | no | built in; every profile; inside the amalgamation |
| JSON | yes | whole file | yes | required by the provenance report and Report Replay |
| TOML | yes | whole file | yes | the recommended hand-written format |
| XML | yes | no | no | requires an explicit element-to-key convention |
| SQLite | yes | transactional | yes | a store rather than a document |
| Environment | yes | no | no | built in |
| Command line | yes | no | no | built in |

### JSON

```json
{ "server": { "port": 8080, "hosts": ["a", "b"] } }
```

```text
server.port         INT   8080
server.hosts.0      TEXT  "a"
server.hosts.1      TEXT  "b"
server.hosts.count  INT   2
```

JSON has one numeric type. The rule is mechanical: a number containing `.`, `e` or `E` is
`CF_REAL`; otherwise it is `CF_INT` when it fits in 64 bits, and `CF_REAL` with a recorded
warning when it does not.

### TOML

Built in, as the subset D-23 describes (`src/serialize/toml.c`). Native types map
directly; arrays and arrays of tables become index keys; every value carries its line and
column. TOML's dates and times have no ConfigForge equivalent and are **refused** rather
than mapped: the error says to quote the value as a string, which keeps the choice of
meaning with the person who wrote it. Multi-line strings and `inf`/`nan` are refused the
same way. A key set twice follows this library's rule rather than TOML's - the later
wins, and Doctor's CFD001 reports the earlier - which answers "why did my edit do
nothing" better than refusing to start.

Written TOML puts a `[header]` on each top-level table and dotted keys below it, with
arrays inline, so that no header is ever repeated - TOML forbids that, and the emitter's
order cannot always avoid it otherwise.

### XML

XML has no canonical mapping to a key space, so the convention is declared explicitly and
tested rather than inferred.

```xml
<server port="8080">
  <host>a</host>
  <host>b</host>
</server>
```

```text
server.port         TEXT  "8080"     from the attribute
server.host.0       TEXT  "a"
server.host.1       TEXT  "b"
server.host.count   INT   2
```

Rules:

* attributes and child elements share one key namespace; a collision between an attribute
  and a child of the same name is an error, never a merge
* repeated sibling elements with the same name become an array
* mixed content — an element with both text and element children — is an error, because it
  has no sensible configuration meaning
* namespace prefixes are part of the key and are quoted when they contain a colon
* XML carries no types, so every value is `CF_TEXT` and the definition converts it
* XML is read-only. Writing XML back while preserving formatting, comments and entities is
  harder than the TOML case and is not attempted

### SQLite

SQLite needs no flattening at all, because the flat key space *is* its natural shape.

```sql
CREATE TABLE configforge (
    key   TEXT PRIMARY KEY NOT NULL,
    type  TEXT NOT NULL,     /* null | bool | int | real | text */
    value TEXT,
    meta  TEXT               /* optional: who set it, when, why  */
);
```

Reading is one ordered `SELECT`. The `meta` column is where a written value can record its
own provenance, which makes a store the only source kind that can explain itself across
process restarts.

### SQLite Is the Answer to the Write Problem

The specification defers format-preserving writes into hand-maintained files, because
rewriting a TOML document while keeping its comments, ordering and spacing is a separate
project. `taplo` exists in another ecosystem for exactly this reason.

SQLite removes the problem instead of solving it. A store has no comments to preserve and
no formatting to lose, and it has transactions, so `write()` and `propagate()` become
atomic and crash-safe rather than rewrite-and-hope.

The position that follows is worth stating plainly:

> When an application needs runtime-writable configuration, the write target is a
> **store**, not a document. SQLite where it is available, the built-in overrides file
> where it is not. Hand-maintained documents stay read-only, which is also what the people
> who maintain them expect.

---

## 40. Type Inference

Formats divide into those that carry types and those that do not.

```text
carries types      JSON, TOML, SQLite
carries none       INI, XML, environment, command line
```

**Where a definition exists, the definition decides.** A typeless source produces `CF_TEXT`
and the definition converts it with range and validation applied. No inference happens at
all. This is the normal case and it is why Tier 1 is worth reaching.

**Where no definition exists** — Tier 0, no contract — inference is unavoidable, and the
rule is deliberately timid:

```text
"true" / "false"                  BOOL   exactly these, lowercase, nothing else
optional sign then digits only    INT    when it fits in 64 bits
C-locale floating point form      REAL
anything else                     TEXT
```

Note what is missing from that list. `yes`, `no`, `on`, `off`, `y`, `n` stay text. YAML 1.1
reads `NO` as boolean false, which turns the country code for Norway into `false`, and the
same class of surprise reaches INI files the moment a parser gets creative.

Leading zeros stay text, so `08` is a string rather than an octal accident. A version
string `1.10` stays text rather than becoming the real number `1.1`. **A value a human
would have to stop and think about is left alone.**

---

## 41. JSON Schema Integration

JSON Schema is not a source. It describes the *contract*, which in ConfigForge is the
definition set, so it connects to a different subsystem — and it connects in both
directions.

**Import: schema to definitions.** A schema supplies key names, types, defaults, ranges,
required flags and descriptions, which is exactly what a definition carries. An application
can replace a block of `cf_def_*` calls with one schema file.

**Export: definitions to schema.** The reverse is worth more than it looks. A generated
schema gives editors autocomplete, inline documentation and live validation of the
configuration file itself:

```json
{ "$schema": "./myapp.schema.json", "server": { "port": 8080 } }
```

Most editors act on this with no plugin. An application that declares its configuration
contract in code gets editor support for its configuration files for free, for a few
hundred lines of generator.

| JSON Schema | ConfigForge |
| ----------- | ----------- |
| `type: integer` | `CF_INT` |
| `type: number` | `CF_REAL` |
| `type: string` | `CF_TEXT` |
| `type: boolean` | `CF_BOOL` |
| `type: object` | key prefix |
| `type: array` | index keys plus `count` |
| `default` | definition default |
| `minimum`, `maximum` | range |
| `enum` | allowed value set |
| `required` | required flag |
| `description` | help text, shown in `explain()` |
| `writeOnly` | sensitive |

Unsupported keywords — `oneOf`, `allOf`, `$ref` across files, `patternProperties` — are
rejected with a message naming the keyword, never ignored. An honestly documented subset is
worth more than a full implementation that silently drops constraints.

---

## 42. Build-Time Feature Selection

Every format beyond the built-in set is a compile-time option, and so is every part of
the library a program can do without (D-38).

```text
CF_WITH_INI        ON     built in, no dependency, inside the amalgamation
CF_WITH_JSON       ON     built in; Report Replay and Doctor read it
CF_WITH_TOML       ON     built in, the documented subset (D-23); no dependency
CF_WITH_XML        OFF    vendored C parser
CF_WITH_SQLITE     OFF    links libsqlite3
CF_WITH_REGISTRY   OFF    Windows
CF_WITH_REAL       ON     the floating-point value type and its exact formatter
CF_WITH_DIR        ON     the drop-in directory source (conf.d)
CF_WITH_DOTENV     ON     the .env source and its lexer
CF_WITH_EMIT       ON     writing a configuration out: cf_emit and every encoder
CF_WITH_IMAGE      ON     the binary snapshot image, writer and reader
CF_WITH_REPORT     ON     the provenance report and Report Replay; Doctor needs it
CF_WITH_CXX        ON     header only; costs nothing when unused
```

A part that is compiled out keeps its functions, so a program links against any build,
and each function fails by name: a directory source in a build without one stops the
configuration from being built with "this build was compiled without CF_WITH_DIR" -
the same rule as a disabled format, below, and for the same reason.

### A Disabled Format Fails Loudly

This is the rule that matters, and it follows directly from the project's premise.

If `CF_WITH_XML` is off and an application adds `app.xml` as a source, the result is an
error when the configuration is built:

```text
error: source 'app.xml' requires XML support
       this build was compiled without CF_WITH_XML
```

Not a silent skip, not a fall back to defaults, not a warning buried in a log. A
configuration system whose entire purpose is explaining where values come from must not
have a build option that makes a file invisible.

### The Feature Set Is Part of the Report

The compiled-in format set is queryable at runtime, appears in `explain()` output and is
recorded in the provenance report. A report produced by a restricted build is therefore
distinguishable from one produced by a full build, so Doctor can say "this source was not
readable by that build" instead of reporting a false drift.

### Format Detection

By file extension, with an explicit override available per source. Never by content
sniffing. Sniffing is exactly the kind of implicit behaviour this project rejects, and it
fails in the most confusing possible way when a file is renamed.

---

## 43. Providers and Plugins

ConfigForge should support user-defined providers.

A provider may implement a source such as:

```text
Redis
Consul
Database
Remote API
Device NVRAM
Custom embedded storage
Company-specific configuration service
```

Providers should not necessarily be dynamically loaded.

The architecture should support:

```text
Built-in Provider
Static Provider
Dynamic Provider
```

without requiring dynamic loading.

This is important for embedded and statically linked applications.

---

## 44. Optional Features

ConfigForge should use feature-level build configuration.

For example:

```text
CONFIGFORGE_ENABLE_ENV
CONFIGFORGE_ENABLE_FILE
CONFIGFORGE_ENABLE_TOML
CONFIGFORGE_ENABLE_NATIVE
```

Watching is not on this list as built. Whether a handle to wait on
exists is the platform's answer rather than a build choice, so
`cf_has_feature("watch")` reports what the platform provides; an option
for it switched nothing and made every report claim it was absent.

The exact build interface may differ.

The important requirement is:

> Disabled functionality must not become an unnecessary runtime or build-time dependency.

For example, an application that does not use JSON should not need a JSON parser in its final binary.

---

## 45. Implementation Language and Standards

```text
core                 C89 baseline, opportunistic upgrades where detected
public C header      C89, must compile standalone under -std=c89 -pedantic
C++ header           C++03
optional C++ header  C++11 and later conveniences, additive
build                CMake 3.10; toolchains without it use the amalgamation (D-31)
```

### Why C Rather Than C++

Three reasons, in order of weight.

**Bindings.** A C ABI is the universal FFI target. Python `ctypes` and `cffi`, Rust
`bindgen`, C# P/Invoke, Java JNA, Go cgo, Node N-API and WASM all consume it directly and
none needs a shim. A C++ core would require a C export layer to reach any of them, which
means writing the C interface anyway and maintaining two surfaces.

**No runtime dependency.** A C library links into a C project, a C++ project, a firmware
image and a WASM module without dragging in a C++ runtime, static initialisers or an
unwinder.

**Maintainability fit.** A library its author cannot comfortably maintain is worse than one
written in a more ergonomic language. This is a legitimate engineering input, not a
concession.

The counter-argument that previously favoured C++ was composite lifetime management, and it
was dissolved by the value model. With a flat key space there are no recursive values and no
composite ownership graph, so there is very little that RAII would have been protecting.

### Why C89 as the Baseline

C89 is the widest target a portable library can have, and the opportunistic strategy in the
portability layer recovers most of what a later standard would otherwise have given. SQLite
is the standing proof that this combination produces a library that runs everywhere and
still has a simple API.

The cost is real and is accepted rather than minimised:

```text
declarations at the top of each block
no // comments
no designated initializers
no flexible array members          char data[1] idiom instead
no snprintf                        own bounded formatter required
no stdint.h                        own typedefs by detection
no long long guarantee             detection required; absence is a build error
no _Bool                           own cf_bool
```

Two of those are not really costs. The bounded formatter has to exist regardless, because
the C library's numeric formatting is locale-dependent and therefore unusable here. And
`cf_bool` is preferable to `_Bool` in any case, because an ABI type should not change size
with the consumer's language standard.

**What C89 does not buy.** Toolchains with 16-bit `int`, no 64-bit integer type, or near
and far pointer models remain out of scope. Those are semantic limits rather than syntactic
ones, and C89 conformance does not reach them. Stated so the support matrix stays honest.

### Symbol Discipline

A C library has no namespaces, so the prefix rule *is* the namespace and it has to be
enforced rather than intended.

```text
cf_       public functions and types
CF_       public macros and enumerators
cfi_      internal symbols with external linkage
static    everything else
```

No public symbol may exist without a `cf_` or `CF_` prefix. CI checks the exported symbol
table with `nm` against an allow-list, because a leaked unprefixed symbol breaks someone
else's build and is found by the person who links it rather than the person who wrote it.

### String Representation

Text values are UTF-8 and are stored with an explicit length. The API offers both forms,
because both are genuinely needed:

```c
cf_get_text  (cfg, key, def)          /* NUL-terminated, convenient     */
cf_get_text_n(cfg, key, &len, def)    /* pointer and length, no scan    */
```

Stored values are NUL-terminated *in addition to* carrying a length, so the first form is
free. The consequence is that an embedded NUL inside a configuration value is rejected
when the source is read, rather than silently truncating something much later.

### Standards

| Component | Standard | Gate |
| --------- | -------- | ---- |
| Core implementation | C89 baseline | `-std=c89 -pedantic -Werror` on GCC and Clang in CI |
| Public C header | C89 | compiled standalone as C89 in CI |
| C++ wrapper | C++03 | `-std=c++98 -pedantic` |
| Optional C++ header | C++11 | additive, never referenced by the baseline |
| Tests and tooling | unconstrained | run on a modern toolchain |

---

## 46. Portability Layer

The baseline is C89. Where a toolchain offers more, the library uses it. This strategy has
exactly one failure mode, and this section exists to prevent it: **two code paths that are
never compared become two behaviours.**

Determinism is a stated requirement of this design. If the C89 path and the C99 path format
a number differently, the golden `explain()` fixtures fail on one platform and the
determinism hash differs between two builds of the same library. That is a worse outcome
than never having used the standard function at all.

### The Rule

> Opportunistic substitution is permitted for **capability and performance**.
> It is forbidden for **anything whose output is part of a contract**.

### Permitted Substitutions

| Concern | C89 baseline | Upgrade when detected |
| ------- | ------------ | --------------------- |
| Fixed-width integers | typedefs by compiler detection | `<stdint.h>` (C99) |
| 64-bit integer | `long long` or `__int64` by detection | C99 `long long` |
| Boolean | `typedef int cf_bool` | kept regardless, for ABI stability |
| Inline | `static` functions | `inline` (C99), `__inline` (MSVC) |
| Compile-time assertion | negative array size idiom | `_Static_assert` (C11) |
| Variable-length struct tail | `char data[1]` idiom | flexible array member (C99) |
| Atomic reference count | mutex, or single-threaded build | `__sync_*` (GCC 4.1), `__atomic_*` (GCC 4.7), `_Atomic` (C11) |
| Generic accessor | typed functions only | `_Generic` (C11), header only |
| Branch hints, format checking | nothing | GCC and Clang attributes |
| Message assembly with `%s` and `%d` | own bounded formatter | `snprintf` (C99) |
| Sorting | own merge sort | `qsort` with a byte-wise comparator |

Note on the first table row people reach for: `sprintf` exists in C89, `snprintf` does not.
The C89 fallback is therefore not "use `sprintf`" but "write a bounded formatter", because
an unbounded one is a defect regardless of standard level.

### Forbidden Substitutions

Always the library's own implementation, on every toolchain, at every standard level:

| Concern | Why |
| ------- | --- |
| Number to text, text to number | `strtod` and `printf("%f")` follow the active locale's decimal separator. A file written on one machine would not parse identically on another, and the determinism hash would differ by locale. |
| Case folding in key normalisation | `tolower` and `toupper` are locale-dependent. In a Turkish locale `toupper('i')` is `'İ'` and `tolower('I')` is `'ı'`. A library that normalises keys through the C library resolves different keys on a Turkish system than on an English one. |
| Key ordering | `strcoll` is locale-dependent. Ordering must be byte-wise or the snapshot hash is not reproducible. |
| Whitespace and digit classification | `isspace` and `isdigit` are locale-dependent above ASCII. |

The pattern is consistent: **every locale-sensitive C library function is forbidden.** This
is not caution about old compilers. It is the same bug class on a current one, and it is why
the numeric conversion code has to exist from the first milestone rather than being added
when someone reports it.

### Where Detection Lives

All feature detection lives in `cf_port.h` and `port.c`. No other file may test
`__STDC_VERSION__`, `_MSC_VER`, `__GNUC__` or any comparable macro. Enforced by a grep in
CI, because the value of a portability layer is precisely that it is the only place with
conditional compilation in it.

### How Both Paths Stay Honest

The C89 fallbacks would otherwise be exercised only on toolchains nobody runs daily, and
would rot without anyone noticing.

```text
CF_FORCE_PORTABLE=1     disables every opportunistic upgrade
```

CI builds the full conformance suite twice on the modern toolchain, once normally and once
with `CF_FORCE_PORTABLE`, and asserts that:

* every golden `explain()` fixture matches byte for byte
* the determinism hash is identical
* the test suite passes identically

This is what turns "C89 with opportunistic upgrades" from a slogan into a property. A
fallback path that is never compared against the fast path is not a fallback; it is untested
code waiting for a user with an old compiler to find it.

---

## 47. Support Matrix

"Maximum reach" is not a specification. A support matrix is.

| Tier | Meaning | Contents |
| ---- | ------- | -------- |
| **A — Supported** | in CI, every commit | GCC 4.8+, Clang 3.4+, MSVC 2015+, Apple Clang, Emscripten, ARM GCC 4.9+ |
| **B — Best effort** | compiles, not continuously tested; reports accepted | GCC 3.x–4.7, MSVC 2005–2013, vendor toolchains with C89 and a 64-bit integer type |
| **C — Out of scope** | stated, not hedged | 16-bit `int`, no 64-bit integer type, near/far pointer models |

The distinction matters because Tier B costs almost nothing beyond C89 conformance, while
Tier C would require a different value model, a different integer strategy and a different
memory model.

A target that is not in CI is not Tier A, whatever the specification claims elsewhere.

---

## 48. Versioning and Compatibility

```text
0.x    no stability promise; the C ABI is not published
1.0    C ABI frozen; semantic versioning begins
```

After 1.0:

* the C ABI is additive only; no signature or struct layout changes
* descriptor structs carry a size field and a version, so attributes can be added
* the C++ API changes in a minor release only in source-compatible ways
* the provenance report schema is versioned independently, and Doctor refuses to compare
  reports across a breaking schema boundary rather than comparing them incorrectly

Publishing a C ABI before 1.0 would freeze exactly the decisions Phase 3 exists to
validate. This is why the ABI is a Phase 3 deliverable.

---

## 49. Distribution

ConfigForge is a compiled library rather than a header-only one. A fully header-only
architecture becomes steadily more expensive as optional providers, parsers, OS
integrations and watchers are added, and it is not available to a C library in any case.

```text
ConfigForge Core     compiled library, C89
C API                the public interface
C++ wrapper          header-only, thin, C++03
Providers            independently compiled optional components
Doctor               separate binary
```

### Distribution Forms

```text
Amalgamation     generated configforge.c + configforge.h, dropped into a project
                 with no build system change at all
CMake package    find_package(ConfigForge) or FetchContent
System package   later, once the interface is frozen
```

The amalgamation is listed first deliberately. SQLite's reach owes as much to `sqlite3.c`
as to its portability: a developer evaluating the library copies two files and compiles
them with whatever they already have, and meets the build system only after deciding to
keep it. For a library whose premise is reaching old and unusual toolchains, the
distribution form that requires no build system is the important one.

The amalgamation is generated and tested in CI from the real sources, never maintained by
hand. It contains the built-in INI parser rather than an optional one, which is what keeps
it dependency-free.

Static linking remains fully supported.

---

## 50. Native OS Configuration

OS-native configuration is an important optional source category.

The design is inspired by systems such as `QSettings`, where platform-specific configuration mechanisms can be exposed through a common API.

Examples may include:

* Windows Registry
* macOS preferences
* Linux desktop configuration systems
* Other platform-native stores

These should be optional providers rather than core dependencies.

---

## 51. Build-Time Configuration

Build-time values may be represented as read-only configuration sources.

Examples include:

* Compiler definitions
* Generated headers
* CMake-generated values
* Build metadata
* Platform information

Runtime ConfigForge operations must not pretend that a build-time value can be modified.

The source capability model naturally represents this:

```text
BuildSource
    READ
```

---

## 52. Configuration Mapping

Definitions may optionally map parameters to source-specific representations.

For example:

```text
server.port

Environment:
    MYAPP_SERVER_PORT

Command Line:
    --server-port

File:
    server.port
```

This mapping is explicit.

Defining a parameter does not automatically create variables or files in every configured source.

---

## 53. Configuration Lifecycle

A configuration context follows a conceptual lifecycle:

```text
Create
  ↓
Register Sources
  ↓
Register Definitions
  ↓
Load Policy Sources
  ↓
Compile Policy
  ↓
Validate Capabilities / Policy
  ↓
Load Source Data
  ↓
Resolve
  ↓
Create Snapshot
  ↓
Run
  ↓
Watch / Reload / Runtime Mutation
  ↓
New Snapshot
```

Policy changes may trigger recompilation and re-resolution.

Existing snapshots remain immutable.

---

## 54. Isolation and Shared State

ConfigForge should not use implicit global mutable state.

Each `ConfigContext` defines its own configuration universe.

Applications that require shared configuration may explicitly share a context.

This makes behavior predictable in:

* multi-threaded applications
* libraries embedded into larger applications
* tests
* plugins
* multiple application components
* multiple independent configuration environments

---

## 55. Testing Strategy

The architecture should make deterministic testing possible without requiring real operating-system resources.

Every major abstraction should be testable independently.

Examples:

```text
Definition Tests
Source Tests
Capability Tests
Policy Tests
Policy Compiler Tests
Resolver Tests
Snapshot Tests
Materializer Tests
Watch Tests
Diagnostics Tests
```

A fake in-memory source should be sufficient for most core tests.

The resolver should be testable as a deterministic component without filesystem or environment dependencies.

---

## 56. Repository Layout and Tooling

Implementation lives in its own repository. The layout is fixed now because the
directory structure encodes the architectural boundaries, and a boundary that is not
visible in the file system does not stay enforced.

```text
configforge/
├── CMakeLists.txt
├── Makefile                     plain build for toolchains without CMake
├── include/
│   └── configforge/
│       ├── configforge.h        C API, C89, C11 enhanced
│       ├── configforge.hpp      C++03 wrapper
│       ├── configforge11.hpp    optional C++11+ conveniences
│       └── cf_port.h            feature detection, cf_i64, cf_bool, atomics
├── src/
│   ├── core/                    no OS, no allocation assumptions
│   │   ├── port.c               fallbacks: formatter, ASCII case folding, atomics
│   │   ├── value.c              five types, locale-independent numeric conversion
│   │   ├── keyspace.c           sorted flat map, key normalisation
│   │   ├── define.c             definitions, contract
│   │   ├── policy.c             policy model, compiler, effective policy
│   │   ├── resolve.c            resolver, provenance chain, array truncation
│   │   ├── snapshot.c           storage, reference counting
│   │   └── alloc.c              pluggable allocator, arena
│   ├── sources/                 default, memory, env, cli, file
│   ├── diag/                    explain, inspect, report serialisation
│   └── platform/                per-OS providers; later phases
├── tests/
│   ├── unit/
│   ├── golden/                  explain() output against fixtures
│   ├── fuzz/                    key mapping, file parsing, report reading
│   └── conformance/             one suite, run against every build profile
├── bench/
├── tools/
│   ├── amalgamate/
│   └── doctor/                  separate binary
├── examples/
└── docs/
```

**Two rules, both checked by script in CI.**

1. `src/core` must not include anything from `src/sources` or `src/platform`.
2. No file outside `cf_port.h` and `port.c` may contain compiler or standard detection
   macros.

Every portability claim in the support matrix collapses the first time either rule is
broken quietly.

### Tooling

| Concern | Choice | Note |
| ------- | ------ | ---- |
| Build | CMake 3.10, plus a hand-written Makefile | old CMake floor, because a new CMake on an old toolchain is a separate problem |
| Conformance gate | `-std=c89 -pedantic -Werror` on GCC and Clang | plus a standalone C89 compile of the public header |
| Portability gate | full suite under `CF_FORCE_PORTABLE=1` | output compared byte for byte against the normal build |
| Tests | a small C test harness, or doctest for the C++ layer | test code is not bound by the core's constraints |
| Golden tests | text fixtures | `explain()` output is a user-facing contract and is tested as one |
| Fuzzing | libFuzzer or AFL | key mapping, file parsing and report reading are the real attack surface |
| Sanitizers | ASan, UBSan, TSan | on the modern toolchain; TSan specifically on snapshot acquire and release under reload |
| Static analysis | clang-tidy, cppcheck | cppcheck because it works against C89 without a modern compiler |
| Formatting | clang-format | enforced in CI |

**Development model.** Develop on a modern toolchain, where sanitizers, fuzzing and
static analysis actually work. Treat the old compilers as a conformance gate, not as a
development environment. Writing C89 in 2026 does not mean using a 2005 compiler daily; it
means writing nothing a 2005 compiler would reject.

### Tests That Match This Design

**Determinism.** The same sources and policy produce the same snapshot, byte for byte,
across runs, platforms and portability modes. Asserted as a hash over the serialised
snapshot.

**Provenance completeness.** For every key in every fixture, `explain()` names a source
and a reason, and the named source actually contains the named value. A property test,
not a list of examples.

**Array truncation.** A fixture set in which a high-precedence source shortens an array
supplied by a lower-precedence one, asserting that no ghost element survives and that the
discarded candidates remain visible in `explain()`.

**Locale independence.** The whole suite runs under a comma-decimal locale and under a
Turkish locale, asserting identical output. The second is not a curiosity; it is the
standard test for the `tolower` hazard described in the portability layer.

**Profile equivalence.** The conformance suite runs unchanged against every build profile
and produces identical results except where a source is unavailable by construction.

---

## 57. Security and Safety Principles

ConfigForge should follow several strict principles:

1. Reading configuration must not mutate sources.
2. `set()` must not persist data by default.
3. Persistent writes must be explicit or explicitly enabled through propagation.
4. A policy cannot grant capabilities a source does not possess.
5. Conflicting policies must not be silently resolved without a defined precedence.
6. Source failures must be observable.
7. Propagation must report partial failures.
8. Secrets should not automatically appear in diagnostics.
9. Sensitive values should support redaction in inspection and tracing.

Secrets management itself is outside the initial core scope.

---

## 58. Future Direction

The architecture should leave room for future capabilities without making them mandatory.

Potential future extensions include:

* Remote configuration providers
* Distributed configuration
* Advanced transactional writes
* Rollback mechanisms
* Configuration synchronization
* Secrets providers
* Schema registries
* Additional declarative policy formats
* WASM support
* Language bindings
* Dynamic provider loading

These should be implemented as extensions rather than requirements of the core.

---

## 59. Target Environments

The core must not assume that environment variables, command-line arguments, a
filesystem, an unbounded heap, threads or a locale exist. It does assume C99 and a
64-bit integer type; the support matrix states where that stops.

| Environment | Available sources | Notes |
| ----------- | ----------------- | ----- |
| Linux, macOS, BSD | all | reference target |
| Windows | all, plus Registry | UTF-16 at the boundary, UTF-8 internally |
| WASI | build-time, file, environment, in-memory | CLI depends on the host |
| WASM in browser | build-time, in-memory, fetched blob, Web Storage provider | no environment, no CLI, no filesystem |
| Android, iOS | build-time, bundled file, native preferences | no meaningful environment or CLI |
| RTOS (Zephyr, FreeRTOS) | build-time, in-memory, flash key-value provider | fixed-capacity mode |
| Bare metal, no OS | build-time, in-memory | no allocation, no I/O |

### Build Profiles

Portability ships as three profiles rather than as one adjective.

| Profile | Contents | Status |
| ------- | -------- | ------ |
| **Hosted** | all sources, standard allocation, threads | Phase 1; the reference profile |
| **Constrained** | pluggable allocator, file and memory sources, optional single-threaded | Phase 3; in CI |
| **Freestanding** | fixed-capacity arena, build-time and memory sources only, no libc beyond a listed set | Phase 5; designed for, validated when a consumer exists |

The three do not cost the same and the specification should not pretend otherwise.

The *discipline* that makes Freestanding possible — a pluggable allocator, no locale
dependency, bounded buffers, no hidden allocation — is cheap early and expensive to
retrofit, because retrofitting means rewriting every allocation site. It is adopted from
the first commit.

*Validating* Freestanding requires a board, a toolchain and a CI runner, and that cost is
only worth paying once something actually runs there. Claiming bare-metal support with no
bare-metal consumer is the same error as publishing an ABI with no binding: an
unvalidated promise that constrains every later decision.

### Consequences for the Core

* **Allocation.** A pluggable allocator, plus a fixed-capacity mode where all storage
  comes from a caller-supplied arena and resolution fails cleanly on exhaustion.
* **Locale.** Numeric parsing and formatting are implemented in the library. See the
  language section.
* **Encoding.** UTF-8 internally on every platform; conversion happens inside the
  platform provider.
* **Threads.** Usable single-threaded with synchronisation compiled out, which is also
  the fallback for toolchains with no atomic support.
* **Time and filesystem.** Used by providers only.

This is why the source set belongs to providers and not to the core. The core that
survives bare metal is the value model, the key space, the policy model, the resolver,
the snapshot and the provenance chain. Everything else is optional by construction.

---

## 60. Cross-Ecosystem Influences

The design borrows deliberately. Each row states what is taken and, where relevant,
what is explicitly rejected.

| Source | Taken | Rejected |
| ------ | ----- | -------- |
| .NET `IConfiguration`, `GetDebugView()` | **the flat key space itself**, and per-key provider attribution as a first-class API | provider-order-only precedence with no per-parameter policy; index-based array merging, whose ghost-element defect is fixed by the truncation rule |
| Spring Boot `OriginTrackedValue`, `/actuator/env` | flat `PropertySource` model; origin attached to the value itself; a matrix view | framework coupling; HTTP as the only access path |
| Git `config --list --show-origin --show-scope` | scope vocabulary and origin output as a plain CLI feature | untyped, string-only values |
| Rust `figment` | provenance carried into error messages; profiles | |
| Rust `config-rs` | layered source builder | |
| Go `viper` | the source set and the ambition | implicit behaviour, case-insensitive keys, silent precedence surprises |
| Nix module system | options declared with type, default and description; numeric override priorities | configuration as a full programming language |
| Terraform | a plan step before applying; a documented variable precedence chain | state ownership |
| Ansible | cautionary example: 20+ precedence levels with no explain facility | |
| Hydra, OmegaConf | printing the fully composed configuration; config groups | interpolation inside the core |
| pydantic-settings | the definition as a typed contract validated at the boundary | |
| Elixir `config.exs` vs `runtime.exs` | explicit split between build-time and runtime configuration | |
| Erlang/OTP application environment | per-component isolated configuration namespaces | |
| systemd drop-in directories, `systemd-analyze cat-config` | directory layering with lexical order; a command that prints the merged result with origins | |
| OpenWrt UCI | write-back with an explicit commit step on embedded targets | |
| Kubernetes, Helm | deep merge for maps, replacement for lists | |
| Chrome `chrome://policy` | policy layering with an effective-policy view | |
| Emacs `describe-variable` | value plus where it was set, as an everyday tool | |
| dotenv | the value of a one-line entry point | one line as the *only* entry point |
| Zephyr Kconfig | build-time configuration for targets without a filesystem | |

The pattern in this table is worth stating plainly: **origin tracking is a solved,
proven idea in managed-language ecosystems and absent in C and C++.** ConfigForge is
not inventing a capability; it is porting a known good one into a language family
that lacks it.

---

## 61. Report Replay

The provenance report exists so that Doctor can compare environments. The same artifact
has a second use that costs almost nothing to support and is worth more than it looks.

**A provenance report can be loaded as a source.**

```text
myapp --cf-replay prod-report.json
```

The report contains every resolved value together with its origin. Loading it as a single
high-precedence source reproduces, on a developer's machine, exactly the configuration a
production process was running. Not an approximation of it, and not a reconstruction from
files that may have changed since.

Three uses follow directly:

* **Reproducing an incident.** "It only misbehaves in production" becomes a local
  reproduction rather than a guess.
* **Deterministic tests.** A captured report is a fixture. A suite can replay a real
  production configuration without production's files, environment or secret store.
* **Bisecting configuration.** Two reports from two deploys, replayed alternately, find
  which key actually changed the behaviour.

### Constraints

* A value-blind report cannot be replayed, because it has no values. Replay needs a full
  report, and a full report contains secrets.
* A full report is therefore handled exactly like a credential: never committed, never
  attached to a ticket, produced on demand and deleted.
* Replay is a distinct source kind and appears as such in `explain()`. A value that came
  from a replayed report must never be mistakable for one that came from the environment.
* Replay is read-only and refuses to participate in propagation.

The capability exists because the report is a complete serialisation of the key space
rather than a diagnostic dump. That was already required for Doctor. Replay is what the
same decision buys a second time, and it is the strongest argument for specifying the
report carefully in Phase 1.

---

## 62. ConfigForge Doctor

The provenance chain has a second use beyond `explain()`. Once resolution can report
where every value came from, the same report can be produced for two environments and
compared before deployment rather than after an incident.

`ConfigForge Doctor` is a separate command-line tool that consumes provenance reports
and answers questions no single running process can answer alone:

* which values are set but unreachable, because a higher-precedence source always wins
* which values agree between two environments but arrive from different sources
* which keys are overridden in staging and not in production

It is specified in [Doctor.md](Doctor.md).

The only obligation it places on the library is the **provenance report artifact**: a
versioned, machine-readable serialisation of the inspection matrix, emitted on demand
and capable of omitting values while retaining origins. That artifact is a Phase 1
deliverable. Retrofitting it later means redesigning the diagnostics output a second
time.

---

## 63. Roadmap

The ordering below deliberately differs from a feature-grouped roadmap. Two items in
the natural grouping force a rewrite if they are scheduled late.

**Provenance cannot be added after the resolver.** A resolver that selects a winner
and discards the rest cannot later explain itself. The provenance chain is therefore
the core data structure from the first line, and `get()` is the winner of that chain
rather than a separate mechanism.

**A C ABI cannot be validated without a consumer.** An ABI designed before any
language layer exists is a guess, and correcting it after publication is a breaking
change. The ABI and the first ergonomic layer over it are therefore built together.

---

### Phase 0 — Decisions

No code. Every decision in [Decisions.md](Decisions.md) is closed and recorded with
its rationale and consequences.

The specification is then frozen. From that point, a new section has to be paid for
with code.

---

### Milestone M0 — Walking Skeleton

Phase 1 is large enough to stall inside. M0 is the smallest thing that proves the
architecture end to end, and it is the first commit-to-demo target.

**Scope.**

* `cf_app(name, argc, argv, &err)` over defaults, one file, environment and command line
* The five value types, flat key space
* No arrays, no policy, no definitions, no write, no watch, no C++ layer
* Provenance chain retained from the first line
* `explain(key)` and `inspect()` as text

**Definition of done.**

1. The C program in the public API section compiles and runs on Linux, macOS and
   Windows, and passes `-std=c89 -pedantic -Werror`.
2. `explain()` output matches a golden fixture across a fixture set in which every
   source wins at least once.
3. The determinism hash is identical on all three platforms.
4. A deliberately malformed value produces one readable error at construction and
   nothing at read time.
5. No locale-dependent conversion anywhere, verified by running the suite under a
   comma-decimal locale.
6. The public header compiles standalone as `-std=c89 -pedantic`.
7. The suite passes identically with and without `CF_FORCE_PORTABLE=1`, and under a
   Turkish locale.

**Outcome.** Complete. Roughly 4,000 lines of library and 3,400 of tests and examples,
against an estimate of 2,500 to 3,500 for the library, so the estimate was low by about a
quarter. Two things account for most of it and both were found by writing the code rather
than by reading the design:

* the exact floating-point digit generator (D-25) turned out to subsume the shortest
  representation algorithm, which was worth the extra 300 lines and was not planned
* sources had to record text rather than typed values, because a source cannot know
  whether a definition exists for the key it just read. Typing became a separate pass.
  The specification already said the declaration decides the type; the implementation is
  what revealed that a source is the wrong place to decide it.

Everything the definition of done asked for is enforced by a test rather than asserted in
prose: the golden fixture is compared line by line, the digest is checked identical across
the default, portable and minimal builds, the public headers are compiled standalone as
C89 by a gate, and the malformed-value path is a test that asserts the error names the
value, the bound and the place to fix it.

---

### Phase 1 — Provenance Core

* Value model
* Definitions and the definition contract
* Source abstraction and capabilities
* In-memory, default, file, environment and command-line sources
* Deterministic resolver built on a provenance chain
* Merge semantics
* Immutable, reference-counted snapshots
* Error and result model
* Locale-independent numeric parsing
* `explain()` and matrix `inspect()`
* Provenance report artifact, schema v1

Implemented behind a single internal interface, with no published ABI. At the end of
this phase the library is already usable and already does the one thing no comparable
C or C++ library does.

---

### Phase 2 — Policy

* Policy model and layers
* Policy compiler and effective policy
* Capability validation and conflict validation
* Parameter-level policy
* Policy reasoning inside `explain()` output

Programmatic policy only. No declarative format yet.

---

### Phase 3 — Language Layers and Profiles

* C++03 wrapper over the C API
* Optional C++11 convenience header
* Usage tiers 0 through 3 complete
* Constrained and fixed-capacity build profiles
* Full platform matrix in CI
* C API frozen at the end of this phase; 1.0 follows

The C API exists from Phase 1 because it *is* the API, not an export layer. What Phase 3
adds is the first non-C consumer, and that is what validates the interface before it is
frozen. Freezing an interface that has only ever been called from C would repeat the
mistake this ordering exists to avoid.

---

### Phase 4 — Mutation and Lifecycle

* `set()`
* Explicit `write()`
* Materializer
* Propagation with per-source results
* Watch, reload and selective reload

Write support is initially restricted to a dedicated overrides file that ConfigForge
owns and rewrites in full. Format-preserving writes into hand-maintained files are a
separate problem and not a Phase 4 obligation.

---

### What Phase 1 Actually Cost

Recorded because the estimates are the only part of a plan that can be
checked afterwards.

```text
portability layer        ~700 lines   estimated 600
numeric conversion       ~800         estimated 300; Dragon4 subsumed the
                                      shortest-representation algorithm
core (value, keyspace,
  define, validate,
  config, alloc, error)  ~2,600
serialization layer      ~1,500       not in the M0 estimate at all
sources and flattener    ~1,000
diagnostics and report   ~900
C++03 wrapper            ~300
Doctor                   ~700
tests and examples       ~4,400
```

Three things were not in the plan and were found by writing the code:

* a source cannot know whether a definition exists for the key it just
  read, so typing had to become a pass of its own
* emission has to put leaves before containers at every level, or an
  INI writer silently moves a root key into the previous section
* two static functions in different files with the same name compile
  fine until the amalgamation puts them in one translation unit

---

### Phase 5 — Ecosystem

* TOML policy DSL, policy layers, runtime policy overrides
* ConfigForge Doctor
* Rust bindings
* Native OS providers
* Additional providers
* WASM research

ConfigForge Doctor may be started at any point after Phase 1, because its only
dependency is the provenance report artifact. It is listed here rather than earlier
because it is a separate deliverable, not because it is blocked.

---

### Deliberately Deferred

Retained in the design, not scheduled until a real user asks:

* CBOR or MessagePack: a third document codec whose only advantage over
  JSON is size, which is not the problem a configuration file has. The
  binary artifact that was needed is the snapshot image, and it is not a
  document. See D-28 and `docs/ImageFormat.md`.
* format-preserving file writing
* cross-source transactions and rollback
* value interpolation and references
* remote and distributed providers
* dynamic provider loading
* schema registries

---

## 64. Non-Goals

ConfigForge is not intended to become:

* A general-purpose database
* A secrets management platform
* A distributed configuration service
* A service discovery system
* A network synchronization framework
* A general-purpose serialization framework
* A dependency injection framework

These systems may integrate with ConfigForge through providers, but they should not become part of the core.

---

## 65. Architectural Principles

ConfigForge should preserve the following principles throughout development:

```text
Simple by default.
Explicit when behavior has side effects.
Deterministic by design.
Fast on the hot path.
Immutable snapshots.
No implicit global state.
Capabilities describe reality.
Policies describe permission and precedence.
Definitions describe application contracts.
Sources provide data.
Resolvers resolve.
Materializers propagate.
Diagnostics explain.
Providers extend.
```

The most important rule is:

> **Convenience must never hide a significant side effect.**

---

## 66. Project Vision

ConfigForge is intended to become a general-purpose configuration infrastructure rather than another configuration-file parser.

Its long-term value comes from treating configuration as a system:

```text
Sources
    +
Definitions
    +
Policies
    +
Resolution
    +
Immutable Runtime State
    +
Controlled Mutation
    +
Diagnostics
```

The result should be a library that can be used for a simple desktop application with a few environment variables, while still being capable of supporting complex applications with multiple configuration files, runtime overrides, native OS settings, custom providers, policy layers, hot reload, and performance-sensitive embedded environments.

The core should remain small, predictable, portable, statically linkable, and free of unnecessary mandatory dependencies.

**ConfigForge should make configuration management easier without taking control away from the programmer.**
