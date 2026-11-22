# Working on ConfigForge with Codex

Read CLAUDE.md first: it holds the rules for every agent on this project
(the user, the roles, where the work is, the brief, the habits). This page
says only what is different when you are Codex.

## Your role: the deputy

When a Claude session runs as the manager, you are its deputy (CLAUDE.md,
"Two budgets: the manager and the deputy"):

- You carry cards and CI fixes, as `docs/Plan.md` and the card decide,
  and may hand parts to sub-agents of your own; check what they return.
- The manager commits, pushes, watches CI and answers design questions.
  It also takes over what you leave half done, so end every run at a
  point the next one (yours or the manager's) can continue from.
- A design choice the card does not make: ask, do not guess. A question
  only of the order of the card's steps, or a fixture that contradicts
  the card's own rule, you decide by the card's rules and write under the
  card's "Organiser's answers".

## Your sandbox

In `workspace-write` you can read and write the worktree you are given
and build and test with CMake and MSVC (build directories under the
system temporary directory). You cannot write git's index (so you cannot
commit), start Python, reach WSL or the network. Leave changes
uncommitted and say what is ready, with commit subjects and their files.
Regenerate `docs/API.md` (`cmake -P scripts/generate/apiref.cmake`) as
your last action in a worktree you changed: it cites source lines.

Known on the maintainer's workstation, not faults of the code: in the
sandbox `unit.test_registry`, `example.policy` and `example.policy_cpp`
cannot write HKCU; in a build with `CF_MESSAGE_MAX=128`,
`unit.test_yaml` and `unit.test_compose` see messages cut because the
temporary paths are long.

## The desk

When the manager runs you in a loop, `.claude/worktrees/deputy/` holds
`inbox.md` (the manager's notes, newest at the bottom: what is BUSY, what
to do, answers) and `outbox.md` (your reports, one per run). Begin each
report with `STATUS: READY`, `WORKING`, `STOPPED` or `ALL DONE`, and claim
only what you ran: the failing line before the code, the builds and
ctest results with exit codes.

## Your budget

Your limits are in the newest `~/.codex/sessions/**/rollout-*.jsonl`
(`"rate_limits"`). Near the end of a window, stop at the end of a piece
of work, not in the middle of one.
