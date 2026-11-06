# API reference

Generated from the public headers by `scripts/generate/apiref.cmake`, and
checked against them by `gate.apiref`: edit the comment in the
header, then run the script. Every function is listed with the
comment written above it, under the section of the header it is
in; the types are in the headers themselves. Implementation links
open the source on `main`, with platform variants listed together.

Semantic versioning covers all of it (D-56) except the internal
tier, which the library's own layers use and 1.x may change:
`cf_flatten.h`, `cf_keyspace.h`, `cf_numeric.h`, `cf_platform.h`,
the definition set's `cf_defset_*` and `cf_def_*` in `cf_define.h`,
and `cf_port.h`'s `cf_ref_*`, `cf_fmt*` and `cf_cmp_*`. The flags a
definition takes are bits 0-15 and a source's bits 16 and up; an
unknown bit is refused by name.

A struct the caller fills and the library reads - `cf_error`,
`cf_def`, `cf_live_options`, `cf_live_status`, `cf_log_event`,
`cf_report_info` and Doctor's `cf_doctor_gate` - begins with
`size`, so 1.x can append a member (D-51): start from
`CF_ERROR_INIT`, `cf_def_init`, `CF_LIVE_OPTIONS_INIT`,
`CF_REPORT_INFO_INIT` or `CF_DOCTOR_GATE_INIT`, or set `size` to
`sizeof` the struct. Their members are in the headers.

In C11 and later `configforge.h` also has `cf_get(cfg, key, &out)`,
a macro typed by `out` - `int`, `long`, `long long`, `_Bool`,
`const char*`, and `double` where `CF_WITH_REAL` is on - that calls
the matching `cf_get_*` below; C++ has `get<T>` instead.

Every error carries one of these codes, stable from 1.0;
`cf_code_name` and `cf_code_description` give the last two columns.

| Code | Name | Meaning |
| ---- | ---- | ------- |
| `CF_OK` | `ok` | no error |
| `CF_E_KEY` | `key` | malformed or unknown key |
| `CF_E_DEFINITION` | `definition` | a definition contradicts itself |
| `CF_E_CONVERT` | `convert` | text does not denote that type |
| `CF_E_VALIDATE` | `validate` | outside a range, or not permitted |
| `CF_E_REQUIRED` | `required` | a required parameter has no value |
| `CF_E_SOURCE` | `source` | a source could not be read |
| `CF_E_CAPABILITY` | `capability` | a source cannot do what was asked |
| `CF_E_POLICY` | `policy` | policy conflict |
| `CF_E_MEMORY` | `memory` | allocation refused or arena full |

- [`configforge.h`](#configforgeh) - 77
- [`cf_alloc.h`](#cf_alloch) - 12
- [`cf_define.h`](#cf_defineh) - 27
- [`cf_doctor.h`](#cf_doctorh) - 25
- [`cf_edit.h`](#cf_edith) - 9
- [`cf_emit.h`](#cf_emith) - 1
- [`cf_entry.h`](#cf_entryh) - 13
- [`cf_error.h`](#cf_errorh) - 5
- [`cf_explain.h`](#cf_explainh) - 4
- [`cf_flatten.h`](#cf_flattenh) - 6
- [`cf_image.h`](#cf_imageh) - 10
- [`cf_iterate.h`](#cf_iterateh) - 5
- [`cf_keyspace.h`](#cf_keyspaceh) - 33
- [`cf_live.h`](#cf_liveh) - 17
- [`cf_log.h`](#cf_logh) - 1
- [`cf_numeric.h`](#cf_numerich) - 7
- [`cf_platform.h`](#cf_platformh) - 19
- [`cf_port.h`](#cf_porth) - 10
- [`cf_report.h`](#cf_reporth) - 6
- [`cf_serialize.h`](#cf_serializeh) - 21
- [`cf_source.h`](#cf_sourceh) - 30
- [`cf_value.h`](#cf_valueh) - 12
- [`cf_version.h`](#cf_versionh) - 7

## configforge.h

configforge.h - the public interface.

One include. Everything below is reachable from here, and nothing
above Tier 0 has to be learned to use Tier 0.

    cf_error    err = CF_ERROR_INIT;
    cf_config*  cfg;
    cf_i64      port;

    cfg = cf_app("myapp", argc, argv, &err);
    if (cfg == NULL) { fputs(err.message, stderr); return 1; }
    port = cf_get_int(cfg, "server.port", 8080);
    cf_config_free(cfg);

### Tier 0

The conventional sources and the conventional precedence for a named
application:

    command line  >  environment (NAME_)  >  built-in default

cf_app reads no file. For files, start from cf_app_builder, which
holds the same two sources, and add cf_builder_file at
CF_RANK_USER_FILE or CF_RANK_SYS_FILE. Both expand response files
(@file) and take the command line as UTF-8.

```c
CF_NODISCARD CF_API cf_config* cf_app(const char* name, int argc, char* const* argv, cf_error* err);
```

Implementation: [`cf_app` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1287).

### Tier 1 and 2

The builder owns an arena; the configuration inherits it. A builder
is consumed by cf_builder_create whether that succeeds or fails, so
there is exactly one way to release it and no path that leaks.
Abandoning one without creating is cf_builder_abort.

The per-call error is deferred rather than returned: a definition
block reads as a block, and the first failure is reported once by
cf_builder_create. Adding a check after every line would be the
ceremony this design exists to remove.

```c
CF_API cf_builder* cf_build(const cf_allocator* alloc);
```

Implementation: [`cf_build` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L191).

```c
CF_API cf_builder* cf_build_in(cf_arena* arena);
```

Implementation: [`cf_build_in` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L195).

Build inside a buffer the caller owns.

This is the freestanding path, and it was missing: cf_arena_fixed
existed and nothing in the builder accepted an arena, so the claim
that a target with no heap can hand over a buffer had no way to be
true. Found by writing the test for it.

The arena is borrowed, not owned: cf_config_free releases the
configuration's hold on it and never frees the caller's buffer.
Everything the configuration points at lives in that buffer, so it
has to outlive the configuration - which on the target this exists
for is a static array, and outlives everything.

```c
CF_API cf_builder* cf_app_builder(const char* name, int argc, char* const* argv);
```

Implementation: [`cf_app_builder` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L231).

cf_app's builder, before create: the command line (CF_RANK_CLI) and
the NAME_ environment (CF_RANK_ENV), for adding files and definitions
to.

```c
CF_API void cf_builder_abort(cf_builder* b);
```

Implementation: [`cf_builder_abort` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L202).

Frees a builder that will not be created; a cf_build_in arena is the
caller's and is left alone.

```c
CF_API cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank, const cf_pair* pairs, size_t count);
CF_API cf_bool cf_builder_env(cf_builder* b, const char* name, int rank, const char* prefix, char* const* environment);
CF_API cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc, char* const* argv);
```

Implementation: [`cf_builder_memory` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L312), [`cf_builder_env` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L322), [`cf_builder_cli` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L333).

The pairs, the environment and the command line are borrowed: the
builder keeps the pointer, not a copy, and reads it when
cf_builder_create runs, so what it points at has to stay valid until
that returns - an argument vector filled in an inner block is gone by
then. What the configuration holds is its own copy, so none of the
three has to outlive it. The name and the prefix are copied at once.

```c
CF_API cf_bool cf_builder_file(cf_builder* b, const char* name, int rank, const char* path, const char* format, unsigned flags);
```

Implementation: [`cf_builder_file` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L343).

The format is taken from the extension unless one is named. A file
whose format this build left out is an error that says which option
to turn on; CF_OPTIONAL makes a *missing* file acceptable, and
never makes an unreadable one acceptable.

```c
CF_API cf_bool cf_builder_file_codec(cf_builder* b, const char* name, int rank, const char* path, const struct cf_codec* codec, unsigned flags);
```

Implementation: [`cf_builder_file_codec` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L380).

A file read by a codec the caller supplies, for a format this library
does not have; see cf_source_file_codec. The codec must outlive the
configuration.

```c
CF_API cf_bool cf_builder_codec(cf_builder* b, const struct cf_codec* codec);
```

Implementation: [`cf_builder_codec` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L547).

```c
CF_API const struct cf_codec* cf_config_codec(const cf_config* c, const char* format);
```

Implementation: [`cf_config_codec` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L575).

The codec a configuration reads and writes `format` with: one its
builder was given, else the built-in one, else NULL.

```c
CF_API cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank, const char* path, const char* prefix, unsigned flags);
```

Implementation: [`cf_builder_dotenv` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L415).

A .env file, read with the environment's key mapping: a .env file is
an environment written down, so the escape and the natural spelling
behave identically in both.

```c
CF_API cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank, const char* path, const char* format, unsigned flags);
```

Implementation: [`cf_builder_dir` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L509).

A drop-in directory: every file in byte order, all at one rank, so
10-base losing to 90-site is precedence rather than a merge rule and
explain() names the file that won.

```c
CF_API cf_bool cf_builder_secrets(cf_builder* b, const char* name, int rank, const char* path, const char* prefix, unsigned flags);
```

Implementation: [`cf_builder_secrets` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L439).

A directory of secrets, one file a key, as containers mount them and
systemd passes credentials (cf_source_secrets says how names map).

```c
CF_API cf_bool cf_builder_registry(cf_builder* b, const char* name, int rank, const char* root, unsigned flags);
```

Implementation: [`cf_builder_registry` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L463).

A key of the Windows Registry and its subkeys (cf_source_registry
says how names and types map). Windows only; elsewhere it fails by
name, CF_OPTIONAL or not.

```c
CF_API cf_bool cf_builder_store(cf_builder* b, const char* name, int rank, const char* path, unsigned flags);
```

Implementation: [`cf_builder_store` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L486).

A SQLite store (CF_WITH_SQLITE): typed values, each with what its
writer said about it. cf_write writes one, when it is granted.

```c
CF_API cf_bool cf_builder_writable(cf_builder* b, const char* source_name);
```

Implementation: [`cf_builder_writable` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L935).

Allow cf_write to write the source of this name - the program's
permission, as against the source's capability. Every source is read
only until granted. A grant to a source that cannot be written, or to
no source of that name, fails cf_builder_create, before anything is
read. A derived configuration keeps the grants of the one it came
from.

```c
CF_API cf_bool cf_builder_source(cf_builder* b, const cf_source* s);
```

Implementation: [`cf_builder_source` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L608).

Register a source the caller built itself. The source object is
copied, so the caller's may be on the stack; any state it points at
must outlive the configuration.

```c
CF_API cf_bool cf_builder_define(cf_builder* b, const cf_def* d);
```

Implementation: [`cf_builder_define` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L675).

```c
CF_API cf_bool cf_builder_schema(cf_builder* b, const char* text, size_t n, const char* name);
CF_API cf_bool cf_builder_schema_file(cf_builder* b, const char* path);
```

Implementation: [`cf_builder_schema` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L649), [`cf_builder_schema_file` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L662).

The contract from a JSON Schema - the text, or a file - as
cf_defset_schema reads one: the definitions cf_schema would write
back. A keyword it does not read is refused by name and place, and
cf_builder_create reports it. (1.2)

```c
CF_API cf_bool cf_builder_open(cf_builder* b, const char* prefix);
CF_API cf_bool cf_builder_bool(cf_builder* b, const char* key, cf_bool dflt, unsigned flags);
CF_API cf_bool cf_builder_int(cf_builder* b, const char* key, cf_i64 dflt, cf_i64 min, cf_i64 max, unsigned flags);
CF_API cf_bool cf_builder_text(cf_builder* b, const char* key, const char* dflt, unsigned flags);
CF_API cf_bool cf_builder_real(cf_builder* b, const char* key, double dflt, double min, double max, unsigned flags);
```

Implementation: [`cf_builder_open` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L636), [`cf_builder_bool` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L688), [`cf_builder_int` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L700), [`cf_builder_text` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L713), [`cf_builder_real` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L727).

Exempt a prefix from unknown-key rejection, for a key space the
program cannot know in advance: a plugin's settings are not in the
contract because the plugin is not either.

```c
CF_API cf_bool cf_builder_key(cf_builder* b, const char* key, cf_type type, const char* dflt, const char* min, const char* max, unsigned flags);
```

Implementation: [`cf_builder_key` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L782).

One spelling for every scalar type (D-74), and the one every binding
passes through. The default and the bounds are text, read by the
rules a file's value is; `type` names the type, or is CF_TYPE_NULL
for "the default decides": "true" a bool, "8080" an int, "0.5" a
real, anything else text. `min` and `max` bound an int or a real and
are NULL for no range. A default or a bound that does not read as the
type is refused, a range on a bool or text is refused, and so is a
key with neither a type nor a default. (1.3)

```c
CF_API void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy);
```

Implementation: [`cf_builder_unknown` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L831).

```c
CF_API void cf_builder_log(cf_builder* b, cf_log_fn fn, void* user, cf_log_level min);
```

Implementation: [`cf_builder_log` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L843).

Where what deserves a log line and is not an error goes (cf_log.h):
unknown keys under CF_UNKNOWN_WARN, deprecated keys in use, values
refused and fallen back from. Events below `min` are not made. A
configuration derived from this one logs to the same place.

```c
CF_API void cf_builder_track_reads(cf_builder* b);
```

Implementation: [`cf_builder_track_reads` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L837).

Record which keys the program reads, for finding configuration
nothing uses. Off by default, and nothing changes for a read when it
is off. On, a key counts as read the first time cf_get_*, cf_try_*,
cf_has or cf_type_of asks for it through this snapshot; listing or
reporting is not a read. Reads stay safe concurrently (D-14): the
flag is atomic and written once. A configuration derived from this
one tracks too, from nothing. cf_was_read and cf_read_count answer;
cf_report adds "read" to every key, a boolean, so a blind report
still carries no value.

```c
CF_NODISCARD CF_API cf_config* cf_builder_create(cf_builder* b, cf_error* err);
```

Implementation: [`cf_builder_create` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L985).

### Reading

These do not fail. Everything that can go wrong was detected while
the configuration was being built and reported once, there.

A read of the wrong type returns the fallback rather than coercing:
with a definition present that is a programming error, and coercing
would hide it. cf_try_* distinguishes absent from wrong-type for
callers who want to know, and cf_type_of answers directly.

```c
CF_API cf_bool cf_get_bool(const cf_config* c, const char* key, cf_bool fallback);
CF_API cf_i64 cf_get_int(const cf_config* c, const char* key, cf_i64 fallback);
CF_API const char* cf_get_text(const cf_config* c, const char* key, const char* fallback);
CF_API double cf_get_real(const cf_config* c, const char* key, double fallback);
```

Implementation: [`cf_get_bool` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1940), [`cf_get_int` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1945), [`cf_get_text` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1950), [`cf_get_real` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1972).

```c
CF_API const char* cf_get_text_n(const cf_config* c, const char* key, size_t* len, const char* fallback);
```

Implementation: [`cf_get_text_n` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1955).

Pointer and length, for text that may contain anything. The stored
value is NUL-terminated as well, so cf_get_text stays free.

```c
CF_NODISCARD CF_API cf_code cf_try_bool(const cf_config* c, const char* key, cf_bool* out);
CF_NODISCARD CF_API cf_code cf_try_int(const cf_config* c, const char* key, cf_i64* out);
CF_NODISCARD CF_API cf_code cf_try_text(const cf_config* c, const char* key, const char** out, size_t* len);
CF_NODISCARD CF_API cf_code cf_try_real(const cf_config* c, const char* key, double* out);
```

Implementation: [`cf_try_bool` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1992), [`cf_try_int` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2001), [`cf_try_text` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2010), [`cf_try_real` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2026).

CF_OK, CF_E_KEY when absent, CF_E_CONVERT on a type mismatch.

```c
CF_API cf_type cf_type_of(const cf_config* c, const char* key);
CF_API cf_bool cf_has(const cf_config* c, const char* key);
```

Implementation: [`cf_type_of` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2036), [`cf_has` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2041).

CF_TYPE_NULL when the key is absent.

```c
CF_API cf_bool cf_tracks_reads(const cf_config* c);
CF_API cf_bool cf_was_read(const cf_config* c, const char* key);
CF_API size_t cf_read_count(const cf_config* c);
```

Implementation: [`cf_tracks_reads` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2045), [`cf_was_read` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2049), [`cf_read_count` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2060).

Read coverage (cf_builder_track_reads). CF_FALSE and 0 for a
configuration that does not track. cf_read_count counts keys, not
reads; neither of these is itself a read.

### Diagnostics

No setup, no flag, no build option. The provenance chain is what
get() reads, so printing it costs nothing that was not already paid.

A key defined CF_SENSITIVE has its chain shown in full and its values
shown as "&lt;sensitive, N bytes>": which source won and where it was
set is the question, and it is not the secret.

```c
CF_API size_t cf_explain(const cf_config* c, const char* key, char* buf, size_t cap);
```

Implementation: [`cf_explain` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2078).

```c
CF_API size_t cf_explain_json(const cf_config* c, const char* key, cf_report_mode mode, char* buf, size_t cap);
CF_API size_t cf_inspect(const cf_config* c, char* buf, size_t cap);
```

Implementation: [`cf_explain_json` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2142), [`cf_inspect` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2089).

The same chain as JSON, for an administration endpoint or a tool:
cf_report_key_write says what it holds. CF_REPORT_BLIND is what an
endpoint wants - presence, origin, length and digest for a sensitive
key, never its value.

```c
CF_API size_t cf_diff(const cf_config* before, const cf_config* after, cf_diff_fn fn, void* ctx);
```

Implementation: [`cf_diff` (src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/diff.c#L31).

The number of changed keys; `fn` may be NULL to only count them.

```c
CF_API size_t cf_diff_text(const cf_config* before, const cf_config* after, char* buf, size_t cap);
```

Implementation: [`cf_diff_text` (src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/diff.c#L272).

One line per change, with where each new value came from, and the
values of CF_SENSITIVE keys redacted as explain() redacts them:

  server.port: 8080 -> 9443 (file app.ini:2:8)
  log.level: added, "debug" (cli)
  cache.size: 64, now from file app.ini:7:14, was from default
  db.password: changed, &lt;sensitive> (env)

Measure-or-write, like every renderer here.

```c
CF_API size_t cf_diff_json(const cf_config* before, const cf_config* after, char* buf, size_t cap);
```

Implementation: [`cf_diff_json` (src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/diff.c#L260).

The same changes as JSON Lines, one object a line, for a log pipeline:

  {"key": "server.port", "change": "value", "before": 8080,
   "after": 9443, "from": {"source": "file", "at": "app.ini:2:8"}}

"change" is added, removed, value or origin; "from" is where the
new value is, "was_from" where the old one was when that differs. A
CF_SENSITIVE key has "sensitive": true and no values. Measure-or-write.

```c
CF_API cf_u64 cf_digest(const cf_config* c);
```

Implementation: [`cf_digest` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2113).

Order-independent over the resolved configuration, identical on
every platform and in both portability modes.

```c
CF_API cf_u64 cf_fingerprint(const cf_config* c);
```

Implementation: [`cf_fingerprint` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2117).

What identifies this configuration where operators look - a log line,
a trace, a report's "fingerprint": as cf_digest, except that a secret
counts by the digest a value-blind report shows for it, never by its
value, so the fingerprint can be published wherever a blind report
could. Two configurations with the same values and secrets have the
same one, on every platform. Not cryptographic: it tells changes
apart, and proves nothing. (1.1)

```c
CF_API size_t cf_fingerprint_format(cf_u64 fp, char* buf, size_t cap);
```

Implementation: [`cf_fingerprint_format` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2122).

`fp` as reports and logs write it, "fnv1a:" and sixteen hex digits,
in `buf` as far as `cap` allows; the length it needs, 22. (1.1)

```c
CF_API size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf, size_t cap);
```

Implementation: [`cf_report` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2154).

The provenance report: the whole key space with every candidate,
serialised. Doctor compares two of them; the library can read one
back with cf_builder_replay. Measure-or-write. `info` starts from
CF_REPORT_INFO_INIT, or is NULL for a blind report that names
nothing; one that did not start there is refused, as cf_report_write
says, and nothing is written.

A full report contains every value and is therefore a
credential-bearing artifact. The blind form keeps presence, origin,
length and a digest, which is everything an audit needs and nothing
a leak would want.

```c
CF_API size_t cf_schema(const cf_config* c, const char* title, char* buf, size_t cap);
```

Implementation: [`cf_schema` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2099).

The configuration's contract as a JSON Schema, closed when it rejects
unknown keys (CF_UNKNOWN_REJECT). See cf_schema_write.

```c
CF_API cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank, const char* path, unsigned flags);
```

Implementation: [`cf_builder_replay` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L585).

Load a full report as a source. Reproduces exactly the configuration
a process was running, rather than a reconstruction from files that
may have changed since. `flags` is a source's: CF_OPTIONAL, or 0.

```c
CF_API size_t cf_count(const cf_config* c);
CF_API const cf_apply_report* cf_apply_stats(const cf_config* c);
CF_API const cf_keyspace* cf_keys(const cf_config* c);
CF_API const cf_source_view* cf_sources(const cf_config* c);
CF_API const cf_defset* cf_defs(const cf_config* c);
```

Implementation: [`cf_count` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2126), [`cf_apply_stats` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2130), [`cf_keys` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2165), [`cf_sources` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2134), [`cf_defs` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L2138).

How many keys the configuration knows. A key is known once anything
supplied it, so one that has no value is counted too: every candidate
for it was refused, or it is an element beyond the end of its array.
cf_has says which keys have one.

### Ownership and threads

The contract, decision D-14:

  Reads are safe concurrently. Building and releasing are the
  caller's to synchronise.

Concurrent reads are the entire point of an immutable snapshot;
locking the read path would contradict the performance model and buy
nothing, because there is nothing to protect. An unstated threading
contract is assumed to be full internal locking by users and nothing
at all by implementers, which is why this is written down rather than
left to be inferred.

A snapshot is reference counted, so a reader can keep using one while
another is built. The count is atomic; everything it protects is
immutable.

### Changing a value

A snapshot is immutable, so setting one means deriving another. The
original stays valid and unchanged, which is the point: a reader
holding it keeps seeing a configuration that does not move underneath
it, and a thread reading while another sets needs no lock because
nothing is being written.

The derived configuration is independent - it copies what it needs -
so neither has to outlive the other. That costs a pass over the key
space, which is microseconds for a configuration and buys the caller
one less lifetime to reason about.

Provenance survives. The new value arrives from a source of its own,
above the command line because the user just did it deliberately, and
the value it displaced is still in the chain. explain() afterwards
says "set at run time, and before that it came from the file", which
is the question people actually ask after changing a setting.

```c
CF_NODISCARD CF_API cf_config* cf_derive(const cf_config* base, const char* source_name, const cf_pair* pairs, size_t count, cf_error* err);
```

Implementation: [`cf_derive` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1434).

```c
CF_NODISCARD CF_API cf_config* cf_derive_in(const cf_config* base, cf_arena* arena, const char*    source_name, const cf_pair* pairs, size_t count, cf_error* err);
```

Implementation: [`cf_derive_in` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1456).

The same, inside an arena the caller owns. A configuration built with
cf_build_in has no allocator to borrow, so it must use this one.

### Keeping a change

Deriving changes a snapshot; nothing outlives the process. To keep a
change, write it to a source - which asks three questions first, and
any "no" refuses every item before one is written:

  capability     can that source be written at all? Only a store can
                 (Design.md, section 39).
  permission     did the program grant it (cf_builder_writable)?
  authorization  would the contract accept each value from that
                 source? The read side's own rules decide - type,
                 range, allowed spellings, unknown keys, and a key's
                 policy (D-33): a key that may only be set from the
                 environment is not written to a store, because
                 writing past an operator's lock would lift it. A
                 command (CF_TRANSIENT) is never written.

`meta` - who, when, why - goes with each value, and is what explain()
shows as where it was set once the store is read again. The
configuration does not change: it is a snapshot. Rebuild, or derive,
to see the write.

Declared APPEND lists use final indices here. Setting an existing
element owned by the target translates to its source-local index;
a target without a list count may set or remove final-index patches.
Changing a count, deleting a contributing element, or addressing
another contributor's element is CF_E_CAPABILITY before any write.
Such structural edits need a complete source-local list update, not
an ambiguous change to the aggregate count. REPLACE lists keep their
ordinary index keys. The source's write callback sees local keys.

```c
CF_NODISCARD CF_API cf_bool cf_write(const cf_config* c, const char* target, const cf_write_item* items, size_t count, const char* meta, cf_error* err);
```

Implementation: [`cf_write` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1759).

### Change notification

Pollable, never a callback: a library that owns a thread and calls
you back has made a threading decision that belongs to you.

    if (cf_changed(cfg)) {
        cf_config* next = rebuild();
        cf_config_free(cfg);
        cfg = next;
    }

The old snapshot stays valid while anything holds it, so reloading
changes no contract: cf_get_text's pointer is valid until *that*
configuration is released, watched or not.

What is compared is modification time and size. That is cheap,
portable and pollable, and it misses a file rewritten inside one
clock tick at the same length. Treat a true as worth acting on, not
as a guarantee; the exact answer is to rebuild and compare digests,
which costs the re-read you were about to do anyway.

```c
CF_API cf_bool cf_changed(const cf_config* c);
CF_API size_t cf_watch_count(const cf_config* c);
CF_API const char* cf_watch_path(const cf_config* c, size_t i);
```

Implementation: [`cf_changed` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1836), [`cf_watch_count` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1825), [`cf_watch_path` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1829).

### A handle an event loop can wait on

For a program that already has a loop and would rather wait than
poll. Still not a callback and still no thread: the library hands
over something to wait on, and the waiting is the caller's.

    cf_watcher* w = cf_watcher_open(cfg, NULL, &err);

    add cf_watcher_fd(w) (POSIX) or cf_watcher_handle(w) (Windows)
    to the loop, and when it is ready:

    cf_watcher_drain(w);
    if (cf_changed(cfg)) {
        rebuild and swap, then close w and open one on the new
        configuration, whose files may not be the same ones
    }

With no loop at all, cf_watcher_wait(w, timeout_ms) waits and drains
in one call, on whichever thread the caller gives it.

A wake-up means "ask now", never "it changed". What is watched is
each file's directory - editors save by renaming a new file over the
old one - so a neighbour can wake it, and cf_changed is the answer.
Drain before asking: the other order can consume a wake-up for a
change that arrived after the question.

The watcher copies what it needs, so the configuration it was opened
on may be released first. It is the caller's to use from one thread
at a time; the configuration's own threading contract is unchanged.

Linux (inotify), macOS and the BSDs (kqueue), and Windows. Elsewhere
cf_watcher_open fails with CF_E_CAPABILITY and says so, and
cf_changed works as before.

```c
CF_NODISCARD CF_API cf_watcher* cf_watcher_open(const cf_config* c, const cf_allocator* a, cf_error* err);
```

Implementation: [`cf_watcher_open` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1859).

`a` NULL means the default allocator. Fails when the configuration
read no file, since there would be nothing to wake it.

```c
CF_API int cf_watcher_fd(const cf_watcher* w); /* -1 where it is not a descriptor */
CF_API void* cf_watcher_handle(const cf_watcher* w); /* NULL where it is not a HANDLE */
CF_API cf_bool cf_watcher_drain(cf_watcher* w);
```

Implementation: [`cf_watcher_fd` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1878), [`cf_watcher_handle` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1882), [`cf_watcher_drain` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1886).

```c
CF_API int cf_watcher_wait(cf_watcher* w, long timeout_ms);
CF_API void cf_watcher_close(cf_watcher* w);
```

Implementation: [`cf_watcher_wait` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1890), [`cf_watcher_close` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1894).

1 woken (and drained), 0 timed out, -1 failed. Negative: no limit.

```c
CF_API void cf_config_ref(cf_config* c);
CF_API void cf_config_free(cf_config* c);
```

Implementation: [`cf_config_ref` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1898), [`cf_config_free` (src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/src/core/config.c#L1904).

## cf_alloc.h

cf_alloc.h - allocation interface and arena.

Every allocation in the library passes through a cf_allocator, so a
caller can supply their own, and so the fixed-capacity profile can
refuse to allocate at all rather than growing silently.

Snapshots are immutable and are freed whole, which is exactly what
an arena is for: allocation is a pointer bump and release is one
call, with no per-object bookkeeping anywhere.

```c
CF_API const cf_allocator* cf_default_allocator(void);
```

Implementation: [`cf_default_allocator` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L31).

malloc and free. NULL on targets built with CF_NO_STDLIB.

### Arena

Two flavours. A growing arena takes blocks from an allocator; a
fixed arena works inside a buffer the caller supplies and fails
cleanly when it is exhausted, which is the freestanding profile.

```c
CF_API cf_arena* cf_arena_new(const cf_allocator* a, size_t block_size);
CF_API cf_arena* cf_arena_fixed(void* buffer, size_t size);
```

Implementation: [`cf_arena_new` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L91), [`cf_arena_fixed` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L122).

`block_size` is a hint: anything below 1 KiB is replaced by a sensible
default, because an arena that takes sixty-four bytes at a time is a
malloc wrapper with extra steps. Said out loud because a silently
ignored parameter is how a test ends up measuring the default three
times and reporting it as three results.

```c
CF_API void cf_arena_free(cf_arena* ar);
```

Implementation: [`cf_arena_free` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L204).

Release the arena, zeroing every byte it handed out first. A
configuration's arena holds its values - passwords and tokens among
them - and the text of every file it read, and memory handed back to
an allocator is memory the next allocation, a core dump or a swap
file can show to someone else. A fixed arena is zeroed and not
released: the buffer is the caller's.

```c
CF_API void cf_arena_wipe(cf_arena* ar);
```

Implementation: [`cf_arena_wipe` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L193).

Zero every byte the arena has handed out, without releasing it. For a
caller who owns the arena - cf_build_in - and is done with what lived
in it: nothing allocated from it may be used afterwards.

```c
CF_API const cf_allocator* cf_arena_allocator(const cf_arena* ar);
```

Implementation: [`cf_arena_allocator` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L176).

The allocator this arena takes blocks from, or NULL for a fixed one.
Needed so that deriving a configuration keeps using the allocator the
caller supplied rather than quietly falling back to malloc.

```c
CF_API void* cf_arena_alloc(cf_arena* ar, size_t n);
CF_API void* cf_arena_calloc(cf_arena* ar, size_t n);
```

Implementation: [`cf_arena_alloc` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L227), [`cf_arena_calloc` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L264).

```c
CF_API char* cf_arena_dup(cf_arena* ar, const char* s, size_t len);
```

Implementation: [`cf_arena_dup` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L272).

Copy len bytes and terminate. Stored text is NUL-terminated in
addition to carrying a length, so the convenience accessors are
free and an embedded NUL is detectable by the caller.

```c
CF_API size_t cf_arena_used(const cf_arena* ar);
CF_API size_t cf_arena_capacity(const cf_arena* ar);
CF_API cf_bool cf_arena_is_fixed(const cf_arena* ar);
```

Implementation: [`cf_arena_used` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L289), [`cf_arena_capacity` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L293), [`cf_arena_is_fixed` (src/core/alloc.c)](https://github.com/develooper1994/configforge/blob/main/src/core/alloc.c#L297).

## cf_define.h

cf_define.h - definitions, the typed contract.

A definition is what turns configuration from a bag of strings into
a contract. With one present, the definition decides the type and no
inference happens at all; without one, Tier 0 inference applies and
is deliberately timid.

Definitions are also what make the useful diagnostics possible: typo
detection needs the set of legal keys, required-parameter checking
needs to know what is required, and redaction needs to know what is
sensitive. None of that can be recovered from the values alone.

```c
CF_API void cf_def_init_sized(cf_def* d, size_t size, cf_type type, const char* key);
```

Implementation: [`cf_def_init_sized` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L22).

Initialise a descriptor: every field zero, `size` the caller's
sizeof(cf_def), so a library from a later 1.x writes no further than
the caller's struct reaches (D-51). Call it as cf_def_init, which
passes that size; cf_def_init_sized is for a binding that lays out
cf_def itself.

### Definition set

```c
CF_API cf_bool cf_defset_init(cf_defset* ds, cf_arena* arena, size_t hint);
```

Implementation: [`cf_defset_init` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L36).

```c
CF_API cf_bool cf_define(cf_defset* ds, const cf_def* d, cf_error* err);
```

Implementation: [`cf_define` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L383).

Copies the descriptor. Rejects a definition that contradicts itself:
a default outside its own range, an empty enum, a range on a type
that has none. Those are programming errors and are worth refusing
at the point they are written.

```c
CF_API cf_bool cf_def_bool(cf_defset* ds, const char* key, cf_bool dflt, unsigned flags, cf_error* err);
CF_API cf_bool cf_def_int(cf_defset* ds, const char* key, cf_i64 dflt, cf_i64 min, cf_i64 max, unsigned flags, cf_error* err);
CF_API cf_bool cf_def_text(cf_defset* ds, const char* key, const char* dflt, unsigned flags, cf_error* err);
CF_API cf_bool cf_def_real(cf_defset* ds, const char* key, double dflt, double min, double max, unsigned flags, cf_error* err);
```

Implementation: [`cf_def_bool` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L541), [`cf_def_int` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L553), [`cf_def_text` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L570), [`cf_def_real` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L583).

```c
CF_API void cf_defset_seal(cf_defset* ds);
```

Implementation: [`cf_defset_seal` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L655).

Sorts for binary lookup; calling twice is harmless.

```c
CF_API cf_bool cf_defset_open(cf_defset* ds, const char* prefix, cf_error* err);
```

Implementation: [`cf_defset_open` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L605).

Exempt a prefix from unknown-key rejection.

```c
CF_API cf_bool cf_defset_schema(cf_defset* ds, const char* text, size_t n, const char* name, cf_error* err);
```

Implementation: [`cf_defset_schema` (src/bridge/schema.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/schema.c#L714).

Definitions from a JSON Schema (draft 2020-12 or 2019-09), the reverse
of cf_schema_write: an object's properties are keys, a leaf's type,
range, length, enum, default, deprecated and writeOnly (sensitive) its
definition, "required" CF_REQUIRED, an array of scalars a list, and
additionalProperties true an open prefix. A keyword this library does
not read - $ref, allOf, pattern and the rest - is refused by name and
place rather than dropped: a constraint the program believes in and
the library does not enforce is worse than an error. Read by the JSON
reader, so CF_E_CAPABILITY in a build without CF_WITH_JSON. `name`
is what errors call the text. (1.2)

```c
CF_API cf_bool cf_defset_schema_file(cf_defset* ds, const char* path, cf_error* err);
```

Implementation: [`cf_defset_schema_file` (src/bridge/schema.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/schema.c#L763).

The same, from a file. (1.2)

```c
CF_API cf_bool cf_defset_is_open(const cf_defset* ds, const char* key, size_t keylen);
```

Implementation: [`cf_defset_is_open` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L626).

Is this key under an exempt prefix? Matches at a segment boundary
only, so opening "plugins" does not open "pluginsomething".

```c
CF_API const cf_def* cf_defset_find(const cf_defset* ds, const char* key, size_t keylen);
```

Implementation: [`cf_defset_find` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L718).

Authored descriptors are returned for exact keys. A declared list's
count and canonical decimal indices return immutable scalar
projections; an item projection's key is the list root. Enumeration
still returns only authored descriptors.

```c
CF_API const cf_def* cf_defset_list(const cf_defset* ds, const char* key, size_t keylen);
```

Implementation: [`cf_defset_list` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L747).

The declared list containing a key, including its root. Unlike find,
this also finds a list whose caller supplied an invalid member name,
so the apply pass can refuse that member instead of treating it as
an unrelated, undefined setting.

```c
CF_API cf_u64 cf_defset_fingerprint(const cf_defset* ds, const cf_keyspace* ks);
```

Implementation: [`cf_defset_fingerprint` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L900).

The fingerprint of `ks` under these definitions (cf_fingerprint): as
cf_ks_digest, except that a key defined CF_SENSITIVE enters by its
cf_ks_value_digest text, as a value-blind report shows it, rather
than by its value. `ds` may be NULL: nothing is sensitive.

```c
CF_API cf_u64 cf_fingerprint_seed(void);
CF_API cf_u64 cf_fingerprint_step(cf_u64 h, const char* key, size_t keylen, const char* type, const char* text, size_t n);
CF_API size_t cf_defset_count(const cf_defset* ds);
CF_API const cf_def* cf_defset_at(const cf_defset* ds, size_t i);
```

Implementation: [`cf_fingerprint_seed` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L884), [`cf_fingerprint_step` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L890), [`cf_defset_count` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L752), [`cf_defset_at` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L756).

The fingerprint a step at a time, for a reader that holds keys as text -
Doctor, recomputing a report's - and not as a key space: start from the
seed, then give each selected entry in key order its key, the type name
(cf_type_name) and the value's text, or for a secret the text of its
cf_ks_value_digest. cf_defset_fingerprint is these steps over a key
space, so the two cannot disagree.

```c
CF_API const cf_def* cf_defset_nearest(const cf_defset* ds, const char* key, size_t keylen, size_t* distance);
```

Implementation: [`cf_defset_nearest` (src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/src/core/define.c#L825).

Nearest defined key by edit distance, or NULL when nothing is close.
This is what turns "unknown key server.pot" into "did you mean
server.port", and it is available only because the definition set is
a contract rather than documentation.

### Applying the contract

Run after cf_ks_resolve. Converts every textual candidate to the
type its definition names, validates it, and marks failures as
rejected with a reason rather than discarding them. Re-selects
afterwards, so a rejected winner hands over to the next candidate
down the chain only when that is what the definition asked for.

```c
CF_API void cf_policy_text(const char* const* policy, char* buf, size_t cap);
```

Implementation: [`cf_policy_text` (src/core/validate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/validate.c#L511).

Renders a policy list as the specification writes it: "env > local".
Shared because both the rejection message and the coherence check
below want the same spelling, and two renderings of one thing drift.

```c
CF_API cf_bool cf_defs_apply(cf_keyspace* ks, const cf_defset* ds, const cf_rank_names* names, cf_unknown_policy unknown, cf_apply_report* report, cf_error* err);
```

Implementation: [`cf_defs_apply` (src/core/validate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/validate.c#L953).

```c
CF_API cf_bool cf_source_defaults(struct cf_source* s, const char* name, int rank, const cf_defset* ds);
```

Implementation: [`cf_source_defaults` (src/sources/defaults.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/defaults.c#L66).

Definition defaults, as an ordinary source. Declared here because
it is the one source that reads a definition set; it is implemented
with the others.

A default is a value from the lowest-precedence source rather than a
separate mechanism, which is why explain() shows one losing to a
file with no special case anywhere - and why it has a name to print
rather than a question mark.

```c
CF_API void cf_source_env_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_env_defs` (src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/env.c#L62).

Give the environment source the key list, so it accepts the natural
single-underscore spelling as well as the escaped one. Without a
contract only the escaped spelling can be enumerated unambiguously;
with one, both work and nobody has to type the escape.

```c
CF_API void cf_source_dotenv_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_dotenv_defs` (src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dotenv.c#L154).

Give a .env file the same natural names and list separators.

```c
CF_API void cf_source_file_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_file_defs` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L169).

Give a file, or a directory of them, the list separators, for a format
that carries no types (cf_flatten_lists): a declared list splits an
INI value as it splits the environment's. (1.2)

```c
CF_API void cf_source_cli_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_cli_defs` (src/sources/cli.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/cli.c#L101).

Give the command-line source the key list, which is what unlocks
--key value, --no-key and aliases. Without it only the = form carries
a value, because nothing else can be parsed correctly.

## cf_doctor.h

cf_doctor.h - ConfigForge Doctor's rules, as a library.

The `doctor` program is this library with a command line around it.
A program can run the same rules on itself - at startup, before it
serves, a service can refuse a production configuration whose
password came from the command line - and a test can assert that a
configuration has no findings, without starting a process or writing
a report to disk.

    cf_doctor* d = cf_doctor_new();
    cf_doctor_lint(d, cfg, "production", &err);
    for (i = 0; i &lt; cf_doctor_count(d); ++i) {
        const cf_finding* f = cf_doctor_finding(d, i);
        fprintf(stderr, "%s %s %s: %s\n", f->rule, f->level, f->key, f->message);
    }
    cf_doctor_free(d);

Doctor reads provenance reports and never resolves configuration
itself (Doctor.md): linting a cf_config writes its report in memory
and reads that, so the rules see exactly what `doctor lint` sees.
Findings accumulate across calls on one context until it is freed;
a context is used by one thread at a time.

A separate library, libconfigforge_doctor, static, and not part of
the configuration library: a device that reads configuration does not
carry the rules that audit it.

```c
CF_DOCTOR_API cf_doctor* cf_doctor_new(void);
CF_DOCTOR_API void cf_doctor_free(cf_doctor* d);
```

Implementation: [`cf_doctor_new` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L1962), [`cf_doctor_free` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L1972).

A context, or NULL when there is no memory for one.

```c
CF_DOCTOR_API cf_bool cf_doctor_accept(cf_doctor* d, const char* rule, const char* key);
CF_DOCTOR_API cf_bool cf_doctor_baseline(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_accept` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2016), [`cf_doctor_baseline` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2028).

A finding already accepted - "CFD001", "server.port" - so it is
counted and not reported. cf_doctor_baseline reads them from a file,
one per line, as `doctor --baseline` does; a missing file is an empty
baseline.

```c
CF_DOCTOR_API cf_bool cf_doctor_severity(cf_doctor* d, const char* rule, const char* level, const char* profile, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_severity_file(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_severity` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2036), [`cf_doctor_severity_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2059).

Where a project disagrees with a rule's default: `level` is "error",
"warning", "info" or "off", for every profile or only `profile`.
cf_doctor_severity_file reads a `doctor --severity` file. Neither
takes effect for findings already recorded.

```c
CF_DOCTOR_API cf_bool cf_doctor_lint(cf_doctor* d, const cf_config* c, const char* profile, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_report(cf_doctor* d, const char* json, size_t n, const char* name, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_file(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_lint` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2137), [`cf_doctor_lint_report` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2106), [`cf_doctor_lint_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2122).

Lint one configuration. A cf_config is reported in memory, with every
value, so the type and range rules can run; `profile` is the one the
report states, which some severities depend on, and may be NULL. A
report - text, or a file - is linted as `doctor lint` lints it, with
the profile it carries. False with `err` set when the input cannot be
read; true otherwise, whatever was found.

```c
CF_DOCTOR_API cf_bool cf_doctor_only_changed_file(cf_doctor* d, const char* path, cf_error* err);
CF_DOCTOR_API size_t cf_doctor_unchanged_count(const cf_doctor* d);
```

Implementation: [`cf_doctor_only_changed_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L1986), [`cf_doctor_unchanged_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2012).

`doctor lint --base B --only-changed`: from now on a lint reports only
what a change touched - the keys whose candidates differ from the
report in `path`, or that only one of the two has, and the sources
those came from - so a large configuration can adopt Doctor one change
at a time. The rest are counted (cf_doctor_unchanged_count), not
recorded. A later call replaces the base. (1.2)

```c
CF_DOCTOR_API cf_bool cf_doctor_diff(cf_doctor* d, const cf_config* a, const cf_config* b, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_report(cf_doctor* d, const char* a, size_t an, const char* aname, const char* b, size_t bn, const char* bname, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_files(cf_doctor* d, const char* a, const char* b, cf_error* err);
```

Implementation: [`cf_doctor_diff` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2191), [`cf_doctor_diff_report` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2153), [`cf_doctor_diff_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2173).

Compare two: what one has and the other lacks, values from sources
of different kinds, overrides in one and not the other, and every
value that differs - `doctor diff`.

```c
CF_DOCTOR_API cf_bool cf_doctor_history_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_blame_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);
```

Implementation: [`cf_doctor_history_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2266), [`cf_doctor_blame_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2286).

A sequence of reports, oldest first - one service's report from each
deployment - in the order given; a report's own date is not trusted
to order it. cf_doctor_history_files adds an info finding, rule
"HISTORY", for each change from one report to the next: a key added
or removed, now set or no longer set, a value that changed, a value
that came from another source, and a step that cannot be settled
because one report has only a digest the other cannot match. `key`
limits it to one key, NULL for every key. cf_doctor_blame_files adds
one, rule "BLAME": the report since which `key` has been as the last
report has it, and what changed there, placed where that report says
the value was set. Neither puts a value in a message.
False with `err` set, and nothing found, when a report cannot be
read, `key` is in none of them, or there is no memory to record the
result.

```c
CF_DOCTOR_API size_t cf_doctor_count(const cf_doctor* d);
CF_DOCTOR_API const cf_finding* cf_doctor_finding(cf_doctor* d, size_t i);
```

Implementation: [`cf_doctor_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2310), [`cf_doctor_finding` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2314).

The findings so far, in the order they were found.

```c
CF_DOCTOR_API size_t cf_doctor_count_at(const cf_doctor* d, const char* level);
```

Implementation: [`cf_doctor_count_at` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2329).

How many at a level - "error", "warning", "info" - or "accepted", for
the ones the baseline accepted.

```c
CF_DOCTOR_API size_t cf_doctor_stale_count(const cf_doctor* d);
CF_DOCTOR_API cf_bool cf_doctor_stale(const cf_doctor* d, size_t i, const char** rule, const char** key);
```

Implementation: [`cf_doctor_stale_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2348), [`cf_doctor_stale` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2362).

Baseline entries that matched nothing: a fix worth noticing, or a
rule that quietly stopped working.

### Doctor as the gate a reload passes through

The preflight a service runs at startup, run again on every snapshot
cf_live is about to hand out, so an edit an hour later is refused as
the same mistake would have been at startup - before a reader sees
it, with the running configuration left serving:

    static cf_doctor_gate gate = CF_DOCTOR_GATE_INIT;
    cf_live_options       o    = CF_LIVE_OPTIONS_INIT;
    gate.profile = "production";
    o.mode       = CF_LIVE_AUTO;
    o.build      = build;      o.build_user = &app;
    o.check      = cf_doctor_live_check;
    o.check_user = &gate;
    cf_live* live = cf_live_open_with(&o, &err);

```c
CF_DOCTOR_API cf_bool cf_doctor_live_check(void* user, const cf_config* running, const cf_config* next, cf_error* err);
```

Implementation: [`cf_doctor_live_check` (tools/doctor/gate.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/gate.c#L35).

A cf_live_check_fn: `user` is a cf_doctor_gate, or NULL for the
defaults. Refuses `next` when a finding reaches `fail_on`, the first
such finding in `err` - "CFD007 error db.password: ..." with where it
was set as the error's source. A gate that did not start from
CF_DOCTOR_GATE_INIT - its size is below 1.0's - refuses every
snapshot, saying so.

```c
CF_DOCTOR_API size_t cf_doctor_rule_count(void);
CF_DOCTOR_API const char* cf_doctor_rule_id(size_t i);
CF_DOCTOR_API const char* cf_doctor_rule_text(const char* id);
```

Implementation: [`cf_doctor_rule_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2383), [`cf_doctor_rule_id` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2387), [`cf_doctor_rule_text` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2391).

The rule catalogue (Doctor.md): each rule's id and what it finds.

## cf_edit.h

cf_edit.h - creating, changing and deleting settings, as one change.

Reading is the snapshot's (cf_get_*, explain). Writing goes to a
source that can be written - a store (cf_builder_store) the program
granted with cf_builder_writable - and an edit gathers a change to it
before anything is written:

    cf_edit* e = cf_edit_begin(cfg, "store", &err);
    cf_edit_set_int(e, "pool.size", 32);
    cf_edit_set_text(e, "log.level", "debug");
    cf_edit_unset(e, "feature.beta");            -- the next source down supplies it
    if (!cf_edit_commit(e, "admin, 2026-10-30: incident 42", &err)) { ... }

Commit holds the whole change to the contract - the types, ranges and
spellings, and each key's policy (D-33) - and writes it in one
transaction, its note beside every value, or writes nothing and says
why. The snapshot does not change: rebuild, or under cf_live call
cf_live_commit, which commits and reloads in one step.

An edit is used by one thread. Its strings are copied, so the caller's
may go as soon as each call returns.

```c
CF_NODISCARD CF_API cf_edit* cf_edit_begin(struct cf_config* c, const char* target, cf_error* err);
```

Implementation: [`cf_edit_begin` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L26).

A change to `target`, a source of `c` by name. The edit keeps its own
reference to `c`. NULL, with `err` saying why, when `c` or `target`
is NULL or there is no memory - an edit allocates from the default
allocator, also for a configuration built in an arena. Whether the
source exists, can be written and may be, is asked at commit with the
rest, so every refusal comes from one place.

```c
CF_API cf_bool cf_edit_set_text(cf_edit* e, const char* key, const char* value);
CF_API cf_bool cf_edit_set_int(cf_edit* e, const char* key, cf_i64 value);
CF_API cf_bool cf_edit_set_bool(cf_edit* e, const char* key, cf_bool value);
CF_API cf_bool cf_edit_set_real(cf_edit* e, const char* key, double value);
```

Implementation: [`cf_edit_set_text` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L110), [`cf_edit_set_int` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L130), [`cf_edit_set_bool` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L140), [`cf_edit_set_real` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L151).

Set a key: as text, which the contract types as it types a file's, or
already typed. A key set twice keeps the second. False for no memory
or a key that is not one, which commit then reports.

```c
CF_API cf_bool cf_edit_unset(cf_edit* e, const char* key);
```

Implementation: [`cf_edit_unset` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L162).

Delete a key from the target, so that the next source down - a file,
the defaults - supplies it again.

```c
CF_API size_t cf_edit_count(const cf_edit* e);
```

Implementation: [`cf_edit_count` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L172).

How many keys the edit changes.

```c
CF_NODISCARD CF_API cf_bool cf_edit_commit(cf_edit* e, const char* note, cf_error* err);
```

Implementation: [`cf_edit_commit` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L184).

Check and write the whole change, `note` (who, when, why) beside each
value. Consumes the edit either way.

```c
CF_API void cf_edit_abort(cf_edit* e);
```

Implementation: [`cf_edit_abort` (src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/src/core/edit.c#L176).

Drop the edit; nothing is written.

## cf_emit.h

cf_emit.h - writing a configuration out.

Restricted, on purpose, to a file ConfigForge owns and rewrites in
full. Editing a hand-written file in place means preserving its
comments, its ordering and its whitespace: the parser would have to
keep a concrete syntax tree that every codec then maintains, and
getting it slightly wrong silently destroys something a person wrote.

What "save settings" actually needs is an overrides file, which is
what git config, systemd drop-ins and sudoers.d all are.

```c
CF_API size_t cf_emit(const struct cf_config* c, const char* format, cf_emit_what what, int rank, char* buf, size_t cap, cf_error* err);
```

Implementation: [`cf_emit` (src/bridge/emit.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/emit.c#L239).

Measure-or-write, like everything else that renders: with a NULL
buffer nothing is stored and the length still accumulates.
`rank` is read only for CF_EMIT_FROM_RANK.
Refuses rather than truncates: a format with no array syntax meeting
an array it would have to write is an error before anything is
written, not a surprise halfway through a file. An array that `what`
leaves out is not one it would have to write.
A configuration nested deeper than CF_MAX_DEPTH (cf_build_options.h)
is refused as CF_E_KEY in every format: it is as deep as a reader
takes, and a file past that could not be read back.
A selected key that is both a value and a container is also CF_E_KEY.
Literal ${ in a text value is written as $${ so construction from
the emitted file preserves it, rather than expanding it again.
The lower-level cf_doc_emit does not interpret or escape references.
Whatever the reason, a refusal returns 0 with `err` set, and a buffer
that was given holds an empty string - not the front of a document
that the walk had written before it came to the reason.

## cf_entry.h

cf_entry.h - flat entry producers, and how a source spells a key.

Sources divide by what they have to do with the bytes.

  flat        env, command line, .env files, a registry
              already key-and-value; the work is lexing and naming
  hierarchical  JSON, TOML, XML, INI
              a document; the work is parsing, and the flattener
              turns the events into keys
  direct      memory pairs, defaults, a replayed report
              no bytes to read at all

The middle group already had a seam: a parser is an event producer
and never touches a key space. The first group did not, and each of
its sources reached into the key space itself. That made every lexer
need an arena and a key space to be tested at all, and it put the
decision "what does a candidate look like" in three files.

A lexer now produces entries into a sink. Recording them into a key
space is one implementation of that sink; a test's implementation is
an array.

### Key mapping

Each flat source spells a key its own way, and the spelling is a
separate concern from the lexing. Keeping them apart is what makes
the direction that matters possible: given a definition, a source
can be asked what *it* would call that key, which is how the
environment gains its natural single-underscore spelling once a
contract exists.

```c
CF_API void cf_keymap_init(cf_keymap* m, cf_keymap_kind kind, const char* prefix);
```

Implementation: [`cf_keymap_init` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L172).

```c
CF_API size_t cf_keymap_to_key(const cf_keymap* m, const char* raw, size_t n, char* out, size_t cap);
```

Implementation: [`cf_keymap_to_key` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L180).

Source spelling to canonical. Returns the length written, or 0 when
the name does not belong to this source or does not derive a valid
key. Skipped rather than repaired: PATH and HOME land here.

```c
CF_API size_t cf_keymap_from_key(const cf_keymap* m, const char* key, size_t n, cf_bool escaped, char* out, size_t cap);
```

Implementation: [`cf_keymap_from_key` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L221).

Canonical to source spelling. `escaped` selects between the form the
mapping can read back unaided and the natural one a person writes:

  server.tls_min_version
    escaped -> MYAPP_SERVER__TLS_MIN_VERSION   enumerable
    natural -> MYAPP_SERVER_TLS_MIN_VERSION    ambiguous alone,
                                               unambiguous with a
                                               definition to match
Both are accepted when a contract exists, which is the whole gain:
the escape is what enumeration needs, and nobody has to type it.

### Entry sink

```c
CF_API cf_bool cf_keysink_init(cf_keysink* k, cf_keyspace* ks, int rank);
CF_API const cf_entry_sink* cf_keysink_of(cf_keysink* k);
```

Implementation: [`cf_keysink_init` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L293), [`cf_keysink_of` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L307).

```c
CF_API cf_bool cf_listsink_init(cf_listsink* out, cf_keyspace* ks, int rank, const struct cf_defset* defs, cf_bool repeated);
CF_API const cf_entry_sink* cf_listsink_of(cf_listsink* out);
CF_API cf_bool cf_listsink_finish(cf_listsink* out);
```

Implementation: [`cf_listsink_init` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L133), [`cf_listsink_of` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L150), [`cf_listsink_finish` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L154).

### Lexers

No arena, no key space, no source object. Everything these need is
an argument, which is what makes them testable against an array.

```c
CF_API cf_bool cf_lex_env(char* const* environment, const cf_keymap* map, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_env` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L315).

Entries that do not belong to this prefix, or do not derive a valid
key, are skipped. An environment is shared with every other program
on the machine, so refusing it would be refusing the machine.

### Command line

--key=value        everywhere
  --key              true, when the key is a flag or undeclared
  --key value        when a declaration says the key takes one
  --no-key           false, when the key is a declared flag
  -k value           when a declaration names the alias
  --                 stop

The forms below the first two need to know whether an option takes a
value, which is the one thing a command line cannot be parsed
correctly without. A parser that guesses eats positional arguments,
and getopt takes a spec for exactly this reason.

So the lexer asks. Tier 0 passes no spec and gets the two forms that
need no knowledge; Tier 1 passes one built from the definitions and
gets the rest. Same shape as the environment's two spellings: a
contract buys capability, and its absence buys predictability rather
than guesswork.

```c
CF_API cf_bool cf_lex_cli(int argc, char* const* argv, const cf_keymap* map, const cf_cli_spec* spec, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_cli` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L381).

`spec` may be NULL, which is Tier 0.

A malformed option is rejected rather than skipped. A command line
belongs to this program alone, so an unparseable option is the user's
typo, and ignoring it is how a misspelled flag becomes an hour of
confusion.

### Response files

myapp @args.txt --port=8080

One argument per line, `#` comments, blank lines skipped. Accepted
because the Windows command-line length limit is real and this is the
universal way around it.

Expansion is a separate pass rather than something the lexer does, so
the lexer keeps needing nothing but its arguments. Nesting is
refused: a response file that includes another is a command line
nobody can follow from the one they typed, and one that includes
itself is a loop.

```c
CF_API cf_bool cf_cli_expand(int argc, char* const* argv, cf_arena* arena, int* out_argc, char*** out_argv, cf_error* err);
```

Implementation: [`cf_cli_expand` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L534).

### The command line in UTF-8

The real portability problem with a command line is not syntax. On
Windows `main`'s argv arrives in the active ANSI code page, so a
value containing any non-ASCII character is wrong before any parser
sees it - and what the code page dropped cannot be converted back,
which is why the Windows path reads GetCommandLineW rather than
converting argv.

Identity everywhere else, so a caller writes the same two lines on
every platform and only one of them needed them.

```c
CF_API char* const* cf_cli_args(int argc, char* const* argv, cf_arena* arena, int* out_argc);
```

Implementation: [`cf_cli_args` (src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/compat.c#L25).

### A .env file

# comment
  KEY=value
  export KEY=value        the shell convention, accepted and ignored
  KEY="quoted value"

The same shape as the environment, which is the point: a .env file is
an environment written down. It takes the same key mapping, so the
escape and the natural spelling behave identically in both.

```c
CF_API cf_bool cf_lex_dotenv(const char* text, size_t n, const char* origin, const cf_keymap* map, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_dotenv` (src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/entry.c#L645).

## cf_error.h

cf_error.h - the error model.

Errors are concentrated at construction rather than spread across
every read. Once a configuration exists, reading is infallible for
any key with a default or a definition, so almost everything here is
a construction-time condition.

The error is a caller-supplied struct. Not a thread-local
last-error, because global error state contradicts a stated
principle; not an allocation, because the failure path has to work
in the fixed-capacity profile, which is exactly where allocation
fails.

```c
CF_API void cf_error_clear(cf_error* e);
```

Implementation: [`cf_error_clear` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L33).

Empties `e`: code CF_OK, key, source and message empty. `size` is
kept, so a cleared error can be reused. NULL is ignored.

```c
CF_API void cf_error_copy(cf_error* dst, const cf_error* src);
```

Implementation: [`cf_error_copy` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L57).

What `src` says, copied into `dst` as far as `dst` has room: 1.0's
fields always, a later field where dst->size covers it. dst->size is
kept. Safe with NULL on either side.

```c
CF_API cf_bool cf_error_set(cf_error* e, cf_code code, const char* key, const char* source, const char* fmt, ...);
```

Implementation: [`cf_error_set` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L87).

Set code, key, source and a formatted message. Safe with a NULL
error, so every call site can report unconditionally instead of
guarding. Returns CF_FALSE so a caller can write
`return cf_error_set(err, ...);`

```c
CF_API const char* cf_code_name(cf_code c);
CF_API const char* cf_code_description(cf_code c);
```

Implementation: [`cf_code_name` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L11), [`cf_code_description` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L22).

## cf_explain.h

cf_explain.h - the feature the library exists for.

explain() is not a debugging extra bolted onto a resolver. It is the
provenance chain printed, and the chain is what get() reads. That
ordering is the reason this works at all: a resolver that selected a
winner and discarded the rest could not explain itself afterwards,
and retrofitting the chain would mean rewriting the resolver.

```c
CF_API size_t cf_ks_explain(const cf_keyspace* ks, const cf_source_view* sv, const char* key, size_t keylen, char* buf, size_t cap);
```

Implementation: [`cf_ks_explain` (src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/explain.c#L148).

The low-level renderers. The public API in configforge.h wraps
these; these take a key space and a source list directly, which is
what the tests and the Doctor tool need.

```c
CF_API size_t cf_ks_inspect(const cf_keyspace* ks, const cf_source_view* sv, char* buf, size_t cap);
```

Implementation: [`cf_ks_inspect` (src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/explain.c#L153).

Render every key. Sources that never win appear in the trailing
summary, which is what makes a dead source visible.

```c
CF_API size_t cf_ks_explain_redacted(const cf_keyspace* ks, const cf_source_view* sv, const cf_defset* ds, const char* key, size_t keylen, char* buf, size_t cap);
CF_API size_t cf_ks_inspect_redacted(const cf_keyspace* ks, const cf_source_view* sv, const cf_defset* ds, char* buf, size_t cap);
```

Implementation: [`cf_ks_explain_redacted` (src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/explain.c#L158), [`cf_ks_inspect_redacted` (src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/explain.c#L181).

The same two, with every value of a key that `ds` marks CF_SENSITIVE
shown as "&lt;sensitive>", and as "&lt;sensitive, N bytes>" when it is text:
what a blind report gives, and nothing more. The chain - which source
won, which lost, and where each was set - is shown in full, because
that is the question and it is not the secret.

cf_explain and cf_inspect pass the configuration's own definitions. A
diagnostic is where a secret is most often pasted: into a ticket, a
chat, a log somebody else reads. The two above are the raw renderers
for a key space with no definitions.

## cf_flatten.h

cf_flatten.h - document events to and from the flat key space.

INTERNAL (D-51): exported for ConfigForge Doctor, the bindings and the
tests, and outside the 1.x promise - a minor release may change it.
A program uses the configforge.h functions instead.

The bridge between the serialization layer and the configuration
model. It lives on this side of the boundary on purpose: the
serialization layer may use the leaf types and may not touch a key
space, which is what keeps extracting it later a directory move.

```c
CF_API cf_bool cf_flatten_init(cf_flatten* f, cf_keyspace* ks, int rank);
CF_API const cf_doc_sink* cf_flatten_sink(cf_flatten* f);
```

Implementation: [`cf_flatten_init` (src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/flatten.c#L314), [`cf_flatten_sink` (src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/flatten.c#L334).

```c
CF_API void cf_flatten_lists(cf_flatten* f, const struct cf_defset* ds);
```

Implementation: [`cf_flatten_lists` (src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/flatten.c#L338).

For a format that carries no types - INI, .properties - whose values
the definitions decide: a declared list with a separator splits its
text value, `ports = 80,443` the elements 80 and 443. (1.2)

```c
CF_API cf_bool cf_source_read_blob(const struct cf_source* s, cf_keyspace* ks, const cf_codec* codec, const cf_blob* blob, const char* path, cf_error* err);
```

Implementation: [`cf_source_read_blob` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L29).

A document's bytes through a codec into the key space, at the source's
rank, the source's definitions deciding lists for a format without
types: what the file and directory sources do with each file, for a
source of one's own that has bytes to read. False with `err` set.

```c
CF_API cf_bool cf_doc_emit(const cf_keyspace* ks, const cf_doc_sink* out, cf_error* err);
```

Implementation: [`cf_doc_emit` (src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/flatten.c#L751).

Walk a resolved key space, emitting the document it describes.
Structure is reconstructed from the sorted keys with nothing built:
the keys under one prefix are a contiguous run, so the sort is
already the tree.

A group of keys is an array when it is exactly the one a reader
records: the indexes 0 to n - 1 as a reader writes them, none missing,
and a `count` of n, and nothing else. Any other group is a table, with
its indexes and `count` as members, so that what is written reads back
as the same keys; a table with a member called `count` is a table. The
members of an array come out in index order, whatever they are, and
carry no name; in a table the scalars come first, then the arrays,
then the tables, because a format that has written a sub-table's
header cannot go back to its parent. A selected key that is both a
scalar and a container prefix is refused as CF_E_KEY, naming the key,
rather than written as two members with the same name.

```c
CF_API cf_bool cf_doc_emit_where(const cf_keyspace* ks, const cf_doc_sink* out, cf_emit_pred pred, const void* ctx, cf_error* err);
```

Implementation: [`cf_doc_emit_where` (src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/flatten.c#L755).

## cf_image.h

cf_image.h - the binary snapshot image.

The one thing a binary form does that no text format can: load
without parsing and without allocating. That is what a target with
no filesystem, no heap and a boot-time budget needs, and it is the
artifact the freestanding profile was missing - build-time
configuration was described as a source and had nothing to be.

Not a codec. The key space is already a sorted array plus a string
arena, so the image is a picture of that rather than a document, and
routing it through the document event model would mean inventing a
shape that happens to match the layout. See decision D-28.

    magic     "CFGB", major, minor
    layout    header | key index | values | strings
    order     little endian, always
    integrity the determinism digest, checked on load

Optionally the whole candidate chain travels with it, so a device
with no filesystem can still answer why a value is what it is.

```c
CF_API size_t cf_image_write(const cf_keyspace* ks, const cf_source_view* sv, unsigned flags, char* buf, size_t cap);
```

Implementation: [`cf_image_write` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L203).

Write the image. Measure-or-write, like everything else that renders:
with a NULL buffer nothing is stored and the length still
accumulates.

### Reading

The image is used in place. Nothing is copied, nothing is allocated,
and the buffer must outlive the view - which is exactly the point on
a target where the buffer is a `const char[]` in flash.

```c
CF_NODISCARD CF_API cf_bool cf_image_open(cf_image* img, const char* buf, size_t n, cf_error* err);
```

Implementation: [`cf_image_open` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L364).

Validates the header, the bounds and the digest. Refuses rather than
guesses: a truncated image and a corrupt one look the same from
inside, and neither should produce a configuration.

```c
CF_API size_t cf_image_count(const cf_image* img);
CF_API cf_bool cf_image_at(const cf_image* img, size_t i, const char** key, size_t* klen, cf_value* v);
CF_API cf_bool cf_image_get(const cf_image* img, const char* key, size_t klen, cf_value* v);
```

Implementation: [`cf_image_count` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L448), [`cf_image_at` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L488), [`cf_image_get` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L523).

```c
CF_API cf_bool cf_image_load(const cf_image* img, cf_keyspace* ks, int rank, cf_error* err);
```

Implementation: [`cf_image_load` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L598).

Load an image into a key space at a rank, which is what a source
does. Text values point into the image, so it must outlive the
configuration.

### As a source

```c
CF_API size_t cf_source_image_state_size(void);
```

Implementation: [`cf_source_image_state_size` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L724).

```c
CF_API cf_bool cf_source_image_mem(cf_source* s, const char* name, int rank, const char* buf, size_t n, void* state, size_t state_size);
```

Implementation: [`cf_source_image_mem` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L744).

From memory: a build-time image linked into the program. No file, no
parser, no allocation - the freestanding path.

```c
CF_API cf_bool cf_source_image_file(cf_source* s, const char* name, int rank, const char* path, void* state, size_t state_size);
```

Implementation: [`cf_source_image_file` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L757).

From a file, for hosted targets.

```c
CF_API const cf_error* cf_source_image_error(const cf_source* s);
```

Implementation: [`cf_source_image_error` (src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/image.c#L766).

## cf_iterate.h

cf_iterate.h - walking a prefix.

The questions a program actually asks of a configuration are rarely
"what is server.port", which a plain get answers. They are "what are
the elements of this array" and "which plugins are configured", and
both needed the caller to build keys by hand and guess where to stop.

Both are a scan from one binary search, because the key space is
sorted. Nothing is allocated: the iterator is the caller's.

```c
CF_API cf_bool cf_iter_keys(cf_iter* it, const struct cf_config* c, const char* prefix);
```

Implementation: [`cf_iter_keys` (src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/iterate.c#L58).

Every key under `prefix`, in key order. That is byte order, not the
order a document had it: under an array "upstream.10" comes before
"upstream.2", so an array is read with cf_array_count and cf_array_key
below. A prefix matches at a segment boundary only, so "server" does
not match "servername" - the bug every hand-written prefix check has.
An empty or NULL prefix walks the whole configuration.

```c
CF_API cf_bool cf_iter_children(cf_iter* it, const struct cf_config* c, const char* prefix);
```

Implementation: [`cf_iter_children` (src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/iterate.c#L62).

Each distinct immediate child of `prefix`, once, in key order:

    plugins.auth.enabled     -> auth
    plugins.auth.timeout
    plugins.cache.size       -> cache

This is how a program discovers what is configured rather than being
told in advance, which is what plugin and per-tenant configuration
both need.

```c
CF_API cf_bool cf_iter_next(cf_iter* it);
```

Implementation: [`cf_iter_next` (src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/iterate.c#L66).

### Arrays

```c
CF_API size_t cf_array_count(const struct cf_config* c, const char* key);
```

Implementation: [`cf_array_count` (src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/iterate.c#L120).

The recorded length, which is not the same as the number of index
keys present: an element below it may be missing. The length is also
where the array ends - the source that wins "&lt;key>.count" fixes it,
and an element at or past it, from any source, is not in the
configuration: it is not read, listed or written, and explain() shows
it as a "truncated" candidate. A count that is not an integer, or is
negative, fixes nothing, and nor does one that is a template.

```c
CF_API size_t cf_array_key(char* buf, size_t cap, const char* key, size_t index);
```

Implementation: [`cf_array_key` (src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/src/core/iterate.c#L141).

Builds "&lt;key>.&lt;index>", so a loop does not have to. Measure-or-write
like everything else that renders.

## cf_keyspace.h

cf_keyspace.h - the flat key space and the provenance chain.

The 1.x promise (D-51) covers what a source written by a program uses:
cf_key_valid, cf_ks_arena and the cf_ks_add* functions, called from a
source's load. The cf_keyspace struct's members, and every other
cf_ks_ function, are INTERNAL - exported for ConfigForge Doctor and the
tests, and free to change in a minor release.

This is the core data structure, and the ordering of the design
depends on it: the chain is not a diagnostic that was added later,
it is what get() reads. A resolver that selected a winner and threw
the rest away could not explain itself afterwards.

### Keys

Canonical form is dotted, lowercase ASCII, and at most CF_KEY_MAX - 1
bytes, so that a key and its terminator fit the buffers sized by the
limit. Comparison is byte-wise and case-sensitive: case-insensitive
keys are Viper's most-reported defect, because they silently collapse
two distinct parameters. Derivation to MYAPP_SERVER_PORT and
--server-port belongs to the source that needs it, not here.

```c
CF_API cf_bool cf_key_valid(const char* key, size_t len);
```

Implementation: [`cf_key_valid` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L13).

```c
CF_API size_t cf_key_segments(const char* key, size_t len);
```

Implementation: [`cf_key_segments` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L46).

Number of dot-separated segments, 0 when the key is invalid.

```c
CF_API int cf_key_cmp(const char* a, size_t alen, const char* b, size_t blen);
```

Implementation: [`cf_key_cmp` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L60).

Byte-wise, never strcoll: a reproducible snapshot hash depends on a
reproducible order.

### Provenance

```c
CF_API const char* cf_outcome_name(cf_outcome o);
```

Implementation: [`cf_outcome_name` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L64).

```c
CF_API cf_bool cf_outcome_can_win(cf_outcome o);
```

Implementation: [`cf_outcome_can_win` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L75).

Whether a candidate with this outcome may still be selected. The
resolver asks this instead of listing the losing outcomes, so a new
one cannot be added and forgotten in the one place it matters.

### Key space

```c
CF_API cf_bool cf_ks_init(cf_keyspace* ks, cf_arena* arena, size_t hint);
```

Implementation: [`cf_ks_init` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L97).

```c
CF_API cf_arena* cf_ks_arena(const cf_keyspace* ks);
```

Implementation: [`cf_ks_arena` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L122).

The arena everything in this space lives in. Sources that have to
read a document into memory borrow it, so a configuration remains
one arena and one release.

```c
CF_API cf_bool cf_ks_add(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L196).

Record a candidate. Keys and text are copied into the arena, so the
caller's buffers need not outlive the call. Candidates may arrive in
any order; the chain is kept ordered by precedence.

```c
CF_API cf_bool cf_ks_add_text(cf_keyspace* ks, const char* key, size_t keylen, int source, const char* text, size_t textlen, const char* at);
```

Implementation: [`cf_ks_add_text` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L201).

Record a candidate as written, leaving its type to be decided.

```c
CF_API cf_bool cf_ks_add_authored(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add_authored` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L213).

Record a typed candidate that somebody wrote - a JSON string, say -
so the reserved syntax applies to it as it does to raw text.

```c
CF_API cf_bool cf_ks_add_resolved(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add_resolved` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L328).

```c
CF_API cf_bool cf_ks_add_default(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value);
```

Implementation: [`cf_ks_add_default` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L219).

Record a definition's default (CF_CAND_DEFAULT).

```c
CF_API cf_bool cf_ks_rename(cf_keyspace* ks, const char* from, size_t fromlen, const char* to, size_t tolen);
CF_API cf_bool cf_ks_rename_list(cf_keyspace* ks, const char* from, size_t fromlen, const char* to, size_t tolen);
```

Implementation: [`cf_ks_rename` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L297), [`cf_ks_rename_list` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L302).

Before resolution: every candidate of `from` offered to `to` as well,
CF_CAND_RENAMED. The old key keeps its own, for code that still reads
it. False when there is no room.

```c
CF_API cf_bool cf_ks_add_candidate(cf_keyspace* ks, const char* key, size_t keylen, const cf_candidate* c);
```

Implementation: [`cf_ks_add_candidate` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L307).

Record a copy of another key space's candidate, at its rank, with what
it was - textual, a default, the template it was made from - and not
what it was waiting for: it is not read for references again.

```c
CF_API cf_bool cf_ks_replay(cf_keyspace* ks, const cf_keyspace* base);
```

Implementation: [`cf_ks_replay` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L368).

Internal snapshot replay, in original insertion order and at the
source-local keys of appended lists. Derived counts are not replayed.

```c
CF_API cf_bool cf_ks_append_list(cf_keyspace* ks, const char* key, size_t keylen, const cf_list_part* parts, size_t count, const cf_candidate* result, cf_error* err);
```

Implementation: [`cf_ks_append_list` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L392).

Internal: plans are collected before this changes the key index.
Resolve once after applying all plans.

```c
CF_API const char* cf_ks_original_key(const cf_keyspace* ks, const cf_candidate* c);
```

Implementation: [`cf_ks_original_key` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L340).

The source-local key before remapping, or NULL if it never moved.
c must be an owned candidate of this space, not a copied descriptor.

```c
CF_API cf_bool cf_ks_reject(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at, const char* reason);
```

Implementation: [`cf_ks_reject` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L465).

Record a candidate that failed conversion or validation. It never
wins, and it is never erased: explain() has to be able to say that
the environment did supply a value and why it was refused.

```c
CF_API cf_bool cf_ks_resolve(cf_keyspace* ks);
```

Implementation: [`cf_ks_resolve` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L569).

Sorts and groups the candidates. Returns false when there was no room
to do it, which is not the same as an empty configuration: the caller
must distinguish, or a target out of memory gets a snapshot with no
keys and nothing to say why.

```c
CF_API size_t cf_ks_candidate_count(const cf_keyspace* ks);
```

Implementation: [`cf_ks_candidate_count` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L565).

Candidates appended so far, which is what an out-of-room message
should name: the caller sized a buffer, not a key count.

```c
CF_API void cf_ks_infer_untyped(cf_keyspace* ks);
```

Implementation: [`cf_ks_infer_untyped` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L654).

Tier 0 typing. Every textual candidate that no definition claimed
gets a type by inference, which is deliberately timid: see D-24.

This is a separate pass rather than something the sources do,
because a source cannot know whether a definition exists for the key
it just read. Running it before the definitions were applied would
mean inferring first and overriding afterwards, which is how a
declared type ends up losing to a guess.

```c
CF_API cf_entry* cf_ks_at_mut(cf_keyspace* ks, size_t i);
```

Implementation: [`cf_ks_at_mut` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L675).

Mutable access, for the passes that legitimately change outcomes:
applying definitions marks candidates rejected. Casting away const
at the call site would work and would also hide which passes write,
so the write path is named instead.

```c
CF_API void cf_ks_reselect(cf_keyspace* ks);
```

Implementation: [`cf_ks_reselect` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L682).

Recompute the selected candidate of every entry. Needed after
anything that changes an outcome, which in practice means after the
definitions have been applied and some winners have been refused.

```c
CF_API const cf_entry* cf_ks_find(const cf_keyspace* ks, const char* key, size_t keylen);
```

Implementation: [`cf_ks_find` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L743).

Binary search over the resolved entries. NULL when absent, and NULL
before cf_ks_resolve: a half-built space has no answer to give, and
returning a provisional one would be worse than returning none.

```c
CF_API size_t cf_ks_lower_bound(const cf_keyspace* ks, const char* key, size_t keylen);
```

Implementation: [`cf_ks_lower_bound` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L723).

Index of the first key that is not less than `key`, which is where a
prefix scan starts. Returns cf_ks_count when every key sorts before
it. The space is sorted, so this is a binary search and a prefix scan
costs O(log n) plus the matches rather than a walk.

```c
CF_API const cf_value* cf_ks_get(const cf_keyspace* ks, const char* key, size_t keylen);
```

Implementation: [`cf_ks_get` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L766).

The selected value, or NULL when the key is absent or every
candidate was rejected.

```c
CF_API size_t cf_ks_count(const cf_keyspace* ks);
CF_API const cf_entry* cf_ks_at(const cf_keyspace* ks, size_t i);
```

Implementation: [`cf_ks_count` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L774), [`cf_ks_at` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L778).

```c
CF_API cf_bool cf_ks_is_sorted(const cf_keyspace* ks);
```

Implementation: [`cf_ks_is_sorted` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L708).

Verifies the ordering invariant the design depends on.

```c
CF_API cf_u64 cf_ks_digest(const cf_keyspace* ks);
```

Implementation: [`cf_ks_digest` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L841).

Order-independent digest over the resolved space. Two runs that
produce the same configuration produce the same digest, on every
platform and in both portability modes. This is the determinism
requirement made checkable.

```c
CF_API cf_u64 cf_ks_value_digest(const cf_value* v);
```

Implementation: [`cf_ks_value_digest` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L808).

FNV-1a over one value's canonical text: what a value-blind report
says of a secret, so that "did it change" has an answer and "what is
it" has none. Not cryptographic, and not presented as one.

```c
CF_API size_t cf_ks_digest_text(cf_u64 h, char* buf, size_t cap);
```

Implementation: [`cf_ks_digest_text` (src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/src/core/keyspace.c#L823).

A digest as a report writes it, "fnv1a:" and sixteen hex digits, in
`buf` as far as `cap` allows; the length it needs, 22.

## cf_live.h

cf_live.h - a configuration that reloads, under a mode the program
chooses.

cf_changed and cf_watcher say a file moved on; what to do then is a
rebuild, a check and a swap that readers on other threads must not
see half done. cf_live is that, once, so every program does not write
it again:

    static cf_builder* build(void* user) {
        cf_builder* b = cf_app_builder("myapp", 0, NULL);
        cf_builder_int(b, "server.port", 8080, 1, 65535, CF_RESTART);
        cf_builder_text(b, "log.level", "info", 0);
        return b;
    }

    cf_live* live = cf_live_open(CF_LIVE_AUTO, build, NULL, NULL, NULL, &err);

    on any thread, for each request:
        cf_config* cfg = cf_live_get(live);
        ... read cfg ...
        cf_config_free(cfg);

    on one thread, in the event loop or a timer:
        cf_live_poll(live, &err);

Three modes:

  CF_LIVE_MANUAL   nothing reloads until cf_live_reload is called -
                   an admin command, a SIGHUP.
  CF_LIVE_APPROVE  a change is noticed and built, then waits:
                   cf_live_pending shows it, cf_live_approve swaps it
                   in, cf_live_reject drops it. An operator, or a
                   Doctor preflight in `check`, decides.
  CF_LIVE_AUTO     a change is noticed, built, checked and swapped.

Still no thread and no callback from a thread of the library's: the
caller's loop calls cf_live_poll, or cf_live_wait where it has none.
A build or a check that fails leaves the running snapshot in place,
and the error says why - a reload never makes a program worse.

A key defined CF_RESTART cannot change while the program runs: the
port it listens on, the size of a pool it allocated. Its new value
does not reach readers; the running one is kept as the top source,
named "running", so explain() shows both - the file's new value
shadowed, the running one selected - and cf_live_restarts lists the
keys waiting for a restart.
A declared scalar list is held as a whole, including its count, when
any member changes. Its resolved values are not appended or expanded
again. The held members remain pinned in derived views of that
snapshot; a fresh build after restart reads the new list normally.

Probation: a swap a program cannot vouch for until it has run on it -
a pool that connects, a route that answers. With `probation` set in
cf_live_options, the snapshot a swap replaced is kept, and for that
many polls the new one is on probation: cf_live_rollback returns
readers to the kept one, a `health` function asked at each of those
polls does it by answering CF_FALSE, and cf_live_confirm, or the last
of the polls, releases it. The library has no clock - C89 has none
worth the name - so the program's own poll interval is the unit: ten
polls a second apart is ten seconds. A swap during probation keeps the
snapshot that was last confirmed, not the one under suspicion, and
starts the count again. The files that were rolled back from still say
what they said, so the values they build to are not swapped in again:
a poll rebuilds them, finds the rejected values and stays where it is,
until a file says something else. cf_live_reload is asked for, so it is
a retry and may swap them in again.

cf_live_get may be called from any thread, at any time - in a build
with threads: a CF_SINGLE_THREADED build (cf_has_feature reports
"single-threaded") has one. The others belong to one thread at a
time, the one that drives the reloads.

```c
CF_NODISCARD CF_API cf_live* cf_live_open_with(const cf_live_options* o, cf_error* err);
```

Implementation: [`cf_live_open_with` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L578).

Builds and checks the first snapshot, and fails as that does. A size
below 1.0's is refused.

```c
CF_NODISCARD CF_API cf_live* cf_live_open(cf_live_mode mode, cf_live_build_fn build, cf_live_check_fn check, void* user, const cf_allocator* a, cf_error* err);
CF_API void cf_live_close(cf_live* l);
```

Implementation: [`cf_live_open` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L545), [`cf_live_close` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L652).

The shorthand: one `user` for both functions. `check` may be NULL; `a`
NULL is the default allocator.

```c
CF_API struct cf_config* cf_live_get(cf_live* l);
```

Implementation: [`cf_live_get` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L665).

The current snapshot, with a reference the caller releases with
cf_config_free. Any thread. A reader keeps the snapshot it got for as
long as it holds it, so one request sees one configuration.

```c
CF_API cf_live_result cf_live_poll(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_poll` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L678).

Under APPROVE and AUTO: if a file of the running snapshot (or of the
one pending) has changed, rebuild. MANUAL: CF_LIVE_UNCHANGED.

```c
CF_API cf_live_result cf_live_reload(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_reload` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L717).

Rebuild now, in any mode, as if a file had changed: SWAPPED, PENDING
under APPROVE, or UNCHANGED when the rebuild found the same values
(the file stamps are refreshed, and no new generation counted).

```c
CF_API cf_live_result cf_live_wait(cf_live* l, long timeout_ms, cf_error* err);
```

Implementation: [`cf_live_wait` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L728).

cf_live_poll after waiting up to `timeout_ms` for a file to change,
on a watcher cf_live keeps open on the running snapshot's files. For
a program with no loop of its own, on a thread it gives to this.
Where the platform has no watcher, or the snapshot read no file, it
polls at once: C89 has no portable way to sleep. Under MANUAL there
is nothing to wait for and it returns UNCHANGED at once, so a thread
looping on it spins: use cf_live_reload there.

```c
CF_NODISCARD CF_API cf_live_result cf_live_commit(cf_live* l, struct cf_edit* e, const char* note, cf_error* err);
```

Implementation: [`cf_live_commit` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L786).

Commit `e` (cf_edit.h) and reload, in one step: the change an
administrator made reaches readers the way an edit to a file does,
through the build, the check and the mode - under APPROVE it waits to
be approved. CF_LIVE_FAILED, with `err` saying why, when the commit
was refused, and nothing was written: the log has an "edit-refused"
event, and the status is not touched, since nothing was reloaded. An
edit that commits and then fails the build or the check is a failed
reload like any other. Consumes the edit.

```c
CF_API const struct cf_config* cf_live_pending(const cf_live* l);
CF_API cf_live_result cf_live_approve(cf_live* l, cf_error* err);
CF_API void cf_live_reject(cf_live* l);
```

Implementation: [`cf_live_pending` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L735), [`cf_live_approve` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L739), [`cf_live_reject` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L754).

The snapshot waiting for approval, or NULL; it stays the cf_live's.

```c
CF_API cf_live_result cf_live_rollback(cf_live* l, const char* reason, cf_error* err);
```

Implementation: [`cf_live_rollback` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L764).

Readers get the snapshot the probationary swap replaced, and the
probation ends: CF_LIVE_ROLLED_BACK, the generation one more, a
"reload-rolled-back" event with `reason`. CF_LIVE_FAILED, with `err`
saying so and nothing else touched, when no swap is on probation.
`reason` may be NULL. (1.1)

```c
CF_API cf_bool cf_live_confirm(cf_live* l);
```

Implementation: [`cf_live_confirm` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L774).

Ends the probation: the snapshot readers have is the one to keep, and
the one before it is released. False when none was on probation. A
"reload-confirmed" event. (1.1)

```c
CF_API cf_u64 cf_live_generation(cf_live* l);
```

Implementation: [`cf_live_generation` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L835).

How many snapshots readers have been given: 1 after open, one more a
swap. A reader can tell that what it holds is no longer current.

```c
CF_API size_t cf_live_restarts(const cf_live* l, char* buf, size_t cap);
```

Implementation: [`cf_live_restarts` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L847).

The CF_RESTART keys whose new value waits for a restart, one a line,
in `buf` as far as `cap` allows; the length it needs, 0 for none.

```c
CF_API cf_bool cf_live_status_get(cf_live* l, cf_live_status* out);
```

Implementation: [`cf_live_status_get` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L802).

Copies the status into `out`, whose `size` the caller sets, as far as
it reaches (D-51): a caller built against a later, larger struct gets
what this library has, one built against an earlier gets what it
asked for. False for a NULL, or a size smaller than 1.0's.

```c
CF_API cf_bool cf_live_last_error(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_last_error` (src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/src/core/live.c#L817).

The last failure's error, copied into `err` (cf_error_copy); false,
and `err` cleared, when no reload has failed. Any thread.

## cf_log.h

cf_log.h - what the library has to say that is not an error.

An error stops a configuration and comes back in a cf_error. Some
things deserve a line in the program's log without stopping anything:
a key nobody defined, under CF_UNKNOWN_WARN; a deprecated key still in
use; a value the contract refused and fell back from; each reload, and
the keys waiting for a restart. Each is an event with a level, a
stable name a log pipeline can match on, and the key and source it is
about, handed to a function the program gives:

    static void to_syslog(void* user, const cf_log_event* e) {
        syslog(e->level >= CF_LOG_WARN ? LOG_WARNING : LOG_INFO,
               "configforge %s %s: %s", e->event, e->key ? e->key : "-",
               e->message);
    }
    cf_builder_log(b, to_syslog, NULL, CF_LOG_INFO);

The library never writes to stderr or anywhere else by itself. An
event's strings belong to the library and are valid during the call.
The function is called on the thread that creates the configuration,
or that drives a cf_live's reloads.

```c
CF_API const char* cf_log_level_name(cf_log_level level);
```

Implementation: [`cf_log_level_name` (src/core/error.c)](https://github.com/develooper1994/configforge/blob/main/src/core/error.c#L43).

The name of a level: "debug", "info", "warn", "error".

## cf_numeric.h

cf_numeric.h - locale-independent numeric conversion.

INTERNAL (D-51): exported for ConfigForge Doctor, the bindings and the
tests, and outside the 1.x promise - a minor release may change it.
A program uses the configforge.h functions instead.

strtod, atoi and printf("%f") all follow the active locale's decimal
separator, so a configuration file written on one machine would not
parse identically on another. These conversions are therefore always
the library's own, at every standard level and on every toolchain.

Floating point uses exact integer digit generation (Steele & White /
Burger & Dybvig), which produces the shortest decimal string that
round-trips, identically on every platform. Decision D-25.

### Integers

```c
CF_API size_t cf_i64_to_text(cf_i64 v, char* buf, size_t cap);
CF_API size_t cf_u64_to_text(cf_u64 v, char* buf, size_t cap);
```

Implementation: [`cf_i64_to_text` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L50), [`cf_u64_to_text` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L26).

Write the decimal form of v. Returns the length written, or 0 when
cap is too small. Always terminates when cap > 0.

```c
CF_API cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out, size_t* end);
CF_API cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out, size_t* end);
```

Implementation: [`cf_text_to_i64` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L102), [`cf_text_to_u64` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L75).

Parse an optionally signed decimal integer. Returns CF_TRUE on
success. Rejects overflow, empty input, and surrounding space;
callers trim deliberately. When end is NULL the whole input must be
consumed; otherwise *end receives the number of bytes consumed.

### Floating point

Disable with CF_WITH_REAL=0 on targets where CF_REAL is not needed.
The exact digit generator uses about 1 KB of stack, which is the
only part of the library that a very small target might refuse.

```c
CF_API size_t cf_f64_to_text(double v, char* buf, size_t cap);
```

Implementation: [`cf_f64_to_text` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L553).

Write the shortest decimal string that reads back as exactly v.
Output is identical on every platform. Returns the length written,
or 0 when cap is too small or v is not finite.

NaN and infinity are deliberately not representable: a configuration
value cannot be either, and rejecting them here keeps every codec
from having to invent a spelling.

```c
CF_API cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end);
```

Implementation: [`cf_text_to_f64` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L732).

Parse a C-locale floating point number. The decimal separator is
always '.', never the locale's. Accepts the forms produced by
cf_f64_to_text and the usual hand-written ones.

### Tier 0 type inference

Used only when no definition exists. Deliberately timid: see the
Type Inference section of the specification and decision D-24.

```c
CF_API cf_scalar_kind cf_infer_scalar(const char* s, size_t len);
```

Implementation: [`cf_infer_scalar` (src/core/numeric.c)](https://github.com/develooper1994/configforge/blob/main/src/core/numeric.c#L929).

## cf_platform.h

cf_platform.h - what the platform provides, behind one door.

INTERNAL (D-51): exported for ConfigForge Doctor, the bindings and the
tests, and outside the 1.x promise - a minor release may change it.
A program uses the configforge.h functions instead.

Three things a configuration library needs that C89 has no answer
for: the process environment, the command line as UTF-8, and a
directory listing. Each has a POSIX shape, a Windows shape, and a
target with neither.

The rule is the same one the portability layer follows for compilers:
**the build picks the file, the call site never asks**. A platform
implementation lives in src/platform/&lt;name>/ and is the only place
allowed to name a platform macro, which gate.detection enforces.

Each implementation guards its whole body with its own
CF_PLATFORM_* macro. That guard is not a scattered #ifdef: it exists
so the amalgamation, which is one translation unit and cannot have
the build choose for it, can carry every platform and let the
preprocessor select.

### Which platform

Decided here and nowhere else. CF_PLATFORM_NONE can be set by hand
for a target that has none of this, and then every capability below
reports that it is absent rather than pretending.

```c
CF_API const char* cf_platform_name(void);
```

Implementation: [`cf_platform_name` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L18), [`cf_platform_name` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L71), [`cf_platform_name` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L33).

The name this build was compiled for, for explain() and the report.

### Capabilities

Queried rather than assumed, so a caller can say "this build cannot
list a directory" instead of handing back an empty one - which looks
exactly like an empty configuration and is the more dangerous of the
two answers.

```c
CF_API cf_bool cf_platform_has_environ(void);
CF_API cf_bool cf_platform_has_dir_list(void);
CF_API cf_bool cf_platform_has_wide_args(void);
```

Implementation: [`cf_platform_has_environ` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L22), [`cf_platform_has_environ` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L75), [`cf_platform_has_environ` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L37), [`cf_platform_has_dir_list` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L26), [`cf_platform_has_dir_list` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L79), [`cf_platform_has_dir_list` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L41), [`cf_platform_has_wide_args` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L30), [`cf_platform_has_wide_args` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L83), [`cf_platform_has_wide_args` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L45).

### The environment

```c
CF_API char* const* cf_platform_environ(void);
```

Implementation: [`cf_platform_environ` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L34), [`cf_platform_environ` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L89), [`cf_platform_environ` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L49).

NULL where none is reachable, which is not an error: a program with
no environment is a normal thing on a small target.

### The command line in UTF-8

On Windows `main`'s argv arrives in the active ANSI code page, so a
value containing any non-ASCII character is wrong before any parser
sees it - and what the code page dropped cannot be converted back,
which is why the Windows implementation reads the wide command line
rather than converting argv. Identity everywhere else, so a caller
writes the same two lines on every platform.

```c
CF_API char* const* cf_platform_args(int argc, char* const* argv, cf_arena* arena, int* out_argc);
```

Implementation: [`cf_platform_args` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L38), [`cf_platform_args` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L93), [`cf_platform_args` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L53).

### Directory listing

```c
CF_API int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max, cf_error* err);
```

Implementation: [`cf_platform_dir_list` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L55), [`cf_platform_dir_list` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L180), [`cf_platform_dir_list` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L200).

Names only, no path, no order, dotfiles already skipped. Returns the
count, or -1 with `err` set. The caller sorts, because the order
decides which file wins and that is the caller's rule, not the
platform's.

### The Windows Registry

Every value under one key, in UTF-8, each with the path of the
subkey holding it. Windows only; everywhere else the walk fails and
says so, because a registry source that read nothing on Linux would
look exactly like an empty one.

```c
CF_API cf_bool cf_platform_has_registry(void);
```

Implementation: [`cf_platform_has_registry` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L83), [`cf_platform_has_registry` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L156), [`cf_platform_has_registry` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L544).

```c
CF_API int cf_platform_registry_walk(const char* root, cf_arena* arena, cf_platform_reg_fn fn, void* ctx, cf_error* err);
```

Implementation: [`cf_platform_registry_walk` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L87), [`cf_platform_registry_walk` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L160), [`cf_platform_registry_walk` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L728).

Every value under `root` - "HKCU\\Software\\MyApp", HKCU, HKLM, HKCR,
HKU or HKCC first - and every subkey's, to CF_MAX_DEPTH. Buffers come
from `arena`. 0 when walked, 1 when `root` does not exist, -1 with
`err` set.

### File stamps

Modification time and size, which is what a change check compares.
Cheap, portable and pollable - and it misses a file rewritten inside
one clock tick with the same length, which is said out loud here
rather than discovered later. The exact answer is to rebuild and
compare digests.

A directory is stamped by what it holds: the latest modification time
among it and its entries, and their total size plus their count, with
dotfiles skipped as the listing skips them. A directory's own time
moves when an entry is added or removed and not when one is edited in
place, so stamping only the directory missed the commonest change to
a drop-in directory there is.

```c
CF_API cf_bool cf_platform_stamp(const char* path, cf_stamp* out);
```

Implementation: [`cf_platform_stamp` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L45), [`cf_platform_stamp` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L127), [`cf_platform_stamp` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L170).

False when the path cannot be stamped, which is not an error: a file
that is not there yet has a stamp saying so, and its arrival is a
change like any other.

### Opening a file by a UTF-8 path

fopen, with the path in UTF-8 everywhere (D-15). On Windows fopen
takes the active code page, which cannot spell most of the world's
names - a home directory under a Turkish or Greek user name cannot
be opened through it on a Western code page - so the Windows
implementation converts to UTF-16 and calls _wfopen. Identity
elsewhere.

Returns a FILE* as void*, so that this header does not include
&lt;stdio.h> for everyone who includes the library: gate.interface
holds the public headers to the system headers they need. NULL when
the file cannot be opened, or on a build with no stdio.

```c
CF_API void* cf_platform_fopen(const char* path, const char* mode);
```

Implementation: [`cf_platform_fopen` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L64), [`cf_platform_fopen` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L149), [`cf_platform_fopen` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L123).

### Who can read a file

1 when every user of the machine can read it, 0 when not, -1 when
this platform cannot say. Windows says -1: whether "everyone" may read
a file is a walk over an access control list with inheritance and
group membership in it, and an answer that guessed would be wrong in
both directions - which, for the one question this exists to answer,
"is this secret exposed", is worse than no answer.

```c
CF_API int cf_platform_world_readable(const char* path);
```

Implementation: [`cf_platform_world_readable` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L76), [`cf_platform_world_readable` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L171), [`cf_platform_world_readable` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L195).

### A handle to wait on (D-35, option 3)

Something an event loop can wait on beside its own descriptors: an
inotify descriptor on Linux, a kqueue descriptor on macOS and the
BSDs, an event HANDLE on Windows. It is not a callback and it owns no
thread - the caller waits, on a thread of its choosing, when it
chooses.

What is watched is the directory each path lives in, or the path
itself when that is a directory. Editors save by writing a new file
and renaming it over the old one, and a watch on the old file follows
it into the bin. So a wake-up means "ask now", never "it changed": a
neighbouring file wakes it too, and cf_changed stays the authority.

A directory that does not exist yet is watched through its nearest
ancestor that does, and drain() moves the watch down as the missing
levels appear - so an optional file in a directory nobody has created
still wakes the caller when it arrives.

```c
CF_API cf_bool cf_platform_has_wake(void);
```

Implementation: [`cf_platform_has_wake` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L98), [`cf_platform_has_wake` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L333), [`cf_platform_has_wake` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L406).

```c
CF_API cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t n, const cf_allocator* a, cf_error* err);
```

Implementation: [`cf_platform_wake_open` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L102), [`cf_platform_wake_open` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L337), [`cf_platform_wake_open` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L410).

NULL with `err` set, naming the path and the reason. The paths are
copied; nothing passed here need outlive the call.

```c
CF_API int cf_platform_wake_fd(const cf_platform_wake* w);
```

Implementation: [`cf_platform_wake_fd` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L111), [`cf_platform_wake_fd` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L386), [`cf_platform_wake_fd` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L459).

A descriptor that becomes readable, or -1 where the handle is not a
descriptor.

```c
CF_API void* cf_platform_wake_handle(const cf_platform_wake* w);
```

Implementation: [`cf_platform_wake_handle` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L116), [`cf_platform_wake_handle` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L390), [`cf_platform_wake_handle` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L464).

A HANDLE that becomes signalled, or NULL where the handle is not one.

```c
CF_API cf_bool cf_platform_wake_drain(cf_platform_wake* w);
```

Implementation: [`cf_platform_wake_drain` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L121), [`cf_platform_wake_drain` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L400), [`cf_platform_wake_drain` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L468).

Consume what woke the handle and re-arm anything that needs it. False
when some path can no longer wake it; polling cf_changed still works.

```c
CF_API int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms);
```

Implementation: [`cf_platform_wake_wait` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L126), [`cf_platform_wake_wait` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L448), [`cf_platform_wake_wait` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L500).

Wait up to `timeout_ms` (negative: without limit). 1 when woken, and
then already drained; 0 on timeout; -1 when waiting failed.

```c
CF_API void cf_platform_wake_close(cf_platform_wake* w);
```

Implementation: [`cf_platform_wake_close` (src/platform/none/none.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/none/none.c#L132), [`cf_platform_wake_close` (src/platform/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/posix/posix.c#L473), [`cf_platform_wake_close` (src/platform/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/src/platform/win32/win32.c#L520).

## cf_port.h

cf_port.h - ConfigForge portability layer.

This is the ONLY header in the project permitted to test __STDC_VERSION__,
_MSC_VER, __GNUC__ or any comparable macro. Enforced by CI.

Baseline is C89. Where the toolchain offers more, it is used - but only for
capability and performance, never for anything whose output is part of a
contract. See the Portability Layer section of the specification.

Define CF_FORCE_PORTABLE to disable every opportunistic upgrade. CI builds
the whole suite both ways and compares the output byte for byte.

### Standard level detection

### Build features

In a header of their own, because there are two versions of it: the
source tree's holds defaults, and the one generated at configure time
holds the values a compiled library actually used. An installed
library ships the generated one, so a consumer cannot disagree with
it by accident - and disagreeing is not a small thing, because
CF_WITH_REAL decides whether cf_value's union has a double in it.

Included here rather than where each option is used, so that every
translation unit agrees whether or not it includes that header. A
feature macro visible in one file and absent in another is how two
objects end up with different struct layouts.

### 64-bit integer

Required. A build without one fails here rather than silently
narrowing CF_INT, because a value model that varies per platform
makes cross-platform determinism impossible.

### Boolean

Deliberately not _Bool even where available: an ABI type must not
change size with the consumer's language standard.

### GCC's extensions

CF_GNUC is GCC's version where GCC's extensions are really there, and
0 where they are not. __GNUC__ alone cannot say: the Portable C
Compiler defines it as 4 to get past system headers that test for it,
and has neither the atomic builtins nor all of the attributes, so a
test of __GNUC__ chose __sync_add_and_fetch and pcc stopped at the
first file. Everything below asks CF_GNUC.

### inline

### What a later standard can say (D-53)

The library is C89 so that it builds everywhere; a program compiled
as a later standard gets what that standard adds to the same
declarations, as the C++ header does for C++.

CF_NODISCARD marks a call whose result is the only way to know it
failed - a create, a try, an open, a write. C23's and C++17's
[[nodiscard]], which a (void) cast silences where ignoring is meant.
Not GCC's warn_unused_result, which a cast does not silence, so code
that ignores a result on purpose would have no way to say so.

### Export control

CF_API marks every function a program may call. In a static build it
is nothing. In a shared one it is what makes the library usable at
all on Windows - a DLL exports only what is marked, so without it the
DLL exported nothing - and on ELF and Mach-O it pairs with
-fvisibility=hidden, so the library's internals stay internal and the
dynamic symbol table holds the interface and nothing else.

CF_SHARED comes from cf_build_options.h, which states the build's
facts: a consumer of an installed DLL gets dllimport without being
told. CF_BUILDING_LIBRARY is defined only while the library's own
sources compile.

### Compile-time assertion

The C89 fallback is the negative-array-size idiom. The typedef name
carries the line number so two assertions can share a scope.

### Function attributes

### Atomic reference counting

Snapshot acquire/release needs one atomic increment. Toolchains with
no atomic support get a single-threaded build, refused at compile
time if threads were requested. Stated in the support matrix rather
than hidden.

```c
CF_API void cf_ref_init(cf_refcount* rc, cf_i32 n);
CF_API cf_i32 cf_ref_inc(cf_refcount* rc);
CF_API cf_i32 cf_ref_dec(cf_refcount* rc);
CF_API cf_i32 cf_ref_get(const cf_refcount* rc);
```

Implementation: [`cf_ref_init` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L24), [`cf_ref_inc` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L28), [`cf_ref_dec` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L44), [`cf_ref_get` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L62).

Sets the initial count. Not a plain assignment at the call site:
the member is volatile on some builds and its spelling is this
layer's business, not the caller's.

### ASCII-only character handling

NEVER use &lt;ctype.h>. tolower/toupper/isspace/isdigit are
locale-dependent: in a Turkish locale toupper('i') is 'I' with a dot
and tolower('I') loses it, so key normalisation through the C library
resolves different keys on a Turkish system than on an English one.

```c
CF_API int cf_cmp_bytes(const char* a, size_t alen, const char* b, size_t blen);
CF_API int cf_cmp_ascii_ci(const char* a, size_t alen, const char* b, size_t blen);
```

Implementation: [`cf_cmp_bytes` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L86), [`cf_cmp_ascii_ci` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L103).

Byte-wise comparison. Never strcoll: ordering must be reproducible.

### Bounded formatting

snprintf is C99. The C89 fallback is not "use sprintf" - an unbounded
formatter is a defect at any standard level - so a small bounded
formatter is implemented instead, and used unconditionally so that
output is identical in both portability modes.

Supported conversions: %s %c %d %i %u %zu %x %% and the ConfigForge
extensions %I (cf_i64), %U (cf_u64) and %S (a const char* followed
by a size_t, for a slice that is not NUL-terminated). Width,
precision and flags are deliberately not supported; callers that
need alignment build it themselves.

Returns the number of characters that would have been written,
excluding the terminator, as snprintf does. Always NUL-terminates
when cap > 0.

```c
CF_API size_t cf_fmt(char* buf, size_t cap, const char* fmt, ...);
CF_API size_t cf_fmt_append(char* buf, size_t cap, size_t at, const char* fmt, ...);
CF_API size_t cf_vfmt(char* buf, size_t cap, const char* fmt, va_list ap);
```

Implementation: [`cf_fmt` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L260), [`cf_fmt_append` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L275), [`cf_vfmt` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L251).

### Build description, reported in explain() and in the provenance

report so that a restricted build is distinguishable from a full one.

```c
CF_API const char* cf_port_description(void);
```

Implementation: [`cf_port_description` (src/core/port.c)](https://github.com/develooper1994/configforge/blob/main/src/core/port.c#L303).

## cf_report.h

cf_report.h - the provenance report.

The artifact D-16 specifies. It is the interface between the library
and Doctor, between ConfigForge and any foreign-ecosystem adapter,
and - through Report Replay - a source format the library itself has
to read. Changing it later breaks all three, which is why the schema
is versioned from v1 and why the blind and full forms are
distinguishable at the top rather than by inspection.

```c
CF_API size_t cf_report_write(const cf_keyspace* ks, const cf_defset* ds, const cf_source_view* sv, const cf_report_info* info, char* buf, size_t cap);
```

Implementation: [`cf_report_write` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L352).

Render the report. Measure-or-write: with a NULL buffer nothing is
stored and the length still accumulates. `info` may be NULL, for a
blind report that names nothing. 0, and an empty `buf`, when there is
no key space or `info` did not start from CF_REPORT_INFO_INIT - its
size is below 1.0's - or its mode is neither CF_REPORT_BLIND nor
CF_REPORT_FULL. A call with no error to give refuses by writing
nothing.

```c
CF_API size_t cf_report_key_write(const cf_keyspace* ks, const cf_defset* ds, const cf_source_view* sv, const char* key, size_t keylen, cf_report_mode mode, char* buf, size_t cap);
```

Implementation: [`cf_report_key_write` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L316).

One key as the report writes it, alone - explain() for a machine:

  { "configforge_key": 1, "key": "server.port",
    "value": { "definition": ..., "candidates": [...], "selected": ... } }

The object under "value" is exactly what the report holds under
"keys.&lt;key>", so whatever reads one reads the other. A key nobody
supplied has no candidates and a null selection. `mode` redacts as
the report does. Unsupported modes return 0 and an empty buffer.
Measure-or-write; 0 without CF_WITH_REPORT.

```c
CF_API size_t cf_schema_write(const cf_defset* ds, const char* title, cf_bool closed, char* buf, size_t cap, cf_error* err);
```

Implementation: [`cf_schema_write` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L714).

The definitions as a JSON Schema (draft 2020-12), for editors: a file
that names it gets completion, documentation and validation as it is
typed, from the contract the program declared. Design.md, section 41.

`closed` puts "additionalProperties": false on every object, and true
on a prefix opened with cf_defset_open, so an editor flags a misspelt
key the way CF_UNKNOWN_REJECT does and cf_defset_schema reads the
schema back as the same definitions. "required" is never
written - a required key may come from the environment, so a file
without it is not wrong - and neither is a sensitive key's default.
Measure-or-write; 0 with `err` set when the keys nest deeper than
CF_MAX_DEPTH, or in a build without CF_WITH_REPORT.

### Report Replay

A full report loaded as a source reproduces, on a developer's
machine, exactly the configuration a production process was running.
Not an approximation, and not a reconstruction from files that may
have changed since.

A blind report cannot be replayed, because it has no values. That is
refused rather than half-applied.

```c
CF_API size_t cf_source_replay_state_size(void);
```

Implementation: [`cf_source_replay_state_size` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L970).

```c
CF_API cf_bool cf_source_replay(cf_source* s, const char* name, int rank, const char* path, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_replay` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L974).

```c
CF_API const cf_error* cf_source_replay_error(const cf_source* s);
```

Implementation: [`cf_source_replay_error` (src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/src/diag/report.c#L994).

## cf_serialize.h

cf_serialize.h - the serialization layer.

Reading a configuration file and writing a provenance report are the
same problem seen from two sides: bytes to a key space, and a key
space to bytes. They are solved once, here, in a layer that knows
nothing about configuration semantics.

    bytes  &lt;->  blob / sink          byte source and sink
                codec                format grammar, both directions
                document events      begin_table, begin_array, scalar
                flattener            events &lt;-> flat key space

The flattener is deliberately not in this file. This layer may use
the leaf types - values, errors, arenas - and may not touch the key
space, the definitions or the sources. That boundary is checked in
CI so that extracting this layer later stays a directory move.

### Bytes in

A whole document in memory rather than a streaming reader.
Configuration files are kilobytes; a streaming parser would be twice
the code, would need its own pushback buffer, and would give up both
zero-copy string slices and trivially exact locations. The one thing
streaming buys - bounded memory over a huge document - is not a
property any configuration file needs.

```c
CF_API void cf_blob_mem(cf_blob* b, const char* p, size_t n, const char* name);
```

Implementation: [`cf_blob_mem` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L15).

```c
CF_API cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err);
```

Implementation: [`cf_blob_file` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L39).

Reads a whole file into the arena. CF_E_SOURCE when it cannot be
opened or read; the caller decides whether that is fatal.

```c
CF_API cf_arena* cf_blob_arena(cf_arena* ar);
CF_API void cf_blob_arena_done(cf_arena* bytes, cf_arena* ar);
```

Implementation: [`cf_blob_arena` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L24), [`cf_blob_arena_done` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L31).

Where a file's bytes go for one reading (D-61): an arena of their own,
from `ar`'s allocator, so that they are gone once the reader is done -
the key space keeps copies of the values - or `ar` itself when it is
fixed and has none to lend. NULL when there is no room for one.
cf_blob_arena_done frees it, zeroed, when it was its own.

### Bytes out

Measure-or-write, exactly like cf_fmt: with a NULL buffer nothing is
stored and the length still accumulates, so a caller sizes a buffer
by rendering once into nothing.

```c
CF_API void cf_sink_init(cf_sink* s, char* buf, size_t cap);
CF_API void cf_sink_write(cf_sink* s, const char* p, size_t n);
CF_API void cf_sink_putc(cf_sink* s, char c);
CF_API void cf_sink_puts(cf_sink* s, const char* z);
CF_API void cf_sink_fmt(cf_sink* s, const char* fmt, ...);
CF_API void cf_sink_terminate(cf_sink* s);
CF_API cf_bool cf_sink_overflowed(const cf_sink* s);
```

Implementation: [`cf_sink_init` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L133), [`cf_sink_write` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L139), [`cf_sink_putc` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L152), [`cf_sink_puts` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L159), [`cf_sink_fmt` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L165), [`cf_sink_terminate` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L185), [`cf_sink_overflowed` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L191).

```c
CF_API void cf_sink_json_string(cf_sink* out, const char* p, size_t n);
CF_API void cf_sink_json_value(cf_sink* out, const cf_value* v);
```

Implementation: [`cf_sink_json_string` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L195), [`cf_sink_json_value` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L236).

JSON, written by hand where a document is not the key space's (a
report, a diff): text as a JSON string, escaped; a value as JSON.

### Locations

```c
CF_API size_t cf_loc_text(const cf_loc* at, char* buf, size_t cap);
```

Implementation: [`cf_loc_text` (src/serialize/blob.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/blob.c#L260).

"app.toml:14:3", or just the file when there is no position.

### Document events

A parser is an event producer, never a source. It emits these and
knows nothing about keys; one shared flattener owns key
construction, array counting and location formatting.

Without that split the key rules would live in every parser and
would drift apart, which is the repetition this project exists to
remove - applied to its own internals.

`name` is the member name inside a table and NULL inside an array,
where position is the name.

### Codecs

```c
CF_API const cf_codec* cf_codec_by_name(const char* name);
CF_API const cf_codec* cf_codec_by_path(const char* path);
```

Implementation: [`cf_codec_by_name` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L67), [`cf_codec_by_path` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L126).

Compiled-in codecs only. NULL when the format exists but this build
left it out, which the caller must report rather than ignore: a
configuration system whose purpose is explaining where values come
from must not have a build option that makes a file invisible.

```c
CF_API const cf_codec* cf_codec_among(const cf_codec* const* list, size_t n, const char* name, const char* path);
```

Implementation: [`cf_codec_among` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L141).

Among a caller's codecs (cf_builder_codec): the one called `name`
when it is not NULL, else the one whose extensions match `path`;
NULL for none. The built-in codecs are not looked at.

```c
CF_API const char* cf_format_of_path(const char* path);
CF_API const char* cf_format_option(const char* format);
```

Implementation: [`cf_format_of_path` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L184), [`cf_format_option` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L199).

Names a format this library knows about whether or not it was
compiled in, so the error can say which option to turn on.

```c
CF_API size_t cf_codec_count(void);
CF_API const cf_codec* cf_codec_at(size_t i);
```

Implementation: [`cf_codec_count` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L55), [`cf_codec_at` (src/serialize/codec.c)](https://github.com/develooper1994/configforge/blob/main/src/serialize/codec.c#L63).

## cf_source.h

cf_source.h - sources.

A source produces candidates. It does not resolve anything and it
does not know what other sources exist; precedence is a rank it is
given, and the key space does the rest. That is what lets a source
be tested in isolation, and it is why the resolver has no per-source
special cases in it.

```c
CF_API cf_bool cf_source_load(const cf_source* s, cf_keyspace* ks);
CF_API void cf_source_destroy(cf_source* s);
```

Implementation: [`cf_source_load` (src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/memory.c#L69), [`cf_source_destroy` (src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/memory.c#L76).

### A view over the sources

Defined here rather than with the diagnostics that consume it: it is
a view over sources, and having it in cf_explain.h meant the core
could not name a source without including the top layer. The apply
pass needs exactly that to resolve a per-parameter policy, which is
what made the misplacement visible.

### Memory source

Explicit pairs. Also serves as the defaults source: a default is a
value from the lowest-precedence source, not a separate mechanism.

```c
CF_API cf_bool cf_source_memory(cf_source* s, const char* name, int rank, const cf_pair* pairs, size_t count);
```

Implementation: [`cf_source_memory` (src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/memory.c#L50).

### Environment source

The environment is passed in rather than taken from a global,
because C89 has no portable environ and because a source that can be
handed a fake environment can be tested properly.

Key derivation, with no definitions present:

    MYAPP_SERVER__PORT   ->  server.port
    MYAPP_TLS_MIN_VERSION ->  tls_min_version

A double underscore is the separator; a single underscore is a
literal underscore in the key. The obvious alternative, mapping a
single underscore to a dot, cannot be reversed: MYAPP_A_B_C could be
a.b.c, a.b_c, a_b.c or a_b_c, and picking one silently is exactly
the kind of guess this library exists to avoid. .NET reached the
same conclusion and uses the same escape.

Variables that do not carry the prefix, or that derive an invalid
key, are skipped rather than guessed at.

```c
CF_API cf_bool cf_source_env(cf_source* s, const char* name, int rank, const char* prefix, char* const* environment);
```

Implementation: [`cf_source_env` (src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/env.c#L47).

```c
CF_API char* const* cf_environ(void); /* cf_platform_environ, kept for callers */
```

Implementation: [`cf_environ` (src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/compat.c#L21).

The platform environment, or NULL where none is reachable.

```c
CF_API size_t cf_env_key(const char* prefix, const char* var, size_t varlen, char* out, size_t cap);
```

Implementation: [`cf_env_key` (src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/env.c#L69).

Exposed for testing: derive a canonical key from a variable name.
Returns the length written, or 0 when the name does not belong to
this prefix or does not derive a valid key.

### Command line source

Accepted forms:

    --server.port=8080       value
    --tls.enabled            bool true
    --                       stop parsing

The separate form "--key value" is deliberately absent at Tier 0: it
cannot be parsed correctly without knowing which options take
values, and a parser that guesses eats positional arguments. It
becomes available at Tier 1, where definitions say.

Keys are written with dots, not dashes. A command line can carry a
dot, so there is no reason to introduce an ambiguous escape the way
the environment needs one.

Arguments that are not options are ignored: they are the program's
positional arguments, not configuration.

```c
CF_API cf_bool cf_source_cli(cf_source* s, const char* name, int rank, int argc, char* const* argv);
```

Implementation: [`cf_source_cli` (src/sources/cli.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/cli.c#L86).

### File source

Reads bytes, picks a codec by extension unless a format is named,
and hands the events to the flattener. It parses nothing itself.

A format that is not compiled in fails loudly and names the option
to turn on. Silently skipping the file would be the one thing a
library about provenance must never do.

```c
CF_API size_t cf_source_file_state_size(void);
```

Implementation: [`cf_source_file_state_size` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L175).

```c
CF_API cf_bool cf_source_file(cf_source* s, const char* name, int rank, const char* path, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_file` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L133).

```c
CF_API cf_bool cf_source_file_codec(cf_source* s, const char* name, int rank, const char* path, const struct cf_codec* codec, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_file_codec` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L155).

The same, with the codec handed over rather than looked up: a format
this library does not have, written against cf_doc_sink - over
libyaml's events, expat's callbacks, anything that streams. The codec
is borrowed and must outlive the configuration.

Explicit rather than registered. A registry code could add to at run
time would be global mutable state, which this library has none of,
and two parts of one program registering different codecs under one
name would be a conflict nobody could see.

```c
CF_API const cf_error* cf_source_file_error(const cf_source* s);
```

Implementation: [`cf_source_file_error` (src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/file.c#L179).

The reason a file source failed, or NULL when it did not. Sources
return a plain boolean because most cannot fail in interesting ways;
a file can fail in a dozen, and the caller needs the detail.

```c
CF_API size_t cf_source_dotenv_state_size(void);
```

Implementation: [`cf_source_dotenv_state_size` (src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dotenv.c#L118).

A .env file: the same key mapping as the environment, because a .env
file is an environment written down. Unlike the environment it
belongs to this program, so a name that derives no key is an error
rather than another program's variable.

```c
CF_API cf_bool cf_source_dotenv(cf_source* s, const char* name, int rank, const char* path, const char* prefix, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_dotenv` (src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dotenv.c#L122).

```c
CF_API const cf_error* cf_source_dotenv_error(const cf_source* s);
```

Implementation: [`cf_source_dotenv_error` (src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dotenv.c#L144).

### A drop-in directory

conf.d is how systemd, sudo, apt and a dozen others let a package add
configuration without editing a file it does not own. Files are read
in byte order, and every one shares the source's rank, so 10-base
losing to 90-site is precedence rather than a merge rule and
explain() names the file that won.

Dotfiles are skipped: an editor's .swp and a package manager's
.dpkg-new both land in such a directory and neither is
configuration.

```c
CF_API cf_bool cf_source_snapshot(cf_source* s, const char* name, const cf_keyspace* ks);
```

Implementation: [`cf_source_snapshot` (src/sources/snapshot.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/snapshot.c#L35).

An existing key space, replayed. Every candidate at the rank it
originally had, not only the winners: a library whose premise is
explaining itself must not lose provenance at the moment a value
changes, which is exactly when somebody asks.

```c
CF_API const char* cf_source_path(const cf_source* s);
```

Implementation: [`cf_source_path` (src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/src/sources/compat.c#L14).

The file a source reads, or NULL.

```c
CF_API size_t cf_source_dir_state_size(void);
```

Implementation: [`cf_source_dir_state_size` (src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dir.c#L188).

```c
CF_API cf_bool cf_source_dir(cf_source* s, const char* name, int rank, const char* path, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_dir` (src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dir.c#L192).

```c
CF_API const cf_error* cf_source_dir_error(const cf_source* s);
```

Implementation: [`cf_source_dir_error` (src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dir.c#L214).

```c
CF_API void cf_source_dir_codecs(cf_source* s, const struct cf_codec* const* codecs, size_t n);
```

Implementation: [`cf_source_dir_codecs` (src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/dir.c#L224).

A program's own codecs for the directory's files, looked at before the
built-in ones (cf_builder_codec, D-68). The list is borrowed and must
outlive the source.

### A directory of secrets (CF_WITH_DIR)

One file a key, as Docker and Kubernetes mount secrets and systemd
hands over credentials: the file named db.password is db.password;
one named as an environment variable would be - MYAPP_DB__PASSWORD,
with `prefix` "MYAPP_" - is mapped as the environment's is; a name
that is neither is an error. The value is the file's content without
its one trailing newline, as text for the contract to type and, for
a secret, to mark CF_SENSITIVE. Dotfiles are skipped, which is also
what Kubernetes' own ..data entries are.

```c
CF_API size_t cf_source_secrets_state_size(void);
```

Implementation: [`cf_source_secrets_state_size` (src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/secrets.c#L152).

```c
CF_API cf_bool cf_source_secrets(cf_source* s, const char* name, int rank, const char* path, const char* prefix, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_secrets` (src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/secrets.c#L156).

```c
CF_API const cf_error* cf_source_secrets_error(const cf_source* s);
```

Implementation: [`cf_source_secrets_error` (src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/secrets.c#L178).

### A key of the Windows Registry

Every value under `root` - HKCU\Software\MyApp - and under its
subkeys, which are sections: Server\Port is server.port. Names are
lowered. REG_DWORD and REG_QWORD are integers, REG_SZ is text the
contract types, REG_EXPAND_SZ is not expanded, REG_MULTI_SZ is a list
(key.0, key.1, key.count); any other type, and a set unnamed value,
is an error naming it. explain() shows each value's full path.
CF_OPTIONAL makes a missing key acceptable. Windows only: on any
other platform the source fails, optional or not, and says why.

```c
CF_API size_t cf_source_registry_state_size(void);
```

Implementation: [`cf_source_registry_state_size` (src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/registry.c#L202).

```c
CF_API cf_bool cf_source_registry(cf_source* s, const char* name, int rank, const char* root, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_registry` (src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/registry.c#L206).

```c
CF_API const cf_error* cf_source_registry_error(const cf_source* s);
```

Implementation: [`cf_source_registry_error` (src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/registry.c#L227).

### A SQLite store (CF_WITH_SQLITE)

The flat key space in one table - configforge(key, type, value,
meta) - read in key order, each value typed by its row, and located
as "&lt;path>: &lt;meta>", so whatever the writer said about a change is
what explain() says about it after a restart. The store is opened
read only and never created by reading; CF_OPTIONAL makes a
missing one acceptable. Without CF_WITH_SQLITE the source fails, and
says which option it needs.

```c
CF_API size_t cf_source_store_state_size(void);
```

Implementation: [`cf_source_store_state_size` (src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/store.c#L332).

```c
CF_API cf_bool cf_source_store(cf_source* s, const char* name, int rank, const char* path, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_store` (src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/store.c#L336).

```c
CF_API const cf_error* cf_source_store_error(const cf_source* s);
```

Implementation: [`cf_source_store_error` (src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/store.c#L356).

```c
CF_API cf_bool cf_store_set(const char* path, const cf_write_item* items, size_t count, const char* meta, cf_error* err);
```

Implementation: [`cf_store_set` (src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/src/bridge/store.c#L192).

Write `items` to the store at `path`, creating it and its table if
need be, in one transaction: every item or none, and a key that is
not valid refuses the lot before anything is written. `meta` - who,
when, why - is stored with each and is what explain() shows as where
the value was set; NULL stores none. Hand-written documents are never
written: a store is where a running program's changes go (Design.md,
section 39).

This writes what it is given and asks nothing; it is the tool for
provisioning a store. A running program writes through cf_write,
which asks whether it may.

## cf_value.h

cf_value.h - the value model.

Five types. Structure is a naming convention on the key, never a
shape in the data: see the Value Model section of the design. That
decision is what keeps a C implementation reasonable, because there
is no recursive value and therefore no ownership graph.

```c
CF_API const char* cf_type_name(cf_type t);
```

Implementation: [`cf_type_name` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L9).

```c
CF_API const char* cf_type_json_name(cf_type t);
```

Implementation: [`cf_type_json_name` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L22).

JSON Schema spells the same types differently; the report and the
schema generator both need that spelling. Two names, one list.

```c
CF_API void cf_value_null(cf_value* v);
CF_API void cf_value_bool(cf_value* v, cf_bool b);
CF_API void cf_value_int(cf_value* v, cf_i64 i);
CF_API void cf_value_real(cf_value* v, double r);
CF_API void cf_value_text(cf_value* v, const char* p, size_t n);
```

Implementation: [`cf_value_null` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L33), [`cf_value_bool` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L38), [`cf_value_int` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L43), [`cf_value_real` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L49), [`cf_value_text` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L55).

```c
CF_API cf_bool cf_value_from_text(cf_type t, const char* s, size_t n, cf_value* out);
```

Implementation: [`cf_value_from_text` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L61).

Convert text to the requested type. Returns CF_FALSE when the text
does not denote a value of that type; the caller then reports a
conversion failure naming the parameter and the source, which is
the only place conversion errors are allowed to appear.

```c
CF_API cf_bool cf_value_infer(const char* s, size_t n, cf_value* out);
```

Implementation: [`cf_value_infer` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L117).

Infer a type from text. Used only where no definition exists, and
deliberately timid: see decision D-24.

```c
CF_API size_t cf_value_to_text(const cf_value* v, char* buf, size_t cap);
```

Implementation: [`cf_value_to_text` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L138).

Canonical text form. Identical on every platform, which is what
makes the determinism hash and the golden fixtures meaningful.
Returns the length written, or 0 when cap is too small.

```c
CF_API int cf_value_cmp(const cf_value* a, const cf_value* b);
CF_API cf_bool cf_value_equal(const cf_value* a, const cf_value* b);
```

Implementation: [`cf_value_cmp` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L195), [`cf_value_equal` (src/core/value.c)](https://github.com/develooper1994/configforge/blob/main/src/core/value.c#L228).

## cf_version.h

cf_version.h - one place the version exists.

A version that lives in two places is a version that disagrees with
itself. These three macros are the only place it is written by hand:
CMake reads them before project(), the Conan recipe reads them, and
the one copy that cannot read anything - the vcpkg manifest - is
rewritten with this file by `cmake -DTO=x.y.z -P scripts/release/version.cmake`
and checked against it by gate.version, as is the CHANGELOG's newest
section. A release is the tag vX.Y.Z on a commit where these say
X.Y.Z; the release workflow refuses any other.

Compatibility, restated from the design:

    0.x   nothing is promised; the C interface is not frozen
    1.0   the C interface freezes and semantic versioning begins

After 1.0 the C interface is additive only: no signature changes, no
struct layout changes. Descriptor structs carry a size field so that
fields can be added without either.

```c
CF_API const char* cf_version(void);
CF_API int cf_version_number(void);
```

Implementation: [`cf_version` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L10), [`cf_version_number` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L14).

The version the library was *compiled* as, which is not necessarily
the header the caller compiled against. A program that links a shared
build can compare the two and say so, which is the only way that
mismatch is ever noticed before it misbehaves.

```c
CF_API const char* cf_build_id(void);
```

Implementation: [`cf_build_id` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L28).

The commit the library was built from, as git abbreviates it -
"20d0f4b" - with "-dirty" when the tree had uncommitted changes, or ""
when the build could not know (no git, or a copy that is neither a
checkout nor a git archive). Build metadata, not part of the version:
two builds of 0.2.0 compare equal by cf_version_number() and can still
be told apart by this. Never NULL. Set at build time, not configure
time, so it follows a checkout without reconfiguring; a build without
CMake can pass -DCF_BUILD_ID="\"...\"".

```c
CF_API size_t cf_feature_count(void);
CF_API const char* cf_feature_at(size_t i);
CF_API cf_bool cf_has_feature(const char* name);
```

Implementation: [`cf_feature_count` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L97), [`cf_feature_at` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L101), [`cf_has_feature` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L128).

What this build can do, as a NUL-terminated list: "ini", "json",
"real", and so on. Recorded in the provenance report as well, so a
report from a restricted build is distinguishable from a full one.
"lists" means the sized definition supports scalar lists, their
flat-source forms and append merging; it is present in every build
that implements them, including a minimal build.

```c
CF_API size_t cf_build_limit(const char* name);
```

Implementation: [`cf_build_limit` (src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/src/core/version.c#L109).

A build limit, by name: "key" (CF_KEY_MAX), "source" (CF_SOURCE_MAX),
"message" (CF_MESSAGE_MAX), "path" (CF_PATH_MAX) and "depth"
(CF_MAX_DEPTH); 0 for a name it does not know. The first three size
cf_error's buffers, so they are part of a build's ABI (D-45): a
program, or a binding that mirrors cf_error, compiled against other
limits must not trust the layout, and this is how it can tell.

