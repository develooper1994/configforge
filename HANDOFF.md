# HANDOFF (2026-11-26)

State: main has the inventory (2395d65: finished cards removed, Roadmap
items marked) and the index's Version column. Card drafts, not yet read
cold, are on main in docs/plan (moved from their branches, which are
deleted), from: `plan/cards-c1` (docs/plan/C-change.md,
C-1a to C-6b), `plan/cards-c2` (docs/plan/C-change-2.md, C-7 to C-11),
`plan/cards-f` (docs/plan/F-files.md, F-1a to F-4b), `plan/cards-l`
(docs/plan/L-tools.md, L-11 mutation testing with Mull, from a measured
run). Also `plan/cards-k` (docs/plan/K-correctness.md, K-1a to K-8,
20 cards; its TO DECIDE: K-4d, K-6a, K-6b, K-7a, K-7b, K-8), and `plan/cards-g`
(docs/plan/G-guides.md, G-1a to G-9 and the parked G-9-m), and `plan/cards-p`
(docs/plan/P-measured.md, P-5 to P-16 and the parked P-15-m; TO DECIDE: P-13). Codex has no allowance until 2026-11-29 09:47; Claude carries on.

The order (the maintainer, 2026-11-26, relayed from a side session):
every open item in docs gets a card before any card is carried out; each
card is read cold by a worker that did not write it; all land on main;
then the cards are carried in index order and each is deleted when done.
Groups: 1 C (people who change configuration), 2 F (files and safety),
3 K (correctness), 4 X (formats), 5 L (tools), 6 G (documents), 7 P
(vector library and optimisation), 8 M (the maintainer's items).

Waiting for the maintainer, asked in chat, not answered there: a relayed
message said every TO DECIDE is approved by its recommendation; it was
not written into Plan.md because it did not come from the maintainer
directly. Until the maintainer says so in their own session, these
cards keep their TO DECIDE: C-2a (`cf_unit`, `cf_def.unit`), C-9
(splitting one key into two), C-10a and C-10b (Windows; `http://`
targets), F-1b (`cf_emit_file`), F-2a (`cf_builder_overrides`), F-3j
(`cf_docedit`), F-4b (`doctor graph`).

## Steps

1. Write the remaining groups' cards, one worker at a time while the
   Claude window is past half: (K, G, P, L-12 on and M done) X. Then group Y, the maintainer's five ideas of 2026-11-26
   (docs/Roadmap.md, "From the maintainer, 2026-11-26": concurrent reads
   and writes by choice, compression, an analysis tool, a configuration
   compiler, threads and processes made easier): FILE
   docs/plan/Y-asked.md, ids Y-1 on, decisions from D-186; all five
   researched in depth and thought through before any card (the maintainer,
   2026-11-26): for each, a research unit first, by the largest model at
   high effort with web search, writing docs/research/<topic>.md (what the
   world's libraries and tools do, their options and failure modes, what
   fits D-100 to D-108, a recommendation with its reasons); the cards are
   written from that document, and the analysis tool starts by doing what
   the others do before it differs. Each worker
   gets "the common brief" below followed by its group's part, works in
   a worktree `git worktree add .claude/worktrees/cards-<g> -b
   plan/cards-<g> origin/main`, and edits only its one file. Commit and
   push the branch when it reports STATUS: DONE. Done: six branches
   pushed, one card file each.
2. Cold read, per card file, by a worker that did not write it: every
   file:line against main's tree, every step runnable, every "decided"
   complete; it fixes what is wrong in place and lists what it changed.
   Done: each file read, its fixes committed on its branch.
3. Parked cards (ids ending "-m") go into docs/Plan.md's "Waits on the
   maintainer (skipped by the loop)" table, status "parked"; any step
   the C and F drafts left marked for the maintainer moves into one.
   Number the cards in the order of the groups (1 to 7, then M), give
   each its version from docs/Maintaining.md "Versions", add the index
   rows to docs/Plan.md (Order | Card | Version | Size | Needs |
   Status), copy every card file onto main, delete the plan/cards-*
   branches and worktrees, and delete this file in that commit. A card
   whose needs include an unanswered TO DECIDE is parked too (status
   "parked", waits on: the TO DECIDE), and the loop goes past it. Done: one docs-only commit on main.
4. Carry the cards by the loop, the first open card first (docs/Plan.md,
   "The loop").

Mull is installed in `configforge-dev` (0.39.1, LLVM 21) by the spike of
2026-11-26; its report is in `/root/mspike/report/spike.sqlite` there
(not needed: L-11 carries the numbers).

## The common brief

You write work cards for ConfigForge; you change no code. The repository is this worktree.

Read first: AGENTS.md and CLAUDE.md "Habits" (Tokens), and follow them. Then docs/Plan.md lines 116-142 ("A card": its fields) and an example card's form: docs/plan/P-word-at-a-time.md lines 89-140 (P-3). Never read docs/Roadmap.md, docs/Decisions.md, docs/Design.md or docs/plan/A-architecture.md whole: read only the ranges named below (at most ~120 lines at a time), and find a decision with `grep -n "D-<n> " docs/Decisions.md docs/plan/A-architecture.md`. Find code with docs/Codemap.md and grep, then read ranges of at most ~120 lines.

Write the cards listed below into FILE (a new file), one `## <id> - <title>` section each, in the order given, with exactly the fields of docs/Plan.md "A card", as a bullet list: id, goal, why, needs, size, decided, files, steps, accept, stop if, docs (no "Organiser's answers" and no "notes" yet). Begin the file with `# <group title>` and a short preamble: what the group is for, and that each card is read cold against the tree before it opens.

The standard: a middle-tier worker carries a card out without asking anything. So:
- Every name, signature, error wording, limit and file is decided; cite the code a card builds on as file:line of the current tree.
- Steps numbered, the test first, "see it fail" a step of its own, each step with its command.
- accept: ACCEPT-LINUX, ACCEPT-WINDOWS and ACCEPT-CI as docs/Plan.md lines 69-114 say, plus the card's own check.
- docs: the exact documents: CHANGELOG.md under "## 1.9.0 — unreleased", "### Added" (or Changed/Fixed); the Roadmap item to mark 🟩 (its heading text); a new decision when the card decides something a reader of the code must know, numbered from the range given below, with its text written in the card's "decided" so the worker only copies it into docs/Decisions.md.
- The design in docs/plan/A-architecture.md (D-100 to D-108) and docs/Decisions.md is decided: a card follows it and never reopens it.
- A choice that is genuinely the maintainer's (a new public interface whose shape no document settles, a new external dependency, a change that breaks callers before 2.0.0): write it as **TO DECIDE** with the options and your recommendation, and decide everything around it.
- Do not add requirements the Roadmap item does not ask for. One card is one unit that lands as one commit: split an item into "a" and "b" cards when it would be more than ~600 lines of change.
- An item already done in the tree gets no card: say so in the report with the evidence (commit, file:line).

Then end with the report as your final answer: begin with `STATUS: DONE` (or `STATUS: STOPPED` and why), then one index row per card, `| ? | <id> <short title> | <size> | <needs> | open |`, then per card one line: what it decides that the Roadmap item left open, and each TO DECIDE. Under 500 words. Edit no file but FILE.


VERSIONS: every card belongs to one milestone of docs/Maintaining.md lines 99-114 (read that table). Its index row carries it: `| ? | <id> <short title> | <version> | <size> | <needs> | open |`, and the card's docs field names that version's CHANGELOG heading ("## <version> — unreleased"). A TO DECIDE stays a TO DECIDE, with its options and recommendation; decide everything around it. Work that needs the maintainer's own access or an outside party (registry tokens or trusted publishing, repository settings, the OSS-Fuzz application, tags and releases, paid tools, hardware that cannot be reached) is never a step inside an agent card: it is a card of its own, with an id ending in "-m" (e.g. M-4-m), listed in the report as `PARKED | <id> <title> | <version> | <what it waits on>`. An agent card that would depend on it is written so its agent part stands alone (docs/Plan.md, "Waits on the maintainer").


## The groups' parts

### K

FILE: docs/plan/K-correctness.md. Group title: "K - correctness: the readers held to their standards". Card ids K-1 onward, decisions numbered from D-134 to D-140. The items (docs/Roadmap.md line ranges):
1. `.properties` held to Java's `Properties`, value by value: lines 697-713.
2. The other libraries' CVEs and bug reports, as tests here: lines 1201-1234.
3. The readers proved, what remains (the blob, an INI reader; N-11 proved the BOM skip, `cf_fmt` and the integer conversions under CBMC, `tests/proofs`): lines 689-696.
4. Differential fuzzing, what remains: JSON, TOML, XML and CBOR against the libraries P-1b builds beside ours (N-12 did YAML against libyaml, `tests/fuzz/yamldiff*.c`): lines 1347-1355 and 1476-1480.
5. The YAML reader's remaining limits (the TOML and INI limits N-4 left): lines 1323-1332.
6. One diagnostic model across formats, what remains after A-10 and N-13: lines 1333-1346.
7. Formats registered, with what each can do, what remains: lines 1314-1322.
8. The three differences between the YAML readers ("to decide"): docs/Roadmap.md lines 1510-1518, and N-12's notes, items 4 and 5: `git show 70ad1c3^:docs/plan/N-next.md` lines 302-314. One card that fixes each to YAML 1.2's text, with each choice a TO DECIDE carrying its recommendation.

### X

FILE: docs/plan/X-formats.md. Group title: "X - more formats, and what a format is". Card ids X-1 onward, decisions numbered from D-141 to D-150. Read docs/Roadmap.md lines 156-225 first (the section, "asked 2026-11-13"). The items, in this order:
1. Binary formats that describe themselves: MessagePack first, then BSON, UBJSON, Smile, Ion, Bencode (one card each, MessagePack fully; the later ones may follow MessagePack's card as their pattern and say only what differs): lines 168-170.
2. Formats a configuration is really kept in: Apple property lists, HCL, HOCON, KDL: lines 171-172.
3. TLV as it is met, not only as the standards have it: lines 177-183.
4. Formats bound to a schema (Avro, Protobuf, Thrift, FlatBuffers, Cap'n Proto): lines 173-176. These need an external schema and likely a new interface: write one card that settles what can be done without a new public interface, and mark the rest TO DECIDE.
5. Fast, and without copying where it can be: lines 184-187.
6. The serialization sources, laid out by what they share: lines 1013-1037.
Not here: "whole or as a stream" (lines 188-) is the 2.0 stream reader, the maintainer's.

### L, from L-12

FILE: docs/plan/L-tools-2.md. Group title: "L - the tools, continued". Card ids L-12 onward (L-11, mutation testing, is written by the organiser: do not write it), decisions numbered from D-151 to D-158. Read docs/plan/L-tools.md (37 lines) first. The items (docs/Roadmap.md line ranges unless said):
1. MISRA's remainder: the matrix of every rule, the public headers with no `backlog` line, the compliance report: lines 952-974 and 1496-1501; N-19 built `gate.misra` (`git log --oneline --grep N-19`, docs/compliance/).
2. verify sees the version CI sees: lines 995-999.
3. EVA over `numeric`'s float half: line 69 and docs/Safety.md "Proven, for every value: Frama-C EVA" (find with grep); L-3b did the integer half (4c3ce8c, scripts/check/eva.cmake, tests/eva/).
4. Measured against the libraries a program would otherwise use: lines 743-753.
5. Sizes and work predicted, then checked, what remains: lines 364-400 (the file buffer is done).
6. Byte order, word size and a bare-metal target in CI: lines 847-854.
7. Arm supported well: lines 1258-1271.
8. Small embedded systems, what remains (the Cortex-M `.text` and stack gate): lines 1272-1313.

### G

FILE: docs/plan/G-guides.md. Group title: "G - documents a reader can use". Card ids G-1 onward, decisions numbered from D-159 to D-163. Documentation cards still have steps and an accept (the gates that check documents: `gate.links`, `gate.codemap`, codespell, mkdocs build in CI `docs`). The items (docs/Roadmap.md line ranges):
1. Documents kept short: `docs/Decisions.md` split into `docs/decisions/D-NN.md` with the index kept, and the Roadmap's finished items moved to a page of their own: lines 1008-1012. Check every tool that reads Decisions.md or cites it by line (grep scripts/, cmake/, docs/Codemap.md, scripts/work/brief.py) and decide how each follows.
2. "Start here", one page a language: lines 942-951.
3. Recipes, not code, what remains (reload events as log records, OPA and CUE): lines 938-941.
4. Being found: a playground on the documentation site: lines 906-915 (a WebAssembly build exists: `EXTRA=wasm`).
5. Used by a program not written for it: lines 754-759.

### P

FILE: docs/plan/P-measured.md. Group title: "P - the vector library carried further, and optimisation from measurement". Card ids P-5 onward (P-3, in docs/plan/P-word-at-a-time.md, waits for 2.4.0 and is not rewritten; P-4 is done), decisions numbered from D-164 to D-170. Read docs/plan/P-word-at-a-time.md lines 1-86 (its rules, which every card here keeps) first. The items (docs/Roadmap.md line ranges):
1. Profiling after every benchmark, and the optimisations it points to (`scripts/work/profile.py` exists, L-5): lines 1532-1539.
2. Where optimisation learns from (the fastest libraries): lines 1054-1086: write a card only for what produces something checkable (a written survey with its numbers), else say why not.
3. Beat the others at everything, and say where it does; the live optimisation and benchmark page; benchmarks fair, and without mercy, what remains of each: lines 1087-1098, 1164-1200.
4. The SWAR and SIMD backend, what remains: lines 1099-1163 and 1522-1531. SIMD behind `CF_WITH_SIMD` is the maintainer's decision: write the measured case for SSE2 and NEON as a card that ends in numbers and a recommendation, with the build option itself TO DECIDE.
5. The OpenSSF Scorecard score raised (low priority), what the project can meet itself: lines 861-894 and 1540-1544. Repository settings are the maintainer's: list them, no card.

### M

FILE: docs/plan/M-maintainer.md. Group title: "M - the items that wait for the maintainer's word". Card ids M-1 onward, decisions numbered from D-171 to D-185. These items need the maintainer's approval before they open: begin each card's "needs" with "the maintainer's approval", put the item's open question(s) as TO DECIDE with options and a recommendation, and decide everything else in full, so the card can open the day the answer comes. Split a large item into one-commit cards (a, b, c...). The items (docs/Roadmap.md line ranges unless said):
1. Go (cgo) and .NET (P/Invoke) bindings: lines 53, 147-148 (version 2.3.0).
2. 2.0: the stream reader, the dynamic sizing modes, the image header's layout, and what they break, once: lines 40, 58, 149-155, 401-405, 417 (version 2.0.0), with D-61 (grep docs/Decisions.md).
3. Ways to read: DOM, SAX (the events as a public API), lazy, in-situ: lines 1235-1257 (version 2.0.0).
4. OSS-Fuzz: lines 1356-1360 (version 1.9.0; the application to Google is a maintainer step).
5. An evidence pack with each release: lines 1361-1370 (version 1.9.0).
6. Published where programs get their libraries (PyPI, crates.io, by trusted publishing): lines 787-804 (version 1.10.0; registry set-up is a maintainer step).
7. Distribution, as infrastructure: lines 895-905 (version 2.3.0).
8. One digest a purpose, and a door for the program's own: lines 916-931 (version 1.10.0).
`CF_WITH_SIMD` with SSE2 and NEON is written in group P by another worker: not here.

## The research brief (group Y)

Each of group Y's five topics gets this brief, then its topic: the
maintainer's words, what to cover, and its file
(docs/research/concurrent-access.md, compression.md, analysis-tool.md,
config-compiler.md, threads-and-processes.md). Model: the largest, at
high effort.

You do one research unit for ConfigForge, a configuration library in C (C89 core, layered components under lib/, readers and writers for JSON, YAML, TOML, INI, XML, CBOR and more, a contract of typed keys, sources ranked by precedence, Doctor reports, bindings for C++, Rust, Python). Repository: C:\Users\selcu\Projects\github\ProjectIdeas\ideas\library\configforge (branch main). You write ONE new file, named below, and change nothing else. Do not commit.

Read first, only these ranges: CLAUDE.md "Habits" (Tokens); docs/plan/A-architecture.md, the headings D-100 to D-108 (find with grep -n '^### D-10'; read the ones the topic touches, ~120 lines at a time); docs/Roadmap.md, the section "From the maintainer, 2026-11-26" (grep for it) for your topic's item. Find what the library already does for your topic with docs/Codemap.md, grep and git log, and read the code ranges that matter.

Then research the world, in depth, with web search: what other libraries, tools and runtimes do for this topic (name each, link its documentation, say what it does, its options and its failure modes), and any standard or paper that settles a point. Prefer primary sources (the project's own docs and source). At least eight sources.

Then think it through and write the document, in English, in the project's plain style (short sentences, no marketing), with these sections:
1. The question, as the maintainer asked it (their words, translated) and as it stands in this library today (what exists, with file:line).
2. What the world does: one subsection per library or tool, with links.
3. What breaks: the failure modes and hard cases the sources show (races, partial writes, bombs, portability, ...), each with its source.
4. What fits this library: options measured against D-100 to D-108 and the C89 core, each with its cost, what it needs (a dependency, a build option, a new public interface) and what it rules out.
5. A recommendation: what to build first, what after, and what not to build, with reasons; the public interface sketched (names, signatures) where one is needed; what is the maintainer's decision (a new dependency, a new public interface) marked TO DECIDE with options and the recommendation.
6. The cards it implies: a short list (title, size, version from docs/Maintaining.md "Versions" lines 99-114), not the cards themselves.
7. Sources: numbered list of links.

Length: as long as the topic needs, typically 250-500 lines. End your turn with a report under 200 words: the file, the recommendation in three lines, the TO DECIDE items.
