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
      and whose "needs" are all "done", and which `python
      scripts/check/ready.py <card id>` passes (nothing left to decide,
      every citation true; `brief.py` refuses a card that fails it).
      A "parked" card (the section
      "Waits on the maintainer") is never taken and never stops the
      loop: skip it, and skip a card that needs it. No such card ->
      STOP: report which cards wait and on what.
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
 9. git push -u origin work/<card id> (a push to work/* starts no CI; the
      maintainer, 2026-11-27: "anlamli degisiklik olsun sonra CI ile
      ugras"). ACCEPT's local runs are the card's check.
10. Only when the card is done - every step and ACCEPT green - squash it
      onto the batch branch `batch` (cut from main when none exists) as
      one commit:
        git switch batch; git merge --squash work/<card id>
        git commit    # subject "<card id>: <its goal>", body: the
                      # branch's commit subjects, in order
        python scripts/work/index.py --done <card id> <that commit>
        git commit -am "Plan: <card id> done (<that commit>)"
        git push origin batch           # no CI either
      Delete the branch here and on the remote. Now and then, `python
      scripts/work/index.py --archive` moves done cards to
      docs/plan/Done.md.
11. The batch goes to main once it holds about a dozen cards, when a
      module ends, before a release, or before the maintainer's stop
      time: `gh workflow run ci --ref batch -f tier=full` (ACCEPT-CI),
      one run for the whole batch. Red -> fix on `batch` with a commit
      "Batch: <what>", run again. Green -> `git push origin batch:main`
      (a fast-forward; main moved meanwhile -> merge origin/main into
      batch and run again), then delete `batch`. Go to 1.
```
```

A card is one commit on main; a batch of cards reaches main only as the tree CI passed. Work that is not
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

Before the squash, the whole suite once, by hand (`work.py land` does it
and refuses to squash on a failure; docs/Maintaining.md, "CI in two
tiers"). Pushes to the branch run only what their paths need:

```sh
gh workflow run ci --ref work/<card id> -f tier=full
gh run watch --exit-status $(gh run list --branch work/<card id> --workflow ci --event workflow_dispatch --limit 1 --json databaseId --jq '.[0].databaseId')
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
| **version** | `X.Y.Z`, the release the card belongs to (docs/plan/Layout.md), or `not now`. Right after **size**. |
| **status** | `open`, `parked`, `parked: <why>` or `done (<short hash>)`. "waits" is never written: `scripts/work/index.py` computes it from **needs**. Right after **version**. |
| **decided** | Every design choice already made: names, signatures, error wordings, limits, what was rejected and why. This is the part whoever carries the card out must not have to supply. |
| **files** | The files to create or change, and the shared ones to touch last. |
| **steps** | Numbered. Each is one action with its command. The test comes before the code, and "see it fail" is its own step. |
| **accept** | Which ACCEPT applies, plus any check particular to the card. |
| **stop if** | The conditions under which nothing is changed and the card is handed back. |
| **docs** | The exact documents to update: the D-number to add, the CHANGELOG heading, the Roadmap row. |
| **Organiser's answers** | Only when needed: a question someone carrying the card met and the organiser (or, for a question of step order or of a fixture against the card's own rule, whoever carries it) answered, dated. They are part of "decided". |
| **notes** | Empty until someone fails at it; then what failed, dated. |
| **cost** | Optional, last: `<model>, <effort>, <turns> turns, <percent of the 5-hour window>`, written by the organiser when the card lands. |

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

* **Answered, 2026-11-26** (relayed from the maintainer's side session): every TO DECIDE discussed that day is answered, each under its card's "Organiser's answers" (D-220 to D-229; the cards they answer follow them and are open: C-2a, F-2a, F-3j, K-6a, K-6b, X-21, Y-11, Y-12, Y-16, Y-17, Y-18, Y-24, Y-27, Y-28, Y-29, Y-36, 2026-11-27). Cards written from those answers (2026-11-27): K-9a, K-9d to K-9g, K-9b (`docs/plan/K-correctness.md`; K-9c is done), K-10c-m, Y-15a, Y-15b, Y-41-m, Y-42 (`docs/plan/Y-asked.md`), C-9-m, C-10c-m, C-10d-m (`docs/plan/C-change-2.md`).
* **The layout approved, 2026-11-26**: `docs/plan/Layout.md` (stage 0,
  then each `lib/` module: module, tool, bindings, documents; interim
  releases; the "not now" list) is the order of work. 2.0.0's breaks
  come early, right after 1.9.0.
* **The standing answer rule, 2026-11-26** (the maintainer): a question
  whose answer follows D-220, adds no external dependency and breaks no
  existing caller is answered by the card's own recommendation, without
  asking; whoever carries the card writes it under "Organiser's
  answers". Any other question still waits for the maintainer.
* **Standing approval, 2026-11-27** (the maintainer: "always make the
  best decision; I always approve"; "you are free to tag and publish a
  release; if you cannot, do not stop working"): a card's design question
  is answered by the organiser's best recommendation, written under the
  card's "Organiser's answers" with "(the maintainer's standing approval,
  2026-11-27)"; nothing waits for an answer. A milestone whose cards are
  done with CI green is tagged and released by the organiser. Still the
  maintainer's: the repository settings, a password or token, a decision
  of docs/plan/A-architecture.md reopened, a default of numbers changed.
* **2026-11-27** (the maintainer): the default behaviour of numbers
  does not change (K-10a, K-10b add options only); CI use is kept small
  (the repository earns nothing): L-22 runs a fast core on every push
  and the whole suite once a card.
* **Asked, 2026-11-26**: every card in "Waits on the maintainer" with
  "its TO DECIDE" waits for an answer written into the card (each
  TO DECIDE carries its options and a recommendation); the M cards wait
  for the maintainer's approval. A relayed message said every TO DECIDE
  is approved by its recommendation; it is not written in as an answer
  until the maintainer says so in their own words.

## The index

Generated by `python scripts/work/index.py` from the card files and
`docs/plan/Layout.md`; never edited by hand (`gate.index` fails when it
differs from what the script writes). The order is the order in which
Layout.md first names each card. A card is `open`, `parked`, `done` (with
its commit), or `waits` (with what it waits for: computed, never written). The finished cards (H, W3, S, T,
W5, L, A, I, N and P, but for the open cards) are recorded in "Done, their
cards removed", below; the cards are in `docs/plan/*.md`.
P-3 (word-at-a-time scanning before SIMD: `docs/plan/P-word-at-a-time.md`)
waits for 2.4.0: D-101 gives the choice of backend to the program then, and
D-102 moves the scans into `lib/text`.

**The order of work before Layout.md (2026-11-26, the maintainer; kept
for its words on the groups: the order now is Layout.md's).** Every open
item of docs/ has a card, in `docs/plan/*.md`, and a row below, in this
order of groups: C (the people who change configuration, `C-change.md`,
`C-change-2.md`), F (files and safety, `F-files.md`), K (correctness,
`K-correctness.md`), X (formats, `X-formats.md`: X-18 to X-21 first, by
the Roadmap), L (tools, `L-tools.md`, `L-tools-2.md`), G (documents,
`G-guides.md`), P (optimisation, `P-word-at-a-time.md`, `P-measured.md`),
Y (the maintainer's five ideas of 2026-11-26, `Y-asked.md`), then M (the
items that wait for the maintainer's word, `M-maintainer.md`). The loop
takes the first `open` card whose needs are done; a card whose status is
`waits: <id>` needs a card that is parked or not landed, and is passed
over until that one is done. A parked card (an unanswered TO DECIDE, an
id ending in `-m`, or an M card before the maintainer's approval) is
listed again in "Waits on the maintainer" with what it waits on.
Read cold already: C, F, K, L-11, L-tools-2, G and P; X, Y and M are read
cold just before their group's first card opens ("Read again after the
layout changes", below).

Where things stand (2026-11-26). Left to the maintainer, outside every card: the
repository settings (a ruleset on `main` with required reviews and checks,
signed commits and tags, two-factor authentication; the Roadmap's
"Repository settings"), which Scorecard's BranchProtection, CodeReview,
CIIBestPractices and Maintained findings wait on; publishing to PyPI and
crates.io (their tokens, or trusted publishing); confirming the struct
hack's five accepted EVA alarms in `.eva-accepted.txt` (`cf_block`'s
`data[1]`); whether to keep Dependabot's pull requests, which queue ahead
of a card's CI.

<!-- index begin (scripts/work/index.py) -->
| Order | Card | Version | Size | Needs | Status |
| ----: | ---- | ------- | ----- | ----- | ------ |
| 1 | [M-10](plan/M-maintainer.md#m-10) The dynamic sizing modes: the interface | 2.0.0 | medium | nothing | done (9d8cc70) |
| 2 | [K-6a](plan/K-correctness.md#k-6a) A refusal's byte offset | 2.0.0 | medium | nothing | done (d7212ea) |
| 3 | [K-6b](plan/K-correctness.md#k-6b) A refusal's reason shown by the tools | 2.0.0 | small | nothing | done (1ed49bc) |
| 4 | [K-9a](plan/K-correctness.md#k-9a) Three failure guarantees: written in every public function's header comment, listed in a table, and held by `gate.guarantees` | 2.0.0 | medium | nothing | done (da8f3b9) |
| 5 | [K-9d](plan/K-correctness.md#k-9d) Guarantees written for the lower components: base, port, platform, text and serialize | 2.0.0 | medium | [K-9a](plan/K-correctness.md#k-9a) | done (21a1a1f) |
| 6 | [K-9e](plan/K-correctness.md#k-9e) Guarantees written for the facade: `configforge.h` | 2.0.0 | medium | [K-9a](plan/K-correctness.md#k-9a) | done (52c76cc) |
| 7 | [K-9f](plan/K-correctness.md#k-9f) Guarantees written for sources and the key space: `cf_source.h`, `cf_keyspace.h`, `cf_entry.h`, `cf_iterate.h` | 2.0.0 | medium | [K-9a](plan/K-correctness.md#k-9a) | done (0ea8118) |
| 8 | [K-9g](plan/K-correctness.md#k-9g) Guarantees written for the rest of the configuration library, and the to-do list closed | 2.0.0 | medium | [K-9a](plan/K-correctness.md#k-9a), [K-9d](plan/K-correctness.md#k-9d), [K-9e](plan/K-correctness.md#k-9e), [K-9f](plan/K-correctness.md#k-9f) | done (c898df5) |
| 9 | [K-9b](plan/K-correctness.md#k-9b) Allocation faults injected into every load and write path, and what a failure leaves behind checked | 2.0.0 | large | [K-9a](plan/K-correctness.md#k-9a), [K-9d](plan/K-correctness.md#k-9d), [K-9e](plan/K-correctness.md#k-9e), [K-9f](plan/K-correctness.md#k-9f), [K-9g](plan/K-correctness.md#k-9g) | done (42dc309) |
| 10 | [K-9j](plan/K-correctness.md#k-9j) The two checks K-9b left: a builder given its source again, and a candidate added without its text | 2.0.0 | small | [K-9b](plan/K-correctness.md#k-9b) | done (78d7f39) |
| 11 | [M-11a](plan/M-maintainer.md#m-11a) Sizing in the readers and the key space | 2.0.0 | medium | [M-10](plan/M-maintainer.md#m-10) | done (a2eb91e) |
| 12 | [M-11b](plan/M-maintainer.md#m-11b) `cf_error` owns its text (the 2.0 break) | 2.0.0 | large | [M-11a](plan/M-maintainer.md#m-11a) | done (08e6470) |
| 13 | [M-12](plan/M-maintainer.md#m-12) The image header's layout, version 2 | 2.0.0 | medium | nothing | done (c818cba) |
| 14 | [M-13](plan/M-maintainer.md#m-13) 2.0, once: the list of what breaks, and the gate that holds it | 2.0.0 | small | [M-11b](plan/M-maintainer.md#m-11b), [M-12](plan/M-maintainer.md#m-12) | done (289121d) |
| 15 | [C-2a](plan/C-change.md#c-2a) Durations and sizes: the unit on an int definition | 2.1.0 | large | nothing | open |
| 16 | [C-2b](plan/C-change.md#c-2b) Durations and sizes: the schema, the report, `cf_emit` and every reader | 2.1.0 | medium | [C-2a](plan/C-change.md#c-2a) | waits: [C-2a](plan/C-change.md#c-2a) |
| 17 | [C-8a](plan/C-change-2.md#c-8a) A read the contract does not have, noticed: the library records it | 2.1.0 | medium | nothing | open |
| 18 | [C-9](plan/C-change-2.md#c-9) Values migrated, not only renamed: a function on the old key's definition | 2.1.0 | medium | nothing | open |
| 19 | [C-11](plan/C-change-2.md#c-11) A program's files where its platform keeps them: `cf_builder_app_files` | 2.1.0 | medium | nothing | open |
| 20 | [C-7](plan/C-change-2.md#c-7) Profiles: `--profile` or `NAME_PROFILE` adds `config.<profile>.<ext>` above the base file | 2.1.0 | medium | [C-11](plan/C-change-2.md#c-11) | waits: [C-11](plan/C-change-2.md#c-11) |
| 21 | [C-10a](plan/C-change-2.md#c-10a) A running program asked for its report: the library answers one request on a descriptor | 2.1.0 | medium | nothing | open |
| 22 | [F-1b](plan/F-files.md#f-1b) `cf_emit_file`: a configuration written to a file whole or not at all (the file layer's third part) | 2.1.0 | small | F-1a | open |
| 23 | [Y-42](plan/Y-asked.md#y-42) Include and import, in standard syntax: `"$include"` and `"$import"`, a root, a depth, a loader that plugs in (D-228) | 2.1.0 | large | nothing | open |
| 24 | [Y-9](plan/Y-asked.md#y-9) A second `cf_live` driver refused, and D-48's sentence checked | 2.1.0 | small | [Y-1](plan/Y-asked.md#y-1) | waits: [Y-1](plan/Y-asked.md#y-1) |
| 25 | [Y-10](plan/Y-asked.md#y-10) `cf_live_get` and `cf_config_free` under threads on the benchmark's page; `cf_live_generation` without the lock | 2.1.0 | medium | nothing | open |
| 26 | [Y-1](plan/Y-asked.md#y-1) The Guide's "At the same time": what is guarded and what is not | 2.1.0 | small | nothing | open |
| 27 | [C-1a](plan/C-change.md#c-1a) `configforge diff` over arrays of arrays | 2.1.0 | medium | nothing | open |
| 28 | [C-1b](plan/C-change.md#c-1b) `configforge diff --schema`: units compared as values | 2.1.0 | medium | [C-2a](plan/C-change.md#c-2a), [C-2b](plan/C-change.md#c-2b), [C-1a](plan/C-change.md#c-1a) | waits: [C-2a](plan/C-change.md#c-2a) |
| 29 | [C-3a](plan/C-change.md#c-3a) `configforge diff --schema` flags the keys out of range and the keys that touch a secret | 2.1.0 | medium | [C-1b](plan/C-change.md#c-1b) | waits: [C-1b](plan/C-change.md#c-1b) |
| 30 | [C-3b](plan/C-change.md#c-3b) The pull request's comment says which keys are out of range and which touch a secret | 2.1.0 | small | [C-3a](plan/C-change.md#c-3a), N-15 | waits: [C-3a](plan/C-change.md#c-3a) |
| 31 | [C-4a](plan/C-change.md#c-4a) `doctor drift`: the source of a value, and a list file | 2.1.0 | medium | nothing | open |
| 32 | [C-4b](plan/C-change.md#c-4b) Drift across N configuration files: `configforge diff --all` | 2.1.0 | medium | [C-1a](plan/C-change.md#c-1a) | waits: [C-1a](plan/C-change.md#c-1a) |
| 33 | [C-6a](plan/C-change.md#c-6a) Shell completions from the contract: `configforge gen --lang bash\|zsh\|fish\|powershell` | 2.1.0 | medium | nothing | open |
| 34 | [C-6b](plan/C-change.md#c-6b) A man page and a sample file from the contract: `configforge gen --lang man\|sample-toml\|sample-yaml` | 2.1.0 | medium | [C-6a](plan/C-change.md#c-6a) | waits: [C-6a](plan/C-change.md#c-6a) |
| 35 | [C-8b](plan/C-change-2.md#c-8b) Doctor's CFD021: a read of a key the contract does not have, in `doctor lint` | 2.1.0 | small | [C-8a](plan/C-change-2.md#c-8a) | waits: [C-8a](plan/C-change-2.md#c-8a) |
| 36 | [C-10b](plan/C-change-2.md#c-10b) `configforge doctor pull unix:<path>`, and an example that serves from a loop | 2.1.0 | medium | [C-10a](plan/C-change-2.md#c-10a) | waits: [C-10a](plan/C-change-2.md#c-10a) |
| 37 | [Y-34](plan/Y-asked.md#y-34) `gen check --report`: a predicted, value-blind report from files and a contract | 2.1.0 | medium | nothing | open |
| 38 | [Y-35](plan/Y-asked.md#y-35) Doctor on a predicted report: the rules a running program is needed for are off and named; SARIF columns and fingerprints | 2.1.0 | small | [Y-34](plan/Y-asked.md#y-34) | waits: [Y-34](plan/Y-asked.md#y-34) |
| 39 | [Y-37](plan/Y-asked.md#y-37) A secret written in a checked file: CFD022 | 2.1.0 | small | [Y-35](plan/Y-asked.md#y-35), [C-8b](plan/C-change-2.md#c-8b), [C-8b](plan/C-change-2.md#c-8b) | waits: [Y-35](plan/Y-asked.md#y-35) |
| 40 | [Y-38](plan/Y-asked.md#y-38) Defined but never read, over a set of reports: CFD023 | 2.1.0 | small | [C-8a](plan/C-change-2.md#c-8a), [C-8b](plan/C-change-2.md#c-8b) | waits: [C-8a](plan/C-change-2.md#c-8a) |
| 41 | [F-4a](plan/F-files.md#f-4a) The references drawn, first by jq | 2.1.0 | small | nothing | open |
| 42 | [C-4c](plan/C-change.md#c-4c) `doctor drift` from Python | 2.1.0 | small | [C-4a](plan/C-change.md#c-4a) | waits: [C-4a](plan/C-change.md#c-4a) |
| 43 | [Y-7](plan/Y-asked.md#y-7) "Threads and processes" in the Guide: four recipes, compiled in CI, a `fork` test | 2.1.0 | medium | [Y-1](plan/Y-asked.md#y-1), Y-8 | waits: [Y-1](plan/Y-asked.md#y-1) |
| 44 | [Y-36](plan/Y-asked.md#y-36) Recipes: the contract's schema in yaml-language-server and Taplo, conftest over a report with places | 2.1.0 | small | [Y-34](plan/Y-asked.md#y-34) | waits: [Y-34](plan/Y-asked.md#y-34) |
| 45 | [G-1a](plan/G-guides.md#g-1a) The tools that read a decision follow the split | 2.1.0 | medium | nothing | open |
| 46 | [G-1b](plan/G-guides.md#g-1b) Decisions.md split into one file a decision | 2.1.0 | medium | [G-1a](plan/G-guides.md#g-1a) | waits: [G-1a](plan/G-guides.md#g-1a) |
| 47 | [G-2](plan/G-guides.md#g-2) The Roadmap's finished items on a page of their own | 2.1.0 | medium | [G-1b](plan/G-guides.md#g-1b), [G-2](plan/G-guides.md#g-2) | waits: [G-1b](plan/G-guides.md#g-1b) |
| 48 | [C-9-m](plan/C-change-2.md#c-9-m) One key split into two: `migrate_from` on the new key's definition | 2.1.0 | medium | [C-9](plan/C-change-2.md#c-9) | waits: [C-9](plan/C-change-2.md#c-9) |
| 49 | [C-10c-m](plan/C-change-2.md#c-10c-m) The report's channel on Windows: an `AF_UNIX` socket behind the platform's descriptor functions | 2.1.0 | medium | [C-10a](plan/C-change-2.md#c-10a), [C-10b](plan/C-change-2.md#c-10b) | waits: [C-10a](plan/C-change-2.md#c-10a) |
| 50 | [C-10d-m](plan/C-change-2.md#c-10d-m) `configforge doctor pull` reads `http://` and `tcp:` targets, with a timeout and a size as options | 2.1.0 | medium | [C-10b](plan/C-change-2.md#c-10b) | waits: [C-10b](plan/C-change-2.md#c-10b) |
| 51 | [F-4b](plan/F-files.md#f-4b) `configforge doctor graph`, built only for a want F-4a's filters do not meet | 2.1.0 | medium | [F-4a](plan/F-files.md#f-4a) | waits: [F-4a](plan/F-files.md#f-4a) |
| 52 | [Y-41-m](plan/Y-asked.md#y-41-m) Simple expressions: `"${= 8000 + 80}"`, total, inside a string | 2.1.0 | medium | [Y-42](plan/Y-asked.md#y-42) | waits: [Y-42](plan/Y-asked.md#y-42) |
| 53 | [Y-29](plan/Y-asked.md#y-29) The manifest: inputs and their digests, the contract's digest, version, profile, open keys, the output's digest | 2.2.0 | small | [Y-24](plan/Y-asked.md#y-24), [Y-25](plan/Y-asked.md#y-25), [M-34](plan/M-maintainer.md#m-34) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 54 | [M-32](plan/M-maintainer.md#m-32) One digest a purpose: SHA-256 as the fingerprint's option | 2.2.0 | large | nothing | open |
| 55 | [M-33](plan/M-maintainer.md#m-33) A digest function the program gives | 2.2.0 | medium | [M-32](plan/M-maintainer.md#m-32) | waits: [M-32](plan/M-maintainer.md#m-32) |
| 56 | [M-34](plan/M-maintainer.md#m-34) A gate built on a SHA-256 fingerprint, in the Guide, with its test | 2.2.0 | small | [M-32](plan/M-maintainer.md#m-32), [M-33](plan/M-maintainer.md#m-33) | waits: [M-32](plan/M-maintainer.md#m-32) |
| 57 | [Y-24](plan/Y-asked.md#y-24) `configforge gen build`: sources by rank, a contract, a target, deterministic output | 2.2.0 | medium | [Y-34](plan/Y-asked.md#y-34) | waits: [Y-34](plan/Y-asked.md#y-34) |
| 58 | [Y-25](plan/Y-asked.md#y-25) The open-key checks: what the compiler can prove about the keys left to run time | 2.2.0 | small | [Y-24](plan/Y-asked.md#y-24) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 59 | [Y-26](plan/Y-asked.md#y-26) `CF_IMAGE_CONFIGURED`: an image that holds only what a source configured, and defaults and secrets out unless asked | 2.2.0 | small | [Y-24](plan/Y-asked.md#y-24) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 60 | [Y-27](plan/Y-asked.md#y-27) The generated C: `gen build --target c` (plain typed code) and `--target c-baked` (the baked tables) | 2.2.0 | large | [Y-24](plan/Y-asked.md#y-24), [Y-26](plan/Y-asked.md#y-26) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 61 | [Y-28](plan/Y-asked.md#y-28) `cf_builder_errors`: every diagnostic with its place, and `gen build --json` and `gen check --all` | 2.2.0 | medium | [Y-24](plan/Y-asked.md#y-24), K-9c | waits: [Y-24](plan/Y-asked.md#y-24) |
| 62 | [Y-33](plan/Y-asked.md#y-33) The manifest inside the image (2.2.0, in the header M-12 reserves for it) | 2.2.0 | small | [Y-29](plan/Y-asked.md#y-29), [M-12](plan/M-maintainer.md#m-12) | waits: [Y-29](plan/Y-asked.md#y-29) |
| 63 | [Y-31](plan/Y-asked.md#y-31) Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel and KCL as front ends | 2.2.0 | small | [Y-24](plan/Y-asked.md#y-24) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 64 | [Y-30-m](plan/Y-asked.md#y-30-m) Rules across keys in the contract: a fixed declarative set | 2.2.0 | medium | [Y-24](plan/Y-asked.md#y-24) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 65 | [Y-32-m](plan/Y-asked.md#y-32-m) A build file read with the tool's own contract | 2.2.0 | medium | [Y-24](plan/Y-asked.md#y-24), [Y-29](plan/Y-asked.md#y-29) | waits: [Y-24](plan/Y-asked.md#y-24) |
| 66 | [Y-39-m](plan/Y-asked.md#y-39-m) Relations in the contract: `pattern`, beside Y-30-m's `if`/`then` and `dependentRequired` | 2.3.0 | large | [Y-30-m](plan/Y-asked.md#y-30-m), [Y-36](plan/Y-asked.md#y-36) | waits: [Y-30-m](plan/Y-asked.md#y-30-m) |
| 67 | [Y-40-m](plan/Y-asked.md#y-40-m) The editor's hover: the report's origins through the schema, with no server of ours | 2.3.0 | medium | [Y-34](plan/Y-asked.md#y-34), [Y-36](plan/Y-asked.md#y-36) | waits: [Y-34](plan/Y-asked.md#y-34) |
| 68 | [X-18](plan/X-formats.md#x-18) The helpers each reader writes again: a table, kept true by a gate | 2.2.0 | medium | nothing | open |
| 69 | [X-19](plan/X-formats.md#x-19) The printers apart: one file a format's writer | 2.2.0 | medium | [X-18](plan/X-formats.md#x-18) | waits: [X-18](plan/X-formats.md#x-18) |
| 70 | [X-20](plan/X-formats.md#x-20) What repeats, once: the helpers X-18's table says move | 2.2.0 | large | [X-19](plan/X-formats.md#x-19) | waits: [X-19](plan/X-formats.md#x-19) |
| 71 | [X-21](plan/X-formats.md#x-21) Text and binary formats as two groups a build can take apart | 2.2.0 | medium | [X-19](plan/X-formats.md#x-19) | waits: [X-19](plan/X-formats.md#x-19) |
| 72 | [K-7a](plan/K-correctness.md#k-7a) One table of formats: the lists that copy it read it | 2.2.0 | medium | nothing | open |
| 73 | [K-5a](plan/K-correctness.md#k-5a) TOML's joined path judged as one number | 2.2.0 | medium | nothing | open |
| 74 | [K-5b](plan/K-correctness.md#k-5b) INI's reader limits nesting as the others do | 2.2.0 | small | [K-5a](plan/K-correctness.md#k-5a) | waits: [K-5a](plan/K-correctness.md#k-5a) |
| 75 | [K-8](plan/K-correctness.md#k-8) The YAML reader's three differences from libyaml, fixed to YAML 1.2's text | 2.2.0 | medium | nothing | open |
| 76 | [K-8b](plan/K-correctness.md#k-8b) An anchor whose name holds `?`, alone at the top: the built-in parser and libyaml differ | 2.2.0 | small | nothing | open |
| 77 | [K-1a](plan/K-correctness.md#k-1a) `.properties` read by Java's own `Properties`, and the two compared | 2.2.0 | medium | nothing | open |
| 78 | [K-1b](plan/K-correctness.md#k-1b) What the comparison with Java found, fixed | 2.2.0 | medium | [K-1a](plan/K-correctness.md#k-1a) | waits: [K-1a](plan/K-correctness.md#k-1a) |
| 79 | [K-10a](plan/K-correctness.md#k-10a) A number's text, kept when the program asks | 2.2.0 | large | nothing | open |
| 80 | [K-10b](plan/K-correctness.md#k-10b) Underflow and infinity as options, the defaults as today | 2.2.0 | medium | [K-10a](plan/K-correctness.md#k-10a) | waits: [K-10a](plan/K-correctness.md#k-10a) |
| 81 | [K-10c-m](plan/K-correctness.md#k-10c-m) Numbers beyond 64 bits or `double`, accepted as text under an option | 2.2.0 | large | [K-10a](plan/K-correctness.md#k-10a), [K-10b](plan/K-correctness.md#k-10b) | waits: [K-10a](plan/K-correctness.md#k-10a) |
| 82 | [K-2a](plan/K-correctness.md#k-2a) The other libraries' known weaknesses, searched and counted, as a table | 2.2.0 | medium | nothing | open |
| 83 | [K-2b](plan/K-correctness.md#k-2b) `gate.weaknesses`: each row names a test that exists | 2.2.0 | medium | [K-2a](plan/K-correctness.md#k-2a) | waits: [K-2a](plan/K-correctness.md#k-2a) |
| 84 | [K-2c](plan/K-correctness.md#k-2c) The weaknesses of expansion, nesting, lengths and time, as tests | 2.2.0 | medium | [K-2b](plan/K-correctness.md#k-2b) | waits: [K-2b](plan/K-correctness.md#k-2b) |
| 85 | [K-2d](plan/K-correctness.md#k-2d) The weaknesses of truncation, NUL, UTF-8, numbers and duplicates, as tests | 2.2.0 | medium | [K-2b](plan/K-correctness.md#k-2b) | waits: [K-2b](plan/K-correctness.md#k-2b) |
| 86 | [K-2e](plan/K-correctness.md#k-2e) The last rows, the table closed, the Roadmap item done | 2.2.0 | small | [K-2c](plan/K-correctness.md#k-2c), [K-2d](plan/K-correctness.md#k-2d) | waits: [K-2c](plan/K-correctness.md#k-2c) |
| 87 | [K-3a](plan/K-correctness.md#k-3a) The blob's file reader proved up to a bound | 2.2.0 | large | nothing | open |
| 88 | [K-3b](plan/K-correctness.md#k-3b) The INI reader proved up to a bound | 2.2.0 | large | [K-3a](plan/K-correctness.md#k-3a) | waits: [K-3a](plan/K-correctness.md#k-3a) |
| 89 | [K-4a](plan/K-correctness.md#k-4a) One harness for differential fuzzing, and JSON against yyjson | 2.2.0 | large | P-1b | open |
| 90 | [K-4b](plan/K-correctness.md#k-4b) TOML against tomlc17 | 2.2.0 | medium | [K-4a](plan/K-correctness.md#k-4a) | waits: [K-4a](plan/K-correctness.md#k-4a) |
| 91 | [K-4c](plan/K-correctness.md#k-4c) CBOR against tinycbor | 2.2.0 | medium | [K-4a](plan/K-correctness.md#k-4a) | waits: [K-4a](plan/K-correctness.md#k-4a) |
| 92 | [K-4d](plan/K-correctness.md#k-4d) XML against a second parser | 2.2.0 | medium | [K-4a](plan/K-correctness.md#k-4a) | waits: [K-4a](plan/K-correctness.md#k-4a) |
| 93 | [L-14a](plan/L-tools-2.md#l-14a) EVA over the float readers: `cf_text_to_f64` and the bit conversions | 2.2.0 | large | L-3b | open |
| 94 | [L-14b](plan/L-tools-2.md#l-14b) EVA over the float printer: `cf_gen_digits` and `cf_f64_to_text` | 2.2.0 | large | [L-14a](plan/L-tools-2.md#l-14a) | waits: [L-14a](plan/L-tools-2.md#l-14a) |
| 95 | [Y-15a](plan/Y-asked.md#y-15a) `lib/compress`: the two interfaces `cf_packer` and `cf_unpacker`, their options with the method's own defaults, the one-call `cf_pack` and `cf_unpack`, and a test backend | 2.3.0 | large | [Y-14](plan/Y-asked.md#y-14), [K-9b](plan/K-correctness.md#k-9b) | waits: [Y-14](plan/Y-asked.md#y-14) |
| 96 | [Y-15b](plan/Y-asked.md#y-15b) Streaming `cf_unpacker` and `cf_packer`, and `cf_unpack_chain` | 2.3.0 | large | [Y-15a](plan/Y-asked.md#y-15a) | waits: [Y-15a](plan/Y-asked.md#y-15a) |
| 97 | [Y-16](plan/Y-asked.md#y-16) gzip through zlib: CF_WITH_ZLIB | 2.3.0 | medium | [Y-15a](plan/Y-asked.md#y-15a) | waits: [Y-15a](plan/Y-asked.md#y-15a) |
| 98 | [Y-17](plan/Y-asked.md#y-17) zstd through libzstd: CF_WITH_ZSTD | 2.3.0 | medium | [Y-16](plan/Y-asked.md#y-16) | waits: [Y-16](plan/Y-asked.md#y-16) |
| 99 | [Y-18](plan/Y-asked.md#y-18) The sources read through the layer: flags, extensions, provenance, bindings | 2.3.0 | medium | [Y-15a](plan/Y-asked.md#y-15a), [Y-16](plan/Y-asked.md#y-16), [Y-17](plan/Y-asked.md#y-17) | waits: [Y-15a](plan/Y-asked.md#y-15a) |
| 100 | [Y-21-m](plan/Y-asked.md#y-21-m) A built-in inflate for the embedded presets | 2.3.0 | large | [Y-16](plan/Y-asked.md#y-16), [Y-20](plan/Y-asked.md#y-20) | waits: [Y-16](plan/Y-asked.md#y-16) |
| 101 | [Y-22-m](plan/Y-asked.md#y-22-m) xz and lz4 frames | 2.3.0 | medium | [Y-17](plan/Y-asked.md#y-17) | waits: [Y-17](plan/Y-asked.md#y-17) |
| 102 | [Y-23-m](plan/Y-asked.md#y-23-m) zstd dictionaries on a source | 2.3.0 | medium | [Y-18](plan/Y-asked.md#y-18), [Y-20](plan/Y-asked.md#y-20) | waits: [Y-18](plan/Y-asked.md#y-18) |
| 103 | [Y-19](plan/Y-asked.md#y-19) `configforge codec` through the layer, and a bomb corpus | 2.3.0 | small | [Y-18](plan/Y-asked.md#y-18) | waits: [Y-18](plan/Y-asked.md#y-18) |
| 104 | [Y-14](plan/Y-asked.md#y-14) Compressed input by the program's own zlib: the D-103 recipe, a Guide section and an example | 2.3.0 | small | nothing | open |
| 105 | [F-3a](plan/F-files.md#f-3a) Wave 6, the core: a change spliced into a document and checked by reading it back | 2.3.0 | large | F-1a, [F-3i](plan/F-files.md#f-3i), [F-3j](plan/F-files.md#f-3j) | waits: [F-3i](plan/F-files.md#f-3i) |
| 106 | [F-3b](plan/F-files.md#f-3b) Wave 6, JSON and JSON5: a value changed, a member added or removed | 2.3.0 | medium | [F-3a](plan/F-files.md#f-3a) | waits: [F-3a](plan/F-files.md#f-3a) |
| 107 | [F-3c](plan/F-files.md#f-3c) Wave 6, INI and .properties: a value changed, a line added or removed | 2.3.0 | medium | [F-3a](plan/F-files.md#f-3a) | waits: [F-3a](plan/F-files.md#f-3a) |
| 108 | [F-3d](plan/F-files.md#f-3d) Wave 6, TOML: a value changed, a key removed | 2.3.0 | medium | [F-3a](plan/F-files.md#f-3a) | waits: [F-3a](plan/F-files.md#f-3a) |
| 109 | [F-3e](plan/F-files.md#f-3e) Wave 6, TOML: a key added | 2.3.0 | medium | [F-3d](plan/F-files.md#f-3d) | waits: [F-3d](plan/F-files.md#f-3d) |
| 110 | [F-3f](plan/F-files.md#f-3f) Wave 6, YAML: a value changed, a key removed | 2.3.0 | medium | [F-3a](plan/F-files.md#f-3a) | waits: [F-3a](plan/F-files.md#f-3a) |
| 111 | [F-3g](plan/F-files.md#f-3g) Wave 6, YAML: a key added | 2.3.0 | medium | [F-3f](plan/F-files.md#f-3f) | waits: [F-3f](plan/F-files.md#f-3f) |
| 112 | [F-3h](plan/F-files.md#f-3h) Wave 6, TLV: a value changed, an entry added or removed, the bytes a layout skips kept | 2.3.0 | large | [F-3a](plan/F-files.md#f-3a) | waits: [F-3a](plan/F-files.md#f-3a) |
| 113 | [F-3j](plan/F-files.md#f-3j) Wave 6, the document edit as a public interface | 2.3.0 | medium | [F-3i](plan/F-files.md#f-3i), [F-1b](plan/F-files.md#f-1b) | waits: [F-3i](plan/F-files.md#f-3i) |
| 114 | [Y-20](plan/Y-asked.md#y-20) Writing: cf_pack, deterministic gzip and zstd, `convert --compress`, a compressed image | 2.3.0 | medium | [Y-16](plan/Y-asked.md#y-16), [Y-17](plan/Y-asked.md#y-17), [Y-19](plan/Y-asked.md#y-19), [F-1b](plan/F-files.md#f-1b) | waits: [Y-16](plan/Y-asked.md#y-16) |
| 115 | [F-2a](plan/F-files.md#f-2a) An override file the library owns: a file source that cf_write can commit to | 2.3.0 | large | F-1a, Y-2, [Y-42](plan/Y-asked.md#y-42) | waits: [Y-42](plan/Y-asked.md#y-42) |
| 116 | [F-2b](plan/F-files.md#f-2b) settings.c is the override source's example | 2.3.0 | small | [F-2a](plan/F-files.md#f-2a), [F-1b](plan/F-files.md#f-1b), [F-1b](plan/F-files.md#f-1b) | waits: [F-2a](plan/F-files.md#f-2a) |
| 117 | [Y-4](plan/Y-asked.md#y-4) `cf_file_source_options.lock`, `.wait_ms` and `.write_attempts`, `.read_attempts`, `cf_builder_write_wait`: the lock around `overrides_write`, and the store's wait (F-5b in the research) | 2.3.0 | medium | Y-3, [F-2a](plan/F-files.md#f-2a) | waits: [F-2a](plan/F-files.md#f-2a) |
| 118 | [Y-12](plan/Y-asked.md#y-12) `swapped` in `cf_live_options`: workers told of a swap | 2.3.0 | small | [Y-9](plan/Y-asked.md#y-9) | waits: [Y-9](plan/Y-asked.md#y-9) |
| 119 | [F-3i](plan/F-files.md#f-3i) Wave 6, `configforge codec get`, `set` and `unset` | 2.3.0 | medium | F-1a, [F-3b](plan/F-files.md#f-3b), [F-3c](plan/F-files.md#f-3c), [F-3d](plan/F-files.md#f-3d), [F-3e](plan/F-files.md#f-3e), [F-3f](plan/F-files.md#f-3f), [F-3g](plan/F-files.md#f-3g), [F-3h](plan/F-files.md#f-3h) | waits: [F-3b](plan/F-files.md#f-3b) |
| 120 | [M-5](plan/M-maintainer.md#m-5) The stream: one interface for reading in pieces, with a fallback every codec has | 2.4.0 | large | nothing | open |
| 121 | [M-6](plan/M-maintainer.md#m-6) The stream, native: CBOR and TLV (the length-prefixed formats first) | 2.4.0 | medium | [M-5](plan/M-maintainer.md#m-5) | waits: [M-5](plan/M-maintainer.md#m-5) |
| 122 | [M-7](plan/M-maintainer.md#m-7) The stream, native: JSON, JSON5 and `.properties` | 2.4.0 | large | [M-5](plan/M-maintainer.md#m-5), [M-6](plan/M-maintainer.md#m-6) | waits: [M-5](plan/M-maintainer.md#m-5) |
| 123 | [M-8](plan/M-maintainer.md#m-8) The stream, native: INI and TOML (the readers that look back) | 2.4.0 | large | [M-5](plan/M-maintainer.md#m-5), [M-7](plan/M-maintainer.md#m-7) | waits: [M-5](plan/M-maintainer.md#m-5) |
| 124 | [M-9](plan/M-maintainer.md#m-9) The stream, native: YAML, and the file source that uses it | 2.4.0 | large | [M-5](plan/M-maintainer.md#m-5), [M-7](plan/M-maintainer.md#m-7), [M-8](plan/M-maintainer.md#m-8) | waits: [M-5](plan/M-maintainer.md#m-5) |
| 125 | [M-14](plan/M-maintainer.md#m-14) Ways to read: the document tree (DOM), a public type of the serialization library | 2.4.0 | large | nothing | open |
| 126 | [M-15](plan/M-maintainer.md#m-15) Ways to read: lazy, a structural index first | 2.4.0 | large | [M-14](plan/M-maintainer.md#m-14) | waits: [M-14](plan/M-maintainer.md#m-14) |
| 127 | [M-16](plan/M-maintainer.md#m-16) Ways to read: in-situ strings | 2.4.0 | medium | nothing | open |
| 128 | [K-7b](plan/K-correctness.md#k-7b) The ways a format can be read, in its table entry | 2.4.0 | small | [K-7a](plan/K-correctness.md#k-7a), [M-14](plan/M-maintainer.md#m-14), [M-15](plan/M-maintainer.md#m-15), [M-16](plan/M-maintainer.md#m-16) | waits: [K-7a](plan/K-correctness.md#k-7a) |
| 129 | [M-17](plan/M-maintainer.md#m-17) Ways to read: the table, and each mode measured | 2.4.0 | medium | [M-9](plan/M-maintainer.md#m-9), [M-14](plan/M-maintainer.md#m-14), [M-15](plan/M-maintainer.md#m-15), [M-16](plan/M-maintainer.md#m-16), [K-7b](plan/K-correctness.md#k-7b) | waits: [M-9](plan/M-maintainer.md#m-9) |
| 130 | [Y-13-m](plan/Y-asked.md#y-13-m) A shared image across processes: memfd and seals, `shm_open`, a read-only section, the source | 2.4.0 | large | [Y-7](plan/Y-asked.md#y-7), Y-8 | waits: [Y-7](plan/Y-asked.md#y-7) |
| 131 | [X-1](plan/X-formats.md#x-1) MessagePack: reader, writer, vectors, fuzz target | 2.5.0 | large | nothing | open |
| 132 | [X-2](plan/X-formats.md#x-2) BSON | 2.5.0 | large | [X-1](plan/X-formats.md#x-1) | waits: [X-1](plan/X-formats.md#x-1) |
| 133 | [X-3](plan/X-formats.md#x-3) UBJSON | 2.5.0 | medium | [X-2](plan/X-formats.md#x-2), [X-2](plan/X-formats.md#x-2) | waits: [X-2](plan/X-formats.md#x-2) |
| 134 | [X-4](plan/X-formats.md#x-4) Smile | 2.5.0 | large | [X-3](plan/X-formats.md#x-3) | waits: [X-3](plan/X-formats.md#x-3) |
| 135 | [X-5](plan/X-formats.md#x-5) Amazon Ion (binary): reader, read only | 2.5.0 | large | [X-4](plan/X-formats.md#x-4) | waits: [X-4](plan/X-formats.md#x-4) |
| 136 | [X-6](plan/X-formats.md#x-6) Bencode, read only | 2.5.0 | medium | [X-5](plan/X-formats.md#x-5) | waits: [X-5](plan/X-formats.md#x-5) |
| 137 | [X-7](plan/X-formats.md#x-7) Property lists, the binary form (bplist00) | 2.5.0 | large | [X-6](plan/X-formats.md#x-6), [X-1](plan/X-formats.md#x-1) | waits: [X-6](plan/X-formats.md#x-6) |
| 138 | [X-8](plan/X-formats.md#x-8) Property lists, the XML form, with a built-in scanner | 2.5.0 | large | [X-7](plan/X-formats.md#x-7) | waits: [X-7](plan/X-formats.md#x-7) |
| 139 | [X-9](plan/X-formats.md#x-9) HCL (HashiCorp Configuration Language 2), the literal subset | 2.5.0 | large | [X-8](plan/X-formats.md#x-8) | waits: [X-8](plan/X-formats.md#x-8) |
| 140 | [X-10](plan/X-formats.md#x-10) HOCON, without substitutions and includes | 2.5.0 | large | [X-9](plan/X-formats.md#x-9) | waits: [X-9](plan/X-formats.md#x-9) |
| 141 | [X-11](plan/X-formats.md#x-11) KDL (version 2) | 2.5.0 | large | [X-10](plan/X-formats.md#x-10) | waits: [X-10](plan/X-formats.md#x-10) |
| 142 | [X-12](plan/X-formats.md#x-12) TLV as the standards have it: DER, SIMPLE-TLV, EMV and KLV | 2.5.0 | large | nothing | open |
| 143 | [X-13](plan/X-formats.md#x-13) TLV as devices write it: field order, widths, an implied length, a packed head | 2.5.0 | large | [X-12](plan/X-formats.md#x-12) | waits: [X-12](plan/X-formats.md#x-12) |
| 144 | [X-14](plan/X-formats.md#x-14) Formats bound to a schema: Avro's container, and what the rest waits on | 2.5.0 | large | [X-13](plan/X-formats.md#x-13) | waits: [X-13](plan/X-formats.md#x-13) |
| 145 | [X-15](plan/X-formats.md#x-15) The benchmark learns the binary formats: MiB/s for each codec | 2.5.0 | medium | [X-1](plan/X-formats.md#x-1), [X-7](plan/X-formats.md#x-7) | waits: [X-1](plan/X-formats.md#x-1) |
| 146 | [X-16](plan/X-formats.md#x-16) The benchmark learns the text formats and the read-only ones | 2.5.0 | medium | [X-15](plan/X-formats.md#x-15), [X-8](plan/X-formats.md#x-8), [X-11](plan/X-formats.md#x-11), [X-5](plan/X-formats.md#x-5), [X-14](plan/X-formats.md#x-14) | waits: [X-15](plan/X-formats.md#x-15) |
| 147 | [X-17](plan/X-formats.md#x-17) Text handed to the sink where it lies | 2.5.0 | large | [X-16](plan/X-formats.md#x-16), P-1 | waits: [X-16](plan/X-formats.md#x-16) |
| 148 | [M-1](plan/M-maintainer.md#m-1) Go (cgo): the module over the C library | 2.6.0 | large | nothing | open |
| 149 | [M-2](plan/M-maintainer.md#m-2) Go: an example and a CI job | 2.6.0 | medium | [M-1](plan/M-maintainer.md#m-1) | waits: [M-1](plan/M-maintainer.md#m-1) |
| 150 | [M-3](plan/M-maintainer.md#m-3) .NET (P/Invoke): the assembly over the C library | 2.6.0 | large | nothing | open |
| 151 | [M-4](plan/M-maintainer.md#m-4) .NET: an example and a CI job | 2.6.0 | medium | [M-3](plan/M-maintainer.md#m-3) | waits: [M-3](plan/M-maintainer.md#m-3) |
| 152 | [G-3](plan/G-guides.md#g-3) "Start here" in C, and the gate that keeps every such page true | 2.6.0 | medium | [G-1b](plan/G-guides.md#g-1b), [G-1b](plan/G-guides.md#g-1b), [G-2](plan/G-guides.md#g-2) | waits: [G-1b](plan/G-guides.md#g-1b) |
| 153 | [G-4](plan/G-guides.md#g-4) "Start here" in C++ | 2.6.0 | small | [G-3](plan/G-guides.md#g-3) | waits: [G-3](plan/G-guides.md#g-3) |
| 154 | [G-5](plan/G-guides.md#g-5) "Start here" in Rust | 2.6.0 | small | [G-3](plan/G-guides.md#g-3) | waits: [G-3](plan/G-guides.md#g-3) |
| 155 | [G-6](plan/G-guides.md#g-6) "Start here" in Python | 2.6.0 | small | [G-2](plan/G-guides.md#g-2), [G-3](plan/G-guides.md#g-3), [G-4](plan/G-guides.md#g-4), [G-5](plan/G-guides.md#g-5) | waits: [G-2](plan/G-guides.md#g-2) |
| 156 | [G-7](plan/G-guides.md#g-7) Recipes: reload events as log records, a report in OPA and CUE | 2.6.0 | medium | [G-2](plan/G-guides.md#g-2), [G-3](plan/G-guides.md#g-3) | waits: [G-2](plan/G-guides.md#g-2) |
| 157 | [G-8](plan/G-guides.md#g-8) A playground on the documentation site | 2.6.0 | large | [G-1b](plan/G-guides.md#g-1b), [G-3](plan/G-guides.md#g-3) | waits: [G-1b](plan/G-guides.md#g-1b) |
| 158 | [G-9](plan/G-guides.md#g-9) A program not written for the library, moved onto it in a fork | 2.3.0 | large | [G-3](plan/G-guides.md#g-3) | waits: [G-3](plan/G-guides.md#g-3) |
| 159 | [M-22](plan/M-maintainer.md#m-22) Publishing by trusted publishing: the release workflow's registry jobs | 2.6.0 | large | nothing | open |
| 160 | [M-24](plan/M-maintainer.md#m-24) The clean-machine install, in CI | 2.6.0 | medium | [M-22](plan/M-maintainer.md#m-22), [M-23-m](plan/M-maintainer.md#m-23-m), [M-25-m](plan/M-maintainer.md#m-25-m) | waits: [M-22](plan/M-maintainer.md#m-22) |
| 161 | [M-26](plan/M-maintainer.md#m-26) Wheels for each platform | 2.6.0 | medium | [M-22](plan/M-maintainer.md#m-22) | waits: [M-22](plan/M-maintainer.md#m-22) |
| 162 | [M-27](plan/M-maintainer.md#m-27) Recipes in the tree: Meson WrapDB, the Bazel Central Registry, conda-forge | 2.6.0 | large | [M-22](plan/M-maintainer.md#m-22) | waits: [M-22](plan/M-maintainer.md#m-22) |
| 163 | [M-28](plan/M-maintainer.md#m-28) Embedded registries: ESP-IDF, PlatformIO, Zephyr | 2.6.0 | medium | [M-27](plan/M-maintainer.md#m-27) | waits: [M-27](plan/M-maintainer.md#m-27) |
| 164 | [M-31](plan/M-maintainer.md#m-31) The monthly count of downloads, traffic and issues | 2.6.0 | small | nothing | open |
| 165 | [P-3](plan/P-word-at-a-time.md#p-3) The readers' scans on lanes' four kept operations, kept only where they win | 2.7.0 | large | P-2 | open |
| 166 | [P-7](plan/P-measured.md#p-7) Why a real number costs 2000 instructions a byte in JSON and TOML | 2.4.0 | large | [P-6](plan/P-measured.md#p-6) | waits: [P-6](plan/P-measured.md#p-6) |
| 167 | [P-8](plan/P-measured.md#p-8) CBOR writes a place for every scalar: build it by hand, same bytes | 2.4.0 | small | nothing | open |
| 168 | [P-9](plan/P-measured.md#p-9) The second profile: the formats and the lines P-1 did not look at | 2.4.0 | medium | [P-3](plan/P-word-at-a-time.md#p-3), [P-7](plan/P-measured.md#p-7), [P-8](plan/P-measured.md#p-8) | waits: [P-3](plan/P-word-at-a-time.md#p-3) |
| 169 | [P-10](plan/P-measured.md#p-10) What the fastest libraries do, as a written survey with its numbers | 2.4.0 | medium | nothing | open |
| 170 | [P-13](plan/P-measured.md#p-13) SSE2 and NEON: the measured case, in numbers and a recommendation | 2.4.0 | large | [P-3](plan/P-word-at-a-time.md#p-3), [P-3](plan/P-word-at-a-time.md#p-3) | waits: [P-3](plan/P-word-at-a-time.md#p-3) |
| 171 | [P-16](plan/P-measured.md#p-16) Eight digits at a time: a word body for `digits8`, measured in lanes | 2.4.0 | medium | [P-10](plan/P-measured.md#p-10) | waits: [P-10](plan/P-measured.md#p-10) |
| 172 | [L-16a](plan/L-tools-2.md#l-16a) The emitter sized from the key space it walks, kept only if it pays | 2.7.0 | small | nothing | open |
| 173 | [L-16b](plan/L-tools-2.md#l-16b) Sequential read advice for a large file, kept only if it pays | 2.7.0 | small | nothing | open |
| 174 | [L-16c](plan/L-tools-2.md#l-16c) Branch hints on the readers' hot paths, kept only where they pay | 2.7.0 | medium | [P-3](plan/P-word-at-a-time.md#p-3) | waits: [P-3](plan/P-word-at-a-time.md#p-3) |
| 175 | [Y-11](plan/Y-asked.md#y-11) `cf_live_reader`: a worker's handle that caches a snapshot, its test under ThreadSanitizer, C++ and Rust wrappers | 2.7.0 | medium | [Y-10](plan/Y-asked.md#y-10), [Y-9](plan/Y-asked.md#y-9) | waits: [Y-10](plan/Y-asked.md#y-10) |
| 176 | [L-11](plan/L-tools.md#l-11) Mutation testing with Mull: the score of every component beside coverage, held above a floor | 2.1.0 | medium | nothing | open |
| 177 | [L-12a](plan/L-tools-2.md#l-12a) The public headers with no record at all | 2.1.0 | large | nothing | open |
| 178 | [L-12b](plan/L-tools-2.md#l-12b) The matrix of every MISRA rule, made from the record | 2.1.0 | medium | [L-12a](plan/L-tools-2.md#l-12a) | waits: [L-12a](plan/L-tools-2.md#l-12a) |
| 179 | [L-12c](plan/L-tools-2.md#l-12c) The compliance report, attached to each release | 2.1.0 | small | [L-12b](plan/L-tools-2.md#l-12b) | waits: [L-12b](plan/L-tools-2.md#l-12b) |
| 180 | [L-13](plan/L-tools-2.md#l-13) verify sees the version CI sees | 2.1.0 | small | nothing | open |
| 181 | [L-15a](plan/L-tools-2.md#l-15a) The comparison with whole configuration libraries: time to look up | 2.1.0 | medium | nothing | open |
| 182 | [L-15b](plan/L-tools-2.md#l-15b) libconfig in the comparison | 2.1.0 | medium | [L-15a](plan/L-tools-2.md#l-15a) | waits: [L-15a](plan/L-tools-2.md#l-15a) |
| 183 | [L-17](plan/L-tools-2.md#l-17) Byte order and word size in CI: s390x, i386 and armv7 under qemu-user | 2.1.0 | medium | nothing | open |
| 184 | [L-19a](plan/L-tools-2.md#l-19a) A Linux arm64 job | 2.1.0 | small | nothing | open |
| 185 | [L-19b](plan/L-tools-2.md#l-19b) A qemu line carries its date and commit | 2.1.0 | small | [L-17](plan/L-tools-2.md#l-17) | waits: [L-17](plan/L-tools-2.md#l-17) |
| 186 | [L-18b](plan/L-tools-2.md#l-18b) The Cortex-M `.text` of each part, in a table and a gate | 2.1.0 | medium | L-18a | open |
| 187 | [P-5](plan/P-measured.md#p-5) A profile after every weekly benchmark, kept as an artifact and compared | 2.1.0 | medium | nothing | open |
| 188 | [P-6](plan/P-measured.md#p-6) A cost that should not be there: bytes per instruction across sizes and formats | 2.1.0 | medium | nothing | open |
| 189 | [P-11](plan/P-measured.md#p-11) Print speed in the comparison: JSON, four libraries | 2.1.0 | large | nothing | open |
| 190 | [P-12](plan/P-measured.md#p-12) The README leads with the lines ConfigForge wins, made by a script from the numbers | 2.1.0 | medium | [P-11](plan/P-measured.md#p-11), [P-11](plan/P-measured.md#p-11) | waits: [P-11](plan/P-measured.md#p-11) |
| 191 | [P-14](plan/P-measured.md#p-14) The OpenSSF Scorecard: every finding read, every one the repository can meet met | 2.1.0 | medium | nothing | open |
| 192 | [M-20](plan/M-maintainer.md#m-20) The evidence pack: the script that assembles it, with a manifest that never claims what is not there | 2.1.0 | medium | nothing | open |
| 193 | [M-21](plan/M-maintainer.md#m-21) The evidence pack in the release | 2.1.0 | small | [M-20](plan/M-maintainer.md#m-20) | waits: [M-20](plan/M-maintainer.md#m-20) |
| 194 | [M-35](plan/M-maintainer.md#m-35) A card's id is a link, in Plan.md's index and in the Roadmap | 2.1.0 | small | nothing | open |
| 195 | [K-9h](plan/K-correctness.md#k-9h) Thread, signal and allocation lines beside the guarantee, held by `gate.guarantees` | 2.0.0 | medium | [K-9a](plan/K-correctness.md#k-9a) | done (b424389) |
| 196 | [K-9i](plan/K-correctness.md#k-9i) Fork, callbacks, durability and inherited descriptors: one table, each row a test | 2.0.0 | small | nothing | done (922a272) |
| 197 | [F-6](plan/F-files.md#f-6) The sync a replace needs, by platform: `fdatasync` on Linux, `F_FULLFSYNC` on macOS, kept only if measured | 2.7.0 | small | nothing | open |
| 198 | [F-7](plan/F-files.md#f-7) Many small files: measured first, then the cost per file cut where it shows | 2.7.0 | medium | nothing | open |
| 199 | [L-28](plan/L-tools-2.md#l-28) The CERT C matrix: every rule, which tool sees it, and the review for the rest | 2.0.0 | medium | nothing | done (1d4e24a) |
| 200 | [K-9k](plan/K-correctness.md#k-9k) The text of a system error without `strerror`'s shared buffer (CERT CON33-C) | 2.1.0 | small | [L-28](plan/L-tools-2.md#l-28) | done (a3bc664) |
| 201 | [L-25](plan/L-tools-2.md#l-25) MC/DC measured by GCC's condition coverage, with a floor that only rises | 2.1.0 | medium | nothing | done (79296cf) |
| 202 | [L-26a](plan/L-tools-2.md#l-26a) Each decision names the tests that hold it: the line, the table and `gate.trace` | 2.1.0 | medium | nothing | open |
| 203 | [L-26b](plan/L-tools-2.md#l-26b) Every decision traced: the to-do list emptied | 2.1.0 | large | [L-26a](plan/L-tools-2.md#l-26a) | waits: [L-26a](plan/L-tools-2.md#l-26a) |
| 204 | [L-27](plan/L-tools-2.md#l-27) The safety manual: the assumptions a user's program must hold, each pointing at its evidence | 2.1.0 | medium | [L-12c](plan/L-tools-2.md#l-12c), [L-25](plan/L-tools-2.md#l-25), [L-26a](plan/L-tools-2.md#l-26a) | waits: [L-12c](plan/L-tools-2.md#l-12c) |
| 205 | [L-17-m](plan/L-tools-2.md#l-17-m) The `targets` jobs required | 2.1.0 | small | [L-17](plan/L-tools-2.md#l-17) | parked: not now |
| 206 | [L-19-m](plan/L-tools-2.md#l-19-m) Arm devices run, recorded with their commit | 2.1.0 | small | [L-19b](plan/L-tools-2.md#l-19b) | parked: not now |
| 207 | [P-15-m](plan/P-measured.md#p-15-m) The repository settings the Scorecard waits on (the maintainer's) | 2.1.0 | small | [P-14](plan/P-measured.md#p-14) | parked: not now |
| 208 | [M-19-m](plan/M-maintainer.md#m-19-m) OSS-Fuzz: the application (parked) | 2.1.0 | small | M-18 | parked: not now |
| 209 | [M-23-m](plan/M-maintainer.md#m-23-m) Registry set-up (parked) | 2.6.0 | small | [M-22](plan/M-maintainer.md#m-22) | parked: not now |
| 210 | [M-25-m](plan/M-maintainer.md#m-25-m) Upstream submissions: vcpkg and ConanCenter (parked) | 2.6.0 | small | [M-22](plan/M-maintainer.md#m-22) | parked: not now |
| 211 | [M-29-m](plan/M-maintainer.md#m-29-m) Submissions and registry accounts (parked) | 2.3.0 | small | [M-27](plan/M-maintainer.md#m-27), [M-28](plan/M-maintainer.md#m-28) | parked: not now |
| 212 | [M-30-m](plan/M-maintainer.md#m-30-m) The GitHub Action in a repository of its own (parked) | 2.3.0 | small | nothing | parked: not now |
| 213 | [G-9-m](plan/G-guides.md#g-9-m) Publishing the fork and what it found | 2.3.0 | small | [G-9](plan/G-guides.md#g-9) | parked: not now |
<!-- index end -->

### Waits on the maintainer (skipped by the loop)

Work that needs the maintainer's own access or an outside party:
registry tokens or trusted publishing, repository settings, the OSS-Fuzz
application, tags and releases, paid tools, hardware that cannot be
reached (the maintainer, 2026-11-26). Each is a card of its own, apart
from the agents' cards, with status "parked". An agent card never holds
such a step: its agent part is written to stand alone, or it is parked
too. A parked card opens only when the maintainer says it can be done;
one that cannot be done is dropped and recorded under "Done" as dropped.

| Card | Version | Waits on | Status |
| ---- | ------- | -------- | ------ |
| L-17-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| L-19-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| G-9-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| P-15-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-19-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-23-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-25-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-29-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-30-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |

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

* **W5-convert-and-gen.md** (1.7.0 and 1.8.0; W5-V1, W5-S1, W5-G1 to
  W5-G4, W5-V2; D-94 to D-96, D-109, D-110). `configforge codec convert`
  and `format`, every value the target cannot hold named, never dropped
  quietly (W5-V1, 26ab6bd); a contract drafted from a sample, `gen
  schema --from` (W5-S1, 0a8de0f); `gen --lang cpp`, `rust` and `python`
  (W5-G1, 071ed40; W5-G2, 54f39fb; W5-G3, 950a099); every ordered pair
  of formats tested against what each can hold (W5-V2, 70bd398); and
  generators outside the tree, `configforge-gen-<lang>` on the PATH
  (W5-G4, adc2a63).
* **L-tools.md** (1.7.0 to 1.9.0; L-1 to L-10, L-3b). The other
  languages' linters as gates: shellcheck, ruff, actionlint, codespell
  (L-1, 7bec7d8); Valgrind, AFL++, gitleaks, osv-scanner and Scorecard
  in CI (L-2, 6442c8f); include-what-you-use and a link check (L-4,
  a87a174); Infer and semgrep with the project's own rules (L-3,
  c2c14f4); one command for the profilers (L-5, 13e9e70); a map of the
  tree and `brief.py`, a brief per card (L-6, 00dd641); CI in 13.9
  minutes instead of 19.2, nothing taken away (L-7, 09fe647); and
  `doctor.api` cheap under MemorySanitizer (L-8, 179c6df); the Scorecard
  findings the project can fix itself (L-9, 3f251f8); and supply-chain
  hardening: every fetch held by `gate.fetches`, read-only workflow
  tokens, vcpkg pinned, dependency review, the SBOM a release file
  (L-10, c4a13af); and Frama-C EVA over `port`, the integer half of
  `numeric` and `base`, every alarm accepted with its reason in
  `.eva-accepted.txt` (L-3b, 4c3ce8c).
* **A-architecture.md** (1.8.0; A-1 to A-13; D-100 to D-108, D-112; the
  design stays in the file). One directory a component (A-1, 824181f) and
  each a project built alone (A-1b, 031d54d); the `lanes` vector library
  (A-2, da0dd49); one UTF-8, hex digit and table hash (A-3, 520baa2);
  another encoding read by name (A-4, e4069b8); bytes that are not UTF-8 refused with place and
  hint (A-5, 98c068e); `configforge codec recode` (A-6, 0b4b97a); the
  escapes in families (A-7, fca866c); whitespace, comments, newlines and
  the place of a refusal (A-8, c3bba84); composed codecs (A-9, a4ea9e1,
  made whole in the next commit); a refusal's place and reason
  (A-10, f1a4fd7); build profiles and the C version per file (A-11,
  b837c23); the README's feature matrix made from the build (A-12,
  23b5d2a); and a page of the project's limits held by `gate.limits`
  (A-13, 622d26b).
* **I-integration.md** (1.8.0; I-1, cb9d8a3; D-109). The thirteen
  findings of the integration check of the 1.8.0 group, fixed together.
* **P-allocators.md** (1.8.0; P-4, ca2f115; D-72). Allocation measured:
  the wipe through `memset` saved 25% of instructions on a 16 MiB load
  and 72% on a tiny document; three growth candidates measured and
  closed. The findings stay in the file.
* **N-next.md** (1.8.0 and 1.9.0; N-1 to N-17, N-19 and N-20, I-2; D-100; the file keeps only its intro). `configforge diff` of two
  files by value (N-1, 77a11ae), of nested tables (N-5, ad5b840) and of
  scalar arrays (N-8, 7bbd943), with a Git difftool recipe (N-3,
  efb74f1); the musl release archive built twice and compared (N-2,
  6c67353); the YAML codec delivering one root table (N-4, f6c0ba7);
  `cf_usage`, `--help` from the contract (N-6, 83f5c64); a reload that
  names the keys it changed (N-7, 37ea469); cppcheck on both libraries
  (N-9, ec0de1e) and its style class (N-10, 67a451a); and
  `cf_blob_skip_bom` proved with CBMC (N-11, 765de40). In 1.9.0: the
  thirteen 1.8 experimental functions promised (I-2, 7146038; D-89);
  a reason's severity and weakness id (N-13, 2687154); `diff` over
  arrays of tables (N-14, 73c2798); the changed keys of a pull request
  in a comment (N-15, dfc1fed); differential fuzzing of the YAML reader
  against libyaml (N-12, d8a0cfa); Git's `textconv` driver (N-16,
  bbb1a77); `doctor drift`, many reports grouped by value (N-17,
  ca3dd7b); a lone CR ending a YAML comment (N-20, d04740d); and `gate.misra`,
  cppcheck's MISRA addon on the `misra` profile with a record for every
  report (N-19, 24b1c7d; D-112).
* **P-word-at-a-time.md** (1.8.0 and 1.9.0; P-1, P-1b, P-2; P-3 is open;
  D-101). The benchmark measures large documents and says where time
  goes (P-1, 90b6541); the other libraries beside ours, on a page CI
  keeps (P-1b, 427ec6a); and `lanes`' word-at-a-time backend, kept for
  four operations where it wins (P-2, e0c0904).

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

**The plan laid out again (the maintainer, 2026-11-26, in this session).**
The index above is ordered by group, not by version: the loop would carry
2.2.0 cards (F) before 1.9.0 ones (K, L), 1.9.0 holds 73 cards, every
2.0.0 card waits on the maintainer, and some needs point forward (F-2a
needs Y-2). So, before any new card is written, the plan is laid out
again, and the loop takes no card until it is:

1. **By module and stage, not by group letter.** Stage 0 first, the
   foundations every module builds on: one error model (D-223), options
   with defaults (D-220), the file layer (D-224). Then each `lib/`
   component climbs the same steps: the module (its C interface and
   tests, for the programmer), then the tool (`configforge`'s command on
   it), then the bindings, then the documents.
2. **Interim releases.** When a module finishes its first step, an interim
   release is proposed to the maintainer (tags stay the maintainer's).
3. **No question left open.** A card opens only when a cheap check
   (a script, not a model) passes: no TO DECIDE, no unsettled "or" in
   "decided", every option named with its default, every `file:line`
   finding its symbol, every need in the index. The index is generated
   from the card files by a script in the repository and kept true by a
   gate. A card carries its decision in its own text: no "to be
   rewritten" state.
4. **A "not now" list.** What is deferred is written once, so that no
   session discusses it again.
5. **The budget written down.** A card's measured cost (model, effort,
   share of the window) is kept, and the versions are set by it.
6. **Cold reads only when needed** (the maintainer: "useful, but only
   when needed; scanning the whole plan wastes tokens"): when the check
   of point 3 fails, or the code a card builds on changed in design.
   Never a whole file, never in advance.

The first step is a one-page outline of this layout (stage 0, the order of
the modules, each interim release's scope, the "not now" list) for the
maintainer's approval; the 193 cards are then placed into it, merged
where they overlap.

**Read cold once, just before the group opens** (superseded by point 6 above: only when needed) (the maintainer,
2026-11-26, relayed from a side session: "it does no real work meanwhile; the
order of work must be clear": no more whole-file readings in
advance; the work's order is the index). A card file is read cold once,
by someone who did not write it, just before its group's first card
opens, and corrected in one commit. It is not read again for every
change to the code: a card that finds its lines moved finds the symbol by
name, and only a card whose needed card landed with a different design
is checked again, alone. A gate that checks each card's `file:line`
citations still point at the symbol they name is to be written as a card
of group L, so that a model's reading is kept for the design.

**Features first, and layers so that code is touched once** (the
maintainer, 2026-11-26, relayed from a side session: "put real feature
additions first; use abstraction layers to reduce refactoring"). Among the cards whose needs are done, the loop
takes first one that adds something a user of the library sees (groups
C, F, X, Y) before correctness, tools, documents or optimisation (K, L,
G, P); the index's order is kept until the maintainer settles the order
of the groups. A card that changes code another card will change again
does it through an interface of its component (D-100 to D-108: one
component a `lib/` directory, its internal layers behind interfaces), so
that the later card adds to the layer and does not rewrite its callers.

Each wave's milestone is in `docs/Maintaining.md`, "Versions", laid
out again on 2026-11-19: versions go on after 2.0.0, which marks what
breaks, not the end of the plan.
