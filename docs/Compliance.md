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
| **MISRA C:2012** (with Amendments 1-4; MISRA C:2023 consolidates them) | coding, C | the library: `src/`, `include/` | planned: matrix and gate, below |
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
| **Standard I/O** | MISRA C Rule 21.6 (required): no `<stdio.h>` | a configuration library must read files | Deviation from 21.6, confined to the file sources in `src/bridge/` and the platform layer. A program that reads only from memory (`cf_builder_text`, the document sources) calls none of it; an option that leaves the file sources out of the build (as `CF_WITH_DIR` leaves out directories) is part of the MISRA profile's work. |
| **Recursion** | MISRA C Rule 17.2 (required): none | nested documents are naturally recursive | No deviation needed where the readers are iterative; where a reader recurses, the depth is bounded (D-82's budget, the depth limit) and the bound is the deviation's evidence. The audit lists every recursive function. |
| **`goto`** | MISRA C Rule 15.1 (advisory): none; CERT MEM12-C recommends `goto` for cleanup | | Not used: the library has no `goto`. Nothing to decide. |
| **Fixed-width types** | MISRA Dir 4.6: use sized typedefs | C89 has no `<stdint.h>` | Followed through the library's own `cf_i64`, `cf_u64` and friends, sized by `CF_STATIC_ASSERT`. |
| **C version** | MISRA C:2012 targets C90 and C99; MISRA C:2023 adds C11 | the project's floor is C89 (D-72) | Compatible: C89/C90 is MISRA's first language. Code that needs C99 or later is optional (`CF_WITH_PREFERENCES` on macOS compiles `posix.c` as C99 for CoreFoundation's headers) and is outside the MISRA profile. |
| **Floating point** | MISRA rules on float comparison and conversion | configuration values can be reals | Real values are optional (`CF_WITH_REAL`); the MISRA profile can leave them out. Where they are in, the parser and printer are listed in Safety.md's "Not yet proven". |

## The MISRA profile

A safety user does not need the whole library. The **MISRA profile** is a
named build configuration (a CMake preset, `misra`, built from the
existing `minimal` one) made of what such a user takes: the core, memory
and document sources, a fixed arena, no directories (`CF_WITH_DIR=OFF`),
no threads (`CF_SINGLE_THREADED`), no reals unless asked
(`CF_WITH_REAL=OFF`). The matrix
and the gate are checked against this profile first; the full library is
reported against too, with its extra deviations listed separately.

## Deviation records

Each deviation is one entry in `docs/compliance/deviations.md` (planned),
in the form MISRA Compliance:2020 asks for:

* the rule and its category (mandatory rules are never deviated);
* where it applies (files, functions, or "the whole library");
* why: the conflict above, or the requirement that needs it;
* what makes it safe: the design rule, test, fuzzer or proof that holds
  the risk the rule was written against;
* who decided, and the D-number when it is a decision of the project.

A suppression in the checker's configuration points at its record; a
suppression with no record fails the gate.

## Automation

What runs, and where. A row is "planned" until its gate is in the tree.

| Check | Tool | Gate | Runs | Status |
| ----- | ---- | ---- | ---- | ------ |
| CERT C rules a tool can see | clang-tidy `cert-*`, warnings as errors | the `analyze` preset, `EXTRA=tidy` | every commit | done |
| MISRA C rules a free tool can see | cppcheck with its MISRA addon, on the MISRA profile | `gate.misra`: fails a violation that has no record | every commit (tier 1) | planned |
| Records and suppressions agree | a script reads `deviations.md` and the suppression list | inside `gate.misra`: a suppression without a record, or a record no suppression uses, fails | every commit | planned |
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
  ask for, is not measured yet.
* **Static analysis and proofs**: the tables above and Safety.md's CBMC
  proofs.
* **Known limits**: Safety.md's "Not yet proven", and the deviation
  records.
* **Tool qualification**: not provided; the user's process qualifies the
  tools it relies on.

## Order of work

1. The MISRA profile preset, and cppcheck's addon run against it to
   count what there is.
2. The deviation records for the conflicts above, and `gate.misra`.
3. The CERT matrix of rules clang-tidy does not see.
4. The CWE table and `gate.cwe`.
5. The compliance report as a release artifact.
6. MISRA C++:2023 for the wrapper.
7. A qualified tool's run, when one can be had.

These are planned in `docs/Roadmap.md`, "Critical-system standards".
