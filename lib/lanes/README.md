# lanes

Span operations over bytes - the index of the first byte of a set, the
length of the valid UTF-8 prefix, eight digits to their value - with a
scalar reference for each and the processor's detection. C89, no
allocation, no global state.

lanes is **not ConfigForge's**. ConfigForge is one of its users (D-101); it
includes nothing of ConfigForge's, reads none of its build variables, and
`cmake -S lib/lanes` builds it alone. Its own license is in `LICENSE`.

* **Depends on**: nothing
* **Public header**: `include/lanes/lanes.h`
* **Version**: its own, `LANES_VERSION_MAJOR/MINOR/PATCH`, 0.1.0
* **Tests**: `tests/`

## Use

```c
#include <lanes/lanes.h>

const lanes_ops* ops = lanes_best();
size_t at = ops->find_byte(p, n, ',');     /* n when there is none */

lanes_set set;
lanes_set_init(&set, (const unsigned char*)"\"\\\n", 3);
at = ops->find_any(p, n, &set);
```

A backend is a constant table of function pointers (`lanes_ops`), so
nothing is detected or chosen behind the caller's back, and two threads can
never race over a choice. The program asks for one and keeps it:

| Call | Gives |
| ---- | ----- |
| `lanes_scalar()` | the reference, plain loops, always built |
| `lanes_swar()` | the word-at-a-time backend, or NULL when it is not built |
| `lanes_by_isa(LANES_ISA_x)` | that backend, or NULL when it was not built or this processor lacks it |
| `lanes_best()` | the best built backend this processor runs |
| `lanes_cpu()` | the `LANES_ISA_*` bits this processor runs |
| `lanes_built()` | the `LANES_ISA_*` bits compiled in |

The operations, in `lanes_ops`:

| Operation | Returns |
| --------- | ------- |
| `find_byte(p, n, c)` | the index of the first byte equal to `c`, or `n` |
| `find_any(p, n, set)` | the index of the first byte in the set, or `n` |
| `find_lt(p, n, c)` | the index of the first byte below `c`, or `n` |
| `ascii_run(p, n)` | the number of leading bytes below 0x80 |
| `utf8_valid(p, n)` | the length of the longest prefix that is whole, valid UTF-8: no overlong form, no surrogate, nothing above U+10FFFF; a sequence cut off by `n` is not in it |
| `count_byte(p, n, c)` | the number of bytes equal to `c` |
| `digits8(p, &v)` | 1 and the value when the eight bytes at `p` are all `'0'`-`'9'`, else 0 and `v` is left alone |

A pointer may be NULL when `n` is 0. No function reads at or past `p + n`.

## Backends

Two are built: `scalar`, and the word-at-a-time backend (`swar32` or
`swar64`, by the width of `unsigned long`: C89, words read with `memcpy`,
no pointer cast, no dependence on byte order). An operation of the word
backend whose word body did not beat its loop in `cf_bench lanes` has the
scalar function as its table entry. The vector backends (`sse2`, `sse42`,
`avx2`, `avx512`, `neon`, `sve`) come later, each added as a table and
compared with the scalar one by the tests. `lanes_cpu()` is real already: `cpuid` and `xgetbv` on x86 (AVX2 and
AVX-512 counted only when the system saves their registers), the
auxiliary vector on Linux Arm, NEON on any AArch64. The test prints it, so
a CI log shows what each runner has.

The option `CF_LANES_BACKENDS` (`scalar`, `c89` or `all`, default `all`)
chooses what is compiled: `scalar` the reference alone (no word is read
through `memcpy`), `c89` and `all` the word backend too (`all` adds the
vector ones when they exist).

## Build it alone

```sh
cmake -S lib/lanes -B build-lanes -DBUILD_TESTING=ON
cmake --build build-lanes
ctest --test-dir build-lanes
```

`add_subdirectory(lib/lanes)` in another project gives the static library
`lanes`; `LANES_BUILD_TESTS=ON` adds its tests there too, and
`LANES_FUZZ=ON` makes `fuzz_lanes` a libFuzzer target (clang). A project
that compiles the sources into a library of its own includes
`lanes_sources.cmake`, which sets `LANES_SOURCES`.

## Embedded under a prefix

Built with `LANES_PREFIX_CF`, every public name is `cf_lanes_<name>`
(`lanes.h` renames them, as zlib's `Z_PREFIX` does), and callers still
write `lanes_<name>`. A program that also links a lanes of its own sees no
clash. ConfigForge builds it that way.
