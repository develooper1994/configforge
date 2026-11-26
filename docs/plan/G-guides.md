# G - documents a reader can use

This group is for the reader who is not the author: documents short enough
to open (G-1a, G-1b, G-2), a page that takes a newcomer from the build to
`explain()` in each language (G-3 to G-6), the recipes that remain (G-7),
a playground on the documentation site (G-8) and the study of a program
not written for the library (G-9). Each card is read cold against the tree
before it opens (docs/Plan.md, "Read again after the layout changes"). The
gates that check documents are `gate.links` (lychee, local links), `gate.codemap`, `gate.limits`,
`codespell` and, in CI's `docs` workflow, `mkdocs build --strict` followed by
`scripts/check/check_docs.py`. Line numbers are those of `main` at 94602a2.

Order: G-1a, G-1b, G-2 (documents; G-1b before G-2 because both edit
`mkdocs.yml`), then G-3 to G-6 (G-3 first: it builds the gate the others
use), G-7, G-8, G-9. G-1a, G-1b and G-2 belong to 1.9.0 (the Roadmap places
them in no later row, and they change no code); the rest belong to 2.3.0
(docs/Maintaining.md:109, the row "more languages, and being found").
For a 2.3.0 card, the CHANGELOG heading is `## 2.3.0 — unreleased`: if
`CHANGELOG.md` lacks it, create it directly above `## 1.9.0 — unreleased`
with the same sub-headings. A new decision after G-1b is a new file
`docs/decisions/D-<n>.md` (its first line `## D-<n> — <title>`, the shape of
docs/Decisions.md:11-26) and a row in the index of `docs/Decisions.md`.
Before G-1b it is a section appended to `docs/Decisions.md` and a row in the
index (docs/Decisions.md:30-132).

## G-1a - The tools that read a decision follow the split

* **id**: G-1a
* **goal**: every tool that finds a decision by its number finds it in `docs/decisions/D-<n>.md` when that file exists and in `docs/Decisions.md` when it does not, and a new gate `gate.decisions` fails when the index and the files disagree; nothing is split yet.
* **why**: G-1b moves 111 decisions out of a 5,326-line file. If the tools learn the new place first, the split is one mechanical commit that cannot break a card brief or the limits gate, and each card lands alone.
* **needs**: nothing
* **size**: medium
* **decided**:
  * **Layout (D-159, text below).** One decision, one file: `docs/decisions/D-<n>.md`, `<n>` exactly as the heading writes it (`D-01`, `D-70`, `D-112`; two digits at least, as now). The file begins with the decision's own `## D-<n> — <title>` line and holds its text unchanged. `docs/Decisions.md` keeps its title, "How to Use This Document" and "Decision Index" (lines 1-133); each index row's ID cell becomes a link `[D-01](decisions/D-01.md)`; the sections themselves leave. A decision not in the index (D-18 to D-22 are absent from it: check) is a G-1b stop.
  * **The five readers, and how each follows.**
    1. `scripts/work/brief.py:70-90` `resolve`: for `decision`, `path` is `docs/decisions/<decision>.md` if that file exists, else `docs/Decisions.md`; with the new file the section is the whole file (the existing `sections(lines)` walk finds its `## ` heading, so the loop and the `re.match(rf"^{decision}\b", title)` test stay as they are). A card that writes `D-7` for the heading `D-07` is looked up as `D-07` as well (try the name as written, then padded to two digits). The label stays `docs/Decisions.md, D-70` (tests/work/test_brief.py:29 expects it): keep it so a brief reads the same before and after.
    2. `cmake/gates/GateLimits.cmake:24-34`: a reference `D-<n>` in `docs/Limits.md` is satisfied by `docs/decisions/D-<n>.md` existing, or by the old `## D-<n>` heading in `docs/Decisions.md`. Error wording kept: "docs/Limits.md names D-<n>, which docs/Decisions.md does not have".
    3. `scripts/generate/limits.cmake:430`: the generated link `[D-109](Decisions.md)` becomes `[D-109](decisions/D-109.md)` only in G-1b (the generator writes `docs/Limits.md`, which `gate.limits` compares byte for byte). In G-1a nothing changes here.
    4. `docs/Maintaining.md:265`, "`grep -n "^## D-<n>" docs/Decisions.md`": G-1b rewrites it to `cat docs/decisions/D-<n>.md`. `CLAUDE.md` and `CONTRIBUTING.md:46,113,290` name `docs/Decisions.md` as the place and stay true (the index is there).
    5. `mkdocs.yml:57` (`Decisions: docs/Decisions.md`): G-1b adds the files to the site (below).
  * **`gate.decisions`.** New script `scripts/check/decisions.py`, run as `ctest -R gate.decisions` (registered beside `gate.limits`, `cmake/ConfigForgeGates.cmake:112`, with `python3` found as the other python gates find it, `grep -n "find_package(Python\|Python3_EXECUTABLE" cmake/ConfigForgeGates.cmake`). With no `docs/decisions/` it passes (G-1a). With it: every `docs/decisions/D-*.md` starts with `## ` and the same `D-<n>` as its file name; every file is linked from the index exactly once; every index row's link target exists; no `## D-` heading remains in `docs/Decisions.md`; the first line of failure is `decisions: <file>: <reason>`, exit 1.
  * **D-159, to be appended to `docs/Decisions.md` (and its index row, status **Decided**, blocks "documents"):** "## D-159 — One decision, one file. Question: how does a reader open one decision of 111 without scrolling past the rest? Options: keep one file and add an outline (the site already has one; the file stays 5,000 lines for an agent that reads by line); split by theme (a decision belongs to several themes); one file per decision. Trade-off: one file per decision costs 111 files and a link rewrite; it makes a decision's address its path, so a card cites it exactly and a worker reads it alone. Recommendation and decision: one file per decision, `docs/decisions/D-<n>.md`, the index kept in `docs/Decisions.md` with a link per row; the tools that find a decision by number (the card brief, the limits gate) follow, and `gate.decisions` keeps the index and the files together. Rejected: a generated single file (it would be the file this removes). Consequence: a new decision is a file and an index row."
* **files**: `scripts/work/brief.py`, `tests/work/test_brief.py`, `cmake/gates/GateLimits.cmake`, `scripts/check/decisions.py` (new), `tests/work/test_decisions.py` (new), `cmake/ConfigForgeGates.cmake`, `docs/Decisions.md` (D-159 and its row), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-1a main`.
  2. Test first, `tests/work/test_brief.py`: add a second fixture decision in `docs/decisions/D-71.md` (heading `## D-71 — Split`, one body line) and a card citing it; expect `--- docs/Decisions.md, D-71` and the body line. Beside it, in the same fixture, keep D-70 in `docs/Decisions.md` (the fallback). Read the fixture builder at `tests/work/test_brief.py:1-20` first.
  3. See it fail: `python -m unittest tests.work.test_brief 2>&1 | tail -n 20` (or the command `scripts/README.md` gives for these tests) fails on D-71.
  4. Change `resolve` in `scripts/work/brief.py:70-90` as decided; run the test: passes, and D-70 still passes.
  5. Test first for the gate, `tests/work/test_decisions.py`: a temp tree with `docs/Decisions.md` (index rows linking D-01, D-02) and `docs/decisions/D-01.md`, `D-02.md` passes; each of these fails with its message: a file missing from the index, a link to a missing file, a file whose heading number differs from its name, a `## D-03` left in `docs/Decisions.md`. See them fail (the script does not exist), then write `scripts/check/decisions.py` (stdlib only, Python 3.8 syntax) until they pass.
  6. `GateLimits.cmake`: replace the `if(NOT decisions MATCHES ...)` test (line 28) with one that also accepts `EXISTS "${SRC}/docs/decisions/${ref}.md"`. See it fail first: create `docs/decisions/D-109.md` temporarily containing `## D-109`, delete D-109's heading from a scratch copy of `docs/Decisions.md` via `-DSRC=<copy>` and run `cmake -DSRC=<copy> -DBIN=<tmp> -P cmake/gates/GateLimits.cmake` (read the script's header for its arguments); it must pass after the change and fail before.
  7. Register `gate.decisions` in `cmake/ConfigForgeGates.cmake` beside `gate.limits`; `cmake --preset default && ctest --preset default -R "gate\.(decisions|limits)"`.
  8. Append D-159 and its row; CHANGELOG under "## 1.9.0 — unreleased", "### Added": "`gate.decisions`; the card brief and `gate.limits` find a decision in `docs/decisions/` (G-1a, D-159)". Roadmap: the item "Documents kept short" stays ⬜ until G-2 (G-1a is half of it): add one line "G-1a: the tools follow; G-1b: the split; G-2: the finished items" under it.
  9. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake`; `cmake -P scripts/generate/apiref.cmake`; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `python -m unittest tests.work.test_brief tests.work.test_decisions` exit 0; `ctest --preset default -R "gate\.(decisions|limits)"` exit 0.
* **stop if**: `gate.limits` cannot be made to accept both places without rewriting `limits.cmake`'s output; a tool other than the five finds a decision by a line number into `docs/Decisions.md` (`grep -rn "Decisions\.md[:#(]" docs scripts cmake tests` returns more than docs/Roadmap.md-style prose): list it under notes and stop; three failures on the same thing.
* **docs**: D-159 (text above); CHANGELOG "## 1.9.0 — unreleased" "### Added"; Roadmap "Documents kept short" (the line above).

## G-1b - Decisions.md split into one file a decision

* **id**: G-1b
* **goal**: `docs/Decisions.md` is the preface and the index (about 140 lines), every decision's text is in `docs/decisions/D-<n>.md` unchanged but for its links, the site and the tools use the new places, and `gate.decisions` is green on the real tree.
* **why**: a reader opens one decision, not a 5,000-line file. A worker's brief names `D-<n>` and receives only it (CLAUDE.md "Tokens": Decisions.md was a quarter of what workers read).
* **needs**: G-1a
* **size**: medium (a script does the move; the rest is five small edits)
* **decided**:
  * **The splitter** is `scripts/docs/split_decisions.py`, run once and kept (so the commit shows how). It reads `docs/Decisions.md`; a decision is the text from a `## D-<n>` heading (outside code fences: reuse the fence rule of `scripts/work/brief.py:20-34`, import `sections`) to the next `## ` heading at the same level; text before the first and after the last decision belongs to the preface (the last decision's end is the file's end: check that `docs/Decisions.md` ends with `D-112`'s text, 5,076 onward; the closing remark at the file's end, "Phase 0 ends when ...", belongs to the preface and is kept at the foot of the index file).
  * **Links.** In a moved file, `](Design.md)` and every relative link that does not start with `http`, `#` or `../` gains `../` (the file is one directory deeper); `](#d-<n>...)` anchors to another decision become `](D-<n>.md)`; an anchor into the same decision stays. The splitter lists every link it changed and every `#anchor` it could not resolve; the unresolved are fixed by hand in the same commit (a handful is expected). `mkdocs build --strict` has `validation.links` at `warn` for anchors (mkdocs.yml:44-47): after the split the build must have no new warning.
  * **Index.** In `docs/Decisions.md`, each row's ID cell `D-01` becomes `[D-01](decisions/D-01.md)`; nothing else in the row changes. D-18 to D-22 and any decision with a section and no row: the splitter stops and prints them; G-1b adds the row with status read from the section's "Status" line (stop if there is none).
  * **Site.** `mkdocs.yml` nav under Design keeps `Decisions: docs/Decisions.md`; the files are found by links alone (`scripts/docs/docs_site.py` already maps `docs/` into `docs/` of the site). `mkdocs build --strict` warns of pages in the docs dir that the nav omits: set `validation.nav.omitted_files: info` if it does (read `mkdocs.yml:40-47` for the key's place); do not list 111 files in the nav.
  * **Tool follow-up.** `scripts/generate/limits.cmake:430` writes `](decisions/D-109.md)`; regenerate `docs/Limits.md` (`cmake -P scripts/generate/limits.cmake`: read its header for arguments; `gate.limits` compares). `docs/Maintaining.md:265`: "`cat docs/decisions/D-<n>.md`". `docs/Maintaining.md:255` (the token measurement) is history: unchanged. `docs/Codemap.md` has no entry for markdown: regenerate and check `git diff --stat docs/Codemap.md` is empty or lines only.
  * **Other documents' citations.** `grep -rn "Decisions.md" docs README.md CONTRIBUTING.md .github CLAUDE.md AGENTS.md` shows prose citations ("D-36 in `docs/Decisions.md`", CONTRIBUTING.md:290): each stays true because the index is in that file and names the decision's link; leave them. A citation by line number into Decisions.md (`Decisions.md:<n>`) is rewritten to the decision's file: the grep earlier found only docs/Design.md's prose at the cited text "with the decision in Decisions.md" (line 683), left as it is.
  * **Cards and notes** in `docs/plan/*.md` cite `docs/Decisions.md:<range>`: `grep -rn "Decisions\.md:[0-9]" docs/plan HANDOFF.md`; each such citation is rewritten to `docs/decisions/D-<n>.md` by hand (the range's decision is the heading above it in the old file; `git show main:docs/Decisions.md | sed -n <range>p` finds it). If more than 25 are found, stop (a split card for the citations).
* **files**: `scripts/docs/split_decisions.py` (new), `docs/decisions/*.md` (111 new), `docs/Decisions.md`, `scripts/generate/limits.cmake`, `docs/Limits.md` (generated), `docs/Maintaining.md`, `mkdocs.yml`, `docs/plan/*.md` (citations), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-1b main` (after G-1a has landed).
  2. `python scripts/work/work.py` is not needed. Record the oracle first: `git show main:docs/Decisions.md | grep -c "^## D-"` (the count of sections) and `git show main:docs/Decisions.md | wc -c`.
  3. Write `scripts/docs/split_decisions.py` with a `--check` mode that, from the new files, reassembles the old text (preface + each file in index order with the link rewrite undone) and compares character counts within the rewritten-link difference: it prints `decisions: <n> moved, <m> links rewritten, <k> anchors unresolved`.
  4. Test first, `tests/work/test_split_decisions.py`: a fixture `Decisions.md` of three decisions (one with a fenced block containing a `## D-99` line, one with a link `](Design.md)` and one with `](#d-02-title)`); expect three files, the fenced line kept inside its decision, the links rewritten, the index linked. See it fail, write the script, see it pass.
  5. Run it on the real file: `python scripts/docs/split_decisions.py docs/Decisions.md docs/decisions`. Expect the count of step 2.
  6. `python scripts/check/decisions.py` exit 0. `git diff --stat` shows one rewritten `Decisions.md` and 111 new files.
  7. The tool follow-up of "decided": `limits.cmake`, regenerate `docs/Limits.md`, `Maintaining.md:265`, the citations in `docs/plan/*.md`, `mkdocs.yml` if needed.
  8. `python -m mkdocs build --strict 2>&1 | tail -n 30` (the docs environment: `scripts/docs/requirements.txt`; skip with a note where mkdocs is not installed: CI's `docs` workflow judges); `ctest --preset default -R "gate\.(decisions|limits|links|codemap)"`.
  9. `python scripts/work/brief.py G-1b | grep -c "D-159"` shows the brief still resolves a decision (the card names D-159 in "decided"); and a brief for any card that cites an old decision (`grep -ln "D-100" docs/plan/*.md`, then `python scripts/work/brief.py <that card id> | grep -c "^## D-100"`) is 1.
  10. CHANGELOG "## 1.9.0 — unreleased" "### Changed": "`docs/Decisions.md` is the index; each decision is `docs/decisions/D-<n>.md` (G-1b, D-159)". Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI, with the `docs` workflow green (mkdocs `--strict` and `check_docs.py`); `ctest --preset default -R "gate\.(decisions|limits|links|codemap)"` exit 0; the section count of step 2 equals `ls docs/decisions | wc -l`.
* **stop if**: more than 10 anchors are unresolved or the count of step 2 differs from the files made; `mkdocs build --strict` fails on a link the splitter could not decide; the `docs` workflow was red on `main` before the card (read `gh run list --workflow docs --branch main --limit 1`); three failures on the same thing.
* **docs**: no new D-number (D-159 is G-1a's); CHANGELOG as above; Roadmap: "Documents kept short" stays ⬜ with the line "G-1b done" (G-2 marks it 🟩).

## G-2 - The Roadmap's finished items on a page of their own

* **id**: G-2
* **goal**: `docs/Done.md` holds every Roadmap item marked 🟩 (the detail of each, with its heading), `docs/Roadmap.md` holds what is left, and nothing that was written is lost.
* **why**: the Roadmap is 1,565 lines and a card's author needs the part that is not finished; the "At a glance" table already says what is done in one row (docs/Roadmap.md:29-74).
* **needs**: G-1b (both change `mkdocs.yml`; G-2 also states the Done page's place in the same pattern)
* **size**: medium
* **decided**:
  * **What moves.** Whole items marked 🟩 in the detail sections, not in "At a glance": a bullet that begins `* 🟩 **` with its indented continuation, and a heading that begins `### 🟩` (for example `docs/Roadmap.md:637`) with everything to the next heading. A 🟨 item stays whole, with the text saying what is done and what remains (the maintainer's rule at docs/Roadmap.md:7-10). ✅ items already leave at a release (docs/Roadmap.md:26-28): the Done page is for 🟩 and is emptied of an item when it is released, as the Roadmap is.
  * **Where.** `docs/Done.md`, title `# Done, not yet released`, the preface "Items of the Roadmap marked 🟩: on `main` and verified, not in a published version. Each keeps the heading it had; the Roadmap's row in "At a glance" links here", then the moved items grouped under the heading they came from (`## <Roadmap's ## or ### heading>` then the item). The order inside is the Roadmap's order.
  * **What stays in the Roadmap.** "At a glance" keeps every row (it is the whole list in one table); a 🟩 row's last cell gains a link to the item's anchor on the Done page when the cell has none. In place of a moved item the Roadmap keeps nothing (the row says where it went). The "Marks" paragraph (docs/Roadmap.md:26-28) says: "An item marked 🟩 moves to docs/Done.md; one marked ✅ moves to the CHANGELOG at the next release and leaves".
  * **A mover that can be rerun.** `scripts/docs/move_done.py`, stdlib, idempotent (a second run moves nothing); with `--check` it exits 1 when a 🟩 item is still in the Roadmap. Wired into `scripts/work/work.py`? No: it is run by hand when an item turns 🟩 (the Roadmap's own rule moves it in the same commit as the mark); a line in docs/Maintaining.md under "Working habits" says so.
  * **Citations.** `grep -rn "Roadmap.md:[0-9]" docs HANDOFF.md CLAUDE.md AGENTS.md scripts` cites Roadmap lines (the cards cite `docs/Roadmap.md:1514-1523`, P-3): every moved item changes the numbers below it. The cards' citations are the cold reader's concern (docs/Plan.md "Read again after the layout changes"): the card lists them in its notes' report and fixes the ones whose text moved; a citation into a part that stayed is rewritten to the new line numbers by `git diff`'s hunk offsets, by hand. More than 40 citations: stop.
* **files**: `scripts/docs/move_done.py` (new), `tests/work/test_move_done.py` (new), `docs/Done.md` (new), `docs/Roadmap.md`, `mkdocs.yml` (nav: under Contributing, after Roadmap: `Done: docs/Done.md`), `docs/Maintaining.md`, `docs/plan/*.md` and `HANDOFF.md` (citations), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/G-2 main`.
  2. Count first: `grep -c "^\* 🟩\|^### 🟩" docs/Roadmap.md` and `wc -l docs/Roadmap.md`; write both in the commit message.
  3. Test first, `tests/work/test_move_done.py`: a fixture Roadmap with a 🟩 bullet (two continuation lines), a 🟨 bullet, a `### 🟩 Heading` section and an "At a glance" row; expect the 🟩 pieces in the fixture Done page under their parent headings, the 🟨 bullet unmoved, the table untouched but for the link, a second run changing nothing, `--check` exit 1 before and 0 after. See it fail, write the script, see it pass.
  4. Run on the real file; `git diff --stat docs/Roadmap.md docs/Done.md`: the lines removed from the Roadmap equal the lines added to Done.md minus the headings and preface (print both in the script's last line).
  5. Edit the "Marks" paragraph and `Maintaining.md` as decided; add the nav line.
  6. Rewrite the citations as decided; `ctest --preset default -R "gate\.(links|codemap)"`; `python -m mkdocs build --strict 2>&1 | tail -n 20`.
  7. Roadmap: mark "Documents kept short" 🟩 (it moves itself by the new rule: run the mover once more). CHANGELOG "## 1.9.0 — unreleased" "### Changed": "The Roadmap's finished items are on `docs/Done.md` (G-2)". Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `python scripts/docs/move_done.py --check` exit 0; `python -m unittest tests.work.test_move_done` exit 0.
* **stop if**: an item marked 🟩 has a part that another item cites by heading and the heading is gone; more than 40 citations; three failures on the same thing.
* **docs**: no D-number; CHANGELOG as above; Roadmap: "Documents kept short" 🟩 (its heading text: "Documents kept short.").

## G-3 - "Start here" in C, and the gate that keeps every such page true

* **id**: G-3
* **goal**: `docs/start/c.md` takes a reader from the build to `explain()` in C in under 100 lines, every code block in it is a file that is built and run by a test, and `gate.start` fails when a page's block and its file differ.
* **why**: Design.md, Decisions.md and API.md are 11,000 lines between them; the Guide is 1,700. A newcomer needs the one path that works, and a page whose code is not tested rots (docs/Roadmap.md:942-947).
* **needs**: nothing (G-1b first only for `mkdocs.yml`: if G-1b has not landed, edit the same nav lines)
* **size**: medium
* **decided**:
  * **Pages.** `docs/start/c.md`, `cpp.md`, `rust.md`, `python.md` (G-4 to G-6), and `docs/start/README.md`? No: an index page `docs/start/index.md` is not made; the nav has "Start here" with the four pages, and the README's and the Guide's first lines link to them (G-3 adds the C link and the "Start here" nav group; G-4 to G-6 add theirs).
  * **Form of a page (D-160, below).** Six headings, the same in every language: "1. Get it" (the one build line from Guide section 1, `docs/Guide.md:55-96`, for that language), "2. A contract" (a key with a default and a range), "3. Read a value", "4. Override it", "5. Ask why" (`explain()`; the output is the real output, copied from the test), "6. Where next" (three links into the Guide: sections 3, 5 and 7). No page explains a concept: a link does. A page carries one program of at most 40 lines, shown in pieces; the pieces together are that file, in order.
  * **The file is the source (D-160).** The page includes the example file by marked regions: in the example, `/* start:2 */ ... /* end:2 */` (C, C++, Rust: `// start:2`, `// end:2`; Python: `# start:2`, `# end:2`); in the page, a fence whose first line after the opening is `<!-- from examples/c/start.c start:2 -->`. `gate.start` (below) checks that the fence's text equals the region's, line for line, trailing blanks ignored. Output blocks are fenced `text` and begin `<!-- output examples/c/start.c -->`; the gate compares them with the example's expected output file `examples/c/start.expected` (the test's own recorded output).
  * **The C example.** `examples/c/start.c`, built by `examples/c/CMakeLists.txt:2` (add `start` to `foreach(ex tier0 tier1 explain_demo)`) and run by the existing `examples/expect.cmake` test with the regex that proves `explain()` printed its source (read how `explain_demo` is registered, `grep -n "explain_demo" examples/c/CMakeLists.txt examples/CMakeLists.txt`). Base it on `examples/c/explain_demo.c` (43 lines; its header comment shows the three ways to run it); use only functions that file or `docs/Guide.md:98-123` (section 2) already uses, so no new API is invented here; if a needed function is not there, stop.
  * **`gate.start`.** `scripts/check/start.py` (stdlib, Python 3.8), `ctest -R gate.start`, registered beside the docs gates in `cmake/ConfigForgeGates.cmake`. Walks `docs/start/*.md`, finds every `<!-- from <file> start:<n> -->` and `<!-- output <file> -->`, and compares. Failure line: `start: <page>:<line>: block differs from <file> start:<n>`. A page with no marker fails (`start: <page>: no code is tied to a file`). With a language whose pages are not yet written, nothing is checked: the gate passes on an empty `docs/start/`.
  * **D-160, text to be added (a file `docs/decisions/D-160.md` and an index row, or after G-1b's state):** "## D-160 — A page's code is a tested file. Question: how does a short guide stay true? Options: code typed into the page; code extracted from the page into a test; the page includes regions of a file that a test builds and runs. Trade-off: the first rots; the second makes the test the page's hostage; the third keeps one source and one check. Decision: the page includes marked regions of an example, and `gate.start` compares them; the example is built and run by its own test, and the page's output block is the example's recorded output. Consequence: a change to an example that changes a region fails `gate.start` until the page follows."
* **files**: `examples/c/start.c` (new), `examples/c/start.expected` (new), `examples/c/CMakeLists.txt`, `examples/CMakeLists.txt` (if the test is registered there), `docs/start/c.md` (new), `scripts/check/start.py` (new), `tests/work/test_start.py` (new), `cmake/ConfigForgeGates.cmake`, `mkdocs.yml`, `README.md` (one line), `docs/Guide.md` (one line near line 1-25), `docs/Decisions.md` and its row (or `docs/decisions/D-160.md`), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-3 main`.
  2. Test first, `tests/work/test_start.py`: a temp tree with `docs/start/x.md` and `examples/x.c`; cases: block equal passes; one line changed fails with the message above; a missing region fails; a page with no marker fails; an output block that differs from `.expected` fails; an empty `docs/start/` passes.
  3. See it fail (no script), write `scripts/check/start.py`, see it pass: `python -m unittest tests.work.test_start`.
  4. Write `examples/c/start.c` (at most 40 lines, regions 1 to 5 as the page's headings 2 to 5 need) from `explain_demo.c`. Build and run: `cmake --preset default && cmake --build --preset default --target start && ctest --preset default -R "example.*start"`; record the output into `examples/c/start.expected` (strip no line; the example prints no time or path that varies, else the example is changed so it does not).
  5. Write `docs/start/c.md` by the form above. The first line after the title: "Five minutes, from the build to a value and the reason it has that value. Everything here runs: the code is `examples/c/start.c`."
  6. Register `gate.start`; `ctest --preset default -R "gate\.start"` exit 0. See it fail once: change one character in a region of the example, run, restore.
  7. Add the nav group "Start here" (`mkdocs.yml`, before Guide) with `C: docs/start/c.md`; the README and Guide lines; D-160; CHANGELOG "## 2.3.0 — unreleased" "### Added": "`docs/start/c.md`, a page from the build to `explain()` in C, its code tested, and `gate.start` (G-3, D-160)"; Roadmap: item "Start here, one page a language." is 🟨 (C done; C++, Rust, Python are G-4 to G-6): rewrite its marker, keep the text.
  8. `python -m mkdocs build --strict 2>&1 | tail -n 20` (where installed); format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `ctest --preset default -R "gate\.start|example"` exit 0; `python -m unittest tests.work.test_start` exit 0.
* **stop if**: no function the example needs is in the Guide's section 2 or `explain_demo.c`; the example's output varies between runs; three failures on the same thing.
* **docs**: D-160 (text above); CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap "Start here, one page a language." (🟨).

## G-4 - "Start here" in C++

* **id**: G-4
* **goal**: `docs/start/cpp.md` is G-3's page in C++, its code `examples/cpp/start.cpp`, checked by `gate.start`.
* **why**: as G-3, for the C++ reader, who is told in the Guide to read section 13 after twelve others (docs/Guide.md:1399).
* **needs**: G-3
* **size**: small (G-3 decided the form)
* **decided**:
  * The form, the markers (`// start:<n>`, `// end:<n>`), the six headings and the gate are G-3's and D-160's. The program is the C one in C++: `cf::` calls from `docs/Guide.md:1399-1472` (section 13), no call that section and `examples/cpp/tier1.cpp` do not show. Heading 1 gives the one CMake line, `target_link_libraries(app PRIVATE ConfigForge::configforge)` (Guide:62) and says the header is the same.
  * Built by `examples/cpp/CMakeLists.txt` as `tier1` is (read its loop, add `start`), run by `expect.cmake` as G-3's is.
* **files**: `examples/cpp/start.cpp`, `examples/cpp/start.expected`, `examples/cpp/CMakeLists.txt`, `docs/start/cpp.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-4 main`.
  2. See it fail: write `docs/start/cpp.md` first with the markers pointing at `examples/cpp/start.cpp`; `ctest --preset default -R "gate\.start"` fails (no file).
  3. Write `examples/cpp/start.cpp` (at most 40 lines), build, run, record `start.expected`.
  4. `ctest --preset default -R "gate\.start|example.*start"` exit 0.
  5. Nav, README, Guide lines; CHANGELOG ("Added", one line, G-4); the Roadmap item stays 🟨 until G-6.
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `ctest --preset default -R "gate\.start"` exit 0.
* **stop if**: a call the page needs is in neither section 13 nor `tier1.cpp`; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; no D-number; Roadmap unchanged (🟨).

## G-5 - "Start here" in Rust

* **id**: G-5
* **goal**: `docs/start/rust.md` is G-3's page in Rust, its code `examples/rust/start.rs`, checked by `gate.start`.
* **why**: as G-3, for the Rust reader (Guide section "Rust", docs/Guide.md:1511).
* **needs**: G-3
* **size**: small
* **decided**:
  * Form, markers (`// start:<n>`) and gate as G-3. Heading 1 gives the Cargo line the Guide gives (read `docs/Guide.md:1511-1562`; if it names a path dependency, the page says so and does not invent a crates.io name: publishing is the maintainer's, 1.10.0). The program uses the calls of `examples/rust/tier0.rs` and the Guide's Rust section only.
  * Built and run as the other Rust examples are: read `examples/rust/CMakeLists.txt` (cargo runs an example through `expect.cmake -DEXE=cargo -DARGS=...`, `examples/expect.cmake:5-7`) and add `start` the same way. A machine without cargo skips the example test as the others are; `gate.start` still checks the page against the file and the recorded output.
* **files**: `examples/rust/start.rs`, `examples/rust/start.expected`, `examples/rust/CMakeLists.txt`, `docs/start/rust.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/G-5 main`.
  2. See it fail: page first, markers pointing at the missing file; `ctest --preset default -R "gate\.start"` fails.
  3. Write `examples/rust/start.rs` (at most 40 lines); `cargo fmt --check` and `cargo clippy` on it as the rust examples are checked (`grep -n "clippy\|fmt" .github/workflows/ci.yml | head`); run it; record `start.expected`.
  4. `ctest --preset default -R "gate\.start|example.*start"` exit 0.
  5. Nav, README, Guide lines; CHANGELOG (G-5).
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI (the rust job included); `ctest --preset default -R "gate\.start"` exit 0; `cargo fmt --check` and `cargo clippy -- -D warnings` clean on the example.
* **stop if**: the Guide's Rust section names no way to depend on the crate; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; no D-number.

## G-6 - "Start here" in Python

* **id**: G-6
* **goal**: `docs/start/python.md` is G-3's page in Python, its code `examples/python/start.py`, checked by `gate.start`, and the Roadmap item "Start here, one page a language" is done.
* **why**: as G-3, for the Python reader (Guide "Python", docs/Guide.md:1563; install is `python -m pip install .`, line 1567).
* **needs**: G-3, G-4, G-5 (so that the last card marks the item)
* **size**: small
* **decided**:
  * Form, markers (`# start:<n>`) and gate as G-3. Heading 1 is the Guide's install line (docs/Guide.md:1567) and nothing else. The program uses the calls of `examples/python/tier0.py` and the Guide's Python section only. Built and run as `examples/python/CMakeLists.txt` does `tier0.py`.
  * The item is marked 🟩 here, and `python scripts/docs/move_done.py` (G-2) moves it to `docs/Done.md`.
* **files**: `examples/python/start.py`, `examples/python/start.expected`, `examples/python/CMakeLists.txt`, `docs/start/python.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Done.md`.
* **steps**:
  1. `git switch -c work/G-6 main`.
  2. See it fail: page first; `ctest --preset default -R "gate\.start"` fails.
  3. Write `examples/python/start.py` (at most 40 lines; `ruff check` and `ruff format --check` clean, as `gate.ruff`); run it; record `start.expected`.
  4. `ctest --preset default -R "gate\.(start|ruff)|example.*start"` exit 0.
  5. Nav, README, Guide lines; CHANGELOG (G-6); Roadmap item 🟩; `python scripts/docs/move_done.py`.
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gate\.(start|ruff)"` exit 0; `python scripts/docs/move_done.py --check` exit 0.
* **stop if**: the Python package cannot be built on the machine and no example test can run: report it, do not mark 🟩; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap "Start here, one page a language." 🟩.

## G-7 - Recipes: reload events as log records, a report in OPA and CUE

* **id**: G-7
* **goal**: the Guide has a "Recipes" section with two recipes whose code is tested: each reload's events as OpenTelemetry log records, and a report as the input to an OPA (Rego) policy and to a CUE check about values per profile.
* **why**: the fingerprint as a resource attribute is in the Guide (docs/Guide.md:1286-1296); the other two halves of the item remain (docs/Roadmap.md:938-941). A recipe is a page of someone else's tool's syntax: the library gives no code for it (no tracing SDK, no policy engine is a dependency).
* **needs**: nothing
* **size**: medium
* **decided**:
  * **Place.** A new `### Recipes` heading at the end of Guide section 11 ("The editor, the report and Doctor", docs/Guide.md:1315-1361), before `## 12. Small targets` (line 1362); the contents list (docs/Guide.md:1-25) needs no new line (a `###`). The existing OpenTelemetry paragraph stays where it is; the section's first sentence links to it.
  * **Recipe 1: log records.** The program's own `cf_log_event` callback (docs/Guide.md:1656-1665) maps an event to an OpenTelemetry log record: severity from `e->level`, body `e->message`, attributes `configforge.event`, `configforge.key`, `configforge.source`, `configforge.code`. Shown as C with a stand-in `otel_log_emit(severity, body, n, attrs)` the reader supplies (as the fingerprint recipe's `otel_resource_set`, Guide:1286), the library's side being real: the example is `examples/c/recipe_otel.c`, compiled and run, with the stand-in printing the record as one JSON line; the page's fence is tied to it by G-3's markers (so G-3 must have landed: this card needs it). Reload events come from `cf_live_options.log` (Guide:1650) and the example uses `examples/c/reload.c`'s recipe (Guide:1050-1066) for its trigger.
  * **Recipe 2: OPA and CUE.** The input is the JSON report (`cf_report_json`; shape in D-16, `docs/Decisions.md` section "D-16 — Provenance Report Schema", or `docs/decisions/D-16.md` after G-1b): `grep -rn "cf_report_json\|cf_report_write" examples docs/Guide.md | head` shows how a program writes one. The recipe shows three rules in Rego (a key at `sensitivity` secret must not come from a file; `server.port` in profile `prod` must be above 1023; a key from a source named `cli` is refused in `prod`) and the same three in CUE (`#Report` constraints, `cue vet -d '#Report' report.json schema.cue`). Rule values are the contract's own key names from the example's contract; the `prod` profile exists in the example (`examples/c/layers.c` or `policy.c`: read their headers; use the one that has profiles; stop if none does).
  * **Tests.** `scripts/check/recipes.cmake` (new), `ctest -R recipes.opa` and `recipes.cue`: write the report with the example, then `opa eval --format raw --input <report> --data <rego> 'data.configforge.deny'` must print the expected denial list for a report that breaks a rule and an empty list for one that does not; `cue vet` must fail for the first and pass for the second. A machine without `opa` or `cue` skips the test with a note (as `verify.cmake` skips an EXTRA step whose tool is missing: `scripts/verify.cmake:98-110`); CI installs neither in this card (a later card's choice: adding a tool to the CI image is the maintainer's, the pinned-tools list `scripts/buildenv/setup.sh`). The Rego and CUE files are `docs/recipes/report.rego`, `docs/recipes/report.cue` and the page fences are tied to them by the same markers as G-3 (`<!-- from docs/recipes/report.rego start:1 -->`), with `#` comment markers for Rego and `//` for CUE; `gate.start` walks `docs/start/*.md` only, so this card extends it to also walk the Guide's tied fences (one line in `scripts/check/start.py`: its glob becomes `docs/start/*.md` plus `docs/Guide.md`).
  * **No D-number**: a recipe decides nothing a reader of the code needs.
* **files**: `examples/c/recipe_otel.c` (new), `examples/c/CMakeLists.txt`, `docs/recipes/report.rego` (new), `docs/recipes/report.cue` (new), `docs/recipes/report-bad.json` and `report-good.json` (new, written by the example and kept), `scripts/check/recipes.cmake` (new), `scripts/check/start.py`, `cmake/ConfigForgeGates.cmake` or `examples/CMakeLists.txt` (where the tests are registered), `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-7 main` (after G-3).
  2. Test first: write `scripts/check/recipes.cmake` and the two JSON reports from a real run of the example's report call; `ctest -R recipes` fails (no rego, no cue).
  3. See it fail: run it; the failure names the missing file. Write `report.rego` and `report.cue`; both tests pass where `opa` and `cue` are installed (`opa version`, `cue version`); on a machine without them record in the commit message that the two were not run, and CI will not run them either (see decided).
  4. Write `examples/c/recipe_otel.c`; build and run; its output is a JSON line per event; the expect regex proves `"configforge.event"` is in it.
  5. Write the Guide section; extend `scripts/check/start.py`'s walk; `ctest --preset default -R "gate\.start|recipes|example.*recipe"` exit 0; `gate.codespell` and `gate.links` on the new files.
  6. CHANGELOG "## 2.3.0 — unreleased" "### Added" (G-7); Roadmap "Recipes, not code" 🟩 (heading text: "Recipes, not code"; its header note about the fingerprint recipe is deleted as done).
  7. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gate\.start|example.*recipe"` exit 0; `ctest -R recipes` exit 0 where `opa` and `cue` are installed, "skipped" printed where they are not.
* **stop if**: no example with a profile and a sensitive key exists to produce the report; the log-record mapping needs a field `cf_log_event` does not carry (read `cf_log.h`: the card is split instead of the library changed); three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap "Recipes, not code" 🟩; no D-number.

## G-8 - A playground on the documentation site

* **id**: G-8
* **goal**: the documentation site has a page `playground.html` where a reader pastes a contract and a file, presses a button, and sees `explain()` and the provenance, the library running as WebAssembly in the browser, nothing installed.
* **why**: "Being found" (docs/Roadmap.md:906-915): a newcomer sees why a value is what it is before they install anything. A WebAssembly build exists (tier 2, D-55, `cmake/toolchains/wasm32-wasi.cmake`, `verify -DEXTRA=wasm` at `scripts/verify.cmake:695-724`); it runs the unit tests under Node, never in a browser.
* **needs**: G-3 (the page's example text is the C start page's contract)
* **size**: large (a build, an in-memory entry point, a page, a workflow change)
* **decided**:
  * **No new dependency (D-162, below).** The browser has no WASI. The library built for `wasm32-wasi` imports a handful of `wasi_snapshot_preview1` functions (`fd_write`, `proc_exit`, `clock_time_get`, `environ_*`, `args_*`, `random_get`: the real list is printed by `wasm-objdump -x` or, without it, by instantiating with an empty import object and reading the `WebAssembly.Module.imports` error): the page's script provides them in about 60 lines (writes to a buffer, a clock, empty environment, a fixed random). No shim library from a CDN, no file system. Therefore the playground feeds the library from memory, never a path.
  * **The entry point.** `playground/playground.c` (new), C89, exports (via `__attribute__((export_name("...")))` guarded by `#ifdef __wasm__`, else plain functions so a host build can test it): `cf_play_input(const char* contract_text, const char* file_name, const char* file_text, const char* key)` returning a `const char*` into a static buffer of 64 KiB (a longer result is cut with the line `... cut at 64 KiB`), `cf_play_alloc(size_t)` and `cf_play_free(void*)` for the page to place its strings. It builds a configuration whose one source is the pasted file read from memory by format name taken from `file_name`'s extension (`.toml`, `.json`, `.yaml`, `.ini`) and prints `cf_explain` for `key` (or `cf_report_write` text for the whole configuration when `key` is empty). Which memory-source call to use: `grep -n "cf_source_by_content\|cf_builder_source\w*\|cf_builder_text\w*" lib/*/include/configforge/*.h`; the card uses the one that takes bytes and a format name (`lib/serialize/src/blob.c`'s `cf_source_by_content` is the candidate, docs/Codemap.md:262). If none takes bytes with a format name: **stop** (the card is split; adding the call is a public interface and the maintainer's).
  * **A contract pasted in.** The contract is the tiny text language of `examples`'s contract files if one exists; if the contract is C code only (`cf_builder_define`), the playground offers a form of rows (key, default, min, max) which the page turns into calls: `cf_play_define(key, default_text, min, max)` called once a row before `cf_play_input`. Decide this by `grep -rn "contract" docs/Guide.md | head -20` (Guide section 3, docs/Guide.md:124-223): the page's input is whichever the Guide calls "a contract". Stop if the Guide has both and they differ (the card is two).
  * **The build.** A CMake target `playground` (`playground/CMakeLists.txt`, added under `if(CMAKE_SYSTEM_NAME STREQUAL WASI)` in the top `CMakeLists.txt` so a host build is unchanged) linking the static libraries and `-Wl,--export-dynamic -Wl,--no-entry` (the toolchain's `wasm-ld` options), producing `playground.wasm`. Configured with `--toolchain cmake/toolchains/wasm32-wasi.cmake -DCF_PROFILE=minimal` (the smallest library: no threads, no file notification, as the toolchain says); the size of the file is printed and must be under 1 MiB (else stop: the card says what grew).
  * **The page.** `docs/playground.md` (a page with a `<textarea>` for the contract, one for the file, a key field, a button and a `<pre>` for the result) and `docs/playground/play.js` (the shim and the glue, at most 150 lines, no framework, no CDN) and `docs/playground/configforge.wasm` (built, never committed: `.gitignore`d, as the benchmark's SVGs are, `.github/workflows/docs.yml:30-38`). A page without the wasm file says "The playground is built with the site; this copy was built without it" (the script catches the failed fetch). A reader with JavaScript off sees the same sentence.
  * **CI.** `.github/workflows/docs.yml` gains, before "Check and build the documentation" (docs.yml:39), a step "Build the playground" that installs clang, lld and wasi-libc (`apt-get install -y clang lld wasi-libc libclang-rt-18-dev-wasm32`: the toolchain's comment names these, `cmake/toolchains/wasm32-wasi.cmake:5-7`; pin the version in the way `scripts/buildenv/setup.sh` pins others: read it, and where it has no apt pin pattern, state the package names and nothing else) and runs the two cmake commands, copying `build-wasm/playground/playground.wasm` to `docs/playground/configforge.wasm`. A failure of that step fails the docs job (no `continue-on-error`): the claim of the page is that it is live. `scripts/check/check_docs.py:12-21` gains `"docs/playground/index.html"` and, when the job built it, `"docs/playground/configforge.wasm"` (the script reads the environment variable `CF_PLAYGROUND=1` set by the workflow step).
  * **Tests.** A host unit test `lib/serialize/tests/`? No: `playground/tests/test_play.c` built with the host compiler (the plain-function path of `#ifdef __wasm__`), registered with `cf_unit_tests` in `playground/CMakeLists.txt`, three cases: a TOML file and a contract give the explain text of `examples/c/explain_demo.c`'s style; a file with a syntax error gives the refusal text (not a crash); a result over 64 KiB is cut with the marker. And `scripts/check/playground_node.js` (stdlib Node, like `scripts/check/wasi_run.js`): loads `configforge.wasm` through `play.js`'s shim (the shim is a module `docs/playground/shim.js` exporting `imports(buffer)`; `play.js` imports it) and asserts the same first case; run by `ctest -R playground.node` where `node` and the wasm exist, skipped with a note elsewhere.
  * **D-162, text to be added:** "## D-162 — The playground is the library, in the page, with no shim library. Question: how does a reader try the library with nothing installed? Options: a server that runs it (a service to run and secure); WebAssembly with a WASI shim from a package registry (a dependency and a supply chain for a page); WebAssembly with the 60 lines of imports this library needs, fed from memory. Decision: the third. The library takes its input from memory and prints to a buffer, so the page needs no file system; the wasm file is built with the site and never committed. Consequence: the playground is the library's own tier-2 build; if it breaks, the docs job fails."
* **files**: `playground/playground.c`, `playground/CMakeLists.txt`, `playground/tests/test_play.c` (new), `CMakeLists.txt` (one `add_subdirectory`), `docs/playground.md`, `docs/playground/play.js`, `docs/playground/shim.js` (new), `scripts/check/playground_node.js`, `.github/workflows/docs.yml`, `scripts/check/check_docs.py`, `mkdocs.yml` (nav: Home group, `Playground: docs/playground.md`), `.gitignore`, `docs/Decisions.md` (D-162), `docs/Dependencies.md` (clang, lld, wasi-libc already named at line 70: add "and the playground"), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-8 main`.
  2. Test first: `playground/tests/test_play.c` with the three cases; `cmake --preset default && ctest --preset default -R playground` fails (no source).
  3. See it fail: build errors name `playground.c`. Write `playground/playground.c`; the host test passes.
  4. The wasm build: `cmake -S . -B build-wasm --toolchain cmake/toolchains/wasm32-wasi.cmake -DCF_PROFILE=minimal && cmake --build build-wasm --target playground`; `ls -l build-wasm/playground/playground.wasm` under 1 MiB. Where clang, wasm-ld or node is missing, say so and do the rest; CI judges (as `verify -DEXTRA=wasm` skips).
  5. Test first for the shim: `scripts/check/playground_node.js` asserting case one; see it fail (no shim), write `docs/playground/shim.js`, pass: `node scripts/check/playground_node.js build-wasm/playground/playground.wasm`.
  6. Write `docs/playground.md` and `docs/playground/play.js`; load `docs/playground.md` through `python -m mkdocs build --strict` and, with the wasm copied in, open `build/docs-site/docs/playground/index.html` through a local server (`python -m http.server -d build/docs-site`) and with the browser pane press the button; the `<pre>` shows the explain text. Say in the report what was seen.
  7. `.github/workflows/docs.yml` and `check_docs.py` as decided; `actionlint` on the workflow (`ctest -R gate.actionlint`).
  8. D-162 (text above); Dependencies line; CHANGELOG "## 2.3.0 — unreleased" "### Added" (G-8); Roadmap "Being found": 🟨 with the line "playground done (G-8); the comparison page, the recording and the articles are not cards yet" (the item says "Done when the playground is live and the articles are published": the articles are the maintainer's writing, so the item stays 🟨).
  9. Format, codemap, apiref, ACCEPT. After the push, `gh run watch` the `docs` workflow on the branch: green and `playground.wasm` in the uploaded artifact.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` and the new "Build the playground" step green; `ctest --preset default -R "playground"` exit 0; `node scripts/check/playground_node.js` exit 0 where node and the wasm exist; the page seen working in the browser pane.
* **stop if**: no in-memory source call takes bytes and a format name (see decided); the wasm file exceeds 1 MiB at `CF_PROFILE=minimal`; the contract's pasted form is two things; the Pages deployment needs a setting only the maintainer has (repository settings: write it under notes as a parked G-8-m, do not change it); three failures on the same thing.
* **docs**: D-162 (text above); CHANGELOG "## 2.3.0 — unreleased" "### Added"; docs/Dependencies.md; Roadmap "Being found." (🟨).

## G-9 - A program not written for the library, moved onto it in a fork

* **id**: G-9
* **goal**: one small open-source daemon's configuration code is replaced by this library in a local fork, and what that showed is written as findings, each a fix or an idea in the Roadmap.
* **why**: every example here was written for the library (docs/Roadmap.md:754-759); a real program shows what the Guide does not.
* **needs**: nothing (G-3 helps: its page is the first thing the port follows)
* **size**: large (a port of unfamiliar code; the card stops at ~60 turns and splits)
* **decided**:
  * **Which program is TO DECIDE (the choice of what to adopt is the maintainer's, and publishing is theirs).** Criteria, so the options are comparable: a C daemon of under 20,000 lines; its configuration read from one file of key-value or INI-like text by its own parser in one or two source files; a licence that lets a fork carry a library under MIT or Apache (GPL-2 or later, BSD, MIT, ISC); builds with `make` or CMake on Linux without a package the machine lacks; has a test or a way to start and check it. Options to put to the maintainer: **tinyproxy** (HTTP proxy, a directive file parsed in `src/conf.c`, GPL-2.0-or-later), **inadyn** (DDNS client, libConfuse-based, GPL-2.0), **darkhttpd** (no config file: its flags would be the contract, which shows the flag side, ISC). Recommendation: tinyproxy, because its directives (Port, Listen, Allow, Timeout, LogLevel) are the plain keys, ranges and defaults a contract expresses and its parser is one file. Until the maintainer chooses, the card is parked: it carries the id G-9 and does not open. Everything below is what the agent does once the choice is written under the card's "Organiser's answers".
  * **What the port keeps.** The daemon's behaviour with its own sample configuration: the same listening port, the same log level, the same refusals for a bad value (the daemon's wording of an error need not be kept: a refusal that names the key and the source is the library's and is a finding of its own).
  * **Where.** A clone in `build-g9/<name>` (inside `.gitignore`d `build-*`; not in this repository), a branch `configforge-port` in it, commits in the clone, never a push. The findings go into this repository: `docs/plan/G-9-findings.md`? No new document type: the findings are written as a "notes" section of this card and as Roadmap ideas (one `*` bullet each under "### From a review of the whole, 2026-11-14"'s style heading "### Found by porting <name> (<date>)", marked ⬜).
  * **What to measure and write.** Lines of configuration code removed and added (`git diff --shortstat` of the clone, parser file only); what in the contract the daemon's keys needed that the Guide does not show; each place the port was awkward (an API that wants a type the daemon's code does not have; a message the daemon's operators would not understand); whether `explain()` answered the question an operator of this daemon asks ("why is the listen address what it is"). Each finding is one line, `kind: fix|idea`, the Guide section or header it concerns.
  * **Publishing.** Pushing the fork to a public place and writing about it is the maintainer's, a card of its own: **G-9-m** (parked, 2.3.0, waits on "the maintainer publishes the fork and the text").
* **files**: the clone (outside the tree); in this repository only `docs/plan/G-guides.md` (this card's notes), `docs/Roadmap.md` (the findings as ideas), `CHANGELOG.md` (nothing unless a fix lands), `docs/Roadmap.md` (the item).
* **steps**:
  1. Wait for "Organiser's answers: program = <name>". Then `git switch -c work/G-9 main`.
  2. `git clone <upstream> build-g9/<name> && git -C build-g9/<name> switch -c configforge-port`; build it unchanged; run it with its sample configuration and record what it does (`<name> -h` or its log's first lines) into the card's notes.
  3. Write the contract for its keys in the daemon's own style, tied to the sample configuration (the "See it fail" is that the daemon, built against this library with the old parser removed, refuses the sample file until the contract matches: run it, see the refusal, fix the contract).
  4. Replace the parser by calls to this library (`add_subdirectory` of this repository's path in the clone's build, or `pkg-config configforge` from a local install: the card uses the second so the clone shows what a user sees).
  5. Run the daemon with the sample configuration and with three bad files (a wrong type, an out-of-range value, an unknown key); compare with step 2's recording.
  6. Write the findings under "notes" and the Roadmap ideas; run `ctest --preset default -R "gate\.(links|codespell)"` on the edited documents.
  7. Commit the notes and the ideas to `work/G-9`; format, codemap, apiref, ACCEPT (documents only).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; in the clone: the daemon runs with the sample configuration and the recorded behaviour of step 2 is reproduced (state each difference); each finding is a line under "notes" and an item in the Roadmap or a fix.
* **stop if**: no program is chosen (the card stays parked, not waited on); the daemon cannot be built on the machine; the port needs more than ~600 changed lines in the clone (stop at a working part and say which keys are ported); three failures on the same thing.
* **docs**: CHANGELOG nothing; Roadmap "Used by a program not written for it." stays ⬜ until the maintainer publishes (G-9-m) and 🟨 when the findings are written.

## G-9-m - Publishing the fork and what it found

* **id**: G-9-m
* **goal**: the ported daemon is published and the findings are written up where people read them.
* **why**: the Roadmap's "done" is "the change is published with what it found" (docs/Roadmap.md:758-759); publishing is the maintainer's.
* **needs**: G-9
* **size**: small (the maintainer's own steps)
* **decided**: a public repository under the maintainer's account, a README of the fork stating what changed and what was found (G-9's notes), a link from the Roadmap and the Guide's examples list (docs/Guide.md:26-54). Waits on: the maintainer's account and the choice of place.
* **files**: none in this repository but `docs/Guide.md` and `docs/Roadmap.md` (a link).
* **steps**:
  1. The maintainer pushes the clone's branch `configforge-port` to a public repository.
  2. A worker adds the link and one paragraph to the Guide's examples section and marks the Roadmap item 🟩.
* **accept**: the link resolves (`ctest -R gate.links`).
* **stop if**: the upstream's licence forbids a published fork carrying the change (read the licence file first).
* **docs**: Roadmap "Used by a program not written for it." 🟩.
