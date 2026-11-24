# Security

## Reporting a vulnerability

Please report it privately, through GitHub's **private vulnerability
reporting** on this repository, not in a public issue:

<https://github.com/develooper1994/configforge/security/advisories/new>

(the *Security* tab, "Report a vulnerability"). Include the input that
shows it - for a parser, the bytes - and the build options.

The project has one maintainer and no support contract, so the times are
a best effort, not a promise: an acknowledgement within a week, a first
assessment within two, and a fix for a confirmed vulnerability as soon as
it can be tested, with an advisory published when it is released. The fix
is credited to you in the CHANGELOG unless you ask otherwise. If a week
passes without an answer, say so on the advisory: it has been missed.

## Verifying a release

Every file a release uploads, the Linux archive's SBOM
(`configforge-<v>-linux-x86_64-musl.spdx.json`) included, is listed in
`SHA256SUMS` and carries a build provenance attestation:
`gh attestation verify <file> -R develooper1994/configforge`.

## Supported versions

Only the latest minor version of the current major version (the newest
release, and `main`) receives fixes; an older minor version is not patched,
so the answer to a vulnerability there is to move to the latest. A fix
that cannot keep the interface (D-56) waits for the next major version,
and the advisory says so.

## What is in scope

The parts of the library that read bytes somebody else controls:

* the JSON and INI codecs, the `.env` lexer and the command-line lexer;
* the binary snapshot image reader, which dereferences offsets taken
  from its input and is fuzzed for that reason;
* the provenance report reader used by Report Replay and by Doctor;
* Doctor's adapters, which read other tools' output.

And one property of the design: a **value-blind** report must not carry
the value of a sensitive key - only presence, origin, length and digest.
Neither may `explain()`, `inspect()` or the reason a sensitive value was
refused. Any of them leaking one is a vulnerability.

## What the library already does about it

* Every reader returns rather than crashes on corrupt input; a mutation
  fuzzer runs on every commit and with fresh seeds under the sanitisers
  in CI.
* Bounds are written as subtraction rather than addition, so they cannot
  wrap where `size_t` is 32 bits.
* A fixed-arena profile exercises every size from too small to enough,
  and each one either works or fails with a reason.
* Memory a configuration gives back is zeroed first, so a value or a
  file's text does not survive into the next allocation, a core dump or
  a swap file (`cf_arena_free`, `cf_arena_wipe`). Memory the C library
  keeps for itself - stdio's buffers, the environment - is outside it.
* Doctor reports a secret supplied on the command line or by a
  world-readable file (CFD007) and one supplied from a source its policy
  forbids (CFD016).
