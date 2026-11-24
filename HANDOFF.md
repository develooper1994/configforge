# Handoff (2026-11-24, night)

Landed on main this night, each alone with its CI green: style-1.8.0,
P-4 (the wipe: -25% to -72% instructions), I-1 (the integration
findings, with the full Linux verify all green), L-6 (docs/Codemap.md,
scripts/work/brief.py), L-7 (CI 19 to 14 minutes), L-8 (doctor.api under
MSan 615 s to 0.4), N-1 to N-8 (`configforge diff` with nested tables and
arrays, the musl archive built twice and compared, a difftool recipe,
YAML's single root, `cf_usage`, reload key-changed events). Every card of
docs/Plan.md's index is done except those that wait for a release.

1. **N-9** landed (ec0de1e): cppcheck on the libraries' own sources. Its style class (58 findings, listed in the card's notes) is the next card.
2. **The release**: 1.8.0's tag is the maintainer's. Before it: a full
   Linux verify on main (`CF_EXTRA="components;shared;conformance" python
   scripts/work/work.py verify main full`; it was green at I-1) and
   `work.py win`.
3. **Next cards**: the Roadmap's sections marked "not yet cards" hold
   more; the way that worked tonight: an agent of the middle tier writes
   one card (in N-4's form) from one named section and carries it in its
   own worktree; the organiser lands it (conflicts in the card file, the
   index and CHANGELOG are both-sides additions; `docs/Codemap.md` is
   regenerated on Linux after every landing).
4. L-3b waits for 1.9.0; P-2 and P-3 for 2.4.0.

Budgets at the stop: Claude weekly ~90%, Codex weekly ~95% (both reset
2026-11-26 and later). Lessons are in docs/Maintaining.md ("Where the
tokens go", "Working habits that paid") and CLAUDE.md "Roles".

Delete this file in the commit that finishes step 2.
