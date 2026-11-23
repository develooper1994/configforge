# Working on ConfigForge with Claude Code

Read at the start of every session, on any machine. Everything a
session needs is in this repository: `git pull` and continue. Nothing
that matters may live only on one machine (its memory, a scratch
directory, a second worktree).

## The user

- Speaks Turkish: answer in Turkish. The repository (code, docs,
  commits) is in English.
- Works on two computers (office until 16:50, then home). Before the
  workday ends or a usage limit runs out: commit, push, and leave a
  `HANDOFF.md` (below) for anything unfinished.

## Roles: who organises, who carries out

- **A hierarchy, not a list of names.** The largest model the session
  has is the **organiser**: it plans, writes the cards and briefs,
  checks every result, and intervenes when something goes wrong. It does
  not do routine work itself. Every other unit goes to a **worker**: a
  subagent of any model the tools offer, today's or a later one. No rule
  here names a model; models change, the hierarchy stays. The user
  should not have to repeat this.
- **The smallest worker that can carry the unit.** A card described
  well enough lets a small model do work that a vague one would need a
  large model for, so the first lever is the card, the second the model.
  Pick by the unit, from the tiers the tools have (smallest, middle,
  largest): lookups and mechanical edits to the smallest; a fully
  decided card, tests, documents and research to the middle; only
  genuinely hard code or open design to the largest. Set `model`
  explicitly on every subagent. Independent units run in parallel.
- **Effort is set per unit, and adapts** (maintainer, 2026-11-23). Every
  subagent gets an explicit `effort` (and Codex `-c
  model_reasoning_effort=`): `low` for a lookup or a mechanical edit,
  `medium` for a decided card, `high` for a hard fault or a design
  choice. It moves with what happens: a retry after a failure or a
  doubtful report goes one step up (effort first, then a larger model);
  a unit that came back clean and fast lets the next of its kind go one
  step down; as the budget fills, units drop one step unless they failed
  before. The organiser itself works at the effort the unit in front of
  it needs, not one fixed level.
- A card must be good enough that a middle-tier worker carries it out
  without asking anything (exact files and line ranges, every choice
  made, numbered steps, the test that fails first, ACCEPT, when to
  stop). Before a new card opens, someone who did not write it reads it
  cold against the tree (docs/Plan.md, "Read again after the layout
  changes").
- Check what a subagent reports before passing it on. When it fails,
  stalls or returns something doubtful: read its output, sharpen the
  brief and retry, or take the step yourself.
- **Two budgets: the manager and the deputy** (maintainer, 2026-11-22).
  When Codex is installed, the Claude session is the manager and Codex
  (its larger model for a large or subtle card, its smaller one
  otherwise, chosen as above) the deputy. Codex's allowance is the
  larger of the two (maintainer, 2026-11-23): plan so that the deputy
  carries most cards and the manager's budget goes to briefs, checks,
  commits and CI. The deputy carries cards and CI fixes, with sub-agents of its
  own if it has them; the manager commits, pushes, watches CI, answers
  design questions, and takes over whatever the deputy leaves half done.
  The manager's budget must last longest: as its 5-hour window fills,
  start fewer Claude subagents (one past half, none past ~70%), give the
  work to the deputy, and keep the last ~10% for committing and pushing.
  When Codex nears its own limit, Claude carries on. How to run Codex,
  what its sandbox cannot do, and the desk (`inbox.md`/`outbox.md`)
  are in docs/Maintaining.md, "Working habits that paid". AGENTS.md is
  the deputy's page.
- A card's question that is only about the order of its steps, or a
  fixture that contradicts the card's own rule, is not a design question:
  whoever carries the card decides it by the card's rules and writes one
  line under the card's "Organiser's answers". Only a real design choice
  stops the card.

## Where the work is

- `docs/Plan.md`: "The loop" (follow it literally), ACCEPT, and the
  index of cards in order. `docs/plan/*.md`: the cards.
  `docs/plan/A-architecture.md`: the layers decided on 2026-11-19
  (D-100 to D-108).
- `docs/Maintaining.md`: how the work is done, the versions and
  milestones (2.0.0 marks what breaks; numbering goes on after it),
  the machines.
- `docs/Roadmap.md`: what the work is; `docs/Market.md`: who else
  serves the people who change configuration.
- `HANDOFF.md` at the root, when present: do its steps first, then
  delete it in the commit that finishes its last step (docs/Plan.md,
  "The loop", step 2).

## The brief every card's subagent gets

The organiser starts the worker with the output of
`python scripts/work/brief.py <card id>`.

```
You carry out one card of docs/Plan.md, alone.
1. Read docs/Plan.md ("The loop", "ACCEPT") and the card in docs/plan/*.md
   whole, with its notes and the design section it cites. Do exactly what
   it decides.
2. A choice the card does not make: stop and report it; never guess. A card
   marked TO DECIDE without an answer: stop.
3. Steps in order. "See it fail" is a real step: run the test before the fix.
4. Commit messages in the project's style (git log -20 --oneline).
   Branches as the loop says: work/<card id>, CI green, then the card
   squashed onto main as one commit (step 10) and the branch deleted.
   Push only when the brief says so.
5. Three failures on the same thing: stop, restore, write what failed under
   the card's "notes", report.
6. Before every push, on Linux: `python scripts/work/work.py format`,
   then `cmake -P scripts/generate/codemap.cmake` and
   `cmake -P scripts/generate/apiref.cmake` last. Both documents cite
   line numbers. Watch CI with `gh run watch --interval 60`.
   Commit and push after each step, so that a stop loses nothing.
7. Report: files changed, ACCEPT results with exit codes, anything the card
   got wrong, anything left. Under 300 words.
```

## Habits

- The checks and the landing are `scripts/work/` (`scripts/README.md`):
  a card's subagent runs `work.py win` and `work.py verify`; the
  organiser lands it with `work.py land` (the `.sh`/`.cmd` files are
  being replaced). They work on any machine; nothing is kept outside
  the repository.
- Documentation-only changes go straight to `main`. Pushing to `main`
  bypasses the required CI checks with the maintainer's rights: fine for
  documents; code goes through the loop's branch and CI first.
- Formatting is clang-format 22 (CI's). A machine without it formats
  with what it has and lets CI judge.
- Tokens (measured 2026-11-23, docs/Maintaining.md "Where the tokens
  go"): the bill is turns times the context each turn carries, not the
  size of one read. So:
  - The brief quotes the 20-60 lines of design a worker needs (or names
    `file:start-end`); a worker never reads `A-architecture.md`,
    `Decisions.md`, `Roadmap.md` or `Plan.md` whole, only the section
    cited. Those four were a quarter of what workers read.
  - Look first in `docs/Codemap.md` for the file, symbol or heading,
    then read a range of at most ~120 lines. Never `cat` or read
    whole a file over 300 lines; never `sed -n` a document in big
    chunks.
  - Never read a file again to check an edit: the edit tool fails if it
    did not apply; `git diff -U2 <file>` shows it. Read a file once and
    carry its facts forward in the next message.
  - Cap command output (`| tail -n 60`, `| grep` the failing lines);
    never print a whole build, test or CI log.
  - A worker that passes ~60 turns stops and reports; the card is split
    instead of run longer.
  - The organiser's own context: past ~250k tokens, write the state into
    `HANDOFF.md`, commit, and start a fresh session (or compact) from it.
  - No tool call that a notification, a chained command or the brief
    already covers; wait for background work instead of peeking or
    polling; chain dependent steps into one command; answer a repeated
    report once.
- The design is decided in `docs/plan/A-architecture.md`; a card does
  not reopen it. A real reason to change a decision goes to the user.
- Cards may be stacked (one cut from the branch of the card before it)
  so that work goes on while CI runs; each still lands alone, by tree
  (docs/Maintaining.md: "A card cut from another card's branch").
- Before the maintainer's stop time (a machine switched off, a workday
  ended): every branch pushed, `HANDOFF.md` current, and what was learnt
  written into the repository, not only into a session's memory. The
  next session may be on another machine with other tools and knows
  only what the repository says.
