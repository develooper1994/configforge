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

**How a test script runs here (read cold 2026-11-26).** The tests under
`tests/work/` are not `unittest` modules: each is a script whose `main()` reads
`sys.argv` and is registered in `tests/work/CMakeLists.txt` (`work.brief` runs
`test_brief.py <scripts/work/brief.py> <tests/work/data> <build dir>`; the fixture
tree is `tests/work/data/`, copied into a temp directory by the script). Every new
test script below (`test_decisions.py`, `test_split_decisions.py`,
`test_move_done.py`, `test_start.py`) is written the same way: arguments
`<script under test> <build dir>`, its own `TemporaryDirectory(dir=<build dir>)`
trees, registered as `work.decisions`, `work.split_decisions`, `work.move_done`,
`work.start` in `tests/work/CMakeLists.txt` under the same `if(Python3_Interpreter_FOUND)`.
It is run with `ctest --preset default -R "work\."` (never `python -m unittest`).
A python gate (`gate.decisions`, `gate.start`) is registered in
`cmake/ConfigForgeGates.cmake` with its own
`find_package(Python3 COMPONENTS Interpreter QUIET)` as `tests/work/CMakeLists.txt:1-4`
does, skipped with a `message(STATUS ...)` when Python is absent (that file has no
such lookup today). The Roadmap's marks paragraph is `docs/Roadmap.md:24-26`.

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

<a id="g-1a"></a>
## G-1a - The tools that read a decision follow the split

* **id**: G-1a
* **goal**: every tool that finds a decision by its number finds it in `docs/decisions/D-<n>.md` when that file exists and in `docs/Decisions.md` when it does not, and a new gate `gate.decisions` fails when the index and the files disagree; nothing is split yet.
* **why**: G-1b moves 111 decisions out of a 5,326-line file. If the tools learn the new place first, the split is one mechanical commit that cannot break a card brief or the limits gate, and each card lands alone.
* **needs**: nothing
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **Layout (D-159, text below).** One decision, one file: `docs/decisions/D-<n>.md`, `<n>` exactly as the heading writes it (`D-01`, `D-70`, `D-112`; two digits at least, as now). The file begins with the decision's own `## D-<n> — <title>` line and holds its text unchanged. `docs/Decisions.md` keeps its title, "How to Use This Document" and "Decision Index" (lines 1-133); each index row's ID cell becomes a link `[D-01](decisions/D-01.md)`; the sections themselves leave. A section with no index row is a G-1b stop (D-18 to D-22 have rows, `docs/Decisions.md:51-55`; the sections are not in numeric order in the file: D-60 and D-62 come after D-112, so no tool may assume order).
  * **The five readers, and how each follows.**
    1. `scripts/work/brief.py:70-90` `resolve`: for `decision`, `path` is `docs/decisions/<decision>.md` if that file exists, else `docs/Decisions.md`; with the new file the section is the whole file (the existing `sections(lines)` walk finds its `## ` heading, so the loop and the `re.match(rf"^{decision}\b", title)` test stay as they are). A card that writes `D-7` for the heading `D-07` is looked up as `D-07` as well (try the name as written, then padded to two digits). The label stays `docs/Decisions.md, D-70` (tests/work/test_brief.py:29 expects it): keep it so a brief reads the same before and after.
    2. `cmake/gates/GateLimits.cmake:24-34`: a reference `D-<n>` in `docs/Limits.md` is satisfied by `docs/decisions/D-<n>.md` existing, or by the old `## D-<n>` heading in `docs/Decisions.md`. Error wording kept: "docs/Limits.md names D-<n>, which docs/Decisions.md does not have".
    3. `scripts/generate/limits.cmake:430`: the generated link `[D-109](Decisions.md)` becomes `[D-109](decisions/D-109.md)` only in G-1b (the generator writes `docs/Limits.md`, which `gate.limits` compares byte for byte). In G-1a nothing changes here.
    4. `docs/Maintaining.md:265`, "`grep -n "^## D-<n>" docs/Decisions.md`": G-1b rewrites it to `cat docs/decisions/D-<n>.md`. `CLAUDE.md` and `CONTRIBUTING.md:46,113,290` name `docs/Decisions.md` as the place and stay true (the index is there).
    5. `mkdocs.yml:57` (`Decisions: docs/Decisions.md`): G-1b adds the files to the site (below).
  * **`gate.decisions`.** New script `scripts/check/decisions.py`, run as `ctest -R gate.decisions` (registered beside `gate.limits`, `cmake/ConfigForgeGates.cmake:111-115`, with the Python lookup of the note at the top of this file; run as `"${Python3_EXECUTABLE}" scripts/check/decisions.py <source root>`, the root defaulting to the script's grandparent directory so the test can point it at a temp tree). With no `docs/decisions/` it passes (G-1a). With it: every `docs/decisions/D-*.md` starts with `## ` and the same `D-<n>` as its file name; every file is linked from the index exactly once; every index row's link target exists; no `## D-` heading remains in `docs/Decisions.md`; the first line of failure is `decisions: <file>: <reason>`, exit 1.
  * **D-159, to be appended to `docs/Decisions.md` (and its index row, status **Decided**, blocks "documents"):** "## D-159 — One decision, one file. Question: how does a reader open one decision of 111 without scrolling past the rest? Options: keep one file and add an outline (the site already has one; the file stays 5,000 lines for an agent that reads by line); split by theme (a decision belongs to several themes); one file per decision. Trade-off: one file per decision costs 111 files and a link rewrite; it makes a decision's address its path, so a card cites it exactly and a worker reads it alone. Recommendation and decision: one file per decision, `docs/decisions/D-<n>.md`, the index kept in `docs/Decisions.md` with a link per row; the tools that find a decision by number (the card brief, the limits gate) follow, and `gate.decisions` keeps the index and the files together. Rejected: a generated single file (it would be the file this removes). Consequence: a new decision is a file and an index row."
* **files**: `scripts/work/brief.py`, `tests/work/test_brief.py`, `tests/work/data/docs/decisions/D-71.md` (new), `tests/work/data/docs/plan/fixture.md`, `tests/work/CMakeLists.txt`, `cmake/gates/GateLimits.cmake`, `scripts/check/decisions.py` (new), `tests/work/test_decisions.py` (new), `cmake/ConfigForgeGates.cmake`, `docs/Decisions.md` (D-159 and its row), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-1a main`.
  2. Test first. The fixture is `tests/work/data/` (copied whole by `test_brief.py`, lines 15-22): add `tests/work/data/docs/decisions/D-71.md` (first line `## D-71 — Split`, one body line `Split decision content.`), cite `D-71` in `tests/work/data/docs/plan/fixture.md` where it cites `D-70` (read that file first), and add `"--- docs/Decisions.md, D-71"` and `"Split decision content."` to the `parts` list at `tests/work/test_brief.py:26-38`. D-70 stays in the fixture's `docs/Decisions.md` (the fallback).
  3. See it fail: `ctest --preset default -R work.brief --output-on-failure 2>&1 | tail -n 20` fails on D-71.
  4. Change `resolve` in `scripts/work/brief.py:69-92` as decided; run `ctest --preset default -R work.brief`: passes, and D-70 still passes.
  5. Test first for the gate, `tests/work/test_decisions.py` (arguments `<scripts/check/decisions.py> <build dir>`, registered as `work.decisions`): a temp tree with `docs/Decisions.md` (index rows linking D-01, D-02) and `docs/decisions/D-01.md`, `D-02.md` passes; each of these fails with its message: a file missing from the index, a link to a missing file, a file whose heading number differs from its name, a `## D-03` left in `docs/Decisions.md`. See them fail (the script does not exist), then write `scripts/check/decisions.py` (stdlib only, Python 3.8 syntax) until they pass.
  6. `GateLimits.cmake`: replace the `if(NOT decisions MATCHES ...)` test (line 28) with one that also accepts `EXISTS "${SRC}/docs/decisions/${ref}.md"`. See it fail first, with a scratch tree: `cp -r` the working tree minus `build*`, `.git` and `.claude` into `$TMP/src`; in it rename the line `## D-109 — ...` in `docs/Decisions.md` to `## X-109 — ...` and create `docs/decisions/D-109.md` (`## D-109 — x`); `cmake -DSRC=$TMP/src -DBIN=$TMP/bin -P cmake/gates/GateLimits.cmake` (the arguments `ctest` gives it, `cmake/ConfigForgeGates.cmake:112-115`) fails before the change and passes after.
  7. Register `gate.decisions` in `cmake/ConfigForgeGates.cmake` beside `gate.limits` and `work.decisions` in `tests/work/CMakeLists.txt`; `cmake --preset default && ctest --preset default -R "gate\.(decisions|limits)|work\."`.
  8. Append D-159 and its row; CHANGELOG under "## 1.9.0 — unreleased", "### Added": "`gate.decisions`; the card brief and `gate.limits` find a decision in `docs/decisions/` (G-1a, D-159)". Roadmap: the item "Documents kept short" stays ⬜ until G-2 (G-1a is half of it): add one line "G-1a: the tools follow; G-1b: the split; G-2: the finished items" under it.
  9. `python scripts/work/work.py format`; `cmake -P scripts/generate/codemap.cmake`; `cmake -P scripts/generate/apiref.cmake`; ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "work\.(brief|decisions)"` exit 0; `ctest --preset default -R "gate\.(decisions|limits)"` exit 0.
* **stop if**: `gate.limits` cannot be made to accept both places without rewriting `limits.cmake`'s output; a tool (a script, a gate, a test; not prose) other than the five finds a decision by a line number into `docs/Decisions.md` (`grep -rn "Decisions\.md" scripts cmake tests .github`; the prose citations by line in `docs/plan`, `docs/research` and `HANDOFF.md` are G-1b's): list it under notes and stop; three failures on the same thing.
* **docs**: D-159 (text above); CHANGELOG "## 1.9.0 — unreleased" "### Added"; Roadmap "Documents kept short" (the line above).

<a id="g-1b"></a>
## G-1b - Decisions.md split into one file a decision

* **id**: G-1b
* **goal**: `docs/Decisions.md` is the preface and the index (about 140 lines), every decision's text is in `docs/decisions/D-<n>.md` unchanged but for its links, the site and the tools use the new places, and `gate.decisions` is green on the real tree.
* **why**: a reader opens one decision, not a 5,000-line file. A worker's brief names `D-<n>` and receives only it (CLAUDE.md "Tokens": Decisions.md was a quarter of what workers read).
* **needs**: G-1a
* **size**: medium (a script does the move; the rest is five small edits)
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **The splitter** is `scripts/docs/split_decisions.py`, run once and kept (so the commit shows how). It reads `docs/Decisions.md`; a decision is the text from a `## D-<n>` heading (outside code fences: reuse the fence rule of `scripts/work/brief.py:20-34`, import `sections`) to the next `## ` heading at the same level; text before the first (lines 1-~133: title, "How to Use This Document", "Decision Index") belongs to the preface. The sections are not in numeric order: the last decision in the file is D-62 (line 5186), not D-112. After D-62's own text come a `---` line (5307), an unheaded status-audit passage and `## Closing Phase 0` (5320) to the end: the rule is that the last decision ends at the last `---` line before the first `## ` heading that is not `## D-`, and everything from that `---` to the end of the file is the foot of the index file (so the passage and "Closing Phase 0" stay in `docs/Decisions.md`). Every other decision ends at the next `## ` heading.
  * **Links.** In a moved file, `](Design.md)` and every relative link that does not start with `http`, `#` or `../` gains `../` (the file is one directory deeper); `](#d-<n>...)` anchors to another decision become `](D-<n>.md)`; an anchor into the same decision stays. The splitter lists every link it changed and every `#anchor` it could not resolve; the unresolved are fixed by hand in the same commit (a handful is expected). `mkdocs build --strict` has `validation.links` at `warn` for anchors (mkdocs.yml:42-45): after the split the build must have no new warning.
  * **Index.** In `docs/Decisions.md`, each row's ID cell `D-01` becomes `[D-01](decisions/D-01.md)`; nothing else in the row changes. Any decision with a section and no row (D-18 to D-22 have rows): the splitter stops and prints them; G-1b adds the row with status read from the section's "Status" line (stop if there is none).
  * **Site.** `mkdocs.yml` nav under Design keeps `Decisions: docs/Decisions.md`; the files are found by links alone (`scripts/docs/docs_site.py` already maps `docs/` into `docs/` of the site). `mkdocs build --strict` warns of pages in the docs dir that the nav omits: set `validation.nav.omitted_files: info` if it does (read `mkdocs.yml:42-45` for the key's place); do not list 111 files in the nav.
  * **Tool follow-up.** `scripts/generate/limits.cmake:430` writes `](decisions/D-109.md)`; regenerate `docs/Limits.md` (`cmake -P scripts/generate/limits.cmake`: read its header for arguments; `gate.limits` compares). `docs/Maintaining.md:265`: "`cat docs/decisions/D-<n>.md`". `docs/Maintaining.md:255` (the token measurement) is history: unchanged. `docs/Codemap.md` has no entry for markdown: regenerate and check `git diff --stat docs/Codemap.md` is empty or lines only.
  * **Other documents' citations.** `grep -rn "Decisions.md" docs README.md CONTRIBUTING.md .github CLAUDE.md AGENTS.md` shows prose citations ("D-36 in `docs/Decisions.md`", CONTRIBUTING.md:290): each stays true because the index is in that file and names the decision's link; leave them. A citation by line number into Decisions.md (`Decisions.md:<n>`) is rewritten to the decision's file: the grep earlier found only docs/Design.md's prose at the cited text "with the decision in Decisions.md" (line 683), left as it is.
  * **Cards and notes** in `docs/plan/*.md`, `docs/research/*.md` and `HANDOFF.md` cite `docs/Decisions.md:<range>` (read cold 2026-11-26: 16 in docs/plan and HANDOFF, 10 in docs/research/concurrent-access.md and threads-and-processes.md): `grep -rn "Decisions\.md:[0-9]" docs HANDOFF.md`; each such citation is rewritten to `docs/decisions/D-<n>.md` by hand (the range's decision is the heading above it in the old file; `git show main:docs/Decisions.md | sed -n <range>p` finds it). If more than 40 are found, stop (a split card for the citations). `docs/plan/G-guides.md` itself cites Decisions.md ranges (`docs/Decisions.md:11-26`, `30-132`): rewrite those too, to `docs/Decisions.md` "the index" or the file named.
* **files**: `scripts/docs/split_decisions.py` (new), `docs/decisions/*.md` (111 new), `docs/Decisions.md`, `scripts/generate/limits.cmake`, `docs/Limits.md` (generated), `docs/Maintaining.md`, `mkdocs.yml`, `docs/plan/*.md`, `docs/research/*.md` and `HANDOFF.md` (citations), `tests/work/test_split_decisions.py` (new), `tests/work/CMakeLists.txt`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-1b main` (after G-1a has landed).
  2. Record the oracle first: `git show main:docs/Decisions.md | grep -c "^## D-"` (the count of sections) and `git show main:docs/Decisions.md | wc -c`.
  3. Write `scripts/docs/split_decisions.py` with a `--check` mode that, from the new files, compares the total character count of the preface, the foot and the files with step 2's byte count, within the difference the rewritten links make (order is not compared: the old file's sections are not in numeric order): it prints `decisions: <n> moved, <m> links rewritten, <k> anchors unresolved`.
  4. Test first, `tests/work/test_split_decisions.py` (arguments `<scripts/docs/split_decisions.py> <build dir>`, registered as `work.split_decisions`; it imports `sections` from `scripts/work/brief.py` by path): a fixture `Decisions.md` of three decisions, the last followed by `---` and a `## Closing` heading (the foot stays in the index file) (one with a fenced block containing a `## D-99` line, one with a link `](Design.md)` and one with `](#d-02-title)`); expect three files, the fenced line kept inside its decision, the links rewritten, the index linked. See it fail, write the script, see it pass.
  5. Run it on the real file: `python scripts/docs/split_decisions.py docs/Decisions.md docs/decisions`. Expect the count of step 2.
  6. `python scripts/check/decisions.py` exit 0. `git diff --stat` shows one rewritten `Decisions.md` and 111 new files.
  7. The tool follow-up of "decided": `limits.cmake`, regenerate `docs/Limits.md`, `Maintaining.md:265`, the citations in `docs/plan/*.md`, `mkdocs.yml` if needed.
  8. `python -m mkdocs build --strict 2>&1 | tail -n 30` (the docs environment: `scripts/docs/requirements.txt`; skip with a note where mkdocs is not installed: CI's `docs` workflow judges); `ctest --preset default -R "gate\.(decisions|limits|links|codemap)"`.
  9. `python scripts/work/brief.py G-1b | grep -c "D-159"` shows the brief still resolves a decision (the card names D-159 in "decided"); and a brief for any card that cites an old decision (`grep -ln "D-100" docs/plan/*.md`, then `python scripts/work/brief.py <that card id> | grep -c "^## D-100"`) is 1.
  10. CHANGELOG "## 1.9.0 — unreleased" "### Changed": "`docs/Decisions.md` is the index; each decision is `docs/decisions/D-<n>.md` (G-1b, D-159)". Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI, with the `docs` workflow green (mkdocs `--strict` and `check_docs.py`); `ctest --preset default -R "gate\.(decisions|limits|links|codemap)|work\."` exit 0; the section count of step 2 equals `ls docs/decisions | wc -l`.
* **stop if**: more than 10 anchors are unresolved or the count of step 2 differs from the files made; `mkdocs build --strict` fails on a link the splitter could not decide; the `docs` workflow was red on `main` before the card (read `gh run list --workflow docs --branch main --limit 1`); three failures on the same thing.
* **docs**: no new D-number (D-159 is G-1a's); CHANGELOG as above; Roadmap: "Documents kept short" stays ⬜ with the line "G-1b done" (G-2 marks it 🟩).

<a id="g-2"></a>
## G-2 - The Roadmap's finished items on a page of their own

* **id**: G-2
* **goal**: `docs/Done.md` holds every Roadmap item marked 🟩 (the detail of each, with its heading), `docs/Roadmap.md` holds what is left, and nothing that was written is lost.
* **why**: the Roadmap is 1,565 lines and a card's author needs the part that is not finished; the "At a glance" table already says what is done in one row (docs/Roadmap.md:29-~75).
* **needs**: G-1b (both change `mkdocs.yml`; G-2 also states the Done page's place in the same pattern)
* **size**: medium
* **version**: 2.1.0
* **status**: open
* **decided**:
  * **What moves.** Whole items marked 🟩 in the detail sections, not in "At a glance": a bullet that begins `* 🟩 **` with its indented continuation, and a heading that begins `### 🟩` (for example `docs/Roadmap.md:637`) with everything to the next heading. A 🟨 item stays whole, with the text saying what is done and what remains (the maintainer's rule at docs/Roadmap.md:7-10). ✅ items already leave at a release (docs/Roadmap.md:24-26): the Done page is for 🟩 and is emptied of an item when it is released, as the Roadmap is.
  * **Where.** `docs/Done.md`, title `# Done, not yet released`, the preface "Items of the Roadmap marked 🟩: on `main` and verified, not in a published version. Each keeps the heading it had; the Roadmap's row in "At a glance" links here", then the moved items grouped under the heading they came from (`## <Roadmap's ## or ### heading>` then the item). The order inside is the Roadmap's order.
  * **What stays in the Roadmap.** "At a glance" keeps every row (it is the whole list in one table); a 🟩 row's last cell gains a link to the item's anchor on the Done page when the cell has none. In place of a moved item the Roadmap keeps nothing (the row says where it went). The "Marks" paragraph (docs/Roadmap.md:24-26, read it first: it says now that a ✅ item "moves to the CHANGELOG at the next release and leaves this page; until then it stays here, marked") gains the sentence: "An item marked 🟩 moves to docs/Done.md in the same commit; one marked ✅ moves to the CHANGELOG at the next release and leaves".
  * **A mover that can be rerun.** `scripts/docs/move_done.py [--check] [<roadmap> <done>]` (defaults `docs/Roadmap.md`, `docs/Done.md`; stdlib; idempotent: a second run moves nothing; with `--check` it exits 1 when a 🟩 item is still in the Roadmap). It is not wired into `scripts/work/work.py`: it is run by hand when an item turns 🟩 (the Roadmap's own rule moves it in the same commit as the mark); a line in docs/Maintaining.md under "Working habits" says so.
  * **Citations.** `grep -rn "Roadmap.md:[0-9]" docs HANDOFF.md CLAUDE.md AGENTS.md scripts` cites Roadmap lines (the cards cite `docs/Roadmap.md:1514-1523`, P-3): every moved item changes the numbers below it. The cards' citations are the cold reader's concern (docs/Plan.md "Read again after the layout changes"): the card lists them in its notes' report and fixes the ones whose text moved; a citation into a part that stayed is rewritten to the new line numbers by `git diff`'s hunk offsets, by hand. More than 40 citations: stop.
* **files**: `scripts/docs/move_done.py` (new), `tests/work/test_move_done.py` (new), `docs/Done.md` (new), `docs/Roadmap.md`, `tests/work/CMakeLists.txt`, `mkdocs.yml` (nav: after the `Roadmap` entry, whichever group holds it: `grep -n Roadmap mkdocs.yml`; add `Done: docs/Done.md` beside it, same indent), `docs/Maintaining.md`, `docs/plan/*.md` and `HANDOFF.md` (citations), `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/G-2 main`.
  2. Count first: `grep -c "^\* 🟩\|^### 🟩" docs/Roadmap.md` and `wc -l docs/Roadmap.md`; write both in the commit message.
  3. Test first, `tests/work/test_move_done.py` (arguments `<scripts/docs/move_done.py> <build dir>`, registered as `work.move_done`): a fixture Roadmap with a 🟩 bullet (two continuation lines), a 🟨 bullet, a `### 🟩 Heading` section and an "At a glance" row; expect the 🟩 pieces in the fixture Done page under their parent headings, the 🟨 bullet unmoved, the table untouched but for the link, a second run changing nothing, `--check` exit 1 before and 0 after. See it fail, write the script, see it pass.
  4. Run on the real file; `git diff --stat docs/Roadmap.md docs/Done.md`: the lines removed from the Roadmap equal the lines added to Done.md minus the headings and preface (print both in the script's last line).
  5. Edit the "Marks" paragraph and `Maintaining.md` as decided; add the nav line.
  6. Rewrite the citations as decided; `ctest --preset default -R "gate\.(links|codemap)"`; `python -m mkdocs build --strict 2>&1 | tail -n 20`.
  7. Roadmap: mark "Documents kept short" 🟩 (it moves itself by the new rule: run the mover once more). CHANGELOG "## 1.9.0 — unreleased" "### Changed": "The Roadmap's finished items are on `docs/Done.md` (G-2)". Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `python scripts/docs/move_done.py --check` exit 0; `ctest --preset default -R work.move_done` exit 0.
* **stop if**: an item marked 🟩 has a part that another item cites by heading and the heading is gone; more than 40 citations; three failures on the same thing.
* **docs**: no D-number; CHANGELOG as above; Roadmap: "Documents kept short" 🟩 (its heading text: "Documents kept short.").

<a id="g-3"></a>
## G-3 - "Start here" in C, and the gate that keeps every such page true

* **id**: G-3
* **goal**: `docs/start/c.md` takes a reader from the build to `explain()` in C in under 100 lines, every code block in it is a file that is built and run by a test, and `gate.start` fails when a page's block and its file differ.
* **why**: Design.md, Decisions.md and API.md are 11,000 lines between them; the Guide is 1,700. A newcomer needs the one path that works, and a page whose code is not tested rots (docs/Roadmap.md:942-947).
* **needs**: G-1b (D-160 is a file in `docs/decisions/`; the nav lines are the same ones G-1b and G-2 edit)
* **size**: medium
* **version**: 2.6.0
* **status**: open
* **decided**:
  * **Pages.** `docs/start/c.md`, `cpp.md`, `rust.md`, `python.md` (G-4 to G-6), (no index page `docs/start/index.md` is made); the nav has "Start here" with the four pages, and the README's and the Guide's first lines link to them (G-3 adds the C link and the "Start here" nav group; G-4 to G-6 add theirs).
  * **Form of a page (D-160, below).** Six headings, the same in every language: "1. Get it" (the one build line from Guide section 1, `docs/Guide.md:55-96`, for that language), "2. A contract" (a key with a default and a range), "3. Read a value", "4. Override it", "5. Ask why" (`explain()`; the output is the real output, copied from the test), "6. Where next" (three links into the Guide: sections 3, 5 and 7). No page explains a concept: a link does. A page carries one program of at most 40 lines, shown in pieces; the pieces together are that file, in order.
  * **The file is the source (D-160).** The page includes the example file by marked regions: in the example, `/* start:2 */ ... /* end:2 */` (C, C++, Rust: `// start:2`, `// end:2`; Python: `# start:2`, `# end:2`); in the page, the line `<!-- from examples/c/start.c start:2 -->` immediately followed (no blank line) by a fenced block (a comment inside a fence would print). `gate.start` (below) checks that the fence's text equals the region's text between the marker lines, line for line, trailing blanks ignored. An output block is the line `<!-- output examples/c/start.c -->` followed by a block fenced `text`; the gate compares it with the example's expected output file `examples/c/start.expected` (the full recorded output, compared whole). The marker's file path is relative to the repository root.
  * **The C example.** `examples/c/start.c`, built and run like `layers` is, not like `explain_demo` (which `examples/c/CMakeLists.txt:2-13` registers with a bare `add_test`, no output check): add `start` to the second `foreach(ex layers reload ...)` list (`examples/c/CMakeLists.txt:58-72`, builds only) and register `example_expect(start ${CMAKE_CURRENT_BINARY_DIR}/start-run <regex>...)` after the `layers` block (`example_expect` is `examples/CMakeLists.txt:17`; a `file(MAKE_DIRECTORY ...)` for the run directory as `examples/python/CMakeLists.txt:30` does), one regex per line of `start.expected` (each line's text with regex characters escaped), so the test fails on any line that changes. Base it on `examples/c/explain_demo.c` (43 lines; its header comment shows the three ways to run it); use only functions that file or `docs/Guide.md:98-123` (section 2) already uses, so no new API is invented here; if a needed function is not there, stop.
  * **`gate.start`.** `scripts/check/start.py [<root>]` (stdlib, Python 3.8; `<root>` defaults to the repository), `ctest -R gate.start`, registered beside `gate.limits` in `cmake/ConfigForgeGates.cmake` as G-1a registers `gate.decisions` (the Python lookup of the note at the top). Walks `docs/start/*.md`, finds every `<!-- from <file> start:<n> -->` and `<!-- output <file> -->`, and compares. Failure line: `start: <page>:<line>: block differs from <file> start:<n>`. A page with no marker fails (`start: <page>: no code is tied to a file`). With a language whose pages are not yet written, nothing is checked: the gate passes on an empty `docs/start/`.
  * **D-160, text to be added (the file `docs/decisions/D-160.md`, G-1b having landed, and an index row `| [D-160](decisions/D-160.md) | A page's code is a tested file | documents | **Decided** |`; before G-1b this card does not open):** "## D-160 — A page's code is a tested file. Question: how does a short guide stay true? Options: code typed into the page; code extracted from the page into a test; the page includes regions of a file that a test builds and runs. Trade-off: the first rots; the second makes the test the page's hostage; the third keeps one source and one check. Decision: the page includes marked regions of an example, and `gate.start` compares them; the example is built and run by its own test, and the page's output block is the example's recorded output. Consequence: a change to an example that changes a region fails `gate.start` until the page follows."
* **files**: `examples/c/start.c` (new), `examples/c/start.expected` (new), `examples/c/CMakeLists.txt`, `docs/start/c.md` (new), `scripts/check/start.py` (new), `tests/work/test_start.py` (new), `tests/work/CMakeLists.txt`, `cmake/ConfigForgeGates.cmake`, `mkdocs.yml`, `README.md` (one line), `docs/Guide.md` (one line near lines 1-10), `docs/decisions/D-160.md` (new) and its row in `docs/Decisions.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-3 main`.
  2. Test first, `tests/work/test_start.py` (arguments `<scripts/check/start.py> <build dir>`, registered as `work.start`): a temp tree with `docs/start/x.md` and `examples/x.c`; cases: block equal passes; one line changed fails with the message above; a missing region fails; a page with no marker fails; an output block that differs from `.expected` fails; an empty `docs/start/` passes.
  3. See it fail (no script), write `scripts/check/start.py`, see it pass: `ctest --preset default -R work.start`.
  4. Write `examples/c/start.c` (at most 40 lines, C89, regions 1 to 5 as the page's headings 2 to 5 need) from `explain_demo.c`. Build and run: `cmake --preset default && cmake --build --preset default --target start && ctest --preset default -R "example\.start$"`; record the output into `examples/c/start.expected` (strip no line; the example prints no time or path that varies, else the example is changed so it does not).
  5. Write `docs/start/c.md` by the form above. The first line after the title: "Five minutes, from the build to a value and the reason it has that value. Everything here runs: the code is `examples/c/start.c`."
  6. Register `gate.start`; `ctest --preset default -R "gate\.start"` exit 0. See it fail once: change one character in a region of the example, run, restore.
  7. Add the nav group "Start here" (`mkdocs.yml`, before Guide) with `C: docs/start/c.md`; the README and Guide lines; D-160; CHANGELOG "## 2.3.0 — unreleased" "### Added": "`docs/start/c.md`, a page from the build to `explain()` in C, its code tested, and `gate.start` (G-3, D-160)"; Roadmap: item `**"Start here", one page a language.**` (`docs/Roadmap.md:942`, now ⬜) is 🟨 (C done; C++, Rust, Python are G-4 to G-6): rewrite its marker, keep the text.
  8. `python -m mkdocs build --strict 2>&1 | tail -n 20` (where installed); format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `ctest --preset default -R "gate\.start|example\.start|work\.start"` exit 0.
* **stop if**: no function the example needs is in the Guide's section 2 or `explain_demo.c`; the example's output varies between runs; three failures on the same thing.
* **docs**: D-160 (text above); CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap `"Start here", one page a language.` (🟨).

<a id="g-4"></a>
## G-4 - "Start here" in C++

* **id**: G-4
* **goal**: `docs/start/cpp.md` is G-3's page in C++, its code `examples/cpp/start.cpp`, checked by `gate.start`.
* **why**: as G-3, for the C++ reader, who is told in the Guide to read section 13 after twelve others (docs/Guide.md:1399).
* **needs**: G-3
* **size**: small (G-3 decided the form)
* **version**: 2.6.0
* **status**: open
* **decided**:
  * The form, the markers (`// start:<n>`, `// end:<n>`), the six headings and the gate are G-3's and D-160's. The program is the C one in C++: `cf::` calls from `docs/Guide.md:1399-1472` (section 13), no call that section and `examples/cpp/tier1.cpp` do not show. Heading 1 gives the one CMake line, `target_link_libraries(app PRIVATE ConfigForge::configforge)` (Guide:62) and says the header is the same.
  * Built by `examples/cpp/CMakeLists.txt`: add `start` to the `foreach(ex tier0 tier1 layers ...)` list (lines 5-16; the target is `start_cpp`, C++98 with `-pedantic -Werror`, so the program is C++98: no `auto`, no `nullptr`), and run with `example_expect(start_cpp ${CMAKE_CURRENT_BINARY_DIR}/start-run <regex>...)` as G-3 does for C (the test is `example.start_cpp`; `tier0`/`tier1` use a bare `add_test`, not the model).
* **files**: `examples/cpp/start.cpp`, `examples/cpp/start.expected`, `examples/cpp/CMakeLists.txt`, `docs/start/cpp.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-4 main`.
  2. See it fail: write `docs/start/cpp.md` first with the markers pointing at `examples/cpp/start.cpp`; `ctest --preset default -R "gate\.start"` fails (no file).
  3. Write `examples/cpp/start.cpp` (at most 40 lines), build, run, record `start.expected`.
  4. `ctest --preset default -R "gate\.start|example\.start_cpp"` exit 0.
  5. Nav, README, Guide lines; CHANGELOG ("Added", one line, G-4); the Roadmap item stays 🟨 until G-6.
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` green; `ctest --preset default -R "gate\.start"` exit 0.
* **stop if**: a call the page needs is in neither section 13 nor `tier1.cpp`; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; no D-number; Roadmap unchanged (🟨).

<a id="g-5"></a>
## G-5 - "Start here" in Rust

* **id**: G-5
* **goal**: `docs/start/rust.md` is G-3's page in Rust, its code `examples/rust/start.rs`, checked by `gate.start`.
* **why**: as G-3, for the Rust reader (Guide section "Rust", docs/Guide.md:1511).
* **needs**: G-3
* **size**: small
* **version**: 2.6.0
* **status**: open
* **decided**:
  * Form, markers (`// start:<n>`) and gate as G-3. Heading 1 gives the Cargo line the Guide gives (read `docs/Guide.md:1511-1562`; if it names a path dependency, the page says so and does not invent a crates.io name: publishing is the maintainer's, 1.10.0). The program uses the calls of `examples/rust/tier0.rs` and the Guide's Rust section only.
  * Built and run as the other Rust examples are: cargo finds an example through a `[[example]]` entry in `bindings/rust/Cargo.toml` (lines 28-31 show `tier0`: `name = "start"`, `path = "../../examples/rust/start.rs"`), and `examples/rust/CMakeLists.txt` registers `rust_expect(start ${CMAKE_CURRENT_BINARY_DIR}/start-run <regex>...)` (test `example.start_rust`; `file(MAKE_DIRECTORY)` for the directory as `tier0-run` is made there), one regex a line of `start.expected`. A machine without cargo skips the example test as the others are; `gate.start` still checks the page against the file and the recorded output.
* **files**: `examples/rust/start.rs`, `examples/rust/start.expected`, `bindings/rust/Cargo.toml`, `examples/rust/CMakeLists.txt`, `docs/start/rust.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/G-5 main`.
  2. See it fail: page first, markers pointing at the missing file; `ctest --preset default -R "gate\.start"` fails.
  3. Write `examples/rust/start.rs` (at most 40 lines); `cargo fmt --check` and `cargo clippy` on it as the rust examples are checked (`grep -n "clippy\|fmt" .github/workflows/ci.yml | head`); run it; record `start.expected`.
  4. `ctest --preset default -R "gate\.start|example\.start_rust"` exit 0 (the preset needs `-DCF_RUST=ON`: `docs/Guide.md:1553`; without cargo the test is absent, say so).
  5. Nav, README, Guide lines; CHANGELOG (G-5).
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI (the rust job included); `ctest --preset default -R "gate\.start"` exit 0; `cargo fmt --check` and `cargo clippy -- -D warnings` clean on the example.
* **stop if**: the Guide's Rust section names no way to depend on the crate; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; no D-number.

<a id="g-6"></a>
## G-6 - "Start here" in Python

* **id**: G-6
* **goal**: `docs/start/python.md` is G-3's page in Python, its code `examples/python/start.py`, checked by `gate.start`, and the Roadmap item `"Start here", one page a language.` is done.
* **why**: as G-3, for the Python reader (Guide "Python", docs/Guide.md:1563; install is `python -m pip install .`, line 1567).
* **needs**: G-2 (the mover), G-3, G-4, G-5 (so that the last card marks the item)
* **size**: small
* **version**: 2.6.0
* **status**: open
* **decided**:
  * Form, markers (`# start:<n>`) and gate as G-3. Heading 1 is the Guide's install line (docs/Guide.md:1567) and nothing else. The program uses the calls of `examples/python/tier0.py` and the Guide's Python section only. Run as `examples/python/CMakeLists.txt:32` does `tier0.py`: `file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/start-run)` and `python_expect(start ${CMAKE_CURRENT_BINARY_DIR}/start-run <regex>...)` (test `example.start_python`), one regex a line of `start.expected`.
  * The item is marked 🟩 here, and `python scripts/docs/move_done.py` (G-2) moves it to `docs/Done.md`.
* **files**: `examples/python/start.py`, `examples/python/start.expected`, `examples/python/CMakeLists.txt`, `docs/start/python.md`, `mkdocs.yml`, `README.md`, `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Done.md`.
* **steps**:
  1. `git switch -c work/G-6 main`.
  2. See it fail: page first; `ctest --preset default -R "gate\.start"` fails.
  3. Write `examples/python/start.py` (at most 40 lines; `ruff check` and `ruff format --check` clean, as `gate.ruff`); run it; record `start.expected`.
  4. `ctest --preset default -R "gate\.(start|ruff)|example\.start_python"` exit 0.
  5. Nav, README, Guide lines; CHANGELOG (G-6); Roadmap item 🟩; `python scripts/docs/move_done.py`.
  6. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gate\.(start|ruff)"` exit 0; `python scripts/docs/move_done.py --check` exit 0.
* **stop if**: the Python package cannot be built on the machine and no example test can run: report it, do not mark 🟩; three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap `"Start here", one page a language.` 🟩.

<a id="g-7"></a>
## G-7 - Recipes: reload events as log records, a report in OPA and CUE

* **id**: G-7
* **goal**: the Guide has a "Recipes" section with two recipes whose code is tested: each reload's events as OpenTelemetry log records, and a report as the input to an OPA (Rego) policy and to a CUE check about values per profile.
* **why**: the fingerprint as a resource attribute is in the Guide (docs/Guide.md:1286-1296); the other two halves of the item remain (docs/Roadmap.md:938-941). A recipe is a page of someone else's tool's syntax: the library gives no code for it (no tracing SDK, no policy engine is a dependency).
* **needs**: G-2 (the mover), G-3 (the markers and `gate.start`)
* **size**: medium
* **version**: 2.6.0
* **status**: open
* **decided**:
  * **Place.** A new `### Recipes` heading at the end of Guide section 11 ("The editor, the report and Doctor", docs/Guide.md:1315-1361), before `## 12. Small targets` (line 1362); the contents list (docs/Guide.md:1-25) needs no new line (a `###`). The existing OpenTelemetry paragraph (Guide:1291) stays where it is; the Recipes section's first sentence links to it as `[the fingerprint recipe](#...)` with the anchor mkdocs makes from the heading just above it (`grep -n "^#" docs/Guide.md | sed -n` near 1280; stop to ask if that paragraph has no heading, and use the link `#11-the-editor-the-report-and-doctor`).
  * **Recipe 1: log records.** The program's own `cf_log_event` callback (docs/Guide.md:1656-1665) maps an event to an OpenTelemetry log record: severity from `e->level`, body `e->message`, attributes `configforge.event` (`e->event`), `configforge.key`, `configforge.source`, `configforge.code` (`e->code`) (all fields of `cf_log_event`, `lib/config/include/configforge/cf_log.h:45-56`; `key` and `source` may be NULL: the attribute is then left out). Shown as C with a stand-in `otel_log_emit(severity, body, n, attrs)` the reader supplies (as the fingerprint recipe's `otel_resource_set`, Guide:1286), the library's side being real: the example is `examples/c/recipe_otel.c`, compiled and run, with the stand-in printing the record as one JSON line; the page's fence is tied to it by G-3's markers (so G-3 must have landed: this card needs it). Reload events come from `cf_live_options.log`, `log_user`, `log_min` (`lib/config/include/configforge/cf_live.h:140-148`) and the example uses `examples/c/reload.c`'s recipe (Guide:1050-1066) for its trigger. It is run with `example_expect(recipe_otel ${CMAKE_CURRENT_BINARY_DIR}/recipe-otel-run "\"configforge.event\"" ...)` as G-3 registers `start` (add `recipe_otel` and `recipe_report` to the second `foreach` list of `examples/c/CMakeLists.txt`).
  * **Recipe 2: OPA and CUE.** The input is the JSON report, written by `cf_report_write(ks, ds, sv, &info, buf, cap)` (`lib/config/include/configforge/cf_report.h:65`; there is no `cf_report_json`; shape in D-16: `docs/decisions/D-16.md`, G-1b having landed); `examples/c/secrets.c:60-110` shows a program building one (`cf_report_info info = CF_REPORT_INFO_INIT`, `info.profile` is the string the report calls `"profile"`, `info.mode = CF_REPORT_FULL`). A second new example, `examples/c/recipe_report.c` (C89, from `secrets.c`), writes two reports to the two paths on its command line: `report-bad.json` (a secret key set from a file, `server.port` 80, a value from source `cli`, profile `"prod"`) and `report-good.json` (none of these, profile `"prod"`). The recipe shows three rules in Rego (a key whose report entry has `"sensitive": true` must not have its winning source be a file; `server.port` in profile `prod` must be above 1023; a key whose winning source is named `cli` is refused in `prod`) and the same three in CUE (`#Report` constraints, `cue vet -d '#Report' report.json schema.cue`). Key names are the example's own (use the report's JSON paths of D-16 for "winning source": read `docs/decisions/D-16.md`'s example and write the rule on its field names, not on guessed ones). Profiles are only the report's `profile` string: no example has a profile mechanism, none is needed.
  * **Tests.** `scripts/check/recipes.cmake` (new; `-DEXE=<recipe_report>` `-DSRC=<root>` `-DOPA=<path>`/`-DCUE=<path>`), registered in `examples/CMakeLists.txt` as `recipes.opa` and `recipes.cue` only when `find_program(OPA_EXE opa)` / `find_program(CUE_EXE cue)` finds the tool, else `message(STATUS "recipes.opa skipped: opa not found")` (the pattern of `scripts/verify.cmake:700-708`, which prints "skipped" and continues): the script runs `recipe_report`, then `opa eval --format raw --input <report> --data <rego> 'data.configforge.deny'` must print the expected denial list for a report that breaks a rule and an empty list for one that does not; `cue vet` must fail for the first and pass for the second. CI installs neither in this card (a later card's choice: adding a tool to the CI image is the maintainer's, the pinned-tools list `scripts/buildenv/setup.sh`). The Rego and CUE files are `docs/recipes/report.rego`, `docs/recipes/report.cue` and the page fences are tied to them by the same markers as G-3 (the line `<!-- from docs/recipes/report.rego start:1 -->` before the fence; in the Rego file `# start:1`/`# end:1`, in CUE and C `// start:1`/`// end:1`); `gate.start` walks `docs/start/*.md` only, so this card extends it to also walk `docs/Guide.md`, where a page with no marker does NOT fail (that rule stays for `docs/start/*.md`; a test case in `tests/work/test_start.py` for it, `# start:` accepted as a marker too). The `docs/recipes/*.json` reports are written once by `recipe_report` and committed; `recipes.cmake` rewrites them into the build directory and compares with the committed ones, so a change to the report's shape fails.
  * **No D-number**: a recipe decides nothing a reader of the code needs.
* **files**: `examples/c/recipe_otel.c` (new), `examples/c/recipe_report.c` (new), `examples/c/CMakeLists.txt`, `docs/recipes/report.rego` (new), `docs/recipes/report.cue` (new), `docs/recipes/report-bad.json` and `report-good.json` (new, written by `recipe_report` and kept), `scripts/check/recipes.cmake` (new), `scripts/check/start.py`, `tests/work/test_start.py`, `examples/CMakeLists.txt` (the two recipe tests), `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Done.md`.
* **steps**:
  1. `git switch -c work/G-7 main` (after G-2 and G-3).
  2. Test first: write `examples/c/recipe_report.c`, `scripts/check/recipes.cmake` and the two JSON reports from a real run of the example; `ctest -R recipes` fails (no rego, no cue).
  3. See it fail: run it; the failure names the missing file. Write `report.rego` and `report.cue`; both tests pass where `opa` and `cue` are installed (`opa version`, `cue version`); on a machine without them record in the commit message that the two were not run, and CI will not run them either (see decided).
  4. Write `examples/c/recipe_otel.c`; build and run; its output is a JSON line per event; the expect regex proves `"configforge.event"` is in it.
  5. Write the Guide section; extend `scripts/check/start.py`'s walk; `ctest --preset default -R "gate\.start|recipes|example.*recipe"` exit 0; `gate.codespell` and `gate.links` on the new files.
  6. CHANGELOG "## 2.3.0 — unreleased" "### Added" (G-7); Roadmap `**Recipes, not code**` (`docs/Roadmap.md:938`, now 🟨) becomes 🟩 and its parenthesis about the fingerprint recipe is deleted as done; then `python scripts/docs/move_done.py` (G-2).
  7. Format, codemap, apiref, ACCEPT.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; `ctest --preset default -R "gate\.start|example.*recipe"` exit 0; `ctest -R recipes` exit 0 where `opa` and `cue` are installed, "skipped" printed where they are not.
* **stop if**: `secrets.c` cannot be reduced to a program that writes the two reports without a library change; the log-record mapping needs a field `cf_log_event` does not carry (read `cf_log.h`: the card is split instead of the library changed); three failures on the same thing.
* **docs**: CHANGELOG "## 2.3.0 — unreleased" "### Added"; Roadmap "Recipes, not code" 🟩; no D-number.

<a id="g-8"></a>
## G-8 - A playground on the documentation site

* **id**: G-8
* **goal**: the documentation site has a page `playground.html` where a reader pastes a contract and a file, presses a button, and sees `explain()` and the provenance, the library running as WebAssembly in the browser, nothing installed.
* **why**: "Being found" (docs/Roadmap.md:906-915): a newcomer sees why a value is what it is before they install anything. A WebAssembly build exists (tier 2, D-55, `cmake/toolchains/wasm32-wasi.cmake`, `verify -DEXTRA=wasm` at `scripts/verify.cmake:695-724`); it runs the unit tests under Node, never in a browser.
* **needs**: G-1b (D-162 is a file), G-3 (the page's example text is the C start page's contract)
* **size**: large (a build, an in-memory entry point, a page, a workflow change)
* **version**: 2.6.0
* **status**: open
* **decided**:
  * **No new dependency (D-162, below).** The browser has no WASI. The library built for `wasm32-wasi` imports a handful of `wasi_snapshot_preview1` functions (`fd_write`, `proc_exit`, `clock_time_get`, `environ_*`, `args_*`, `random_get`: the real list is printed by `wasm-objdump -x` or, without it, by instantiating with an empty import object and reading the `WebAssembly.Module.imports` error): the page's script provides them in about 60 lines (writes to a buffer, a clock, empty environment, a fixed random). No shim library from a CDN, no file system. Therefore the playground feeds the library from memory, never a path.
  * **The entry point.** `playground/playground.c` (new), C89, exports (via `__attribute__((export_name("...")))` guarded by `#ifdef __wasm__`, else plain functions so a host build can test it): `cf_play_input(const char* contract_text, const char* file_name, const char* file_text, const char* key)` returning a `const char*` into a static buffer of 64 KiB (a longer result is cut with the line `... cut at 64 KiB`), `cf_play_alloc(size_t)` and `cf_play_free(void*)` for the page to place its strings. It builds a configuration whose one source is the pasted file read from memory by format name taken from `file_name`'s extension (`.toml`, `.json`, `.yaml`, `.ini`) and prints `cf_explain` for `key` (or `cf_report_write` text for the whole configuration when `key` is empty). The memory-source call is `cf_builder_document(b, name, rank, bytes, n, format, flags)` (`flags` 0) (`lib/config/include/configforge/configforge.h:162`; its source form `cf_source_document` is `cf_source.h:~205-212`: bytes and a format name, copied by the builder); `format` is `"toml"`, `"json"`, `"yaml"` or `"ini"` from `file_name`'s extension, and a format not compiled into the `minimal` profile is refused with the library's own message, shown as the result. (`cf_source_by_content` is `CF_INTERNAL_API` and takes a codec and a blob: not used.) Read the exact `cf_builder_document` parameters before writing the entry point; if it takes anything the page cannot give, stop.
  * **A contract pasted in.** The contract (Guide section 3, docs/Guide.md:124-223) is C code (`cf_builder_define`, `cf_builder_text` and the typed helpers, `configforge.h:279-330`); no text language for a contract exists in the library (`configforge schema --from` and `doctor gen` write C or JSON Schema from a file: not used here). So the playground's contract is a form of rows (key, type, default, min, max) which the page turns into calls: `cf_play_define(key, type, default_text, min, max)` (type one of `string`, `int`, `bool`, `float`) called once a row before `cf_play_input`, which drops the rows when it returns. The page's first row set is G-3's `start.c` contract (read it: `examples/c/start.c`). Stop if `cf_builder_define` cannot express a row from text alone (it needs a `cf_def` the entry point cannot fill from those five fields).
  * **The build.** `playground/CMakeLists.txt`, added in the top `CMakeLists.txt` with `add_subdirectory(playground)` inside `if(CF_BUILD_TESTING)` beside `tests` (lines 165-168) for the host test, whose `playground` wasm target is created only `if(CMAKE_SYSTEM_NAME STREQUAL WASI)` (`cmake/toolchains/wasm32-wasi.cmake:11`), linking the static libraries with `-Wl,--export-dynamic -Wl,--no-entry`, producing `playground.wasm`; a host build gains the host test only. (Register the host test with `add_executable` and `add_test` as `tests/` does; read `cmake/ConfigForgeComponent.cmake:284-330` first: `cf_unit_tests` may serve only `lib/` components.) Configured with `--toolchain cmake/toolchains/wasm32-wasi.cmake -DCF_PROFILE=minimal` (the smallest library: no threads, no file notification, as the toolchain says); the size of the file is printed and must be under 1 MiB (else stop: the card says what grew).
  * **The page.** `docs/playground.md` (a page with a `<textarea>` for the contract, one for the file, a key field, a button and a `<pre>` for the result) and `docs/playground/play.js` (the shim and the glue, at most 150 lines, no framework, no CDN) and `docs/playground/configforge.wasm` (built, never committed: `.gitignore`d beside `/docs/performance/` (`.gitignore:66`), as the benchmark's SVGs are, `.github/workflows/docs.yml:30-43`; add `/docs/playground/configforge.wasm`). A page without the wasm file says "The playground is built with the site; this copy was built without it" (the script catches the failed fetch). A reader with JavaScript off sees the same sentence.
  * **CI.** `.github/workflows/docs.yml` gains, before "Check and build the documentation" (docs.yml:44), a step "Build the playground" that installs clang, lld and wasi-libc (`apt-get install -y clang lld wasi-libc libclang-rt-21-dev-wasm32 nodejs`: the package names of `scripts/buildenv/setup.sh:21-23` (21 on Ubuntu 26.04; the toolchain's comment, `cmake/toolchains/wasm32-wasi.cmake:5-7`, says 18 and is stale: correct it to 21 in this card). Use the runner image `.github/workflows/docs.yml` already runs on; if it is not 26.04, the card installs what that image's apt names, read `grep -n "runs-on" .github/workflows/docs.yml`, and reports the version) and runs the two cmake commands, copying `build-wasm/playground/playground.wasm` to `docs/playground/configforge.wasm`. A failure of that step fails the docs job (no `continue-on-error`): the claim of the page is that it is live. `scripts/check/check_docs.py:12-21` gains `"docs/playground/index.html"` and, when the job built it, `"docs/playground/configforge.wasm"` (the script reads the environment variable `CF_PLAYGROUND=1` set by the workflow step).
  * **Tests.** `playground/tests/test_play.c` built with the host compiler (the plain-function path of `#ifdef __wasm__`), registered in `playground/CMakeLists.txt` as `playground.host`, three cases: a TOML file and a contract give the explain text of `examples/c/explain_demo.c`'s style; a file with a syntax error gives the refusal text (not a crash); a result over 64 KiB is cut with the marker. And `scripts/check/playground_node.js` (stdlib Node, like `scripts/check/wasi_run.js`): loads `configforge.wasm` through `play.js`'s shim (the shim is a module `docs/playground/shim.js` exporting `imports(buffer)`; `play.js` imports it) and asserts the same first case; run by `ctest -R playground.node` where `node` and the wasm exist, skipped with a note elsewhere.
  * **D-162, text to be added:** "## D-162 — The playground is the library, in the page, with no shim library. Question: how does a reader try the library with nothing installed? Options: a server that runs it (a service to run and secure); WebAssembly with a WASI shim from a package registry (a dependency and a supply chain for a page); WebAssembly with the 60 lines of imports this library needs, fed from memory. Decision: the third. The library takes its input from memory and prints to a buffer, so the page needs no file system; the wasm file is built with the site and never committed. Consequence: the playground is the library's own tier-2 build; if it breaks, the docs job fails."
* **files**: `playground/playground.c`, `playground/CMakeLists.txt`, `playground/tests/test_play.c` (new), `CMakeLists.txt` (one `add_subdirectory`), `docs/playground.md`, `docs/playground/play.js`, `docs/playground/shim.js` (new), `scripts/check/playground_node.js`, `.github/workflows/docs.yml`, `scripts/check/check_docs.py`, `mkdocs.yml` (nav: Home group, `Playground: docs/playground.md`), `.gitignore`, `cmake/toolchains/wasm32-wasi.cmake` (comment: 18 to 21), `docs/decisions/D-162.md` (new) and its row in `docs/Decisions.md` (`| [D-162](decisions/D-162.md) | The playground is the library, in the page | documents | **Decided** |`), `docs/Dependencies.md` (clang, lld, wasi-libc already named at line 70: add "and the playground"), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/G-8 main`.
  2. Test first: `playground/tests/test_play.c` with the three cases; `cmake --preset default && ctest --preset default -R playground` fails (no source).
  3. See it fail: build errors name `playground.c`. Write `playground/playground.c`; the host test passes.
  4. The wasm build: `cmake -S . -B build-wasm --toolchain cmake/toolchains/wasm32-wasi.cmake -DCF_PROFILE=minimal -DCF_BUILD_TESTING=ON && cmake --build build-wasm --target playground`; `ls -l build-wasm/playground/playground.wasm` under 1 MiB. Where clang, wasm-ld or node is missing, say so and do the rest; CI judges (as `verify -DEXTRA=wasm` skips).
  5. Test first for the shim: `scripts/check/playground_node.js` asserting case one; see it fail (no shim), write `docs/playground/shim.js`, pass: `node scripts/check/playground_node.js build-wasm/playground/playground.wasm`.
  6. Write `docs/playground.md` and `docs/playground/play.js`; load `docs/playground.md` through `python -m mkdocs build --strict` and, with the wasm copied in, open `build/docs-site/docs/playground/index.html` through a local server (`python -m http.server -d build/docs-site`) and with the browser pane press the button; the `<pre>` shows the explain text. Say in the report what was seen.
  7. `.github/workflows/docs.yml` and `check_docs.py` as decided; `actionlint` on the workflow (`ctest -R gate.actionlint`).
  8. D-162 as the file `docs/decisions/D-162.md` and its index row (text above); Dependencies line; CHANGELOG "## 2.3.0 — unreleased" "### Added" (G-8); Roadmap "Being found": 🟨 with the line "playground done (G-8); the comparison page, the recording and the articles are not cards yet" (the item says "Done when the playground is live and the articles are published": the articles are the maintainer's writing, so the item stays 🟨).
  9. Format, codemap, apiref, ACCEPT. After the push, `gh run watch` the `docs` workflow on the branch: green and `playground.wasm` in the uploaded artifact.
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI with `docs` and the new "Build the playground" step green; `ctest --preset default -R "playground"` exit 0; `node scripts/check/playground_node.js` exit 0 where node and the wasm exist; the page seen working in the browser pane.
* **stop if**: `cf_builder_document` takes something the page cannot give (see decided); `cf_builder_define` cannot express a row from the five form fields; the wasm file exceeds 1 MiB at `CF_PROFILE=minimal`; the Pages deployment needs a setting only the maintainer has (repository settings: write it under notes as a parked G-8-m, do not change it); three failures on the same thing.
* **docs**: D-162 (text above); CHANGELOG "## 2.3.0 — unreleased" "### Added"; docs/Dependencies.md; Roadmap "Being found." (🟨).

<a id="g-9"></a>
## G-9 - A program not written for the library, moved onto it in a fork

* **id**: G-9
* **goal**: one small open-source daemon's configuration code is replaced by this library in a local fork, and what that showed is written as findings, each a fix or an idea in the Roadmap.
* **why**: every example here was written for the library (docs/Roadmap.md:754-759); a real program shows what the Guide does not.
* **needs**: nothing (G-3 helps: its page is the first thing the port follows)
* **size**: large (a port of unfamiliar code; the card stops at ~60 turns and splits)
* **version**: 2.3.0
* **status**: open
* **decided**:
  * **Decided: tinyproxy** (the maintainer's standing approval, 2026-11-27). Criteria, so the options are comparable: a C daemon of under 20,000 lines; its configuration read from one file of key-value or INI-like text by its own parser in one or two source files; a licence that lets a fork carry a library under MIT or Apache (GPL-2 or later, BSD, MIT, ISC); builds with `make` or CMake on Linux without a package the machine lacks; has a test or a way to start and check it. tinyproxy (HTTP proxy, upstream `https://github.com/tinyproxy/tinyproxy`, GPL-2.0-or-later) meets all of them: its directives (Port, Listen, Allow, Timeout, LogLevel) are the plain keys, ranges and defaults a contract expresses and its parser is one file, `src/conf.c`. Rejected: **inadyn** (DDNS client, GPL-2.0: its configuration is read by libConfuse, so the port would replace a library, not a parser of its own); **darkhttpd** (ISC: no configuration file, its flags would be the contract, which shows only the flag side). What to adopt is decided here; publishing the fork stays the maintainer's (G-9-m).
  * **What the port keeps.** The daemon's behaviour with its own sample configuration: the same listening port, the same log level, the same refusals for a bad value (the daemon's wording of an error need not be kept: a refusal that names the key and the source is the library's and is a finding of its own).
  * **Where.** A clone in `build-g9/tinyproxy` (inside `.gitignore`d `build-*`; not in this repository), a branch `configforge-port` in it, commits in the clone, never a push. The findings go into this repository: `docs/plan/G-9-findings.md`? No new document type: the findings are written as a "notes" section of this card and as Roadmap ideas (one `*` bullet each under "### From a review of the whole, 2026-11-14"'s style heading "### Found by porting tinyproxy (<date>)", marked ⬜).
  * **What to measure and write.** Lines of configuration code removed and added (`git diff --shortstat` of the clone, parser file only); what in the contract the daemon's keys needed that the Guide does not show; each place the port was awkward (an API that wants a type the daemon's code does not have; a message the daemon's operators would not understand); whether `explain()` answered the question an operator of this daemon asks ("why is the listen address what it is"). Each finding is one line, `kind: fix|idea`, the Guide section or header it concerns.
  * **Publishing.** Pushing the fork to a public place and writing about it is the maintainer's, a card of its own: **G-9-m** (parked, 2.3.0, waits on "the maintainer publishes the fork and the text").
* **files**: the clone (outside the tree); in this repository only `docs/plan/G-guides.md` (this card's notes), `docs/Roadmap.md` (the findings as ideas), `CHANGELOG.md` (nothing unless a fix lands).
* **steps**:
  1. `git switch -c work/G-9 main`.
  2. `git clone https://github.com/tinyproxy/tinyproxy build-g9/tinyproxy && git -C build-g9/tinyproxy switch -c configforge-port`; build it unchanged; run it with its sample configuration and record what it does (`tinyproxy -h` or its log's first lines) into the card's notes.
  3. Write the contract for its keys in the daemon's own style, tied to the sample configuration (the "See it fail" is that the daemon, built against this library with the old parser removed, refuses the sample file until the contract matches: run it, see the refusal, fix the contract).
  4. Replace the parser by calls to this library (`add_subdirectory` of this repository's path in the clone's build, or `pkg-config configforge` from a local install: the card uses the second so the clone shows what a user sees).
  5. Run the daemon with the sample configuration and with three bad files (a wrong type, an out-of-range value, an unknown key); compare with step 2's recording.
  6. Write the findings under "notes" and the Roadmap ideas; run `ctest --preset default -R "gate\.(links|codespell)"` on the edited documents.
  7. Commit the notes and the ideas to `work/G-9`; format, codemap, apiref, ACCEPT (documents only).
* **accept**: ACCEPT-LINUX; ACCEPT-WINDOWS; ACCEPT-CI; in the clone: the daemon runs with the sample configuration and the recorded behaviour of step 2 is reproduced (state each difference); each finding is a line under "notes" and an item in the Roadmap or a fix.
* **stop if**: the daemon cannot be built on the machine; the port needs more than ~600 changed lines in the clone (stop at a working part and say which keys are ported); three failures on the same thing.
* **docs**: CHANGELOG nothing; Roadmap "Used by a program not written for it." stays ⬜ until the maintainer publishes (G-9-m) and 🟨 when the findings are written.
* **Organiser's answers**:
  * The program is tinyproxy, by the card's criteria and recommendation; inadyn and darkhttpd rejected as the "decided" bullet says. The card opens at 2.3.0; publishing is G-9-m, the maintainer's (the maintainer's standing approval, 2026-11-27).

<a id="g-9-m"></a>
## G-9-m - Publishing the fork and what it found

* **id**: G-9-m
* **goal**: the ported daemon is published and the findings are written up where people read them.
* **why**: the Roadmap's "done" is "the change is published with what it found" (docs/Roadmap.md:758-759); publishing is the maintainer's.
* **needs**: G-9
* **size**: small (the maintainer's own steps)
* **version**: 2.3.0
* **status**: parked: not now
* **decided**: a public repository under the maintainer's account, a README of the fork stating what changed and what was found (G-9's notes), a link from the Roadmap and the Guide's examples list (docs/Guide.md:26-54). Waits on: the maintainer's account and the choice of place.
* **files**: none in this repository but `docs/Guide.md` and `docs/Roadmap.md` (a link).
* **steps**:
  1. The maintainer pushes the clone's branch `configforge-port` to a public repository.
  2. A worker adds the link and one paragraph to the Guide's examples section and marks the Roadmap item 🟩.
* **accept**: the link resolves (`ctest -R gate.links`).
* **stop if**: the upstream's licence forbids a published fork carrying the change (read the licence file first).
* **docs**: Roadmap "Used by a program not written for it." 🟩.
