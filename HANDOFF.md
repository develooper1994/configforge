# Handoff (written 2026-11-12, about 15:00)

The session ran out of usage. Do these steps in order. Read
`docs/Plan.md` ("The loop", "ACCEPT") and `docs/Agents.md` first. Delete
this file in the commit that finishes step 4.

## State

* `main` is at `178a4fc`: green, and holds this morning's work
  (systemd credentials in C, seeded name tables, `test_marker`,
  `docs/Agents.md`).
* The branch `work-2026-11-12` is ahead of `main` by two commits and is
  NOT merged, because its CI run is red for one reason only:
  * `470cb8d` - `constexpr` Keys in C++ (D-75 part A), `doctor.marker`,
    credentials in C++, Rust and Python, `docs/Plan.md`. Locally it is
    accepted: `scripts/verify.cmake` all five presets green in WSL,
    Windows 180 of 180. In CI every failing job failed on
    `gate.format.c` alone: the gate wants clang-format 22.1.8 and this
    machine has 18. The lines are in
    `include/configforge/configforge.hpp`,
    `tests/unit/test_wrapper_access.cpp`, `test_wrapper_cxx17.cpp` (and
    perhaps `test_wrapper_cxx20.cpp`).
  * `e04948b` - a new CI job, `format-patch`, that formats with the
    pinned version and uploads the difference as an artifact.

## Steps

1. `git fetch && git switch work-2026-11-12 && git pull --ff-only`.
   If `git status` shows files under `docs/plan/` that are not
   committed, keep them: step 5 is about them.

2. Get the patch CI made and apply it:

   ```sh
   id=$(gh run list --branch work-2026-11-12 --workflow ci --limit 1 --json databaseId --jq '.[0].databaseId')
   gh run view $id --json jobs --jq '.jobs[] | select(.name=="format-patch") | .conclusion'
   ```

   * `failure` is the expected answer: the patch exists.
     `gh run download $id -n format-patch -D /tmp/fp`, read
     `/tmp/fp/format.patch` (it must change only whitespace and line
     breaks in C and C++ files - if it changes anything else, STOP and
     tell the owner), then `git apply /tmp/fp/format.patch`.
   * `success` means the tree is already formatted: skip to step 3.
   * If the job itself is broken (it is new and has run once): read its
     log with `gh run view $id --log-failed`, fix the job in
     `.github/workflows/ci.yml`, push, and repeat this step.

3. Rebuild and test what the patch touched (Windows:
   `cmake --build --preset default && ctest --preset default`; or
   ACCEPT-LINUX in WSL). Commit with a message in the style of
   `git log`, push, and wait for CI:

   ```sh
   gh run watch --exit-status $(gh run list --branch work-2026-11-12 --workflow ci --limit 1 --json databaseId --jq '.[0].databaseId')
   ```

   Red for another reason: read the failing job, fix, push, repeat.
   This is the first CI run that compiles today's Rust change
   (`bindings/rust/src/lib.rs`, `sys.rs`: `Builder::credentials`), which
   could not be compiled locally - expect that it may need a fix.

4. Green: `git push origin work-2026-11-12:main`, then delete the
   branch (`git push origin --delete work-2026-11-12`, and locally after
   `git switch main && git pull --ff-only`). Delete this file in that
   same last commit before pushing to `main`, or in one more commit.

5. The plan's cards. Two architect agents were writing them when the
   session ended; they wrote straight into files, so the files exist
   only on the machine the session ran on, uncommitted, and may be
   incomplete:
   * `docs/plan/W3-native-sources.md` - the macOS user-defaults source;
     where `cf_builder_credentials` is still missing.
   * `docs/plan/H-hardening.md` - the ideas from the security pass.
   * `docs/plan/W4-binary-formats.md` - TLV, BER-TLV, CBOR.
   * `docs/plan/W5-convert-and-gen.md` - `doctor convert`,
     `doctor gen --lang`.
   For each file that exists: check every card has all the fields
   `docs/Plan.md` lists under "A card"; a card that stops mid-sentence
   is deleted. For each file that does not exist: it has to be written
   (an opus-sized job: the brief is to read the code and decide every
   name, signature, limit and error wording, so that a small model can
   carry the card out). Then fill the two places in `docs/Plan.md`
   marked `<!-- owner-list -->` and `<!-- index -->`: the index lists
   every card id in the order to do them, with its model, its needs and
   `open`. Two cards are known to be needed and belong in the index
   whether or not a file has them:
   * D-75 part B: C11 `_Static_assert` in the define macros of
     `include/configforge/configforge.h` (a default within its range, a
     literal default against `max_len`), invisible to a C89 compile.
   * D-75 part C: the code `doctor gen` writes (`tools/doctor/contract.c`)
     carries those checks and the C++ ones.
   Commit the plan as its own commit through the loop.

6. For the owner, to put in the plan's owner list:
   * Install clang-format 22.1.8 in WSL (`pipx install clang-format==22.1.8`
     needs the company's root certificate in WSL's trust store). Until
     then the `format-patch` job is the way.
   * cargo in WSL, so the Rust binding can be built before a push.
   * 1.4.1 contains a changed default (response files are opt-in,
     D-79): release it as 1.4.1 or as 1.5.0?
