# Migrating to 2.0

2.0.0 is the one release that breaks. Every break is in the table, with what
you wrote before and what to write now. Nothing else changes in 2.0
(D-178): `gate.abi`'s baseline, `cmake/configforge.symbols`, is the 2.0
interface, and no function leaves it.

| What changes | Since | Before | Now | D |
|---|---|---|---|---|
| `cf_error`'s `key`, `source` and `message` are owned text | 2.0.0 | `char key[CF_KEY_MAX]`, `char source[CF_SOURCE_MAX]` and `char message[CF_MESSAGE_MAX]` arrays: `sizeof e.message`, `e.message[0] = 0`, `strcpy(e.key, ...)` and copying a `cf_error` by value worked | `const char *`, `""` when unset (NULL only in a struct of zeros before its first use). Read them as strings; never write through them or take `sizeof`. Release a refusal with `cf_error_clear`; copy one with `cf_error_copy`, never by value. The C++ `Error` copies and frees by itself; the Rust and Python bindings read the pointers. A message longer than the old array is kept whole by `cf_error_set_long`, and the library's own long-key refusals use it under semi- and full-dynamic sizing | D-176 |
| The image header is 128 bytes (major 2) | 2.0.0 | 64 bytes, major 1, with 12 reserved bytes; a program that hardcoded `64` or the offset of the first section | `cf_image_write` writes major 2 (or major 1 with `CF_IMAGE_V1`); `cf_image` gains `key_max`, `source_max`, `message_max`, `max_depth` and `build_id`, and `reserved` shrinks to two. Use the sizes and offsets the header and `docs/ImageFormat.md` give. A major 1 image still loads (`build_id` is NULL); a major 2 image from a build with longer keys is refused, naming both limits | D-177 |
| A failure result is `CF_NODISCARD` | 1.9.0 | Dropping the result of a public function that reports a failure compiled silently | Under C23 and C++17 it is a warning (an error under `-Werror`): test the result, or cast it to `void` where you mean to ignore it. The `cf_builder_*` calls are left out: their first failure is deferred to `cf_builder_create` | D-223 |
| The tools print a second line when they refuse a file | 2.0.0 | `configforge codec check`, `dump` and `recode` wrote the one message line on standard error | They also write `  reason: <name> (<severity>[, <CWE>])`. A script that expects exactly one line of standard error takes the first, or filters on the reason | D-223 |

## What does not break

- A `cf_error` the caller allocates keeps working through its `size`: a library
  that sees a smaller struct writes only what fits (D-51). `cf_error.offset`
  (K-6a, D-138) and the owned-text store are behind that rule; the Rust and
  Python bindings' views of a `cf_error` end at `reason` and keep every text
  inline and cut, as before.
- Sizing (M-10, D-175; M-11a, D-176) is a new mode: `MANUAL`, today's limits, is
  the default and reads exactly as before.
- The stream reader (2.4.0) and the manifest in the image (2.2.0) are additive;
  bytes 104 to 111 of the image header are kept for the manifest.

## Checking a program

1. Build it against 2.0 with warnings on. A write through `e.message`, a `sizeof`
   of one of the three texts, or a `cf_error` copied by value is a compile error
   or a warning.
2. Compile once as C23 or C++17 and handle each `CF_NODISCARD` warning.
3. If you keep images: nothing to do for reading; a program that wrote or parsed
   the 64-byte header itself must use major 2's layout.
