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
      and whose "needs" are all "done". A "parked" card (the section
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

* **Asked, 2026-11-26**: every card in "Waits on the maintainer" with
  "its TO DECIDE" waits for an answer written into the card (each
  TO DECIDE carries its options and a recommendation); the M cards wait
  for the maintainer's approval. A relayed message said every TO DECIDE
  is approved by its recommendation; it is not written in as an answer
  until the maintainer says so in their own words.

## The index

In the order to be done. A card is `open`, `done` (with its commit), or
`waits` (with what it waits for). The finished cards (H, W3, S, T,
W5, L, A, I, N and P, but for the open cards) are recorded in "Done, their
cards removed", below; the cards are in `docs/plan/*.md`.
P-3 (word-at-a-time scanning before SIMD: `docs/plan/P-word-at-a-time.md`)
waits for 2.4.0: D-101 gives the choice of backend to the program then, and
D-102 moves the scans into `lib/text`.

**The order of work (2026-11-26, the maintainer).** Every open item of
docs/ has a card, in `docs/plan/*.md`, and a row below, in this order of
groups: C (the people who change configuration, `C-change.md`,
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

| Order | Card | Version | Size | Needs | Status |
| ----: | ---- | ------- | ----- | ----- | ------ |
| 1 | C-1a `configforge diff` over arrays of arrays | 1.9.0 | medium | nothing | open |
| 2 | C-1b `configforge diff --schema`: units compared as values | 1.10.0 | medium | C-2a, C-2b, C-1a | waits: C-2a |
| 3 | C-2a Durations and sizes: the unit on an int definition | 1.10.0 | medium | nothing | parked |
| 4 | C-2b Durations and sizes: the schema, the report, `cf_emit` and every reader | 1.10.0 | medium | C-2a | waits: C-2a |
| 5 | C-3a `configforge diff --schema` flags the keys out of range and the keys that touch a secret | 1.9.0 | medium | C-1b | waits: C-2a |
| 6 | C-3b The pull request's comment says which keys are out of range and which touch a secret | 1.9.0 | small | C-3a | waits: C-2a |
| 7 | C-4a `doctor drift`: the source of a value, and a list file | 1.9.0 | medium | nothing | open |
| 8 | C-4b Drift across N configuration files: `configforge diff --all` | 1.9.0 | medium | C-1a | open |
| 9 | C-4c `doctor drift` from Python | 1.9.0 | small | C-4a | open |
| 10 | C-6a Shell completions from the contract: `configforge gen --lang bash\|zsh\|fish\|powershell` | 1.9.0 | medium | nothing | open |
| 11 | C-6b A man page and a sample file from the contract: `configforge gen --lang man\|sample-toml\|sa | 1.9.0 | medium | C-6a | open |
| 12 | C-7 Profiles: `--profile` or `NAME_PROFILE` adds `config.<profile>.<ext>` above the base file | 1.9.0 | medium | C-11 | open |
| 13 | C-8a A read the contract does not have, noticed: the library records it | 1.9.0 | medium | nothing | open |
| 14 | C-8b Doctor's CFD021: a read of a key the contract does not have, in `doctor lint` | 1.9.0 | small | C-8a | open |
| 15 | C-9 Values migrated, not only renamed: a function on the old key's definition | 1.9.0 | medium | nothing | open |
| 16 | C-10a A running program asked for its report: the library answers one request on a descriptor | 1.9.0 | medium | nothing | open |
| 17 | C-10b `configforge doctor pull unix:<path>`, and an example that serves from a loop | 1.9.0 | medium | C-10a | open |
| 18 | C-11 A program's files where its platform keeps them: `cf_builder_app_files` | 1.9.0 | medium | nothing | open |
| 19 | F-1a cf_platform_write_file: a file replaced whole or not at all | 2.2.0 | medium | nothing | open |
| 20 | F-1b A public call that writes a configuration to a file whole or not at all (TO DECIDE) | 2.2.0 | small | F-1a | parked |
| 21 | F-2a An override file the library owns: a file source that cf_write can commit to (TO DECIDE) | 2.2.0 | large | F-1a, Y-2 | parked |
| 22 | F-2b settings.c is the override source's example | 2.2.0 | small | F-2a, F-1b | waits: F-2a |
| 23 | F-3a Wave 6, the core: a change spliced into a document and checked by reading it back | 2.2.0 | large | nothing | open |
| 24 | F-3b Wave 6, JSON and JSON5: a value changed, a member added or removed | 2.2.0 | medium | F-3a | open |
| 25 | F-3c Wave 6, INI and .properties: a value changed, a line added or removed | 2.2.0 | medium | F-3a | open |
| 26 | F-3d Wave 6, TOML: a value changed, a key removed | 2.2.0 | medium | F-3a | open |
| 27 | F-3e Wave 6, TOML: a key added | 2.2.0 | medium | F-3d | open |
| 28 | F-3f Wave 6, YAML: a value changed, a key removed | 2.2.0 | medium | F-3a | open |
| 29 | F-3g Wave 6, YAML: a key added | 2.2.0 | medium | F-3f | open |
| 30 | F-3h Wave 6, TLV: a value changed, an entry added or removed, the bytes a layout skips kept | 2.2.0 | large | F-3a | open |
| 31 | F-3i Wave 6, `configforge codec get`, `set` and `unset` (TO DECIDE) | 2.2.0 | medium | F-1a, F-3b, F-3c, F-3d, F-3e, F-3f, F-3g | open |
| 32 | F-3j Wave 6, the document edit as a public interface (TO DECIDE) | 2.2.0 | medium | F-3i, F-1b | parked |
| 33 | F-4a The references drawn, first by jq | 1.10.0 | small | nothing | open |
| 34 | F-4b `configforge doctor graph` (TO DECIDE: whether it is built at all) | 1.10.0 | medium | F-4a | parked |
| 35 | K-1a `.properties` read by Java's own `Properties`, and the two compared | 1.10.0 | medium | nothing | open |
| 36 | K-1b What the comparison with Java found, fixed | 1.10.0 | medium | K-1a | open |
| 37 | K-2a The other libraries' known weaknesses, searched and counted, as a table | 1.9.0 | medium | nothing | open |
| 38 | K-2b `gate.weaknesses`: each row names a test that exists | 1.9.0 | medium | K-2a | open |
| 39 | K-2c The weaknesses of expansion, nesting, lengths and time, as tests | 1.9.0 | medium | K-2b | open |
| 40 | K-2d The weaknesses of truncation, NUL, UTF-8, numbers and duplicates, as tests | 1.9.0 | medium | K-2b | open |
| 41 | K-2e The last rows, the table closed, the Roadmap item done | 1.9.0 | small | K-2c, K-2d | open |
| 42 | K-3a The blob's file reader proved up to a bound | 1.9.0 | large | nothing | open |
| 43 | K-3b The INI reader proved up to a bound | 1.9.0 | large | K-3a | open |
| 44 | K-4a One harness for differential fuzzing, and JSON against yyjson | 1.9.0 | large | nothing | open |
| 45 | K-4b TOML against tomlc17 | 1.9.0 | medium | K-4a | open |
| 46 | K-4c CBOR against tinycbor | 1.9.0 | medium | K-4a | open |
| 47 | K-4d XML against a second parser (TO DECIDE) | 1.9.0 | medium | K-4a | open |
| 48 | K-5a TOML's joined path judged as one number | 1.9.0 | medium | nothing | open |
| 49 | K-5b INI's reader limits nesting as the others do | 1.9.0 | small | K-5a | open |
| 50 | K-6a A refusal's byte offset (TO DECIDE) | 1.9.0 | medium | nothing | parked |
| 51 | K-6b A refusal's reason shown by the tools (TO DECIDE) | 1.9.0 | small | nothing | parked |
| 52 | K-7a One table of formats: the lists that copy it read it (TO DECIDE) | 1.9.0 | medium | nothing | open |
| 53 | K-7b The ways a format can be read, in its table entry (2.0.0; TO DECIDE) | 2.0.0 | small | K-7a | parked |
| 54 | K-8 The YAML reader's three differences from libyaml, fixed to YAML 1.2's text (TO DECIDE, eac | 1.9.0 | medium | nothing | open |
| 55 | X-18 The helpers each reader writes again: a table, kept true by a gate | 1.9.0 | medium | nothing | open |
| 56 | X-19 The printers apart: one file a format's writer | 1.9.0 | medium | X-18 | open |
| 57 | X-20 What repeats, once: the helpers X-18's table says move | 1.9.0 | large | X-19 | open |
| 58 | X-21 Text and binary formats as two groups a build can take apart | 1.9.0 | medium | X-19 | parked |
| 59 | X-1 MessagePack: reader, writer, vectors, fuzz target | 2.1.0 | large | nothing | open |
| 60 | X-2 BSON | 2.1.0 | large | X-1 | open |
| 61 | X-3 UBJSON | 2.1.0 | medium | X-2 | open |
| 62 | X-4 Smile | 2.1.0 | large | X-3 | open |
| 63 | X-5 Amazon Ion (binary): reader; its writer is TO DECIDE | 2.1.0 | large | X-4 | open |
| 64 | X-6 Bencode, read only | 2.1.0 | medium | X-5 | open |
| 65 | X-7 Property lists, the binary form (bplist00) | 2.1.0 | large | X-6 | open |
| 66 | X-8 Property lists, the XML form: TO DECIDE (the scanner) | 2.1.0 | large | X-7 | open |
| 67 | X-9 HCL (HashiCorp Configuration Language 2), the literal subset | 2.1.0 | large | X-8 | parked |
| 68 | X-10 HOCON, without substitutions and includes | 2.1.0 | large | X-9 | waits: X-9 |
| 69 | X-11 KDL (version 2) | 2.1.0 | large | X-10 | waits: X-9 |
| 70 | X-12 TLV as the standards have it: DER, SIMPLE-TLV, EMV and KLV | 2.1.0 | large | nothing | parked |
| 71 | X-13 TLV as devices write it: field order, widths, an implied length, a packed head | 2.1.0 | large | X-12 | parked |
| 72 | X-14 Formats bound to a schema: Avro's container, and what the rest waits on | 2.1.0 | large | X-13 | parked |
| 73 | X-15 The benchmark learns the binary formats: MiB/s for each codec | 2.1.0 | medium | X-1, X-7, X-2, X-3, X-4, X-5, X-6 | open |
| 74 | X-16 The benchmark learns the text formats and the read-only ones | 2.1.0 | medium | X-15, X-8, X-11, X-5, X-14, X-9, X-10 | waits: X-9 |
| 75 | X-17 Text handed to the sink where it lies | 2.1.0 | large | X-16 | waits: X-9 |
| 76 | L-11 Mutation testing with Mull: the score of every component beside coverage, held above a flo | 1.9.0 | medium | nothing | open |
| 77 | L-12a The public headers with no record at all | 1.9.0 | large | nothing | open |
| 78 | L-12b The matrix of every MISRA rule, made from the record | 1.9.0 | medium | L-12a | open |
| 79 | L-12c The compliance report, attached to each release | 1.9.0 | small | L-12b | open |
| 80 | L-13 verify sees the version CI sees | 1.9.0 | small | nothing | open |
| 81 | L-14a EVA over the float readers: `cf_text_to_f64` and the bit conversions | 1.9.0 | large | nothing | open |
| 82 | L-14b EVA over the float printer: `cf_gen_digits` and `cf_f64_to_text` | 1.9.0 | large | L-14a | open |
| 83 | L-15a The comparison with whole configuration libraries: time to look up | 1.9.0 | medium | nothing | open |
| 84 | L-15b libconfig in the comparison | 1.9.0 | medium | L-15a | parked |
| 85 | L-16a The emitter sized from the key space it walks, kept only if it pays | 2.4.0 | small | nothing | open |
| 86 | L-16b Sequential read advice for a large file, kept only if it pays | 2.4.0 | small | nothing | open |
| 87 | L-16c Branch hints on the readers' hot paths, kept only where they pay | 2.4.0 | medium | nothing | open |
| 88 | L-17 Byte order and word size in CI: s390x, i386 and armv7 under qemu-user | 1.9.0 | medium | nothing | open |
| 89 | L-17-m The `targets` jobs required | 1.9.0 | small | L-17 | parked |
| 90 | L-18a A Cortex-M build of `minimal` in CI, with its stack gate | 1.9.0 | medium | nothing | open |
| 91 | L-18b The Cortex-M `.text` of each part, in a table and a gate | 1.9.0 | medium | L-18a | open |
| 92 | L-19a A Linux arm64 job | 1.9.0 | small | nothing | open |
| 93 | L-19b A qemu line carries its date and commit | 1.9.0 | small | L-17 | open |
| 94 | L-19-m Arm devices run, recorded with their commit | 1.9.0 | small | L-19b | parked |
| 95 | G-1a The tools that read a decision follow the split | 1.9.0 | medium | nothing | open |
| 96 | G-1b Decisions.md split into one file a decision | 1.9.0 | medium | G-1a | open |
| 97 | G-2 The Roadmap's finished items on a page of their own | 1.9.0 | medium | G-1b | open |
| 98 | G-3 "Start here" in C, and the gate that keeps every such page true | 2.3.0 | medium | G-1b | open |
| 99 | G-4 "Start here" in C++ | 2.3.0 | small | G-3 | open |
| 100 | G-5 "Start here" in Rust | 2.3.0 | small | G-3 | open |
| 101 | G-6 "Start here" in Python | 2.3.0 | small | G-2, G-3, G-4, G-5 | open |
| 102 | G-7 Recipes: reload events as log records, a report in OPA and CUE | 2.3.0 | medium | G-2, G-3 | open |
| 103 | G-8 A playground on the documentation site | 2.3.0 | large | G-1b, G-3 | open |
| 104 | G-9 A program not written for the library, moved onto it in a fork | 2.3.0 | large | nothing | parked |
| 105 | G-9-m Publishing the fork and what it found | 2.3.0 | small | G-9 | parked |
| 106 | P-3 The readers' scans on lanes' four kept operations, kept only where they win | 2.4.0 | large | nothing | waits: 2.4.0 (D-101, D-102: the program chooses the backend; the scans move into lib/text) |
| 107 | P-5 A profile after every weekly benchmark, kept as an artifact and compared | 1.9.0 | medium | nothing | open |
| 108 | P-6 A cost that should not be there: bytes per instruction across sizes and formats | 1.9.0 | medium | nothing | open |
| 109 | P-7 Why a real number costs 2000 instructions a byte in JSON and TOML | 2.4.0 | large | P-6 | open |
| 110 | P-8 CBOR writes a place for every scalar: build it by hand, same bytes | 2.4.0 | small | nothing | open |
| 111 | P-9 The second profile: the formats and the lines P-1 did not look at | 2.4.0 | medium | P-3, P-7, P-8 | waits: P-3 |
| 112 | P-10 What the fastest libraries do, as a written survey with its numbers | 2.4.0 | medium | nothing | open |
| 113 | P-11 Print speed in the comparison: JSON, four libraries | 1.9.0 | large | nothing | open |
| 114 | P-12 The README leads with the lines ConfigForge wins, made by a script from the numbers | 1.9.0 | medium | P-11 | open |
| 115 | P-13 SSE2 and NEON: the measured case, in numbers and a recommendation | 2.4.0 | large | P-3 | waits: P-3 |
| 116 | P-14 The OpenSSF Scorecard: every finding read, every one the repository can meet met | 1.9.0 | medium | nothing | open |
| 117 | P-15-m The repository settings the Scorecard waits on (the maintainer's) | 1.9.0 | small | P-14 | parked |
| 118 | P-16 Eight digits at a time: a word body for `digits8`, measured in lanes | 2.4.0 | medium | P-10 | open |
| 119 | Y-1 The Guide's "At the same time": what is guarded and what is not | 1.9.0 | small | nothing | open |
| 120 | Y-2 `cf_platform_write_file_if`: the replace refused when the file no longer holds the bytes r | 2.2.0 | small | F-1a | open |
| 121 | Y-3 `cf_platform_lock_take` and `_give`: a kernel lock on a kept sidecar file (F-5a in the res | 2.2.0 | medium | F-1a | parked |
| 122 | Y-4 `CF_FILE_LOCK`, `cf_builder_write_wait`, `CF_E_BUSY`: the lock around `overrides_write`, a | 2.2.0 | medium | Y-3, F-2a | parked |
| 123 | Y-5 A stable read: a file read twice and compared, up to 3 reads (F-5c in the research) | 2.2.0 | small | nothing | parked |
| 124 | Y-6 Windows readers that share delete access, if a measurement shows a rename then succeeds ov | 2.2.0 | small | F-1a | open |
| 125 | Y-7 "Threads and processes" in the Guide: four recipes, compiled in CI, a `fork` test | 1.10.0 | medium | Y-1, Y-8 | open |
| 126 | Y-8 Close-on-exec and non-inheritable everywhere the library opens a file, with a child that l | 1.9.0 | small | nothing | open |
| 127 | Y-9 A second `cf_live` driver refused, and D-48's sentence checked | 1.9.0 | small | Y-1 | open |
| 128 | Y-10 `cf_live_get` and `cf_config_free` under threads on the benchmark's page; `cf_live_generat | 1.9.0 | medium | nothing | open |
| 129 | Y-11 `cf_live_reader`: a worker's handle that caches a snapshot, its test under ThreadSanitizer | 2.4.0 | medium | Y-10, Y-9 | parked |
| 130 | Y-12 `swapped` in `cf_live_options`: workers told of a swap | 2.2.0 | small | nothing | parked |
| 131 | Y-13-m A shared image across processes: memfd and seals, `shm_open`, a read-only section, the sou | 2.4.0 | large | Y-7, Y-8 | parked |
| 132 | Y-14 Compressed input by the program's own zlib: the D-103 recipe, a Guide section and an examp | 1.10.0 | small | nothing | open |
| 133 | Y-15 The `cf_unpacker` interface: a byte layer under the codecs, its bounds, a NULL backend (TO | 1.10.0 | large | Y-14 | parked |
| 134 | Y-16 gzip through zlib: CF_WITH_ZLIB (TO DECIDE) | 1.10.0 | medium | Y-15 | parked |
| 135 | Y-17 zstd through libzstd: CF_WITH_ZSTD | 1.10.0 | medium | Y-16 | parked |
| 136 | Y-18 The sources read through the layer: flags, extensions, provenance, bindings (TO DECIDE) | 1.10.0 | medium | Y-15, Y-16, Y-17 | parked |
| 137 | Y-19 `configforge codec` through the layer, and a bomb corpus | 1.10.0 | small | Y-18 | waits: Y-18 |
| 138 | Y-20 Writing: cf_pack, deterministic gzip and zstd, `convert --compress`, a compressed image | 2.2.0 | medium | Y-16, Y-17, Y-19, F-1b | waits: Y-16 |
| 139 | Y-21-m A built-in inflate for the embedded presets | 2.2.0 | large | Y-16, Y-20 | parked |
| 140 | Y-22-m xz and lz4 frames | 1.10.0 | medium | Y-17 | parked |
| 141 | Y-23-m zstd dictionaries on a source | 1.10.0 | medium | Y-18, Y-20 | parked |
| 142 | Y-24 `configforge gen build`: sources by rank, a contract, a target, deterministic output (TO D | 1.9.0 | medium | Y-34 | parked |
| 143 | Y-25 The open-key checks: what the compiler can prove about the keys left to run time | 1.9.0 | small | Y-24 | waits: Y-24 |
| 144 | Y-26 `CF_IMAGE_CONFIGURED`: an image that holds only what a source configured, and defaults and | 1.9.0 | small | Y-24 | parked |
| 145 | Y-27 The baked configuration: `gen build --target c` and `cf_config_from_baked` | 1.9.0 | large | Y-24, Y-26 | parked |
| 146 | Y-28 `cf_builder_diagnostics`: every diagnostic with its place, and `gen build --json` and `gen | 1.10.0 | medium | Y-24 | parked |
| 147 | Y-29 The manifest: inputs and their digests, the contract's digest, version, profile, open keys | 1.10.0 | small | Y-24, Y-25, M-34 | parked |
| 148 | Y-30-m Rules across keys in the contract: a fixed declarative set (TO DECIDE D) | 1.10.0 | medium | Y-24 | parked |
| 149 | Y-31 Guide recipes: CUE, Pkl, Jsonnet, Dhall, Nickel and KCL as front ends | 1.10.0 | small | Y-24 | waits: Y-24 |
| 150 | Y-32-m A build file read with the tool's own contract | 1.10.0 | medium | Y-24, Y-29 | parked |
| 151 | Y-33 The manifest inside the image (2.0.0, with the image header's layout) | 2.0.0 | small | Y-29 | waits: Y-29 |
| 152 | Y-34 `gen check --report`: a predicted, value-blind report from files and a contract (TO DECIDE | 1.10.0 | medium | nothing | parked |
| 153 | Y-35 Doctor on a predicted report: the rules a running program is needed for are off and named; | 1.10.0 | small | Y-34 | waits: Y-34 |
| 154 | Y-36 Recipes: the contract's schema in yaml-language-server and Taplo, conftest over a report w | 1.10.0 | small | nothing | parked |
| 155 | Y-37 A secret written in a checked file: CFD022 (TO DECIDE) | 1.10.0 | small | Y-35, C-8b | parked |
| 156 | Y-38 Defined but never read, over a set of reports: CFD023 (TO DECIDE) | 1.10.0 | small | C-8a, C-8b | open |
| 157 | Y-39-m Relations in the contract: `pattern`, `if`/`then`, `dependentRequired` (TO DECIDE 2) | 2.3.0 | large | Y-36 | parked |
| 158 | Y-40-m The language server | 2.3.0 | large | Y-34, Y-36 | parked |
| 159 | M-1 Go (cgo): the module over the C library | 2.3.0 | large | nothing | parked |
| 160 | M-2 Go: an example and a CI job | 2.3.0 | medium | M-1 | parked |
| 161 | M-3 .NET (P/Invoke): the assembly over the C library | 2.3.0 | large | nothing | parked |
| 162 | M-4 .NET: an example and a CI job | 2.3.0 | medium | M-3 | parked |
| 163 | M-5 The stream: one interface for reading in pieces, with a fallback every codec has | 2.0.0 | large | nothing | parked |
| 164 | M-6 The stream, native: CBOR and TLV (the length-prefixed formats first) | 2.0.0 | medium | M-5 | parked |
| 165 | M-7 The stream, native: JSON, JSON5 and `.properties` | 2.0.0 | large | M-5, M-6 | parked |
| 166 | M-8 The stream, native: INI and TOML (the readers that look back) | 2.0.0 | large | M-5, M-7 | parked |
| 167 | M-9 The stream, native: YAML, and the file source that uses it | 2.0.0 | large | M-5, M-7, M-8 | parked |
| 168 | M-10 The dynamic sizing modes: the interface | 2.0.0 | medium | nothing | parked |
| 169 | M-11a Sizing in the readers and the key space | 2.0.0 | medium | M-10 | parked |
| 170 | M-11b `cf_error` owns its text (the 2.0 break) | 2.0.0 | large | M-11a | parked |
| 171 | M-12 The image header's layout, version 2 | 2.0.0 | medium | nothing | parked |
| 172 | M-13 2.0, once: the list of what breaks, and the gate that holds it | 2.0.0 | small | M-9, M-11b, M-12 | parked |
| 173 | M-14 Ways to read: the document tree (DOM), a public type of the serialization library | 2.0.0 | large | nothing | parked |
| 174 | M-15 Ways to read: lazy, a structural index first | 2.0.0 | large | M-14 | parked |
| 175 | M-16 Ways to read: in-situ strings | 2.0.0 | medium | nothing | parked |
| 176 | M-17 Ways to read: the table, and each mode measured | 2.0.0 | medium | M-9, M-14, M-15, M-16 | parked |
| 177 | M-18 OSS-Fuzz: the project files in the repository | 1.9.0 | small | nothing | parked |
| 178 | M-19-m OSS-Fuzz: the application (parked) | 1.9.0 | small | M-18 | parked |
| 179 | M-20 The evidence pack: the script that assembles it, with a manifest that never claims what is | 1.9.0 | medium | nothing | parked |
| 180 | M-21 The evidence pack in the release | 1.9.0 | small | M-20 | parked |
| 181 | M-22 Publishing by trusted publishing: the release workflow's registry jobs | 1.10.0 | large | nothing | parked |
| 182 | M-23-m Registry set-up (parked) | 1.10.0 | small | M-22 | parked |
| 183 | M-24 The clean-machine install, in CI | 1.10.0 | medium | M-22, M-23-m, M-25-m | parked |
| 184 | M-25-m Upstream submissions: vcpkg and ConanCenter (parked) | 1.10.0 | small | M-22 | parked |
| 185 | M-26 Wheels for each platform | 2.3.0 | medium | M-22 | parked |
| 186 | M-27 Recipes in the tree: Meson WrapDB, the Bazel Central Registry, conda-forge | 2.3.0 | large | M-22 | parked |
| 187 | M-28 Embedded registries: ESP-IDF, PlatformIO, Zephyr | 2.3.0 | medium | M-27 | parked |
| 188 | M-29-m Submissions and registry accounts (parked) | 2.3.0 | small | M-27, M-28 | parked |
| 189 | M-30-m The GitHub Action in a repository of its own (parked) | 2.3.0 | small | nothing | parked |
| 190 | M-31 The monthly count of downloads, traffic and issues | 2.3.0 | small | nothing | parked |
| 191 | M-32 One digest a purpose: SHA-256 as the fingerprint's option | 1.10.0 | large | nothing | parked |
| 192 | M-33 A digest function the program gives | 1.10.0 | medium | M-32 | parked |
| 193 | M-34 A gate built on a SHA-256 fingerprint, in the Guide, with its test | 1.10.0 | small | M-32, M-33 | parked |

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
| C-2a | 1.10.0 | answered (D-222, an open `cf_unit`); the card, C-2b and C-1b rewritten to it | parked |
| F-1b | 2.2.0 | answered (D-224, writing in layers, docs/plan/F-files.md); F-1a, F-1b and F-2b rewritten to it | parked |
| F-2a | 2.2.0 | answered (D-225, a writable file source); F-2a and F-2b rewritten to it | parked |
| F-3j | 2.2.0 | answered (D-226, `cf_docedit_bytes` and a thin layer, a comment option); the card rewritten to it | parked |
| F-4b | 1.10.0 | answered 2026-11-26: built only when it is asked for | parked |
| K-6a | 1.9.0 | answered (D-223, one error model, docs/plan/K-correctness.md); the card rewritten to it | parked |
| K-6b | 1.9.0 | answered (D-223, one error model, docs/plan/K-correctness.md); the card rewritten to it | parked |
| K-7b | 2.0.0 | answered 2026-11-26; still the 2.0.0 card of the ways to read, not written | parked |
| X-21 | 1.9.0 | its TO DECIDE | parked |
| X-9 | 2.1.0 | its TO DECIDE | parked |
| X-12 | 2.1.0 | its TO DECIDE | parked |
| X-13 | 2.1.0 | its TO DECIDE | parked |
| X-14 | 2.1.0 | its TO DECIDE | parked |
| L-15b | 1.9.0 | its TO DECIDE | parked |
| L-17-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| L-19-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| G-9 | 2.3.0 | its TO DECIDE | parked |
| G-9-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| P-15-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-3 | 2.2.0 | answered (D-227, a `cf_locker` that plugs in, `cf_lock_none` by default); the card rewritten to it | parked |
| Y-4 | 2.2.0 | answered (D-227, a `cf_locker` that plugs in, `cf_lock_none` by default); the card rewritten to it | parked |
| Y-5 | 2.2.0 | its TO DECIDE | parked |
| Y-11 | 2.4.0 | answered ((c), `cf_live_reader`); opens only when Y-10's page shows a knee with the cores, or a binding asks | parked |
| Y-12 | 2.2.0 | its TO DECIDE | parked |
| Y-13-m | 2.4.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-15 | 1.10.0 | its rewrite to D-221 (Y-asked.md, "Approved by the maintainer"), then any TO DECIDE left | parked |
| Y-16 | 1.10.0 | its rewrite to D-221 (Y-asked.md, "Approved by the maintainer"), then any TO DECIDE left | parked |
| Y-17 | 1.10.0 | its rewrite to D-221 (Y-asked.md, "Approved by the maintainer"), then any TO DECIDE left | parked |
| Y-18 | 1.10.0 | its rewrite to D-221 (Y-asked.md, "Approved by the maintainer"), then any TO DECIDE left | parked |
| Y-21-m | 2.2.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-22-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-23-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-24 | 1.9.0 | answered ((1), `gen build`); the card rewritten to it | parked |
| Y-26 | 1.9.0 | answered (`--include defaults`, `--include secrets`, off by default) | parked |
| Y-27 | 1.9.0 | answered (two C targets, plain `--target c` the default); the card rewritten to it | parked |
| Y-28 | 1.10.0 | answered (D-223, one error model, docs/plan/K-correctness.md); the card rewritten to it | parked |
| Y-29 | 1.10.0 | its TO DECIDE | parked |
| Y-30-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-32-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-34 | 1.10.0 | its TO DECIDE | parked |
| Y-36 | 1.10.0 | its TO DECIDE | parked |
| Y-37 | 1.10.0 | its TO DECIDE | parked |
| Y-39-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| Y-40-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-1 | 2.3.0 | the maintainer's approval; its TO DECIDE | parked |
| M-2 | 2.3.0 | the maintainer's approval | parked |
| M-3 | 2.3.0 | the maintainer's approval; its TO DECIDE | parked |
| M-4 | 2.3.0 | the maintainer's approval | parked |
| M-5 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-6 | 2.0.0 | the maintainer's approval | parked |
| M-7 | 2.0.0 | the maintainer's approval | parked |
| M-8 | 2.0.0 | the maintainer's approval | parked |
| M-9 | 2.0.0 | the maintainer's approval | parked |
| M-10 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-11a | 2.0.0 | the maintainer's approval | parked |
| M-11b | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-12 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-13 | 2.0.0 | the maintainer's approval | parked |
| M-14 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-15 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-16 | 2.0.0 | the maintainer's approval; its TO DECIDE | parked |
| M-17 | 2.0.0 | the maintainer's approval | parked |
| M-18 | 1.9.0 | the maintainer's approval | parked |
| M-19-m | 1.9.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-20 | 1.9.0 | the maintainer's approval | parked |
| M-21 | 1.9.0 | the maintainer's approval | parked |
| M-22 | 1.10.0 | the maintainer's approval; its TO DECIDE | parked |
| M-23-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-24 | 1.10.0 | the maintainer's approval | parked |
| M-25-m | 1.10.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-26 | 2.3.0 | the maintainer's approval | parked |
| M-27 | 2.3.0 | the maintainer's approval | parked |
| M-28 | 2.3.0 | the maintainer's approval | parked |
| M-29-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-30-m | 2.3.0 | the maintainer's own access, or a user's ask (the card's needs) | parked |
| M-31 | 2.3.0 | the maintainer's approval | parked |
| M-32 | 1.10.0 | the maintainer's approval; its TO DECIDE | parked |
| M-33 | 1.10.0 | the maintainer's approval; its TO DECIDE | parked |
| M-34 | 1.10.0 | the maintainer's approval | parked |

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

**Read cold once, just before the group opens** (the maintainer,
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
