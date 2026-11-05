# ConfigForge Doctor

**ConfigForge Doctor** answers "why is this value what it is?" before deployment
rather than after an incident.

It is a command-line tool, not a library feature. It consumes **provenance reports**
produced by ConfigForge (or by an adapter over a foreign ecosystem), compares them,
and applies rules that a single running process cannot apply to itself.

The library answers the question for one process, now. Doctor answers it for two
environments, before the deployment happens.

---

## 1. Purpose

Three questions motivate the tool. None of them can be answered by looking at a
configuration file, and none of them are answered by existing tooling.

**Which values are set but unreachable?** A key is defined in `app.toml`, and a
higher-precedence source always supplies it. Editing the file does nothing. This is
the most common real configuration defect and nothing reports it today.

**Which values agree by coincidence?** Production and staging both resolve
`database.pool_size` to 32. Production reads it from the environment; staging reads it
from a file baked into the image. A value diff shows no difference. The next time the
environment variable is missing, the two environments behave differently.

**Which overrides exist in one environment and not the other?** A key is overridden by
a command-line flag in staging and not in production. The code path exercised during
testing is not the code path that runs in production.

---

## 2. What It Is Not

* Not a resolver. It never computes configuration; it reads reports.
* Not a secret store, and it does not need to see secret values.
* Not a manifest validator. Manifests are `conftest`, `kubeconform` and `helm --dry-run`.
* Not a runtime daemon. It runs in CI and on a developer's machine.
* Not a replacement for `explain()`. It is `explain()` applied across environments.

---

## 3. The Gap It Fills

Existing tools work in one of two layers and neither reaches the value a process
actually sees.

| Tool class | Examples | What it validates | What it misses |
| ---------- | -------- | ----------------- | -------------- |
| Manifest | `helm --dry-run`, `kubectl diff`, `conftest`, `kubeconform` | manifest structure and policy | the value after precedence resolution |
| Secret store | Vault, Doppler, Infisical | drift inside that store | every other source in the chain |
| Single-process introspection | Spring `/actuator/env`, .NET `GetDebugView()`, `git config --show-origin` | one process, one moment | no diff, no rules, no CI integration |
| File linting | `dotenv-linter` | one file's syntax | precedence, contract, cross-environment |

Doctor occupies the empty cell: **resolved value, with origin, compared across
environments, with rules.**

The closest conceptual precedents are `terraform plan` (show the effect before
applying) and `systemd-analyze cat-config` (show the merged result with origins).
Neither applies to application configuration.

---

## 4. The Provenance Report

The report is the only interface between Doctor and anything else. Its schema is
specified in `Decisions.md` (D-16) and is a Phase 1 deliverable of the library.

```text
schema version
profile label                     e.g. "production"
producing application and version
library version
timestamp
policy summary                    effective precedence, per-parameter overrides
per key (the key space is flat; see the value model)
    definition    type, required, default, sensitive, deprecated, array mode
    candidates    per source: present, value or redacted form, rejection reason
    selected      source and reason
    warnings      unknown keys, coercions, deprecated usage
```

The report is also readable *by the library*, not only by Doctor. See Report Replay in the
specification: a full report loaded as a source reproduces a production configuration
locally. That is why the schema belongs to the library rather than to this tool, and why a
value-blind report must be structurally distinguishable from a full one.

Declared scalar lists retain the flat `<key>.<index>` and `<key>.count`
entries. Their definition metadata adds a `list` object with `key`,
`merge` (`replace` or `append`), and `separator` (one character, or
`null` for no splitting). A list root's default is an array; an element's
default follows its actual default candidate, not its final appended
index. Sensitive defaults are absent from blind reports.

Candidates moved by append merging carry `written_key`, their original
source-local key. Aggregate counts have `synthesized: true`: they are not
another assignment or a duplicate exposure by that source. A running
restart-only list held through reload has `pinned: true` on its held
candidates. These are additive schema-1 fields; readers that do not use
them can ignore them. Full Report Replay loads the resolved list as an
append base, so lower defaults are not appended again.

A candidate that is its definition's declared default carries
`declared_default: true` (an aggregate count never does). Ranks cannot say
it: a caller's own source may share the defaults' rank, and a derived
configuration stands in for the defaults it was built from. A report
without the mark is older; Doctor then takes a candidate from a source of
the kind `default`, or one named `default`, as the default.

`fingerprint` ("fnv1a:" and sixteen hex digits) is the configuration's
`cf_fingerprint`: what a reload's log events and a trace carry, so a
report can be matched to them with one field. A secret enters it only as
the digest a blind report shows, so it is the same in a full and a blind
report of one configuration. Doctor shows it in `dump` and in Markdown,
where a diff says whether it changed. An additive schema-1 field; a
report without it is older.

The report is produced in two ways:

* **Predicted.** Doctor resolves offline from files and a declared environment. Used
  for the change under review.
* **Observed.** The running process emits its own report at startup. Used as ground
  truth for an environment the CI job cannot enter.

---

## 5. Value-Blind Mode

This is the decision that determines whether the tool is deployable at all.

Production values cannot be brought into CI. If Doctor requires them, it will not be
adopted in the environments where it matters most.

In value-blind mode a report carries, for every sensitive key:

```text
present          yes / no
origin           which source supplied it
length           integer
digest           producer-specific change fingerprint
```

Every rule in section 8 except the type and range checks operates on presence, origin
and digest alone. Doctor can therefore audit a production environment without ever
seeing a production credential, and the digest still detects "the value changed"
without revealing what it changed to.

The native writer's `fnv1a:` digest is an unsalted, non-cryptographic
change fingerprint, not proof of equality or protection against guessing.
It covers the complete text byte sequence, including embedded NUL.
Reports from releases through 1.0.0 used the numeric formatting buffer
for text: long sensitive values could carry length zero and the empty
fingerprint. Regenerate those reports before relying on comparisons.

This works because it reuses the redaction mechanism from `Decisions.md` D-13. One
mechanism, two features.

---

## 6. Collecting a Production Report

The gap Doctor must cross is that CI cannot resolve production configuration. The
answer is that production reports itself.

```text
Deploy N
    application starts
    emits observed report (value-blind)
    report stored as a build artifact

Deploy N+1 in CI
    Doctor computes the predicted report for the change
    Doctor diffs predicted(N+1) against observed(N)
    rules run over the pair
```

Two consequences follow. The library needs a startup-time report emission hook, which
is small and belongs to Phase 1. And the very first run has no baseline, which is what
the baseline mechanism in section 9 handles.

---

## 7. Commands

```text
configforge doctor explain <key> [--report R]
    single key, full candidate chain, selection reason

configforge doctor dump [--format table|json|md] [--blind]
    the full matrix for one environment

configforge doctor diff <reportA> <reportB>
    value differences, origin differences, shadow differences, presence
    differences, and defaults that differ where a key takes one

doctor history <report>... [--key K]
    each change from one report to the next, oldest first: a key added or
    removed, a value changed (by digest for a secret), a value from another
    source, and a step nothing can compare; never the value itself

doctor blame <key> <report>...
    the report since which <key> has been as the last report has it, and what
    changed there; "at least" when an earlier step cannot be compared

configforge doctor lint <report> [--rules R] [--baseline B] [--format text|sarif|markdown]
                         [--base <before> --only-changed]
    rule catalogue against a single report

configforge doctor parity <reportA> <reportB> [--require-same-origin]
    contract-level comparison: which keys exist, which are required, which are
    satisfied by the same kind of source

configforge doctor emit [--blind] [--out F]
    produce a report from the current environment

configforge doctor --version
    the version, and the commit Doctor was built from: "ConfigForge Doctor 0.3.0 (20d0f4b)"
```

`explain` and `dump` are convenience wrappers over the library's own capability, made
available without adding a flag to the host application.

### 7a. Querying a report with jq

A report is JSON with a published schema, so a question the commands do not ask is a jq
filter away - no code here, and no second query language to learn (Roadmap.md says why
one was decided against). Each of these was run against a fixture report:

```sh
# Which keys did the environment win?
jq -r '.keys | to_entries[] | select(.value.selected.source == "env") | .key' report.json

# Every losing candidate, from which source, and where it was set.
jq -r '.keys | to_entries[] | .key as $k | .value.candidates[]
       | select(.outcome == "shadowed") | "\($k)  \(.source)  \(.at // "-")"' report.json

# The sensitive keys, and the source each one came from.
jq -r '.keys | to_entries[] | select(.value.definition.sensitive == true)
       | "\(.key)  \(.value.selected.source)"' report.json
```

The shape they rely on: `keys` maps each key to its `definition` (or `null`), its
`candidates` in precedence order - each with `source`, `at`, and an `outcome` unless it
was selected - and the `selected` source, which is `null` for a key no candidate won.
`sources` lists every source with its `kind` and `rank`.

---

## 7b. As a library

Everything above is also `libconfigforge_doctor`: `cf_doctor.h`,
installed with the configuration library as
`ConfigForge::configforge_doctor`. The command line is that library with
argument parsing and printing around it, so the two cannot disagree
about a rule.

| Call | What it does |
| ---- | ------------ |
| `cf_doctor_new`, `cf_doctor_free` | a context; findings accumulate in it until it is freed |
| `cf_doctor_lint(d, cfg, profile, &err)` | lint a configuration in process: its full report is written in memory, read as `doctor lint` reads a file, and wiped |
| `cf_doctor_lint_report`, `cf_doctor_lint_file` | lint a report held as text, or in a file |
| `cf_doctor_diff`, `cf_doctor_diff_report`, `cf_doctor_diff_files` | `doctor diff`, on two configurations, two texts or two files |
| `cf_doctor_history_files`, `cf_doctor_blame_files` | `doctor history` and `doctor blame` over report files in the caller's order, oldest first: every file is read before anything is recorded, and room for every finding is reserved first, so an unreadable report or a full heap adds nothing |
| `cf_doctor_accept`, `cf_doctor_baseline` | a baseline entry, or a `--baseline` file |
| `cf_doctor_severity`, `cf_doctor_severity_file` | a severity line, or a `--severity` file |
| `cf_doctor_count`, `cf_doctor_finding`, `cf_doctor_count_at` | the findings, each with its rule, level, key, place and message |
| `cf_doctor_stale_count`, `cf_doctor_stale` | baseline entries that matched nothing |
| `cf_doctor_rule_count`, `cf_doctor_rule_id`, `cf_doctor_rule_text` | the catalogue below |
| `cf_doctor_live_check` with a `cf_doctor_gate` | the check a `cf_live` reload passes through: each snapshot linted before readers have it, refused on a finding at the gate's level; the gate starts from `CF_DOCTOR_GATE_INIT`, as the library's other structs do (D-51) |

Nothing in it prints, and nothing in it is global: two contexts can run
at once, one thread to a context. It is static, so there is one ABI to
keep - the configuration library's - and it is not part of the
configuration library at all, which is how the line in section 2 holds:
a program that reads configuration does not carry the rules that audit
it unless it asks for them.

**From Python**, `bindings/python/src/configforge_doctor.py` runs the `doctor`
program and reads its SARIF output - so a script sees exactly what a CI
job sees, from the same rules and severities, and there is no ABI to
keep, the library being static:

```python
import configforge_doctor as doctor

for f in doctor.lint("app.json", baseline="doctor.baseline", fail_on="never"):
    print(f.rule, f.level, f.key, f.message, f.file, f.line)
```

`lint`, `diff`, `history`, `blame` and `version`; `DoctorError` for input
Doctor cannot use. The standard library only. `doctor.python` holds it to
the fixtures.

### 7c. A sequence of reports

`diff` compares two environments; `history` and `blame` follow one
service through its deployments. The reports are given oldest first, and
that order is the only one Doctor uses: a report's `generated` time says
when it was written, which a re-run of an old build would get wrong.

```text
$ doctor history prod-0912.json prod-0919.json prod-0926.json
HISTORY info    server.host   prod-0919.json: same value, now from 'user-file' rather than 'env'
HISTORY info    db.password   prod-0926.json: value changed (by digest), still from 'env'
HISTORY info    server.port   prod-0926.json: value changed, now from 'cli' rather than 'env'
```

Findings come step by step, and within a step in the later report's key
order. Each names the later report of the step and is placed where that
report says the value was set. Comparison is `diff`'s: shown values are
compared as text, redacted ones by digest, and a step where one report
has only a digest and the other none to match is reported as not
comparable rather than as unchanged. No message carries a value or a
digest, so a history can go into a ticket the reports themselves would
not. A change is `info`; `--fail-on info` fails a run on any change.
Every report is read before anything is recorded, so an unreadable one
is exit 2 with no partial history, as is a `--key` or a `blame` key that
none of the reports has. The fingerprint limits in section 5 apply: a
digest match is evidence, not proof.

## 8. Rule Catalogue

Each rule has a stable identifier so it can be suppressed individually. Rules marked
**blind-safe** operate without value access.

| ID | Rule | Default severity | Blind-safe |
| -- | ---- | ---------------- | ---------- |
| CFD001 | **Unreachable value.** A key is set where it can never win: the same source sets it again later in the same file or command line, and within one source the later occurrence wins. | error | yes |
| CFD002 | **Origin drift.** Same value in both environments, different source kind. | warning | yes |
| CFD003 | **Shadow divergence.** Overridden in one environment, not the other. | warning | yes |
| CFD004 | **Missing required.** Required key resolves to nothing. | error | yes |
| CFD005 | **Undefined key present.** A source key matches no definition; nearest match suggested. | warning | yes |
| CFD006 | **Contract violation.** Value fails type or range. | error | no |
| CFD007 | **Sensitive value from an unsafe source.** Key marked sensitive is supplied on the command line, or by a file every user of the machine can read. Whether a file is committed to version control is deliberately out of scope: that is the repository's business, not the configuration's. | error | yes |
| CFD008 | **Default-only in production.** Key resolves to its built-in default with no source supplying it. Benign in development, worth knowing in production, so its severity follows the report's profile. | info; warning in `production` | yes |
| CFD009 | **Deprecated key still set.** | warning | yes |
| CFD010 | **Dead source.** A configured source wins for no key at all; everything it sets lost to precedence. A warning rather than an error, because a system file wholly overridden by a user's is often what somebody meant. | warning | yes |
| CFD011 | **Present but empty.** Distinguishes empty from absent. | warning | yes |
| CFD012 | **Parity gap.** Key present in one environment's contract and absent in the other's. | warning | yes |
| CFD013 | **Precedence inversion.** Policy places a less specific source above a more specific one for a parameter. | info | yes |
| CFD014 | **Digest change on a sensitive key** without a corresponding change entry. | info | yes |
| CFD015 | **Array truncation hazard.** An element lies at or beyond the length the winning `<key>.count` gives its array - most often a longer list in a lower-precedence file under a shorter count from the command line - and was discarded. The finding names the count and where it came from. | warning | yes |
| CFD016 | **Policy violation.** A source outside the key's per-parameter policy supplied it. The library rejected the candidate (D-33); this is where that becomes loud. | error | yes |
| CFD017 | **Default drift.** In a diff, a key takes its declared default on one side or both, and the two defaults differ, or one side has none. A program's new default reaches every deployment that never set the key, and no configuration file shows it: `doctor diff deployed.json upgrade.json` names those keys before the upgrade does. Where both sides take their defaults, this is the finding and the value difference is not repeated. | warning | yes |
| CFD018 | **Set but never read.** A source set the key and the program never read it: a setting that changes nothing, most often one the program has stopped reading. Only a report from a program that tracks its reads (`cf_builder_track_reads`) carries the evidence, and only for reads before the report was taken, so take it once the program has done its work. A key whose default won is not a setting and is not reported. | info | yes |

`HISTORY` and `BLAME` are not rules of a configuration but records of a
sequence (section 7c): each is `info`, and a severity file, a baseline or
`--fail-on` treats them as it treats any identifier.
**What one report can prove.** The first draft of CFD001 read "a key set in a source
that can never win", and the natural implementation — every shadowed candidate — is
wrong twice over. A file shadowed by an environment variable here may win in the next
environment, where the variable is unset, so one report cannot prove "never"; and a
finding per shadowed key buries the real one under precedence working as designed. Two
rules came out of it instead. CFD001 is the case that *is* provable: the same origin
sets the key twice, the later line wins wherever the file is read, and editing the
earlier one has no effect. CFD010 is the aggregate: a source that wins nothing at all.

**One problem, one finding.** A rejected candidate is reported by the rule that owns the
rejection — CFD006 for a contract, CFD016 for policy, CFD015 for truncation — and by no
other. A secret on the command line used to be three errors at once (CFD006, CFD010 and
CFD016), and a report that says one thing three times teaches its reader to skim.

**A key nothing won is still a key.** An optional key with no default, whose only value
came from a source its policy refuses, is in the report with `"selected": null`, and
Doctor reads it as it reads any other: CFD016 is how a secret that reached the command
line of such a key is found at all. Doctor used to find keys by their winner, so this
one was in no rule's sight - and a report with no other key was refused as describing
none. CFD004 reads the same state for a *required* key, which the library cannot write:
it will not build a configuration with a required key unset, so that finding comes from
a report another producer wrote.

**Drift is only drift when the values agree.** CFD002 is the same value from a different
kind of source - an environment variable here, a file there - which a diff of the values
shows nothing of. Two values that differ are a difference, and the value diff says so
once; calling it drift as well reported every such key twice, and put a warning on the
commonest difference there is. Kinds are compared, not names: `cli` and `args` are both
the command line. A secret is compared by its digest, so a blind report can be audited;
a digest set against a value - a blind report against a full one - neither agrees nor
differs, and is not reported.

**Doctor never looks at its own disk.** It judges reports from other machines, so
whether a file is world-readable is recorded *by the library, in the report*, at the only
point the file is at hand: `"world_readable": true` on a sensitive candidate that came
from a file. Where the platform cannot say - Windows, where "everyone may read this" is
an access-control walk rather than a bit - the field is absent and the rule is silent,
because an answer that guessed would be wrong in both directions.

Not implemented yet: CFD012 against a contract rather than a key list.

CFD001, CFD002 and CFD003 are the rules that justify the tool. The rest come almost
free once the report exists, and they are what make it worth running every build.

Example output, as the tool prints it:

```text
CFD001  error   server.port   app.ini:2:1 always loses to app.ini:9:1, set later by
                              the same source; editing it has no effect
CFD010  warning user-file     supplies 2 value(s) and wins none; everything it sets is
                              shadowed, e.g. staging.ini:3:1

CFD002  warning database.pool_size
        production: 32  from environment
        staging:    32  from file (bundled in image)
        values agree; sources do not
```

---

## 9. Severity, Baseline and Adoption

The most common way a lint tool dies is producing two hundred findings on first run in
an existing repository. Three mechanisms prevent that, and all three are required from
the first release rather than added after the first complaint.

**Baseline.** `doctor lint <report> --baseline .configforge-baseline` records existing
findings as accepted. Only new findings fail the build, and the tool reports entries
that no longer match anything without failing on them.

As built, the file is one finding per line — `CFD001 server.port`, with `#` comments —
rather than the JSON with a reason field this section first described. It is read in
code review, and a line somebody can delete by hand is a baseline somebody will
actually shrink; the reason goes in the comment above the line.

**Configurable severity.** Every rule's severity is overridable per project, in a file
of the same shape as the baseline and for the same reason - it is read in review:

```text
CFD010 warning              this rule, everywhere
CFD008 error production     this rule, in reports with this profile
CFD011 off                  not this rule at all
```

`doctor lint <report> --severity .configforge-severity`. A profile line wins over a plain
one, a plain one over the profile defaults the catalogue states, and those over the
rule's default. An unknown severity is unusable input (exit 2) rather than a line
skipped: a file half understood leaves the other half silently at its default.

This is also what profiles are for, as D-12 decided them: a label on the report, not a
concept in the library. A rule whose meaning depends on the environment says so in its
catalogue entry, and the severity follows the label.

**Progressive mode.** `doctor lint <report> --base <before> --only-changed` restricts
the rules to what the change under review touched: a key whose candidates - their
sources, outcomes, values and digests, in order - are not the base's, or that only one
side has. Where a value was set is not compared, since moving a line changes nothing.
A finding about a source rather than a key is kept when a touched key came from it.
The rest are counted - "3 on what the change did not touch" - so the run says what it
left out. The change was the missing piece, and the base report is it: the action and
the hook pass `only-changed: true` / `--only-changed` with their base, and the library
has `cf_doctor_only_changed_file`. With a baseline, this is how a large configuration
adopts Doctor one change at a time.

---

## 10. Exit Codes

```text
0   no findings at or above the failure threshold
1   findings at or above the threshold
2   report unreadable, schema version unsupported, or unusable options
3   internal error
```

The threshold is `--fail-on error|warning|info|never`, error by default. SARIF output
keeps the same exit code, so one invocation can both annotate and gate.

Reports are not comparable when their schema versions differ across a breaking
boundary, or when one is value-blind and a requested rule needs values. Doctor says so
rather than silently skipping rules.

---

## 11. CI Integration

```text
- run: configforge doctor emit --blind --out predicted.json
- run: configforge doctor diff observed-prod.json predicted.json --fail-on error
- run: configforge doctor diff observed-prod.json upgrade.json --fail-on warning
            # an upgrade's changed defaults (CFD017), before it is deployed
- run: configforge doctor lint predicted.json --baseline .configforge-baseline
            --severity .configforge-severity --format sarif > doctor.sarif
```

On GitHub the same is an action in this repository, which builds Doctor
from source and writes SARIF for the code-scanning upload:

```yaml
- uses: develooper1994/configforge/actions/doctor@main
  with:
    report: build/provenance.json
    baseline: .configforge-baseline
- uses: github/codeql-action/upload-sarif@v3
  if: always()
  with:
    sarif_file: configforge-doctor.sarif
```

**On a pull request** it does what a reviewer cannot do by reading the
diff of a configuration file: given the base branch's report as `base`,
it runs `doctor diff` between the two and says which key changes value,
which source would win now, which default an upgrade moved (CFD017) and
which setting the program never reads (CFD018). The findings go to the
log, to the job summary as Markdown (`--format markdown`, which lint, diff,
history and blame print too), and with `comment: true` to one comment on the
pull request, updated on each push rather than added again:

```yaml
permissions:
  pull-requests: write          # for the comment; the summary needs none
steps:
  - uses: develooper1994/configforge/actions/doctor@main
    with:
      report: head/provenance.json
      base: base/provenance.json
      comment: true
```

A pull request from a fork has a token that cannot write a comment; the
step says so and passes, and the summary has everything. Secrets are
compared by digest and never shown; values that are not secret are, as
`doctor diff` shows them - take the reports `--blind` to show none. The
step's exit code is the worst of the lint and the diff at `fail-on`.

**Before a commit**, the same as a [pre-commit](https://pre-commit.com)
hook, for a project that keeps its reports in the repository:

```yaml
- repo: https://github.com/develooper1994/configforge
  rev: v1.1.0
  hooks:
    - id: configforge-doctor
      files: ^deploy/.*provenance.*\.json$
      args: [--base, deploy/prod-provenance.json, --fail-on, warning]
```

Each staged report is linted, and diffed against `--base` when one is
given. Doctor is `$CONFIGFORGE_DOCTOR`, or is built once from the hook's
checkout into `~/.cache/configforge-doctor/<commit>`; nothing is
downloaded. Without pre-commit, `actions/doctor/pre-commit.sh` is a
`.git/hooks/pre-commit` of its own. The scripts the action and the hook
run - `run.sh`, `comment.sh`, `pre-commit.sh` - are tested by
`doctor.action`, the comment against a stand-in for `gh`.

Output formats: human-readable text, SARIF 2.1.0 (`--format sarif`) and
Markdown (`--format markdown`), a table whose cells escape what would
format, link or end a row. SARIF matters
more than it looks: it makes findings appear inline on a pull request in most forges
without any per-forge integration work. A finding that knows where its value was set
carries the file and line; one that does not - a variable, a command-line argument -
points at the report itself, which is still the artifact the finding is about.

---

## 12. Foreign Ecosystem Adapters

There is a strong temptation to make Doctor ecosystem-independent by reimplementing
the precedence rules of Viper, Spring, dotenv and others. **This should be refused.**
It is the same trap as maintaining hand-curated hardware compatibility data: knowledge
that is not published in structured form, that goes stale with every upstream release,
and that one person cannot maintain across ten ecosystems.

The sustainable position is that **Doctor owns the report format, not the resolution
logic.** Each ecosystem supplies its own origin data and a thin adapter converts it.

| Ecosystem | Native origin source | Adapter effort |
| --------- | -------------------- | -------------- |
| ConfigForge | native report | none |
| .NET | `ConfigurationRoot.GetDebugView()` | built: `doctor import-dotnet`; winners only |
| Spring Boot | `/actuator/env`, origin-tracked | built: `doctor import-spring` |
| Git config | `git config --list --show-origin --show-scope` | built: `doctor import-git` |
| systemd | `systemd-analyze cat-config`, `systemctl cat` | built: `doctor import-systemd` |
| Go / viper | none available | requires a helper in the host application |
| dotenv | file parse only, no precedence | partial; presence and origin only |

Adapters that cannot supply origins produce presence-only reports, and Doctor says
which rules it is unable to run rather than reporting a clean result.

**The git adapter, as built.**

```text
git config --list --show-origin --show-scope | doctor import-git > git.json
doctor lint git.json
```

Git publishes everything this needs, so the adapter decides only how to write it down,
and goes through the library's own report writer so the result cannot drift from the
schema. Four decisions, each written where it is made:

* precedence is git's documented order - command, worktree, local, global, system - and
  within one scope the later line wins, which is ConfigForge's rule too.
* a multi-valued key such as `remote.origin.fetch` looks in this output exactly like a key
  set twice and overridden. It is recorded as git printed it. Git gives no line numbers,
  so CFD001 - the one rule that would call the earlier occurrence a mistake - cannot
  fire on it.
* a key a report cannot carry, such as one with a URL in its subsection, is left out and
  counted on stderr rather than rewritten into a spelling that could collide with
  another.
* the report is value-blind unless `--full`: git configuration carries credentials more
  often than it looks.

**The Spring Boot adapter, as built.**

```text
curl -s localhost:8080/actuator/env | doctor import-spring > app.json
```

Spring publishes the sources in the order it consults them, and an origin for most
properties. The adapter writes that order down as precedence, Spring's first active
profile as the report's, and `class path resource [application.properties] - 3:13` as
`application.properties:3:13`. Two decisions differ from git's:

* a name a report key cannot carry - `SERVER_PORT` from the environment, before Spring's
  relaxed binding has made it `server.port` - is left out and counted, not re-spelled.
  The binding is Spring's rule, and re-spelling it here is the reimplementation this
  section refuses.
* what is secret is Spring's sanitiser's decision. A value it masked (`******`) is
  sensitive, and on the command line CFD007 reports it; what it did not mask, it
  already exposed. Redacting every key, as the git adapter does, would make every
  command-line property a secret to CFD007.

**The .NET adapter, as built.**

```text
Console.Write(((IConfigurationRoot)config).GetDebugView());   // in the program
... | doctor import-dotnet > app.json
```

`GetDebugView()` prints each key with the provider that won it, and nothing about the
providers that lost. So the report has one candidate a key, and the rules that need a
chain - CFD001, CFD010 - have nothing to say about it. Keys are lowered, .NET's being
case-insensitive, and `:` becomes `.`. A provider's name is the parenthesised group that
ends the line, counted from the right, because `(Optional)` is part of one. Each
provider's kind is its own type - `commandline`, `environment`, `json` - rather than
Doctor's `cli`: the report is value-blind unless `--full`, every key in it sensitive,
and CFD007 would otherwise call every command-line key a secret. What a blind .NET
report is for is `doctor diff` of two environments, by digest.

**The systemd adapter, as built.**

```text
systemd-analyze cat-config systemd/journald.conf | doctor import-systemd > journald.json
systemctl cat nginx.service | doctor import-systemd > nginx.json
```

Both commands print every file systemd reads, the main one and then each drop-in, in
the order systemd applies them, each under a `# /path` header. That order is the
precedence: each file is a source ranked above the one before it, and within a file the
later line wins. The files are printed whole, so a place is `path:line` counted from the
header, and CFD001 fires on a key set twice in one file - as CFD010 does on a drop-in
that wins nothing. `[Journal] Storage` is `journal.storage`. List keys (`ExecStart=`)
accumulate in systemd and an empty assignment resets them; the adapter records the last
assignment as the winner and does not reimplement which keys are lists. A `# /...` line
is a header only at the start or after a blank line, as both commands print it. Blind
unless `--full`: `Environment=` carries credentials.

---

## 12a. Replay Is Not a Doctor Feature

Doctor reads reports to compare them. The library reads reports to *become* them. These
are different capabilities over one artifact, and keeping them separate matters:

* Doctor never needs values and should default to value-blind operation.
* Replay needs values and therefore handles a credential-bearing file.

A report produced for Doctor should be value-blind. A report produced for replay is
produced deliberately, used immediately and deleted. Conflating the two would mean either
crippling replay or routinely writing secrets into CI artifacts.

---

## 13. Non-Goals

* Resolving configuration itself, outside of reading reports.
* Storing secrets, or requiring access to them.
* Reimplementing any other library's precedence rules.
* Editing configuration. Doctor reports; it does not fix.
* A hosted service, a dashboard, or drift alerting.
* Replaying reports. That is a library capability, not a Doctor command.
* Kubernetes-specific behaviour.

---

## 14. Roadmap

Doctor depends on exactly one library deliverable: the provenance report artifact from
Phase 1. It can start any time after that.

```text
D-Phase 1   report reader, dump, explain, diff, CFD001–CFD003
D-Phase 2   full rule catalogue, baseline, severity configuration
D-Phase 3   SARIF output, CI templates, emit hook in the library
D-Phase 4   foreign adapters: .NET, Spring, git config, systemd (built)
D-Phase 5   history and blame over a sequence of reports (built)
D-Phase 6   default drift between two reports, CFD017 (built)
D-Phase 7   keys set and never read, from read coverage, CFD018 (built)
```

Estimated size if the library already emits reports: 1500–2500 lines, three to four
weeks. The expensive part, provenance, lives in the library and is paid for there.

---

## 15. Honest Positioning

Doctor does not generate revenue and is not expected to. `conftest`, `kubeconform` and
`dotenv-linter` are all free and all widely used, and the one company that built a
business in the adjacent manifest-policy space was acquired and its product eventually
discontinued. Treating that category as a revenue bet would be reading the evidence
backwards.

Doctor is worth building for different reasons, and they are the reasons this whole
ecosystem exists:

**It makes the library worth adopting.** "A C++ config library" competes with a dozen
good free options. "The config library that tells you why your production value is
what it is, and catches it in CI" is a distinct proposition.

**It is the demonstrable artifact.** A library is read; a tool is run. CFD001 firing on
someone's real repository and revealing a file they have been editing for months with
no effect is a more convincing argument than any README.

**It validates the design cheaply.** If the provenance model is wrong, Doctor will
fail to express a rule, and that failure surfaces in weeks rather than after the ABI
is frozen.

**It closes the loop on repeated code.** The recurring work being removed is not only
parsing and precedence. It is also the debugging session that happens every time a
deployment behaves unexpectedly and someone adds print statements to find out where a
value came from. Doctor removes that session from every future project, which is the
same goal the library has, applied one layer up.
