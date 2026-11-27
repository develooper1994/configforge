# Done

Cards that landed, one line each; the card's full text is in git at the commit before it was archived: `git show <archive commit>^:docs/plan/<file>`. `python scripts/work/index.py --archive` adds the cards that are done.

| id | goal | landed | file |
| -- | ---- | ------ | ---- |
| F-1a | The file layer: `cf_file_options`, `cf_file_writer`, `cf_file_replace`, a file replaced whole or not at all | 7596e78 | F-files.md |
| K-9c | Errors that cannot be ignored: `CF_NODISCARD` on every failure, and its gate | 2c03110 | K-correctness.md |
| K-11a | Options with defaults as one convention: `cf_*_options_init`, the fixed limits as options, and a gate | 7a2af29 | K-correctness.md |
| K-11b | The `none` platform builds and is tried: no feature assumes an operating system | 286f127 | K-correctness.md |
| K-11c | The table of each feature against each platform, checked by a gate | 52d1cc3 | K-correctness.md |
| L-18a | A Cortex-M build of `minimal` in CI, with its stack gate | b05e2a5 | L-tools-2.md |
| L-20 | The index generated from the card files, and `gate.index` | 84d48d9 | L-tools-2.md |
| L-21 | A card opens only when a script says it is ready | 6521a97 | L-tools-2.md |
| L-22 | CI in two tiers: a fast core on every push, the whole suite once a card | 02acbdd | L-tools-2.md |
| L-23 | A release's notes say how each archive was built | 9cdc40e | L-tools-2.md |
| L-24 | The Rust crate published to crates.io at a tag | a0eb1a4 | L-tools-2.md |
| M-18 | OSS-Fuzz: the project files in the repository | 83264b0 | M-maintainer.md |
| Y-2 | `cf_platform_write_file_if`: the replace refused when the file no longer holds the bytes read (F-1c in the research) | 82ddc27 | Y-asked.md |
| Y-3 | `cf_locker`: a lock that plugs in, `cf_lock_none` and a kernel lock on a kept sidecar file, `cf_lock_sidecar` (F-5a in the research) | 3b241ee | Y-asked.md |
| Y-5 | A stable read: a file read twice and compared, up to 3 reads by default (F-5c in the research) | 442f52e | Y-asked.md |
| Y-6 | Windows readers that share delete access, if a measurement shows a rename then succeeds over an open reader (F-5d in the research) | 16b115d | Y-asked.md |
| Y-8 | Close-on-exec and non-inheritable everywhere the library opens a file, with a child that lists its descriptors | 62812e1 | Y-asked.md |
