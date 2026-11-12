# Working with agents

Much of this project is written by AI agents under a person's direction.
An agent's time is metered - by the token, and by a limit that resets
every few hours - so how the work is divided decides how much of it gets
done. This page is the method. It was arrived at by running out.

## One organiser, many workers

One session organises: it reads the state, plans, writes briefs, reviews
what comes back, verifies, commits. It does not write the code. The
workers are subagents, each given one unit of work and a model no
larger than the unit needs:

| The work | The model |
| -------- | --------- |
| Looking something up, pinning versions, mechanical edits, running a suite and reporting the result | the smallest (haiku) |
| A fix or a test with a clear specification; documents in the house voice | the middle (sonnet) |
| A design to make, an audit, a review of a diff, anything that touches an interface | the largest worker (opus) |

The organiser is the most capable model and the most expensive, so
everything it reads is paid for at the highest rate: it reads outlines,
tails and diffs' statistics, and leaves files to the workers.

## What a brief holds

A worker knows only its brief. A good one is short and complete:

* **The goal in one sentence**, and why it matters to a value being
  explainable - the project's one measure.
* **The files it may edit**, by name, and the files it must not.
* **What is already known**: the line, the trigger, what was ruled out.
  A worker that has to rediscover it pays for it twice.
* **The steps, in order**, with the test that must fail first.
* **Where it stops**: what to do when the scope turns out larger - stop,
  change nothing, report.
* **The report's form and length**: 200 words, what changed, whether the
  test failed before the fix, the suite's result as N of N, and the
  CHANGELOG's sentences. The organiser reads every report in full, so a
  long one is the dearest text in the session.

What every worker must know - the rules below, how to build on the
machine at hand - is written once, in a file, and each brief begins
"read this file first". It is not repeated in every brief.

## Rules every worker follows

1. It does not commit, push or change branches, and does not edit
   `CHANGELOG.md`, `docs/Roadmap.md` or `docs/API.md`: the organiser
   does, once, so that parallel workers do not collide there.
2. Test first: the test is seen to fail, then the fix is written. Where
   the behaviour exists already, it is disabled by returning early to
   show the test can fail (CONTRIBUTING.md, "How work is done here").
3. Its own build directory (`build-<name>`), so that workers build side
   by side. On a machine with little memory, two or three jobs each.
4. Long output goes to a file and only its tail is read. The exit code
   is the answer, never a `grep` of the output.
5. A part it cannot finish is reverted, not left half-written, and the
   report says so.
6. The house rules of CONTRIBUTING.md hold: C89, the stack budget,
   unique `static` names, the 509-character literal. Two more come from
   formatter versions disagreeing: no string literal continued over
   lines as a call's argument (name it first), and no condition that
   needs more than one line (nest the ifs).

## What runs at once

Units that share no file run side by side; units that share one go one
after the other. The files most often shared are the ones rule 1 keeps
for the organiser, and `tests/unit/CMakeLists.txt`, where each worker
changes a line and re-reads before it does. The Roadmap's "What can run
at once" groups the open work by the files it changes for this reason.

Audits are separate from fixes. An audit reads and reports - a finding
is a line, a trigger, the missing check, a fix in a sentence, and how
sure it is - and a fix is briefed from the finding. One reader per area,
so that nothing is read twice.

## Verifying, in the order of cost

1. The worker runs its own tests, then the whole suite, in its own
   build.
2. The organiser builds everything together once, on each system it
   has, as one command in the background whose last lines it reads.
3. A review of the diff - not of the tree - by a worker that has not
   seen the briefs, asked one question: is each fix whole, and did it
   break something else. It found a gap in five of the fixes of the
   first pass it was used on.
4. CI on a branch, for what no machine at hand can run: the other
   systems, the pinned formatter, the newer analysers. Then the branch
   is fast-forwarded into `main`.

Nothing waits by polling. A build, a suite and a CI run are started in
the background and report when they end.

## Stopping without losing the work

The limit can end a session mid-sentence, and the next session may be a
smaller model on another machine. So:

* The state of the work lives in the repository, in `docs/Roadmap.md`,
  and is updated in the commit that changes it.
* Findings that exist only in a session - an audit's results - are
  written to a file as soon as they arrive, before any is acted on.
* When work stops unfinished, a `HANDOFF.md` at the root is committed:
  the state in three lines, then numbered steps, each with its files,
  its test and what done looks like, written for a reader who knows
  nothing of the session. Its first step is always to fetch and see
  what another machine changed. It is deleted by the commit that
  finishes its last step.
* A session begins by fetching and reading the log, never by assuming
  the tree is as it left it.
