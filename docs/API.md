# API reference

Generated from the public headers by `scripts/generate/apiref.cmake`, and
checked against them by `gate.apiref`: edit the comment in the
header, then run the script. Every function is listed with the
comment written above it, under the section of the header it is
in; the types are in the headers themselves. Implementation links
open the source on `main`, with platform variants listed together.

## What 1.x promises

Three tiers. **Promised**: every function not marked otherwise
below; semantic versioning holds it (D-56) - within 1.x a program
built against an earlier release links and runs against a later
one. **Experimental**: marked so, with the release it came in;
exported, and outside the promise until the next minor release,
which makes it promised or removes it (D-89). **Internal**: marked
so; declared only where `CF_INTERNAL_API` is 1, the library's own
build, Doctor's and the tests' (D-51, D-88); free to change in any
release. The flags a definition takes are bits 0-15 and a source's
bits 16 and up; an unknown bit is refused by name.

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

**The serialization library**, `configforge_serialize`, is made of
the headers `cf_port.h`, `cf_error.h`, `cf_numeric.h`, `cf_value.h`,
`cf_alloc.h`, `cf_platform_file.h`, `cf_serialize.h` and
`cf_serialize_version.h`, and holds their functions to the same three
tiers; `libconfigforge` carries every one of them, so a program links
one library or the other, never both.

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
| `CF_E_BUSY` | `busy` | another writer holds it; try again |

Document refusals also carry a reason and, where known, a line and column.

| Reason | Name | Meaning |
| ------ | ---- | ------- |
| `CF_REASON_NONE` | `none` | no document refusal |
| `CF_REASON_SYNTAX` | `syntax` | the bytes break the format's grammar |
| `CF_REASON_UTF8` | `utf-8` | bytes that are not UTF-8 |
| `CF_REASON_ENCODING` | `encoding` | invalid or inapplicable source encoding |
| `CF_REASON_TRUNCATED` | `truncated` | the input ends inside a construct |
| `CF_REASON_TRAILING` | `trailing` | text or bytes after the document |
| `CF_REASON_EMPTY` | `empty` | no document where one is required |
| `CF_REASON_TOP_LEVEL` | `top-level` | the document is not a table at its top |
| `CF_REASON_DEPTH` | `depth` | nesting deeper than CF_MAX_DEPTH |
| `CF_REASON_TOO_LONG` | `too-long` | a key, string, name or length over a library limit |
| `CF_REASON_BUDGET` | `budget` | a document over the reader's size budget |
| `CF_REASON_ESCAPE` | `escape` | a malformed, unknown or invalid escape |
| `CF_REASON_CONTROL` | `control-character` | a control character where the format forbids it |
| `CF_REASON_NUMBER` | `number` | a malformed number, date or time |
| `CF_REASON_NUMBER_RANGE` | `number-range` | a number the library's types cannot hold |
| `CF_REASON_DUPLICATE_KEY` | `duplicate-key` | a key, table or attribute defined twice |
| `CF_REASON_KEY` | `key` | a name or value that cannot be a key |
| `CF_REASON_STRUCTURE` | `structure` | a shape the key space cannot hold |
| `CF_REASON_UNSUPPORTED` | `unsupported` | a construct this library does not read |
| `CF_REASON_STORAGE` | `storage` | the consumer or its workspace refused |

- [`configforge.h`](#configforgeh) - 95
- [`cf_alloc.h`](#cf_alloch) - 12
- [`cf_define.h`](#cf_defineh) - 27
- [`cf_doctor.h`](#cf_doctorh) - 27
- [`cf_edit.h`](#cf_edith) - 9
- [`cf_emit.h`](#cf_emith) - 2
- [`cf_encoding.h`](#cf_encodingh) - 4
- [`cf_entry.h`](#cf_entryh) - 14
- [`cf_error.h`](#cf_errorh) - 13
- [`cf_explain.h`](#cf_explainh) - 4
- [`cf_file.h`](#cf_fileh) - 7
- [`cf_flatten.h`](#cf_flattenh) - 10
- [`cf_image.h`](#cf_imageh) - 10
- [`cf_iterate.h`](#cf_iterateh) - 5
- [`cf_keyspace.h`](#cf_keyspaceh) - 37
- [`cf_live.h`](#cf_liveh) - 18
- [`cf_lock.h`](#cf_lockh) - 2
- [`cf_log.h`](#cf_logh) - 1
- [`cf_numeric.h`](#cf_numerich) - 15
- [`cf_platform.h`](#cf_platformh) - 31
- [`cf_platform_file.h`](#cf_platform_fileh) - 10
- [`cf_port.h`](#cf_porth) - 12
- [`cf_report.h`](#cf_reporth) - 6
- [`cf_serialize.h`](#cf_serializeh) - 34
- [`cf_serialize_version.h`](#cf_serialize_versionh) - 2
- [`cf_source.h`](#cf_sourceh) - 36
- [`cf_text.h`](#cf_texth) - 25
- [`cf_value.h`](#cf_valueh) - 12
- [`cf_version.h`](#cf_versionh) - 8

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
CF_RANK_USER_FILE or CF_RANK_SYS_FILE. Both take the command line as
UTF-8. Neither expands response files (@file): an argument that starts
with @ is an ordinary argument, so an untrusted one cannot inject
options or tell which files exist. Expansion is opt-in, on a builder:
cf_builder_response_files.

```c
CF_NODISCARD CF_API cf_config* cf_app(const char* name, int argc, char* const* argv, cf_error* err);
```

Implementation: [`cf_app` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1813).

Guarantee: strong. Nothing of the caller's changes; NULL, with `err` set, when no
configuration can be made.

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
CF_NODISCARD CF_API cf_builder* cf_build(const cf_allocator* alloc);
```

Implementation: [`cf_build` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L229).

Guarantee: strong. Nothing exists to change; NULL when the allocator fails.

```c
CF_NODISCARD CF_API cf_builder* cf_build_in(cf_arena* arena);
```

Implementation: [`cf_build_in` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L233).

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
 Guarantee: strong. Nothing exists to change; NULL when the arena cannot hold a builder,
 and the arena is then as it was.

```c
CF_NODISCARD CF_API cf_builder* cf_app_builder(const char* name, int argc, char* const* argv);
```

Implementation: [`cf_app_builder` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L269).

cf_app's builder, before create: the command line (CF_RANK_CLI) and
the NAME_ environment (CF_RANK_ENV), for adding files and definitions
to.
Guarantee: strong. Nothing exists to change; NULL when no builder can be made. A
source that cannot be added is held for cf_builder_create.

```c
CF_API cf_bool cf_builder_response_files(cf_builder* b);
```

Implementation: [`cf_builder_response_files` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L295).

Turns response files on for a builder made by cf_app_builder (1.5):
an argument `@path` past argv[0] (and before a `--`) is replaced by the
lines of that file, one argument per line (cf_cli_expand). Off by
default, because the command line may carry arguments the program does
not trust.

Call it before cf_builder_create. A response file that cannot be read
fails the builder, and cf_builder_create reports it; the call itself
returns CF_FALSE then, and for a builder with no command line (one from
cf_build). Calling it twice is harmless.

A program that relied on the old default - cf_app expanding @file -
starts from cf_app_builder, calls this, and then cf_builder_create.
cf_app itself never expands.
 Guarantee: basic. the builder stays valid; a response file that cannot be read is held
 for cf_builder_create. CF_FALSE with nothing held for a NULL builder or one with no
 command line.

```c
CF_API void cf_builder_abort(cf_builder* b);
```

Implementation: [`cf_builder_abort` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L240).

Frees a builder that will not be created; a cf_build_in arena is the
caller's and is left alone.
Guarantee: nothrow. A NULL builder is accepted; the builder is consumed.

```c
CF_API cf_bool cf_builder_memory(cf_builder* b, const char* name, int rank, const cf_pair* pairs, size_t count);
CF_API cf_bool cf_builder_env(cf_builder* b, const char* name, int rank, const char* prefix, char* const* environment);
CF_API cf_bool cf_builder_cli(cf_builder* b, const char* name, int rank, int argc, char* const* argv);
```

Implementation: [`cf_builder_memory` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L416), [`cf_builder_env` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L426), [`cf_builder_cli` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L437).

The pairs, the environment and the command line are borrowed: the
builder keeps the pointer, not a copy, and reads it when
cf_builder_create runs, so what it points at has to stay valid until
that returns - an argument vector filled in an inner block is gone by
then. What the configuration holds is its own copy, so none of the
three has to outlive it. The name and the prefix are copied at once.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_file(cf_builder* b, const char* name, int rank, const char* path, const char* format, unsigned flags);
```

Implementation: [`cf_builder_file` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L447).

The format is the one named, else the extension's, else - when the name
gives none - the one the content turns out to be (D-73; cf_codec_detect
says how). A name's format that cannot read the file is an error that
adds the format the content looks like, as a hint. A file whose format
this build left out is an error that says which option to turn on;
CF_OPTIONAL makes a *missing* file acceptable, and never makes an
unreadable one acceptable.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_document(cf_builder* b, const char* name, int rank, const char* bytes, size_t n, const char* format, unsigned flags);
```

Implementation: [`cf_builder_document` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L549).

A document the program holds in memory - fetched from a server,
embedded in the binary, read from a database - as a source (D-73).
`format` is named, or NULL for the name's extension and then the
content, as for a file. Its places in explain() are `name` and the
line. The `n` bytes are copied: the builder does not borrow them past
the call, so a buffer freed afterwards is never read, and the document
is read again on every reload.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_env_document(cf_builder* b, const char* name, int rank, const char* variable, const char* format, unsigned flags);
```

Implementation: [`cf_builder_env_document` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L555).

A whole document in one environment variable (`APP_CONFIG='{"server":
{"port": 8080}}'`), opt-in per variable, read as a document is: the
format named, or NULL for the content to decide (D-73). The variable
is looked up at every load, so a reload sees what it holds then. Its
places in explain() are the variable's name and the line; `name` is the
source's (NULL: the variable). A variable that is not set is an error,
and nothing with CF_OPTIONAL. The sources of pairs - cf_builder_env -
are another thing: there is no format in `APP_PORT=80`.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_file_codec(cf_builder* b, const char* name, int rank, const char* path, const struct cf_codec* codec, unsigned flags);
```

Implementation: [`cf_builder_file_codec` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L562).

A file read by a codec the caller supplies, for a format this library
does not have; see cf_source_file_codec. The codec must outlive the
configuration.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_codec(cf_builder* b, const struct cf_codec* codec);
```

Implementation: [`cf_builder_codec` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L800).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API const struct cf_codec* cf_config_codec(const cf_config* c, const char* format);
```

Implementation: [`cf_config_codec` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L824).

The codec a configuration reads and writes `format` with: one its
builder was given, else the built-in one, else NULL.
Guarantee: nothrow. NULL when no codec of that name is known.

```c
CF_API cf_bool cf_builder_dotenv(cf_builder* b, const char* name, int rank, const char* path, const char* prefix, unsigned flags);
```

Implementation: [`cf_builder_dotenv` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L597).

A .env file, read with the environment's key mapping: a .env file is
an environment written down, so the escape and the natural spelling
behave identically in both.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_dir(cf_builder* b, const char* name, int rank, const char* path, const char* format, unsigned flags);
```

Implementation: [`cf_builder_dir` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L738).

A drop-in directory: every file in byte order, all at one rank, so
10-base losing to 90-site is precedence rather than a merge rule and
explain() names the file that won.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_secrets(cf_builder* b, const char* name, int rank, const char* path, const char* prefix, unsigned flags);
```

Implementation: [`cf_builder_secrets` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L621).

A directory of secrets, one file a key, as containers mount them and
systemd passes credentials (cf_source_secrets says how names map).
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_credentials(cf_builder* b, const char* name, int rank, const char* prefix, unsigned flags);
```

Implementation: [`cf_builder_credentials` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L645).

systemd's credentials (LoadCredential=, SetCredential=,
ImportCredential=): the directory $CREDENTIALS_DIRECTORY names, looked
up at every load, read as cf_builder_secrets reads one - a credential's
id is its key, or is mapped as an environment variable's name with
`prefix`; the one trailing newline is dropped; CF_SENSITIVE is the
contract's to give. Every value's place in explain() and in a report is
"systemd credential &lt;id> (&lt;file>)"; `name` is the source's (NULL:
"credentials"). The variable unset or empty is an error that names it,
and nothing with CF_OPTIONAL; set, it must name a directory that can be
read whole, optional or not (D-78, D-80). Not Linux only: the variable
is read wherever it is set. Under CF_WITH_DIR, as the secrets
directory is.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_registry(cf_builder* b, const char* name, int rank, const char* root, unsigned flags);
```

Implementation: [`cf_builder_registry` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L668).

A key of the Windows Registry and its subkeys (cf_source_registry
says how names and types map). Windows only; elsewhere it fails by
name, CF_OPTIONAL or not.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_preferences(cf_builder* b, const char* name, int rank, const char* domain, unsigned flags);
```

Implementation: [`cf_builder_preferences` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L691).

macOS's user defaults (CFPreferences) for one domain, as `defaults
read &lt;domain>` shows them: the current user's, on any host. A
dictionary is a section and an array a list, as in a JSON file:
Server = { Port = 9443; } is server.port. Names are lowered and a
name no key can carry is an error naming it; a string is text the
contract types (`defaults write` writes strings unless told -int),
a number is an int or a real, a boolean a bool, and a date or data
is refused by name. Each value's place in explain() and a report is
"defaults &lt;domain> &lt;path>", Server/Port. `name` is the source's
(NULL: the domain). A domain with no values is an error, and nothing
with CF_OPTIONAL. macOS only, under CF_WITH_PREFERENCES (off by
default: it links CoreFoundation); elsewhere it fails by name,
CF_OPTIONAL or not. Not watched: cf_changed does not see a change to
it, as it does not for the Registry. The other scopes (this host
only, any user, managed) are not read (D-87).
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_store(cf_builder* b, const char* name, int rank, const char* path, unsigned flags);
```

Implementation: [`cf_builder_store` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L715).

A SQLite store (CF_WITH_SQLITE): typed values, each with what its
writer said about it. cf_write writes one, when it is granted.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_writable(cf_builder* b, const char* source_name);
```

Implementation: [`cf_builder_writable` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1331).

Allow cf_write to write the source of this name - the program's
permission, as against the source's capability. Every source is read
only until granted. A grant to a source that cannot be written, or to
no source of that name, fails cf_builder_create, before anything is
read. A derived configuration keeps the grants of the one it came
from.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_source(cf_builder* b, const cf_source* s);
```

Implementation: [`cf_builder_source` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L857).

Register a source the caller built itself. The source object is
copied, so the caller's may be on the stack; any state it points at
must outlive the configuration.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_define(cf_builder* b, const cf_def* d);
```

Implementation: [`cf_builder_define` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L956).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_schema(cf_builder* b, const char* text, size_t n, const char* name);
CF_API cf_bool cf_builder_schema_file(cf_builder* b, const char* path);
```

Implementation: [`cf_builder_schema` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L930), [`cf_builder_schema_file` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L943).

The contract from a JSON Schema - the text, or a file - as
cf_defset_schema reads one: the definitions cf_schema would write
back. A keyword it does not read is refused by name and place, and
cf_builder_create reports it. (1.2)
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_open(cf_builder* b, const char* prefix);
```

Implementation: [`cf_builder_open` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L885).

Exempt a prefix from unknown-key rejection, for a key space the
program cannot know in advance: a plugin's settings are not in the
contract because the plugin is not either.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API void cf_limit_options_init(cf_limit_options* o);
```

Implementation: [`cf_limit_options_init` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L922).

Guarantee: nothrow.

```c
CF_API cf_bool cf_builder_limits(cf_builder* b, const cf_limit_options* o);
```

Implementation: [`cf_builder_limits` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L898).

Sets the limits of `b`, before or between its calls to cf_builder_open.
CF_FALSE for a NULL `b`, or an `o` whose `size` does not reach
`max_open_prefixes`; and, with the failure held for cf_builder_create as
the builder's others are (CF_E_DEFINITION, "more than N open
prefixes"), when `b` already holds more prefixes than the new limit. A
derived configuration (cf_derive) and a write keep the limit.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_bool(cf_builder* b, const char* key, cf_bool dflt, unsigned flags);
```

Implementation: [`cf_builder_bool` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L969).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_int(cf_builder* b, const char* key, cf_i64 dflt, cf_i64 min, cf_i64 max, unsigned flags);
```

Implementation: [`cf_builder_int` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L981).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_text(cf_builder* b, const char* key, const char* dflt, unsigned flags);
```

Implementation: [`cf_builder_text` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L994).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_real(cf_builder* b, const char* key, double dflt, double min, double max, unsigned flags);
```

Implementation: [`cf_builder_real` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1008).

Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API cf_bool cf_builder_key(cf_builder* b, const char* key, cf_type type, const char* dflt, const char* min, const char* max, unsigned flags);
```

Implementation: [`cf_builder_key` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1063).

One spelling for every scalar type (D-74), and the one every binding
passes through. The default and the bounds are text, read by the
rules a file's value is; `type` names the type, or is CF_TYPE_NULL
for "the default decides": "true" a bool, "8080" an int, "0.5" a
real, anything else text. `min` and `max` bound an int or a real and
are NULL for no range. A default or a bound that does not read as the
type is refused, a range on a bool or text is refused, and so is a
key with neither a type nor a default. (1.3)
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API void cf_builder_unknown(cf_builder* b, cf_unknown_policy policy);
```

Implementation: [`cf_builder_unknown` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1112).

Guarantee: nothrow.

```c
CF_API void cf_builder_log(cf_builder* b, cf_log_fn fn, void* user, cf_log_level min);
```

Implementation: [`cf_builder_log` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1235).

Where what deserves a log line and is not an error goes (cf_log.h):
unknown keys under CF_UNKNOWN_WARN, deprecated keys in use, values
refused and fallen back from. Events below `min` are not made. A
configuration derived from this one logs to the same place.
Guarantee: nothrow.

```c
CF_API void cf_builder_track_reads(cf_builder* b);
```

Implementation: [`cf_builder_track_reads` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1118).

Record which keys the program reads, for finding configuration
nothing uses. Off by default, and nothing changes for a read when it
is off. On, a key counts as read the first time cf_get_*, cf_try_*,
cf_has or cf_type_of asks for it through this snapshot; listing or
reporting is not a read. Reads stay safe concurrently (D-14): the
flag is atomic and written once. A configuration derived from this
one tracks too, from nothing. cf_was_read and cf_read_count answer;
cf_report adds "read" to every key, a boolean, so a blind report
still carries no value.
Guarantee: nothrow.

```c
CF_EXPERIMENTAL(2, 1) void cf_builder_track_misreads(cf_builder* b);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_builder_track_misreads` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1124).

Record the reads the contract does not have (D-121): a typed read
(cf_get_*, cf_try_*) of a key the contract does not define and that is
under no open prefix, or whose value, or definition when it has no
value, has another type than the read asked for. cf_has, cf_type_of
and cf_explain ask what is there, and are not reads. Off by default;
independent of cf_builder_track_reads. A configuration derived from
this one tracks too, from nothing. Sixteen distinct (key, kind,
wanted) are kept, claimed without a lock; the rest are lost. No value
is recorded. cf_misreads and cf_misread_at answer, and cf_report adds
a "misreads" array.
Guarantee: nothrow.

```c
CF_API cf_bool cf_builder_digest_key(cf_builder* b, const unsigned char* key, size_t n);
```

Implementation: [`cf_builder_digest_key` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1130).

Digest every sensitive value under this key (D-85): SipHash-2-4,
written "sip24:&lt;id>:&lt;16 hex digits>", in place of the unkeyed
FNV-1a digest, wherever a value-blind report, a full one or the
fingerprint stands a digest for a secret. Without a key a digest
can be tested against guesses - a PIN in a moment; with one it
cannot, and reports made under one key still compare. `id` is the
key's own digest, so two keys are told apart without either being
shown. `n` must be 16; the bytes are copied and wiped with the
configuration, and a derived configuration keeps them.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API void cf_sizing_options_init(cf_sizing_options* o);
```

Implementation: [`cf_sizing_options_init` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1175).

Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_builder_sizing(cf_builder* b, const cf_sizing_options* o);
```

Implementation: [`cf_builder_sizing` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1184).

Sets the sizing mode of what this builder creates; the last call wins,
and once cf_builder_create has run the mode is fixed. NULL options are
the defaults, CF_SIZING_MANUAL. CF_FALSE, with the error held for
cf_builder_create, for a NULL builder, options too small to hold a cap,
an unknown mode, SEMI with a cap of 0 (CF_E_POLICY), or a mode other than
MANUAL in a build with fixed limits (CF_E_CAPABILITY). A derived
configuration keeps its original's mode.
Guarantee: basic. the builder stays valid and holds its first failure; the mode and
the cap are as they were when the options are refused.

```c
CF_NODISCARD CF_API cf_bool cf_config_sizing(const cf_config* c, cf_sizing_options* o);
```

Implementation: [`cf_config_sizing` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1225).

Fills `o` (its `size` is read first and kept) with the mode and cap this
configuration was built with. CF_FALSE for a NULL argument or options
too small to hold a cap.
Guarantee: strong. A query; `o` is not written when it returns CF_FALSE.

```c
CF_NODISCARD CF_API cf_config* cf_builder_create(cf_builder* b, cf_error* err);
```

Implementation: [`cf_builder_create` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1472).

Guarantee: strong. The configuration is made apart and published at the end. The
builder is consumed whether this succeeds or fails, and the failure is in `err`; no
half-made configuration exists.

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
```

Implementation: [`cf_get_bool` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2508), [`cf_get_int` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2513), [`cf_get_text` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2518).

Guarantee: nothrow. The fallback for an absent key or another type.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_API double cf_get_real(const cf_config* c, const char* key, double fallback);
```

Implementation: [`cf_get_real` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2540).

Guarantee: nothrow. The fallback for an absent key or another type.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_API const char* cf_get_text_n(const cf_config* c, const char* key, size_t* len, const char* fallback);
```

Implementation: [`cf_get_text_n` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2523).

Pointer and length, for text that may contain anything. The stored
value is NUL-terminated as well, so cf_get_text stays free.
Guarantee: nothrow. The fallback for an absent key or another type.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_NODISCARD CF_API cf_code cf_try_bool(const cf_config* c, const char* key, cf_bool* out);
CF_NODISCARD CF_API cf_code cf_try_int(const cf_config* c, const char* key, cf_i64* out);
CF_NODISCARD CF_API cf_code cf_try_text(const cf_config* c, const char* key, const char** out, size_t* len);
```

Implementation: [`cf_try_bool` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2560), [`cf_try_int` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2569), [`cf_try_text` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2578).

CF_OK, CF_E_KEY when absent, CF_E_CONVERT on a type mismatch.
Guarantee: strong. A read; the configuration is untouched and `out` is unspecified
unless the result is CF_OK.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_NODISCARD CF_API cf_code cf_try_real(const cf_config* c, const char* key, double* out);
```

Implementation: [`cf_try_real` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2594).

Guarantee: strong. A read; the configuration is untouched and `out` is unspecified
unless the result is CF_OK.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_API cf_type cf_type_of(const cf_config* c, const char* key);
```

Implementation: [`cf_type_of` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2604).

CF_TYPE_NULL when the key is absent.
Guarantee: nothrow. CF_TYPE_NULL for an absent key.

```c
CF_NODISCARD CF_API cf_bool cf_has(const cf_config* c, const char* key);
```

Implementation: [`cf_has` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2609).

Guarantee: strong. A query; it changes nothing.

```c
CF_NODISCARD CF_API cf_bool cf_tracks_reads(const cf_config* c);
CF_NODISCARD CF_API cf_bool cf_was_read(const cf_config* c, const char* key);
```

Implementation: [`cf_tracks_reads` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2647), [`cf_was_read` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2651).

Read coverage (cf_builder_track_reads). CF_FALSE and 0 for a
configuration that does not track. cf_read_count counts keys, not
reads; neither of these is itself a read.
Guarantee: strong. A query; it changes nothing.

```c
CF_API size_t cf_read_count(const cf_config* c);
```

Implementation: [`cf_read_count` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2662).

Guarantee: nothrow.

```c
CF_EXPERIMENTAL(2, 1) cf_bool cf_tracks_misreads(const cf_config* c);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_tracks_misreads` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2613).

Misreads (cf_builder_track_misreads). cf_misreads is the number of
distinct (key, kind, wanted) recorded, 0 for a configuration that does
not track or NULL. cf_misread_at gives the i-th in the order first
recorded: CF_FALSE past the end or when not tracking; any out pointer
may be NULL; `*key` is owned by the configuration and valid until it
is freed (cut at CF_KEY_MAX - 1 bytes). `found` is CF_TYPE_NULL for an
undefined key. Neither is itself a read.
Guarantee: strong. A query; it changes nothing.

```c
CF_EXPERIMENTAL(2, 1) size_t cf_misreads(const cf_config* c);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_misreads` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2617).

Guarantee: strong. A query; it changes nothing.

```c
CF_EXPERIMENTAL(2, 1) cf_bool cf_misread_at(const cf_config* c, size_t i, const char** key, cf_misread_kind* kind, cf_type* wanted, cf_type* found);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_misread_at` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2621).

Guarantee: strong. A query; it changes nothing.

### Diagnostics

No setup, no flag, no build option. The provenance chain is what
get() reads, so printing it costs nothing that was not already paid.

A key defined CF_SENSITIVE has its chain shown in full and its values
shown as "&lt;sensitive, N bytes>": which source won and where it was
set is the question, and it is not the secret.

```c
CF_API size_t cf_explain(const cf_config* c, const char* key, char* buf, size_t cap);
```

Implementation: [`cf_explain` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2680).

Guarantee: strong. The configuration is untouched; the size needed is returned and
`buf` is the caller's.

```c
CF_API size_t cf_explain_json(const cf_config* c, const char* key, cf_report_mode mode, char* buf, size_t cap);
```

Implementation: [`cf_explain_json` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2744).

The same chain as JSON, for an administration endpoint or a tool:
cf_report_key_write says what it holds. CF_REPORT_BLIND is what an
endpoint wants - presence, origin, length and digest for a sensitive
key, never its value.
Guarantee: strong. As cf_explain.

```c
CF_API size_t cf_inspect(const cf_config* c, char* buf, size_t cap);
```

Implementation: [`cf_inspect` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2691).

Guarantee: strong. As cf_explain.

```c
CF_API size_t cf_diff(const cf_config* before, const cf_config* after, cf_diff_fn fn, void* ctx);
```

Implementation: [`cf_diff` (lib/config/src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/diff.c#L35).

The number of changed keys; `fn` may be NULL to only count them.
Guarantee: nothrow. Neither configuration is touched; `fn` may stop it.

```c
CF_API size_t cf_diff_text(const cf_config* before, const cf_config* after, char* buf, size_t cap);
```

Implementation: [`cf_diff_text` (lib/config/src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/diff.c#L281).

One line per change, with where each new value came from, and the
values of CF_SENSITIVE keys redacted as explain() redacts them:

  server.port: 8080 -> 9443 (file app.ini:2:8)
  log.level: added, "debug" (cli)
  cache.size: 64, now from file app.ini:7:14, was from default
  db.password: changed, &lt;sensitive> (env)

Measure-or-write, like every renderer here.
Guarantee: strong. Neither configuration is touched; the size needed is returned and
`buf` is the caller's.

```c
CF_API size_t cf_diff_json(const cf_config* before, const cf_config* after, char* buf, size_t cap);
```

Implementation: [`cf_diff_json` (lib/config/src/diag/diff.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/diff.c#L269).

The same changes as JSON Lines, one object a line, for a log pipeline:

  {"key": "server.port", "change": "value", "before": 8080,
   "after": 9443, "from": {"source": "file", "at": "app.ini:2:8"}}

"change" is added, removed, value or origin; "from" is where the
new value is, "was_from" where the old one was when that differs. A
CF_SENSITIVE key has "sensitive": true and no values. Measure-or-write.
Guarantee: strong. Neither configuration is touched; the size needed is returned and
`buf` is the caller's.

```c
CF_API cf_u64 cf_digest(const cf_config* c);
```

Implementation: [`cf_digest` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2715).

Order-independent over the resolved configuration, identical on
every platform and in both portability modes.
Guarantee: nothrow.

```c
CF_API cf_u64 cf_fingerprint(const cf_config* c);
```

Implementation: [`cf_fingerprint` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2719).

What identifies this configuration where operators look - a log line,
a trace, a report's "fingerprint": as cf_digest, except that a secret
counts by the digest a value-blind report shows for it, never by its
value, so the fingerprint can be published wherever a blind report
could. Two configurations with the same values and secrets have the
same one, on every platform. Not cryptographic: it tells changes
apart, and proves nothing. (1.1)
Guarantee: nothrow.

```c
CF_API size_t cf_fingerprint_format(cf_u64 fp, char* buf, size_t cap);
```

Implementation: [`cf_fingerprint_format` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2724).

`fp` as reports and logs write it, "fnv1a:" and sixteen hex digits,
in `buf` as far as `cap` allows; the length it needs, 22. (1.1)
Guarantee: nothrow. `buf` is the caller's; the length needed is returned.

```c
CF_API size_t cf_report(const cf_config* c, const cf_report_info* info, char* buf, size_t cap);
```

Implementation: [`cf_report` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2756).

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
Guarantee: strong. The configuration is untouched; an `info` that did not start from
CF_REPORT_INFO_INIT writes nothing; the size needed is returned and `buf` is the
caller's.

```c
CF_EXPERIMENTAL(2, 1) void cf_serve_options_init(cf_serve_options* o);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_serve_options_init` (lib/config/src/diag/serve.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/serve.c#L231).

The defaults above. A NULL pointer is accepted and left alone.
Guarantee: nothrow.

```c
CF_EXPERIMENTAL(2, 1) cf_bool cf_serve_report(const cf_config* c, int fd, const cf_serve_options* opt, cf_error*               err);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_serve_report` (lib/config/src/diag/serve.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/serve.c#L240).

A running program answers one request for its report (D-123). `fd` is a
connected stream socket (AF_UNIX or AF_INET) the program's own loop
accepted; it is made non-blocking for the call, restored before return and
never closed, and the library accepts, listens, binds and keeps nothing:
no thread, no state. One HTTP/1.0 request is read (the headers, at most
4096 bytes, each wait at most `timeout_ms`) and one answer written:
`GET /report` is 200 with the value-blind report (always blind: an
`opt->info` asking for CF_REPORT_FULL is refused with CF_E_POLICY), another
target 404, another method 405, a malformed request 400, a build without
CF_WITH_REPORT 501. Any of these is an answer and CF_TRUE. CF_FALSE with
`err` (CF_E_SOURCE) when nothing could be answered: a timeout, the peer
closing first, an I/O error, a descriptor that is not a socket. `opt` that
did not start from CF_SERVE_OPTIONS_INIT: CF_FALSE, CF_E_CAPABILITY. A build
without a POSIX descriptor: CF_FALSE, CF_E_CAPABILITY.
Guarantee: basic. The configuration is untouched; the descriptor stays open
and the program's, and a failed call may have written part of an answer.
Thread: safe.
Signal: unsafe.
Alloc: may.

```c
CF_API size_t cf_schema(const cf_config* c, const char* title, char* buf, size_t cap);
```

Implementation: [`cf_schema` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2701).

The configuration's contract as a JSON Schema, closed when it rejects
unknown keys (CF_UNKNOWN_REJECT). See cf_schema_write.
Guarantee: strong. The configuration is untouched; the size needed is returned and
`buf` is the caller's.

```c
CF_API size_t cf_usage(const cf_config* c, const char* program, const char* env_prefix, char* buf, size_t cap);
```

Implementation: [`cf_usage` (lib/config/src/diag/usage.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/usage.c#L174).

The --help text of a program, from its contract: for each definition
its flags (its aliases, then --key), value, description, and one line
of what the contract knows - required, default, range, length,
repeatable, environment name, deprecated, sensitive. A sensitive
key's default is not shown. `env_prefix` is the one given to
cf_builder_env ("MYAPP_" for cf_app); NULL leaves the environment
name out. `program` NULL is "program". Measure-or-write: the length
of the whole text, without the NUL; NULL `buf` only measures, and a
short one gets the prefix that fits. It does not intercept --help:
a program declares a bool and calls this. (1.8)
Guarantee: strong. The configuration is untouched; the size needed is returned and
`buf` is the caller's.

```c
CF_API cf_bool cf_builder_replay(cf_builder* b, const char* name, int rank, const char* path, unsigned flags);
```

Implementation: [`cf_builder_replay` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L834).

Load a full report as a source. Reproduces exactly the configuration
a process was running, rather than a reconstruction from files that
may have changed since. `flags` is a source's: CF_OPTIONAL, or 0.
Guarantee: basic. the builder stays valid and holds its first failure;
cf_builder_create reports it (nowhere to hold it for a NULL builder).

```c
CF_API size_t cf_count(const cf_config* c);
CF_API const cf_apply_report* cf_apply_stats(const cf_config* c);
CF_API const cf_keyspace* cf_keys(const cf_config* c);
CF_API const cf_source_view* cf_sources(const cf_config* c);
CF_API const cf_defset* cf_defs(const cf_config* c);
```

Implementation: [`cf_count` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2728), [`cf_apply_stats` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2732), [`cf_keys` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2768), [`cf_sources` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2736), [`cf_defs` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2740).

How many keys the configuration knows. A key is known once anything
supplied it, so one that has no value is counted too: every candidate
for it was refused, or it is an element beyond the end of its array.
cf_has says which keys have one.
Guarantee: nothrow.

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

Implementation: [`cf_derive` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1970).

Guarantee: strong. The derived configuration is made apart; on failure `base` is as
it was and nothing is left behind.

```c
CF_NODISCARD CF_API cf_config* cf_derive_in(const cf_config* base, cf_arena* arena, const char*    source_name, const cf_pair* pairs, size_t count, cf_error* err);
```

Implementation: [`cf_derive_in` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L1992).

The same, inside an arena the caller owns. A configuration built with
cf_build_in has no allocator to borrow, so it must use this one.
Guarantee: strong. As cf_derive; the caller's arena is used only for what the derived
configuration holds.

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

Implementation: [`cf_write` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2300).

Guarantee: strong. The configuration (the read snapshot) never changes; every item is
checked before one is written, and a store writes them in one transaction that is
rolled back on failure.

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
CF_NODISCARD CF_API cf_bool cf_changed(const cf_config* c);
```

Implementation: [`cf_changed` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2377).

Guarantee: strong. A query; it changes nothing.

```c
CF_API size_t cf_watch_count(const cf_config* c);
```

Implementation: [`cf_watch_count` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2366).

Guarantee: nothrow.

```c
CF_API const char* cf_watch_path(const cf_config* c, size_t i);
```

Implementation: [`cf_watch_path` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2370).

Guarantee: nothrow. NULL for an index past the end.

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

Implementation: [`cf_watcher_open` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2400).

`a` NULL means the default allocator. Fails when the configuration
read no file, since there would be nothing to wake it.
Guarantee: strong. Nothing exists to change; NULL, with `err` set, when no watcher
can be made.

```c
CF_API int cf_watcher_fd(const cf_watcher* w);
```

Implementation: [`cf_watcher_fd` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2419).

-1 where it is not a descriptor.
Guarantee: nothrow.

```c
CF_API void* cf_watcher_handle(const cf_watcher* w);
```

Implementation: [`cf_watcher_handle` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2423).

NULL where it is not a HANDLE.
Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_watcher_drain(cf_watcher* w);
```

Implementation: [`cf_watcher_drain` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2427).

Guarantee: strong. A failed drain leaves the watcher valid, to be asked again.

```c
CF_API int cf_watcher_wait(cf_watcher* w, long timeout_ms);
```

Implementation: [`cf_watcher_wait` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2431).

1 woken (and drained), 0 timed out, -1 failed. Negative: no limit.
Guarantee: basic. A failure (-1) leaves the watcher valid, but wake-ups may have been
consumed.

```c
CF_API void cf_watcher_close(cf_watcher* w);
```

Implementation: [`cf_watcher_close` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2435).

Guarantee: nothrow. A NULL watcher is accepted.

```c
CF_API void cf_config_ref(cf_config* c);
```

Implementation: [`cf_config_ref` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2439).

Guarantee: nothrow.

```c
CF_API void cf_config_free(cf_config* c);
```

Implementation: [`cf_config_free` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2445).

Guarantee: nothrow. A NULL configuration is accepted.

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

Implementation: [`cf_default_allocator` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L33).

malloc and free. NULL on targets built with CF_NO_STDLIB.
Guarantee: nothrow. NULL on a target built with CF_NO_STDLIB.

### Arena

Two flavours. A growing arena takes blocks from an allocator; a
fixed arena works inside a buffer the caller supplies and fails
cleanly when it is exhausted, which is the freestanding profile.

```c
CF_NODISCARD CF_API cf_arena* cf_arena_new(const cf_allocator* a, size_t block_size);
```

Implementation: [`cf_arena_new` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L99).

`block_size` is a hint: anything below 1 KiB is replaced by a sensible
default, because an arena that takes sixty-four bytes at a time is a
malloc wrapper with extra steps. Said out loud because a silently
ignored parameter is how a test ends up measuring the default three
times and reporting it as three results.
Guarantee: strong. Nothing exists to change; NULL when the allocator fails.

```c
CF_API cf_arena* cf_arena_fixed(void* buffer, size_t size);
```

Implementation: [`cf_arena_fixed` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L131).

Guarantee: strong. Nothing exists to change; NULL when the buffer is NULL or too
small.

```c
CF_API void cf_arena_free(cf_arena* ar);
```

Implementation: [`cf_arena_free` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L221).

Release the arena, zeroing every byte it handed out first. A
configuration's arena holds its values - passwords and tokens among
them - and the text of every file it read, and memory handed back to
an allocator is memory the next allocation, a core dump or a swap
file can show to someone else. A fixed arena is zeroed and not
released: the buffer is the caller's.
Guarantee: nothrow.

```c
CF_API void cf_arena_wipe(cf_arena* ar);
```

Implementation: [`cf_arena_wipe` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L210).

Zero every byte the arena has handed out, without releasing it. For a
caller who owns the arena - cf_build_in - and is done with what lived
in it: nothing allocated from it may be used afterwards.
Guarantee: nothrow.

```c
CF_API const cf_allocator* cf_arena_allocator(const cf_arena* ar);
```

Implementation: [`cf_arena_allocator` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L190).

The allocator this arena takes blocks from, or NULL for a fixed one.
Needed so that deriving a configuration keeps using the allocator the
caller supplied rather than quietly falling back to malloc.
Guarantee: nothrow.

```c
CF_API void* cf_arena_alloc(cf_arena* ar, size_t n);
```

Implementation: [`cf_arena_alloc` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L244).

Guarantee: strong. NULL when a fixed arena is exhausted or the allocator refuses; the
arena is as it was.

```c
CF_API void* cf_arena_calloc(cf_arena* ar, size_t n);
```

Implementation: [`cf_arena_calloc` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L294).

Guarantee: strong. As cf_arena_alloc; the bytes are zeroed only once the request is
served.

```c
CF_NODISCARD CF_API char* cf_arena_dup(cf_arena* ar, const char* s, size_t len);
```

Implementation: [`cf_arena_dup` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L302).

Copy len bytes and terminate. Stored text is NUL-terminated in
addition to carrying a length, so the convenience accessors are
free and an embedded NUL is detectable by the caller.
Guarantee: strong. As cf_arena_alloc; NULL also for a NULL `s`.

```c
CF_API size_t cf_arena_used(const cf_arena* ar);
```

Implementation: [`cf_arena_used` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L319).

Guarantee: nothrow.

```c
CF_API size_t cf_arena_capacity(const cf_arena* ar);
```

Implementation: [`cf_arena_capacity` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L323).

Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_arena_is_fixed(const cf_arena* ar);
```

Implementation: [`cf_arena_is_fixed` (lib/base/src/alloc.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/alloc.c#L327).

Guarantee: strong. A query; it changes nothing, and a NULL arena is not fixed.

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

Implementation: [`cf_def_init_sized` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L35).

Initialise a descriptor: every field zero, `size` the caller's
sizeof(cf_def), so a library from a later 1.x writes no further than
the caller's struct reaches (D-51). Call it as cf_def_init, which
passes that size; cf_def_init_sized is for a binding that lays out
cf_def itself.
Guarantee: nothrow. A NULL `d` is ignored; a `size` below 1.0's writes only
`d->size`, which cf_builder_define then refuses by name.

### Definition set

```c
CF_NODISCARD CF_API cf_bool cf_defset_init(cf_defset* ds, cf_arena* arena, size_t hint);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_init` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L49).

```c
CF_NODISCARD CF_API cf_bool cf_define(cf_defset* ds, const cf_def* d, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_define` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L415).

Copies the descriptor. Rejects a definition that contradicts itself:
a default outside its own range, an empty enum, a range on a type
that has none. Those are programming errors and are worth refusing
at the point they are written.

```c
CF_NODISCARD CF_API cf_bool cf_def_bool(cf_defset* ds, const char* key, cf_bool dflt, unsigned flags, cf_error* err);
CF_NODISCARD CF_API cf_bool cf_def_int(cf_defset* ds, const char* key, cf_i64 dflt, cf_i64 min, cf_i64 max, unsigned flags, cf_error* err);
CF_NODISCARD CF_API cf_bool cf_def_text(cf_defset* ds, const char* key, const char* dflt, unsigned flags, cf_error* err);
CF_NODISCARD CF_API cf_bool cf_def_real(cf_defset* ds, const char* key, double dflt, double min, double max, unsigned flags, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_def_bool` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L580), [`cf_def_int` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L592), [`cf_def_text` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L609), [`cf_def_real` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L622).

```c
CF_API void cf_defset_seal(cf_defset* ds);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_seal` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L726).

Sorts for binary lookup; calling twice is harmless.

```c
CF_NODISCARD CF_API cf_bool cf_defset_open(cf_defset* ds, const char* prefix, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_open` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L669).

Exempt a prefix from unknown-key rejection.

```c
CF_NODISCARD CF_API cf_bool cf_defset_schema(cf_defset* ds, const char* text, size_t n, const char* name, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_schema` (lib/config/src/bridge/schema.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/schema.c#L719).

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
CF_NODISCARD CF_API cf_bool cf_defset_schema_file(cf_defset* ds, const char* path, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_schema_file` (lib/config/src/bridge/schema.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/schema.c#L768).

The same, from a file. (1.2)

```c
CF_NODISCARD CF_API cf_bool cf_defset_is_open(const cf_defset* ds, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_is_open` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L694).

Is this key under an exempt prefix? Matches at a segment boundary
only, so opening "plugins" does not open "pluginsomething".

```c
CF_API const cf_def* cf_defset_find(const cf_defset* ds, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_find` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L831).

Authored descriptors are returned for exact keys. A declared list's
count and canonical decimal indices return immutable scalar
projections; an item projection's key is the list root. Enumeration
still returns only authored descriptors.

```c
CF_API const cf_def* cf_defset_list(const cf_defset* ds, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_list` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L860).

The declared list containing a key, including its root. Unlike find,
this also finds a list whose caller supplied an invalid member name,
so the apply pass can refuse that member instead of treating it as
an unrelated, undefined setting.

```c
CF_API cf_u64 cf_defset_fingerprint(const cf_defset* ds, const cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_fingerprint` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L1061).

The fingerprint of `ks` under these definitions (cf_fingerprint): as
cf_ks_digest, except that a key defined CF_SENSITIVE enters by its
cf_ks_value_digest text, as a value-blind report shows it, rather
than by its value. `ds` may be NULL: nothing is sensitive.

```c
CF_API cf_u64 cf_fingerprint_seed(void);
CF_API cf_u64 cf_fingerprint_step(cf_u64 h, const char* key, size_t keylen, const char* type, const char* text, size_t n);
```

Implementation: [`cf_fingerprint_seed` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L1045), [`cf_fingerprint_step` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L1051).

The fingerprint a step at a time, for a reader that holds keys as text -
Doctor, recomputing a report's - and not as a key space: start from the
seed, then give each selected entry in key order its key, the type name
(cf_type_name) and the value's text, or for a secret the text of its
cf_ks_value_digest. cf_defset_fingerprint is these steps over a key
space, so the two cannot disagree.
Guarantee: nothrow. A hash of what is given; a NULL key, type or text is empty.

```c
CF_API size_t cf_defset_count(const cf_defset* ds);
CF_API const cf_def* cf_defset_at(const cf_defset* ds, size_t i);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_count` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L865), [`cf_defset_at` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L869).

```c
CF_API const cf_def* cf_defset_nearest(const cf_defset* ds, const char* key, size_t keylen, size_t* distance);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_defset_nearest` (lib/config/src/core/define.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/define.c#L994).

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

Implementation: [`cf_policy_text` (lib/config/src/core/validate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/validate.c#L679).

Renders a policy list as the specification writes it: "env > local".
Shared because both the rejection message and the coherence check
below want the same spelling, and two renderings of one thing drift.
Guarantee: nothrow. Measure-or-write as cf_fmt: the text is cut where `cap` ends,
a NULL `buf` stores nothing, and a NULL or empty policy gives "".

```c
CF_NODISCARD CF_API cf_bool cf_defs_apply(cf_keyspace* ks, const cf_defset* ds, const cf_rank_names* names, cf_unknown_policy    unknown, cf_apply_report* report, cf_error* err);
```

Implementation: [`cf_defs_apply` (lib/config/src/core/validate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/validate.c#L1128).

Applies the definitions to the key space: types, defaults, policy, lists, arrays.
Guarantee: basic. `ks` stays valid but may be applied up to the key that failed
(the caller drops it, as the builder does); `ds` is untouched, `report` is zeroed
first, and a NULL `ks` or `ds` fails with `err` set before anything is touched.

```c
CF_NODISCARD CF_API cf_bool cf_source_defaults(struct cf_source* s, const char* name, int rank, const cf_defset* ds);
```

Implementation: [`cf_source_defaults` (lib/config/src/sources/defaults.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/defaults.c#L90).

Definition defaults, as an ordinary source. Declared here because
it is the one source that reads a definition set; it is implemented
with the others.

A default is a value from the lowest-precedence source rather than a
separate mechanism, which is why explain() shows one losing to a
file with no special case anywhere - and why it has a name to print
rather than a question mark.
Guarantee: strong. A NULL `s` fails before anything is touched; otherwise `s` is
set whole. The definitions are only referred to.

```c
CF_API void cf_source_env_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_env_defs` (lib/config/src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/env.c#L92).

Give the environment source the key list, so it accepts the natural
single-underscore spelling as well as the escaped one. Without a
contract only the escaped spelling can be enumerated unambiguously;
with one, both work and nobody has to type the escape.
Guarantee: nothrow. Stores the pointer; a NULL `s` is ignored.

```c
CF_API void cf_source_dotenv_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_dotenv_defs` (lib/config/src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dotenv.c#L168).

Give a .env file the same natural names and list separators.
Guarantee: nothrow. As cf_source_env_defs.

```c
CF_API void cf_source_file_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_file_defs` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L605).

Give a file, or a directory of them, the list separators, for a format
that carries no types (cf_flatten_lists): a declared list splits an
INI value as it splits the environment's. (1.2)
Guarantee: nothrow. As cf_source_env_defs.

```c
CF_API void cf_source_cli_defs(struct cf_source* s, const cf_defset* ds);
```

Implementation: [`cf_source_cli_defs` (lib/config/src/sources/cli.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/cli.c#L113).

Give the command-line source the key list, which is what unlocks
--key value, --no-key and aliases. Without it only the = form carries
a value, because nothing else can be parsed correctly.
Guarantee: nothrow. As cf_source_env_defs.

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

Implementation: [`cf_doctor_new` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2320), [`cf_doctor_free` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2330).

A context, or NULL when there is no memory for one.

```c
CF_DOCTOR_API cf_bool cf_doctor_accept(cf_doctor* d, const char* rule, const char* key);
CF_DOCTOR_API cf_bool cf_doctor_baseline(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_accept` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2380), [`cf_doctor_baseline` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2392).

A finding already accepted - "CFD001", "server.port" - so it is
counted and not reported. cf_doctor_baseline reads them from a file,
one per line, as `doctor --baseline` does; a missing file is an empty
baseline.

```c
CF_DOCTOR_API cf_bool cf_doctor_severity(cf_doctor* d, const char* rule, const char* level, const char* profile, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_severity_file(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_severity` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2400), [`cf_doctor_severity_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2423).

Where a project disagrees with a rule's default: `level` is "error",
"warning", "info" or "off", for every profile or only `profile`.
cf_doctor_severity_file reads a `doctor --severity` file. Neither
takes effect for findings already recorded.

```c
CF_DOCTOR_API cf_bool cf_doctor_lint(cf_doctor* d, const cf_config* c, const char* profile, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_report(cf_doctor* d, const char* json, size_t n, const char* name, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_lint_file(cf_doctor* d, const char* path, cf_error* err);
```

Implementation: [`cf_doctor_lint` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2501), [`cf_doctor_lint_report` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2470), [`cf_doctor_lint_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2486).

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

Implementation: [`cf_doctor_only_changed_file` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2344), [`cf_doctor_unchanged_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2370).

`doctor lint --base B --only-changed`: from now on a lint reports only
what a change touched - the keys whose candidates differ from the
report in `path`, or that only one of the two has, and the sources
those came from - so a large configuration can adopt Doctor one change
at a time. The rest are counted (cf_doctor_unchanged_count), not
recorded. A later call replaces the base. (1.2)

```c
CF_DOCTOR_API void cf_doctor_redact_all(cf_doctor* d, cf_bool on);
```

Implementation: [`cf_doctor_redact_all` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2374).

From now on, nothing this context reports shows the value of any
key, as if each were sensitive: a finding says that a value
changed, was refused or came from somewhere, not what it is.
Comparisons still use the values a full report carries. For
output that goes where the configuration should not, such as a
pull-request comment.

```c
CF_DOCTOR_API cf_bool cf_doctor_diff(cf_doctor* d, const cf_config* a, const cf_config* b, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_report(cf_doctor* d, const char* a, size_t an, const char* aname, const char* b, size_t bn, const char* bname, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_diff_files(cf_doctor* d, const char* a, const char* b, cf_error* err);
```

Implementation: [`cf_doctor_diff` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2555), [`cf_doctor_diff_report` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2517), [`cf_doctor_diff_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2537).

Compare two: what one has and the other lacks, values from sources
of different kinds, overrides in one and not the other, and every
value that differs - `doctor diff`.

```c
CF_DOCTOR_API cf_bool cf_doctor_history_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);
CF_DOCTOR_API cf_bool cf_doctor_blame_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);
```

Implementation: [`cf_doctor_history_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2630), [`cf_doctor_blame_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2650).

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
CF_DOCTOR_API cf_bool cf_doctor_drift_files(cf_doctor* d, const char* const* paths, size_t n, const char* key, cf_error* err);
```

Implementation: [`cf_doctor_drift_files` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2766).

A set of reports, in no order - one host's report each (1.9.0). For
each key on which they do not all agree, cf_doctor_drift_files adds a
warning finding, rule "DRIFT", whose message groups the reports by
what they hold: the values, or only "value 1", "value 2" for a secret
or under cf_doctor_redact_all, and "absent from" the reports that lack
the key. A set whose reports cannot all be compared for a key, a digest
on one side and none of its kind on the other, gets an info finding,
rule "CFD020", for that key instead. The source of a value is not
compared. `key` limits it to one key, NULL for every key. False with
`err` set, and nothing found, when a report cannot be read, there are
fewer than two reports or more than 1024, `key` is in none of them, or
there is no memory to record the result.

```c
CF_DOCTOR_API size_t cf_doctor_count(const cf_doctor* d);
CF_DOCTOR_API const cf_finding* cf_doctor_finding(cf_doctor* d, size_t i);
```

Implementation: [`cf_doctor_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2674), [`cf_doctor_finding` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2678).

The findings so far, in the order they were found.

```c
CF_DOCTOR_API size_t cf_doctor_count_at(const cf_doctor* d, const char* level);
```

Implementation: [`cf_doctor_count_at` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2693).

How many at a level - "error", "warning", "info" - or "accepted", for
the ones the baseline accepted.

```c
CF_DOCTOR_API size_t cf_doctor_stale_count(const cf_doctor* d);
CF_DOCTOR_API cf_bool cf_doctor_stale(const cf_doctor* d, size_t i, const char** rule, const char** key);
```

Implementation: [`cf_doctor_stale_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2712), [`cf_doctor_stale` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2726).

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

Implementation: [`cf_doctor_rule_count` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2747), [`cf_doctor_rule_id` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2751), [`cf_doctor_rule_text` (tools/doctor/engine.c)](https://github.com/develooper1994/configforge/blob/main/tools/doctor/engine.c#L2755).

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

Implementation: [`cf_edit_begin` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L32).

A change to `target`, a source of `c` by name. The edit keeps its own
reference to `c`. NULL, with `err` saying why, when `c` or `target`
is NULL or there is no memory - an edit allocates from the allocator the
configuration was built with, and from the default one for a
configuration built in a fixed buffer, which has none. Whether the
source exists, can be written and may be, is asked at commit with the
rest, so every refusal comes from one place.
Guarantee: strong. The checks come before anything is made: on NULL, `c` is
untouched and no edit exists; on success `c` gains one reference, which
commit or abort gives back.

```c
CF_NODISCARD CF_API cf_bool cf_edit_set_text(cf_edit* e, const char* key, const char* value);
CF_NODISCARD CF_API cf_bool cf_edit_set_int(cf_edit* e, const char* key, cf_i64 value);
CF_NODISCARD CF_API cf_bool cf_edit_set_bool(cf_edit* e, const char* key, cf_bool value);
CF_NODISCARD CF_API cf_bool cf_edit_set_real(cf_edit* e, const char* key, double value);
```

Implementation: [`cf_edit_set_text` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L116), [`cf_edit_set_int` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L136), [`cf_edit_set_bool` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L146), [`cf_edit_set_real` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L157).

Set a key: as text, which the contract types as it types a file's, or
already typed. A key set twice keeps the second. False for no memory
or a key that is not one, which commit then reports.
Guarantee: basic. The edit stays valid: the first refusal is held for commit, the
keys set before it stay set, and cf_edit_abort drops it. The target is not read
or written until commit.

```c
CF_NODISCARD CF_API cf_bool cf_edit_unset(cf_edit* e, const char* key);
```

Implementation: [`cf_edit_unset` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L168).

Delete a key from the target, so that the next source down - a file,
the defaults - supplies it again.
Guarantee: basic. As cf_edit_set_text.

```c
CF_API size_t cf_edit_count(const cf_edit* e);
```

Implementation: [`cf_edit_count` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L178).

How many keys the edit changes.
Guarantee: nothrow. 0 for a NULL edit.

```c
CF_NODISCARD CF_API cf_bool cf_edit_commit(cf_edit* e, const char* note, cf_error* err);
```

Implementation: [`cf_edit_commit` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L190).

Check and write the whole change, `note` (who, when, why) beside each
value. Consumes the edit either way.
Guarantee: strong. Nothing is written until the whole document is made; a
failure leaves the target as it was and the error is the first refusal, or
the check's. A NULL edit fails with `err` set.

```c
CF_API void cf_edit_abort(cf_edit* e);
```

Implementation: [`cf_edit_abort` (lib/config/src/core/edit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/edit.c#L182).

Drop the edit; nothing is written.
Guarantee: nothrow. A NULL edit is accepted; the edit is consumed.

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

Implementation: [`cf_emit` (lib/config/src/bridge/emit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/emit.c#L232).

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
Guarantee: strong. The configuration is untouched; a refusal returns 0 with
`err` set and an empty `buf`, and the size needed is returned otherwise.

```c
CF_NODISCARD CF_API cf_bool cf_emit_file(const struct cf_config* c, const char* path, const char* format, cf_emit_what what, int rank, const cf_file_options* o, cf_error* err);
```

Implementation: [`cf_emit_file` (lib/config/src/bridge/emit.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/emit.c#L348).

cf_emit, then cf_file_replace: `path` holds the whole document or keeps its
old bytes. `format` NULL: cf_format_of_path(path), and a name it cannot tell
is CF_E_CAPABILITY. `o` NULL: the defaults of cf_file_replace (CF_SYNC_COMMIT,
one sync a save). A refusal of cf_emit keeps its own message and writes
nothing; a failed write is cf_file_replace's `cannot write '%s': &lt;reason>`.
Returns CF_TRUE when the file holds the document.
Guarantee: strong. A failure leaves `path` as it was and `c` untouched; it
returns CF_FALSE with `err` set.

## cf_encoding.h

cf_encoding.h - another encoding, read only by name (D-108).

Every text inside the library is UTF-8. A file in another encoding is
read when the program names the encoding: a CF_ENCODING_* value in the
flags of its source, or its name in the tools and the bindings. The
bytes are recoded to UTF-8 as they are read. A byte-order mark is a
declaration, not a guess: UTF-16's, in either order, makes the file
UTF-16.

Public, so that the serialization library's own program reaches it as
the configuration library does.

```c
CF_NODISCARD CF_API cf_bool cf_encoding_by_name(const char* name, unsigned* flags);
```

Implementation: [`cf_encoding_by_name` (lib/text/src/encoding.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/encoding.c#L114).

CF_TRUE and *flags the encoding's value for one of: "utf-8",
"iso-8859-1" or "latin1", "windows-1252" or "cp1252",
"windows-1254" or "cp1254", "iso-8859-9" or "latin5",
"utf-16le", "utf-16be"; ASCII case ignored. CF_FALSE otherwise.
Guarantee: strong. `flags` is written only when the name is known.

```c
CF_API const char* cf_encoding_name(unsigned flags);
```

Implementation: [`cf_encoding_name` (lib/text/src/encoding.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/encoding.c#L129).

The canonical name ("windows-1254") of the encoding in `flags`,
or NULL for a value above CF_ENCODING_LAST.
Guarantee: nothrow. NULL for a value above CF_ENCODING_LAST.

```c
CF_API size_t cf_encoding_recode(unsigned from, const char* in, size_t n, char* out, size_t cap, size_t* bad);
```

Implementation: [`cf_encoding_recode` (lib/text/src/encoding.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/encoding.c#L222).

Recodes n bytes of `from` (any value but CF_ENCODING_UTF8) into
UTF-8 at out, which holds cap bytes. Returns the bytes written, or
(size_t)-1 with *bad the offset of the first byte of what has no
character: Windows-1252's 0x81, 0x8D, 0x8F, 0x90, 0x9D;
Windows-1254's those and 0x8E, 0x9E; an unpaired UTF-16
surrogate; a last half code unit. Enough room: 3 * n for the
single-byte encodings (0x80 to 0x9F of the Windows pages are
mostly U+20xx, three bytes), 3 * (n / 2) for UTF-16. With less
room it returns (size_t)-1 with *bad == n, an offset no byte has,
having written nothing past cap.
Guarantee: strong. Only `out` and `*bad` are written; `out` is unspecified after a
failure, and nothing is written past `cap`.

```c
CF_NODISCARD CF_API cf_bool cf_encoding_check(const char* origin, const char* p, size_t n, cf_error* err);
```

Implementation: [`cf_encoding_check` (lib/text/src/encoding.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/encoding.c#L293).

CF_TRUE when the n bytes at p are UTF-8. Otherwise CF_FALSE, and
err set (CF_E_SOURCE) to
"&lt;origin>:&lt;line>:&lt;column>: byte 0x&lt;XX> is not valid UTF-8&lt;hint>",
the place of the first byte that is not, counted as every reader
counts (lines by LF, columns in bytes from 1). The hint, when the bytes
that are not part of a valid sequence look like another encoding and
the input has no multi-byte UTF-8 of its own, says which and how to
read it so: "; it may be windows-1254 (Turkish): read it with
CF_ENCODING_WINDOWS_1254, or the encoding windows-1254 in the tools".
It never changes a value. A NULL origin is "input". (D-108)
Guarantee: strong. The input is never changed; on a refusal only `err` is written.

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

Implementation: [`cf_keymap_init` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L183).

Guarantee: nothrow. A NULL map is ignored.

```c
CF_API size_t cf_keymap_to_key(const cf_keymap* m, const char* raw, size_t n, char* out, size_t cap);
```

Implementation: [`cf_keymap_to_key` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L191).

Source spelling to canonical. Returns the length written, or 0 when
the name does not belong to this source or does not derive a valid
key. Skipped rather than repaired: PATH and HOME land here.
Guarantee: nothrow. 0 for a NULL argument, a name that does not belong or derives
no valid key, or a `cap` too small; `out` is the caller's.

```c
CF_API size_t cf_keymap_from_key(const cf_keymap* m, const char* key, size_t n, cf_bool escaped, char* out, size_t cap);
```

Implementation: [`cf_keymap_from_key` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L232).

Canonical to source spelling. `escaped` selects between the form the
mapping can read back unaided and the natural one a person writes:

  server.tls_min_version
    escaped -> MYAPP_SERVER__TLS_MIN_VERSION   enumerable
    natural -> MYAPP_SERVER_TLS_MIN_VERSION    ambiguous alone,
                                               unambiguous with a
                                               definition to match
Both are accepted when a contract exists, which is the whole gain:
the escape is what enumeration needs, and nobody has to type it.
Guarantee: nothrow. 0 for a NULL argument or a `cap` too small; `out` is the
caller's.

### Entry sink

```c
CF_NODISCARD CF_API cf_bool cf_keysink_init(cf_keysink* k, cf_keyspace* ks, int rank);
```

Implementation: [`cf_keysink_init` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L304).

Guarantee: strong. A NULL sink or key space fails with the key space as it was.

```c
CF_API const cf_entry_sink* cf_keysink_of(cf_keysink* k);
```

Implementation: [`cf_keysink_of` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L318).

Guarantee: nothrow. NULL for a NULL sink.

```c
CF_NODISCARD CF_API cf_bool cf_listsink_init(cf_listsink* out, cf_keyspace* ks, int rank, const struct cf_defset* defs, cf_bool repeated);
```

Implementation: [`cf_listsink_init` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L144).

Guarantee: strong. A NULL `out` or key space fails with the key space as it was;
`out` is then empty.

```c
CF_API const cf_entry_sink* cf_listsink_of(cf_listsink* out);
```

Implementation: [`cf_listsink_of` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L161).

Guarantee: nothrow. NULL for a NULL sink.

```c
CF_NODISCARD CF_API cf_bool cf_listsink_finish(cf_listsink* out);
```

Implementation: [`cf_listsink_finish` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L165).

Guarantee: basic. The sink and the key space stay valid; the counts of some lists
may be recorded, so the caller drops them. False for a NULL sink or one that
already failed.

### Lexers

No arena, no key space, no source object. Everything these need is
an argument, which is what makes them testable against an array.

```c
CF_NODISCARD CF_API cf_bool cf_lex_env(char* const* environment, const cf_keymap* map, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_env` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L326).

Entries that do not belong to this prefix, or do not derive a valid
key, are skipped. An environment is shared with every other program
on the machine, so refusing it would be refusing the machine.
Guarantee: basic. `err` is set and the sink stays valid; the entries read before
the failure stay recorded in it, so the caller drops what it records into.

```c
CF_API const char* cf_env_value(char* const* environment, const char* name, size_t* n);
```

Implementation: [`cf_env_value` (lib/config/src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/env.c#L51).

One variable's value, borrowed from the environment, and its length in
`n` (which may be NULL); NULL when it is not set - an empty value is
"" and set. The first of two with one name, as getenv takes it.
Guarantee: nothrow. NULL for a NULL environment or name, and for a variable that is
not set.

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
CF_NODISCARD CF_API cf_bool cf_lex_cli(int argc, char* const* argv, const cf_keymap* map, const cf_cli_spec* spec, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_cli` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L400).

`spec` may be NULL, which is Tier 0.

A malformed option is rejected rather than skipped. A command line
belongs to this program alone, so an unparsable option is the user's
typo, and ignoring it is how a misspelled flag becomes an hour of
confusion.

Guarantee: basic. `err` is set and the sink stays valid; the entries read before
the failure stay recorded in it, so the caller drops what it records into.

### Response files

myapp @args.txt --port=8080

One argument per line, `#` comments, blank lines skipped. Accepted
because the Windows command-line length limit is real and this is the
universal way around it.

Nothing runs this on its own: cf_app and cf_app_builder leave an
argument that starts with @ alone, and a program turns expansion on
with cf_builder_response_files (D-79).

Expansion is a separate pass rather than something the lexer does, so
the lexer keeps needing nothing but its arguments. Nesting is
refused: a response file that includes another is a command line
nobody can follow from the one they typed, and one that includes
itself is a loop.

```c
CF_NODISCARD CF_API cf_bool cf_cli_expand(int argc, char* const* argv, cf_arena* arena, int* out_argc, char*** out_argv, cf_error* err);
```

Implementation: [`cf_cli_expand` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L564).

Guarantee: basic. The arena stays valid and keeps the copies made so far until it
is released; `*out_argc` and `*out_argv` are then unspecified and `err` is set.

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

Implementation: [`cf_cli_args` (lib/config/src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/compat.c#L28).

Guarantee: basic. The arena may keep the arguments converted so far until it is
released; when the command line cannot be converted the result is `argv` and
`*out_argc` is `argc`. `out_argc` must not be NULL.

### A .env file

# comment
  KEY=value
  export KEY=value        the shell convention, accepted and ignored
  KEY="quoted value"

The same shape as the environment, which is the point: a .env file is
an environment written down. It takes the same key mapping, so the
escape and the natural spelling behave identically in both.

```c
CF_NODISCARD CF_API cf_bool cf_lex_dotenv(const char* text, size_t n, const char* origin, const cf_keymap* map, const cf_entry_sink* out, cf_error* err);
```

Implementation: [`cf_lex_dotenv` (lib/config/src/sources/entry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/entry.c#L666).

Guarantee: basic. `err` is set and the sink stays valid; the entries read before
the failure stay recorded in it, so the caller drops what it records into.

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

### Keys

Canonical form is dotted, lowercase ASCII, and at most CF_KEY_MAX - 1
bytes, so that a key and its terminator fit the buffers sized by the
limit. Comparison is byte-wise and case-sensitive: case-insensitive
keys are Viper's most-reported defect, because they silently collapse
two distinct parameters. Derivation to MYAPP_SERVER_PORT and
--server-port belongs to the source that needs it, not here.

```c
CF_NODISCARD CF_API cf_bool cf_key_valid(const char* key, size_t len);
```

Implementation: [`cf_key_valid` (lib/base/src/key.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/key.c#L11).

Guarantee: strong. A pure check; CF_FALSE says the key is not canonical.
Thread: safe.
Signal: safe.
Alloc: none.

```c
CF_API void cf_error_clear(cf_error* e);
```

Implementation: [`cf_error_clear` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L233).

Empties `e`: code CF_OK, key, source, message, place and reason empty, and
frees a long text it owned. `size` is kept, so a cleared error can be
reused. NULL is ignored.
Guarantee: nothrow.

```c
CF_API void cf_error_copy(cf_error* dst, const cf_error* src);
```

Implementation: [`cf_error_copy` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L244).

What `src` says, copied into `dst`: 1.0's fields always, a later field
where dst->size covers it. The texts are copied into `dst`'s own store
(a long one into a block `dst` owns), never pointed at in `src`. If
src->size does not cover a later field, that field is cleared. dst->size
is kept. Safe with NULL on either side.
Guarantee: nothrow. A text that needs a block and gets none is cut to the
old array's size; a NULL side is ignored.

```c
CF_API cf_bool cf_error_set(cf_error* e, cf_code code, const char* key, const char* source, const char* fmt, ...);
```

Implementation: [`cf_error_set` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L265).

Set code, key, source and a formatted message, clearing place and reason.
Safe with a NULL
error, so every call site can report unconditionally instead of
guarding. Returns CF_FALSE so a caller can write
`return cf_error_set(err, ...);`
The key is cut at CF_KEY_MAX - 1 bytes, the source at CF_SOURCE_MAX - 1
and the message at CF_MESSAGE_MAX - 1, whatever the sizing mode: what
cf_error_set writes is what it always wrote. cf_error_set_long is the
call that keeps more.
Guarantee: nothrow. The texts are cut to fit; a NULL `e` is ignored.

```c
CF_API cf_bool cf_error_set_long(cf_error* e, cf_code code, const char* key, const char* source, size_t cap, const char* fmt, ...);
```

Implementation: [`cf_error_set_long` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L292).

As cf_error_set, with each of the three texts allowed `cap` bytes; 0 means
no cap (the sizing mode's cap, from cf_config_sizing, when the caller knows
one). A text past the old array's size goes to a block the error owns. A
build with CF_FIXED_LIMITS never allocates, and when an allocation fails the
text is cut, as cf_error_set cuts it. Returns CF_FALSE.
Guarantee: nothrow. The texts are cut to `cap` and to what can be
allocated; a NULL `e` is ignored.

```c
CF_API const char* cf_code_name(cf_code c);
```

Implementation: [`cf_code_name` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L19).

Guarantee: nothrow. "?" for an unknown code.

```c
CF_API const char* cf_code_description(cf_code c);
```

Implementation: [`cf_code_description` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L30).

Guarantee: nothrow. "unknown" for an unknown code.

```c
CF_API const char* cf_reason_name(int reason);
```

Implementation: [`cf_reason_name` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L41).

The name ("number-range") and description of a reason. Returns "?" and
"unknown", respectively, for an unknown value.
Guarantee: nothrow.

```c
CF_API const char* cf_reason_description(int reason);
```

Implementation: [`cf_reason_description` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L88).

Guarantee: nothrow.

```c
CF_API const char* cf_reason_severity(int reason);
```

Implementation: [`cf_reason_severity` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L74).

Whether a refusal is "error" (the input is malformed by its format's own
rules), "warning" (well formed, refused for a limit or a policy) or "none";
and the CWE it defends, such as "CWE-674", or "" for none. "?" and "" for
an unknown value. Promoted in 2.0.
Guarantee: nothrow.

```c
CF_API const char* cf_reason_weakness(int reason);
```

Implementation: [`cf_reason_weakness` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L81).

Guarantee: nothrow.

```c
CF_API void cf_error_place(cf_error* e, int reason, unsigned int line, unsigned int col);
```

Implementation: [`cf_error_place` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L114).

Sets where (file NULL, line, col) and reason on an error whose message
is already set, where e->size reaches them. NULL is ignored.
Guarantee: nothrow.

```c
CF_API void cf_error_place_at(cf_error* e, int reason, unsigned int line, unsigned int col, size_t offset);
```

Implementation: [`cf_error_place_at` (lib/base/src/error.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/error.c#L101).

As cf_error_place, and the byte offset of the cause in the input (the
index of the byte, from 0) where e->size reaches it; (size_t)-1 says the
reader has none. cf_error_place leaves it (size_t)-1. Promised since 2.1
(experimental in 2.0, D-89).
Guarantee: nothrow.

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_explain` (lib/config/src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/explain.c#L153).

The low-level renderers. The public API in configforge.h wraps
these; these take a key space and a source list directly, which is
what the tests and the Doctor tool need.

```c
CF_API size_t cf_ks_inspect(const cf_keyspace* ks, const cf_source_view* sv, char* buf, size_t cap);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_inspect` (lib/config/src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/explain.c#L158).

Render every key. Sources that never win appear in the trailing
summary, which is what makes a dead source visible.

```c
CF_API size_t cf_ks_explain_redacted(const cf_keyspace* ks, const cf_source_view* sv, const cf_defset* ds, const char* key, size_t keylen, char* buf, size_t cap);
CF_API size_t cf_ks_inspect_redacted(const cf_keyspace* ks, const cf_source_view* sv, const cf_defset* ds, char* buf, size_t cap);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_explain_redacted` (lib/config/src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/explain.c#L163), [`cf_ks_inspect_redacted` (lib/config/src/diag/explain.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/explain.c#L186).

The same two, with every value of a key that `ds` marks CF_SENSITIVE
shown as "&lt;sensitive>", and as "&lt;sensitive, N bytes>" when it is text:
what a blind report gives, and nothing more. The chain - which source
won, which lost, and where each was set - is shown in full, because
that is the question and it is not the secret.

cf_explain and cf_inspect pass the configuration's own definitions. A
diagnostic is where a secret is most often pasted: into a ticket, a
chat, a log somebody else reads. The two above are the raw renderers
for a key space with no definitions.

## cf_file.h

cf_file.h - the file layer: a file replaced whole or not at all, and a
writer for a program that writes continuously (D-224, D-127).

It knows nothing of configuration and any program can use it alone:
a write that goes through here is never half a file that the next
start reads. Three layers, each usable without the one above:

  cf_file_options  how a file is written: buffering, durability, and the
                   wait of a rename on Windows. Every setting has a
                   default, and a NULL `options` anywhere below is those
                   defaults.
  cf_file_writer   streaming, in place, no temporary: records, logs.
  cf_file_replace  a file made to hold exactly the bytes given, by a
                   temporary in the same directory, synced once and put
                   in place by one rename.

(The third, cf_emit_file, writes a configuration and is cf_emit.h's.)

Buffering is C's setvbuf, and durability is the platform's own sync
(fsync, FlushFileBuffers). Paths are UTF-8 everywhere (D-15). A build
for the `none` platform has no files to write: every call but
cf_file_options_init fails with CF_E_CAPABILITY, `this build has no
files to write`, and a program built for it still links.

```c
CF_API void cf_file_options_init(cf_file_options* o);
```

Implementation: [`cf_file_options_init` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L30).

Sets every member to its default and `size`. A NULL `o` does nothing, and
a NULL `options` in any call below is these defaults. A `size` below the
members above is CF_E_SOURCE, `file options of an unknown size`.
`sync_ms` needs a clock: a platform with none refuses a writer that asks
for CF_SYNC_EVERY with sync_ms != 0 (CF_E_CAPABILITY, `this platform has
no clock for sync_ms`). A writer, and its `buffer_size` above 0, are taken
from `allocator` (default: cf_default_allocator()) and released at the
close; a build with no allocator refuses it (CF_E_CAPABILITY).
Guarantee: nothrow.

### The writer: streaming, in place

cf_file_writer_open opens `path` for writing (truncated, or appended
to with options->append) - no temporary: a crash leaves what was
written. A failure of any call is CF_E_SOURCE, `cannot write '%s':
&lt;reason>`, source = path. One writer, one thread (D-128).

```c
CF_NODISCARD CF_API cf_file_writer* cf_file_writer_open(const char* path, const cf_file_options* o, cf_error* err);
```

Implementation: [`cf_file_writer_open` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L220).

Guarantee: basic. On failure no writer exists and nothing leaks; the file may
have been created or truncated.

```c
CF_NODISCARD CF_API cf_bool cf_file_writer_write(cf_file_writer* w, const void* data, size_t size, cf_error* err);
```

Implementation: [`cf_file_writer_write` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L228).

Guarantee: basic. The writer stays valid; some of the bytes may be on disk.

```c
CF_NODISCARD CF_API cf_bool cf_file_writer_flush(cf_file_writer* w, cf_error* err);
```

Implementation: [`cf_file_writer_flush` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L237).

To the system, not forced to the device.
Guarantee: basic. The writer stays valid; bytes may be on disk.

```c
CF_NODISCARD CF_API cf_bool cf_file_writer_sync(cf_file_writer* w, cf_error* err);
```

Implementation: [`cf_file_writer_sync` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L243).

Flush, then forced to the device.
Guarantee: basic. The writer stays valid; bytes may be on disk.

```c
CF_NODISCARD CF_API cf_bool cf_file_writer_close(cf_file_writer* w, cf_error* err);
```

Implementation: [`cf_file_writer_close` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L249).

The last flush, the sync the policy asks for, the close. Always frees
`w`, whether it returns true or not; NULL is fine.
Guarantee: basic. `w` is released whether it fails or not; bytes may be on disk.

### The replace: a file whole or not at all

True when `path` holds exactly `size` bytes of `data`; false with `err`
set (CF_E_SOURCE, source = path) and the old file, or its absence, as it
was, and no temporary left. The temporary is `.&lt;name>.cf-tmp-&lt;k>` in the
same directory, written, synced (by default: see CF_SYNC_DEFAULT) and put
in place by one rename. On Windows a rename fails while another handle
holds the target without delete sharing, and is retried for `retry_ms`.

A symbolic link is refused (name the file it points to), as is a path
that is not a regular file and, on Windows, a read-only one. The mode of
the file replaced is kept on POSIX; on Windows no attribute or ACL is
copied and the new file takes the directory's.

```c
CF_NODISCARD CF_API cf_bool cf_file_replace(const char* path, const void* data, size_t size, const cf_file_options* o, cf_error* err);
```

Implementation: [`cf_file_replace` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L153).

Guarantee: strong. On failure the target is as it was and no temporary is left.

## cf_flatten.h

cf_flatten.h - document events to and from the flat key space.

INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
outside the 1.x promise - a minor release may change it. Its functions
are declared only under CF_INTERNAL_API (D-51, D-88). A program uses
the configforge.h functions instead.

The bridge between the serialization layer and the configuration
model. It lives on this side of the boundary on purpose: the
serialization layer may use the leaf types and may not touch a key
space, which is what keeps extracting it later a directory move.

```c
CF_NODISCARD CF_API cf_bool cf_flatten_init(cf_flatten* f, cf_keyspace* ks, int rank);
CF_API const cf_doc_sink* cf_flatten_sink(cf_flatten* f);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_flatten_init` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L409), [`cf_flatten_sink` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L432).

```c
CF_API void cf_flatten_lists(cf_flatten* f, const struct cf_defset* ds);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_flatten_lists` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L436).

For a format that carries no types - INI, .properties - whose values
the definitions decide: a declared list with a separator splits its
text value, `ports = 80,443` the elements 80 and 443. (1.2)

```c
CF_API size_t cf_decode_budget(size_t n);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_decode_budget` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L442).

CF_DECODE_FACTOR times n, and CF_DECODE_FLOOR more, saturated at
SIZE_MAX rather than wrapped.

```c
CF_API void cf_flatten_budget(cf_flatten* f, size_t bytes);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_flatten_budget` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L449).

Bound what the flattener adds from now on to `bytes`, counted as
D-82 counts them; 0 is no bound.

```c
CF_NODISCARD CF_API cf_bool cf_flatten_decode(cf_flatten* f, const cf_codec* codec, const cf_blob* blob, const char* path, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_flatten_decode` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L460).

`blob` through `codec` into the flattener's key space, bounded by
cf_decode_budget(blob->n) unless a budget is set already. False
with `err` set: the codec's reason, or, when the budget refused a
key, the budget's, naming `path` - whatever the codec said, since
a refusal from the sink reads to it as a key it could not add.

```c
CF_NODISCARD CF_API cf_bool cf_source_read_blob(const struct cf_source* s, cf_keyspace* ks, const cf_codec* codec, const cf_blob* blob, const char* path, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_source_read_blob` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L90).

A document's bytes through a codec into the key space, at the source's
rank, the source's definitions deciding lists for a format without
types: what the file and directory sources do with each file, for a
source of one's own that has bytes to read. False with `err` set.

```c
CF_API const cf_codec* cf_source_by_content(const cf_codec* const* own, size_t n, const cf_blob* blob, const char* path, cf_arena* scratch, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_source_by_content` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L157).

The codec the content of `blob` says (D-73; cf_codec_detect), `own`
(n of them) a program's codecs looked at first; NULL with `err` set,
naming `path`, when the bytes are empty, read by no format, or read
differently by two loose ones. `scratch` is the trial readers' work
space (cf_blob_arena). What the file and directory sources do for a
file whose name gives no format.

```c
CF_NODISCARD CF_API cf_bool cf_doc_emit(const cf_keyspace* ks, const cf_doc_sink* out, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_doc_emit` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L909).

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
CF_NODISCARD CF_API cf_bool cf_doc_emit_where(const cf_keyspace* ks, const cf_doc_sink* out, cf_emit_pred pred, const void* ctx, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_doc_emit_where` (lib/config/src/bridge/flatten.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/flatten.c#L913).

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

Implementation: [`cf_image_write` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L196).

Write the image. Measure-or-write, like everything else that renders:
with a NULL buffer nothing is stored and the length still
accumulates.
Guarantee: strong. The key space is only read; the size needed is returned
(0 for a NULL one) and `buf` is the caller's.

### Reading

The image is used in place. Nothing is copied, nothing is allocated,
and the buffer must outlive the view - which is exactly the point on
a target where the buffer is a `const char[]` in flash.

```c
CF_NODISCARD CF_API cf_bool cf_image_open(cf_image* img, const char* buf, size_t n, cf_error* err);
```

Implementation: [`cf_image_open` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L375).

Validates the header, the bounds and the digest. Refuses rather than
guesses: a truncated image and a corrupt one look the same from
inside, and neither should produce a configuration.
Guarantee: basic. `img` is always valid: a refusal leaves it zeroed, an empty
image, so a caller that ignores the result iterates nothing; `err` says why.

```c
CF_API size_t cf_image_count(const cf_image* img);
```

Implementation: [`cf_image_count` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L491).

Guarantee: nothrow. 0 for a NULL image.

```c
CF_NODISCARD CF_API cf_bool cf_image_at(const cf_image* img, size_t i, const char** key, size_t* klen, cf_value* v);
CF_NODISCARD CF_API cf_bool cf_image_get(const cf_image* img, const char* key, size_t klen, cf_value* v);
```

Implementation: [`cf_image_at` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L531), [`cf_image_get` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L566).

Guarantee: strong. A read of the image; false for an index past the end, a NULL
image or a record that points outside it, and then `key`, `klen` and `v` are
unspecified.

```c
CF_NODISCARD CF_API cf_bool cf_image_load(const cf_image* img, cf_keyspace* ks, int rank, cf_error* err);
```

Implementation: [`cf_image_load` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L641).

Load an image into a key space at a rank, which is what a source
does. Text values point into the image, so it must outlive the
configuration.
Guarantee: basic. The key space stays valid and may hold the keys added before
a refusal; the image is only read, and `err` says why.

### As a source

```c
CF_API size_t cf_source_image_state_size(void);
```

Implementation: [`cf_source_image_state_size` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L767).

Guarantee: nothrow. A size.

```c
CF_NODISCARD CF_API cf_bool cf_source_image_mem(cf_source* s, const char* name, int rank, const char* buf, size_t n, void* state, size_t state_size);
```

Implementation: [`cf_source_image_mem` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L787).

From memory: a build-time image linked into the program. No file, no
parser, no allocation - the freestanding path.
Guarantee: strong. The arguments are checked before `s` or `state` is touched;
CF_FALSE leaves both as they were. The image is read at load, not here.

```c
CF_NODISCARD CF_API cf_bool cf_source_image_file(cf_source* s, const char* name, int rank, const char* path, void* state, size_t state_size);
```

Implementation: [`cf_source_image_file` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L800).

From a file, for hosted targets.
Guarantee: strong. As cf_source_image_mem.

```c
CF_API const cf_error* cf_source_image_error(const cf_source* s);
```

Implementation: [`cf_source_image_error` (lib/config/src/bridge/image.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/image.c#L809).

Guarantee: nothrow. The load's error, or NULL when it did not fail (or `s` is not
an image source).

## cf_iterate.h

cf_iterate.h - walking a prefix.

The questions a program actually asks of a configuration are rarely
"what is server.port", which a plain get answers. They are "what are
the elements of this array" and "which plugins are configured", and
both needed the caller to build keys by hand and guess where to stop.

Both are a scan from one binary search, because the key space is
sorted. Nothing is allocated: the iterator is the caller's.

```c
CF_NODISCARD CF_API cf_bool cf_iter_keys(cf_iter* it, const struct cf_config* c, const char* prefix);
```

Implementation: [`cf_iter_keys` (lib/config/src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/iterate.c#L62).

Every key under `prefix`, in key order. That is byte order, not the
order a document had it: under an array "upstream.10" comes before
"upstream.2", so an array is read with cf_array_count and cf_array_key
below. A prefix matches at a segment boundary only, so "server" does
not match "servername" - the bug every hand-written prefix check has.
An empty or NULL prefix walks the whole configuration.
Guarantee: strong. Reads a finished configuration and changes nothing but `it`;
false for a NULL `it` or a configuration with no key space, with `it` then
empty, so cf_iter_next is false too.

```c
CF_NODISCARD CF_API cf_bool cf_iter_children(cf_iter* it, const struct cf_config* c, const char* prefix);
```

Implementation: [`cf_iter_children` (lib/config/src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/iterate.c#L66).

Each distinct immediate child of `prefix`, once, in key order:

    plugins.auth.enabled     -> auth
    plugins.auth.timeout
    plugins.cache.size       -> cache

This is how a program discovers what is configured rather than being
told in advance, which is what plugin and per-tenant configuration
both need.

Guarantee: strong. As cf_iter_keys.

```c
CF_NODISCARD CF_API cf_bool cf_iter_next(cf_iter* it);
```

Implementation: [`cf_iter_next` (lib/config/src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/iterate.c#L70).

Guarantee: strong. Changes nothing but `it`; false when the walk is done, and for
a NULL or empty `it`.

### Arrays

```c
CF_API size_t cf_array_count(const struct cf_config* c, const char* key);
```

Implementation: [`cf_array_count` (lib/config/src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/iterate.c#L124).

The recorded length, which is not the same as the number of index
keys present: an element below it may be missing. The length is also
where the array ends - the source that wins "&lt;key>.count" fixes it,
and an element at or past it, from any source, is not in the
configuration: it is not read, listed or written, and explain() shows
it as a "truncated" candidate. A count that is not an integer, or is
negative, fixes nothing, and nor does one that is a template.
Guarantee: nothrow. 0 for a NULL key and for a configuration with no length
recorded.

```c
CF_API size_t cf_array_key(char* buf, size_t cap, const char* key, size_t index);
```

Implementation: [`cf_array_key` (lib/config/src/core/iterate.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/iterate.c#L145).

Builds "&lt;key>.&lt;index>", so a loop does not have to. Measure-or-write
like everything else that renders.
Guarantee: nothrow. Measure-or-write, as cf_fmt: the text is cut where `cap` ends.

## cf_keyspace.h

cf_keyspace.h - the flat key space and the provenance chain.

The 1.x promise (D-51) covers what a source written by a program uses:
cf_key_valid, cf_ks_arena and the cf_ks_add* functions, called from a
source's load. The cf_keyspace struct's members, and every other
cf_ks_ function, are INTERNAL - exported for ConfigForge Doctor and the
tests, and free to change in a minor release. Those functions are
declared only under CF_INTERNAL_API (D-51, D-88).

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
CF_API size_t cf_key_segments(const char* key, size_t len);
```

Implementation: [`cf_key_segments` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L18).

Number of dot-separated segments, 0 when the key is invalid.
Guarantee: nothrow. 0 for an invalid key.

```c
CF_API int cf_key_cmp(const char* a, size_t alen, const char* b, size_t blen);
```

Implementation: [`cf_key_cmp` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L32).

Byte-wise, never strcoll: a reproducible snapshot hash depends on a
reproducible order.
Guarantee: nothrow.

### Provenance

```c
CF_API const char* cf_outcome_name(cf_outcome o);
```

Implementation: [`cf_outcome_name` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L36).

Guarantee: nothrow. "?" for a value that is no outcome.

```c
CF_NODISCARD CF_API cf_bool cf_outcome_can_win(cf_outcome o);
```

Implementation: [`cf_outcome_can_win` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L73).

Whether a candidate with this outcome may still be selected. The
resolver asks this instead of listing the losing outcomes, so a new
one cannot be added and forgotten in the one place it matters.
Guarantee: strong. A pure check; it changes nothing, and false is also the
answer for a value that is no outcome.

```c
CF_API const char* cf_refusal_name(cf_refusal k);
CF_API const char* cf_refusal_text(cf_refusal k);
```

Implementation: [`cf_refusal_name` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L61), [`cf_refusal_text` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L67).

The kind's short name ("range"), as the report writes it, and the fixed
wording a sensitive key's reason takes ("out of range"). "" and "" for
CF_REFUSAL_NONE.
Guarantee: nothrow. "" for a value that is no refusal kind; the comment covers
cf_refusal_text, which falls back the same way.

### Key space

```c
CF_NODISCARD CF_API cf_bool cf_ks_init(cf_keyspace* ks, cf_arena* arena, size_t hint);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_init` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L95).

```c
CF_API cf_arena* cf_ks_arena(const cf_keyspace* ks);
```

Implementation: [`cf_ks_arena` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L169).

The arena everything in this space lives in. Sources that have to
read a document into memory borrow it, so a configuration remains
one arena and one release.
Guarantee: nothrow. NULL for a NULL space.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L312).

Record a candidate. Keys and text are copied into the arena, so the
caller's buffers need not outlive the call. Candidates may arrive in
any order; the chain is kept ordered by precedence.
Guarantee: basic. The space stays valid and holds what was added before; a failed
call adds no candidate, and the arena keeps the copies it made until it is
released. False for a NULL or resolved space, an invalid key, or no room.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add_text(cf_keyspace* ks, const char* key, size_t keylen, int source, const char* text, size_t textlen, const char* at);
```

Implementation: [`cf_ks_add_text` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L317).

Record a candidate as written, leaving its type to be decided.
Guarantee: basic. As cf_ks_add.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add_authored(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add_authored` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L329).

Record a typed candidate that somebody wrote - a JSON string, say -
so the reserved syntax applies to it as it does to raw text.
Guarantee: basic. As cf_ks_add.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add_resolved(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at);
```

Implementation: [`cf_ks_add_resolved` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L480).

Guarantee: basic. As cf_ks_add.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add_default(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value);
```

Implementation: [`cf_ks_add_default` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L335).

Record a definition's default (CF_CAND_DEFAULT).
Guarantee: basic. As cf_ks_add.

```c
CF_NODISCARD CF_API cf_bool cf_ks_rename(cf_keyspace* ks, const char* from, size_t fromlen, const char* to, size_t tolen);
CF_NODISCARD CF_API cf_bool cf_ks_rename_list(cf_keyspace* ks, const char* from, size_t fromlen, const char* to, size_t tolen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_rename` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L440), [`cf_ks_rename_list` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L451).

Before resolution: every candidate of `from` offered to `to` as well,
CF_CAND_RENAMED. The old key keeps its own, for code that still reads
it. False when there is no room.

```c
CF_NODISCARD CF_API cf_bool cf_ks_rename_with(cf_keyspace* ks, const char* from, size_t fromlen, const char* to, size_t tolen, cf_ks_convert_fn conv, void* ctx);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_rename_with` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L445).

As cf_ks_rename, with `conv` (or NULL) called on each copy; a converted
copy's place says "(from &lt;old>, converted)". False on no room and on a FAIL
of the hook, which the caller tells apart by the error it set in `ctx`.

```c
CF_NODISCARD CF_API cf_bool cf_ks_add_candidate(cf_keyspace* ks, const char* key, size_t keylen, const cf_candidate* c);
```

Implementation: [`cf_ks_add_candidate` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L456).

Record a copy of another key space's candidate, at its rank, with what
it was - textual, a default, the template it was made from - and not
what it was waiting for: it is not read for references again.
Guarantee: strong. The candidate is added with its `written` text or not at all;
false leaves the key space as it was.

```c
CF_NODISCARD CF_API cf_bool cf_ks_replay(cf_keyspace* ks, const cf_keyspace* base);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_replay` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L520).

Internal snapshot replay, in original insertion order and at the
source-local keys of appended lists. Derived counts are not replayed.

```c
CF_NODISCARD CF_API cf_bool cf_ks_append_list(cf_keyspace* ks, const char* key, size_t keylen, const cf_list_part* parts, size_t count, const cf_candidate* result, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_append_list` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L557).

Internal: plans are collected before this changes the key index.
Resolve once after applying all plans.

```c
CF_API const char* cf_ks_original_key(const cf_keyspace* ks, const cf_candidate* c);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_original_key` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L492).

The source-local key before remapping, or NULL if it never moved.
c must be an owned candidate of this space, not a copied descriptor.

```c
CF_NODISCARD CF_API cf_bool cf_ks_reject(cf_keyspace* ks, const char* key, size_t keylen, int source, const cf_value* value, const char* at, const char* reason);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_reject` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L641).

Record a candidate that failed conversion or validation. It never
wins, and it is never erased: explain() has to be able to say that
the environment did supply a value and why it was refused.

```c
CF_NODISCARD CF_API cf_bool cf_ks_resolve(cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_resolve` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L745).

Sorts and groups the candidates. Returns false when there was no room
to do it, which is not the same as an empty configuration: the caller
must distinguish, or a target out of memory gets a snapshot with no
keys and nothing to say why.

```c
CF_API size_t cf_ks_candidate_count(const cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_candidate_count` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L741).

Candidates appended so far, which is what an out-of-room message
should name: the caller sized a buffer, not a key count.

```c
CF_API void cf_ks_infer_untyped(cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_infer_untyped` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L832).

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_at_mut` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L853).

Mutable access, for the passes that legitimately change outcomes:
applying definitions marks candidates rejected. Casting away const
at the call site would work and would also hide which passes write,
so the write path is named instead.

```c
CF_API void cf_ks_reselect(cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_reselect` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L860).

Recompute the selected candidate of every entry. Needed after
anything that changes an outcome, which in practice means after the
definitions have been applied and some winners have been refused.

```c
CF_API const cf_entry* cf_ks_find(const cf_keyspace* ks, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_find` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L921).

Binary search over the resolved entries. NULL when absent, and NULL
before cf_ks_resolve: a half-built space has no answer to give, and
returning a provisional one would be worse than returning none.

```c
CF_API size_t cf_ks_lower_bound(const cf_keyspace* ks, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_lower_bound` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L901).

Index of the first key that is not less than `key`, which is where a
prefix scan starts. Returns cf_ks_count when every key sorts before
it. The space is sorted, so this is a binary search and a prefix scan
costs O(log n) plus the matches rather than a walk.

```c
CF_API const cf_value* cf_ks_get(const cf_keyspace* ks, const char* key, size_t keylen);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_get` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L944).

The selected value, or NULL when the key is absent or every
candidate was rejected.

```c
CF_API size_t cf_ks_count(const cf_keyspace* ks);
CF_API const cf_entry* cf_ks_at(const cf_keyspace* ks, size_t i);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_count` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L952), [`cf_ks_at` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L956).

```c
CF_NODISCARD CF_API cf_bool cf_ks_is_sorted(const cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_is_sorted` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L886).

Verifies the ordering invariant the design depends on.

```c
CF_API cf_u64 cf_ks_digest(const cf_keyspace* ks);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_digest` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L1138).

Order-independent digest over the resolved space. Two runs that
produce the same configuration produce the same digest, on every
platform and in both portability modes. This is the determinism
requirement made checkable.

```c
CF_API cf_u64 cf_ks_value_digest(const cf_value* v);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_value_digest` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L997).

FNV-1a over one value's canonical text: what a value-blind report
says of a secret, so that "did it change" has an answer and "what is
it" has none. Not cryptographic, and not presented as one.

```c
CF_API size_t cf_ks_digest_text(cf_u64 h, char* buf, size_t cap);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_digest_text` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L1007).

A digest as a report writes it, "fnv1a:" and sixteen hex digits, in
`buf` as far as `cap` allows; the length it needs, 22.

```c
CF_API cf_u64 cf_siphash24(const unsigned char* key, const void* p, size_t n);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_siphash24` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L1067).

SipHash-2-4 of `n` bytes under the 16-byte `key` (D-85). INTERNAL:
exported for the tests' vectors.

```c
CF_API size_t cf_ks_secret_digest_text(const cf_value* v, const unsigned char* key, char* buf, size_t cap);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ks_secret_digest_text` (lib/config/src/core/keyspace.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/keyspace.c#L1115).

The digest a report and the fingerprint stand for a secret's value:
cf_ks_digest_text(cf_ks_value_digest(v)) when `key` is NULL, else
"sip24:&lt;id>:&lt;16 hex digits>" (D-85). Into `buf` as far as `cap`
allows; the length it needs, at most 31.

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
CF_API void cf_live_options_init(cf_live_options* o);
```

Implementation: [`cf_live_options_init` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L730).

The same, as a call a binding can make: `size`, and every member at
its default - what the macro sets. A NULL `o` does nothing.
Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_live* cf_live_open_with(const cf_live_options* o, cf_error* err);
```

Implementation: [`cf_live_open_with` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L656).

Builds and checks the first snapshot, and fails as that does. A size
below 1.0's is refused; NULL options are refused too, since `build` is
required (docs/Architecture.md, "Options with defaults").
Guarantee: strong. The options are checked before anything is made; a refusal
or a failed first build leaves nothing behind but `err`.

```c
CF_NODISCARD CF_API cf_live* cf_live_open(cf_live_mode mode, cf_live_build_fn build, cf_live_check_fn check, void* user, const cf_allocator* a, cf_error* err);
```

Implementation: [`cf_live_open` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L623).

The shorthand: one `user` for both functions. `check` may be NULL; `a`
NULL is the default allocator.
Guarantee: strong. As cf_live_open_with.

```c
CF_API void cf_live_close(cf_live* l);
```

Implementation: [`cf_live_close` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L739).

Stops the reloads and frees them with the snapshots they hold; a reader's own
reference to a snapshot outlives it.
Guarantee: nothrow. A NULL `l` is accepted; `l` is consumed.

```c
CF_API struct cf_config* cf_live_get(cf_live* l);
```

Implementation: [`cf_live_get` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L754).

The current snapshot, with a reference the caller releases with
cf_config_free. Any thread. A reader keeps the snapshot it got for as
long as it holds it, so one request sees one configuration.
Guarantee: nothrow. NULL for a NULL `l`; the reference is the caller's.

```c
CF_API cf_live_result cf_live_poll(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_poll` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L767).

Under APPROVE and AUTO: if a file of the running snapshot (or of the
one pending) has changed, rebuild. MANUAL: CF_LIVE_UNCHANGED.
Guarantee: strong. A failed rebuild keeps serving the snapshot readers had:
CF_LIVE_FAILED, `err` set, the failure counted in the status.

```c
CF_API cf_live_result cf_live_reload(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_reload` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L806).

Rebuild now, in any mode, as if a file had changed: SWAPPED, PENDING
under APPROVE, or UNCHANGED when the rebuild found the same values
(the file stamps are refreshed, and no new generation counted).
Guarantee: strong. As cf_live_poll: a failed reload keeps serving the snapshot it
had.

```c
CF_API cf_live_result cf_live_wait(cf_live* l, long timeout_ms, cf_error* err);
```

Implementation: [`cf_live_wait` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L817).

cf_live_poll after waiting up to `timeout_ms` for a file to change,
on a watcher cf_live keeps open on the running snapshot's files. For
a program with no loop of its own, on a thread it gives to this.
Where the platform has no watcher, or the snapshot read no file, it
polls at once: C89 has no portable way to sleep. Under MANUAL there
is nothing to wait for and it returns UNCHANGED at once, so a thread
looping on it spins: use cf_live_reload there.
Guarantee: strong. As cf_live_poll.

```c
CF_NODISCARD CF_API cf_live_result cf_live_commit(cf_live* l, struct cf_edit* e, const char* note, cf_error* err);
```

Implementation: [`cf_live_commit` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L875).

Commit `e` (cf_edit.h) and reload, in one step: the change an
administrator made reaches readers the way an edit to a file does,
through the build, the check and the mode - under APPROVE it waits to
be approved. CF_LIVE_FAILED, with `err` saying why, when the commit
was refused, and nothing was written: the log has an "edit-refused"
event, and the status is not touched, since nothing was reloaded. An
edit that commits and then fails the build or the check is a failed
reload like any other. Consumes the edit.
Guarantee: strong. A refused commit writes nothing and touches no snapshot; one
that commits and then fails the build or the check keeps serving the snapshot
readers had, as cf_live_reload.

```c
CF_API const struct cf_config* cf_live_pending(const cf_live* l);
```

Implementation: [`cf_live_pending` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L824).

The snapshot waiting for approval, or NULL; it stays the cf_live's.
Guarantee: nothrow. NULL for a NULL `l`.

```c
CF_API cf_live_result cf_live_approve(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_approve` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L828).

Makes the pending snapshot the one readers get; CF_LIVE_UNCHANGED when none waits.
Guarantee: strong. Nothing waiting, or a NULL `l` (FAILED, `err` set), changes
nothing; otherwise the swap is whole.

```c
CF_API void cf_live_reject(cf_live* l);
```

Implementation: [`cf_live_reject` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L843).

Drops the pending snapshot; the running one is untouched.
Guarantee: nothrow. A NULL `l` is accepted.

```c
CF_API cf_live_result cf_live_rollback(cf_live* l, const char* reason, cf_error* err);
```

Implementation: [`cf_live_rollback` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L853).

Readers get the snapshot the probationary swap replaced, and the
probation ends: CF_LIVE_ROLLED_BACK, the generation one more, a
"reload-rolled-back" event with `reason`. CF_LIVE_FAILED, with `err`
saying so and nothing else touched, when no swap is on probation.
`reason` may be NULL. (1.1)
Guarantee: strong. With no swap on probation nothing is touched; otherwise the
old snapshot is back whole.

```c
CF_NODISCARD CF_API cf_bool cf_live_confirm(cf_live* l);
```

Implementation: [`cf_live_confirm` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L863).

Ends the probation: the snapshot readers have is the one to keep, and
the one before it is released. False when none was on probation. A
"reload-confirmed" event. (1.1)
Guarantee: strong. False changes nothing.

```c
CF_API cf_u64 cf_live_generation(cf_live* l);
```

Implementation: [`cf_live_generation` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L924).

How many snapshots readers have been given: 1 after open, one more a
swap. A reader can tell that what it holds is no longer current.
Guarantee: nothrow. 0 for a NULL `l`.

```c
CF_API size_t cf_live_restarts(const cf_live* l, char* buf, size_t cap);
```

Implementation: [`cf_live_restarts` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L936).

The CF_RESTART keys whose new value waits for a restart, one a line,
in `buf` as far as `cap` allows; the length it needs, 0 for none.
Guarantee: nothrow. Measure-or-write: the text is cut where `cap` ends and is
terminated; a NULL `buf` stores nothing.

```c
CF_NODISCARD CF_API cf_bool cf_live_status_get(cf_live* l, cf_live_status* out);
```

Implementation: [`cf_live_status_get` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L891).

Copies the status into `out`, whose `size` the caller sets, as far as
it reaches (D-51): a caller built against a later, larger struct gets
what this library has, one built against an earlier gets what it
asked for. False for a NULL, or a size smaller than 1.0's.
Guarantee: strong. False writes nothing to `out`; the live is only read.

```c
CF_NODISCARD CF_API cf_bool cf_live_last_error(cf_live* l, cf_error* err);
```

Implementation: [`cf_live_last_error` (lib/config/src/core/live.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/live.c#L906).

The last failure's error, copied into `err` (cf_error_copy); false,
and `err` cleared, when no reload has failed. Any thread.
Guarantee: strong. The live is only read; the copy is the caller's.

## cf_lock.h

cf_lock.h - a lock that plugs in (D-227, D-220).

A write may exclude another writer, and what does it is the program's
choice: `cf_lock_none()` takes nothing (the default), `cf_lock_sidecar()`
takes a kernel lock on a file kept beside the one written, and a program
may fill a `cf_locker` of its own (a service it asks, a database lock, an
embedded mutex), as it fills a `cf_packer`. The locker knows nothing of
configuration; it is given a path and a wait.

```c
CF_API const cf_locker* cf_lock_none(void);
```

Implementation: [`cf_lock_none` (lib/platform/src/lock.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/lock.c#L47).

The built-in lockers, as functions (no data is exported from the
library): the default, which takes nothing and always succeeds, and the
kernel lock on the file `&lt;dir>/.&lt;base>.cf-lock` beside `path`, which is
kept and never removed. A `cf_lock_sidecar()` on the `none` platform
returns a locker whose `take` fails CF_E_CAPABILITY, `this build has no
files to lock`.
Guarantee: nothrow. A constant; nothing is taken.

```c
CF_API const cf_locker* cf_lock_sidecar(void);
```

Implementation: [`cf_lock_sidecar` (lib/platform/src/lock.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/lock.c#L53).

Guarantee: nothrow. A constant; nothing is taken until the locker is used.

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

Implementation: [`cf_log_level_name` (lib/config/src/core/config.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/config.c#L2772).

The name of a level: "debug", "info", "warn", "error".
Guarantee: nothrow. A static name; "?" for a level out of range.

## cf_numeric.h

cf_numeric.h - locale-independent numeric conversion, and units.

INTERNAL (D-51), but for the units at the end: the conversions are
exported for ConfigForge Doctor and the tests, and outside the 1.x
promise - a minor release may change them. Their functions are declared
only under CF_INTERNAL_API (D-51, D-88). A program uses the
configforge.h functions instead. The units ("30s", "512MiB"; D-113,
D-222) are public, in their first minor release (D-89).

strtod, atoi and printf("%f") all follow the active locale's decimal
separator, so a configuration file written on one machine would not
parse identically on another. These conversions are therefore always
the library's own, at every standard level and on every toolchain.

Floating point uses exact integer digit generation (Steele & White /
Burger & Dybvig), which produces the shortest decimal string that
round-trips, identically on every platform. Decision D-25.

### Integers

```c
CF_NODISCARD CF_API cf_bool cf_text_to_i64(const char* s, size_t len, cf_i64* out, size_t* end);
CF_NODISCARD CF_API cf_bool cf_text_to_u64(const char* s, size_t len, cf_u64* out, size_t* end);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_to_i64` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L55), [`cf_text_to_u64` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L28).

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_f64_to_text` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L531).

Write the shortest decimal string that reads back as exactly v.
Output is identical on every platform. Returns the length written,
or 0 when cap is too small or v is not finite.

NaN and infinity are deliberately not representable: a configuration
value cannot be either, and rejecting them here keeps every codec
from having to invent a spelling.

```c
CF_NODISCARD CF_API cf_bool cf_text_to_f64(const char* s, size_t len, double* out, size_t* end);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_to_f64` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L725).

Parse a C-locale floating point number. The decimal separator is
always '.', never the locale's. Accepts the forms produced by
cf_f64_to_text and the usual hand-written ones.

Range: a number from 10^309 up, or one that would round to infinity,
is refused. Below the smallest subnormal the result is zero. At most
290 significant digits decide the value; later digits move only the
exponent and the rounding, so a literal of hundreds of digits is read
as what it is, and one lying exactly on a halfway point may be one
unit in the last place off.

```c
CF_API cf_u64 cf_f64_bits(double d);
CF_API double cf_f64_from_bits(cf_u64 bits);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_f64_bits` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L889), [`cf_f64_from_bits` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L895).

A double as its IEEE 754 bit pattern, and back, by memcpy. (1.5)

```c
CF_API double cf_f64_from_f16_bits(cf_u32 bits);
CF_API double cf_f64_from_f32_bits(cf_u32 bits);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_f64_from_f16_bits` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L904), [`cf_f64_from_f32_bits` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L921).

A half- and a single-precision pattern (in the low 16 and 32 bits) as
a double, exactly, the non-finite ones included. (1.5)

### Tier 0 type inference

Used only when no definition exists. Deliberately timid: see the
Type Inference section of the specification and decision D-24.

```c
CF_API cf_scalar_kind cf_infer_scalar(const char* s, size_t len);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_infer_scalar` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1014).

### Units: durations and sizes (D-113, D-222)

`"1h30m"` and `"512MiB"` read as the whole number of the unit's base
(5400000 ms, 536870912 B). A unit is an open value: the two built-in
ones are reached through cf_unit_duration_get and cf_unit_size_get
(functions, not data, so that a shared library on every platform
exports them), and a program writes a cf_unit of its own with its own
parser. The parsers know no configuration: cf_def.unit (cf_define.h)
is what binds a key to one. Every abbreviation the two read is a row
of the table in docs/Guide.md, "Durations and sizes".

Notation: case-sensitive, no whitespace, no locale. A fraction ("1.5h")
is read when the result is a whole number of base units, and refused
otherwise, never rounded. A bare number is the base unit. A suffixed
form has no sign. Every product and sum is checked against the
largest cf_i64 before it is made; an overflow is a refusal.

```c
CF_EXPERIMENTAL(2, 1) void cf_unit_options_init(cf_unit_options* o);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_unit_options_init` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1361).

The defaults above. A NULL pointer is accepted and left alone.
Guarantee: nothrow.

```c
CF_EXPERIMENTAL(2, 1) const cf_unit* cf_unit_duration_get(void);
CF_EXPERIMENTAL(2, 1) const cf_unit* cf_unit_size_get(void);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_unit_duration_get` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1764), [`cf_unit_size_get` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1768).

The two built-in units, valid for the life of the program.
Guarantee: nothrow.

```c
CF_NODISCARD CF_EXPERIMENTAL(2, 1) cf_bool cf_unit_parse(const cf_unit* u, const char* s, size_t n, cf_i64*      out, const char** reason);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_unit_parse` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1372).

`u->parse`, usable alone: NULL arguments (no unit, no text, no place for the
number) are refused. `reason` may be NULL, and is NULL on a refusal that has
no sentence of its own (the text is not in the unit's notation).
Guarantee: strong.

```c
CF_EXPERIMENTAL(2, 1) size_t cf_unit_format(const cf_unit* u, cf_i64 v, char* buf, size_t cap);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_unit_format` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1383).

`u->format`: a duration as "1h30m", a size as "512MiB" (the largest exact
suffix), a negative number as the bare number. Returns the length written
(the text is terminated), or 0 with an empty string when `cap` is too small
or the unit has no formatter.
Guarantee: nothrow.

```c
CF_NODISCARD CF_EXPERIMENTAL(2, 1) cf_bool cf_unit_table_parse(const cf_unit_table* t, const char* s, size_t n, cf_i64*      out, const char** reason);
```

Experimental since 2.1: outside the 1.x promise until the next minor release makes it promised or removes it.

Implementation: [`cf_unit_table_parse` (lib/numeric/src/numeric.c)](https://github.com/develooper1994/configforge/blob/main/lib/numeric/src/numeric.c#L1299).

One term, or a sum of terms, `number` and an abbreviation, read exactly (no
floating point): the notation above, over this table. A bare number
(optionally signed) is the base unit when `allow_bare`.
Guarantee: strong.

## cf_platform.h

cf_platform.h - what the platform provides, behind one door.

INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
outside the 1.x promise - a minor release may change it. Its functions
are declared only under CF_INTERNAL_API (D-51, D-88), but
cf_platform_name and the cf_platform_has_ queries, which are promised:
a program may ask what the build can do. A program uses the
configforge.h functions instead.

Three things a configuration library needs that C89 has no answer
for: the process environment, the command line as UTF-8, and a
directory listing. Each has a POSIX shape, a Windows shape, and a
target with neither.

The rule is the same one the portability layer follows for compilers:
**the build picks the file, the call site never asks**. A platform
implementation lives in src/platform/&lt;name>/ and is the only place
allowed to name a platform macro, which gate.semgrep enforces.

Each implementation guards its whole body with its own
CF_PLATFORM_* macro. That guard is not a scattered #ifdef: it exists
so the amalgamation, which is one translation unit and cannot have
the build choose for it, can carry every platform and let the
preprocessor select.

### Which platform

Decided in cf_platform_file.h, included above, and nowhere else:
CF_PLATFORM_WIN32, CF_PLATFORM_POSIX and CF_PLATFORM_NONE. NONE can be
set by hand for a target that has none of this, and then every
capability below reports that it is absent rather than pretending.

```c
CF_API const char* cf_platform_name(void);
```

Implementation: [`cf_platform_name` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L14), [`cf_platform_name` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L93), [`cf_platform_name` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L35).

The name this build was compiled for, for explain() and the report.
Guarantee: nothrow.

### Capabilities

Queried rather than assumed, so a caller can say "this build cannot
list a directory" instead of handing back an empty one - which looks
exactly like an empty configuration and is the more dangerous of the
two answers.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_environ(void);
```

Implementation: [`cf_platform_has_environ` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L18), [`cf_platform_has_environ` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L97), [`cf_platform_has_environ` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L39).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_dir_list(void);
```

Implementation: [`cf_platform_has_dir_list` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L22), [`cf_platform_has_dir_list` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L101), [`cf_platform_has_dir_list` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L43).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_wide_args(void);
```

Implementation: [`cf_platform_has_wide_args` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L26), [`cf_platform_has_wide_args` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L105), [`cf_platform_has_wide_args` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L47).

Guarantee: strong. A capability query; it changes nothing.

### The environment

```c
CF_API char* const* cf_platform_environ(void);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_environ` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L30), [`cf_platform_environ` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L111), [`cf_platform_environ` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L51).

NULL where none is reachable, which is not an error: a program with
no environment is a normal thing on a small target.

### A descriptor

What cf_serve_report needs of a connected stream socket (D-123): a
capability query, a socket test, the non-blocking flag, and a read and
a write each bounded by a time. POSIX only; the other platforms say
no, so that the channel is a part behind an interface (D-229).

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_fd_io(void);
```

Implementation: [`cf_platform_has_fd_io` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L34), [`cf_platform_has_fd_io` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L119), [`cf_platform_has_fd_io` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L935).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_NODISCARD CF_API cf_bool cf_platform_fd_is_socket(int fd);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_fd_is_socket` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L38), [`cf_platform_fd_is_socket` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L123), [`cf_platform_fd_is_socket` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L939).

Guarantee: strong. A query on the descriptor; it changes nothing.

```c
CF_API int cf_platform_fd_nonblock(int fd, int on);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_fd_nonblock` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L43), [`cf_platform_fd_nonblock` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L129), [`cf_platform_fd_nonblock` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L944).

Set (`on` true) or clear O_NONBLOCK; returns the flag as it was, 1 or 0, or
-1 on failure.
Guarantee: basic. The flag may be left as it was found when -1 is returned.

```c
CF_API long cf_platform_fd_read(int fd, void* buf, size_t cap, long timeout_ms);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_fd_read` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L49), [`cf_platform_fd_read` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L170), [`cf_platform_fd_read` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L950).

Bytes read (at most `cap`), 0 at end of stream, -1 on failure, -2 when
`timeout_ms` passed with nothing to read; negative: no limit.
Guarantee: basic. Nothing but `buf` is written; the descriptor's position is
the stream's.

```c
CF_NODISCARD CF_API cf_bool cf_platform_fd_write(int fd, const void* buf, size_t n, long timeout_ms);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_fd_write` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L57), [`cf_platform_fd_write` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L190), [`cf_platform_fd_write` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L958).

Every byte of `buf`, or false: each wait is at most `timeout_ms` (negative:
no limit). A peer that went away is a false return and never a signal.
Guarantee: basic. Some of the bytes may have been written when false is returned.

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_args` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L65), [`cf_platform_args` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L254), [`cf_platform_args` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L55).

### Directory listing

```c
CF_API int cf_platform_dir_list(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_dir_list` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L99), [`cf_platform_dir_list` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1012), [`cf_platform_dir_list` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L332).

Names only, no path, no order, dotfiles already skipped. Returns the
count, or -1 with `err` set. The caller sorts, because the order
decides which file wins and that is the caller's rule, not the
platform's.

Only files are listed, and links are followed to see what they
name, because a Kubernetes mount is links to files. A subdirectory
is skipped. Anything else - a FIFO, a socket, a device, a link to
nothing - is -2 with `err` naming it: opened as a file, a FIFO waits
for a writer for ever. -2 is also more than `max` files, or a name
too long for a slot. -1 is a directory that cannot be read at all,
which an optional source may accept; -2 is one that was read and
cannot be handed over whole, which nothing should, because the file
left out could be the one that decides a value.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_trusted_dir(void);
```

Implementation: [`cf_platform_has_trusted_dir` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L109), [`cf_platform_has_trusted_dir` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1017), [`cf_platform_has_trusted_dir` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L385).

Whether this platform can say who may write to a directory and its
files (POSIX: yes; Windows and a build with no operating system:
no - an access control list is not a mode, and an answer that
guessed would be wrong both ways).
Guarantee: strong. A capability query; it changes nothing.

```c
CF_API int cf_platform_dir_list_trusted(const char* path, char (*names)[CF_PLATFORM_NAME_MAX], int max, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_dir_list_trusted` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L113), [`cf_platform_dir_list_trusted` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1025), [`cf_platform_dir_list_trusted` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L389).

cf_platform_dir_list, and in the same pass the check of D-83: the
directory, every entry and every file an entry names must be ones
nobody but their owner and root can change, and a sticky
directory's entries must belong to root, to the directory's owner
or to the program's user. -2 with
`err` naming the first that is not, as for any entry that cannot be
handed over (CF_OPTIONAL does not excuse it). Where
cf_platform_has_trusted_dir is false it is cf_platform_dir_list.

### The Windows Registry

Every value under one key, in UTF-8, each with the path of the
subkey holding it. Windows only; everywhere else the walk fails and
says so, because a registry source that read nothing on Linux would
look exactly like an empty one.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_registry(void);
```

Implementation: [`cf_platform_has_registry` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L135), [`cf_platform_has_registry` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L312), [`cf_platform_has_registry` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L702).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_API int cf_platform_registry_walk(const char* root, cf_arena* arena, cf_platform_reg_fn fn, void* ctx, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_registry_walk` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L139), [`cf_platform_registry_walk` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L316), [`cf_platform_registry_walk` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L886).

Every value under `root` - "HKCU\\Software\\MyApp", HKCU, HKLM, HKCR,
HKU or HKCC first - and every subkey's, to CF_MAX_DEPTH. Buffers come
from `arena`. 0 when walked, 1 when `root` does not exist, -1 with
`err` set.

### macOS's user defaults

One domain's values as a document - dictionaries, arrays and values
in order - with each value's path for explain(). macOS only, under
CF_WITH_PREFERENCES; everywhere else the walk fails and says so,
because a source that read nothing would look exactly like an empty
domain.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_preferences(void);
```

Implementation: [`cf_platform_has_preferences` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L152), [`cf_platform_has_preferences` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L335), [`cf_platform_has_preferences` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L968).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_API int cf_platform_preferences_walk(const char* domain, cf_arena* arena, cf_platform_pref_fn fn, void* ctx, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_preferences_walk` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L156), [`cf_platform_preferences_walk` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L537), [`cf_platform_preferences_walk` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L972).

Every value of `domain` ("com.example.app") in the scope W3-F2b's
answer names. Buffers come from `arena`. 0 when walked, 1 when the
domain has no values, -1 with `err` set.

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
CF_NODISCARD CF_API cf_bool cf_platform_stamp(const char* path, cf_stamp* out);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_stamp` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L89), [`cf_platform_stamp` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L288), [`cf_platform_stamp` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L235).

False when the path cannot be stamped, which is not an error: a file
that is not there yet has a stamp saying so, and its arrival is a
change like any other.

```c
CF_NODISCARD CF_API int cf_platform_lock_take(cf_lock* held, const char* path, unsigned long wait_ms, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_lock_take` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L72), [`cf_platform_lock_take` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L736), [`cf_platform_lock_take` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L167).

The sidecar lock on `path`.cf-lock: 1 with it held; 0 with err set: CF_E_BUSY
"'%s' is busy: another writer holds its lock; try again" after `wait_ms`,
CF_E_SOURCE "cannot lock '%s': %s" when the lock file cannot be opened.
`held->handle` is a file descriptor on POSIX, (long)(INT_PTR) of the HANDLE
on Windows, -1 when not held. (D-227)

```c
CF_API void cf_platform_lock_give(cf_lock* held);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_lock_give` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L83), [`cf_platform_lock_give` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L792), [`cf_platform_lock_give` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L228).

Releases and closes; does nothing for a lock not held. The lock file is kept.

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_world_readable` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L118), [`cf_platform_world_readable` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L611), [`cf_platform_world_readable` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L260).

### Creating a file only its owner can read

For a file that may hold a secret and does not exist yet, such as a
new store. cf_platform_has_private_create says whether this platform
can do it; a caller that gets false creates the file its own way
rather than failing, because the default mode is what it had before.

cf_platform_create_private returns 1 when it created an empty file
that only its owner can read and write, 0 when the path already
exists (it is left exactly as it was, mode and contents; "exists" is
an answer, not an error), and -1 with `err` set, naming the path and
the reason, when it cannot be created - a directory that is not
there, no permission. It never replaces or truncates anything.

POSIX creates it with mode 0600 in the same call that creates it
(O_CREAT | O_EXCL), not by changing the mode afterwards, so there is
no moment at which another user could open it; the umask can only
remove bits from 0600, never add them. Windows creates it
(CREATE_NEW) with a protected access control list that grants full
access to the file's owner alone and inherits nothing from the
directory. What that does not do: an administrator can still take
ownership, and backup software still reads it. A build with no
operating system has no answer and reports it.

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_private_create(void);
```

Implementation: [`cf_platform_has_private_create` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L123), [`cf_platform_has_private_create` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L803), [`cf_platform_has_private_create` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L265).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_API int cf_platform_create_private(const char* path, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_create_private` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L127), [`cf_platform_create_private` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L807), [`cf_platform_create_private` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L269).

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
CF_NODISCARD CF_API cf_bool cf_platform_has_wake(void);
```

Implementation: [`cf_platform_has_wake` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L173), [`cf_platform_has_wake` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1150), [`cf_platform_has_wake` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L564).

Guarantee: strong. A capability query; it changes nothing.

```c
CF_NODISCARD CF_API cf_platform_wake* cf_platform_wake_open(const char* const* paths, size_t              n, const cf_allocator* a, cf_error*           err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_open` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L177), [`cf_platform_wake_open` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1154), [`cf_platform_wake_open` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L568).

NULL with `err` set, naming the path and the reason. The paths are
copied; nothing passed here need outlive the call.

```c
CF_API int cf_platform_wake_fd(const cf_platform_wake* w);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_fd` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L187), [`cf_platform_wake_fd` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1203), [`cf_platform_wake_fd` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L617).

A descriptor that becomes readable, or -1 where the handle is not a
descriptor.

```c
CF_API void* cf_platform_wake_handle(const cf_platform_wake* w);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_handle` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L192), [`cf_platform_wake_handle` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1207), [`cf_platform_wake_handle` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L622).

A HANDLE that becomes signalled, or NULL where the handle is not one.

```c
CF_NODISCARD CF_API cf_bool cf_platform_wake_drain(cf_platform_wake* w);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_drain` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L197), [`cf_platform_wake_drain` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1217), [`cf_platform_wake_drain` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L626).

Consume what woke the handle and re-arm anything that needs it. False
when some path can no longer wake it; polling cf_changed still works.

```c
CF_API int cf_platform_wake_wait(cf_platform_wake* w, long timeout_ms);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_wait` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L202), [`cf_platform_wake_wait` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1265), [`cf_platform_wake_wait` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L658).

Wait up to `timeout_ms` (negative: without limit). 1 when woken, and
then already drained; 0 on timeout; -1 when waiting failed.

```c
CF_API void cf_platform_wake_close(cf_platform_wake* w);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_wake_close` (lib/platform/src/none/none.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/none.c#L208), [`cf_platform_wake_close` (lib/platform/src/posix/posix.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/posix.c#L1290), [`cf_platform_wake_close` (lib/platform/src/win32/win32.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/win32.c#L678).

## cf_platform_file.h

cf_platform_file.h - which platform this is, and opening a file by a
UTF-8 path: the one thing of the platform layer that the serialization
library uses.

INTERNAL (D-51): exported for ConfigForge Doctor and the tests, and
outside the 1.x promise. Its function is declared only under
CF_INTERNAL_API (D-51, D-88). cf_platform.h includes this header, so
everything that saw the declaration there still does.

It is a header of its own so that the serialization library can open a
file without carrying the environment, the directory listing, the
Registry or the change handle behind cf_platform.h. For the same
reason it holds the choice of platform: the three files that
implement the opener guard themselves by it (src/platform/&lt;name>/file.c),
and may include nothing of the platform layer but this.

### Which platform

Decided here and nowhere else. CF_PLATFORM_NONE can be set by hand
for a target that has none of this, and then every capability below
reports that it is absent rather than pretending.

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_fopen` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L24), [`cf_platform_fopen` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L44), [`cf_platform_fopen` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L45).

```c
CF_NODISCARD CF_API cf_bool cf_platform_has_files(void);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_has_files` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L36), [`cf_platform_has_files` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L86), [`cf_platform_has_files` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L61).

Whether this build has files to open: CF_TRUE on posix and win32, and on
none unless CF_NO_STDIO is defined. A reader asks before it opens, so that
a build with no files answers CF_E_CAPABILITY and not a plain "cannot
open" (K-11b, D-220).

```c
CF_API int cf_platform_write_file_with(const char* path, const void* data, size_t size, const cf_file_options* o, unsigned  fail_at, void (*on_retry)(void* user), void* user, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_write_file_with` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L47), [`cf_platform_write_file_with` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L358), [`cf_platform_write_file_with` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L342).

------------------------------------------------------------------ *
The core of cf_file_replace (cf_file.h; D-224, D-127)

1 or 0, failing at step `fail_at` (1 to cf_platform_write_steps()) as if
that system call had failed (0: no failure), and calling `on_retry(user)`
before each sleep of the Windows rename retry when not NULL (for the
test, which closes the holder there). No global state (D-101).
cf_file_replace is this with fail_at 0 and no hook.
------------------------------------------------------------------

### The replace refused when the file changed (D-186)

```c
CF_API int cf_platform_write_file_if(const char* path, const void* data, size_t size, const void* expect, size_t expect_size, const cf_file_options* o, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_write_file_if` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L208).

1 when `path` holds exactly `size` bytes of `data`. -1 when the file no
longer holds `expect` (`expect_size` bytes; NULL and 0: the file must not
exist), with `err` set (CF_E_SOURCE, "'%s' changed while it was written")
and the file as the other writer left it. 0 with `err` set for any other
failure, as cf_file_replace.

```c
CF_API int cf_platform_write_file_if_hook(const char* path, const void* data, size_t size, const void* expect, size_t expect_size, const cf_file_options* o, void (*between)(void* user), void* user, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_write_file_if_hook` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L199).

The same, calling `between(user)` once, after the temporary is complete
and before the comparison: for the test, which changes the file there. No
global state (D-101). NULL: no call.

```c
CF_API int cf_platform_write_file_if_with(const char* path, const void* data, size_t size, const void* expect, size_t expect_size, const cf_file_options* o, unsigned fail_at, void (*between)(void* user), void* user, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_write_file_if_with` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L63), [`cf_platform_write_file_if_with` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L369), [`cf_platform_write_file_if_with` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L349).

The core of both, failing at step `fail_at` as cf_platform_write_file_with
does (the test's loop over the steps).

```c
CF_API unsigned cf_platform_write_steps(void);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_write_steps` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L75), [`cf_platform_write_steps` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L110), [`cf_platform_write_steps` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L80).

How many steps can fail: POSIX 6, Windows 5, none 0.

```c
CF_API unsigned long cf_platform_now_ms(void);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_now_ms` (lib/platform/src/none/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/none/file.c#L79), [`cf_platform_now_ms` (lib/platform/src/posix/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/posix/file.c#L90), [`cf_platform_now_ms` (lib/platform/src/win32/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/win32/file.c#L65).

A monotonic millisecond counter from an arbitrary start, for
CF_SYNC_EVERY's sync_ms: clock_gettime(CLOCK_MONOTONIC) where that is
defined, GetTickCount64 on Windows; (unsigned long)-1 where there is no
clock.

```c
CF_API unsigned long cf_platform_writer_syncs(const cf_file_writer* w);
CF_NODISCARD CF_API cf_bool cf_platform_writer_close_counted(cf_file_writer* w, unsigned long*  syncs, cf_error*       err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_platform_writer_syncs` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L255), [`cf_platform_writer_close_counted` (lib/platform/src/file_layer.c)](https://github.com/develooper1994/configforge/blob/main/lib/platform/src/file_layer.c#L260).

For the test: how many syncs this writer has issued, and the close that
says so (cf_file_writer_close, which also reports it in *syncs when that
is not NULL). A count of the writer's own, never a global.

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_ref_init` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L23), [`cf_ref_inc` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L27), [`cf_ref_dec` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L43), [`cf_ref_get` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L61).

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

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_cmp_bytes` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L85), [`cf_cmp_ascii_ci` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L102).

Byte-wise comparison. Never strcoll: ordering must be reproducible.

### Integers

```c
CF_API size_t cf_i64_to_text(cf_i64 v, char* buf, size_t cap);
CF_API size_t cf_u64_to_text(cf_u64 v, char* buf, size_t cap);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_i64_to_text` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L147), [`cf_u64_to_text` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L123).

Write the decimal form of v. Returns the length written, or 0 when
cap is too small. Always terminates when cap > 0.

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
```

Implementation: [`cf_fmt` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L312).

Guarantee: nothrow. The text is cut to `cap`, NUL-terminated; the return is the full
length.

```c
CF_API size_t cf_fmt_append(char* buf, size_t cap, size_t at, const char* fmt, ...);
```

Implementation: [`cf_fmt_append` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L327).

Guarantee: nothrow. As cf_fmt; an `at` past the end of `buf` writes nothing.

```c
CF_API size_t cf_vfmt(char* buf, size_t cap, const char* fmt, va_list ap);
```

Implementation: [`cf_vfmt` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L303).

Guarantee: nothrow. As cf_fmt.

### Build description, reported in explain() and in the provenance

report so that a restricted build is distinguishable from a full one.

```c
CF_API const char* cf_port_description(void);
```

Implementation: [`cf_port_description` (lib/port/src/port.c)](https://github.com/develooper1994/configforge/blob/main/lib/port/src/port.c#L355).

Guarantee: nothrow.

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

Implementation: [`cf_report_write` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L416).

Render the report. Measure-or-write: with a NULL buffer nothing is
stored and the length still accumulates. `info` may be NULL, for a
blind report that names nothing. 0, and an empty `buf`, when there is
no key space or `info` did not start from CF_REPORT_INFO_INIT - its
size is below 1.0's - or its mode is neither CF_REPORT_BLIND nor
CF_REPORT_FULL. A call with no error to give refuses by writing
nothing.
Guarantee: strong. Key space, definitions and sources are read, never
written; `buf` is the caller's and holds an empty string on a refusal.

### Misreads (D-121), internal, not exported

```c
CF_API size_t cf_report_key_write(const cf_keyspace* ks, const cf_defset* ds, const cf_source_view* sv, const char* key, size_t keylen, cf_report_mode mode, char* buf, size_t cap);
```

Implementation: [`cf_report_key_write` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L380).

One key as the report writes it, alone - explain() for a machine:

  { "configforge_key": 1, "key": "server.port",
    "value": { "definition": ..., "candidates": [...], "selected": ... } }

The object under "value" is exactly what the report holds under
"keys.&lt;key>", so whatever reads one reads the other. A key nobody
supplied has no candidates and a null selection. `mode` redacts as
the report does. Unsupported modes return 0 and an empty buffer.
Measure-or-write; 0 without CF_WITH_REPORT.
Guarantee: strong. As cf_report_write.

```c
CF_API size_t cf_schema_write(const cf_defset* ds, const char* title, cf_bool closed, char* buf, size_t cap, cf_error* err);
```

Implementation: [`cf_schema_write` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L805).

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
Guarantee: strong. The definitions are read, never written; a refusal sets `err`
and leaves an empty `buf`.

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

Implementation: [`cf_source_replay_state_size` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L1063).

Guarantee: nothrow. A size.

```c
CF_NODISCARD CF_API cf_bool cf_source_replay(cf_source* s, const char* name, int rank, const char* path, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_replay` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L1067).

Guarantee: strong. The arguments are checked before `s` or `state` is touched;
CF_FALSE leaves both as they were. The file is read at load, not here.

```c
CF_API const cf_error* cf_source_replay_error(const cf_source* s);
```

Implementation: [`cf_source_replay_error` (lib/config/src/diag/report.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/diag/report.c#L1087).

Guarantee: nothrow. The load's error, or NULL when it did not fail (or `s` is not
a replay source).

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

Implementation: [`cf_blob_mem` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L21).

Guarantee: nothrow.

```c
CF_API size_t cf_blob_skip_bom(const cf_blob* b);
```

Implementation: [`cf_blob_skip_bom` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L30).

How many bytes a reader skips at the start of a document: 3 when it
begins with a UTF-8 byte order mark (EF BB BF), else 0. A mark is not
part of the text, and only the first bytes are one.
Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_text_is_utf8(const char* p, size_t n);
```

Implementation: [`cf_text_is_utf8` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L38).

Whether n bytes are UTF-8 as RFC 3629 has it: no overlong form, no
surrogate (U+D800 to U+DFFF), nothing past U+10FFFF, no sequence cut
short. A NUL is a character like any other. (1.5)
Guarantee: strong. A pure check; it changes nothing.

```c
CF_NODISCARD CF_API cf_bool cf_blob_file(cf_blob* b, cf_arena* ar, const char* path, cf_error* err);
```

Implementation: [`cf_blob_file` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L59).

Reads a whole file into the arena. CF_E_SOURCE when it cannot be
opened or read; the caller decides whether that is fatal.
Guarantee: basic. `b` is untouched; the arena keeps the bytes it took and stays
valid; nothing leaks.

```c
CF_API cf_arena* cf_blob_arena(cf_arena* ar);
```

Implementation: [`cf_blob_arena` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L42).

Where a file's bytes go for one reading (D-61): an arena of their own,
from `ar`'s allocator, so that they are gone once the reader is done -
the key space keeps copies of the values - or `ar` itself when it is
fixed and has none to lend. NULL when there is no room for one.
cf_blob_arena_done frees it, zeroed, when it was its own.
Guarantee: strong. NULL when there is no room; `ar` is as it was.

```c
CF_API void cf_blob_arena_done(cf_arena* bytes, cf_arena* ar);
```

Implementation: [`cf_blob_arena_done` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L51).

Guarantee: nothrow.

### Bytes out

Measure-or-write, exactly like cf_fmt: with a NULL buffer nothing is
stored and the length still accumulates, so a caller sizes a buffer
by rendering once into nothing.

```c
CF_API void cf_sink_init(cf_sink* s, char* buf, size_t cap);
```

Implementation: [`cf_sink_init` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L158).

Guarantee: nothrow.

```c
CF_API void cf_sink_write(cf_sink* s, const char* p, size_t n);
```

Implementation: [`cf_sink_write` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L164).

Guarantee: basic. The sink stays valid; past its room the text is cut and the length
still counts.

```c
CF_API void cf_sink_putc(cf_sink* s, char c);
```

Implementation: [`cf_sink_putc` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L176).

Guarantee: basic. As cf_sink_write.

```c
CF_API void cf_sink_puts(cf_sink* s, const char* z);
```

Implementation: [`cf_sink_puts` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L183).

Guarantee: basic. As cf_sink_write; a NULL `z` writes nothing.

```c
CF_API void cf_sink_fmt(cf_sink* s, const char* fmt, ...);
```

Implementation: [`cf_sink_fmt` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L189).

Guarantee: basic. As cf_sink_write.

```c
CF_API void cf_sink_terminate(cf_sink* s);
```

Implementation: [`cf_sink_terminate` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L209).

Guarantee: basic. As cf_sink_write; the text is cut at the room and ends with a NUL.

```c
CF_NODISCARD CF_API cf_bool cf_sink_overflowed(const cf_sink* s);
```

Implementation: [`cf_sink_overflowed` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L215).

Guarantee: strong. A query; it changes nothing.

```c
CF_API void cf_sink_json_string(cf_sink* out, const char* p, size_t n);
```

Implementation: [`cf_sink_json_string` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L231).

JSON, written by hand where a document is not the key space's (a
report, a diff): text as a JSON string, escaped; a value as JSON.
Guarantee: basic. As cf_sink_write.

```c
CF_API void cf_sink_json_value(cf_sink* out, const cf_value* v);
```

Implementation: [`cf_sink_json_value` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L248).

Guarantee: basic. As cf_sink_write.

### Locations

```c
CF_API size_t cf_loc_text(const cf_loc* at, char* buf, size_t cap);
```

Implementation: [`cf_loc_text` (lib/serialize/src/blob.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/blob.c#L274).

"app.toml:14:3", or just the file when there is no position.
Guarantee: nothrow. Cut to `cap`; the return is the full length.

### Document events

A parser is an event producer, never a source. It emits these and
knows nothing about keys; one shared flattener owns key
construction, array counting and location formatting.

Without that split the key rules would live in every parser and
would drift apart, which is the repetition this project exists to
remove - applied to its own internals.

`name` is the member name inside a table and NULL inside an array,
where position is the name.

```c
CF_API size_t cf_sink_key_limit(const cf_doc_sink* out);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_sink_key_limit` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L415).

What a reader may build a key of: the sink's key_limit when it reaches
that member and lends workspace, otherwise CF_KEY_MAX - 1.

```c
CF_NODISCARD CF_API cf_bool cf_sink_grow(const cf_doc_sink* out, char** buf, size_t* cap, size_t keep, size_t need);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_sink_grow` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L424).

Room for `need` bytes in *buf, which holds `cap` and whose first `keep`
   bytes stay: when it is short, a block of twice the size (or `need`, if
   more) from reserve_work, the kept bytes copied. CF_FALSE, with *buf and
cap as they were, when the sink lends no workspace or is out of it.

### Codecs

```c
CF_API const cf_codec* cf_codec_by_name(const char* name);
```

Implementation: [`cf_codec_by_name` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L147).

Compiled-in codecs only. NULL when the format exists but this build
left it out, which the caller must report rather than ignore: a
configuration system whose purpose is explaining where values come
from must not have a build option that makes a file invisible.
Guarantee: nothrow. NULL for a name with no compiled-in codec.

```c
CF_API const cf_codec* cf_codec_by_path(const char* path);
```

Implementation: [`cf_codec_by_path` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L207).

Guarantee: nothrow. NULL for a path no compiled-in codec claims.

```c
CF_API const cf_codec* cf_codec_among(const cf_codec* const* list, size_t n, const char* name, const char* path);
```

Implementation: [`cf_codec_among` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L222).

Among a caller's codecs (cf_builder_codec): the one called `name`
when it is not NULL, else the one whose extensions match `path`;
NULL for none. The built-in codecs are not looked at.
Guarantee: nothrow. NULL for none.

```c
CF_NODISCARD CF_API cf_bool cf_codec_decode(const cf_codec* c, const cf_blob* in, const cf_doc_sink* out, cf_error* err);
```

Implementation: [`cf_codec_decode` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L279).

Reads `in` through `c`: its decode_self when its size reaches that
member and it is set, else its decode. False with `err` set when it
refuses the bytes, or has neither. A text codec that does not declare
its own encoding (CF_CODEC_SELF_ENCODED) reads only bytes that are
UTF-8: cf_encoding_check looks first, and refuses the rest with their
place. (1.5; the check 1.8, D-108)
Guarantee: basic. `in` is not changed; `out` may have been given the events before
the refusal.

```c
CF_NODISCARD CF_API cf_bool cf_codec_can_read(const cf_codec* c);
```

Implementation: [`cf_codec_can_read` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L257).

Whether `c` reads: CF_CODEC_READ and a decoder of either kind. (1.5)
Guarantee: strong. A pure check; it changes nothing.

```c
CF_API const char* cf_format_of_path(const char* path);
```

Implementation: [`cf_format_of_path` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L339).

Names a format this library knows about whether or not it was
compiled in, so the error can say which option to turn on.
Guarantee: nothrow.

```c
CF_API const char* cf_format_option(const char* format);
```

Implementation: [`cf_format_option` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L388).

Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_format_holds(const char* format, unsigned* holds);
```

Implementation: [`cf_format_holds` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L354).

CF_TRUE and *holds the bits of the format called `format`, whether
or not this build has its codec; CF_FALSE, *holds untouched, for a
format this library has no writer for (xml, properties) and for a
name it does not know. (1.8)
Guarantee: strong. `holds` is written only on success.

```c
CF_NODISCARD CF_API cf_bool cf_format_limits(const char* format, size_t* max_path, size_t* max_depth);
```

Implementation: [`cf_format_limits` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L372).

CF_TRUE and the limits of the reader of the format called `format`:
max_path the longest joined path of names (dots counted) to a table,
   array or value that the format both writes and reads back, a length
   the reader and the writer both take and not the first one refused
   (TOML's reader limits the dotted key of a line, relative to its header,
   so no one number is exact); (size_t)-1 when there is none; and
max_depth the most tables and arrays one inside another below the
   document it accepts ((size_t)-1 for none; 0 for a format with no
   nesting). A format never writes what its own reader refuses, so the
   converter leaves out a table past these. CF_FALSE, both untouched, for
   a name this library does not know. (1.8)
   Guarantee: strong. Both outputs are written only on success.

```c
CF_API size_t cf_codec_count(void);
```

Implementation: [`cf_codec_count` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L66).

Guarantee: nothrow.

```c
CF_API const cf_codec* cf_codec_at(size_t i);
```

Implementation: [`cf_codec_at` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L74).

Guarantee: nothrow. NULL past the last codec.

```c
CF_API size_t cf_codec_select(unsigned all_of, unsigned none_of, const cf_codec** out, size_t cap);
```

Implementation: [`cf_codec_select` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L78).

The built-in codecs whose caps hold every bit of `all_of` and none of
`none_of`, in registry order: at most `cap` of them are written to
`out` (NULL when `cap` is 0), and the number that match in all is
returned, as the size cf_report needs is. (1.5)
Guarantee: nothrow. At most `cap` are written; the return is the number that match.

```c
CF_API size_t cf_codec_kind(const cf_codec* c, char* buf, size_t cap);
```

Implementation: [`cf_codec_kind` (lib/serialize/src/codec.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/codec.c#L108).

What kind of format `c` is, as text for a person, comma-separated in
this order: "text" or "binary"; "self-describing" or "schema-bound";
"typed" or "untyped"; "arrays", "arrays of values" or "no arrays";
"read and written", "read" or "written" ("neither read nor written"
for a codec that does neither). Returns the length needed, as
snprintf does; a NULL codec gives 0 and an empty string. (1.5)
Guarantee: nothrow. Cut to `cap`; the return is the length needed.

### Tag, length, value, by a layout the program gives (D-93)

TLV has no one standard, so it is a codec the program makes: the
layout says how wide a tag and a length are and in which byte
order, how the document ends, and what each tag is - its key, its
type, and for a nested one the tags inside it. Each value is placed
in explain() as "&lt;file>, byte &lt;n>", n the offset of its tag. A tag
the layout does not name is refused unless the layout says to skip
or keep it. Read only: writing back keeps what a layout skips, and
is wave 6's.

```c
CF_NODISCARD CF_API cf_bool cf_tlv_codec_sized(cf_codec* out, size_t out_size, const cf_tlv_layout* layout, cf_error*            err);
```

Implementation: [`cf_tlv_codec_sized` (lib/serialize/src/tlv.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/tlv.c#L1094).

A codec that reads `layout`, made in `out`: the program owns `out`,
the layout, and every table and key it points to, and each must
outlive every configuration that reads with the codec - as any codec
given to cf_builder_codec must; a `static` one is the usual way.
The layout is checked whole here, and false with `err` set
(CF_E_DEFINITION, the layout's name as source) names the first
mistake; `out` then reads nothing. The codec is named the layout's
name, takes its extensions, reads only, and is never chosen from
content (CF_CODEC_NAMED_ONLY). Off in a build without CF_WITH_TLV,
and says so. (1.5)
Guarantee: strong. `out` is the caller's output and reads nothing after a refusal;
nothing else is touched.

```c
CF_NODISCARD CF_API cf_bool cf_codec_compose_sized(cf_codec* out, size_t out_size, const cf_compose* spec, cf_error*         err);
```

Implementation: [`cf_codec_compose_sized` (lib/serialize/src/compose.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/compose.c#L191).

Make a named, read-only codec in the program's storage. Keep out, spec,
the base, the name and both contexts alive while any configuration uses
the codec. A repair lends its bytes until decoding returns. A filter
fills the four document callbacks of up and forwards events to down.
A build without CF_WITH_COMPOSE refuses with CF_E_CAPABILITY.
Guarantee: strong. `out` is the caller's output and reads nothing after a refusal;
nothing else is touched.

### The format from the name, then the content (D-73)

Several simple strategies in turn, the first that answers deciding, and
each says why it did: the format a program names; the name's extension;
and, when the name gives none, the content. The content is never taken
on a guess at a few bytes: a look at the first bytes only orders the
candidates, and the decision is by reading - each codec reads the bytes
into a sink that keeps a hash of the events, and one that accepts them
and yields at least one keyed value is a candidate. A strict format
(the builder's own, CBOR, JSON, JSON5, TOML, XML) is taken at once; a loose
one (YAML, INI) only when no other loose format reads the bytes into
different events, else the bytes are refused as ambiguous. Properties,
whose grammar accepts nearly any text, is never chosen from content.

```c
CF_API const cf_codec* cf_codec_detect(const cf_codec* const* own, size_t n, const cf_blob* in, const char* name, cf_arena* ar, cf_detect* out);
```

Implementation: [`cf_codec_detect` (lib/serialize/src/detect.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/detect.c#L251).

The codec for `in`, or NULL. `own` (n of them) are a builder's own
codecs, looked at first, as for extensions; `name`, when not NULL, is a
file name to take the extension's format from - that format is returned
as it stands, without reading, so no document read before is read
differently; with no name, or none whose extension a codec claims, the
content decides. `ar` is the trial readers' work space, and is not
reset: pass one that can be thrown away (cf_blob_arena), or NULL for one
made and released inside the call. `out`, when not NULL, says how the
answer was reached.
Guarantee: basic. `ar`, when lent, keeps what the trial readers took and stays valid;
nothing leaks.

## cf_serialize_version.h

cf_serialize_version.h - the serialization library's version (D-90).

configforge_serialize is made of the headers cf_port.h, cf_error.h,
cf_numeric.h, cf_value.h, cf_alloc.h, cf_platform_file.h, cf_serialize.h
and this one. It does not include cf_version.h, which declares what this
library does not define.

```c
CF_API const char* cf_serialize_version(void);
```

Implementation: [`cf_serialize_version` (lib/serialize/src/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/version.c#L15).

The version the serialization library was compiled as: "1.7.0", and the
same as one number. (1.7)
Guarantee: nothrow.

```c
CF_API int cf_serialize_version_number(void);
```

Implementation: [`cf_serialize_version_number` (lib/serialize/src/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/serialize/src/version.c#L20).

Guarantee: nothrow.

## cf_source.h

cf_source.h - sources.

A source produces candidates. It does not resolve anything and it
does not know what other sources exist; precedence is a rank it is
given, and the key space does the rest. That is what lets a source
be tested in isolation, and it is why the resolver has no per-source
special cases in it.

```c
CF_NODISCARD CF_API cf_bool cf_source_load(const cf_source* s, cf_keyspace* ks);
```

Implementation: [`cf_source_load` (lib/config/src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/memory.c#L84).

Guarantee: basic. The key space stays valid and may hold some of the source's keys;
the caller drops it. The reason is the source's own (cf_source_file_error and its
kin) or the key space's refusal. A NULL or unfilled source, or a NULL key space,
fails and adds nothing.

```c
CF_API void cf_source_destroy(cf_source* s);
```

Implementation: [`cf_source_destroy` (lib/config/src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/memory.c#L91).

Guarantee: nothrow. A NULL or unfilled source is accepted; no source owns memory,
its state is the caller's.

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
CF_NODISCARD CF_API cf_bool cf_source_memory(cf_source* s, const char* name, int rank, const cf_pair* pairs, size_t count);
```

Implementation: [`cf_source_memory` (lib/config/src/sources/memory.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/memory.c#L65).

Guarantee: strong. A NULL `s` or `pairs` fails with `s` as it was; the pairs are
borrowed.

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
CF_NODISCARD CF_API cf_bool cf_source_env(cf_source* s, const char* name, int rank, const char* prefix, char* const* environment);
```

Implementation: [`cf_source_env` (lib/config/src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/env.c#L77).

Guarantee: strong. A NULL `s` fails with `s` as it was; the prefix and the
environment are borrowed.

```c
CF_API char* const* cf_environ(void); /* cf_platform_environ, kept for callers */
```

Implementation: [`cf_environ` (lib/config/src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/compat.c#L24).

The platform environment, or NULL where none is reachable.
Guarantee: nothrow. NULL where no environment is reachable.

```c
CF_API size_t cf_env_key(const char* prefix, const char* var, size_t varlen, char* out, size_t cap);
```

Implementation: [`cf_env_key` (lib/config/src/sources/env.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/env.c#L99).

Exposed for testing: derive a canonical key from a variable name.
Returns the length written, or 0 when the name does not belong to
this prefix or does not derive a valid key.
Guarantee: nothrow. 0 for a name that does not belong to the prefix, derives no
valid key or does not fit `cap`; `out` is the caller's.

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
CF_NODISCARD CF_API cf_bool cf_source_cli(cf_source* s, const char* name, int rank, int argc, char* const* argv);
```

Implementation: [`cf_source_cli` (lib/config/src/sources/cli.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/cli.c#L98).

Guarantee: strong. A NULL `s` fails with `s` as it was; argv is borrowed.

### File source

Reads bytes, picks the codec - the format named, else the extension's,
else the content's (D-73) - and hands the events to the flattener. It
parses nothing itself.

A format that is not compiled in fails loudly and names the option
to turn on. Silently skipping the file would be the one thing a
library about provenance must never do.

```c
CF_NODISCARD CF_API cf_bool cf_file_read_stable(cf_blob* b, cf_arena* ar, const char* path, unsigned attempts, void (*between)(void* user), void* user, cf_error* err);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_file_read_stable` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L61).

cf_blob_file, but the file is stamped before and after the read and read
again when the stamp moved (mtime, size or presence), up to `attempts`
reads in all (0 reads as 3, the default); CF_E_SOURCE "'%s' changed while
it was read" when all of them saw it move. `between(user)`, when not NULL,
is called after each stamp and before the read: for the test, which
changes the file there. A file that cannot be opened fails as
cf_blob_file's does, with no retry (an absent file is not a file being
written).

```c
CF_API size_t cf_source_file_state_size(void);
```

Implementation: [`cf_source_file_state_size` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L611).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_file(cf_source* s, const char* name, int rank, const char* path, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_file` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L513).

Guarantee: strong. A NULL `s`, `path` or `state`, a `state` smaller than the state
size, or an encoding the build does not know fails with `s` and `state` as they
were.

```c
CF_NODISCARD CF_API cf_bool cf_source_file_codec(cf_source* s, const char* name, int rank, const char* path, const struct cf_codec* codec, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_file_codec` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L537).

The same, with the codec handed over rather than looked up: a format
this library does not have, written against cf_doc_sink - over
libyaml's events, expat's callbacks, anything that streams. The codec
is borrowed and must outlive the configuration.

Explicit rather than registered. A registry code could add to at run
time would be global mutable state, which this library has none of,
and two parts of one program registering different codecs under one
name would be a conflict nobody could see.
Guarantee: strong. As cf_source_file; a codec that cannot read fails the same way.

```c
CF_API const cf_error* cf_source_file_error(const cf_source* s);
```

Implementation: [`cf_source_file_error` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L615).

The reason a file source failed, or NULL when it did not. Sources
return a plain boolean because most cannot fail in interesting ways;
a file can fail in a dozen, and the caller needs the detail.
Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

```c
CF_API void cf_source_file_codecs(cf_source* s, const struct cf_codec* const* codecs, size_t n);
```

Implementation: [`cf_source_file_codecs` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L583).

A program's own codecs for a file or a document, looked at before the
built-in ones, for the extension and for the content (D-73). The list
is borrowed and must outlive the source.
Guarantee: nothrow. A NULL source, or one of another kind, is ignored.

```c
CF_NODISCARD CF_API cf_bool cf_source_document(cf_source* s, const char* name, int rank, const char* bytes, size_t n, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_document` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L550).

A document the program holds in memory, as a source: the same reading
as a file's, from `bytes` (n of them) instead of a path. `name` is what
locations and explain() show; `format` is named, or NULL for the name's
extension and then the content (D-73). The bytes are borrowed here and
must outlive the source; cf_builder_document copies them.
Guarantee: strong. As cf_source_file; a NULL `name`, or NULL `bytes` with `n` above
0, fails the same way.

```c
CF_NODISCARD CF_API cf_bool cf_source_env_document(cf_source* s, const char* name, int rank, const char* variable, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_env_document` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L566).

A whole document in one environment variable, as a source (D-73),
opt-in per variable: its value is read as a document, `format` named,
or NULL for the content to decide, as for cf_source_document. Looked up
in the platform's environment at every load, so a reload sees what the
variable holds then. `name` is the source's (NULL: the variable); what
explain() and locations show is the variable. The variable is borrowed
and must outlive the source; cf_builder_env_document copies it. A
variable that is not set is an error, or nothing with CF_OPTIONAL.
Guarantee: strong. As cf_source_document; a NULL or empty variable name, or one
holding '=', fails the same way.

```c
CF_API const char* cf_source_file_format(const cf_source* s, cf_detect_how* how);
```

Implementation: [`cf_source_file_format` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L593).

The format a file or document source read with at its last load, by
the codec's name, and how it was chosen (CF_DETECT_NAMED, _EXTENSION or
_CONTENT; `how` may be NULL). NULL before a load, for a source that is
neither, and when no format was chosen. A load that failed in the
chosen format still names it.
Guarantee: nothrow. NULL for a NULL source or one of another kind, with `*how` then
CF_DETECT_NONE.

```c
CF_API const char* cf_source_file_encoding(const cf_source* s);
```

Implementation: [`cf_source_file_encoding` (lib/config/src/bridge/file.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/file.c#L621).

The encoding the last load of a file, document or environment-document
source read, by its canonical name: "utf-8", "utf-16le" or "utf-16be"
(a byte-order mark said so), or the one named in the flags, such as
"windows-1254". NULL before a load and for any other kind of source.
(1.8, D-108)
Guarantee: nothrow. NULL for a NULL source or one of another kind.

```c
CF_API size_t cf_source_dotenv_state_size(void);
```

Implementation: [`cf_source_dotenv_state_size` (lib/config/src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dotenv.c#L131).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_dotenv(cf_source* s, const char* name, int rank, const char* path, const char* prefix, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_dotenv` (lib/config/src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dotenv.c#L135).

Guarantee: strong. A NULL `s`, `path` or `state`, a `state` smaller than the state
size, or an encoding the build does not know fails with `s` and `state` as they
were.

```c
CF_API const cf_error* cf_source_dotenv_error(const cf_source* s);
```

Implementation: [`cf_source_dotenv_error` (lib/config/src/bridge/dotenv.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dotenv.c#L158).

Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

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
CF_NODISCARD CF_API cf_bool cf_source_snapshot(cf_source* s, const char* name, const cf_keyspace* ks);
```

Implementation: [`cf_source_snapshot` (lib/config/src/sources/snapshot.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/snapshot.c#L36).

An existing key space, replayed. Every candidate at the rank it
originally had, not only the winners: a library whose premise is
explaining itself must not lose provenance at the moment a value
changes, which is exactly when somebody asks.
Guarantee: strong. A NULL `s` or `ks` fails with `s` as it was; `ks` is borrowed
and must outlive the source.

```c
CF_API const char* cf_source_path(const cf_source* s);
```

Implementation: [`cf_source_path` (lib/config/src/sources/compat.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/sources/compat.c#L17).

The file a source reads, or NULL.
Guarantee: nothrow. NULL for a NULL source and for one with no path.

```c
CF_API size_t cf_source_dir_state_size(void);
```

Implementation: [`cf_source_dir_state_size` (lib/config/src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dir.c#L236).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_dir(cf_source* s, const char* name, int rank, const char* path, const char* format, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_dir` (lib/config/src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dir.c#L240).

Guarantee: strong. A NULL `s`, `path` or `state`, a `state` smaller than the state
size, or an encoding the build does not know fails with `s` and `state` as they
were.

```c
CF_API const cf_error* cf_source_dir_error(const cf_source* s);
```

Implementation: [`cf_source_dir_error` (lib/config/src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dir.c#L263).

Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

```c
CF_API void cf_source_dir_codecs(cf_source* s, const struct cf_codec* const* codecs, size_t n);
```

Implementation: [`cf_source_dir_codecs` (lib/config/src/bridge/dir.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/dir.c#L273).

A program's own codecs for the directory's files, looked at before the
built-in ones (cf_builder_codec, D-68). The list is borrowed and must
outlive the source.
Guarantee: nothrow. A NULL source, or one of another kind, is ignored.

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

Implementation: [`cf_source_secrets_state_size` (lib/config/src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/secrets.c#L248).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_secrets(cf_source* s, const char* name, int rank, const char* path, const char* prefix, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_secrets` (lib/config/src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/secrets.c#L276).

Guarantee: strong. A NULL `s`, `path` or `state`, or a `state` smaller than the
state size, fails with `s` and `state` as they were.

```c
CF_API const cf_error* cf_source_secrets_error(const cf_source* s);
```

Implementation: [`cf_source_secrets_error` (lib/config/src/bridge/secrets.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/secrets.c#L294).

Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

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

Implementation: [`cf_source_registry_state_size` (lib/config/src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/registry.c#L207).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_registry(cf_source* s, const char* name, int rank, const char* root, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_registry` (lib/config/src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/registry.c#L211).

Guarantee: strong. A NULL `s`, `root` or `state`, or a `state` smaller than the
state size, fails with `s` and `state` as they were.

```c
CF_API const cf_error* cf_source_registry_error(const cf_source* s);
```

Implementation: [`cf_source_registry_error` (lib/config/src/bridge/registry.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/registry.c#L232).

Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

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

Implementation: [`cf_source_store_state_size` (lib/config/src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/store.c#L344).

Guarantee: nothrow. A constant.

```c
CF_NODISCARD CF_API cf_bool cf_source_store(cf_source* s, const char* name, int rank, const char* path, unsigned flags, void* state, size_t state_size);
```

Implementation: [`cf_source_store` (lib/config/src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/store.c#L348).

Guarantee: strong. A NULL `s`, `path` or `state`, or a `state` smaller than the
state size, fails with `s` and `state` as they were.

```c
CF_API const cf_error* cf_source_store_error(const cf_source* s);
```

Implementation: [`cf_source_store_error` (lib/config/src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/store.c#L368).

Guarantee: nothrow. NULL for a NULL source, for a source of another kind and for
one whose last load did not fail.

```c
CF_NODISCARD CF_API cf_bool cf_store_set(const char* path, const cf_write_item* items, size_t count, const char* meta, cf_error* err);
```

Implementation: [`cf_store_set` (lib/config/src/bridge/store.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/bridge/store.c#L198).

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
Guarantee: basic. The store holds none of `items` (one transaction, rolled back),
but a store that did not exist may be left behind, empty; `err` is set.

## cf_text.h

cf_text.h - the text helpers every reader shares: UTF-8 decoded,
validated and encoded, a hexadecimal digit read, and the hash of a
reader's name table (D-102).

INTERNAL (D-51): exported for the tests, and outside the 1.x promise - a
minor release may change it. Its functions are declared only under
CF_INTERNAL_API (D-51, D-88). A program uses cf_text_is_utf8 from
cf_serialize.h instead.

```c
CF_API size_t cf_text_utf8_decode(const char* p, const char* end, cf_u32* cp);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_utf8_decode` (lib/text/src/utf8.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/utf8.c#L15).

The code point at p, and its length, or 0 when p does not start a
whole, valid sequence before end, and 0 when p == end. *cp is written
only when the length is not 0. Valid is RFC 3629's: no overlong form,
no surrogate, nothing above U+10FFFF.

```c
CF_API size_t cf_text_utf8_valid(const char* p, size_t n);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_utf8_valid` (lib/text/src/utf8.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/utf8.c#L58).

The length of the longest valid prefix of p[0..n), through lanes_scalar().

```c
CF_API size_t cf_text_utf8_encode(cf_u32 cp, char out[4]);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_utf8_encode` (lib/text/src/utf8.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/utf8.c#L62).

Writes cp into out[0..3] and returns 1 to 4; the caller has refused
surrogates and values above 0x10FFFF.

```c
CF_API int cf_text_hex_value(int c);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_hex_value` (lib/text/src/hex.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/hex.c#L7).

0-15 for a hex digit of either case, -1 otherwise.

```c
CF_API void cf_text_esc_decode(const cf_text_esc_family* f, const char* p, const char* end, cf_text_esc* r);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_esc_decode` (lib/text/src/escape.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/escape.c#L29).

Decodes the escape whose letter is at p (the byte after the
backslash), reading nothing at or past end. Moves nothing.

```c
CF_API size_t cf_text_escape(const cf_text_escaper* e, cf_u32 cp, char out[8]);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_escape` (lib/text/src/escape.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/escape.c#L130).

The escape for cp into out (at most 6 bytes are written; out holds
8) and its length, or 0 when cp is written as it is.

```c
CF_API size_t cf_text_ref_count(const char* p, size_t n);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_ref_count` (lib/text/src/refs.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/refs.c#L14).

The "${" of a reference. How many of them n bytes hold.

```c
CF_API size_t cf_text_ref_next(const char* p, size_t n, size_t from);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_ref_next` (lib/text/src/refs.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/refs.c#L25).

The index of the '$' of the first "${" at or after `from`, or n.

```c
CF_API size_t cf_text_ref_unescape(const char* p, size_t n, char* out);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_ref_unescape` (lib/text/src/refs.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/refs.c#L36).

"$${" read as "${", into out, which holds n bytes; the length.

```c
CF_API void cf_text_ref_widen(char* buf, size_t from, size_t end, size_t limit);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_ref_widen` (lib/text/src/refs.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/refs.c#L48).

buf[from..end) widened in place, each "${" written "$${": from the
back, with extra = cf_text_ref_count(buf + from, end - from); a
byte is stored only where its new place is below `limit`.

```c
CF_API size_t cf_text_break(unsigned rule, const char* p, const char* end);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_break` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L10).

The length of the break at p, or 0.

```c
CF_API const char* cf_text_eol(unsigned rule, const char* p, const char* end);
CF_API const char* cf_text_blanks(unsigned set, const char* p, const char* end);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_eol` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L31), [`cf_text_blanks` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L38).

The first break at or after p, or end.

```c
CF_API size_t cf_text_trim(const char* p, size_t n, const char** out);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_trim` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L48).

Trim space, TAB and CR at both ends as INI and dotenv do.

```c
CF_API void cf_text_track_init(cf_text_track* t, const char* start);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_track_init` (lib/text/src/track.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/track.c#L7).

```c
CF_API void cf_text_track_line(cf_text_track* t, const char* after);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_track_line` (lib/text/src/track.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/track.c#L12).

A line ends. The next starts at after.

```c
CF_API unsigned int cf_text_track_col(const cf_text_track* t, const char* at);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_track_col` (lib/text/src/track.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/track.c#L17).

The 1-based column in bytes.

```c
CF_API int cf_text_skip(const cf_text_syntax* sy, cf_text_track* t, const char** pp, const char* end, const char** bad);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_skip` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L123).

Past blanks, comments and line breaks, counting each break in t.
On refusal, bad and pp point at the refused byte or the end of an
unclosed comment. On success, bad is untouched.

```c
CF_API int cf_text_comment(const cf_text_syntax* sy, const char** pp, const char* end, const char** bad);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_comment` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L67).

One '#' comment at pp, to its line's end. The TOML rule consumes a
CR before LF and checks the comment body.

```c
CF_NODISCARD CF_API cf_bool cf_text_fail(cf_error* err, cf_code code, const char* file, unsigned int line, unsigned int col, int reason, const char* fmt, ...);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_fail` (lib/text/src/fail.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/fail.c#L34).

Sets err with no key, file as source, and "file:line:col: body".
With col 0 the message is "file:line: body". line is at least 1.
fmt uses cf_fmt's conversions, including %S. Returns CF_FALSE.

```c
CF_NODISCARD CF_API cf_bool cf_text_fail_at(cf_error* err, cf_code code, const char* file, unsigned int line, unsigned int col, size_t offset, int reason, const char* fmt, ...);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_fail_at` (lib/text/src/fail.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/fail.c#L13).

As cf_text_fail, and the byte offset of the cause in the input, from 0
((size_t)-1 when the caller has none): what cf_error_place_at stores.

```c
CF_NODISCARD CF_API cf_bool cf_text_is_ecma_space(cf_u32 cp);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_is_ecma_space` (lib/text/src/space.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/space.c#L59).

The ECMAScript 5.1 whitespace and line terminators beyond the basic
ASCII spaces and CR/LF, including VT, FF, U+2028 and U+2029.

```c
CF_API size_t cf_text_seed(const void* where);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_seed` (lib/text/src/hash.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/hash.c#L23).

The seed the readers' name tables use: made of two addresses, `where`
and a local's, and never 0.

```c
CF_API size_t cf_text_hash(size_t seed, const char* s, size_t n);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_hash` (lib/text/src/hash.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/hash.c#L34).

FNV-1a of n bytes from seed, in a size_t. The same in 32 bits agrees
with its low 32 bits, so a bucket chosen by the low bits is the same.

```c
CF_API void cf_text_place(const char* p, size_t at, unsigned unit, cf_u32* line, cf_u32* col);
```

Internal (`CF_INTERNAL_API`): exported for the library's own layers, Doctor and the tests, outside the 1.x promise; a program does not see it.

Implementation: [`cf_text_place` (lib/text/src/place.c)](https://github.com/develooper1994/configforge/blob/main/lib/text/src/place.c#L16).

The line and column, both from 1, of the byte at offset `at` of p, in the
original's units (D-108): bytes for `unit` 1, two-byte code units for
CF_ENCODING_UTF16LE or CF_ENCODING_UTF16BE, a line ending at each LF
unit. For a refusal of bytes that are not UTF-8 (A-5) and of a byte with
no character in the encoding named (A-4).

## cf_value.h

cf_value.h - the value model.

Five types. Structure is a naming convention on the key, never a
shape in the data: see the Value Model section of the design. That
decision is what keeps a C implementation reasonable, because there
is no recursive value and therefore no ownership graph.

```c
CF_API const char* cf_type_name(cf_type t);
```

Implementation: [`cf_type_name` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L12).

Guarantee: nothrow.

```c
CF_API const char* cf_type_json_name(cf_type t);
```

Implementation: [`cf_type_json_name` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L25).

JSON Schema spells the same types differently; the report and the
schema generator both need that spelling. Two names, one list.
Guarantee: nothrow.

```c
CF_API void cf_value_null(cf_value* v);
```

Implementation: [`cf_value_null` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L36).

Guarantee: nothrow.

```c
CF_API void cf_value_bool(cf_value* v, cf_bool b);
```

Implementation: [`cf_value_bool` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L41).

Guarantee: nothrow.

```c
CF_API void cf_value_int(cf_value* v, cf_i64 i);
```

Implementation: [`cf_value_int` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L46).

Guarantee: nothrow.

```c
CF_API void cf_value_real(cf_value* v, double r);
```

Implementation: [`cf_value_real` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L52).

Guarantee: nothrow.

```c
CF_API void cf_value_text(cf_value* v, const char* p, size_t n);
```

Implementation: [`cf_value_text` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L58).

Guarantee: nothrow. The text is referenced, not copied.

```c
CF_NODISCARD CF_API cf_bool cf_value_from_text(cf_type t, const char* s, size_t n, cf_value* out);
```

Implementation: [`cf_value_from_text` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L64).

Convert text to the requested type. Returns CF_FALSE when the text
does not denote a value of that type; the caller then reports a
conversion failure naming the parameter and the source, which is
the only place conversion errors are allowed to appear.
Guarantee: strong. `out` is written only when the text denotes the type.

```c
CF_NODISCARD CF_API cf_bool cf_value_infer(const char* s, size_t n, cf_value* out);
```

Implementation: [`cf_value_infer` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L120).

Infer a type from text. Used only where no definition exists, and
deliberately timid: see decision D-24.
Guarantee: strong. `out` is written only on success.

```c
CF_API size_t cf_value_to_text(const cf_value* v, char* buf, size_t cap);
```

Implementation: [`cf_value_to_text` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L141).

Canonical text form. Identical on every platform, which is what
makes the determinism hash and the golden fixtures meaningful.
Returns the length written, or 0 when cap is too small.
Guarantee: nothrow. When `cap` is too small, `buf` is left an empty string and the
length is 0.

```c
CF_API int cf_value_cmp(const cf_value* a, const cf_value* b);
```

Implementation: [`cf_value_cmp` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L198).

Guarantee: nothrow.

```c
CF_NODISCARD CF_API cf_bool cf_value_equal(const cf_value* a, const cf_value* b);
```

Implementation: [`cf_value_equal` (lib/base/src/value.c)](https://github.com/develooper1994/configforge/blob/main/lib/base/src/value.c#L231).

Guarantee: strong. A pure comparison; it changes nothing.

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

Implementation: [`cf_version` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L13), [`cf_version_number` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L17).

The version the library was *compiled* as, which is not necessarily
the header the caller compiled against. A program that links a shared
build can compare the two and say so, which is the only way that
mismatch is ever noticed before it misbehaves.
Guarantee: nothrow. Static text and a number; nothing is allocated.

```c
CF_NODISCARD CF_API const char* cf_build_id(void);
```

Implementation: [`cf_build_id` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L31).

The commit the library was built from, as git abbreviates it -
"20d0f4b" - with "-dirty" when the tree had uncommitted changes, or ""
when the build could not know (no git, or a copy that is neither a
checkout nor a git archive). Build metadata, not part of the version:
two builds of 0.2.0 compare equal by cf_version_number() and can still
be told apart by this. Never NULL. Set at build time, not configure
time, so it follows a checkout without reconfiguring; a build without
CMake can pass -DCF_BUILD_ID="\"...\"".
Guarantee: strong. A pure query of a constant (a nothrow function cannot be
CF_NODISCARD, so the word is strong); nothing changes.

```c
CF_NODISCARD CF_API const char* cf_build_profile(void);
```

Implementation: [`cf_build_profile` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L35).

The build profile this library was built with (D-105): "default",
"minimal", "file:acme", or "unspecified" for a build of the sources
that named none. Never NULL.
Guarantee: strong. As cf_build_id.

```c
CF_API size_t cf_feature_count(void);
CF_API const char* cf_feature_at(size_t i);
CF_API cf_bool cf_has_feature(const char* name);
```

Implementation: [`cf_feature_count` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L116), [`cf_feature_at` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L120), [`cf_has_feature` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L147).

What this build can do, as a NUL-terminated list: "ini", "json",
"real", and so on. Recorded in the provenance report as well, so a
report from a restricted build is distinguishable from a full one.
"lists" means the sized definition supports scalar lists, their
flat-source forms and append merging; it is present in every build
that implements them, including a minimal build.
Guarantee: nothrow. Reads a constant table; cf_feature_at gives NULL past the
end and cf_has_feature false for a NULL name.

```c
CF_API size_t cf_build_limit(const char* name);
```

Implementation: [`cf_build_limit` (lib/config/src/core/version.c)](https://github.com/develooper1994/configforge/blob/main/lib/config/src/core/version.c#L128).

A build limit, by name: "key" (CF_KEY_MAX), "source" (CF_SOURCE_MAX),
"message" (CF_MESSAGE_MAX), "path" (CF_PATH_MAX) and "depth"
(CF_MAX_DEPTH); 0 for a name it does not know. The first three size
cf_error's buffers, so they are part of a build's ABI (D-45): a
program, or a binding that mirrors cf_error, compiled against other
limits must not trust the layout, and this is how it can tell.
Guarantee: nothrow. 0 for a NULL or unknown name.

