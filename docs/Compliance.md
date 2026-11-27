# Coding standards for critical systems

ConfigForge is meant to be usable where a failure is expensive: a car, a
medical device, an industrial controller, a flight system. Such a project
asks first which coding standard a library was held to, and how that is
shown. This page answers: which standards apply, which are followed, which
rules are set aside and why, and what checks each one on every commit.

**The rule of this page:** nothing is claimed that a check does not show.
"Follows MISRA C" here means "every rule is listed below with its status,
each deviation is written down with its reason, and a gate in CI fails a
new violation". It never means "certified": no tool qualified for a
safety process (Polyspace, Helix QAC, PC-lint Plus, LDRA) has checked the
code, and this page says so until one has.

`docs/Safety.md` says how memory and thread safety are held (arenas,
immutable snapshots, sanitizers, fuzzing, CBMC proofs). This page builds
on it and does not repeat it.

## Which standards, and what kind each is

A coding standard is a list of rules about the code. A process standard
is about how software is made and shown to be right; a library cannot
pass one alone, but it can give its user's certification the evidence it
asks for.

| Standard | Kind | Scope here | Status |
| -------- | ---- | ---------- | ------ |
| **MISRA C:2012** (with Amendments 1-4; MISRA C:2023 consolidates them) | coding, C | the library: `lib/` | partly: `gate.misra` runs the addon over the library's sources and every report is recorded (`docs/compliance/deviations.txt`); the matrix and the public headers' zero are planned, below |
| **SEI CERT C** | coding, C, security | the library, Doctor | partly: clang-tidy's `cert-*` checks run as errors on every build (`.clang-tidy`); the matrix of rules no tool checks is planned |
| **CWE** (Top 25, and CWE-658, weaknesses of C) | weakness catalogue | everything that reads untrusted bytes | planned: a table from each weakness to the design rule, test, fuzzer or proof that holds it |
| **MISRA C++:2023** (absorbs AUTOSAR C++14) | coding, C++ | the C++ wrapper, `configforge.hpp` | later: the wrapper is a thin header over the C API |
| **ISO 26262, IEC 61508, IEC 62304, DO-178C, EN 50128** | process | none by itself | not claimed; what the library can give a user's process is listed at the end |

### Considered and excluded

| Standard | Why not |
| -------- | ------- |
| **Barr-C (Embedded C Coding Standard)** | Largely a style guide; its layout rules disagree with the formatter the project is held to (`.clang-format`, `gate.format.c`), and its safety rules are a subset of MISRA's. |
| **JSF AV C++** | Superseded by MISRA C++:2023 for new work. |
| **AUTOSAR C++14** | Merged into MISRA C++:2023; following both would be following one twice. |
| **HIC++ (High Integrity C++)** | Overlaps MISRA C++:2023, which the safety industry asks for by name. |
| **NASA/JPL "Power of Ten" and JPL C** | Short and good, but no reviewer asks for it by name; its rules (no recursion, bounded loops, no dynamic memory after initialisation, assertions) are covered below through MISRA and Safety.md. |

The bindings (Rust, Python) are outside this page: each language has its
own tools (`cargo clippy`, the Python package's tests), and the C library
underneath is what the standards above are about.

## Where the standards disagree

Standards written for different worlds contradict each other, and a
library that serves both a microcontroller and a server cannot satisfy
every rule literally. Each conflict is decided once, here, and the
decision becomes a deviation record (below) rather than an argument in
each review.

| Topic | One says | The other says | Here |
| ----- | -------- | -------------- | ---- |
| **Dynamic memory** | MISRA C Rule 21.3 (required): no `malloc`/`free` | CERT C MEM rules: allocate, check every result, free once | Deviation from 21.3. All memory comes through the allocator interface (D-03) into arenas freed whole; `cf_arena_fixed` lets a program run with no heap at all, on a static buffer. A MISRA user builds with a fixed arena and the deviation does not apply to them. |
| **One exit per function** | MISRA C Rule 15.5 (advisory): a single return | CERT and common practice: return early on error, which avoids deep nesting and the error flag MISRA's form needs | Deviation from 15.5 (advisory, so allowed with a record). Early returns on error are the house style (`docs/Conventions.md`). |
| **Standard I/O** | MISRA C Rule 21.6 (required): no `<stdio.h>` | a configuration library must read files | Deviation from 21.6, confined to the file sources in `lib/config/src/bridge/` and the platform layer. A program that reads only from memory (`cf_builder_text`, the document sources) calls none of it; an option that leaves the file sources out of the build (as `CF_WITH_DIR` leaves out directories) is part of the MISRA profile's work. |
| **Recursion** | MISRA C Rule 17.2 (required): none | nested documents are naturally recursive | No deviation needed where the readers are iterative; where a reader recurses, the depth is bounded (D-82's budget, the depth limit) and the bound is the deviation's evidence. The audit lists every recursive function. |
| **`goto`** | MISRA C Rule 15.1 (advisory): none; CERT MEM12-C recommends `goto` for cleanup | | Not used: the library has no `goto`. Nothing to decide. |
| **Fixed-width types** | MISRA Dir 4.6: use sized typedefs | C89 has no `<stdint.h>` | Followed through the library's own `cf_i64`, `cf_u64` and friends, sized by `CF_STATIC_ASSERT`. |
| **C version** | MISRA C:2012 targets C90 and C99; MISRA C:2023 adds C11 | the project's floor is C89 (D-72) | `cf_source_needs_c` declares when a file needs C99 or C11. The MISRA profile refuses these declarations at configure time. |
| **Floating point** | MISRA rules on float comparison and conversion | configuration values can be reals | Real values are optional (`CF_WITH_REAL`); the MISRA profile can leave them out. Where they are in, the parser and printer are listed in Safety.md's "Not yet proven". |
| **The environment** | MISRA C keeps to the standard language and library (Rule 1.2, advisory, on extensions; Rule 8.5 on where an external object is declared; the numbers are to be confirmed against the edition the matrix names) | a configuration library reads environment variables, and the platform layer reads them through POSIX's `environ` (`lib/platform/src/posix/posix.c`), not `getenv` | To decide with the matrix: a deviation confined to the platform layer, or the environment source left out of the MISRA profile. A program that reads only from memory calls none of it. |
| **Reading a word at a time** | MISRA C Rule 21.15 (required; to be confirmed as above): `memcpy`'s pointers are to compatible types; the Rule 11 group forbids the cast that would replace it | the word-at-a-time scans (`docs/plan/P-word-at-a-time.md`) read an `unsigned long` from bytes with `memcpy` | No deviation: the MISRA profile builds the vector library's scalar reference alone (`CF_LANES_BACKENDS=scalar`, D-101), the byte loops. |

## The MISRA profile

A safety user does not need the whole library. The **MISRA profile** is
defined in `cmake/profiles/misra.cmake`. Configure it with
`cmake --preset misra`. It includes the core, memory
and document sources, a fixed arena, no directories (`CF_WITH_DIR=OFF`),
no threads (`CF_SINGLE_THREADED`), no reals unless asked
(`CF_WITH_REAL=OFF`), and the byte loops rather than the word-at-a-time
or SIMD scans (`CF_LANES_BACKENDS=scalar`). The matrix
and the gate are checked against this profile first; the full library is
reported against too, with its extra deviations listed separately.

**The public headers come first, and take no deviation.** A user's own
checker reads `include/` inside every file of theirs that includes it,
so a violation there is a finding in their project, and a deviation
there is one they must adopt. The `.c` files they can set aside as
adopted code, with this page's records; the headers they cannot. The
gate's first target is therefore `include/` with no violation and no
record, before the sources are counted.

**Rules about a whole program.** Some rules can be judged only where
the program is whole: unused macros, types and tags (the Rule 2 group),
an object with external linkage used in one file only (Rule 8.7). A
library's public names are unused from its own side. The matrix marks
these "for the integrator to check", not "followed".

## Deviation records

Each deviation is one record in `docs/compliance/deviations.txt`, in the
form MISRA Compliance:2020 asks for. The file is plain text, one line a
record, because the gate that reads it is CMake (D-112):

| Line | Meaning |
| ---- | ------- |
| `tool \| cppcheck \| 2.19` | the cppcheck version the counts were made under; exactly one, first |
| `ignore \| 2.5 \| <reason>` | a rule the gate does not read (the rules about a whole program) |
| `<rule> \| <file> \| <count> \| <kind> \| <reason>` | the exact number of reports of that rule in that file |

A kind is `deviation` (decided, permanent, the reason naming its row above or
a D-number; refused in a public header), `tool` (the code complies and the
addon is wrong) or `backlog` (a real report no one has decided on: counted,
never growing, the first card to take the rule turns its lines into a fix or a
deviation). A record is by file and count, not file and line: a line number
moves with every edit. Each record carries what MISRA Compliance:2020 asks
for:

* the rule and its category (mandatory rules are never deviated);
* where it applies (files, functions, or "the whole library");
* why: the conflict above, or the requirement that needs it;
* what makes it safe: the design rule, test, fuzzer or proof that holds
  the risk the rule was written against;
* who decided, and the D-number when it is a decision of the project.

There are no suppressions: a `cppcheck-suppress` of a MISRA rule under
`lib/` fails the gate, and the record is the only place. Regenerate it with
`cmake -DSRC=. -DBIN=<misra build> -DDEFINES=<compiler macros> -DWRITE=ON -P
cmake/gates/GateMisra.cmake` and review the difference.

The records are one of three documents MISRA Compliance:2020 asks for,
and a user who takes the library as adopted code needs all three:

* the **guideline enforcement plan**: for each rule, which tool or
  review checks it (the Automation table below, one row per rule);
* the **deviation records**, above;
* the **guideline compliance summary**: for each rule, followed,
  deviated (with its record), or not applicable. The compliance report
  attached to a release is this summary.

MISRA's rule texts are under copyright. The matrix, the records and the
report name rules by number only; no rule text is committed.

## Automation

What runs, and where. A row is "planned" until its gate is in the tree.

| Check | Tool | Gate | Runs | Status |
| ----- | ---- | ---- | ---- | ------ |
| CERT C rules a tool can see | clang-tidy `cert-*`, warnings as errors | the `analyze` preset, `EXTRA=tidy` | every commit | done |
| MISRA C rules a free tool can see | cppcheck with its MISRA addon, on the MISRA profile | `gate.misra`: fails a report the record lacks and a record line that matches no report | every commit (tier 1), the `profiles (misra)` job | done (N-19) |
| Records and suppressions agree | `gate.misra` reads `deviations.txt` and scans `lib/` | there are no suppressions: one fails the gate, and a record no report uses fails as stale | every commit | done (N-19) |
| The compliance report | the same run, written as Markdown | uploaded as a CI artifact and attached to each release: every rule, its status, its count, its record | each release | planned |
| CWE coverage | the CWE table, checked to name only tests, fuzzers and proofs that exist | `gate.cwe` | every commit | planned |
| Rules no tool checks (MISRA's "undecidable" and the directives) | a review checklist, one line per rule, kept with the matrix | read at each release; the report says when it was last read | each release | planned |
| A qualified tool's run | Polyspace, Helix QAC or PC-lint Plus | none in CI (licensed); a run's report is attached when one is made | when available | not done |

The free tools see only part of MISRA: cppcheck's addon checks most
decidable rules of MISRA C:2012 but not all, and none of the undecidable
ones. The report says which rules each tool checked, so a reader knows
what "no violation" covers. A qualified tool is what a certification
body will ask for; until one has been run, this page says "follows, with
a free checker", not "compliant".

## What the library gives a user's safety process

A library is not certified on its own, but the evidence a process
standard asks for can be prepared once here and reused by every user:

* **Requirements traced to tests**: every rule in `docs/Decisions.md`
  (the D-numbers) names the tests that hold it; a generated table from
  each D-number to its tests is planned.
* **Structural coverage**: statement and branch coverage are measured by
  the `coverage` build; MC/DC, which DO-178C level A and ISO 26262 ASIL D
  ask for, is not measured yet. Free compilers can measure it (GCC 14's
  `-fcondition-coverage`, Clang 18's `-fcoverage-mcdc`); a `coverage`
  build that reports it is planned.
* **A safety manual** (planned, `docs/compliance/safety-manual.md`):
  what a user must hold for the evidence here to apply to their
  program. The build it was shown for (the `misra` preset, the version
  and commit id), and the assumptions of use: memory from a fixed
  arena, every returned status checked, the sources left out of the
  profile not linked. This is the document ISO 26262 asks of a
  component developed out of context.
* **Static analysis and proofs**: the tables above and Safety.md's CBMC
  proofs.
* **Known limits**: Safety.md's "Not yet proven", and the deviation
  records.
* **Tool qualification**: not provided; the user's process qualifies the
  tools it relies on.

## Order of work

The maintainer's aim (2026-11-27): the library usable in the most
critical work (safety-critical, high-integrity), with free tools only,
each kind of safety added in stages as far as it is feasible. Cards that
exist for the steps below: L-12a/b/c (MISRA headers, matrix, report),
M-20/M-21 (the evidence pack), K-2a/b (known weaknesses), K-9a-K-9i
(failure, thread and signal guarantees), L-25 (MC/DC with GCC's
`-fcondition-coverage`), L-26a/b (each D-number traced to its tests,
`gate.trace`), L-27 (the safety manual, `gate.manual`) and L-28 (the
CERT C matrix, `gate.cert`), written 2026-11-28.


1. The MISRA profile preset, and cppcheck's addon run against it to
   count what there is. Done (A-11, N-19).
2. The public headers with no violation, and the gate on them. Open: 33
   record lines (131 reports) in `include/` directories; the card that takes
   them to zero lets the gate refuse any `backlog` in one.
3. The deviation records for the conflicts above, and `gate.misra` on
   the sources. Done (N-19): 6,337 reports in 613 lines, 33 of them lines
   in `include/` directories, all `backlog`; the four deviations above are
   recorded, 17.3 is the addon's.
4. The CERT matrix of rules clang-tidy does not see.
5. The CWE table and `gate.cwe`.
6. The compliance report as a release artifact, and the safety manual.
7. MISRA C++:2023 for the wrapper.
8. A qualified tool's run, when one can be had.

This work comes before the optimization cards
(`docs/plan/P-word-at-a-time.md`), since it touches most source files;
only their measurement (P-1) may come earlier.

## What may be said, and when

| When | The README and a release may say |
| ---- | -------------------------------- |
| Today | CERT C checks are errors on every commit; the readers are fuzzed, run under sanitizers and pass the formats' conformance suites; the bounds named in Safety.md are proven with CBMC. |
| `gate.misra` is in CI | "Checked against MISRA C:2012 on every commit with cppcheck; every deviation is recorded; the compliance summary is attached to each release." |
| A qualified tool has run | "MISRA C:2012 compliant", with the tool, its version and the summary. |
| Never, without an assessor | "Certified" to ISO 26262, IEC 61508 or any process standard. |

A badge that can be had without a licence: the OpenSSF Best Practices
badge (a public self-assessment with a passing level), planned beside
the report.

These are planned in `docs/Roadmap.md`, "Critical-system standards".
