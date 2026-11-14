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
 8. Commit: the card's files, CHANGELOG.md, docs/Roadmap.md, and this
      index with the card's status set to "done" and the commit named.
 9. git push -u origin work/<card id>; wait for CI (ACCEPT-CI).
      red -> read the failing job's log, fix, commit, push, go to 9.
10. Green: git push origin work/<card id>:main; delete the branch here
      and on the remote. Go to 1.
```

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

Three failures that are not the code, so that they are recognised:

* `gate.format.c` in CI and not locally: the local clang-format is not
  22.1.8. Rewrite the lines CI names so that no version moves them
  (no string literal continued over lines as a call\'s argument, no
  condition over more than one line); do not argue with the formatter.
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
| **size** | The smallest that can do it: small (mechanical), medium (a specified change with its tests) or
large (a design, an interface, anything subtle). A larger one may always take it. |
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
* 1.4.1 holds a changed default (response files are opt-in, D-79): it
  is released as 1.5.0 or as 1.4.1 - not decided.

## The index

In the order to be done. A card is `open`, `done` (with its commit), or
`waits` (with what it waits for).

| Order | Card | Size | Needs | Status |
| ----: | ---- | ----- | ----- | ------ |
| 13 | H-5 A directory others can write to is not read as secrets | large | - | open |
| 14 | H-7 An undefined key named like a secret is kept as one | large | - | open |
| 15 | H-8a A secret's digest that cannot be tested without a key | large | - | open |
| 16 | W5-V1 `doctor convert` | large | W4-C3 | waits: the tools split (Roadmap, "Tools, distribution and digests") |
| 17 | W5-S1 `doctor schema --from` | medium | W5-V1 | waits: the tools split (Roadmap, "Tools, distribution and digests") |
| 18 | H-2 The contract `doctor gen` writes, checked by the compiler | medium | H-1 | open |
| 19 | H-6 Doctor's `--redact-all`, on in the action unless asked | medium | - | open |
| 20 | H-8b Doctor tells digests of different keys apart | medium | H-8a | open |
| 21 | W3-F2a The platform capability for macOS's user defaults | medium | - | open |
| 22 | W3-F2b `cf_builder_preferences`: the source, off macOS | medium | W3-F2a | open |
| 23 | W3-F2c Reading CFPreferences on macOS (CI-only) | large | W3-F2b | open |
| 24 | W3-F3 `cf_builder_credentials` in every language, the Guide, an example | medium | - | open: the bindings have it (470cb8d); the card's rest is to check |
| 25 | W3-F2d User defaults in C++, Rust, Python and the Guide | medium | W3-F2c | open |
| 26 | W5-G1 `doctor gen --lang cpp` | large | W5-S1 | waits: the tools split (Roadmap, "Tools, distribution and digests") |
| 27 | W5-G2 `doctor gen --lang rust` | large | W5-G1 | waits: the tools split (Roadmap, "Tools, distribution and digests") |
| 28 | W5-G3 `doctor gen --lang python` | medium | W5-G2 | waits: the tools split (Roadmap, "Tools, distribution and digests") |

Done and released in 1.5.0, their cards removed: H-1, H-3, H-4 and the
whole of wave 4 (CBOR; TLV by a layout and BER-TLV).

Not yet cards: the Roadmap's "More formats, and what a format is"
(MessagePack first, then BSON, UBJSON, property lists, HCL, Avro,
Protobuf, TLV as embedded devices write it, a benchmark, a stream);
the cumulative C99 and C23 layers; wave 6 (every operation on a file's data, written back
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
set up once on each registry; see "Distribution, as infrastructure"), a
smaller 1.x promise (internals fenced, an experimental tier) together
with the serialization layer as a library target of its own (Roadmap,
"More formats": before the first new format), the
platform's file locations with profiles, misreads noticed, help and
completions from the contract, durations and sizes, big-endian, 32-bit
and bare-metal CI, value migrations, outside trust checks. From "Tools,
distribution and digests, 2026-11-14": Doctor kept to reports, with
`configforge gen` and the serialization library's own tool beside it
behind one `configforge` front door (wave 5's cards are rewritten for
it, and wait until then); distribution and being found (the WebAssembly
playground first); one digest a purpose, SHA-256 for a fingerprint that
decides, and a digest the program gives; a report asked of a running
program; the OpenTelemetry and OPA recipes; a "Start here" page a
language.
