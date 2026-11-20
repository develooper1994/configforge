# HANDOFF - W5-V2, stopped 2026-11-20 (weekly usage limit)

State: every step of W5-V2 is built on this branch and passes everywhere
but one place: the full verify's `compilers` step, under tcc only, fails
(test_keyspace "add text/int", test_scale, test_lists, test_oom,
test_detect fail; test_serialize killed; test_report, test_emit segfault;
doctor tests "cannot start"). main (5706c26, A-5) passes tcc, so a change
of this branch is miscompiled by tcc or relies on what tcc lacks.

1. Find the cause: build with tcc in the WSL distribution as
   scripts/verify.cmake's `compilers` step does (grep tcc there); start
   with what this branch changed in headers every file includes
   (lib/serialize/include/configforge/cf_serialize.h: CF_HOLDS_*,
   cf_format_holds, cf_format_limits) and the format table in
   lib/serialize/src/codec.c (size_t members, `((size_t)-1)`,
   `((size_t)CF_MAX_DEPTH - 1u)` initialisers); bisect this branch's
   commits with the tcc build if needed. Fix it portably (C89).
   Done: `CF_EXTRA="components;abi;conformance" python scripts/work/work.py
   verify W5-V2` ends "verify W5-V2 full: ok".
2. Land it from the main checkout: `CF_EXTRA="components;abi;conformance"
   python scripts/work/work.py land W5-V2 "every pair of formats, and what
   each can hold"`; delete this file in the commit that finishes step 1.
3. Then, per docs/Plan.md: A-6 (codec recode), and a new card for the
   YAML writer's unclosed quote on names with control bytes, NUL or
   invalid UTF-8 (found by W5-V2's fuzz target); remove scripts/work/*.sh,
   env.sh and win.cmd once nothing uses them (work.py replaces them).
