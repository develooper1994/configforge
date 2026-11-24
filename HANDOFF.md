# Handoff (2026-11-24, night)

Landed on main this night, each alone with its CI green: style-1.8.0,
P-4 (the wipe: -25% to -72% instructions), I-1 (the integration
findings, with the full Linux verify all green), L-6 (docs/Codemap.md,
scripts/work/brief.py), L-7 (CI 19 to 14 minutes), L-8 (doctor.api under
MSan 615 s to 0.4), N-1 to N-10 (`configforge diff` with nested tables and
arrays, the musl archive built twice and compared, a difftool recipe,
YAML's single root, `cf_usage`, reload key-changed events). Every card of
docs/Plan.md's index is done except those that wait for a release.

1. **N-9** (ec0de1e) and **N-10** (67a451a): cppcheck on the libraries' own sources, its style class included.
2. **1.8.0 is released** (v1.8.0 at 7da87c6, 2026-11-24: Linux musl,
   Windows x64, macOS arm64, the amalgamation, SHA256SUMS; the full Linux
   verify, `work.py win` and a release dry run green first). PyPI and
   crates.io are not in the release workflow (their tokens are the
   maintainer's).
   **Before 1.9.0 can open, a decision that is the maintainer's** (D-89):
   each function experimental since 1.8 becomes `CF_API` or is removed:
   cf_build_profile, cf_codec_compose_sized, cf_encoding_by_name,
   cf_encoding_check, cf_encoding_name, cf_encoding_recode,
   cf_error_place, cf_format_holds, cf_format_limits,
   cf_reason_description, cf_reason_name, cf_source_file_encoding,
   A recommendation was prepared (2026-11-24, by reading each declaration
   and its callers): none to remove; CF_API for cf_build_profile,
   cf_codec_compose_sized, cf_encoding_by_name, cf_encoding_check,
   cf_encoding_name, cf_encoding_recode, cf_error_place (77 callers in
   the library), cf_format_holds, cf_reason_description, cf_reason_name,
   cf_source_file_encoding. Two want a choice first: `cf_format_limits`
   has two fixed out-parameters (a third limit would break it: add it now,
   or say further limits come as a new function); `cf_usage` takes no
   options (a trailing `unsigned flags`, 0 for now, or promote as is).
   Found on the way: four of them (cf_encoding_by_name, cf_encoding_name,
   cf_reason_name, cf_source_file_encoding) are already used by the Python
   and Rust bindings, which D-89 says expose no experimental function.
   P-2's CI is red only on this (`gate.abi`), as expected until the opening.
   cf_usage (`cmake/configforge.experimental.symbols`). Opening 1.9.0
   without it turns `gate.abi` red (it did once tonight: reverted,
   a7fe458). Then `cmake -DTO=1.9.0 -P scripts/release/version.cmake`.
   Waiting on 1.9.0: **P-2** on `origin/work/P-2` (the lanes'
   word-at-a-time backend; four operations kept, two left on the loop;
   its CHANGELOG entry already under 1.9.0; CI to re-check after the
   opening).
   **N-12** on `origin/work/N-12`: stopped at its own stop rule (the
   differential fuzzer between the built-in YAML reader and libyaml
   kept finding classes of difference past ten); its findings are in
   its card's notes; one is a real fault (a NUL inside a comment is
   accepted, input base64 `IwAj`), a fix for 1.8.1.
   Scratch directories `C:\d\cf-work\p2mut1..4` left by a worker
   (the safety check refused their removal): delete them by hand.
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
