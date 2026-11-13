# The plan

`docs/Roadmap.md` says what the work is and how far it has come. This
page says how to do it, one unit at a time, in enough detail that the
unit needs no design from whoever carries it out. It is written to be
followed literally by a small model: every choice that can be made in
advance is made here, and every choice that cannot is marked
**ASK THE OWNER** and is not to be guessed.

Read `docs/Agents.md` first: it is why the plan has this shape.

## The loop

Whoever works - a person, or a session of any model - does this and
nothing else:

```
 1. git fetch; git status -sb; git log --oneline HEAD..origin/main
      behind      -> git pull --ff-only, read the new commits, go to 1
      diverged    -> STOP. Tell the owner. Reset, rebase and force nothing.
      dirty tree you did not make -> STOP. Ask the owner.
 2. If HANDOFF.md exists at the root: do its steps first, then delete it
      in the commit that finishes its last step.
 3. Open the index below. Take the first card whose status is "open"
      and whose "needs" are all "done". No such card -> STOP: report
      which cards wait and on what.
 4. Card says ASK THE OWNER and the answer is not written in it -> ask,
      write the answer into the card, commit that alone, go to 3.
 5. git switch -c work/<card id>
 6. Do the card's steps in order. Never skip "see the test fail".
 7. Run ACCEPT (below). Not exit 0 -> fix, go to 7. After three tries
      on the same failure -> STOP: revert the unit (git restore, git
      clean of your new files), write what failed into the card under
      "Notes", commit that, report.
 8. Commit: the card's files, CHANGELOG.md, docs/Roadmap.md, and this
      index with the card's status set to "done" and the commit named.
 9. git push -u origin work/<card id>; wait for CI (ACCEPT-CI).
      red -> read the failing job's log, fix, commit, push, go to 9.
10. Green: git push origin work/<card id>:main; delete the branch here
      and on the remote. Go to 1.
```

A session that must stop between 5 and 10 writes `HANDOFF.md` (Agents.md,
"Stopping without losing the work"), commits it on the work branch and
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

Three failures that are not the code, so that they are recognised:

* `gate.format.c` in CI and not locally: the local clang-format is not
  22.1.8. Rewrite the lines CI names so that no version moves them
  (Agents.md, rule 6); do not argue with the formatter.
* `doctor.api` "sixteen times the keys": it measures time. Rerun it
  alone before believing it.
* `gate.apiref`: a header comment changed. Run
  `cmake -P scripts/generate/apiref.cmake` and stage `docs/API.md`.

## A card

Every unit of work is a card in a file under `docs/plan/`. A card has
these fields and no others:

| Field | Holds |
| ----- | ----- |
| **id** | e.g. `W4-2`. The branch is `work/W4-2`. |
| **goal** | One sentence: what is true when it is done. |
| **why** | What it does for a value being able to say why it is what it is, or for the safety of reading it. |
| **needs** | The ids that must be done first, or "nothing". |
| **model** | The smallest that can do it: haiku, sonnet or opus (Agents.md has the table). A larger one may always take it. |
| **decided** | Every design choice already made: names, signatures, error wordings, limits, what was rejected and why. This is the part a small model cannot supply. |
| **files** | The files to create or change, and the shared ones to touch last. |
| **steps** | Numbered. Each is one action with its command. The test comes before the code, and "see it fail" is its own step. |
| **accept** | Which ACCEPT applies, plus any check particular to the card. |
| **stop if** | The conditions under which nothing is changed and the card is handed back. |
| **docs** | The exact documents to update: the D-number to add, the CHANGELOG heading, the Roadmap row. |
| **notes** | Empty until someone fails at it; then what failed, dated. |

A card that turns out wrong - its "decided" cannot be built as written -
is not repaired by whoever is carrying it out. That is a "stop if": the
unit is reverted, the obstacle is written under "notes", and the card
waits for a session able to redesign it. Following a wrong card
faithfully is the dearest mistake this plan can make.

## For the owner

Things only the owner can do or decide. Each unblocks the cards named.

* **Answered, 2026-11-12, for every card written so far**: where a card
  says ASK THE OWNER, the answer is the card's own recommendation. That
  covers H-5 (a group may write a secrets directory only when it is its
  owner's private group), H-7 (an undefined key within "did you mean"
  distance of a sensitive key is kept as that secret), H-8a (the digest
  key comes only from the program; FNV-1a stays the default), W3-F2b
  (the scope it recommends) and W5-G2 (as it recommends). A card whose
  question is new, or whose recommendation cannot be built, still waits.
* **The order**, 2026-11-12: the library's own code first - what reads,
  bounds and explains a value - and the bindings after it, since a
  binding follows once the C is settled. One problem at a time, carried
  to done before the next is opened.
* 1.4.1 holds a changed default (response files are opt-in, D-79): it
  is released as 1.5.0 or as 1.4.1 - not decided.

## The index

In the order to be done. A card is `open`, `done` (with its commit), or
`waits` (with what it waits for).

| Order | Card | Model | Needs | Status |
| ----: | ---- | ----- | ----- | ------ |
| 1 | H-1 C11's define macros, checked by the compiler | sonnet | - | done (cda7744) |
| 2 | H-3 What one document may make, bounded in the flattener | sonnet | - | done (fbb833c) |
| 3 | H-4 The fuzzers fail a document that costs a multiple of its size | sonnet | H-3 | done (febac60) |
| 4 | W4-C1 The CBOR reader | sonnet | - | done (85608b9) |
| 5 | W4-C2 CBOR held to RFC 8949's vectors | sonnet | W4-C1 | done (e3dd16e) |
| 6 | W4-C3 Writing CBOR | opus | W4-C1 | done (aabeeca) |
| 7 | W4-C4 CBOR from content, fuzzed and documented | sonnet | W4-C2, W4-C3 | done |
| 8 | W4-T1 A codec that carries its own data | opus | - | done |
| 9 | W4-T2 A TLV layout, and reading a flat one | opus | W4-T1 | done |
| 10 | W4-T3 Nesting, lists and unknown tags | sonnet | W4-T2 | done |
| 11 | W4-T4 BER-TLV | sonnet | W4-T3 | done |
| 12 | W4-T5 TLV fuzzed and documented | sonnet | W4-T4, W4-C4 | open |
| 13 | H-5 A directory others can write to is not read as secrets | opus | - | open |
| 14 | H-7 An undefined key named like a secret is kept as one | opus | - | open |
| 15 | H-8a A secret's digest that cannot be tested without a key | opus | - | open |
| 16 | W5-V1 `doctor convert` | opus | W4-C3 | open |
| 17 | W5-S1 `doctor schema --from` | sonnet | W5-V1 | open |
| 18 | H-2 The contract `doctor gen` writes, checked by the compiler | sonnet | H-1 | open |
| 19 | H-6 Doctor's `--redact-all`, on in the action unless asked | sonnet | - | open |
| 20 | H-8b Doctor tells digests of different keys apart | sonnet | H-8a | open |
| 21 | W3-F2a The platform capability for macOS's user defaults | sonnet | - | open |
| 22 | W3-F2b `cf_builder_preferences`: the source, off macOS | sonnet | W3-F2a | open |
| 23 | W3-F2c Reading CFPreferences on macOS (CI-only) | opus | W3-F2b | open |
| 24 | W3-F3 `cf_builder_credentials` in every language, the Guide, an example | sonnet | - | open: the bindings have it (470cb8d); the card's rest is to check |
| 25 | W3-F2d User defaults in C++, Rust, Python and the Guide | sonnet | W3-F2c | open |
| 26 | W5-G1 `doctor gen --lang cpp` | opus | W5-S1 | open |
| 27 | W5-G2 `doctor gen --lang rust` | opus | W5-G1 | open |
| 28 | W5-G3 `doctor gen --lang python` | sonnet | W5-G2 | open |

Not yet cards: wave 6 (every operation on a file's data, written back
in its format), wave 7 (Go and .NET), wave 8 (2.0: the stream reader
and the dynamic sizing modes), proofs with CBMC, `.properties` against
Java's own reader.
