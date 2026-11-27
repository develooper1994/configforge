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
and outside the gate. A name in `scripts/check/guarantees_todo.txt` has no
line yet (it reads `not yet` below); the list only shrinks, and the file is
deleted when the last header is annotated.

The gate refuses: a public name with no line; a word that is not one of the
three; a `CF_NODISCARD` function that claims `nothrow` (a function that can
report a failure is not `nothrow`); a to-do name that has a line or is not
public; a table that is not what the headers say (`scripts/check/guarantees.py
--write` rewrites it).

## The library never ends the program

No file of `lib/*/src` or `lib/*/include` calls `abort`, `exit`, `_Exit`,
`quick_exit`, `longjmp`, `siglongjmp`, `setjmp`, `sigsetjmp`, `assert`,
`raise` or `__builtin_trap`. The gate reads each file with its comments and
string literals removed and looks for the name followed by `(`. An exception
is one line `path|token|reason` in `scripts/check/nonlocal.txt`; the list is
empty.

An allocation that fails returns `CF_E_MEMORY`, never another code and never a
crash. That is held at run time by K-9b's tests, not by this gate.

## The table

<!-- guarantees:begin -->

| Function | Header | Guarantee |
| -------- | ------ | --------- |
| `cf_app` | `configforge.h` | strong |
| `cf_app_builder` | `configforge.h` | strong |
| `cf_apply_stats` | `configforge.h` | nothrow |
| `cf_arena_alloc` | `cf_alloc.h` | strong |
| `cf_arena_allocator` | `cf_alloc.h` | nothrow |
| `cf_arena_calloc` | `cf_alloc.h` | strong |
| `cf_arena_capacity` | `cf_alloc.h` | nothrow |
| `cf_arena_dup` | `cf_alloc.h` | strong |
| `cf_arena_fixed` | `cf_alloc.h` | strong |
| `cf_arena_free` | `cf_alloc.h` | nothrow |
| `cf_arena_is_fixed` | `cf_alloc.h` | strong |
| `cf_arena_new` | `cf_alloc.h` | strong |
| `cf_arena_used` | `cf_alloc.h` | nothrow |
| `cf_arena_wipe` | `cf_alloc.h` | nothrow |
| `cf_array_count` | `cf_iterate.h` | not yet |
| `cf_array_key` | `cf_iterate.h` | not yet |
| `cf_blob_arena` | `cf_serialize.h` | strong |
| `cf_blob_arena_done` | `cf_serialize.h` | nothrow |
| `cf_blob_file` | `cf_serialize.h` | basic |
| `cf_blob_mem` | `cf_serialize.h` | nothrow |
| `cf_blob_skip_bom` | `cf_serialize.h` | nothrow |
| `cf_build` | `configforge.h` | strong |
| `cf_build_id` | `cf_version.h` | not yet |
| `cf_build_in` | `configforge.h` | strong |
| `cf_build_limit` | `cf_version.h` | not yet |
| `cf_build_profile` | `cf_version.h` | not yet |
| `cf_builder_abort` | `configforge.h` | nothrow |
| `cf_builder_bool` | `configforge.h` | basic |
| `cf_builder_cli` | `configforge.h` | basic |
| `cf_builder_codec` | `configforge.h` | basic |
| `cf_builder_create` | `configforge.h` | strong |
| `cf_builder_credentials` | `configforge.h` | basic |
| `cf_builder_define` | `configforge.h` | basic |
| `cf_builder_digest_key` | `configforge.h` | basic |
| `cf_builder_dir` | `configforge.h` | basic |
| `cf_builder_document` | `configforge.h` | basic |
| `cf_builder_dotenv` | `configforge.h` | basic |
| `cf_builder_env` | `configforge.h` | basic |
| `cf_builder_env_document` | `configforge.h` | basic |
| `cf_builder_file` | `configforge.h` | basic |
| `cf_builder_file_codec` | `configforge.h` | basic |
| `cf_builder_int` | `configforge.h` | basic |
| `cf_builder_key` | `configforge.h` | basic |
| `cf_builder_limits` | `configforge.h` | basic |
| `cf_builder_log` | `configforge.h` | nothrow |
| `cf_builder_memory` | `configforge.h` | basic |
| `cf_builder_open` | `configforge.h` | basic |
| `cf_builder_preferences` | `configforge.h` | basic |
| `cf_builder_real` | `configforge.h` | basic |
| `cf_builder_registry` | `configforge.h` | basic |
| `cf_builder_replay` | `configforge.h` | basic |
| `cf_builder_response_files` | `configforge.h` | basic |
| `cf_builder_schema` | `configforge.h` | basic |
| `cf_builder_schema_file` | `configforge.h` | basic |
| `cf_builder_secrets` | `configforge.h` | basic |
| `cf_builder_sizing` | `configforge.h` | basic |
| `cf_builder_source` | `configforge.h` | basic |
| `cf_builder_store` | `configforge.h` | basic |
| `cf_builder_text` | `configforge.h` | basic |
| `cf_builder_track_reads` | `configforge.h` | nothrow |
| `cf_builder_unknown` | `configforge.h` | nothrow |
| `cf_builder_writable` | `configforge.h` | basic |
| `cf_changed` | `configforge.h` | strong |
| `cf_cli_args` | `cf_entry.h` | not yet |
| `cf_cli_expand` | `cf_entry.h` | not yet |
| `cf_code_description` | `cf_error.h` | nothrow |
| `cf_code_name` | `cf_error.h` | nothrow |
| `cf_codec_among` | `cf_serialize.h` | nothrow |
| `cf_codec_at` | `cf_serialize.h` | nothrow |
| `cf_codec_by_name` | `cf_serialize.h` | nothrow |
| `cf_codec_by_path` | `cf_serialize.h` | nothrow |
| `cf_codec_can_read` | `cf_serialize.h` | strong |
| `cf_codec_compose_sized` | `cf_serialize.h` | strong |
| `cf_codec_count` | `cf_serialize.h` | nothrow |
| `cf_codec_decode` | `cf_serialize.h` | basic |
| `cf_codec_detect` | `cf_serialize.h` | basic |
| `cf_codec_kind` | `cf_serialize.h` | nothrow |
| `cf_codec_select` | `cf_serialize.h` | nothrow |
| `cf_config_codec` | `configforge.h` | nothrow |
| `cf_config_free` | `configforge.h` | nothrow |
| `cf_config_ref` | `configforge.h` | nothrow |
| `cf_config_sizing` | `configforge.h` | strong |
| `cf_count` | `configforge.h` | nothrow |
| `cf_def_init_sized` | `cf_define.h` | not yet |
| `cf_default_allocator` | `cf_alloc.h` | nothrow |
| `cf_defs` | `configforge.h` | nothrow |
| `cf_defs_apply` | `cf_define.h` | not yet |
| `cf_derive` | `configforge.h` | strong |
| `cf_derive_in` | `configforge.h` | strong |
| `cf_diff` | `configforge.h` | nothrow |
| `cf_diff_json` | `configforge.h` | strong |
| `cf_diff_text` | `configforge.h` | strong |
| `cf_digest` | `configforge.h` | nothrow |
| `cf_edit_abort` | `cf_edit.h` | not yet |
| `cf_edit_begin` | `cf_edit.h` | not yet |
| `cf_edit_commit` | `cf_edit.h` | not yet |
| `cf_edit_count` | `cf_edit.h` | not yet |
| `cf_edit_set_bool` | `cf_edit.h` | not yet |
| `cf_edit_set_int` | `cf_edit.h` | not yet |
| `cf_edit_set_real` | `cf_edit.h` | not yet |
| `cf_edit_set_text` | `cf_edit.h` | not yet |
| `cf_edit_unset` | `cf_edit.h` | not yet |
| `cf_emit` | `cf_emit.h` | not yet |
| `cf_encoding_by_name` | `cf_encoding.h` | strong |
| `cf_encoding_check` | `cf_encoding.h` | strong |
| `cf_encoding_name` | `cf_encoding.h` | nothrow |
| `cf_encoding_recode` | `cf_encoding.h` | strong |
| `cf_env_key` | `cf_source.h` | not yet |
| `cf_env_value` | `cf_entry.h` | not yet |
| `cf_environ` | `cf_source.h` | not yet |
| `cf_error_clear` | `cf_error.h` | nothrow |
| `cf_error_copy` | `cf_error.h` | nothrow |
| `cf_error_place` | `cf_error.h` | nothrow |
| `cf_error_place_at` | `cf_error.h` | nothrow |
| `cf_error_set` | `cf_error.h` | nothrow |
| `cf_explain` | `configforge.h` | strong |
| `cf_explain_json` | `configforge.h` | strong |
| `cf_feature_at` | `cf_version.h` | not yet |
| `cf_feature_count` | `cf_version.h` | not yet |
| `cf_file_options_init` | `cf_file.h` | nothrow |
| `cf_file_replace` | `cf_file.h` | strong |
| `cf_file_writer_close` | `cf_file.h` | basic |
| `cf_file_writer_flush` | `cf_file.h` | basic |
| `cf_file_writer_open` | `cf_file.h` | basic |
| `cf_file_writer_sync` | `cf_file.h` | basic |
| `cf_file_writer_write` | `cf_file.h` | basic |
| `cf_fingerprint` | `configforge.h` | nothrow |
| `cf_fingerprint_format` | `configforge.h` | nothrow |
| `cf_fingerprint_seed` | `cf_define.h` | not yet |
| `cf_fingerprint_step` | `cf_define.h` | not yet |
| `cf_fmt` | `cf_port.h` | nothrow |
| `cf_fmt_append` | `cf_port.h` | nothrow |
| `cf_format_holds` | `cf_serialize.h` | strong |
| `cf_format_limits` | `cf_serialize.h` | strong |
| `cf_format_of_path` | `cf_serialize.h` | nothrow |
| `cf_format_option` | `cf_serialize.h` | nothrow |
| `cf_get_bool` | `configforge.h` | nothrow |
| `cf_get_int` | `configforge.h` | nothrow |
| `cf_get_real` | `configforge.h` | nothrow |
| `cf_get_text` | `configforge.h` | nothrow |
| `cf_get_text_n` | `configforge.h` | nothrow |
| `cf_has` | `configforge.h` | strong |
| `cf_has_feature` | `cf_version.h` | not yet |
| `cf_image_at` | `cf_image.h` | not yet |
| `cf_image_count` | `cf_image.h` | not yet |
| `cf_image_get` | `cf_image.h` | not yet |
| `cf_image_load` | `cf_image.h` | not yet |
| `cf_image_open` | `cf_image.h` | not yet |
| `cf_image_write` | `cf_image.h` | not yet |
| `cf_inspect` | `configforge.h` | strong |
| `cf_iter_children` | `cf_iterate.h` | not yet |
| `cf_iter_keys` | `cf_iterate.h` | not yet |
| `cf_iter_next` | `cf_iterate.h` | not yet |
| `cf_key_cmp` | `cf_keyspace.h` | not yet |
| `cf_key_segments` | `cf_keyspace.h` | not yet |
| `cf_key_valid` | `cf_error.h` | strong |
| `cf_keymap_from_key` | `cf_entry.h` | not yet |
| `cf_keymap_init` | `cf_entry.h` | not yet |
| `cf_keymap_to_key` | `cf_entry.h` | not yet |
| `cf_keys` | `configforge.h` | nothrow |
| `cf_keysink_init` | `cf_entry.h` | not yet |
| `cf_keysink_of` | `cf_entry.h` | not yet |
| `cf_ks_add` | `cf_keyspace.h` | not yet |
| `cf_ks_add_authored` | `cf_keyspace.h` | not yet |
| `cf_ks_add_candidate` | `cf_keyspace.h` | not yet |
| `cf_ks_add_default` | `cf_keyspace.h` | not yet |
| `cf_ks_add_resolved` | `cf_keyspace.h` | not yet |
| `cf_ks_add_text` | `cf_keyspace.h` | not yet |
| `cf_ks_arena` | `cf_keyspace.h` | not yet |
| `cf_lex_cli` | `cf_entry.h` | not yet |
| `cf_lex_dotenv` | `cf_entry.h` | not yet |
| `cf_lex_env` | `cf_entry.h` | not yet |
| `cf_limit_options_init` | `configforge.h` | nothrow |
| `cf_listsink_finish` | `cf_entry.h` | not yet |
| `cf_listsink_init` | `cf_entry.h` | not yet |
| `cf_listsink_of` | `cf_entry.h` | not yet |
| `cf_live_approve` | `cf_live.h` | not yet |
| `cf_live_close` | `cf_live.h` | not yet |
| `cf_live_commit` | `cf_live.h` | not yet |
| `cf_live_confirm` | `cf_live.h` | not yet |
| `cf_live_generation` | `cf_live.h` | not yet |
| `cf_live_get` | `cf_live.h` | not yet |
| `cf_live_last_error` | `cf_live.h` | not yet |
| `cf_live_open` | `cf_live.h` | not yet |
| `cf_live_open_with` | `cf_live.h` | not yet |
| `cf_live_options_init` | `cf_live.h` | not yet |
| `cf_live_pending` | `cf_live.h` | not yet |
| `cf_live_poll` | `cf_live.h` | not yet |
| `cf_live_reject` | `cf_live.h` | not yet |
| `cf_live_reload` | `cf_live.h` | not yet |
| `cf_live_restarts` | `cf_live.h` | not yet |
| `cf_live_rollback` | `cf_live.h` | not yet |
| `cf_live_status_get` | `cf_live.h` | not yet |
| `cf_live_wait` | `cf_live.h` | not yet |
| `cf_loc_text` | `cf_serialize.h` | nothrow |
| `cf_lock_none` | `cf_lock.h` | nothrow |
| `cf_lock_sidecar` | `cf_lock.h` | nothrow |
| `cf_log_level_name` | `cf_log.h` | not yet |
| `cf_outcome_can_win` | `cf_keyspace.h` | not yet |
| `cf_outcome_name` | `cf_keyspace.h` | not yet |
| `cf_platform_has_dir_list` | `cf_platform.h` | strong |
| `cf_platform_has_environ` | `cf_platform.h` | strong |
| `cf_platform_has_preferences` | `cf_platform.h` | strong |
| `cf_platform_has_private_create` | `cf_platform.h` | strong |
| `cf_platform_has_registry` | `cf_platform.h` | strong |
| `cf_platform_has_trusted_dir` | `cf_platform.h` | strong |
| `cf_platform_has_wake` | `cf_platform.h` | strong |
| `cf_platform_has_wide_args` | `cf_platform.h` | strong |
| `cf_platform_name` | `cf_platform.h` | nothrow |
| `cf_policy_text` | `cf_define.h` | not yet |
| `cf_port_description` | `cf_port.h` | nothrow |
| `cf_read_count` | `configforge.h` | nothrow |
| `cf_reason_description` | `cf_error.h` | nothrow |
| `cf_reason_name` | `cf_error.h` | nothrow |
| `cf_reason_severity` | `cf_error.h` | nothrow |
| `cf_reason_weakness` | `cf_error.h` | nothrow |
| `cf_refusal_name` | `cf_keyspace.h` | not yet |
| `cf_refusal_text` | `cf_keyspace.h` | not yet |
| `cf_report` | `configforge.h` | strong |
| `cf_report_key_write` | `cf_report.h` | not yet |
| `cf_report_write` | `cf_report.h` | not yet |
| `cf_schema` | `configforge.h` | strong |
| `cf_schema_write` | `cf_report.h` | not yet |
| `cf_serialize_version` | `cf_serialize_version.h` | nothrow |
| `cf_serialize_version_number` | `cf_serialize_version.h` | nothrow |
| `cf_sink_fmt` | `cf_serialize.h` | basic |
| `cf_sink_init` | `cf_serialize.h` | nothrow |
| `cf_sink_json_string` | `cf_serialize.h` | basic |
| `cf_sink_json_value` | `cf_serialize.h` | basic |
| `cf_sink_overflowed` | `cf_serialize.h` | strong |
| `cf_sink_putc` | `cf_serialize.h` | basic |
| `cf_sink_puts` | `cf_serialize.h` | basic |
| `cf_sink_terminate` | `cf_serialize.h` | basic |
| `cf_sink_write` | `cf_serialize.h` | basic |
| `cf_sizing_options_init` | `configforge.h` | nothrow |
| `cf_source_cli` | `cf_source.h` | not yet |
| `cf_source_cli_defs` | `cf_define.h` | not yet |
| `cf_source_defaults` | `cf_define.h` | not yet |
| `cf_source_destroy` | `cf_source.h` | not yet |
| `cf_source_dir` | `cf_source.h` | not yet |
| `cf_source_dir_codecs` | `cf_source.h` | not yet |
| `cf_source_dir_error` | `cf_source.h` | not yet |
| `cf_source_dir_state_size` | `cf_source.h` | not yet |
| `cf_source_document` | `cf_source.h` | not yet |
| `cf_source_dotenv` | `cf_source.h` | not yet |
| `cf_source_dotenv_defs` | `cf_define.h` | not yet |
| `cf_source_dotenv_error` | `cf_source.h` | not yet |
| `cf_source_dotenv_state_size` | `cf_source.h` | not yet |
| `cf_source_env` | `cf_source.h` | not yet |
| `cf_source_env_defs` | `cf_define.h` | not yet |
| `cf_source_env_document` | `cf_source.h` | not yet |
| `cf_source_file` | `cf_source.h` | not yet |
| `cf_source_file_codec` | `cf_source.h` | not yet |
| `cf_source_file_codecs` | `cf_source.h` | not yet |
| `cf_source_file_defs` | `cf_define.h` | not yet |
| `cf_source_file_encoding` | `cf_source.h` | not yet |
| `cf_source_file_error` | `cf_source.h` | not yet |
| `cf_source_file_format` | `cf_source.h` | not yet |
| `cf_source_file_state_size` | `cf_source.h` | not yet |
| `cf_source_image_error` | `cf_image.h` | not yet |
| `cf_source_image_file` | `cf_image.h` | not yet |
| `cf_source_image_mem` | `cf_image.h` | not yet |
| `cf_source_image_state_size` | `cf_image.h` | not yet |
| `cf_source_load` | `cf_source.h` | not yet |
| `cf_source_memory` | `cf_source.h` | not yet |
| `cf_source_path` | `cf_source.h` | not yet |
| `cf_source_registry` | `cf_source.h` | not yet |
| `cf_source_registry_error` | `cf_source.h` | not yet |
| `cf_source_registry_state_size` | `cf_source.h` | not yet |
| `cf_source_replay` | `cf_report.h` | not yet |
| `cf_source_replay_error` | `cf_report.h` | not yet |
| `cf_source_replay_state_size` | `cf_report.h` | not yet |
| `cf_source_secrets` | `cf_source.h` | not yet |
| `cf_source_secrets_error` | `cf_source.h` | not yet |
| `cf_source_secrets_state_size` | `cf_source.h` | not yet |
| `cf_source_snapshot` | `cf_source.h` | not yet |
| `cf_source_store` | `cf_source.h` | not yet |
| `cf_source_store_error` | `cf_source.h` | not yet |
| `cf_source_store_state_size` | `cf_source.h` | not yet |
| `cf_sources` | `configforge.h` | nothrow |
| `cf_store_set` | `cf_source.h` | not yet |
| `cf_text_is_utf8` | `cf_serialize.h` | strong |
| `cf_tlv_codec_sized` | `cf_serialize.h` | strong |
| `cf_tracks_reads` | `configforge.h` | strong |
| `cf_try_bool` | `configforge.h` | strong |
| `cf_try_int` | `configforge.h` | strong |
| `cf_try_real` | `configforge.h` | strong |
| `cf_try_text` | `configforge.h` | strong |
| `cf_type_json_name` | `cf_value.h` | nothrow |
| `cf_type_name` | `cf_value.h` | nothrow |
| `cf_type_of` | `configforge.h` | nothrow |
| `cf_usage` | `configforge.h` | strong |
| `cf_value_bool` | `cf_value.h` | nothrow |
| `cf_value_cmp` | `cf_value.h` | nothrow |
| `cf_value_equal` | `cf_value.h` | strong |
| `cf_value_from_text` | `cf_value.h` | strong |
| `cf_value_infer` | `cf_value.h` | strong |
| `cf_value_int` | `cf_value.h` | nothrow |
| `cf_value_null` | `cf_value.h` | nothrow |
| `cf_value_real` | `cf_value.h` | nothrow |
| `cf_value_text` | `cf_value.h` | nothrow |
| `cf_value_to_text` | `cf_value.h` | nothrow |
| `cf_version` | `cf_version.h` | not yet |
| `cf_version_number` | `cf_version.h` | not yet |
| `cf_vfmt` | `cf_port.h` | nothrow |
| `cf_was_read` | `configforge.h` | strong |
| `cf_watch_count` | `configforge.h` | nothrow |
| `cf_watch_path` | `configforge.h` | nothrow |
| `cf_watcher_close` | `configforge.h` | nothrow |
| `cf_watcher_drain` | `configforge.h` | strong |
| `cf_watcher_fd` | `configforge.h` | nothrow |
| `cf_watcher_handle` | `configforge.h` | nothrow |
| `cf_watcher_open` | `configforge.h` | strong |
| `cf_watcher_wait` | `configforge.h` | basic |
| `cf_write` | `configforge.h` | strong |

nothrow 81, strong 57, basic 47, not yet 130

<!-- guarantees:end -->
