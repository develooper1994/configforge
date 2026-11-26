# Research: an analysis tool for configuration files

Research for the Roadmap item "An analysis tool for configuration files"
(docs/Roadmap.md, "From the maintainer, 2026-11-26"). Not a card and not a
decision: what the world does, what breaks, what fits this library, and
what is left for the maintainer to decide. Written 2026-11-26.

## 1. The question

**As the maintainer asked it** (Turkish, translated): "An analysis tool
for configuration files could be built. I don't know, a static or dynamic
analysis tool comes to mind, but that was only an example. First similar
to what is in the world, then it differentiates. Research is needed."

**As the Roadmap holds it** (docs/Roadmap.md:1567-1575): Doctor already
lints a report, diffs two and finds drift; this asks for static analysis
of configuration files (unused keys, keys the contract lacks, values a
rule forbids, secrets in plain text, what one file overrides in another)
and dynamic analysis (what a running program read, misread or never
read). The same as the world first, then what only the contract adds.

**What the library does today.** More than the question assumes. Five
pieces already analyse configuration; none of them is a tool that takes
a set of files and reports every finding with its place.

* **Doctor reads reports, not files** (docs/Doctor.md, sections 2, 7, 8;
  `tools/doctor/`). A report is what a program wrote with `cf_report`
  (`lib/config/include/configforge/configforge.h:666`): every key, every
  candidate with its place (`app.ini:2:1`), the winner and why, the
  definition, refused candidates and their reason. Doctor applies twenty
  rules to it (`tools/doctor/engine.c:1019-1038`, docs/Doctor.md:417-445):
  CFD001 a key the same source sets twice, CFD005 an undefined key with
  the nearest name, CFD006 a contract violation, CFD007 a secret from the
  command line or a world-readable file, CFD009 a deprecated key set,
  CFD010 a source that wins nothing, CFD015 an array cut by its count,
  CFD016 a source the key's policy refuses, CFD018 set but never read,
  and the rest. `lint` writes text, Markdown or SARIF 2.1.0
  (`tools/doctor/doctor.c:105-155`), with a baseline and a severity file
  (docs/Doctor.md, section 9). `diff`, `history`, `blame` and `drift`
  compare reports. A non-goal, stated: Doctor never resolves configuration
  itself (docs/Doctor.md:853).
* **`configforge gen check`** (`tools/gen/contract.c:50-97`, docs/Tools.md,
  "configforge gen"): files, in order, against a JSON Schema contract,
  unknown keys refused. It is the only static check of files against the
  contract today, and it stops at the **first** refusal: it builds a
  configuration and prints `err.message` when `cf_builder_create` fails.
  No place in a machine form, no SARIF, no second finding.
* **`configforge codec check`** (docs/Tools.md, "configforge codec"):
  whether each file is well formed, with the reader the program uses.
  Syntax only, like the syntax half of yamllint.
* **Read coverage** (`cf_builder_track_reads`, `configforge.h:425-434`;
  `cf_was_read`, `cf_read_count`, `configforge.h:560-565`): the program
  records which keys it read; the report carries `"read"` per key;
  CFD018 reads it. This is dynamic analysis, and it exists.
* **Misreads, planned** (card C-8a, docs/plan/C-change-2.md:103-195;
  C-8b, :197-226; version 1.10.0, "misreads noticed"): a typed read of a
  key the contract does not define, or as another type, recorded without
  a lock in sixteen slots, listed under `"misreads"` in the report, and
  Doctor's CFD021 on it. Not landed.

Around them: the contract is written as a JSON Schema (`cf_builder_schema_file`,
D-64) and written back as one (`cf_schema`, `configforge.h:671`), closed
when unknown keys are refused. The contract reader takes a fixed subset of
keywords (`lib/config/src/bridge/schema.c`): `type`, `minimum`, `maximum`,
the exclusive pair, `minLength`, `maxLength`, `enum`, `default`,
`deprecated`, `required`, `additionalProperties`, `items`, `format`,
`readOnly`, `writeOnly`, descriptions. No `pattern`, no `if`/`then`, no
`dependentRequired`: nothing yet relates one key to another.

So the honest restatement: the library can already *analyse*; what it
lacks is (a) a static mode that takes files without the program and
reports every finding with its place, (b) rules about values beyond a
key's own type and range, and (c) the dynamic half finished (misreads,
a report pulled from a running program).

## 2. What the world does

Four families, then the research and the output standard. For each tool:
static or dynamic, what it finds, how rules are written, its output, how
CI runs it.

### 2.1 Policy over parsed data

**conftest and OPA (Rego)** [1][2][3]. Static. conftest parses a file into
plain data and evaluates Rego rules over it: rules named `deny`, `warn`
or `violation` (optionally `deny_<name>`) in the `main` namespace, each
yielding a message. It parses many formats: JSON, YAML, TOML, INI, XML,
HCL, HOCON, `.properties`, `.env`, Dockerfile, CUE, Jsonnet, nginx, VCL,
EDN and others [1]. `--combine` hands all files to one rule as an array of
`{path, contents}`, so a rule can compare files [2]. Output: text, JSON,
TAP, table, JUnit, GitHub annotations, Azure DevOps and SARIF [2]. Exit
codes: 0 clean, 1 a failure; with `--fail-on-warn`, 1 a warning and 2 a
failure [2]. Rules are tested with `conftest verify` and Rego unit tests.
A location is only what the rule writes into a `_loc` field, and only the
JSON and GitHub outputs use it [1]: the parsers produce data without
places, so a finding points at a file, not a line, unless the rule author
works for it. Rego is Datalog-like over JSON `input` and `data`; an
expression that errors makes the rule *undefined*, not false, unless
`--strict-builtin-errors` is given [3].

**CUE (`cue vet`)** [4][5]. Static. Data files (JSON, YAML, TOML, text)
are unified with a CUE schema; `-d` picks the definition to check against,
`-c` demands concrete values. CUE definitions are closed by default: a
field the definition does not name is an error, and `...` opens it [5].
CUE imports JSON Schema (`cue import`) and can vet against it [5]. Rules
are the schema itself: types, bounds, regular expressions, and relations
between fields, in one language. Output is error text; no SARIF.

### 2.2 Schema validators

**kubeconform** (and **kubeval**, its predecessor) [6][7]. Static.
Kubernetes manifests against JSON Schemas made from the Kubernetes
OpenAPI specification, fetched from a registry, cached, in parallel.
`-strict` refuses properties the schema does not have and duplicate
keys; `-ignore-missing-schemas` passes a document whose schema it cannot
find [6]. Output: text, JSON, JUnit, TAP, pretty. It says plainly that it
does not do what the server does, and points to `kubectl --dry-run=server`
for that [6]. kubeval's README: "This project is no longer maintained",
use kubeconform [7].

**JSON Schema validators**: ajv, check-jsonschema and the rest of the
catalogue at json-schema.org [8][9]. Static. One document against one
schema. check-jsonschema is a CLI and a set of pre-commit hooks with
schemas built in (GitHub workflows, Renovate, Azure Pipelines) and remote
schemas cached [9]. Bowtie compares implementations against the
specification, because they disagree [8]. Unknown keys pass unless the
schema says `additionalProperties: false`.

### 2.3 Linters and security scanners for one kind of file

**yamllint** [10]. Static, syntax and style. Rules by name: `key-duplicates`,
`truthy`, `octal-values`, `float-values`, `anchors`, `empty-values` are
about meaning; `indentation`, `line-length`, `trailing-spaces` and the
rest are style. `truthy` exists because `yes`, `on` and `off` are booleans
in YAML 1.1 and strings in 1.2, so two readers of one file disagree [10].
Knows nothing of what keys mean.

**Checkov** [11]. Static, security. Frameworks: Terraform and its plan,
CloudFormation, Kubernetes, Helm, Kustomize, Dockerfile, ARM, Bicep,
serverless, GitHub Actions, GitLab CI, OpenAPI, plain JSON and YAML,
secrets. Checks have stable ids (`CKV_AWS_123`), are written in Python or
YAML, and are loaded with `--external-checks-dir`. Output: CLI, JSON,
JUnit XML, SARIF, GitLab SAST, CSV, CycloneDX, SPDX. `--soft-fail` always
exits 0; `--baseline` (made by `--create-baseline`) reports only new
failures; `--skip-check` takes ids or severities [11].

**tfsec, now Trivy** [12][13]. Static, security. tfsec checked Terraform,
wrote SARIF, JSON, CSV, Checkstyle and JUnit, and took Rego policies; its
README now asks its users to move to Trivy [12]. `trivy config` checks
Dockerfile, Kubernetes, Terraform, CloudFormation, ARM and Helm, the kind
told from each file, with checks written in Rego and downloaded as a
bundle, findings rated UNKNOWN to CRITICAL and filtered by `--severity`
[13].

**KICS** [14]. Static, security, from Checkmarx. Terraform, Kubernetes,
Docker, CloudFormation, Ansible, OpenAPI, Helm, Pulumi, Compose and more.
Reports: JSON, SARIF, GitLab SAST, JUnit, SonarQube, HTML, PDF, CycloneDX,
ASFF, CSV, Code Climate [14].

**Polaris** [15]. Static in CI, dynamic in a cluster. One rule set run
three ways: a dashboard over what is running, an admission controller
that refuses or changes workloads, and a CLI over local YAML. Thirty-odd
built-in checks; custom checks written in JSON Schema [15].

**kube-linter** [16]. Static. Kubernetes YAML, Helm charts and Kustomize
against best-practice checks (`latest-tag`, `no-read-only-root-fs`,
`unset-memory-requirements`); checks switched on and off in a config
file, custom ones from templates; SARIF and JSON written in one run;
non-zero exit on a finding [16].

**Datree** [17]. Static, Kubernetes policy, with a hosted service. The
company closed in 2023 and its repositories are archived, security fixes
included [17]. docs/Market.md:89 already records it.

**hadolint** [18]. Static. A Dockerfile parsed to a syntax tree, rules
`DLnnnn` on the tree and ShellCheck's `SCnnnn` on each `RUN`. Output:
tty, JSON, Checkstyle, Code Climate, GitLab, GNU, SonarQube, SARIF,
JUnit. `# hadolint ignore=DL3003` above a line, or a global ignore;
`--failure-threshold` sets which severity fails; labels checked against a
small schema of formats (url, semver, rfc3339, email) [18].

**ansible-lint** [19]. Static. Playbooks and roles, with `schema[...]`
rules that validate files against JSON Schemas, and profiles from `min`
to `production` that grow stricter. `--fix` rewrites. Output includes
SARIF (`--sarif-file` beside the normal output) and Code Climate.
`--generate-ignore` writes `.ansible-lint-ignore`, every current finding
as a file and rule pair, so old findings are tolerated and new ones
fail; `# noqa: rule` per line [19].

### 2.4 The program checks its own configuration

**systemd-analyze** [20]. `verify` loads unit files and the units they
name and warns of unknown sections and directives, missing dependencies,
man pages in `Documentation=` that are not installed, and `ExecStart=`
commands that are missing or not executable; `--recursive-errors` sets
whose warnings make the exit status non-zero. It checks against the host
it runs on. `cat-config` prints a configuration file with its drop-ins in
precedence order; `security` scores a service's sandboxing from 0 to 10
and can run offline on unit files [20]. Doctor's systemd adapter already
reads `cat-config` (docs/Doctor.md, section 12).

**nginx -t** [21]. The program's own parser: "nginx checks the
configuration for correct syntax, and then tries to open files referred
in the configuration". `-T` also prints the configuration files [21].
Static in that nothing is served; dynamic in that it touches the machine.

**sshd -t and -T** [22]. `-t` checks the configuration file and the host
keys. `-T` prints the effective configuration after checking it, and with
`-C user=...,addr=...,host=...` applies the `Match` blocks that a given
connection would meet [22]. This is the only tool in this list that
answers "what value would *this* case get", which is `explain()` for one
key under one context.

**Augeas** [23][24]. Not a checker but a parser for many system files at
once: a lens per format, written in a small ML-like language, maps a file
to a tree and back, and edits change only what they touch. A file a lens
cannot read is an error in the tree, not a crash. It models syntax and
very little meaning [24].

**Spring Boot configuration metadata and its IDE checks** [25][26]. The
closest to this library. An annotation processor reads
`@ConfigurationProperties` classes at compile time and writes
`META-INF/spring-configuration-metadata.json`: each property's name,
type, description, default and deprecation (level, reason, replacement,
since); hints list valid values [25]. A deprecation at level `error`
means the property "is no longer managed and is not bound": setting it
has no effect [25]. IDEs (IntelliJ IDEA, and Spring Tools for VS Code,
which keeps an index of the metadata) use it for completion, hover and
validation of `application.properties` and `application.yml` [26]: an
unknown key, a value of the wrong type, a deprecated key with its
replacement. It is a contract shipped with the program and read by
tools that never run it.

**Language servers: yaml-language-server and Taplo** [27][28]. Static,
in the editor. yaml-language-server validates YAML against JSON Schema
(drafts 4, 7, 2019-09, 2020-12), completes keys and defaults, shows a
description on hover; a schema is chosen by a modeline
`# yaml-language-server: $schema=...`, a `$schema` key, a glob in the
settings, or the JSON Schema Store, in that order [27]. Taplo does the
same for TOML 1.0, as a formatter, a validator against JSON Schema, a
language server and a CLI [28].

### 2.5 Research on configuration errors

* **SPEX** (Xu et al., SOSP 2013) [29] infers each parameter's
  constraints from the program's source (type, range, relations between
  parameters) and then feeds the program values that break them: 3,800
  constraints for 2,500 parameters, 743 bad reactions (crash, hang,
  silent wrong behaviour) across seven systems. The point: the
  constraints exist in the code whether or not anyone wrote them down.
* **PCheck** (Xu et al., OSDI 2016) [30] finds that many systems do not
  check a parameter until they first use it, sometimes only on a failure
  path, so an error lies latent until it does damage; PCheck generates
  checks that run at start-up and caught about three quarters of such
  errors.
* **ctest** (Sun et al., OSDI 2020) [31] runs the program's own tests
  with the production configuration values substituted, so a change is
  tested in the code it reaches rather than against a schema.
* **"Too many knobs"** (Xu et al., FSE 2015) [32]: most parameters are
  set by almost nobody; a small share is set by most users. The
  configuration actually written is a thin slice of what the contract
  allows.

### 2.6 The output standard

**SARIF 2.1.0** (OASIS) [33]: a JSON log of runs; each run names its
tool and rules; each result has `ruleId`, `level` (`none`, `note`,
`warning`, `error`), a message, locations (`artifactLocation.uri` with a
`region` of line and column), `partialFingerprints` that let a result be
recognised after the file moves, `suppressions` and `baselineState`.
GitHub's code scanning reads it [34]; Checkov, tfsec, KICS, kube-linter,
hadolint, ansible-lint and conftest all write it. Doctor writes it too,
with a line and no column and no fingerprint (`tools/doctor/doctor.c:105-155`).

## 3. What breaks

1. **Two parsers, two answers.** A linter that reads the file with a
   different parser from the program's checks a document the program
   never sees. yamllint's `truthy` rule exists because YAML 1.1 and 1.2
   read `on` differently [10]; Bowtie exists because JSON Schema
   validators disagree [8]; conftest reads INI, HOCON and `.properties`
   with its own parsers [1]. The defect is invisible from either side.
2. **No place, or a place by hand.** Parsers that return plain data lose
   the line. conftest reports a line only when the rule writes `_loc`,
   and only two of its outputs carry it [1]. A finding without a line is
   a finding a reviewer must search for.
3. **Unknown keys pass by default.** JSON Schema allows extra properties
   unless told not to; kubeconform needs `-strict` [6]; CUE is the
   exception, closed by default [5]. A misspelt key is the commonest
   error and the default lets it through.
4. **A missing schema reads as a pass.** `-ignore-missing-schemas` [6]
   makes a document nobody checked look checked. Custom resources are
   the usual case.
5. **Structure is not behaviour.** kubeconform says it does not do what
   the server does [6]; SPEX [29] and PCheck [30] show that the
   constraints a program enforces are in its code, often unwritten, and
   that a value can be well-typed and still crash it. ctest [31] answers
   by running the code.
6. **Checks on the wrong machine.** `nginx -t` opens the files the
   configuration names, and `systemd-analyze verify` looks for the
   executables, on the machine it runs on [20][21]. In CI that is not the
   machine that will run the service. Doctor met the same problem with
   CFD007 and solved it by recording `world_readable` where the file is
   read (docs/Doctor.md:481-487); a check of files in a CI checkout would
   record the checkout's permissions, which say nothing about production.
7. **One environment proves nothing about another.** A key shadowed by
   an environment variable here may win where the variable is unset
   (docs/Doctor.md:449-456). A static check of a set of files can only
   speak for that set.
8. **A rule that errors passes.** In Rego an expression that fails is
   undefined, and an undefined `deny` denies nothing [3]. A policy with a
   typo in a field name is silently satisfied.
9. **Noise kills adoption.** Every mature tool grew a baseline: Checkov's
   `--baseline` [11], ansible-lint's ignore file [19], hadolint's
   pragmas and threshold [18], Doctor's baseline (section 9). A first run
   with five hundred findings is a tool switched off.
10. **Tools die; formats survive.** kubeval is unmaintained [7], Datree
    closed [17], tfsec folded into Trivy [12]. Rules written in a tool's
    own language die with it. SARIF outlives each tool [33].
11. **Secrets in the output.** A finding that quotes a value uploads a
    password to the code-scanning page. Checkov has a secrets framework
    that masks; Doctor is value-blind by default and `--redact-all`
    hides every value (docs/Doctor.md:236-240). A new tool must keep that.
12. **Latent errors.** A parameter read only on a failure path is never
    read in a test run [30]. Read coverage (CFD018) from one run reports
    it as "never read" when it is merely "not read yet": the report says
    so, and the rule is `info` for that reason (docs/Doctor.md:441).
13. **Dead keys that look alive.** Spring's deprecation level `error`
    marks a property that is no longer bound [25]: setting it does
    nothing, and only metadata knows. Without a contract a tool cannot
    tell "unused" from "used by something it does not see".

## 4. What fits this library

What this library has that none of the tools above has together: one
reader for each format that is *the program's own reader*; a place for
every value; precedence across files, environment and command line; a
typed contract with sensitivity, deprecation, policy per key and open
prefixes; a report that carries all of it; and Doctor's rules and SARIF
already written. The options below are measured against that and
against D-100 to D-108.

### Option A: recipes only

Write the schema for editors (`cf_schema` already does), document the
yaml-language-server modeline and Taplo's schema directive, and a
conftest recipe over a report (already on the Roadmap, docs/Roadmap.md:938-941).

* Cost: a Guide page. Needs nothing.
* Gives: completion and hover in editors for YAML, TOML and JSON; Rego
  rules about values per profile.
* Rules out nothing. Leaves the five static questions mostly
  unanswered for INI, XML and `.properties`, and every finding without
  precedence.

### Option B: a predicted report from files, and Doctor on it

`configforge gen check` already builds a configuration from a contract
and files. Let it keep going instead of stopping: build with unknown keys
kept (`CF_UNKNOWN_WARN`, `lib/config/include/configforge/cf_define.h:318-322`)
so they are candidates and not errors, and write the value-blind report
(`cf_report`) to a file. `configforge doctor lint` then runs on it
unchanged. A refused value is already a refused candidate in a report,
with its reason and place (CFD006, CFD016), so collecting "every refusal"
needs no new error list: the report is the list. (A card confirms that an
unknown key under `CF_UNKNOWN_WARN` is in the report as a candidate, so
CFD005 fires on it.)

* Cost: small. An option on `gen check` (a path to write the report to)
  and a `cf_report_info` saying the report was predicted from files, not
  observed. Doctor needs one change: a field in the report that says
  "predicted from files" switches off what only a running program can
  say (CFD007's world-readable half, CFD018, CFD008's profile).
* Needs: no new public C interface, no dependency. D-100: tools stay in
  `tools/`, no new component. D-107: the refusal's place and reason
  become SARIF regions with no new code. D-108: files are read as the
  program reads them, by name. D-105: tools are not built under
  `minimal` or WASI, as today.
* Gives, for every format the build reads, with lines: an unknown key
  with the nearest name (CFD005), a value out of type or range or not in
  its enum (CFD006), a deprecated key (CFD009), a key set twice in one
  file (CFD001, which is yamllint's `key-duplicates` in every format), a
  file that wins nothing (CFD010), an array cut short (CFD015), a secret
  from a source its policy refuses (CFD016), SARIF, a baseline and a
  severity file.
* Keeps Doctor's non-goal: Doctor still never resolves configuration;
  `gen`, the contract's program, does.
* Rules out: findings about the program's own codecs and repairs (D-103):
  a composed codec lives in the program and `gen` cannot load it. A file
  in such a format is "cannot read", exit 2, never a pass.

### Option C: a new program, `configforge check` or `configforge lint`

The same as B under a new name, files in and SARIF out in one step.

* Cost: a program, its tests, its page, and a third place (beside
  `codec check` and `gen check`) where "check" means something.
* Gives: one command instead of two. B gets the same with a two-line CI
  step, or with `gen check --format sarif` calling Doctor's printer, if
  that printer moves into the Doctor library (`cf_doctor.h`).
* Rules out nothing, but splits the contract's tooling in two.

### Option D: rules about values

What the contract cannot say today is a relation: "if `tls.enabled` then
`tls.cert` is required", "`pool.max` at least `pool.min`", "`url`
matches this pattern".

* **D1, Rego or CUE over the report** (recipe). No dependency in the
  library; the report is JSON; conftest and cue are where people already
  write policy (docs/Market.md:229-233 says to call them, not compete).
  Loses the place unless the recipe copies the candidate's `at` into
  `_loc`, which it can, since the report carries it. Carries failure
  mode 8 (a rule that errors passes).
* **D2, more JSON Schema in the contract**: `pattern`, `if`/`then`/`else`,
  `dependentRequired`, perhaps `minItems`. The contract is already JSON
  Schema, so editors and every validator in section 2.2 understand the
  same rules for free. Cost: a regular-expression engine for `pattern`
  (C89, no dependency, bounded: a subset with no catastrophic
  backtracking) and a relation check at `cf_builder_create`; a public
  interface for the same in C (`cf_builder_requires(b, "tls.enabled",
  "tls.cert")` or the like). Changes the contract, so it is the
  maintainer's.
* **D3, a rule language of our own**. Rejected: failure mode 10, and
  docs/Market.md's "do not compete where the field is crowded".

### Option E: the dynamic half

* **E1, misreads** (C-8a, C-8b): planned and decided; nothing to add but
  to land them.
* **E2, defined but never read**: a contract key no run read. CFD018 is
  "set but never read"; the other half, a definition the program has
  stopped using, is the contract's dead code. One run cannot prove it
  (failure mode 12); a set of reports can, the way `drift` reads a set
  (docs/Doctor.md, section 7e): a key unread in every report given.
  Cost: one rule over a set, in Doctor; no library change.
* **E3, a report asked of a running program** (docs/Roadmap.md:932-937,
  version 2.3.0): the observed report, the dynamic counterpart of B's
  predicted one. `doctor diff predicted.json observed.json` is then the
  check that the files in the repository are what production runs.
* **E4, ctest-style runs** [31]: run the program's tests under the
  candidate configuration. That is the program's test suite's job, not
  a library's; the library's part is `cf_builder_replay` (`configforge.h:689`),
  which already loads a report as a source.
* **E5, SPEX-style inference from source** [29]: rejected. The contract
  is what SPEX has to infer, written down; and a C++ or Rust program with
  typed keys already gets an undefined key as a compile error (D-104,
  tiers 11 and 20).

### Option F: the editor

A language server driven by the contract is on the Roadmap
(docs/Roadmap.md:946-948, version 2.3.0). Before it, Option A gives most
of it for YAML, TOML and JSON through servers that exist [27][28]. What
only this library's server would add: INI, XML and `.properties`, and
from a report what overrides a key in production. Large; later.

### What only the contract adds

Set against section 2, these are the findings no tool there can make,
because none has a typed contract and precedence together:

* **A key the contract lacks, in any format, with the nearest name.**
  JSON Schema validators say "additional property"; CFD005 says "did you
  mean `server.port`".
* **A secret by declaration, not by guess.** gitleaks and Checkov's
  secrets framework guess by pattern and entropy. The contract says
  `password` is `CF_SENSITIVE`, so a literal value for it in a checked-in
  file is a fact, and an undefined key named like a secret is kept as
  one (D-84).
* **What one file overrides in another, across formats and layers.** A
  report of a set of files says which file wins each key and which lose,
  INI against TOML against environment, which no single-format linter
  sees. For the files given, "always loses" is provable, unlike the
  single-report case (failure mode 7).
* **Policy per key**: a secret that came from a source its policy refuses
  (CFD016). No tool above knows which source a value is allowed to come
  from.
* **Deprecated with no effect.** Spring's `error`-level deprecation [25]
  is the only parallel; here a deprecated key is CFD009 in every format.
* **Asked against defined** (misreads): the program read `server.prot`,
  the contract has `server.port`. A file linter cannot see the program;
  a program cannot see its own typo without a contract.
* **Default drift** (CFD017): an upgrade's changed default reaches every
  deployment that never set the key, and no file shows it.
* **The same reader as the program**, so failure mode 1 does not exist.

## 5. Recommendation

**First: Option B.** A predicted report from files, and Doctor on it. It
answers four of the five static questions in the Roadmap item (keys the
contract lacks, values a rule forbids as far as the contract says, what
one file overrides in another, secrets from a refused source) for every
format the library reads, with places and SARIF, and it reuses the two
most expensive things the project already built: the report and
Doctor's rules. Sketch:

```text
configforge gen check --schema app.schema.json [--report out.json]
                      [--unknown warn|reject] <file>...
    every file read as the program would, in order; with --report, the
    value-blind report written, marked "predicted": true, and exit 0
    unless a file cannot be read (2); without it, as today

configforge doctor lint out.json --format sarif --baseline .cf-baseline
    unchanged; a predicted report turns off the rules that need a
    running program, and says which
```

Two small changes in Doctor come with it: SARIF regions gain
`startColumn` (the place already has it: `app.ini:2:1`), and each result
gains a `partialFingerprints` entry (rule, key, file) so GitHub keeps a
finding's identity when lines move [33][34].

**Then, in this order:**

1. **E1**, misreads (C-8a, C-8b), already decided, as planned.
2. **A**, the recipes: the schema for yaml-language-server and Taplo, and
   conftest over a report with `_loc` copied from the candidate's place.
3. **A secret written in a checked file** (TO DECIDE 3).
4. **E2**, defined but never read, over a set of reports.
5. **E3**, a report from a running program, as the Roadmap has it.
6. **D2**, relations in the contract, if TO DECIDE 2 says so.
7. **F**, the language server, last.

**Not to build:** a rule language of our own (D3); Rego or CUE linked
into the library or Doctor (a Go runtime, and failure mode 10); a
Kubernetes linter (crowded, docs/Market.md); inference from program source
(E5); a checker that touches the machine it runs on (failure mode 6);
ctest-style runs inside the library (E4).

### TO DECIDE

1. **Where the static mode lives.**
   * (a) `configforge gen check --report`, then `doctor lint`
     (recommended: no new program; Doctor keeps its non-goal; `gen` is
     already the contract's program).
   * (b) a new `configforge check` (one step; a third meaning of
     "check").
   * (c) Doctor reads files itself (rejected here: it breaks
     docs/Doctor.md:853, "Resolving configuration itself").
2. **Relations between keys.**
   * (a) none in the library; Rego or CUE over the report, as a recipe
     (recommended first: no dependency, no new interface).
   * (b) `pattern`, `if`/`then`, `dependentRequired` in the contract, and
     a C call for the same: a new public interface and a bounded regular
     expression engine in C89 (recommended only when a user brings a
     relation the recipe cannot hold).
3. **A secret written in a file under check.** docs/Doctor.md:430 puts
   "whether a file is committed to version control" out of CFD007's
   scope. In the files mode the files *are* the repository.
   * (a) a new rule (number after C-8b's CFD021): a `CF_SENSITIVE` key,
     or one named like a secret (D-84), has a value in a file given to
     `gen check`; error; the value never printed (recommended: it is the
     one secret finding that is a fact, not a guess).
   * (b) leave it to gitleaks.
4. **Defined but never read** (E2): a rule over a set of reports, `info`
   (recommended), or not at all.

## 6. The cards it implies

| Card | Size | Version |
| ---- | ---- | ------- |
| `gen check --report`: a predicted, value-blind report from files and a contract; `"predicted": true` in the report | medium | 1.10.0 |
| Doctor on a predicted report: rules that need a running program off and named; SARIF `startColumn` and `partialFingerprints` | small | 1.10.0 |
| C-8a and C-8b, misreads (planned) | medium, small | 1.10.0 |
| Recipes: the contract's schema in yaml-language-server and Taplo; conftest over a report with places | small (docs) | 1.10.0 |
| A secret written in a checked file (if TO DECIDE 3 is a) | small | 1.10.0 |
| Defined but never read, over a set of reports (if TO DECIDE 4) | small | 1.10.0 |
| A report asked of a running program (Roadmap) | medium | 2.3.0 |
| Relations in the contract (if TO DECIDE 2 is b) | large | after 2.3.0, no row yet |
| The language server (Roadmap) | large | 2.3.0 |

Sources less certain than the rest: Datree's 2023 closure is known from
forks and mirrors, not an announcement; the IDE checks named for Spring
(unknown key, wrong type, deprecated with replacement) go beyond what the
STS4 README states ("validation, code completions and information
hovers").

## 7. Sources

1. conftest, documentation: https://www.conftest.dev/
2. conftest, options (output formats, `--combine`, `--fail-on-warn`): https://www.conftest.dev/options/
3. Open Policy Agent, the Rego policy language: https://www.openpolicyagent.org/docs/policy-language
4. CUE, `cue help vet`: https://cuelang.org/docs/reference/command/cue-help-vet/
5. CUE, how CUE works with JSON Schema: https://cuelang.org/docs/concept/how-cue-works-with-json-schema/
6. kubeconform: https://github.com/yannh/kubeconform
7. kubeval (unmaintained): https://github.com/instrumenta/kubeval
8. JSON Schema, tools and Bowtie: https://json-schema.org/tools
9. check-jsonschema: https://github.com/python-jsonschema/check-jsonschema
10. yamllint, rules: https://yamllint.readthedocs.io/en/stable/rules.html
11. Checkov, CLI reference: https://www.checkov.io/2.Basics/CLI%20Command%20Reference.html
12. tfsec (moving to Trivy): https://github.com/aquasecurity/tfsec
13. Trivy, misconfiguration scanning: https://trivy.dev/latest/docs/scanner/misconfiguration/
14. KICS, results and report formats: https://docs.kics.io/latest/results/
15. Polaris: https://polaris.docs.fairwinds.com/
16. kube-linter: https://github.com/stackrox/kube-linter
17. Datree organisation (archived repositories): https://github.com/datreeio
18. hadolint: https://github.com/hadolint/hadolint
19. ansible-lint, usage: https://docs.ansible.com/projects/lint/usage/
20. systemd-analyze(1): https://www.freedesktop.org/software/systemd/man/latest/systemd-analyze.html
21. nginx, command-line parameters: https://nginx.org/en/docs/switches.html
22. sshd(8): https://man.openbsd.org/sshd
23. Augeas, documentation: https://augeas.net/docs/
24. Augeas, source and README: https://github.com/hercules-team/augeas
25. Spring Boot, configuration metadata format: https://docs.spring.io/spring-boot/specification/configuration-metadata/format.html
26. Spring Tools for VS Code, Spring Boot support: https://github.com/spring-projects/sts4/blob/main/vscode-extensions/vscode-spring-boot/README.md
27. yaml-language-server: https://github.com/redhat-developer/yaml-language-server
28. Taplo: https://taplo.tamasfe.dev/
29. Xu et al., "Do Not Blame Users for Misconfigurations", SOSP 2013: https://www.eecg.utoronto.ca/~yuan/papers/sosp13-misconfig.pdf
30. Xu et al., "Early Detection of Configuration Errors to Reduce Failure Damage", OSDI 2016: https://www.usenix.org/conference/osdi16/technical-sessions/presentation/xu
31. Sun et al., "Testing Configuration Changes in Context to Prevent Production Failures", OSDI 2020: https://www.usenix.org/conference/osdi20/presentation/sun
32. Xu et al., "Hey, You Have Given Me Too Many Knobs!", ESEC/FSE 2015: https://cs.unibg.it/esecfse_proceedings/fse15/p307-xu.pdf
33. OASIS, SARIF 2.1.0: https://docs.oasis-open.org/sarif/sarif/v2.1.0/sarif-v2.1.0.html
34. GitHub, SARIF support for code scanning: https://docs.github.com/en/code-security/code-scanning/integrating-with-code-scanning/sarif-support-for-code-scanning
