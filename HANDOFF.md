# HANDOFF - W5-V2, stopped 2026-11-20 (weekly usage limit)

State: every step built; local verify (with tcc and pcc now information) is green; CI run 44800087291 still fails in two jobs. Their lines:

```
==7383== ERROR: libFuzzer: deadly signal
D:\a\configforge\configforge\tests\codec\pairs.c(640): warning C6001: Using uninitialized memory '*st.docs.name'.: Lines: 603, 604, 605, 606, 607, 608, 610, 611, 612, 628, 629, 632
D:\a\configforge\configforge\tests\codec\pairs.c(641): warning C6001: Using uninitialized memory '*st.docs.bytes'.: Lines: 603, 604, 605, 606, 607, 608, 610, 611, 612, 628, 629, 63
SUMMARY: libFuzzer: deadly signal
artifact_prefix='./'; Test unit written to ./crash-02ae53ac180461d6bf0b8097c12a3cdaf0f9f4f7
```

1. Fix the `matrix (windows-latest)` analyze finding above (MSVC /analyze; reproduce with `python scripts/work/work.py win <worktree> W5-V2 .` only builds Release - read the line and fix it as the earlier analyzer fixes did).
2. Fix the `libfuzzer` finding above (a crash input is in the job log; run `tests/fuzz` chain target with it under the sanitize preset in WSL).
3. Land: `CF_EXTRA="components;abi;conformance" python scripts/work/work.py land W5-V2 "every pair of formats, and what each can hold"`; delete this file in the commit that finishes step 2.
4. Then A-6, the YAML writer card, dropping scripts/work/*.sh, env.sh and win.cmd.
