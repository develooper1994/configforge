# Failure guarantees

## What this page is

A library inside a controller or a daemon must say what is left when a call
fails, and must never decide to stop the process. Two rules, from the
maintainer (2026-11-26, "exception safety is to be added"), and one
word per public function:

1. Every public function states one failure guarantee in its header comment:
   `nothrow`, `strong` or `basic` (below).
2. The library never ends the program, and an allocation that fails returns
   `CF_E_MEMORY`.

The words are held by `gate.guarantees` (`scripts/check/guarantees.py`); the
table at the end of this page is made from the headers by the same script.
The decision is D-232.

## The words

A *failure* is a call that returns `CF_FALSE`, `NULL` or a code other than
`CF_OK`, or that records one where it returns nothing (a builder holds its
first failure until `cf_builder_create`; a sink holds its overflow).

* **`nothrow`**: it cannot fail. No allocation, no input or output; a NULL
  or an invalid argument gets the fallback its comment states. Destroying
  calls (`_free`, `_close`, `_destroy`, `_abort`), accessors and queries with
  a fallback (`cf_get_int`), and formatting into a caller's buffer that cuts
  the text (`cf_fmt`) are `nothrow`.
* **`strong`**: it may fail; when it does, every object reachable from its
  arguments is as it was before the call. Not counted as state: the
  `cf_error` that holds the failure, and an output buffer or output struct
  of the caller's (its contents are then unspecified, NUL-terminated where
  the comment says so). A call that builds its result apart and publishes it
  at the end is `strong`: `cf_derive`, `cf_write`, `cf_edit_commit`,
  `cf_file_replace` (the target file is unchanged), `cf_live_reload` (the
  current snapshot is still the one served).
* **`basic`**: it may fail; when it does, nothing leaks and every object
  stays valid (it can be used, freed, or the call tried again), but its
  contents may have changed, and the comment says which. A `cf_builder_*`
  call that holds its failure for `cf_builder_create`, a `cf_sink_*` write
  that overflows, `cf_file_writer_write` (some bytes were written).

**Choosing**, in this order: no failure path is `nothrow`; a failure before
anything of the arguments is touched, or touched only after the last point a
failure can happen, is `strong`; otherwise `basic`. A function that can leave
an object invalid or leak is a defect, never a fourth word: it is fixed (its
own commit, a test first) or written under the annotating card's notes.

## How a function's line is read

In the comment directly above a declaration, or above a run of declarations
with no blank line between them (the comment then covers the run), one line
starts `Guarantee: `, then a word, then a full stop, then optionally more
words on the same line:

```c
/* Guarantee: strong. */
/* Guarantee: basic: the sink stays valid; its text is cut where it overflowed. */
```

Where the members of a run differ, they are separate runs.
`scripts/generate/apiref.cmake` copies the comment above each declaration
into [API](API.md), so the word reaches the API reference.

A public function is a name on a non-comment line of
`cmake/configforge.symbols` or `cmake/configforge.experimental.symbols`. The
internal names (`cmake/configforge.internal.symbols`) are outside the promise
and outside the gate. There is no list of exceptions: every public name has its
line, and a new one without it is refused.

The gate refuses: a public name with no line; a word that is not one of the
three (and a `Thread:`, `Signal:` or `Alloc:` line whose word is not one of its
own, below); a `CF_NODISCARD` function that claims `nothrow` (a function that can
report a failure is not `nothrow`); a table that is not what the headers say (`scripts/check/guarantees.py
--write` rewrites it).

## Thread, signal and allocation claims

Three more lines may stand beside `Guarantee:` in the same comment, each a
word and a full stop, each on a line of its own. A line left out is the
weakest word, so no name needs a line to be honest: a line is written only
where it is proved (the maintainer, 2026-11-27, "which of them can the library
provide?"; D-238).

| Line | Words | Default | What it says |
| ---- | ----- | ------- | ------------ |
| `Thread:` | `safe`, `distinct objects`, `unsafe` | `distinct objects` | `safe`: any number of threads may call it on the same objects at once (the readers of a built `cf_config`, D-14). `distinct objects`: calls on different objects are independent; one object is the caller's to serialise. `unsafe`: it touches state shared beyond its arguments; none is expected, and the gate prints each. |
| `Signal:` | `safe`, `unsafe` | `unsafe` | `safe`: it may be called from a signal handler or an interrupt: it reaches no allocator, no stdio, no lock, no call through a pointer, and leaves `errno` as it found it. |
| `Alloc:` | `none`, `may` | `may` | `none`: it never calls the allocator, directly or through a callee. |

```c
/* Guarantee: nothrow. The fallback for an absent key or another type.
   Thread: safe.
   Signal: safe.
   Alloc: none. */
```

Interrupt and real-time claims follow from the words and need none of their
own: a function that is `Signal: safe` with `Alloc: none` and a stated stack
frame (`gate.stack`) may run in an interrupt handler or in a hard real-time
loop.

`gate.guarantees` refuses a word that is not in the table, and it holds the
two words that can be proved. For `Signal: safe` and `Alloc: none` it reads
the compiler's call data: GCC 10 and later write `*.ci` files beside the
library's objects when they are given `-fcallgraph-info`, and the build does
so for `configforge` and `configforge_serialize`. The check follows the calls
of the function through every file, and refuses, naming the path
(`cf_x -> helper -> malloc`): a reached `malloc`, `calloc`, `realloc`, `free`
or the library's arena allocators; a call through a pointer (the allocator's
own `alloc` and `release` are one, and nothing says what else a pointer
holds); and, for `Signal: safe` only, stdio (`fopen`, `printf` and its
relatives, and what GCC turns them into), the locks (`pthread_mutex_*`,
`mtx_*` and their kin) and every name of `scripts/check/notsignal.txt` (one
`name|reason` a line, `*` any run of characters). A path of the compiler's own
(`memcpy`, `__stack_chk_fail`) is not refused. A function the call data does
not hold (it is not built in this configuration) is said, not refused. On a
build without call data (another compiler, or `CF_LTO`) the gate prints that
the two words were not checked, and passes; CI's GCC builds check them.

`Thread: safe` is held by `test_threads` for the readers of a built
`cf_config`, not by the call data: a function that writes shared state would
need a race detector to be caught.

## The library never ends the program

No file of `lib/*/src` or `lib/*/include` calls `abort`, `exit`, `_Exit`,
`quick_exit`, `longjmp`, `siglongjmp`, `setjmp`, `sigsetjmp`, `assert`,
`raise` or `__builtin_trap`. The gate reads each file with its comments and
string literals removed and looks for the name followed by `(`. An exception
is one line `path|token|reason` in `scripts/check/nonlocal.txt`; the list is
empty.

An allocation that fails returns `CF_E_MEMORY`, never another code and never a
crash. That is held at run time by K-9b's tests, not by this gate.

## What holds across a process

The failure guarantees above are about one call. These four are about
what a call meets outside the library: a `fork`, a callback of the
caller's, a crash, an `exec`. Each row names the ctest that holds it
(`gate.guarantees` refuses a name that is not a test).

<!-- process:begin -->

| Across | What holds | Test |
| ------ | ---------- | ---- |
| `fork` | The child may read a `cf_config` built before `fork`. `cf_live_*` in the child is outside the contract (not detected: the library keeps no pid, and a check on every call would cost a system call for a case the contract can exclude). | `unit.test_fork` |
| A callback | A callback must return: an exception, `longjmp` or any other unwinding through the library is outside the contract. The C++ wrapper catches before C (a build that throws is a build that failed; a check that throws refuses and says what was thrown); the Rust and Python bindings do the same before their callbacks reach C. | `unit.test_wrapper_callback` |
| A crash | `cf_file_replace` writes a temporary beside the file, syncs it, renames it over the file, then syncs the directory where the platform can: POSIX syncs the file with `fsync` and the directory with `fsync` of its descriptor; Windows flushes the file and renames with `MOVEFILE_WRITE_THROUGH`, which has no separate directory sync; `none` has no file writer. A crash leaves the old file or the new, never a part; a temporary a crash left is not read, removed or reused. | `unit.test_writefile` |
| `exec` | No descriptor the library keeps is inherited: each is close-on-exec (POSIX) or not inheritable (Windows), D-190. | `unit.test_cloexec` |

<!-- process:end -->

## The table

One row a public function: its failure guarantee, then its thread, signal and
allocation words (the default where its header says none).

<!-- guarantees:begin -->

| Function | Header | Guarantee | Thread | Signal | Alloc |
| -------- | ------ | --------- | ------ | ------ | ----- |
| `cf_app` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_app_builder` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_apply_stats` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_arena_alloc` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_allocator` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_arena_calloc` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_capacity` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_arena_dup` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_fixed` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_free` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_arena_is_fixed` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_new` | `cf_alloc.h` | strong | distinct objects | unsafe | may |
| `cf_arena_used` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_arena_wipe` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_array_count` | `cf_iterate.h` | nothrow | distinct objects | unsafe | may |
| `cf_array_key` | `cf_iterate.h` | nothrow | distinct objects | unsafe | may |
| `cf_blob_arena` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_blob_arena_done` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_blob_file` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_blob_mem` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_blob_skip_bom` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_build` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_build_id` | `cf_version.h` | strong | distinct objects | unsafe | may |
| `cf_build_in` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_build_limit` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_build_profile` | `cf_version.h` | strong | distinct objects | unsafe | may |
| `cf_builder_abort` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_builder_bool` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_cli` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_codec` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_create` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_builder_credentials` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_define` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_digest_key` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_dir` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_document` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_dotenv` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_env` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_env_document` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_file` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_file_codec` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_int` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_key` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_limits` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_log` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_builder_memory` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_open` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_preferences` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_real` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_registry` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_replay` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_response_files` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_schema` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_schema_file` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_secrets` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_sizing` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_source` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_store` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_text` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_builder_track_misreads` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_builder_track_reads` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_builder_unknown` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_builder_writable` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_changed` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_cli_args` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_cli_expand` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_code_description` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_code_name` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_among` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_at` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_by_name` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_by_path` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_can_read` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_codec_compose_sized` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_codec_count` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_decode` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_codec_detect` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_codec_kind` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_codec_select` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_config_codec` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_config_free` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_config_ref` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_config_sizing` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_count` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_def_init_sized` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_default_allocator` | `cf_alloc.h` | nothrow | distinct objects | unsafe | may |
| `cf_defs` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_defs_apply` | `cf_define.h` | basic | distinct objects | unsafe | may |
| `cf_derive` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_derive_in` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_diff` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_diff_json` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_diff_text` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_digest` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_edit_abort` | `cf_edit.h` | nothrow | distinct objects | unsafe | may |
| `cf_edit_begin` | `cf_edit.h` | strong | distinct objects | unsafe | may |
| `cf_edit_commit` | `cf_edit.h` | strong | distinct objects | unsafe | may |
| `cf_edit_count` | `cf_edit.h` | nothrow | distinct objects | unsafe | may |
| `cf_edit_set_bool` | `cf_edit.h` | basic | distinct objects | unsafe | may |
| `cf_edit_set_int` | `cf_edit.h` | basic | distinct objects | unsafe | may |
| `cf_edit_set_real` | `cf_edit.h` | basic | distinct objects | unsafe | may |
| `cf_edit_set_text` | `cf_edit.h` | basic | distinct objects | unsafe | may |
| `cf_edit_unset` | `cf_edit.h` | basic | distinct objects | unsafe | may |
| `cf_emit` | `cf_emit.h` | strong | distinct objects | unsafe | may |
| `cf_encoding_by_name` | `cf_encoding.h` | strong | distinct objects | unsafe | may |
| `cf_encoding_check` | `cf_encoding.h` | strong | distinct objects | unsafe | may |
| `cf_encoding_name` | `cf_encoding.h` | nothrow | distinct objects | unsafe | may |
| `cf_encoding_recode` | `cf_encoding.h` | strong | distinct objects | unsafe | may |
| `cf_env_key` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_env_value` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_environ` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_clear` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_copy` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_place` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_place_at` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_set` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_error_set_long` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_explain` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_explain_json` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_feature_at` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_feature_count` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_file_options_init` | `cf_file.h` | nothrow | distinct objects | unsafe | may |
| `cf_file_replace` | `cf_file.h` | strong | distinct objects | unsafe | may |
| `cf_file_writer_close` | `cf_file.h` | basic | distinct objects | unsafe | may |
| `cf_file_writer_flush` | `cf_file.h` | basic | distinct objects | unsafe | may |
| `cf_file_writer_open` | `cf_file.h` | basic | distinct objects | unsafe | may |
| `cf_file_writer_sync` | `cf_file.h` | basic | distinct objects | unsafe | may |
| `cf_file_writer_write` | `cf_file.h` | basic | distinct objects | unsafe | may |
| `cf_fingerprint` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_fingerprint_format` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_fingerprint_seed` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_fingerprint_step` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_fmt` | `cf_port.h` | nothrow | distinct objects | unsafe | may |
| `cf_fmt_append` | `cf_port.h` | nothrow | distinct objects | unsafe | may |
| `cf_format_holds` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_format_limits` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_format_of_path` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_format_option` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_get_bool` | `configforge.h` | nothrow | safe | safe | none |
| `cf_get_int` | `configforge.h` | nothrow | safe | safe | none |
| `cf_get_real` | `configforge.h` | nothrow | safe | safe | none |
| `cf_get_text` | `configforge.h` | nothrow | safe | safe | none |
| `cf_get_text_n` | `configforge.h` | nothrow | safe | safe | none |
| `cf_has` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_has_feature` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_image_at` | `cf_image.h` | strong | distinct objects | unsafe | may |
| `cf_image_count` | `cf_image.h` | nothrow | distinct objects | unsafe | may |
| `cf_image_get` | `cf_image.h` | strong | distinct objects | unsafe | may |
| `cf_image_load` | `cf_image.h` | basic | distinct objects | unsafe | may |
| `cf_image_open` | `cf_image.h` | basic | distinct objects | unsafe | may |
| `cf_image_write` | `cf_image.h` | strong | distinct objects | unsafe | may |
| `cf_inspect` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_iter_children` | `cf_iterate.h` | strong | distinct objects | unsafe | may |
| `cf_iter_keys` | `cf_iterate.h` | strong | distinct objects | unsafe | may |
| `cf_iter_next` | `cf_iterate.h` | strong | distinct objects | unsafe | may |
| `cf_key_cmp` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_key_segments` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_key_valid` | `cf_error.h` | strong | safe | safe | none |
| `cf_keymap_from_key` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_keymap_init` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_keymap_to_key` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_keys` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_keysink_init` | `cf_entry.h` | strong | distinct objects | unsafe | may |
| `cf_keysink_of` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_ks_add` | `cf_keyspace.h` | basic | distinct objects | unsafe | may |
| `cf_ks_add_authored` | `cf_keyspace.h` | basic | distinct objects | unsafe | may |
| `cf_ks_add_candidate` | `cf_keyspace.h` | strong | distinct objects | unsafe | may |
| `cf_ks_add_default` | `cf_keyspace.h` | basic | distinct objects | unsafe | may |
| `cf_ks_add_resolved` | `cf_keyspace.h` | basic | distinct objects | unsafe | may |
| `cf_ks_add_text` | `cf_keyspace.h` | basic | distinct objects | unsafe | may |
| `cf_ks_arena` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_lex_cli` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_lex_dotenv` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_lex_env` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_limit_options_init` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_listsink_finish` | `cf_entry.h` | basic | distinct objects | unsafe | may |
| `cf_listsink_init` | `cf_entry.h` | strong | distinct objects | unsafe | may |
| `cf_listsink_of` | `cf_entry.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_approve` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_close` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_commit` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_confirm` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_generation` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_get` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_last_error` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_open` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_open_with` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_options_init` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_pending` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_poll` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_reject` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_reload` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_restarts` | `cf_live.h` | nothrow | distinct objects | unsafe | may |
| `cf_live_rollback` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_status_get` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_live_wait` | `cf_live.h` | strong | distinct objects | unsafe | may |
| `cf_loc_text` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_lock_none` | `cf_lock.h` | nothrow | distinct objects | unsafe | may |
| `cf_lock_sidecar` | `cf_lock.h` | nothrow | distinct objects | unsafe | may |
| `cf_log_level_name` | `cf_log.h` | nothrow | distinct objects | unsafe | may |
| `cf_misread_at` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_misreads` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_outcome_can_win` | `cf_keyspace.h` | strong | distinct objects | unsafe | may |
| `cf_outcome_name` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_platform_has_dir_list` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_environ` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_preferences` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_private_create` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_registry` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_trusted_dir` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_wake` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_has_wide_args` | `cf_platform.h` | strong | distinct objects | unsafe | may |
| `cf_platform_name` | `cf_platform.h` | nothrow | distinct objects | unsafe | may |
| `cf_policy_text` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_port_description` | `cf_port.h` | nothrow | distinct objects | unsafe | may |
| `cf_read_count` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_reason_description` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_reason_name` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_reason_severity` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_reason_weakness` | `cf_error.h` | nothrow | distinct objects | unsafe | may |
| `cf_refusal_name` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_refusal_text` | `cf_keyspace.h` | nothrow | distinct objects | unsafe | may |
| `cf_report` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_report_key_write` | `cf_report.h` | strong | distinct objects | unsafe | may |
| `cf_report_write` | `cf_report.h` | strong | distinct objects | unsafe | may |
| `cf_schema` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_schema_write` | `cf_report.h` | strong | distinct objects | unsafe | may |
| `cf_serialize_version` | `cf_serialize_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_serialize_version_number` | `cf_serialize_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_sink_fmt` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_init` | `cf_serialize.h` | nothrow | distinct objects | unsafe | may |
| `cf_sink_json_string` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_json_value` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_overflowed` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_sink_putc` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_puts` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_terminate` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sink_write` | `cf_serialize.h` | basic | distinct objects | unsafe | may |
| `cf_sizing_options_init` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_cli` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_cli_defs` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_defaults` | `cf_define.h` | strong | distinct objects | unsafe | may |
| `cf_source_destroy` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_dir` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_dir_codecs` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_dir_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_dir_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_document` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_dotenv` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_dotenv_defs` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_dotenv_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_dotenv_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_env` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_env_defs` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_env_document` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_file` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_file_codec` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_file_codecs` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_file_defs` | `cf_define.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_file_encoding` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_file_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_file_format` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_file_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_image_error` | `cf_image.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_image_file` | `cf_image.h` | strong | distinct objects | unsafe | may |
| `cf_source_image_mem` | `cf_image.h` | strong | distinct objects | unsafe | may |
| `cf_source_image_state_size` | `cf_image.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_load` | `cf_source.h` | basic | distinct objects | unsafe | may |
| `cf_source_memory` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_path` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_registry` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_registry_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_registry_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_replay` | `cf_report.h` | strong | distinct objects | unsafe | may |
| `cf_source_replay_error` | `cf_report.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_replay_state_size` | `cf_report.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_secrets` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_secrets_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_secrets_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_snapshot` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_store` | `cf_source.h` | strong | distinct objects | unsafe | may |
| `cf_source_store_error` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_source_store_state_size` | `cf_source.h` | nothrow | distinct objects | unsafe | may |
| `cf_sources` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_store_set` | `cf_source.h` | basic | distinct objects | unsafe | may |
| `cf_text_is_utf8` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_tlv_codec_sized` | `cf_serialize.h` | strong | distinct objects | unsafe | may |
| `cf_tracks_misreads` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_tracks_reads` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_try_bool` | `configforge.h` | strong | safe | safe | none |
| `cf_try_int` | `configforge.h` | strong | safe | safe | none |
| `cf_try_real` | `configforge.h` | strong | safe | safe | none |
| `cf_try_text` | `configforge.h` | strong | safe | safe | none |
| `cf_type_json_name` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_type_name` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_type_of` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_unit_duration_get` | `cf_numeric.h` | nothrow | distinct objects | unsafe | may |
| `cf_unit_format` | `cf_numeric.h` | nothrow | distinct objects | unsafe | may |
| `cf_unit_options_init` | `cf_numeric.h` | nothrow | distinct objects | unsafe | may |
| `cf_unit_parse` | `cf_numeric.h` | strong | distinct objects | unsafe | may |
| `cf_unit_size_get` | `cf_numeric.h` | nothrow | distinct objects | unsafe | may |
| `cf_unit_table_parse` | `cf_numeric.h` | strong | distinct objects | unsafe | may |
| `cf_usage` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_value_bool` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_cmp` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_equal` | `cf_value.h` | strong | distinct objects | unsafe | may |
| `cf_value_from_text` | `cf_value.h` | strong | distinct objects | unsafe | may |
| `cf_value_infer` | `cf_value.h` | strong | distinct objects | unsafe | may |
| `cf_value_int` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_null` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_real` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_text` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_value_to_text` | `cf_value.h` | nothrow | distinct objects | unsafe | may |
| `cf_version` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_version_number` | `cf_version.h` | nothrow | distinct objects | unsafe | may |
| `cf_vfmt` | `cf_port.h` | nothrow | distinct objects | unsafe | may |
| `cf_was_read` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_watch_count` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_watch_path` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_watcher_close` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_watcher_drain` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_watcher_fd` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_watcher_handle` | `configforge.h` | nothrow | distinct objects | unsafe | may |
| `cf_watcher_open` | `configforge.h` | strong | distinct objects | unsafe | may |
| `cf_watcher_wait` | `configforge.h` | basic | distinct objects | unsafe | may |
| `cf_write` | `configforge.h` | strong | distinct objects | unsafe | may |

nothrow 150, strong 108, basic 68, not yet 0

thread safe 10, signal safe 10, alloc none 10

<!-- guarantees:end -->
