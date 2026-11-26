# Research: a configuration compiler

Research for the Roadmap item "A configuration compiler" (docs/Roadmap.md,
"From the maintainer, 2026-11-26"). Not a card and not a decision: what
the idea means here, what the world does, what breaks, what fits this
library, and what is left for the maintainer to decide. Written
2026-11-26.

## 1. The question

**As the maintainer asked it** (Turkish, translated): "Something like a
configuration compiler. Not just a text merger, parser and converter, but
a configuration compiler. I have not made it fully clear, and I do not
know the competitors either."

**As the Roadmap holds it** (docs/Roadmap.md:1576-1584): sources, contract
and rules in, one checked artifact out, with what a compiler gives (a
front end, an intermediate form, passes that check and fold, diagnostics
with places, a target chosen). To start from: the image format
(docs/ImageFormat.md), a baked configuration read with no parser linked
(D-104, layer 1), `configforge gen`.

### 1.1 What "compiler" can mean here

A compiler is a front end per input language, one intermediate form,
passes over that form that check and rewrite it, diagnostics that name a
place, and back ends that write a target. Run at build time, it moves
work and failure out of the running program. For configuration that is:

| Part | Here | Today |
| ---- | ---- | ----- |
| Front ends | one reader per format, into document events, then flattened to dotted keys | exists |
| Declarations | the contract: typed keys, ranges, allowed spellings, policy | exists |
| Intermediate form | the key space: every candidate of every key, with rank, place, outcome and reason | exists |
| Passes | precedence, per-key policy, type and range checks, references, lists, renamed keys, unknown keys | exist, at run time |
| Passes missing | rules across keys; every error rather than the first; what stays open for run time | missing |
| Diagnostics | a code, a reason, a key, a source, a line and column | exist, one at a time |
| Driver | a command that takes sources, contract and target, and writes the artifact | missing |
| Targets | the image, baked C tables, generated code, a normalised document, a report, an artifact with a manifest and a digest | some exist, as C calls; none from one command |

The finding of this section: the library already is a compiler in all
but its driver. Its readers are front ends, its key space is an
intermediate form that keeps every place, and `cf_builder_create` runs
the passes. What it lacks is the build-time driver, the passes that only
make sense before run time, and a manifest that says what was built from
what.

### 1.2 What exists, part by part

**Front ends.** One reader per format under `lib/serialize/src/` (JSON,
YAML, TOML, INI, XML, CBOR, properties, TLV), the format found by name
then content (D-73, `lib/serialize/src/detect.c:251-341`), composed
codecs for what is almost a format (D-103,
`lib/serialize/src/compose.c:187-262`). Sources around them: file,
directory, environment, command line, dotenv, secrets, registry,
preferences, a report replayed, an image (the `cf_builder_*` list in
`lib/config/include/configforge/configforge.h`).

**Declarations.** `cf_def` (`lib/config/include/configforge/cf_define.h:68-134`):
type, flags (`CF_REQUIRED`, `CF_SENSITIVE`, `CF_DEPRECATED`, ...),
default, range, lengths, allowed spellings, aliases, the per-key policy
(`cf_define.h:101-114`, D-33), list mode. Read from a JSON Schema
(`cf_builder_schema`, `configforge.h:285`, D-64) and written back as one
(`cf_schema_write`, `lib/config/include/configforge/cf_report.h:102`).

**Intermediate form.** `cf_keyspace` (`lib/config/include/configforge/cf_keyspace.h:185-201`):
entries sorted by key, each with its chain of `cf_candidate`
(`cf_keyspace.h:125-134`): the rank, the value, the place (`at`), the
outcome (`pending`, `selected`, `shadowed`, `rejected`, `truncated`;
`cf_outcome.def:25-29`), the reason, the template as written. The flags
(`cf_keyspace.h:72-123`) already separate what somebody wrote, a default,
a template, a renamed key, a resolved snapshot. This is a richer form
than most configuration languages keep after evaluation: every losing
candidate is still there.

**Passes.** `cf_defs_apply` (`lib/config/src/core/validate.c:1075-1238`)
runs them in one order: per-key policy (`apply_policy`, `validate.c:644-703`),
type, range, allowed and length (`validate_value`, `validate.c:40-116`),
references after precedence (`expand_entry`/`expand_all`,
`validate.c:452-581`, D-10, bounded by `CF_EXPAND_MAX` and
`CF_EXPAND_TOTAL_MAX`, `cf_define.h:155-173`), lists cut and appended
(`cut_arrays`, `validate.c:859-882`; `validate_append_lists`,
`validate.c:914-1032`). Defaults and overrides are "folded" by
precedence: lower rank wins (`CF_RANK_OVERRIDE` -10 to `CF_RANK_DEFAULT`
40, `lib/config/include/configforge/cf_source.h:27-32`). Nothing checks
one key against another: the contract is per key.

**Diagnostics.** `cf_error` has a code, the key, the source, a message,
and since 1.8 a place and a reason (`lib/base/include/configforge/cf_error.h:51-59`,
D-107). A refused candidate keeps its reason and its refusal kind
(`cf_keyspace.h:143-152`). `cf_explain` (`configforge.h:578`) and the
report (`cf_report_write`, `cf_report.h:65`) say why each value is what
it is. Doctor's rules CFD001 to CFD017 (docs/Doctor.md, section 8) read
the report. But `cf_builder_create` (`configforge.h:447`) stops at the
first refusal of a strict key, and `gen check` reports only the first
refusal by design (D-67: "rather than a second engine collecting all of
them").

**Targets.**

* **The image** (D-28, docs/ImageFormat.md, `cf_image.h:54-115`): the
  resolved key space as a header, a sorted index, values and strings,
  little endian, read in place with no parse and no allocation, an
  optional chain so `explain` works on a device. Written only by a C call
  (`cf_image_write`, `lib/config/src/bridge/image.c:194-328`); no command
  writes one; `examples/c/embedded.c:30-55` bakes one by hand. It carries
  no contract, and the source view is unused (`image.c:200`, `(void)sv;`):
  places survive in the chain, source names do not. Its digest is the
  64-bit FNV-1a "determinism digest" of the key space
  (`cf_ks_digest`, `lib/config/src/core/keyspace.c:1048-1081`): it detects
  truncation, it proves nothing. It writes every key that has a value,
  defaults included.
* **A baked configuration** (D-104 layer 1,
  docs/plan/A-architecture.md:263-271): `configforge gen --bake <file>
  --schema S` writes C89 `static const` tables with provenance, read by
  `cf_config_from_baked()` with no reader linked. Designed, not built;
  in the 1.9.0 row of docs/Maintaining.md.
* **Generated code from the contract**: `configforge gen --lang c|cpp|rust|python|contract-json`
  (`tools/gen/gen.c:5-8`; the table at `tools/gen/contract.c:2117-2125`,
  D-95), and any other language by a program on the PATH,
  `configforge-gen-<lang>`, handed the contract as JSON (D-110). This is
  protoc's plugin model (section 2.14). It writes the contract, not
  values.
* **A normalised document**: `cf_emit` (`lib/config/include/configforge/cf_emit.h:24-56`)
  writes a resolved configuration in a format, all keys or only what a
  source configured (`CF_EMIT_CONFIGURED`, which leaves the program's
  defaults out so that a later release can move them). For one document,
  `configforge codec format` and `convert` (`tools/codec/codec.c:10-11`).
* **A report**: the full or value-blind provenance report
  (`cf_report`, `configforge.h:655-667`, D-16), read back as a source by
  `cf_builder_replay` (`configforge.h:689`). A report is already an
  artifact the library can load.
* **A digest**: `cf_digest` and `cf_fingerprint` (`configforge.h:640-652`),
  FNV-1a, "not cryptographic: it tells changes apart". SHA-256 "for a
  fingerprint that decides" is in the 1.10.0 row.

**The driver.** None. The program is the driver: it calls the builder.
The closest command is `configforge gen check --schema S F...`
(`tools/gen/contract.c:50-98`), which builds a configuration from files
against a contract and reports the first refusal; it writes nothing.

## 2. What the world does

Three families. **Languages** that compute configuration and export it
(CUE, Jsonnet, Dhall, Nickel, Pkl, KCL, Starlark, Nix). **Composers**
that combine documents without a language of their own, or with a
template (ytt, kustomize, Helm). **Planners and schema compilers** that
turn a description into a checked artifact for another program
(Terraform, Pulumi, protoc, capnp, Ion Schema, Facebook's Configerator).
docs/Market.md:61-75 already noted of the first family that "the user
adopts one as the source of truth; none reads an existing estate".

### 2.1 CUE

* **From and to.** `.cue` files, and JSON, YAML, TOML and more imported;
  `cue export --out cue|json|toml|yaml|text|binary` [2]. Export refuses a
  value that is not concrete [2].
* **Model.** Types are values; every value sits in a lattice; `&`
  unifies two values to their greatest lower bound, so the order of
  merging does not change the result [1]. Constraints (`<10`) and data
  combine by the same operation [1]. A conflict (`true & false`) is
  bottom, an error [1]. Defaults are marked `*` in a disjunction and
  apply only when nothing conflicts [1].
* **No override.** Two different values for one field are a conflict,
  not a precedence question. There is no "later wins". A default is the
  only soft value.
* **Errors.** A conflict names the values on both sides and where each
  was written.
* **Embedding.** A Go API (`cue/load`, `cuecontext`, `Unify`,
  `Validate`); a schema can be compiled into a Go binary with
  `//go:embed` [3]. `cue get go` turns Go types into CUE [3]. No C API.
* **Cost.** The evaluator was rewritten (evalv3, default since v0.13)
  for performance on large configurations, with a new disjunction
  algorithm [4]; earlier releases admit exponential behaviour in some
  cases [5].

### 2.2 Jsonnet

* **From and to.** `.jsonnet` to JSON; designed so that any system that
  takes JSON takes Jsonnet through one conversion step [6].
* **Model.** Pure, hermetic (the same output on every machine), lazy,
  prototype objects with `self` and `super` and mixin inheritance [6].
  Deliberately Turing-complete: termination was judged not worth
  enforcing [6]. Inputs from outside come only through named external
  variables and top-level arguments.
* **Types.** Dynamic, checked at run time; no static checking, by
  choice [6].
* **Errors.** Stack traces through the nested context [6]. Laziness
  means an error in a field shows only when the field is forced [6].

### 2.3 Dhall

* **From and to.** `.dhall`, to JSON and YAML through `dhall-to-json`
  and `dhall-to-yaml` [7]; several implementations follow one standard
  [7].
* **Model.** Typed, total: evaluation always terminates [7]. Every
  expression normalises to a form with no indirection left [7]. The
  standard defines type inference, beta normalisation, import resolution
  and a binary encoding [8].
* **Imports.** An import may carry `sha256:<hash>` of its normal form;
  a mismatch must fail, and the cache is keyed by that hash [9]. This is
  a digest of meaning, not of bytes: a reformatted file keeps its hash.
* **Cost.** Normal forms can be large and slow to reach: one project
  reported a normal form of about 6 MB taking 90 seconds [10].

### 2.4 Nickel

* **From and to.** `.ncl`, exported to JSON, YAML or TOML.
* **Model.** Gradual typing plus contracts: run-time checks attached
  with `|`, lazy on records, so a failing field does not break unrelated
  ones [11]. Blame names the party at fault, caller or function, and is
  pushed to the place of the offending value [11].
* **Merge with priorities.** `&` merges records recursively and is
  commutative; `default` is lowest, numeric priorities between, `force`
  highest; a higher priority replaces a lower one; equal priorities must
  agree or fail with "non mergeable terms" [12]. A field's contract
  applies to the merged value, even an overridden one [12]. Of all the
  languages, this is the closest to ranked sources.

### 2.5 Pkl (Apple)

* **From and to.** `.pkl`, rendered to JSON, YAML, plist, properties,
  XML, Pkl's own PCF, Jsonnet and protobuf text; several files from one
  module [13].
* **Model.** Lazy, late-bound, immutable objects; `amends` derives an
  object from another, prototype style; classes with single inheritance;
  type constraints such as ranges; nullable by opt-in [13]. Modules from
  files, HTTPS and packages, with an allowlist [13].
* **Errors.** Type mismatches and unknown properties fail at evaluation;
  `trace` prints the expression with file and line [13].
* **Code and run time.** Generators for Java, Kotlin, Swift and Go write
  types from modules [13][14]. But the Go and Swift bindings evaluate by
  spawning the `pkl` program as a child (`pkl server`) and exchanging
  MessagePack messages; the result comes back in Pkl's binary encoding
  [15][16]. The program needs the Pkl evaluator at run time; nothing is
  folded at build time.

### 2.6 KCL

* **From and to.** `.k` files, rendered to configuration such as
  Kubernetes YAML; schemas imported from Terraform provider schemas and
  Kubernetes CRDs [17].
* **Model.** Compiled and statically typed, with inference; schemas
  separate definition from data; built on Rust, C and LLVM with native
  and WebAssembly targets [17].
* **Rules.** A schema's `check:` block holds boolean expressions, each
  with its own message; any false one is an error [18]. Attributes have
  defaults, `?` marks an optional one [18]. This is the one language here
  with rules across fields written beside the schema.

### 2.7 Starlark and Bazel

* **Model.** A Python dialect for configuration, embedded in a host
  program: deterministic, hermetic, no recursion, no unbounded loops,
  values frozen once a module has run, so many threads can load modules
  at once [19]. Dynamically typed, no user types [19].
* **Use.** Bazel's BUILD files and rules evaluate to a graph the host
  then acts on; ytt (2.10) uses it for templates.

### 2.8 HCL and Terraform's plan

* **HCL.** One information model under two syntaxes (native and JSON);
  a body's content is opaque until the application gives a schema of
  attributes and blocks; anything not in the schema is an error by
  default [20]. Expressions are evaluated against variables and
  functions the application supplies; unknown values stand for what is
  not known yet, so types can be checked before the values exist [20].
* **`terraform validate`.** Checks syntax and consistency without state
  or remote APIs; `-json` gives every diagnostic with a severity, a
  summary, a detail and a range: file, start and end line, column and
  byte offset [22]. This is the diagnostic shape a compiler of
  configuration should copy.
* **`terraform plan`.** Compares the configuration with recorded state
  and proposes actions; `-out` saves a plan that `apply` later runs
  exactly [21]. The saved plan holds the whole configuration and its
  values in clear text and must be treated as sensitive [21]. Values
  known only after apply cannot be tested at plan time [23].

### 2.9 Nix and the NixOS module system

* **Nix.** A lazy, pure language whose evaluation produces store
  derivations (`.drv`); each output path contains a hash of all the
  build's inputs [24]. Input-addressed: the same inputs, the same path.
* **NixOS modules.** Options are declared with a type before they may be
  set; definitions from many modules are merged by type (lists
  concatenated), and a unique option defined twice is an error [25].
  Every definition has a numeric priority: plain 100, `mkDefault` 1000,
  `mkForce` 50, the option's own default 1500; only the definitions of
  the lowest number survive [26]. A conflicting declaration names the
  files that declared it [26]. Typed declarations, ranked definitions,
  and an error that names files: the nearest thing in the world to this
  library's contract plus ranked sources.

### 2.10 ytt (Carvel)

YAML templating that knows the YAML structure, so no quoting or
indentation by hand; logic in Starlark; data values supplied per
environment; overlays to change what a template produced; sandboxed and
deterministic [27].

### 2.11 kustomize

No templates and no language: a base, overlays built on it, patches
(strategic merge or JSON 6902), generators and transformers; `kustomize
build` writes a resource list for `kubectl apply` [28]. Its types are
Kubernetes', borrowed.

### 2.12 Helm

Values from the chart's `values.yaml`, a parent chart's, `-f` files and
`--set`, later and more specific winning; `null` deletes a default [29].
Templates render text, so whitespace-significant YAML is produced by
string operations and checked only after rendering. A chart may carry
`values.schema.json`, checked against the merged values [30].

### 2.13 Pulumi

A program in a general-purpose language registers resources; the
engine diffs desired state against the last deployed state; `preview`
shows the plan and `up` applies it through provider plugins [31]. The
language is the program's own; the checked artifact is the plan.

### 2.14 protobuf: protoc, descriptors, text format

* **protoc** parses `.proto` files to descriptors and hands a
  `CodeGeneratorRequest` (the files to generate and every
  `FileDescriptorProto` they import, imports first) to a program named
  `protoc-gen-<name>` on its input; the plugin answers with files or an
  error [33]. One front end, one intermediate form, back ends outside
  the compiler: D-110 already copied this.
* **Text format** is used for configuration and tests; parsing needs the
  message's schema, unknown fields are refused, the specification is a
  draft reverse-engineered from the C++ implementation, and no
  implementation writes a canonical form [32].

### 2.15 Cap'n Proto

`capnp compile` parses schemas and passes the parsed tree, as a Cap'n
Proto message, to a plugin `capnpc-<lang>` [34]. A schema may declare
constants of struct type with literal values; they appear in generated
code [35]. A constant is data baked into the program in the format it
is read in, with no parse at run time: D-104's baked configuration, for
one schema language.

### 2.16 Ion Schema

Types are sets of values defined by constraints; a value is valid only
if it meets all of them; open content is allowed; schemas import types
from other schemas; documents are versioned (`$ion_schema_2_0`) [36].
A schema language and validator, not a compiler; listed because Ion is
on this library's 2.1.0 format list.

### 2.17 Configerator (Facebook)

The one published system that calls its tool a configuration compiler
[37]. A config's schema is written in Thrift; engineers write Python
programs that build the Thrift object, and the compiler writes it as
JSON; a validator written beside the schema runs on every config of that
type during compilation [37]. Dependencies between configs are written
as imports, like an include in C++, and extracted from the source
without a makefile [37]. The compiled JSON and its sources are both kept
in version control; a change goes through code review, integration
tests and a staged canary with automatic rollback [37]. The paper
reports configuration errors as a major source of outages [37], as does
the research on misconfiguration [39].

### 2.18 The lesson from Borg

Google's account of Borg, Omega and Kubernetes: configuration languages
tend to grow until they are Turing-complete, and the result is the
"configuration is code" that hard-coded parameters were removed to
avoid, with weaker debuggers and test tools than a real language; the
authors recommend a clean separation of computation and data [38].

### 2.19 Rules across keys: CEL

CEL is an expression language meant to be embedded: no side effects,
always terminates, cost bounded per construct, optional type checking
against declarations [40]. Kubernetes uses it for validation rules in
custom resource schemas [41]. Its implementations are Go, Java and C++;
none is C89.

### 2.20 Summary

| Tool | From | To | Types | Evaluation | Combining | Errors |
| ---- | ---- | -- | ----- | ---------- | --------- | ------ |
| CUE | CUE, JSON, YAML | CUE, JSON, YAML, TOML | values are types | unification, order-free | must agree; `*` default | both places of a conflict |
| Jsonnet | Jsonnet | JSON | dynamic | lazy, pure, Turing-complete | inheritance, later wins | stack trace |
| Dhall | Dhall | JSON, YAML | static, total | normalisation | functions | type errors |
| Nickel | Nickel | JSON, YAML, TOML | gradual + contracts | lazy | merge with priorities | blame with place |
| Pkl | Pkl | JSON, YAML, plist, XML, ... | classes, constraints | lazy, late-bound | amends | type and constraint |
| KCL | KCL | YAML, JSON | static, inferred | compiled | schema inheritance | check messages |
| Starlark | Starlark | the host's objects | dynamic | deterministic, frozen | the host's | stack trace |
| Terraform | HCL | a plan | schema per provider | partial, unknowns | modules, variables | diagnostics with ranges |
| NixOS | Nix | a system | option types | lazy, pure | numeric priorities | names the files |
| Helm | YAML + templates | YAML text | optional JSON Schema | text templating | later wins | after rendering |
| protoc / capnp | schema | code, descriptors | the schema's | none | none | parse errors |
| Configerator | Python + Thrift | JSON | Thrift | Python | imports | validators |
| ConfigForge | 9+ formats, env, CLI | image, report, document, code | the contract | resolve after precedence | ranks + per-key policy | first refusal, with place |

## 3. What breaks

1. **Turing-complete configuration.** A language that can compute
   becomes a program without a debugger [38]; Jsonnet accepts
   non-termination by design [6]. Configuration that never terminates,
   or that is hard to read, is the failure the Borg authors describe.
2. **Laziness hides errors.** Jsonnet and Nickel check a field only when
   it is used [6][11]; a field nobody exports carries a bad value
   unnoticed. A compiler that checks only what a target reads has the
   same hole.
3. **Evaluation cost.** CUE's disjunctions and Dhall's normal forms can
   grow fast [4][5][10]. A compiler that resolves a few thousand flat
   keys has no such cost, which is a reason to stay flat.
4. **Text templates of structured data.** Helm's templates write YAML as
   text, so indentation and quoting are the template author's [29]; ytt
   exists to remove exactly that [27].
5. **Values not known at build time.** Terraform cannot test values
   known only after apply [23]; HCL carries "unknown" through
   evaluation for that reason [20]. Here: the environment, the command
   line and secrets exist only when the program runs. A build-time result
   that pretends they are absent is wrong once they arrive.
6. **References across the build-time line.** A template `${a}` folded
   at build time, where `a` may still be set at run time, freezes the
   wrong value. D-10 expands after precedence and an image's values are
   not expanded again (`cf_keyspace.h:74-80`), so a compiled template
   cannot follow a later change of the key it names.
7. **Defaults frozen into the artifact.** The image writes every key
   with a value, defaults included (section 1.2). Loaded at a file's
   rank, a baked default outranks the program's own, so a program
   upgraded without its artifact rebuilt keeps the old default. That is
   Doctor's CFD017, "default drift", made permanent. `cf_emit` already
   avoids it with `CF_EMIT_CONFIGURED` (`cf_emit.h:30-33`).
8. **Secrets in the artifact.** Terraform's saved plan carries every
   value in clear text [21]; Helm and kustomize output likewise. A
   compiled configuration with a `CF_SENSITIVE` value in it is a
   credential-bearing file, like a full report (docs/Doctor.md, 12a).
9. **The first error only.** One refusal per run makes a user fix,
   rerun, fix. `terraform validate -json` returns every diagnostic with
   a range [22]; CUE reports every conflict. D-67 chose the first refusal
   for `gen check`; a compiler run in CI should report all.
10. **Two engines that disagree.** Pkl's generated types and its
    run-time evaluator are separate programs [14][15]; CUE's Go API is the
    only embedding [3]. Wherever the build-time checker and the run-time
    reader are different code, a file can pass one and fail the other.
    D-104's rule (every build-time result compared with layer 0) exists
    against this.
11. **Unstable or uncanonical output.** Protobuf's text format has no
    canonical writer [32]. An artifact whose bytes change with nothing
    changed defeats a digest and a review; a timestamp in a report does
    the same unless it comes from `SOURCE_DATE_EPOCH` [42].
12. **A digest that proves nothing.** The image's FNV-1a digest catches
    a partial write (docs/ImageFormat.md); it does not stop a forged
    image. Dhall's import hash is SHA-256 of the normal form [9]; Nix
    hashes inputs [24]. An artifact that a policy "approved" needs a
    cryptographic digest, which 1.10.0 plans.
13. **Hostile artifacts.** The image is the one reader that follows
    offsets from its input; a fuzzer found an unchecked index on its
    first run (docs/ImageFormat.md, "Every number in an image is a
    claim"). Any new target read at run time (baked tables are compiled,
    so not this one) inherits the rule.

## 4. What fits this library

Measured against D-100 to D-108 and the C89 core.

### 4.1 The thesis: one engine, two times

Every language in section 2 has its own evaluator, and the program that
consumes the result reads plain JSON with its own reader. The compiled
output forgets its sources (Configerator keeps them only in version
control [37]); the consumer's run-time overrides then happen in another
system with another notion of precedence.

This library can do what none of them does: **the compiler is the
library's own resolver, run at build time**, writing an artifact that the
same library loads at run time as one ranked source among the others.
Nothing is checked at build time that the run time would not check, and
the artifact still answers `explain`. That is D-104's rule applied to a
whole configuration, and it rules out a second engine (section 3, item
10).

What follows from it:

* **Ranked sources stay.** The world's combinators are unification
  (CUE, must agree), inheritance (Jsonnet, Pkl, later wins), priorities
  (Nickel, NixOS) and templates (Helm, ytt). The library's ranks are
  priorities tied to where a value came from, plus a per-key policy of
  which sources may set a key at all (D-33). NixOS's numbers [26] and
  Nickel's `default`/`force` [12] confirm the model; nobody has the
  per-key source policy.
* **The artifact is partial, on purpose.** Build-time sources (files in
  the repository, the contract's defaults) are folded; run-time sources
  (environment, command line, secrets) stay open, and the compiler knows
  which keys each may still set, from the policy. This is Terraform's
  unknown value [20][23] made into a check instead of a hole.
* **Provenance travels.** The image's chain and the baked tables keep
  "from device.toml line 12, at build time". No language above carries
  its places into its output.

### 4.2 The options

**A. A configuration language of the library's own** (functions,
conditionals, imports with computation). Cost: very large, a new
dependency-free evaluator in C89 under MISRA. Rules out nothing, and
fights D-10's "references to keys, after precedence" and section 2.18.
CUE, Pkl, Dhall, Nickel and Jsonnet do it better already. **Reject.**

**B. A driver over what exists** (`configforge gen build`, sketched in
5.2): sources in rank order, the contract, a profile label, a target;
the passes of `cf_defs_apply`; every diagnostic; a manifest. Cost: one
tool file and its tests; no change to the core beyond 4.2.D. Needs: no
dependency. Rules out: nothing; the targets are those that exist.

**C. Targets.**

| Target | Exists | Cost | Needs |
| ------ | ------ | ---- | ----- |
| image (`.cfgb`) | C call | S: a command around `cf_image_write` | nothing |
| baked C tables | designed (D-104) | L: the generator, `cf_config_from_baked`, tests against layer 0 | a public function, already decided |
| a normalised document | `cf_emit` | S | nothing |
| a report | `cf_report` | S | nothing |
| code from the contract | `gen --lang` | none | nothing |
| a manifest with SHA-256 | no | S | 1.10.0's SHA-256 |

**D. Every diagnostic.** Today the builder stops at the first strict
refusal. A compiler needs the whole list. Two ways: (1) inside the tool
only, by building with every key as `CF_INVALID_FALLBACK` and walking
the key space for rejected candidates, which the key space already
keeps (`cf_keyspace.h:131`, `143-152`); cheap, no new interface, but
the walk misses errors that are not a candidate's (an unreadable file,
an unknown key under reject) and the tool would hold a second reading of
what is an error; (2) a builder option that keeps going and hands each
diagnostic to a callback, in the experimental tier (D-89). (2) is the
honest one: the program itself can then list every problem at start-up.
Needs: a new public interface. It is also where the 1.8.0 row's "one
diagnostic model" leads.

**E. Open keys and the build-time line.** The tool is told which ranks
will exist at run time (`--runtime env,cli`). For each key it computes,
from the policy, which of those may still set it. Then:

* a required key with no build-time value and no run-time source allowed
  to supply it is an error at build time: the compiler proves the
  contract can be met;
* a template whose referenced key is open is an error, naming both keys
  (section 3, item 6);
* a key whose build-time value no run-time source may change is
  "sealed", and the manifest says so.

Cost: S, inside the tool, from `cf_def.policy` and the rank names
(`cf_define.h:345-348`). Needs nothing public.

**F. Defaults and secrets out of the artifact by default.** Write only
what a source configured (as `CF_EMIT_CONFIGURED`), unless asked
(`--with-defaults`, for firmware built with its program); refuse a
`CF_SENSITIVE` value in the artifact unless asked (`--allow-secrets`),
as a full report is a credential-bearing file. For the image this needs
`cf_image_write` to take a choice of keys: a flag beside
`CF_IMAGE_PROVENANCE`. Cost: S. Needs: a new flag value (additive).

**G. Rules across keys.** Three ways: (1) none in the library, and the
report handed to OPA or CUE, as the Roadmap's recipes already plan
(docs/Roadmap.md:938-940); (2) a fixed, declarative set in the contract
- "if a is set, b is required", "a and b exclude each other", "a <= b",
"one of" - each total and linear, C89, the same at run time and at
build time; JSON Schema has `dependentRequired` for the first; (3) an
expression language: CEL [40] is the right shape but has no C
implementation; one of the library's own is option A again. (2) is
KCL's `check:` [18] and Configerator's validators [37] without a
language. It overlaps with the analysis-tool research of the same day
("values a rule forbids"), which should decide it with this.

**H. A manifest.** Beside the artifact, a small JSON: each input's
path, size and SHA-256; the contract's digest; the tool's version and
build id (D-40); the profile label (D-12); the target; the open keys;
the output's digest. Deterministic: no clock unless
`SOURCE_DATE_EPOCH` [42]. Inside the image later, as a section with
`CF_IMAGE_MINOR` 1 (minor versions only add, docs/ImageFormat.md),
with 2.0.0's image header work. Cost: S beside, M inside.

**I. A build file.** The sources, ranks and targets in a file rather
than on the command line, read by the library itself with a contract the
tool defines, so `configforge gen build --explain` explains its own
settings. Cost: M. Needs nothing new; it is a program using the library.
Not first: the command line covers the first users.

**J. Other languages as front ends.** CUE, Jsonnet, Dhall, Nickel, Pkl
and KCL all export JSON, YAML or TOML [2][7][13][17], which the library
reads. A Guide recipe per language (`pkl eval -f json` into
`configforge gen build`) makes this library their back end and run time
without a dependency. The place then says `generated.json:12`, not the
`.pkl` line; carrying a source map through is not worth it now. Cost:
S.

### 4.3 Against the layer rules

* **D-100.** The driver is a tool (`tools/gen/`), not a component;
  the passes stay in `lib/config/`. A diagnostics callback (D) is
  `lib/config/`'s and `lib/base/`'s `cf_error` shape.
* **D-102, D-103, D-108.** The front ends are the readers as they are;
  compressed or recoded inputs come through the composed codecs
  unchanged.
* **D-104.** The compiler is layer 1's C kit grown into a driver; the
  baked tables are one of its targets; the rule "compared with layer 0"
  holds for every target, and is cheap to test: build, load the artifact
  as a source, compare `cf_report` with the configuration built
  directly.
* **D-105.** No profile needs the compiler; `minimal` and `misra` gain
  from it (no reader in the device).
* **D-106.** The driver is C89 like the rest of `tools/gen/`.
* **D-107.** Diagnostics reuse `where` and `reason`; an end position and
  a severity are what the compiler's list adds.
* **C89 core.** Nothing in the core changes except D's callback and F's
  flag, both additive under D-51.

## 5. Recommendation

### 5.1 In order

1. **First, with 1.9.0's baked configuration: the driver and its
   targets.** `configforge gen build`: sources in rank order, the
   contract, `--profile`, `--runtime`, a target of `image`, `report` or a
   format name; the open-key checks (4.2.E); defaults and secrets left
   out unless asked (4.2.F); deterministic output; exit 0, 1 or 2 as
   `gen check`. The baked C tables of D-104 become its `c` target, so the
   two share one front half instead of `gen --bake` growing its own.
2. **Then, in 1.10.0: every diagnostic and the manifest.** The builder's
   keep-going callback (4.2.D), `--json` diagnostics shaped like
   `terraform validate -json` [22] with a key, a source, a place and a
   reason; the manifest with SHA-256 once "one digest a purpose" lands.
3. **Then: rules across keys,** decided together with the analysis
   tool: the fixed declarative set (4.2.G option 2), run by the library
   at run time and therefore by the compiler.
4. **Alongside, any time: the recipes** for CUE, Pkl, Jsonnet, Dhall,
   Nickel and KCL as front ends (4.2.J).
5. **Later, when asked: a build file** (4.2.I), and **the manifest
   inside the image** with 2.0.0's image header layout.

**Not to build:** a configuration language (4.2.A; [38]); text
templates (section 3, item 4); unification in place of ranks (it
removes the override every program needs from its command line);
planning against live state (Terraform, Pulumi: there is no remote
state here); an input-addressed store (Nix); a plug-in protocol for
targets beyond D-110's, which already hands the contract to a program on
the PATH.

**Why the order.** Item 1 turns pieces that exist into a command and
costs little beyond the baking 1.9.0 already holds; it is also the
moment to stop the image freezing defaults. Item 2 needs SHA-256 and a
public interface, which are 1.10.0's. Item 3 is a design choice that the
analysis research should share.

### 5.2 The interface, sketched

The command (spelling TO DECIDE, A):

```text
configforge gen build --schema app.schema.json
        --source defaults.toml=sys-file --source site.toml=user-file
        [--runtime env,cli] [--profile production]
        --target image|c|report|<format> [--out F]
        [--provenance] [--with-defaults] [--allow-secrets]
        [--manifest F] [--json]

exit 0  built, no error
exit 1  a diagnostic of severity error; nothing written
exit 2  unusable input (a schema or a file that cannot be read)
```

The diagnostics callback (TO DECIDE, C), experimental tier (D-89),
grown by `size` (D-51):

```c
typedef enum cf_severity { CF_SEV_ERROR = 0, CF_SEV_WARNING, CF_SEV_NOTE } cf_severity;

typedef struct cf_diag {
    size_t      size;     /* sizeof(cf_diag) as the library filled it */
    cf_severity severity;
    cf_code     code;
    int         reason;   /* CF_REASON_* (D-107) */
    const char* key;      /* NULL when none */
    const char* source;   /* the source's name, NULL when none */
    const char* at;       /* "file:line:col" as explain writes it, or NULL */
    const char* message;  /* a sensitive key's value never appears */
} cf_diag;

typedef void (*cf_diag_fn)(const cf_diag* d, void* user);

/* Keep going past the first refusal: every diagnostic is handed to fn
   during cf_builder_create, which still fails when any was an error. */
CF_API cf_bool cf_builder_diagnostics(cf_builder* b, cf_diag_fn fn, void* user);
```

The image flag (4.2.F), additive:

```c
enum {
    CF_IMAGE_PROVENANCE = 1u << 0,
    CF_IMAGE_CONFIGURED = 1u << 1  /* only what a source set: no defaults */
};
```

### 5.3 TO DECIDE

* **A. Where the compiler lives.** (1) `configforge gen build`, a
  subcommand beside `gen check` and `gen schema`, with D-104's `gen
  --bake` spelled as its `--target c`; (2) a fourth program,
  `configforge build`, which reopens D-99's three programs; (3) only `gen
  --bake` as D-104 wrote it, and the image left to C calls.
  **Recommended: (1).** `gen` is already "the contract, and files held to
  it", and `gen check` already builds a configuration from files.
* **B. How the build is described.** (1) the command line; (2) a build
  file read by the library with the tool's own contract; (3) both, the
  file later. **Recommended: (3).**
* **C. A public diagnostics interface.** (1) `cf_builder_diagnostics` in
  the experimental tier; (2) inside the tool only, by walking the key
  space (4.2.D (1)). **Recommended: (1)**, so a program and the compiler
  report the same list.
* **D. Rules across keys.** (1) none, recipes for OPA and CUE; (2) a
  fixed declarative set in the contract; (3) an expression language.
  **Recommended: (2)**, decided with the analysis-tool research.
* **E. The manifest's place.** (1) a JSON file beside the artifact; (2) a
  section of the image. **Recommended: (1) now, (2) with 2.0.0's image
  header.**
* **F. Defaults in a compiled image.** (1) left out unless asked; (2)
  written, as today. **Recommended: (1)** for the command; the C call
  keeps today's behaviour unless the new flag is given, so 1.x callers
  see no change.

## 6. The cards it implies

Sizes: S a day or less, M two or three, L a week. Versions from
docs/Maintaining.md "Versions" (lines 99-114).

| Card | Size | Version |
| ---- | ---- | ------- |
| `configforge gen build`: sources by rank, schema, profile, `image`, `report` and format targets, deterministic output, exit codes, tests that load the artifact and compare with layer 0 | M | 1.9.0 |
| The open-key checks: run-time ranks, required keys provably met, templates over open keys refused, sealed keys listed | S | 1.9.0 |
| `CF_IMAGE_CONFIGURED`, and the command leaving defaults and secrets out unless asked | S | 1.9.0 |
| The baked configuration (D-104 layer 1) as the `c` target, `cf_config_from_baked`, compared with layer 0 | L | 1.9.0 (already in its row) |
| `cf_builder_diagnostics` (experimental), and `gen build --json` and `gen check --all` with every diagnostic and its place | M | 1.10.0 |
| The manifest: inputs and their SHA-256, the contract's digest, version, profile, open keys, the output's digest, `SOURCE_DATE_EPOCH` | S | 1.10.0, after "one digest a purpose" |
| Rules across keys in the contract, run at run time and at build time | M | 1.10.0 or later, with the analysis tool |
| Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel, KCL as front ends | S | any |
| A build file read with the tool's own contract | M | when asked |
| The manifest inside the image (`CF_IMAGE_MINOR` 1) | S | 2.0.0, with the image header |

## 7. Sources

1. CUE, "The logic of CUE": https://cuelang.org/docs/concept/the-logic-of-cue/
2. CUE, `cue help export`: https://cuelang.org/docs/reference/command/cue-help-export/
3. CUE, "How CUE works with Go": https://cuelang.org/docs/concept/how-cue-works-with-go/
4. CUE, "Upgrading from evalv2 to evalv3": https://cuelang.org/docs/concept/faq/upgrading-from-evalv2-to-evalv3/
5. CUE v0.3.0-alpha1 release notes (exponential cases): https://github.com/cue-lang/cue/releases/tag/v0.3.0-alpha1
6. Jsonnet, design rationale: https://jsonnet.org/articles/design.html
7. Dhall, the language's README: https://github.com/dhall-lang/dhall-lang/blob/master/README.md
8. Dhall, the standard: https://github.com/dhall-lang/dhall-lang/blob/master/standard/README.md
9. Dhall, imports and integrity checks: https://github.com/dhall-lang/dhall-lang/blob/master/standard/imports.md
10. dhallix/nix-derivation issue 8, a slow, large normal form: https://github.com/dhallix/nix-derivation/issues/8
11. Nickel, contracts: https://nickel-lang.org/user-manual/contracts
12. Nickel, merging and priorities: https://nickel-lang.org/user-manual/merging
13. Pkl, language reference: https://pkl-lang.org/main/current/language-reference/index.html
14. Pkl, Go code generation: https://pkl-lang.org/go/current/codegen.html
15. Pkl, message passing API: https://pkl-lang.org/main/latest/bindings-specification/message-passing-api.html
16. pkl-go, the evaluator API: https://pkl-lang.org/go/0.8.0/evaluation.html
17. KCL, introduction: https://www.kcl-lang.io/docs/user_docs/getting-started/intro
18. KCL, schema and `check:` blocks: https://kcl-lang.io/docs/reference/lang/spec/schema
19. Starlark, the specification: https://github.com/bazelbuild/starlark/blob/master/spec.md
20. HCL, the specification: https://github.com/hashicorp/hcl/blob/main/spec.md
21. Terraform, `terraform plan`: https://developer.hashicorp.com/terraform/cli/commands/plan
22. Terraform, `terraform validate`: https://developer.hashicorp.com/terraform/cli/commands/validate
23. terraform-compliance issue 417, values known after apply cannot be tested: https://github.com/eerkunt/terraform-compliance/issues/417
24. Nix, derivations: https://nix.dev/manual/nix/2.24/language/derivations
25. NixOS manual, option definitions and priorities: https://nixos.org/manual/nixos/stable/#sec-option-definitions-setting-priorities
26. nixpkgs, `lib/modules.nix` (`mkOverride`, `filterOverrides`): https://github.com/NixOS/nixpkgs/blob/master/lib/modules.nix
27. ytt (Carvel), README: https://github.com/carvel-dev/ytt
28. kustomize, glossary: https://kubectl.docs.kubernetes.io/references/kustomize/glossary/
29. Helm, values files: https://helm.sh/docs/chart_template_guide/values_files/
30. Helm, charts, schema files: https://helm.sh/docs/topics/charts/#schema-files
31. Pulumi, how Pulumi works: https://www.pulumi.com/docs/iac/concepts/how-pulumi-works/
32. Protocol Buffers, text format specification: https://protobuf.dev/reference/protobuf/textformat-spec/
33. Protocol Buffers, `plugin.pb.h` (`CodeGeneratorRequest`): https://protobuf.dev/reference/cpp/api-docs/google.protobuf.compiler.plugin.pb/
34. Cap'n Proto, other languages and compiler plugins: https://capnproto.org/otherlang.html
35. Cap'n Proto, schema language (constants): https://capnproto.org/language.html
36. Ion Schema 2.0 specification: https://amazon-ion.github.io/ion-schema/docs/isl-2-0/spec
37. Tang, Kooburat, Venkatachalam et al., "Holistic Configuration Management at Facebook", SOSP 2015: https://research.facebook.com/publications/holistic-configuration-management-at-facebook/
38. Burns, Grant, Oppenheimer, Brewer, Wilkes, "Borg, Omega, and Kubernetes", ACM Queue 14(1), 2016: https://queue.acm.org/detail.cfm?id=2898444
39. Xu, Zhang, Huang et al., "Do Not Blame Users for Misconfigurations", SOSP 2013: https://www.eecg.utoronto.ca/~yuan/papers/sosp13-misconfig.pdf
40. CEL, language definition: https://github.com/google/cel-spec/blob/master/doc/langdef.md
41. Kubernetes, CEL in Kubernetes: https://kubernetes.io/docs/reference/using-api/cel/
42. Reproducible Builds, `SOURCE_DATE_EPOCH`: https://reproducible-builds.org/specs/source-date-epoch/
