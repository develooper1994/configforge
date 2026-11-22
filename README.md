This branch holds the numbers of the benchmark's comparison with other libraries (D-111).
It is written by CI (.github/workflows/bench.yml) and by nothing else: one file a run, never edited.
A file is `runs/<year>/<time>-<sha7>-<kind>.json`, the schema is in scripts/bench/run.py, and docs/Performance.md draws it.
