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

- The largest model in the session (Fable; when it is not available,
  Opus) is the **organiser**. It plans, writes the cards and briefs,
  checks every result, and intervenes when something goes wrong. It does
  not do routine work itself. The user should not have to repeat this.
- Units of work go to subagents with `model` set explicitly: **haiku**
  for lookups and mechanical edits; **sonnet** for research, documents,
  tests and code that follows a fully decided card; **opus** only for
  genuinely hard code or design. Independent units run in parallel.
- A card must be good enough that a sonnet subagent carries it out
  without asking anything (exact files, every choice made, numbered
  steps, the test that fails first, ACCEPT, when to stop). Before a new
  card opens, someone who did not write it reads it cold against the
  tree (docs/Plan.md, "Read again after the layout changes").
- Check what a subagent reports before passing it on. When it fails,
  stalls or returns something doubtful: read its output, sharpen the
  brief and retry, or take the step yourself.
- **Two budgets: the manager and the deputy** (maintainer, 2026-11-22).
  When Codex is installed, the Claude session is the manager and Codex
  (GPT-6.2-Astra for a large or subtle card, GPT-6.2-Sol otherwise) the
  deputy. The deputy carries cards and CI fixes, with sub-agents of its
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
6. Before every push: `python scripts/work/work.py format`, then
   `cmake -P scripts/generate/apiref.cmake` last, on Linux (docs/API.md
   cites source lines). Watch CI with `gh run watch --interval 60`.
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
- Tokens: no tool call that a notification, a chained command or the
  brief already covers. Wait for background work instead of peeking;
  chain dependent steps into one command, each step only if the one
  before succeeded; read the card and the failing lines, not whole
  files or logs; answer a repeated report once.
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
