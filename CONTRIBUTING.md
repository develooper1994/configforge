# Contributing

Thank you for looking. This project holds itself to one rule above the
others: **a claim the build does not check is a claim that decays** - so
most of what follows is a check rather than a request.

## Before you start

* Read [`AGENTS.md`](AGENTS.md). It is written for anyone continuing the
  work, person or AI agent, and maps the repository.
* For a change in behaviour, read the relevant entry in
  [`docs/Decisions.md`](docs/Decisions.md) first. If the change
  contradicts a decision, the pull request changes the decision too and
  says why.
* For a bug, an issue with the smallest configuration that shows it and
  the output of `explain()` for the key is the most useful thing you can
  send.

## Making a change

```sh
cmake --preset default && cmake --build --preset default && ctest --preset default
cmake -P scripts/format.cmake
git add -p
cmake -DSOURCE=index -DJOBS=2 -P scripts/verify.cmake
```

`verify.cmake` has to end in `all green`. It builds and tests five
configurations - C89 with the full warning set, the sanitisers, the
portable fallbacks, no floating point, and the static analysers - and
runs the gates that enforce the architecture.

The rules that matter most, all in [`docs/Conventions.md`](docs/Conventions.md):

* C89. Declarations at the top of a block, no `//`, no `snprintf`.
* Every allocation goes through `cf_allocator`; no global mutable state.
* `cf_` for public names, `cfi_` for internal ones with external linkage.
* A test asserts behaviour, says what it expected, and is proved able
  to fail.
* Documents change in the same commit as the code they describe.

## Commits and pull requests

One verified unit per commit. The title says what is now true; the body
says why, what was considered and rejected, and anything found on the
way. `git log` shows the register.

## Licence

Contributions are accepted under the project's licence, zlib (see
[`LICENSE`](LICENSE) and D-36 in `docs/Decisions.md`).
