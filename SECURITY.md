# Security

## Reporting a vulnerability

Please report it privately, through GitHub's **private vulnerability
reporting** on this repository (the *Security* tab, "Report a
vulnerability"), not in a public issue. Include the input that shows it -
for a parser, the bytes - and the build options.

You will get an acknowledgement, and the fix will be credited to you in
the CHANGELOG unless you ask otherwise.

## Supported versions

The latest release of the current major version and `main` receive
fixes. A fix that cannot keep the interface (D-56) waits for the next
major version, and the advisory says so.

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
