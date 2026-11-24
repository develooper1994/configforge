# The plan

`docs/Roadmap.md` says what the work is and how far it has come. This
page says how to do it, one unit at a time, in enough detail that the
unit needs no design from whoever carries it out. It is written to be
followed literally by whoever carries it out: every choice that can be made in
advance is made here, and every choice that cannot is marked
**TO DECIDE** and is not to be guessed.

## The loop

Whoever works does this and
nothing else:

```
 1. git fetch; git status -sb; git log --oneline HEAD..origin/main
      behind      -> git pull --ff-only, read the new commits, go to 1
      diverged    -> STOP. Tell the maintainer. Reset, rebase and force nothing.
      dirty tree you did not make -> STOP. Ask the maintainer.
 2. If HANDOFF.md exists at the root: do its steps first, then delete it
      in the commit that finishes its last step.
 3. Open the index below. Take the first card whose status is "open"
      and whose "needs" are all "done". No such card -> STOP: report
      which cards wait and on what.
 4. Card says TO DECIDE and the answer is not written in it -> ask,
      write the answer into the card, commit that alone, go to 3.
 5. git switch -c work/<card id>
 6. Do the card's steps in order. Never skip "see the test fail".
 7. Run ACCEPT (below). Not exit 0 -> fix, go to 7. After three tries
      on the same failure -> STOP: revert the unit (git restore, git
      clean of your new files), write what failed into the card under
      "Notes", commit that, report.
 8. Commit on the branch, as often as the work needs: the card's files,
      CHANGELOG.md and docs/Roadmap.md.
 9. git push -u origin work/<card id>; wait for CI (ACCEPT-CI).
      red -> read the failing job's log, fix, commit, push, go to 9.
      main moved meanwhile -> git merge origin/main on the branch, go to 9.
10. Only when the card is done - every step, ACCEPT, and ACCEPT-CI green
      on the branch's last commit - squash it onto main as one commit:
        git switch main; git merge --squash work/<card id>
        git commit    # subject "<card id>: <its goal>", body: the
                      # branch's commit subjects, in order
        test "$(git rev-parse HEAD^{tree})" = "$(git rev-parse work/<card id>^{tree})"
        git push origin main
      Delete the branch here and on the remote. The next commit to main
      sets the card's status in this index to "done (<that commit>)".
      Go to 1.
```

A card is one commit on main, the same tree CI passed. Work that is not
done is never squashed: a branch that stops or is handed back keeps its
commits. The cards on main before this rule (T-3 and earlier) keep
their commits: main's history is never rewritten.

**Code first, style at the group's end.** A card's ACCEPT and ACCEPT-CI
ask for everything but the style checks: clang-format (`gate.format.c`,
CI's clang-format step and format-patch job) and clang-tidy (CI's
clang-tidy step). A card may land with those red. When the group of
cards ends - the cards of one milestone, before its release - one unit,
`style-<milestone>` (branch `work/style-<milestone>`), formats the whole tree with the pinned clang-format,
fixes what clang-tidy finds, and lands with CI wholly green. No release
is tagged before it.

Work that must stop between 5 and 10 writes `HANDOFF.md` - the state in
three lines, then numbered steps, each with its files, its test and what
done looks like -, commits it on the work branch and
pushes the branch.

## ACCEPT

A unit is done when these exit 0. Nothing else counts - not a reading of
the output, not a belief that it should pass.

**ACCEPT-LINUX** - on Linux, or in WSL from a copy off `/mnt`:

```sh
git add -A        # verify checks exactly what is staged
cmake -DSOURCE=index -DJOBS=3 -DWORK=$HOME/cfv -P scripts/verify.cmake
```

It runs the presets `default`, `sanitize`, `portable`, `minimal` and
`analyze` and ends with `all green`. `-DMODE=quick` runs `default`
alone, for the loop inside a step; the unit is accepted by the full run.

**ACCEPT-WINDOWS** - in a Visual Studio developer prompt (`vcvars64.bat`):

```bat
cmake --preset default && cmake --build --preset default && ctest --preset default
```

**ACCEPT-CI** - after the push:

```sh
gh run watch --exit-status $(gh run list --branch work/<card id> --workflow ci --limit 1 --json databaseId --jq '.[0].databaseId')
```

CI has what no single machine has: macOS, FreeBSD's kqueue, musl, the
pinned clang-format 22, clang-tidy 21, MSVC `/analyze`, cargo. A card
whose code runs only there (marked **CI-only**) is accepted by CI alone,
and is expected to take more than one push.

Four failures that are not the code, so that they are recognised:

* `gate.format.c` in CI and not locally: the local clang-format is not
  22.1.8. Rewrite the lines CI names so that no version moves them
  (no string literal continued over lines as a call\'s argument, no
  condition over more than one line); do not argue with the formatter.
* `doctor.api` "sixteen times the keys": it measures time. Rerun it
  alone before believing it.
* `gate.apiref`: a header comment changed. Run
  `cmake -P scripts/generate/apiref.cmake` and stage `docs/API.md`.
* `bench gate` exit 3: the runner's compiler or C library is not the
  baseline's; regenerate the baseline with `scripts/bench/gate.py --write`
  from the CI artifact `bench-quick` (D-111).

## A card

Every unit of work is a card in a file under `docs/plan/`. A card has
these fields and no others:

| Field | Holds |
| ----- | ----- |
| **id** | e.g. `W4-2`. The branch is `work/W4-2`. |
| **goal** | One sentence: what is true when it is done. |
| **why** | What it does for a value being able to say why it is what it is, or for the safety of reading it. |
| **needs** | The ids that must be done first, or "nothing". |
| **size** | The smallest that can do it: small (mechanical), medium (a specified change with its tests) or
large (a design, an interface, anything subtle). A larger one may always take it. |
| **decided** | Every design choice already made: names, signatures, error wordings, limits, what was rejected and why. This is the part whoever carries the card out must not have to supply. |
| **files** | The files to create or change, and the shared ones to touch last. |
| **steps** | Numbered. Each is one action with its command. The test comes before the code, and "see it fail" is its own step. |
| **accept** | Which ACCEPT applies, plus any check particular to the card. |
| **stop if** | The conditions under which nothing is changed and the card is handed back. |
| **docs** | The exact documents to update: the D-number to add, the CHANGELOG heading, the Roadmap row. |
| **Organiser's answers** | Only when needed: a question someone carrying the card met and the organiser (or, for a question of step order or of a fixture against the card's own rule, whoever carries it) answered, dated. They are part of "decided". |
| **notes** | Empty until someone fails at it; then what failed, dated. |

A card that turns out wrong - its "decided" cannot be built as written -
is not repaired by whoever is carrying it out. That is a "stop if": the
unit is reverted, the obstacle is written under "notes", and the card
waits for someone able to redesign it. Following a wrong card
faithfully is the dearest mistake this plan can make.

## For the maintainer

Things only the maintainer can do or decide. Each unblocks the cards named.

* **Answered, 2026-11-12, for every card written so far**: where a card
  says TO DECIDE, the answer is the card's own recommendation. That
  covers H-5 (a group may write a secrets directory only when it is the
  private group of the user the directory belongs to), H-7 (an undefined key within "did you mean"
  distance of a sensitive key is kept as that secret), H-8a (the digest
  key comes only from the program; FNV-1a stays the default), W3-F2b
  (the scope it recommends) and W5-G2 (as it recommends). A card whose
  question is new, or whose recommendation cannot be built, still waits.
* **The order**, 2026-11-12: the library's own code first - what reads,
  bounds and explains a value - and the bindings after it, since a
  binding follows once the C is settled. One problem at a time, carried
  to done before the next is opened.
* **Asked, 2026-11-16**: three questions in the 1.7.0 cards, each with
  its options and a recommendation - S-1 (whether `cf_fmt`, the
  platform's name and its `_has_` queries, which the examples call,
  join the 1.x promise), S-4 (the serialization library's name) and
  T-3 (its program's name). Each waits for its answer.
* **1.7.0 released, 2026-11-20**: tagged by the maintainer on `dace0a5`;
  the release workflow published it with its five assets.
* **Decided without the maintainer, 2026-11-20, to be confirmed**: the
  three 1.7.0 items that had no card (one digest a purpose, trusted
  publishing, `doctor graph`) moved to 1.10.0 (`docs/Maintaining.md`,
  "Versions"), so that 1.7.0 is released with the tools its cards built.
  Trusted publishing needs each registry set up once by the maintainer.
* **Decided by the maintainer, 2026-11-20: the project serves real
  users.** tcc and pcc stay in verify's `compilers` step and in CI, but
  as information: their results are shown and a failure no longer stops
  a card (`scripts/verify.cmake`, `STEP_INFORMATIONAL`). GCC, Clang, MSVC
  and musl count. The compilers real users build with join as they are
  set up: bare-metal Arm (1.9.0), CompCert, SDCC; the paid embedded
  compilers (IAR, Keil, TI, Microchip) are wished for. W5-V2's tcc-only
  failure stays to be understood: undefined behaviour the sanitizers
  missed would be a real fault.
* 1.4.1 holds a changed default (response files are opt-in, D-79): it
  is released as 1.5.0 or as 1.4.1 - not decided.

## The index

In the order to be done. A card is `open`, `done` (with its commit), or
`waits` (with what it waits for). The W5-* cards belong to
the 1.7.0 milestone (`docs/Maintaining.md`, "Versions"; the finished
H, W3, S and T cards are recorded in "Done, their cards removed",
below). The L-* cards, the tools by
group (`docs/plan/L-tools.md`), come right after 1.7.0's release. The P-* cards, word-at-a-time
scanning before SIMD (`docs/plan/P-word-at-a-time.md`), are last,
except P-1: it only measures, and comes once 1.7.0 is released, so the
waves after it are measured from the start; P-2 and P-3 wait for those
waves ("After 1.7.0: the order of the waves", below).

| Order | Card | Size | Needs | Status |
| ----: | ---- | ----- | ----- | ------ |
| 33 | W5-V1 `configforge codec convert` and `format` | large | T-3 | done (26ab6bd) |
| 34 | W5-S1 `configforge gen schema --from` | medium | T-2 | done (0a8de0f) |
| 35 | W5-G1 `configforge gen --lang cpp` | large | W5-S1 | done (071ed40) |
| 36 | W5-G2 `configforge gen --lang rust` | large | W5-G1 | done (54f39fb) |
| 37 | W5-G3 `configforge gen --lang python` | medium | W5-G2 | done (950a099) |
| 38b | L-1 shellcheck, ruff, actionlint and codespell, as gates | medium | 1.7.0 released | done (7bec7d8) |
| 38c | L-2 Valgrind, AFL++, gitleaks, osv-scanner and Scorecard in CI | medium | L-1 | done (6442c8f) |
| 38d | L-4 include-what-you-use and a link check | small | L-1 | done (a87a174) |
| 38e | L-3 Infer and semgrep | large | L-2 | done (c2c14f4) |
| 38g | L-3b Frama-C EVA on port, numeric and base | large | L-3; 1.9.0 under way | open |
| 38f | L-5 Profilers: one command for time, cache, memory and vectorization | medium | P-1 | done (13e9e70) |
| 38h | L-6 A map of the tree and a brief per card, so that workers read less | medium | L-5 | done (00dd641) |
| 38i | L-7 CI in minutes, with nothing taken away | small | nothing | done (09fe647) |
| 38j | L-8 doctor.api cheap under MemorySanitizer, nothing dropped | small | L-7 | done (179c6df) |
| 39 | P-1 The benchmark measures large documents, and says where time goes | medium | 1.7.0 released | done (90b6541) |
| 40 | A-1 One directory per component: the files moved, nothing else changed | large | 1.7.0 released | done (824181f) |
| 41 | A-1b Each component a project: its declaration, its tests, its README, built alone | large | A-1 | done (031d54d) |
| 42 | A-2 The vector library, its interface and its scalar reference | medium | A-1b | done (da0dd49) |
| 43 | A-3 One UTF-8, one hex digit, one table hash | medium | A-2 | done (520baa2) |
| 44 | A-4 Another encoding, read only by name | medium | A-3 | done (e4069b8) |
| 45 | A-5 Bytes that are not UTF-8 are refused, with their place and a hint | medium | A-4 | done (98c068e) |
| 46 | A-6 `configforge codec recode`: a file converted once, for review | small | A-5, T-3 | done (0b4b97a) |
| 47 | A-7 The escapes in families | large | A-6 | done (fca866c) |
| 48 | A-8 Whitespace, comments, newlines and the place of a refusal | large | A-7 | done (c3bba84) |
| 49 | A-9 Composed codecs, for what is almost a format | medium | A-5 | done (a4ea9e1, made whole in the next commit) |
| 50 | A-10 A refusal's place and reason | medium | A-8, A-9 | done (f1a4fd7) |
| 51 | A-11 Build profiles and the C version per file | large | A-1b | done (b837c23) |
| 52 | A-12 The feature matrix made from the build | medium | A-11 | done (23b5d2a) |
| 53 | W5-V2 Every pair of formats, and what each can hold | large | A-1b, W5-V1 | done (70bd398) |
| 54 | W5-G4 Generators outside the tree | medium | A-1b, W5-G3 | done (adc2a63) |
| 55 | P-1b The other libraries beside ours, and a page CI keeps | large | P-1, A-1b | done (427ec6a) |
| 56 | A-13 A page of the project's limits | medium | A-12, W5-V2, P-1 | done (622d26b) |
| 56b | I-1 The 1.8.0 group's integration findings (docs/plan/I-integration.md) | medium | style-1.8.0 (done, 8fbe112) | done (cb9d8a3) |
| 57 | P-2 Word-at-a-time scans in C89, with the byte loop as reference | medium | P-1, A-3 | waits: 2.4.0; to be rewritten on D-101 |
| 58 | P-3 The scans P-1 found, a word at a time, kept only where they win | medium | P-2 | waits: P-2 |
| 59 | P-4 Allocation measured, and changed only where it wins | medium | P-1b | done (ca2f115) |
| 60 | N-1 Compare two flat configuration files by value | medium | W5-V1 | done (77a11ae) |
| 61 | N-2 Rebuild the Linux musl release archive and compare the bytes | medium | nothing | done (6c67353) |
| 62 | N-3 Show a tracked JSON file's value changes in Git difftool | small | N-1 | done (efb74f1) |
| 63 | N-4 Deliver one root table to a YAML document sink | small | W5-V2 | done (f6c0ba7) |
| 64 | N-5 Compare nested tables by value in `configforge diff` | small | N-1 | done (ad5b840) |
| 65 | N-6 `cf_usage`: the `--help` text from the contract | small | nothing | done (83f5c64) |
| 66 | N-7 A reload names the keys it changed | small | nothing | done (37ea469) |
| 67 | N-8 Compare scalar arrays by value in `configforge diff` | small | N-5 | done (7bbd943) |
| 68 | N-9 cppcheck runs on both libraries' own sources | small | nothing | done (ec0de1e) |
| 69 | N-10 cppcheck's style class on the libraries | small | N-9 | done (67a451a) |

## Done, their cards removed

A finished card is removed from the index and from its file; what it
did is kept here, and its text in git (`git log -- docs/plan/<file>`).

* **1.5.0**: H-1, H-3, H-4 and the whole of wave 4 (CBOR; TLV by a
  layout and BER-TLV).
* **H-hardening.md** (1.6.0; H-2 to H-8b; D-75, D-83 to D-86). A
  definition's constant key, default and bounds are checked by the
  compiler, in the C that `doctor gen` writes too (H-2, b9383d1,
  aa9436d, 490ee4f, 38c5a58); Doctor's `--redact-all` is on in the
  action (H-6, 9f3394b, 0b85a24, 9de9ef3; D-86); a directory others can
  write to is not read as secrets (H-5, f37813d, 4f42242, 88c8071;
  D-83); an undefined key named like a secret is kept as one (H-7,
  5dca003, 84bd14a; D-84); a secret's digest is keyed (H-8a, 0b082bf,
  d3094ff; D-85) and Doctor tells keys apart (H-8b, ed37f5f, 23018f7,
  cbe0050).
* **W3-native-sources.md** (1.6.0; W3-F2a to W3-F2d, W3-F3; D-80,
  D-87). `cf_builder_credentials` in C++, Rust and Python, the Guide
  and an example (W3-F3, 470cb8d, 059cdd9, 035ce3a, 71142a0); macOS's
  user defaults as an optional source over the flattener: the platform
  capability (W3-F2a, d3cbbf8, 5bb5967, 221b6ca), the source off macOS
  (W3-F2b, 9ac9540, 6aa4566, 928f71c), the read on macOS in CI (W3-F2c,
  a751ba0..529d84d) and the three languages (W3-F2d, 1ddb2ce, 146c605,
  e4435f0).
* **T-tools-and-serialization.md** (1.7.0; S-1 to S-5, T-1 to T-4;
  D-88 to D-90, D-99; T-5 in 1.8.0, fa6295a). What 1.x promises is
  smaller: the internal tier fenced (S-1, ee3d976, d60a0a7, e4abee8,
  d13fdd8; D-88) and an experimental tier (S-2, 8479c91, b096cc3,
  2acb279; D-89). The serialization library is a target of its own
  (S-3, dc54a27, f02e087, 3540ffd, bae204a; S-4, bbd2a88, 203cffa,
  84df09d, 1de8975; D-90), installed, found and packaged (S-5, 22772d8,
  563517c, ac8023e). Three programs behind `configforge <command>`
  (D-99): the front door (T-1, 94ab557, b7cbb7c, 297130f, 10929ab,
  f87885d), `configforge gen` (T-2, 9ad95d2, 01738ac, 555b855,
  9ee1c41), `configforge codec` (T-3, d8d06c1, da16549, 57f897f) and
  `codec dump` (T-4, 0074c04); T-5 removed Doctor's aliases and the
  name `doctor`.

Left over from those cards, now in `docs/Roadmap.md`: the user
defaults' other scopes (a scope prefix in `domain`); a second
amalgamation of the serialization library (with its own repository).

Not yet cards: the Roadmap's "More formats, and what a format is"
(MessagePack first, then BSON, UBJSON, property lists, HCL, Avro,
Protobuf, TLV as embedded devices write it, a benchmark, a stream);
the cumulative C99 and C23 layers (now D-104's layer for C and D-106, docs/plan/A-architecture.md); wave 6 (every operation on a file's data, written back
in its format), wave 7 (Go and .NET), wave 8 (2.0: the stream reader
and the dynamic sizing modes), proofs with CBMC, `.properties` against
Java's own reader. From the Roadmap's "From a review of the whole,
2026-11-14": a file written whole or not at all (first of wave 6, and
`examples/c/settings.c` moved onto it), an override file the library
owns (after it), a reload that names the keys it changed, the
comparison with other libraries (with the formats' benchmark), and one
program not written for the library moved onto it. From "From reading
the project as a newcomer, 2026-11-14", in this order and ahead of the
new formats: publishing to the registries (by trusted publishing,
set up once on each registry; see "Distribution, as infrastructure"), the
platform's file locations with profiles, misreads noticed, help and
completions from the contract, durations and sizes, big-endian, 32-bit
and bare-metal CI, value migrations, outside trust checks. From "Tools,
distribution and digests, 2026-11-14": distribution and being found (the WebAssembly
playground first); one digest a purpose, SHA-256 for a fingerprint that
decides, and a digest the program gives; a report asked of a running
program; the OpenTelemetry and OPA recipes; a "Start here" page a
language.

Each group's version is in the table in `docs/Maintaining.md`,
"Versions".

## After 1.7.0: the order of the waves

Decided by the maintainer on 2026-11-19, from the Roadmap's
"Critical-system standards, and large files" and "Suggestions of 2026-11-17".
None of these is a card yet. Only the next wave's cards are written,
when the wave before it ends, so the plan does not run ahead of the
code. The order, and why each comes where it does:

1. **Now: 1.7.0's cards**, as the index says. Two decisions are
   already in them: W5-G1 makes `gen`'s languages a table (its
   "decided"), and the codecs already are one (`cf_codecs[]` and
   `cf_formats[]` in `lib/serialize/src/codec.c`). After W5-V1: a test of
   every ordered pair of formats (W5-V2, to write). After W5-G3:
   generators outside the tree (W5-G4, to write).
2. **P-1, the measurement** (1.8.0). It changes only `bench/`. Its baseline,
   instruction counts included, shows what each wave below costs or
   saves.
3. **Architecture** (1.8.0; the design and the cards are
   `docs/plan/A-architecture.md`, D-100 to D-108). First a table of the helpers each reader writes
   again (UTF-8, escapes, numbers, whitespace, line and column), with
   line counts. Then those helpers as one internal layer behind an
   interface, with a scalar reference. Then the serialization sources'
   layout. Then build profiles by name (`CF_PROFILE`; not the
   platform's file locations "with profiles" above, which are a
   program's), the C version raised per file through one helper, the
   feature matrix made from the build, and `docs/Limits.md`. This comes
   first because MISRA and the optimizations touch the same files:
   moving code after them is moving it twice.
4. **Trust** (1.9.0), the "outside trust checks": cppcheck on the
   library itself, the `misra` profile, the public headers with no
   violation, the deviation records, `gate.misra`; then the CERT
   matrix, the CWE table and the known-weaknesses table from other
   libraries' CVEs, mutation testing, and the release built twice.
   Big-endian, 32-bit, Arm and bare-metal CI (each target with how it was
   shown: hardware, qemu, built only) run beside it, and are needed
   before any SIMD.
5. **Ways to read** (2.0.0, with the stream reader): the events made a
   public API (SAX), then in-situ strings, then lazy reading. These are
   API decisions, so they come before the optimizations that would bind
   them. Between trust and this, 1.10.0's newcomer items; after it, more
   formats (2.1.0), files written back (2.2.0) and more languages
   (2.3.0).
6. **Optimization** (2.4.0): P-2, P-3, then the SWAR and SIMD backend (build
   time and run-time detection, manual choice first, then automatic),
   with the benchmark against the other libraries and its live page.

Speed never at the cost of the `misra` and `minimal` profiles: the
benchmark measures those profiles against the other libraries too.

**Read again after the layout changes.** After A-1 and A-1b, and
again after the last A card, every open card and every open Roadmap
item that names a path or a function is read against the tree by
someone who did not write it, and corrected in one commit before the
next card opens. The first such reading, on 2026-11-19, found 39
problems in six cards; the tree's layout changing under the cards is
the surest way to make more.

Each wave's milestone is in `docs/Maintaining.md`, "Versions", laid
out again on 2026-11-19: versions go on after 2.0.0, which marks what
breaks, not the end of the plan.
