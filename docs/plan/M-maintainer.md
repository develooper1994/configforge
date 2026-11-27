# M - the items that wait for the maintainer's word

These are the Roadmap items that change what a program may rely on (a new
language binding, the 2.0 breaks, the ways to read, a new interface for
digests) or that reach outside the repository (OSS-Fuzz, the registries).
Each opens only on the maintainer's approval, so each card's "needs" begins
with it, and each open question is a **TO DECIDE** with its options and a
recommendation; everything around the question is decided in full, so a card
can open the day the answer comes. Work that needs the maintainer's own
access (registry settings, the OSS-Fuzz application, new repositories,
submissions to other projects) is a card of its own with an id ending in
`-m`, "parked" (docs/Plan.md, "Waits on the maintainer"); the agent cards
beside it stand alone. Decisions D-171 to D-185 are reserved for this group.
Every card is read cold against the tree before it opens (docs/Plan.md,
"Read again after the layout changes").

Standard ACCEPT of every agent card below (docs/Plan.md:69-114):
ACCEPT-LINUX, ACCEPT-WINDOWS and ACCEPT-CI, plus the card's own check.

## M-1 - Go (cgo): the module over the C library

* **id**: M-1 (version 2.3.0)
* **goal**: `bindings/go/` is a Go module whose package `configforge` builds, resolves, reads and explains a configuration through cgo over the library's shared build, with the same refusals as the C and Python bindings, and `go test` passes against a library built by CMake.
* **why**: a Go program gets the same answer to "why is this value what it is" as a C, Rust or Python one, because the C library does the work (D-104's rule for every binding; `docs/Roadmap.md:147-148`).
* **needs**: the maintainer's approval (Go and .NET are "when asked", `docs/Roadmap.md:53`); the TO DECIDE below. Not registry publishing: the module is used from the repository (`replace` or a `go.work`) until M-23-m.
* **size**: large (a new binding)
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **TO DECIDE: how Go finds the library.** (a) cgo links `-lconfigforge` from `CF_GO_LIB`/`CGO_LDFLAGS` that the caller sets: nothing compiled by `go build`, the caller builds the shared library first (as `bindings/python` loads one, `bindings/python/src/configforge/_native.py`). (b) cgo compiles the C sources into the package (a copy of the amalgamation, `scripts/generate/amalgamate.py`, committed under `bindings/go/native/`), so `go get` needs only a C compiler, as the Rust crate does (`bindings/rust/build.rs`, `Cargo.toml:10-20`). **Recommendation: (b)**; it is the one that works from `go get`, and the amalgamation is already built and tested by the release (`release.yml:63`). The cards below are written for (b); under (a) step 3 of M-1 drops the copy and the `#cgo LDFLAGS` line names `-lconfigforge`.
  * **Module** `github.com/develooper1994/configforge/bindings/go`, package `configforge`, Go 1.21 or later, no dependencies outside the standard library.
  * **Surface**, mirroring the Python binding (`bindings/python/src/configforge/_config.py:156-470`) and no more: `type Builder`, `type Config`, `type Error` (an `error`: fields `Code int`, `Name, Key, Source, Message string`, `Line, Col int`; `Error()` returns `Name + ": " + Message`, with `Key` before the message when it is not empty, as `_config.py:21-46` formats). `App(name string, argv []string) *Builder`; `(*Builder)` methods `Cli(name, rank, argv)`, `Env(name, rank, prefix)`, `File(name, rank, path string, opts ...FileOption)` (`Optional`, `TrustedDir`, `AnyWriter` as options), `Memory(name, rank, map[string]any)`, `DefineBool`, `DefineInt(key, def, min, max)`, `DefineText`, `DefineReal`, `Schema(text)`, `SchemaFile(path)`, `Unknown(policy)`, and `Create() (*Config, error)`. Each method records the first error and returns the same `*Builder`; `Create` returns it. `(*Config)`: `Int(key) (int64, error)`, `Text`, `Bool`, `Real`, `Has(key) bool`, `Explain(key) (string, error)`, `Close()`. Ranks and flags are `const Rank*`/flag values equal to the C enum's (`lib/config/include/configforge/cf_types.h`, found by `grep -n "CF_RANK" lib/config/include/configforge/*.h`).
  * **Memory**: a `Config` is freed by `Close()` (`cf_config_free`) and, as a safety net, by a `runtime.SetFinalizer`; using a closed `Config` returns `Error{Name: "CF_E_STATE", Message: "configuration is closed"}`. Strings returned are Go copies (`C.GoStringN`); no Go pointer is kept by C (cgo's rule), argv and the memory map are copied into C memory freed by `Create`/`Close`.
  * **Refusals**: a `CF_E_*` code from C is passed through with its `cf_error` fields unchanged; the binding adds none of its own except "configuration is closed" and `Error{Name:"CF_E_ARG"}` for a value of a type `Memory` cannot hold (anything but string, bool, int, int64, float64).
  * **Rejected**: generating the binding with a tool (a dependency, and the surface is small); wrapping `cf_live` and `cf_edit` (the Python binding has them; a later card, when asked).
* **files**: `bindings/go/go.mod`, `bindings/go/configforge.go`, `bindings/go/native.go` (the `#cgo` and `#include`), `bindings/go/configforge_test.go`, `bindings/go/native/` (the amalgamation: `configforge.c`, `configforge.h`, from `scripts/generate/amalgamate.py`), `bindings/go/README.md`; last: `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/M-1`. Read `scripts/generate/amalgamate.py` (its docstring and `--help`) and `bindings/python/src/configforge/_config.py:156-470` once.
  2. Create `go.mod` (`module github.com/develooper1994/configforge/bindings/go`, `go 1.21`). Write `configforge_test.go` first: tests `TestDefaults` (builder with `DefineInt("server.port", 8080, 1, 65535)`, no sources: `Int` returns 8080), `TestMemoryWins` (a `Memory` source at `RankUser`-equivalent beats the default), `TestRangeRefused` (a memory value 70000 for that key: `Create` returns an `*Error` whose `Name` is `CF_E_RANGE` or what `tests/python/test_binding.py` expects for the same input (copy its expected `Name`)), `TestExplain` (the text contains the source's name), `TestClosed` (`Int` after `Close` returns the "configuration is closed" error).
  3. Stage the amalgamation: `python scripts/generate/amalgamate.py --out bindings/go/native` (use the flags `--help` shows; if it has no `--out`, copy its two outputs there by name). Add `native.go`: `// #cgo CFLAGS: -I${SRCDIR}/native -DCF_STATIC` and `// #include "configforge.h"`.
  4. See it fail: `cd bindings/go && go test ./...` fails to compile (no types yet). Note the failure.
  5. Write `configforge.go` as decided. Every C call through one helper `fromCError(*C.cf_error) error`. `go vet ./...` and `gofmt -l .` print nothing.
  6. `go test -race ./...` exits 0 on Linux. Add a ctest entry `go.test` beside `rust.cargo_test` (`examples/rust/CMakeLists.txt:97`) in a new `bindings/go/CMakeLists.txt` that runs `go test` only when `find_program(GO_EXE go)` succeeds, and `add_subdirectory(bindings/go)` where `examples/rust` is added.
  7. `CHANGELOG.md` and the docs below; format and generators as the loop says; ACCEPT.
* **accept**: the standard ACCEPT, and `go test -race ./...` exit 0 in `bindings/go` on Linux and on Windows (MinGW or MSVC's cl is not supported by cgo: Windows runs it under the MinGW-w64 gcc that `scripts/buildenv` names; if none is installed ACCEPT-WINDOWS notes `go.test` skipped by `find_program`, and CI is the proof).
* **stop if**: `amalgamate.py` cannot produce a header cgo can include (a macro it needs from a build option): stop, write the failing include under notes; a Go test needs more than the C library's own behaviour to pass (the binding would then differ from C): stop.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "Go: the `bindings/go` module over cgo". `docs/Roadmap.md`: "Wave 7" item 7 stays ⬜ until M-2 (CI job) is done; then 🟩. `docs/Decisions.md`: add D-171 (written here) — "**D-171 — The Go binding is cgo over the amalgamation.** The module compiles a committed copy of the amalgamation, so `go get` needs a C compiler only; its surface is Python's builder and config, no more; the C library's refusals pass through unchanged; a `Config` is closed by `Close` and by a finalizer. Rejected: linking a caller-built shared library (a build step Go programs do not expect), a generated binding."

## M-2 - Go: an example and a CI job

* **id**: M-2 (version 2.3.0)
* **goal**: `examples/go/` runs under ctest and under a CI job of its own, and the module's tests run in CI on Linux, macOS and Windows.
* **why**: wave 7 asks each binding for "its tests, an example and a CI job" (`docs/Roadmap.md:147-148`); a binding with no CI job is not held to the library.
* **needs**: M-1
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * The example is `examples/go/main.go`, package `main`, importing the module by a `replace` in `examples/go/go.mod` (`replace github.com/develooper1994/configforge/bindings/go => ../../bindings/go`). It prints exactly the lines of `examples/python`'s first example (find it: `ls examples/python`; the expected text is in `examples/expect.cmake`), so one expectation file serves both: add a `go.example` test beside the Python one in `examples/CMakeLists.txt` using `expect.cmake`'s helper the way the Rust example does (`examples/rust/CMakeLists.txt:60-95`).
  * The CI job is `go` in `.github/workflows/ci.yml`, a matrix `ubuntu-26.04`, `macos-latest` (the job names used by the existing macOS job at `ci.yml:137`: copy its `runs-on`), `windows-latest`; steps: checkout (the same pinned action line as `ci.yml:33`'s job uses), `actions/setup-go` pinned by commit as the file's other actions are (`ci.yml:35`'s form: SHA and `# vN` comment), Go `1.21` and `stable`, then `cd bindings/go && go vet ./... && go test -race ./...` and `cd examples/go && go run .` compared with `examples/expect.cmake`'s text through `cmake -P`. Windows runs `go test` with `CC=gcc` from the runner's MinGW; if `go test -race` is unsupported there, run without `-race` on Windows only.
* **files**: `examples/go/go.mod`, `examples/go/main.go`, `examples/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-2` from `work/M-1` (stacked, docs/Maintaining.md "A card cut from another card's branch") or from `main` once M-1 has landed.
  2. Write the expectation first: add the `go.example` test in `examples/CMakeLists.txt`; with no `main.go` it fails ("see it fail": `ctest -R go.example`).
  3. Write `main.go`; `ctest -R go.example` exits 0.
  4. Add the `go` job; check it with `actionlint` if installed and by reading `scripts/check/check_docs.py`'s workflow checks (`python scripts/check/check_docs.py`), which must exit 0.
  5. Push; ACCEPT-CI runs the three-platform job. A red platform is fixed in the card unless it is the amalgamation's (then stop).
* **accept**: the standard ACCEPT; `ctest -R "go\."` exit 0 on Linux; the `go` job green on three platforms.
* **stop if**: cgo cannot compile the amalgamation on Windows with the runner's gcc: stop, note the compiler error, and report it: the binding then ships Linux and macOS only until the maintainer says.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "Go: an example and a CI job on three platforms". `docs/Roadmap.md`: mark "Wave 7" item 7 🟩 only when M-4 is also done (one row, both bindings); until then leave it ⬜ and add nothing.

## M-3 - .NET (P/Invoke): the assembly over the C library

* **id**: M-3 (version 2.3.0)
* **goal**: `bindings/dotnet/ConfigForge/` is a .NET assembly (target `net8.0`, `netstandard2.0` is out of scope) whose `Builder`, `Config` and `ConfigForgeException` build, resolve, read and explain a configuration through P/Invoke over the shared library, and its xUnit tests pass.
* **why**: as M-1, for .NET programs (`docs/Roadmap.md:147-148`).
* **needs**: the maintainer's approval; the TO DECIDE below.
* **size**: large
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **TO DECIDE: where the native library comes from.** (a) the assembly loads `configforge` by `NativeLibrary` from the application's directory, and the package carries a built shared library per runtime identifier (`runtimes/<rid>/native/`), as the Python wheel carries its own (`pyproject.toml`'s scikit-build wheel). (b) a source build on install (not possible with NuGet). **Recommendation: (a)**; the cards are written for it. It needs a shared build per RID (linux-x64, win-x64, osx-arm64, the release's three, `release.yml:79-195`), made by the release workflow, which is the part M-4 cannot do before the registry card M-23-m publishes anything.
  * **Names** (PascalCase, .NET's rules): namespace `ConfigForge`; `Builder.App(string name, string[] argv)`, `.Cli`, `.Env`, `.File(name, rank, path, FileFlags flags = None)`, `.Memory(name, rank, IReadOnlyDictionary<string, object> values)`, `.DefineBool/Int/Text/Real`, `.Schema`, `.SchemaFile`, `.Unknown(UnknownPolicy)`, `.Create() : Config`. `Config : IDisposable`: `GetInt(key) : long` (throws `ConfigForgeException`), `TryGetInt(key, out long)`, `GetText`, `GetBool`, `GetReal`, `Has`, `Explain`. A refusal throws `ConfigForgeException` with `Code` (int), `Name`, `Key`, `Source`, `Line`, `Column`; the message is the C message as the Python `Error` writes it (`_config.py:21-46`). Disposed use throws `ObjectDisposedException`.
  * **Interop**: `LibraryImport` source-generated P/Invoke (not `DllImport`) with `StringMarshalling.Utf8`; `cf_error` is a `[StructLayout(LayoutKind.Sequential)]` struct whose arrays are fixed-size buffers sized by the library's own limits: `CF_KEY_MAX`, `CF_SOURCE_MAX`, `CF_MESSAGE_MAX` are read at run time from `cf_limits()`-style functions if the library exports them (`grep -n "CF_KEY_MAX" lib/base/include/configforge/*.h`), otherwise the assembly is built with the constants of the default build and checks `size` at load: it calls `cf_version` and the struct size function and refuses to load when they differ (`ConfigForgeException`, "native library built with other limits").
  * `SafeHandle` subclasses for `cf_config*` and `cf_builder*`; a builder handed to `Create` is released by it.
* **files**: `bindings/dotnet/ConfigForge/ConfigForge.csproj`, `Native.cs`, `Builder.cs`, `Config.cs`, `ConfigForgeException.cs`; `bindings/dotnet/ConfigForge.Tests/ConfigForge.Tests.csproj`, `BuilderTests.cs`; `bindings/dotnet/README.md`; `bindings/dotnet/CMakeLists.txt` (the `dotnet.test` ctest); last: `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/M-3`. Read `_config.py:156-470` once; `grep -n "CF_KEY_MAX\|CF_SOURCE_MAX\|CF_MESSAGE_MAX" lib/base/include/configforge/*.h` to settle the interop clause above and write the answer under "Organiser's answers" (CLAUDE.md: a choice that only follows the card's rules is the worker's).
  2. Create the two projects (`dotnet new classlib -f net8.0`, `dotnet new xunit -f net8.0`), the tests first, the same five as M-1 step 2 (`Defaults`, `MemoryWins`, `RangeRefused`, `Explain`, `Disposed`).
  3. See it fail: `dotnet test bindings/dotnet/ConfigForge.Tests` fails to build.
  4. Write the assembly as decided; the test run needs the shared library: `bindings/dotnet/CMakeLists.txt` builds nothing new, it runs `dotnet test` with `CONFIGFORGE_NATIVE_DIR=$<TARGET_FILE_DIR:configforge>` (the shared build's directory; `CMakeLists.txt:61-64` collects them into one) when `find_program(DOTNET_EXE dotnet)` succeeds, and the assembly's loader looks there first.
  5. `dotnet format --verify-no-changes` exits 0. `dotnet test` exits 0.
  6. Docs, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R dotnet.test` exit 0 on Linux and Windows when `dotnet` 8 is installed (otherwise skipped, and CI is the proof, M-4).
* **stop if**: the struct layout of `cf_error` differs between the shared builds of two platforms in a way the interop clause cannot absorb (padding): stop, write the two sizes under notes.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": ".NET: the `bindings/dotnet` assembly over P/Invoke". `docs/Decisions.md`: D-172 — "**D-172 — The .NET binding is P/Invoke over a shared library per runtime identifier.** Source-generated `LibraryImport`, handles that free what they own, the library's refusals thrown as one exception type carrying the code, key, source and place; the assembly refuses a native library built with other limits. Rejected: C++/CLI (Windows only), a managed port."

## M-4 - .NET: an example and a CI job

* **id**: M-4 (version 2.3.0)
* **goal**: `examples/dotnet/` runs under ctest, and a `dotnet` CI job builds the shared library on Linux, Windows and macOS and runs the assembly's tests and the example there.
* **why**: as M-2, for .NET.
* **needs**: M-3
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * The example is `examples/dotnet/Program.cs` (`net8.0` console, a `ProjectReference` to `ConfigForge.csproj`) printing the same lines as `examples/go` (M-2), the expectation being `examples/expect.cmake`'s; test name `dotnet.example` in `examples/CMakeLists.txt`.
  * The job `dotnet` in `.github/workflows/ci.yml`, matrix as M-2's; steps: checkout, `actions/setup-dotnet` pinned by commit with `dotnet-version: 8.0.x`, `cmake --preset default -DBUILD_SHARED_LIBS=ON` (the shared build `ci.yml:80`'s `shared` job already makes: copy its configure line), `cmake --build`, then `CONFIGFORGE_NATIVE_DIR=<build dir> dotnet test bindings/dotnet/ConfigForge.Tests` and `dotnet run --project examples/dotnet`.
* **files**: `examples/dotnet/examples.csproj` (name it `Example.csproj`), `Program.cs`, `examples/CMakeLists.txt`, `.github/workflows/ci.yml`; last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-4`. Add `dotnet.example` with its expectation; run `ctest -R dotnet.example`: it fails (no project).
  2. Write the example; the test exits 0.
  3. Add the job; `python scripts/check/check_docs.py` exit 0.
  4. Push; ACCEPT-CI.
* **accept**: the standard ACCEPT; the `dotnet` job green on three platforms.
* **stop if**: `LibraryImport` source generation fails on macOS arm64 with the runner's SDK: note the error, stop.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": ".NET: an example and a CI job". `docs/Roadmap.md`: with M-2 done, the "Wave 7" Go and .NET item (heading "**Wave 7**: Go (cgo) and .NET (P/Invoke) bindings", line 147) to 🟩, and the table row "Go and .NET bindings, when asked" (line 53) to 🟩.

## M-5 - The stream: one interface for reading in pieces, with a fallback every codec has

* **id**: M-5 (version 2.0.0)
* **goal**: `cf_codec` has a `decode_stream` member, `cf_codec_decode_stream` reads a `cf_feed` through any codec (a codec without a native stream is fed the whole input first, so the call always works), and an internal `cfi_window` gives a reader a bounded look back and ahead over a feed.
* **why**: D-61 part 3 leaves "a few kilobytes at a time" for 2.0: the peak, not the steady footprint, is what a device without room for the whole file needs (`docs/Decisions.md:2573-2620`). This card makes the interface and proves it with the fallback; M-6 to M-9 make the readers native, one card each.
* **needs**: the maintainer's approval; the TO DECIDE below
* **size**: large (a public interface)
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the shape of the chunk source.** (a) **Pull**: the library calls `read(ctx, buf, cap, err)` when its window is empty; the caller's code stays a plain function. (b) **Push**: the caller calls `cf_stream_feed(state, chunk, n)` and each reader is a resumable state machine: no look-back to manage, but all of `lib/serialize/src` (6,300 lines) is rewritten, and a TOML string measured and read again (D-61) is not expressible. **Recommendation: (a).** The cards are written for it; under (b) M-6 to M-9 are not written.
  * **Names** (in `lib/serialize/include/configforge/cf_serialize.h`, after `cf_blob`, lines 43-47; not `cf_source`, which is a builder's source, `lib/config/include/configforge/configforge.h:277`):
    `typedef struct cf_feed { size_t size; size_t (*read)(void* ctx, char* buf, size_t cap, cf_error* err); void* ctx; const char* name; } cf_feed;`
    `read` returns the bytes put in `buf` (at most `cap`), 0 at the end, `(size_t)-1` with `err` set on failure. `size` is `sizeof(cf_feed)` at the call site (D-51). Helpers: `cf_feed_mem(cf_feed*, const char* p, size_t n, const char* name)` and `cf_feed_file(cf_feed*, FILE*, const char* name)`, both setting `size`.
  * **The codec member.** `cf_codec` (`cf_serialize.h:221-245`) gets, after `data`: `cf_bool (*decode_stream)(const cf_codec* self, cf_feed* in, const cf_doc_sink* out, cf_error* err);`, read only when `cf_codec.size` reaches it (D-51, the rule `cf_codec_decode` already applies to `decode_self`, `cf_serialize.h:271-282`). `CF_API cf_bool cf_codec_decode_stream(const cf_codec* c, cf_feed* in, const cf_doc_sink* out, cf_error* err)`: the codec's `decode_stream` when it has one; else it reads `in` whole through `out->reserve_work` and calls `cf_codec_decode` (with no `reserve_work` in the sink: `CF_E_CAPABILITY`, "this sink has no room for a whole input; give it reserve_work"). Refusals and the UTF-8 check are `cf_codec_decode`'s (`lib/serialize/src/codec.c:279-290`); a native stream checks UTF-8 as it reads, carrying up to three bytes of a cut sequence between chunks.
  * **The window** (`lib/serialize/src/window.h`/`window.c`, internal): `cfi_window` over a `cf_feed` and a buffer of `CF_STREAM_WINDOW` bytes (a build option, default 4096, minimum 256, checked by CMake and by a `_Static_assert`; a row in `docs/Limits.md`, generated by `scripts/generate/limits.cmake`). Functions: `cfi_window_open(w, feed, buf, cap)`; `size_t cfi_window_peek(w, const char** p, size_t want)` (at least `want` bytes when the feed has them, never more than the buffer holds); `void cfi_window_take(w, size_t n)`; `size_t cfi_window_mark(w)`; `cf_bool cfi_window_rewind(w, size_t mark)` (valid within the last `cap - 1` bytes taken, else false); `cfi_window_utf8`. Line and column are counted as bytes are taken (as `cf_text_track_line`, `lib/text/src/space.c`), so a refusal names the same `line:col` as the same input read whole.
  * **Refusals.** A value or a look-back longer than the window: `CF_REASON_BUDGET` (`cf_error.h:57-58`, D-107), message "a value is longer than the read window (CF_STREAM_WINDOW bytes)", at the first byte of the value. A `read` failure: its own `err`, unchanged.
  * **Tests** (`lib/serialize/tests/test_stream.c`, C89, added to `cf_unit_tests(...)` at `lib/serialize/tests/CMakeLists.txt:6`): for each text codec compiled in, each fixture of `tests/codec` read whole and through `cf_codec_decode_stream` over a feed returning 1, 2, 7 and 4096 bytes a call gives the same events (a capturing `cf_doc_sink` of the file's own, like `test_text_escape.c:717-760`'s but keeping the events) and the same refusal text and `line:col`.
  * **Rejected**: a feed as a blob with a callback (a blob is a whole); making `decode` take a feed and dropping the blob form (breaks every caller).
* **files**: `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/codec.c`, `lib/serialize/src/window.c`, `lib/serialize/src/window.h`, `lib/serialize/CMakeLists.txt` (the source list: `grep -n "codec.c" lib/serialize/CMakeLists.txt`), `lib/serialize/tests/test_stream.c`, `lib/serialize/tests/CMakeLists.txt`, the root `CMakeLists.txt` (the option beside the other limits: `grep -rn "CF_MESSAGE_MAX" CMakeLists.txt cmake lib/base/include | head`); last: `docs/Limits.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/Codemap.md` and `docs/API.md` (generated).
* **steps**:
  1. `git switch -c work/M-5`. Add `CF_STREAM_WINDOW` the way `CF_MESSAGE_MAX` is added.
  2. Write `test_stream.c` (whole against pieces, every text codec); it calls `cf_feed_mem` and `cf_codec_decode_stream`.
  3. See it fail: `cmake --build --preset default 2>&1 | tail -n 20` shows the missing symbols.
  4. Add the types, `cf_feed_mem`/`cf_feed_file` and `cf_codec_decode_stream` with the fallback only. `ctest -R test_stream` exits 0.
  5. Write `window.c` with `test_window` in the same test file: peek, take, mark, rewind inside and beyond the window, a feed returning 1 byte a call, one failing on the third call, `cfi_window_utf8` with a sequence cut at every offset of a 4-byte character.
  6. `python scripts/work/work.py format`; the generators; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R "test_stream|test_window"` exit 0.
* **stop if**: `cf_codec` cannot grow without changing a size a test pins (`grep -rn "sizeof(cf_codec)" lib tests`); the fallback needs `reserve_work` in a sink the loader does not give (`lib/config/src/bridge/flatten.c`'s).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "`cf_feed`, `cf_codec_decode_stream` and the read window"; "### Changed": "`cf_codec` has a `decode_stream` member (D-173)". `docs/Roadmap.md`: the item "Whole or as a stream, in every format that can" (line 187) stays ⬜ until M-9. `docs/Decisions.md`: D-173 — "**D-173 — A stream is a pull feed and a bounded window.** `cf_feed` is read by the library when its window is empty; `cf_codec` gains `decode_stream`; a codec with none is fed the whole input, so the call works for every format; the window is `CF_STREAM_WINDOW` bytes (default 4096), a value or look-back past it is refused with `CF_REASON_BUDGET` at its first byte, and a refusal names the place the same input has read whole. Rejected: a push parser, a feed as a blob with a callback."

## M-6 - The stream, native: CBOR and TLV (the length-prefixed formats first)

* **id**: M-6 (version 2.0.0)
* **goal**: CBOR and TLV read through `cf_codec_decode_stream` natively with the window, a peak that does not grow with the input, and the events and refusals of the whole-blob read.
* **why**: a length-prefixed item names its length before its bytes, so a reader never looks back: a stream costs nothing there (`docs/Roadmap.md:187-190`: "the length-prefixed binary formats first").
* **needs**: M-5
* **size**: medium
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * `cbor.c` (969 lines) and `tlv.c` get a `decode_stream`. Each reads an item's head with `cfi_window_peek(w, &p, 9)` (the longest CBOR head). A text or byte string longer than the window goes whole to the sink's `reserve_text` buffer (`cf_serialize.h:135-140`), copied chunk by chunk from the feed and UTF-8-checked as copied (`cfi_window_utf8`).
  * One parse, not two: a local `struct cbor_src` with `peek`/`take` function pointers; the whole-blob `decode` points them at a `memcpy` over the blob, `decode_stream` at the window. The depth, count and size refusals are the whole read's (`grep -n "CF_REASON_" lib/serialize/src/cbor.c`), at the same bytes with the same messages.
  * Tests (`test_stream.c`): the loop of M-5 over the CBOR and TLV fixtures at feed sizes 1, 2, 7, 4096; a CBOR text string of `CF_STREAM_WINDOW * 3 + 5` bytes cut at every offset of a 4-byte character at a window edge; an array of 20,000 small items whose peak (a counting allocator) is within 256 bytes of the same array's of 200 items.
* **files**: `lib/serialize/src/cbor.c`, `lib/serialize/src/tlv.c`, `lib/serialize/tests/test_stream.c`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/M-6`. Add the peak and long-string tests.
  2. See it fail: the peak test fails (the fallback reads the whole array in): `ctest -R test_stream`.
  3. Introduce `cbor_src` so the existing `decode` runs over it unchanged: `ctest -R cbor` exit 0.
  4. Add `decode_stream` and register it in the CBOR codec's table; then TLV the same way.
  5. `ctest -R "test_stream|cbor|tlv|conformance"`; a fuzz minute (`cmake -DMODE=quick -DEXTRA=fuzz -DFUZZ_SECONDS=60 -DSOURCE=index -P scripts/verify.cmake`) shows no new crash.
  6. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_stream` exit 0; the fuzz minute clean.
* **stop if**: the whole read's refusal message cannot be reproduced at a byte of a stream (an indefinite-length string): stop, note the case; do not change the whole read's.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "CBOR and TLV read as a stream". Roadmap: nothing until M-9.

## M-7 - The stream, native: JSON, JSON5 and `.properties`

* **id**: M-7 (version 2.0.0)
* **goal**: the JSON, JSON5 and `.properties` readers read through `cf_codec_decode_stream` natively with a look-back of one token, refusing a value longer than the window as M-5 decided.
* **why**: the text readers that never read back are the next cheapest after the binary ones.
* **needs**: M-5; M-6 lands first so the source pattern is settled
* **size**: large
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * JSON: `js_scan` (`lib/serialize/src/json.c:41-51`) holds `p` and `end` into the blob; the stream form keeps them into the window's buffer, refilled by `cfi_window_peek(w, &s->p, 1)` when `p == end`. Every function that reads more than one byte ahead (`true`, `false`, `null`, a number's end, `\uXXXX` and a surrogate pair's second half, up to 12 bytes) peeks the bytes it needs first. A string longer than the window is copied piecewise into the `reserve_text` buffer. Positions go through the window's counters (`js_fail`, `json.c:103-105`).
  * `.properties` (`lib/serialize/src/properties.c`, 235 lines): a line with its continuation lines is one value that must fit `reserve_text`; the line end is `cf_text_eol` (`lib/text/src/space.c:31-36`) over the window's bytes, a longer line read in several peeks and copied as it goes.
  * Tests: M-5's loop over the JSON, JSON5 and properties fixtures; JSONTestSuite's `y_` and `n_` files at feed sizes 1 and 7: `scripts/check/conformance.py` gets a `--feed N` flag (this card adds it) and the result equals `tests/conformance/floor.txt`'s.
* **files**: `lib/serialize/src/json.c`, `lib/serialize/src/properties.c`, `lib/serialize/tests/test_stream.c`, `scripts/check/conformance.py`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/M-7`. Add `--feed N` to `conformance.py`, which fails with "stream not native" when the codec has no `decode_stream`; run `--feed 7` over JSONTestSuite: it exits 1 (see it fail).
  2. JSON native, one commit: `ctest -R "json|test_stream"`; conformance at `--feed 1`, `7`, `4096` equals the floor.
  3. JSON5 (the same functions, flag `json5`): the same checks.
  4. `.properties`: `ctest -R "properties|test_stream"`.
  5. A fuzz minute each, format, generators, ACCEPT.
* **accept**: the standard ACCEPT; conformance at three feed sizes equals the floor; fuzz clean.
* **stop if**: a conformance case differs at a feed size (a verdict or a refusal's place): note file and size, stop. A string longer than the window that the whole read accepts and the stream refuses is the card's decision (M-5's refusal), not a stop.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "JSON, JSON5 and `.properties` read as a stream". Roadmap: nothing until M-9.

## M-8 - The stream, native: INI and TOML (the readers that look back)

* **id**: M-8 (version 2.0.0)
* **goal**: INI and TOML read as a stream within the window, their look-backs (INI's earlier array run, TOML's string measured and read again) done with window marks, and a document needing more than the window refused with M-5's reason.
* **why**: D-61 names these two as the readers a stream reworks (`docs/Decisions.md:2573-2620`, part 3).
* **needs**: M-5; M-7
* **size**: large
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * TOML: `toml_scan` (`lib/serialize/src/toml.c:115-142`) holds pointers; `toml_string` measures then reads again. In stream mode a single-line string is copied piecewise into `reserve_text` once, with no measure pass; multi-line strings, arrays and inline tables go through the window. A key path and a table header must fit the window: the option's check (M-5) adds `CF_STREAM_WINDOW >= CF_KEY_MAX + 8`. TOML's record of what the document defined lives in `reserve_work` (`cf_serialize.h:142-149`), not in the window; its growth is part of the peak and is said in the `docs/Limits.md` row.
  * INI (`lib/serialize/src/ini.c`): the array run looks back one line; a `cfi_window_mark` is taken at each line start and rewound at most once per line. A line longer than the window is refused (M-5's reason).
  * A line end split across chunks (`\r` last in one, `\n` first in the next) is handled by peeking two bytes.
  * Tests: the loop over the INI and TOML fixtures at feed sizes 1, 2, 7, 4096; toml-test at `--feed 1` and `7` equal to the floor; a TOML value of `CF_STREAM_WINDOW + 1` bytes refused with M-5's message at its first byte (it passes through the fallback, so that test is the failure to see).
* **files**: `lib/serialize/src/toml.c`, `lib/serialize/src/ini.c`, `lib/serialize/tests/test_stream.c`, `CMakeLists.txt` (the option's check); last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/M-8`. Add the oversized-value test and the loop for INI and TOML.
  2. See it fail: the oversized test fails (the fallback accepts it).
  3. INI native: `ctest -R "ini|test_stream"`.
  4. TOML native, single-line strings, then multi-line, then arrays and inline tables; `ctest -R "toml|test_stream"` after each; toml-test at `--feed 1` and `7`.
  5. A fuzz minute each, format, generators, ACCEPT.
* **accept**: the standard ACCEPT; toml-test at the two feed sizes equal to the floor; fuzz clean.
* **stop if**: a TOML date-time or number needs a look-ahead past 64 bytes that a mark cannot give: stop, name the production.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "INI and TOML read as a stream". Roadmap: nothing until M-9.

## M-9 - The stream, native: YAML, and the file source that uses it

* **id**: M-9 (version 2.0.0)
* **goal**: the built-in YAML reader reads as a stream, and `cf_builder_file` with the flag `CF_STREAM` reads a file through a `cf_feed` over `FILE*` instead of into the arena, so the peak of reading a file is the window plus the keys it keeps.
* **why**: YAML's implicit keys look ahead along a line (D-61); the file source is where a program meets the stream (`lib/config/src/bridge/file.c:395-425` reads the bytes whole).
* **needs**: M-5, M-7, M-8
* **size**: large
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * YAML: `yp` (`lib/serialize/src/yamlparse.c:43-58`) looks ahead in `yp_is_key` and `yp_peek_node`; in stream mode the look-ahead is a window mark at the node's start, rewound once; a node whose line is longer than the window is refused (M-5's reason). The libyaml and libfyaml builds (`CF_YAML_LIBYAML`, `CF_YAML_LIBFYAML`) have no stream: their `decode_stream` is NULL and the fallback serves (said in `docs/Formats.md`'s YAML row).
  * `CF_STREAM` is a new `cf_builder_file` flag, the next free bit of the flags enum (`grep -n "CF_OPTIONAL" lib/config/include/configforge/*.h`); it applies to a file read by path (not `document`, not an environment variable). With it `file.c` makes a `cf_feed` by `cf_feed_file` and calls `cf_codec_decode_stream`, never `cf_blob_file`; the window buffer is wiped before it is freed, as the arena's bytes are (D-61, `cf_blob_arena_done`, `file.c:421`). A codec with no native stream reads the whole file as now: the flag is never refused. `cf_live` and Doctor use the same call and need no change.
  * Tests: the loop over YAML fixtures; yaml-test-suite at `--feed 7` equal to its floor; `lib/config/tests/test_stream_file.c`: a 3 MiB JSON file read with `CF_STREAM` has a peak (counting allocator) within 64 KiB of the same keys read from a 100-byte file, and the same values.
* **files**: `lib/serialize/src/yamlparse.c`, `lib/config/src/bridge/file.c`, the flags enum's header, `lib/config/tests/test_stream_file.c` (new; `lib/config/tests/CMakeLists.txt`), `docs/Formats.md`, `docs/Guide.md` (a paragraph, "reading a large file"); last: `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/M-9`. Write `test_stream_file.c` and the YAML loop; both fail (flag unknown; the fallback accepts a long YAML line).
  2. YAML native: `ctest -R "yaml|test_stream"`; yaml-test-suite at the three feed sizes.
  3. The flag and `file.c`; `ctest -R test_stream_file` exit 0; `ctest` whole.
  4. Fuzz, format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_stream_file` exit 0.
* **stop if**: the window's wipe cannot be done on every exit path of `file.c`'s load: stop.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "YAML read as a stream; `CF_STREAM` for a file source". `docs/Roadmap.md`: "Whole or as a stream, in every format that can" (line 187) to 🟩; the row "Sizing: transient file bytes done; stream and dynamic modes for 2.0" (line 40) stays 🟨 until M-11. `docs/Decisions.md`: D-174 — "**D-174 — A reader keeps one mark, and a stream refuses what outgrows its window.** TOML and INI look back one line and YAML ahead one line through a window mark; a value, key path or line longer than `CF_STREAM_WINDOW` is refused with `CF_REASON_BUDGET` at its first byte; the whole read of the same input is unchanged. `CF_STREAM` on a file source is never refused: a codec with no stream reads the file whole."

## M-10 - The dynamic sizing modes: the interface

* **id**: M-10 (version 2.0.0)
* **goal**: `cf_builder_sizing(b, &options)` exists with `CF_SIZING_MANUAL` (today, the default), `CF_SIZING_SEMI` and `CF_SIZING_DYNAMIC`, recorded in the configuration and read back by `cf_config_sizing`; with `MANUAL` nothing changes.
* **why**: D-61 part 1: manual sizing is the mode by build today; the other two need a 2.0 interface (`docs/Decisions.md:2573-2620`).
* **needs**: nothing
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **What grows**: the readers' scratch and the key space's keys and values here and in M-11a (the 255-byte refusal of the built-in readers, `cf_serialize.h:136-139`; the JSON reader's key buffer `scratch`, `lib/serialize/src/json.c:48`), and `cf_error`'s `key`, `source` and `message` in M-11b (`lib/base/include/configforge/cf_error.h:51-59`, which makes `cf_error` a struct that owns its text: D-61). Two cards, so that this interface lands first and M-11b's break is one card. Rejected: only the readers and the key space with `cf_error` left as arrays (a long key could be read and not reported: the error would cut it).
  * **Interface (D-220's convention, K-11a)**, in `lib/config/include/configforge/configforge.h` after `cf_builder_digest_key` (line ~465): `typedef enum cf_sizing { CF_SIZING_MANUAL = 0, CF_SIZING_SEMI = 1, CF_SIZING_DYNAMIC = 2 } cf_sizing;` `typedef struct cf_sizing_options { size_t size; cf_sizing mode; size_t cap; } cf_sizing_options;` `CF_API void cf_sizing_options_init(cf_sizing_options* o);` sets `size` to `sizeof` and the defaults, `mode` `CF_SIZING_MANUAL` (default) and `cap` 0 (default; the largest a single grown buffer may be, in bytes; 0 means none and is meaningful only for `MANUAL` and `DYNAMIC`). `CF_NODISCARD CF_API cf_bool cf_builder_sizing(cf_builder* b, const cf_sizing_options* o);` (NULL `o` is the defaults, that is `MANUAL`; `CF_FALSE` with the error held for a NULL `b`, a `size` below `offsetof(cf_sizing_options, cap) + sizeof(size_t)`, or `SEMI` with `cap` 0: `CF_E_ARG`, "semi-dynamic sizing needs a cap"). `CF_API cf_bool cf_config_sizing(const cf_config* c, cf_sizing_options* o);` fills `o` (its `size` is read first and kept) with the mode and cap the configuration was built with; `CF_FALSE` for a NULL argument. The last `cf_builder_sizing` call wins; once `cf_builder_create` has run the mode is fixed. Rejected: positional arguments `(b, mode, cap)` (D-220: a later member would break the signature).
  * A build with fixed limits accepts `MANUAL` only: the others return `CF_FALSE` with `CF_E_CAPABILITY`, "this build has fixed limits (profile minimal)". "Fixed limits" is a build option this card adds: `option(CF_FIXED_LIMITS ...)` in `cmake/ConfigForgeOptions.cmake` beside `CF_SINGLE_THREADED` (line 106; default `OFF`), passed as `-DCF_FIXED_LIMITS=1` and written into `cf_build_options.h` the way line 198 writes `CF_SINGLE_THREADED`; `cmake/profiles/minimal.cmake` sets it `ON` and no other profile does (`misra` allows the modes: the allocator is the program's).
  * Stored in `cf_defset` (where `cf_builder_digest_key` keeps its key, `b->defs.digest_key`, `lib/config/src/core/config.c:1136`) as the mode and the cap, and kept by a derived configuration the way `config.c:1742-1744` keeps the digest key (D-85's rule).
  * Tests (`lib/config/tests/test_sizing.c`): the three modes accepted; `SEMI` with cap 0 refused; NULL options is `MANUAL`; read back by `cf_config_sizing`; a derived configuration keeps it; the `minimal` preset refuses `SEMI` and `DYNAMIC` with `CF_E_CAPABILITY` (`ctest --preset minimal -R test_sizing`); `cf_sizing_options_init` equals the documented defaults member by member.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): what grows is the card's recommendation, in two cards; the interface takes an options struct under D-220 and K-11a's convention instead of three positional arguments; `CF_FIXED_LIMITS` did not exist in the tree and is added by this card (the maintainer's standing approval, 2026-11-27).
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/src/core/define.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_sizing.c`, `lib/config/tests/CMakeLists.txt`, `cmake/ConfigForgeOptions.cmake`, `cmake/profiles/minimal.cmake`; last: `docs/Limits.md` (a paragraph at its top), `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/M-10`. Write `test_sizing.c`.
  2. See it fail: the build fails (no symbols).
  3. Add the enum, the struct, the three functions, the storage and `CF_FIXED_LIMITS`; `ctest -R test_sizing` in `default` and `ctest --preset minimal -R test_sizing`.
  4. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_sizing` exit 0 in `default` and `minimal`.
* **stop if**: `cf_defset` cannot hold the pair without changing a size a test pins (`grep -rn "sizeof(cf_defset)" lib tests`).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "`cf_builder_sizing` and `cf_sizing_options`: manual, semi- and full-dynamic sizing (D-175)". `docs/Roadmap.md`: nothing until M-11. `docs/Decisions.md`: D-175 — "**D-175 — Sizing is a mode of the builder.** `MANUAL` is the build's limits, as before, and the only mode of a `minimal` build; `SEMI` grows a buffer from the allocator up to the program's cap; `DYNAMIC` has no cap but memory. The mode is stored with the definitions and kept by a derived configuration. What each mode lets grow is D-176."

## M-11a - Sizing in the readers and the key space

* **id**: M-11a (version 2.0.0)
* **goal**: with `SEMI` or `DYNAMIC` the built-in readers and the key space accept keys and values longer than the build's limits (up to the cap for `SEMI`); with `MANUAL` the suite's output is byte for byte what it was.
* **why**: D-61 part 1: a long key is refused today only because a buffer is fixed.
* **needs**: M-10
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * The 255-byte limit of the built-in readers is the absence of `reserve_text` (`cf_serialize.h:136-139`); with `SEMI`/`DYNAMIC` the loader's sink supplies one that grows (`grep -n reserve_text lib/config/src/bridge/flatten.c`), capped at `cap` for `SEMI`. JSON's key buffer, `scratch` (`lib/serialize/src/json.c:48`, sized `CF_KEY_MAX`), and TOML's and YAML's key buffers become `reserve_work` blocks of the size the key needs, doubled up to the cap (each growth one call: "Each call is new memory", `cf_serialize.h:142-149`); a key past the cap is refused with `CF_REASON_BUDGET`, "a key is longer than the sizing cap".
  * The key space's key limit is the mode's: `MANUAL` the build's `CF_KEY_MAX`, `SEMI` the cap, `DYNAMIC` bounded by `size_t` and the arena. The public `cf_key_valid` keeps the build's limit, as documented (`lib/base/include/configforge/cf_error.h:76-87` says so and no caller changes); the key space and the loader use an internal `key_valid_n(key, len, limit)` that `cf_key_valid` calls with `CF_KEY_MAX - 1`.
  * The sizing mode and cap reach the loader through `cf_config_sizing`'s storage (M-10); a reader the caller runs on its own with a sink of its own is unchanged (it supplies `reserve_text` or not, as before). Rejected: bigger stack buffers (the readers stay stack-bounded under `MANUAL`).
  * Where a message names a key longer than the old `CF_KEY_MAX`, M-11a's call sites keep the message short with the existing `cf_error_set` (still cut at `CF_MESSAGE_MAX`); M-11b gives the errors the room.
  * Tests (`test_sizing.c`): a 600-byte key and a 4,000-byte string in JSON, TOML, INI and YAML refused under `MANUAL` with today's message and accepted under `DYNAMIC`; `SEMI` with cap 1024 accepts 600 and refuses 2000.
* **files**: `lib/config/src/bridge/flatten.c`, `lib/config/src/core/keyspace.c`, `lib/serialize/src/json.c`, `toml.c`, `yamlparse.c`, `ini.c`, `lib/config/tests/test_sizing.c`, `docs/Limits.md`; last: `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/M-11a`. Extend `test_sizing.c` with the cases.
  2. See it fail: `DYNAMIC` still refuses them.
  3. The loader's growing callbacks, then the key space; `ctest -R "test_sizing|json|toml|ini|yaml"`.
  4. `ctest` whole in `default`: `MANUAL` unchanged.
  5. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_sizing` exit 0.
* **stop if**: `MANUAL` output differs from today's anywhere.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): the card has no choice of its own; M-10's answer on what grows holds, and `cf_key_valid` is not changed (the maintainer's standing approval, 2026-11-27).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "semi- and full-dynamic sizing in the readers and the key space". `docs/Decisions.md`: D-176 (below, with M-11b).

## M-11b - `cf_error` owns its text (the 2.0 break)

* **id**: M-11b (version 2.0.0)
* **goal**: `cf_error`'s `key`, `source` and `message` are no longer fixed arrays, a 3,000-byte message round-trips under `DYNAMIC`, and every caller, test and binding compiles and passes.
* **why**: D-61 part 1 predicts it: a message cannot grow while `cf_error` holds an array. Layout's stage 0.5 (the error model, D-223) puts this card with the place and reason fields; it needs M-11a's modes, so it lands after them, in the same 2.0.0 release (`cf_error` still changes shape once in a release).
* **needs**: M-11a
* **size**: large
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **The shape: `cf_error` owns its text.** `key`, `source` and `message` become `const char*` (`""` when unset, never NULL after `cf_error_clear` or any `cf_error_set`; a struct filled with zeros before its first use has NULL pointers, as it had empty arrays, and a caller prints them only after a failure). The text lives in the error: a small private inline store the size of the old arrays (`CF_KEY_MAX`, `CF_SOURCE_MAX`, `CF_MESSAGE_MAX`) takes every text that fits, so an error with nothing long costs no allocation and `MANUAL`, `minimal` and the `none` platform behave and cost as today; a text that does not fit goes to one heap block from `cf_default_allocator()` (`lib/base/include/configforge/cf_alloc.h:29`) that the error owns; `cf_error_clear` frees it and `cf_error_copy` copies the text (inline or heap) into `dst`'s own store, so a copy never points into `src`. A build with `CF_FIXED_LIMITS` (M-10) never allocates: a text that does not fit is cut at the old array's size, as today. When the allocation fails the text is cut the same way. Because the pointers may point into the struct itself, a `cf_error` is never copied by value (the header's doc at `cf_error.h:61-67` already says so; it gains the sentence "a plain assignment leaves the texts pointing into the source"). Rejected: (b) the arrays kept and a `char* long_message` added (`key` and `source` would still be cut, and a long key is what M-11a lets through); (c) never growing (D-61 predicts this card and the dynamic modes need it).
  * **`cf_error_set` is unchanged in behaviour**: it cuts the message at `CF_MESSAGE_MAX - 1`, the key at `CF_KEY_MAX - 1` and the source at `CF_SOURCE_MAX - 1`, so that every existing call site and every `MANUAL` output is byte for byte what it was. New: `CF_API cf_bool cf_error_set_long(cf_error* e, cf_code code, const char* key, const char* source, size_t cap, const char* fmt, ...);`, the same with each of the three texts allowed `cap` bytes (the default 0 means no cap, as D-220 point 4 asks; the build's fixed limits when `CF_FIXED_LIMITS`); it returns `CF_FALSE` like `cf_error_set`. The places M-11a changed (they know the mode) call it with the mode's cap from `cf_config_sizing`; every other call site keeps `cf_error_set`.
  * `CF_ERROR_INIT` stays (`sizeof`, code 0, the three pointers at `""`, an empty place; the private store zeroed), still fenced from the formatter. The 1.x fields `code`, `where`, `reason` and K-6a's `offset` are unchanged and keep their places; the pointers take the places of the arrays and the private store follows `reason`/`offset`, so that D-51's rule (a member added later is read only where `size` reaches it) still holds.
  * Callers: `grep -rn "\.message\[\|\.key\[\|\.source\[" lib tests examples bindings tools | head -40` lists the sites that index the arrays; each changes to the pointer form; `bindings/rust/src/sys.rs` and `bindings/python/src/configforge/_native.py` redefine the struct and change with it.
  * D-176 text: "**D-176 — Sizing grows what the allocator holds, and `cf_error` owns its text.** Under `SEMI` and `DYNAMIC` the readers' scratch and the key space's keys follow the mode's cap; `cf_error`'s texts are owned and freed by `cf_error_clear`, so a long message round-trips. `MANUAL` is unchanged. Rejected: bigger stack buffers, a second error struct, arrays kept beside a long-message pointer."
  * Test: `test_sizing.c`: a 3,000-byte message round-trips through `cf_error_set_long` (cap 0) and `cf_error_copy`; `cf_error_clear` frees it (a counting allocator: no live block after clearing both errors); the same message through `cf_error_set` is cut at 255 bytes (`MANUAL` unchanged); a 600-byte key is kept whole by `cf_error_set_long` and cut by `cf_error_set`; the copy's pointers do not point into the source.
* **files**: `lib/base/include/configforge/cf_error.h`, the file holding `cf_error_set`/`cf_error_copy` (`grep -rln cf_error_copy lib/base/src`), the callers the grep lists, `bindings/rust/src/sys.rs`, `bindings/python/src/configforge/_native.py`, `lib/config/tests/test_sizing.c`; last: `CHANGELOG.md`, `docs/Decisions.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-11b`. Add the round-trip test; it does not compile against arrays of 256 bytes (see it fail).
  2. Change the struct, `cf_error_set`, `cf_error_copy`, `cf_error_clear`.
  3. Fix every caller by the grep; `cmake --build` clean.
  4. `ctest` whole; `ctest -R "rust|python"`.
  5. `gate.abi`: regenerate the baseline by the command its header names (`grep -rn "gate.abi" tests/CMakeLists.txt`); the diff is part of M-13's list.
  6. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; the bindings' tests exit 0.
* **stop if**: a binding cannot read the struct through a pointer: note, stop.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): the shape is the card's recommendation (a), with the inline store it already described made explicit so that `MANUAL`, `minimal` and the `none` platform need no allocator; `cf_error_set` keeps its truncation and `cf_error_set_long` is the new call; the break is declared in 2.0.0 and listed by M-13 (the maintainer's standing approval, 2026-11-27).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Changed": "`cf_error`'s `key`, `source` and `message` are owned text, not arrays (D-176)". `docs/Roadmap.md`: the row "Sizing: transient file bytes done; stream and dynamic modes for 2.0" (line 40) to 🟩. `docs/Decisions.md`: D-176 as above.

## M-12 - The image header's layout, version 2

* **id**: M-12 (version 2.0.0)
* **goal**: image major 2 has a 128-byte header carrying the build's limits and the build id; major 1 images still load; `cf_image_write` writes major 2 by default and major 1 with `CF_IMAGE_V1`.
* **why**: "limits and the build id in the report and the image header" was done for the report only; "the image header keeps its layout until 2.0" (`docs/Roadmap.md:405-416`); the 12 reserved bytes at 52-63 (`docs/ImageFormat.md:34-55`) cannot hold the limits.
* **needs**: nothing
* **size**: medium
* **version**: 2.0.0
* **status**: open
* **decided**:
  * **What major 2 changes: the header grows; offsets stay 32-bit.** No target an image is for needs 4 GiB, and a requirement the Roadmap item does not ask for is not added. Rejected: 64-bit offsets (an image over 4 GiB), a section table (the manifest of Y-33 has a place in the header instead).
  * **Layout, major 2**: bytes 0-51 as major 1 except `major` = 2, `minor` = 0; 52-63 reserved zero; at 64 `key_max`, `source_max`, `message_max`, `max_depth` (u16 each: the build's `CF_KEY_MAX`, `CF_SOURCE_MAX`, `CF_MESSAGE_MAX`, `CF_MAX_DEPTH`); at 72 `build_id`, 32 bytes, at most 31 bytes of the text `cf_build_id()` returns (`lib/config/include/configforge/cf_version.h:64`; the report writes the whole id) and a NUL, NUL-padded; at 104 `manifest_off` and at 108 `manifest_len` (u32 each, **reserved for Y-33**: both zero in 2.0.0, written zero, and the reader accepts any value there as it accepts any reserved byte, so an image of a later minor that fills them opens); 112-127 reserved zero. `index_off` is never below 128. Little-endian, offsets multiples of 8. Y-33 (2.2.0) fills the two manifest fields and raises `minor` to 1 with no change of layout and no new major.
  * `CF_IMAGE_MAJOR` 2, `CF_IMAGE_MINOR` 0 (`lib/config/include/configforge/cf_image.h:41-42`). `cf_image_open` (`lib/config/src/bridge/image.c:355-437`) accepts major 1 and 2 and ignores every reserved byte and every field of a higher minor (as `ImageFormat.md` says: "accept a higher one; fields only grow"). `cf_image` (`lib/config/include/configforge/cf_image.h:65-79`) gains `cf_u16 key_max, source_max, message_max, max_depth;` and `const char* build_id` (zero and NULL for major 1), taken from the struct's `reserved[4]` room where they fit and appended where they do not: a struct change a 2.0 may make, listed by M-13. A major 2 image whose `key_max` exceeds this build's `CF_KEY_MAX` is refused: "image was made with longer keys (its limit N, this build's M)".
  * `CF_IMAGE_V1`, a new bit of the `flags` argument `cf_image_write` already takes (`CF_IMAGE_V1 = 1u << 1`, beside `CF_IMAGE_PROVENANCE`; a write-time request that is not stored in the image's own `flags`), writes major 1: no options struct is added because the call already has its option word (D-220 asks for a struct where a call has none), and a signature change is a break this card does not need. The default (no flag) writes major 2. The determinism digest is the same in both (it is of the key space, not of the header).
  * Tests (`lib/config/tests/test_image.c`; `lib/config/tests/test_image.c`): a major 2 image round-trips and reads back the limits and id; a major 1 fixture `tests/support/data/image-v1.cfgb` (written by the unchanged tree, step 1; the test finds it through a `CF_IMAGE_DATA` definition in `lib/config/tests/CMakeLists.txt`, as `CF_EDGES_DIR` is defined there) loads with `build_id == NULL`; major 3 refused with today's message; `key_max` above the build's refused with the message above; a major 2 image with `minor` 1 and nonzero bytes at 104-111 (built by hand in the test) opens; the `image` fuzzer reads both: `feed_image` (`lib/config/tests/test_fuzz.c:305`) opens whatever bytes it is given and needs no change, and `image-v1.cfgb` is added to the image seeds where the fuzz targets keep them (`grep -n image tests/fuzz/CMakeLists.txt`).
* **files**: `lib/config/include/configforge/cf_image.h`, `lib/config/src/bridge/image.c`, `lib/config/tests/test_image.c`, `tests/support/data/image-v1.cfgb`, `lib/config/tests/CMakeLists.txt`, `docs/ImageFormat.md`; last: `CHANGELOG.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/M-12`. On the unchanged tree write a small image with the existing writer (`grep -rn cf_image_write tools lib/config/tests | head -3`) to `tests/support/data/image-v1.cfgb`; commit it alone.
  2. Extend `test_image.c`; it does not build (no `CF_IMAGE_V1`).
  3. See it fail.
  4. Header, writer, opener; `ctest -R image` exit 0 (the v1 fixture loads).
  5. `docs/ImageFormat.md`: a table for major 2 and a paragraph on loading major 1; format; generators; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R image` exit 0; a fuzz minute on `image` clean.
* **stop if**: a consumer outside `lib` hardcodes 64 bytes and cannot be changed (`grep -rn CF_IMAGE tests/consumer examples`).
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): option (b), the card's recommendation; bytes 104-111 are reserved for the manifest of Y-33, which moves to 2.2.0 and uses them (the maintainer's standing approval, 2026-11-27).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Changed": "the image header is 128 bytes and carries the limits and the build id; major 1 images still load (D-177)". `docs/Decisions.md`: D-177 — "**D-177 — Image major 2: a 128-byte header with the build's limits and id.** The 12 reserved bytes could not hold them; offsets stay 32-bit; a major 1 image loads (`build_id` NULL); a major 2 image from a build with longer keys is refused naming both limits. Rejected: 64-bit offsets, a section table. Bytes 104-111 are kept for the manifest."

## M-13 - 2.0, once: the list of what breaks, and the gate that holds it

* **id**: M-13 (version 2.0.0)
* **goal**: `docs/Migrating-2.0.md` lists every break of 2.0 (what, since which D, what to write instead), `CHANGELOG.md`'s "## 2.0.0 — unreleased" has each under "### Changed", the version is 2.0.0, and `gate.abi`'s baseline is the 2.0 interface.
* **why**: "what the sizing modes break, once" (`docs/Roadmap.md:58`, `405-417`) and `docs/Maintaining.md:111`: each break "listed under the release's `### Changed` with its D-number".
* **needs**: M-11b, M-12
* **size**: small
* **version**: 2.0.0
* **status**: open
* **decided**:
  * One table, `| What changes | Since | Before | Now | D |`, a row per break and only those M-11b and M-12 made (`cf_error` owned text, D-176; image major 2, D-177; and whatever `gate.abi`'s diff shows: step 2; the `### Changed` entries K-6a, K-6b and K-9c made in the 1.9.0 and 2.0.0 sections are listed too, each with the D-number its card gives). The stream (M-5 to M-9) is not part of 2.0.0: it comes in 2.4.0 and is additive (M-5 grows `cf_codec` with a `decode_stream` member that is read only where `size` reaches it, D-51), so 2.0.0 does not wait for it and the list needs none of its cards. Nor is the manifest in the image (Y-33, 2.2.0): M-12 reserves its place in the header, so it breaks nothing later. M-10 (the sizing interface) and M-11a add and break nothing.
  * The version is bumped by `scripts/release/version.cmake` with the argument its header comment documents (`Cargo.toml:3`, `pyproject.toml:7` and the rest are its job), never by hand. **The tag is not a step of this card** (docs/Plan.md, "For the maintainer": the organiser tags a milestone whose cards are done with CI green).
* **files**: `docs/Migrating-2.0.md` (new, and `mkdocs.yml`'s nav), `CHANGELOG.md`, the version files `version.cmake` writes, `gate.abi`'s baseline; last: `docs/Roadmap.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/M-13`. `grep -rn "gate.abi" tests/CMakeLists.txt | head -3`; read the baseline's header comment.
  2. Run `ctest -R gate.abi`: it fails (the interface moved since the baseline; the list is the "see it fail").
  3. Write `Migrating-2.0.md` from that list and from the "### Changed" entries of M-11b and M-12 and of the cards of stage 0 (K-6a, K-6b, K-9c).
  4. Bump the version; regenerate the baseline; `ctest -R gate.abi` exit 0.
  5. `python scripts/check/check_docs.py` exit 0; format; generators; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R "gate.abi|gate.docs"` exit 0.
* **stop if**: `gate.abi` shows a break no card of this group made: it is a decision; write it under notes and stop.
* **Organiser's answers** (2026-11-27, the organiser, under the maintainer's standing approval): the card had no open question; it no longer needs M-9, because 2.0.0's breaks come early (docs/plan/Layout.md) and the stream is 2.4.0's additive work; the tag is the organiser's under the standing approval once the 2.0.0 cards are done with CI green, and still not a step of this card (the maintainer's standing approval, 2026-11-27).
* **docs**: `CHANGELOG.md`: the 2.0.0 section's "### Changed" complete. `docs/Roadmap.md`: the rows "2.0: what the sizing modes break, once" (line 58) and "**Wave 8, 2.0**" (line 151) to 🟩. `docs/Decisions.md`: D-178 — "**D-178 — 2.0 breaks once, and says so.** The breaks are those D-176 and D-177 name, listed in `docs/Migrating-2.0.md` with before and after; `gate.abi`'s baseline is the 2.0 interface; nothing else breaks in 2.0."

## M-14 - Ways to read: the document tree (DOM), a public type of the serialization library

* **id**: M-14 (version 2.0.0)
* **goal**: `configforge/cf_dom.h` offers `cf_dom`, a document read once through any codec and walked many times, behind the option `CF_WITH_DOM`, with the same refusals as the event read.
* **why**: "a document rather than a configuration" (`docs/Roadmap.md:1235-1257`): the readers already give events and that interface is public (`cf_doc_sink`, `lib/serialize/include/configforge/cf_serialize.h:126-150`); `configforge codec convert` builds a tree of its own (`tools/codec/convert_core.c`, `cv_convert` 858-895). This card moves that idea into the library as a public type and leaves the tool using it.
* **needs**: the maintainer's approval; the TO DECIDE below
* **size**: large (a public interface)
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the tree's shape.** (a) A **node tree**: one `cf_dom_node` per table, array and scalar, parent and child links, in an arena the `cf_dom` owns; walked by `cf_dom_root`, `cf_dom_child`, `cf_dom_next`. (b) A **flat index** like the key space's (path strings to values): smaller, but a document with arrays of tables and repeated names (XML, TOML array tables) is not a key space (`docs/Architecture.md`, "What is deliberately not here"). **Recommendation: (a)**; it is what "a document rather than a configuration" asks for. Written for (a).
  * **Names.** `typedef struct cf_dom cf_dom;` `typedef struct cf_dom_node cf_dom_node;` (opaque). `CF_API cf_dom* cf_dom_read(const cf_codec* c, const cf_blob* in, const cf_allocator* alloc, cf_error* err);` (NULL with `err` set; the tree is not partial); `cf_dom_free(cf_dom*)`; `cf_dom_root(const cf_dom*)`; `cf_dom_kind(const cf_dom_node*)` returns `CF_DOM_TABLE`, `CF_DOM_ARRAY` or `CF_DOM_SCALAR`; `cf_dom_name(const cf_dom_node*, size_t* n)` (the member's name, NULL for an array element and the root); `cf_dom_first(const cf_dom_node*)`, `cf_dom_next(const cf_dom_node*)`; `cf_dom_find(const cf_dom_node* table, const char* name, size_t n)` (the first member of that name; a table may hold repeated names, as XML does); `cf_dom_value(const cf_dom_node*, cf_value*)` (a scalar); `cf_dom_at(const cf_dom_node*)` (the `cf_loc` of the node: `cf_serialize.h`'s `cf_loc`). `cf_dom_write(const cf_dom*, const cf_codec*, cf_sink*, cf_error*)` writes it through an encoder (`cf_codec.encoder`, `cf_serialize.h:234`).
  * **Memory**: all nodes, names and texts in one arena from `alloc` (`cf_allocator`, as `cf_build` takes: `configforge.h:91`); `cf_dom_free` wipes and frees it. Depth is `CF_MAX_DEPTH`, the readers' own. A tree reads whole: no stream (M-5 to M-9 feed events, a DOM of a stream is the same events).
  * **Option** `CF_WITH_DOM` (default ON in every profile but `minimal`), added to `cmake/ConfigForgeOptions.cmake` beside `CF_WITH_EMIT`; compiled out, each function returns `CF_E_CAPABILITY` naming the option (the file's own rule, its header comment lines 1-10).
  * **The tool** `tools/codec/convert_core.c` is moved onto `cf_dom_read` and `cf_dom_write` only where its tree is the same shape (step 6); a difference is not forced.
  * **Rejected**: mutation of a tree (the edit layer is `cf_edit`, which keeps comments and layout; a DOM here is read-only); a JSON-pointer query language.
  * Tests (`lib/serialize/tests/test_dom.c`, added to `cf_unit_tests(...)`, `tests/CMakeLists.txt:6` of that directory): every fixture of `tests/codec` read to a DOM then written through the same codec gives what the codec's own `cf_emit` of the events gives; a repeated-name table keeps both members in order; locations equal the events'; a refusal is the event read's; the compiled-out build's message.
* **files**: `lib/serialize/include/configforge/cf_dom.h`, `lib/serialize/src/dom.c`, `lib/serialize/CMakeLists.txt`, `cmake/ConfigForgeOptions.cmake`, `lib/serialize/tests/test_dom.c`, `lib/serialize/tests/CMakeLists.txt`, `tools/codec/convert_core.c`; last: `docs/Guide.md` (a short section), `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/M-14`. Add `CF_WITH_DOM` and the empty header; write `test_dom.c`.
  2. See it fail: it does not build.
  3. Write `dom.c` (a `cf_doc_sink` that builds the nodes, so every codec works); `ctest -R test_dom` exit 0, with the option OFF too (`-DCF_WITH_DOM=OFF`, the refusal test only).
  4. `cf_dom_write` through `cf_emit`'s walk.
  5. Move `convert_core.c` onto it if its tree is the same shape; `ctest -R "pairs|codec"` unchanged.
  6. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_dom` exit 0 with the option on and off.
* **stop if**: `convert_core.c`'s tree cannot be the library's (an extra field the tool needs): leave the tool, note it; a codec's events cannot rebuild its own output (comments, order): note which, do not widen the type.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "`cf_dom`: a read-only document tree (`CF_WITH_DOM`)". `docs/Roadmap.md`: "Ways to read: DOM, SAX, lazy, in-situ" (line 1235) stays ⬜ until M-17. `docs/Decisions.md`: D-179 — "**D-179 — A DOM is a read-only node tree built from the events.** `cf_dom` holds tables, arrays and scalars with their names and places in one arena the tree owns; repeated names are kept; it is written back through any encoder; compiled out by `CF_WITH_DOM`. Rejected: a flat key index (a document is not a key space), mutation (that is `cf_edit`)."

## M-15 - Ways to read: lazy, a structural index first

* **id**: M-15 (version 2.0.0)
* **goal**: `cf_lazy_open` builds a structural index of a JSON or CBOR document without parsing values, and `cf_lazy_get_*` parses only the value asked for, behind `CF_WITH_LAZY`, with the event read's refusals for the values it touches.
* **why**: "for reading three keys of a large document" (`docs/Roadmap.md:1244-1246`).
* **needs**: the maintainer's approval; the TO DECIDE below; M-14 (the node vocabulary)
* **size**: large
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * **TO DECIDE: which formats.** (a) JSON and CBOR only (JSON's structure is found by a scan for `{ } [ ] , : "`; CBOR's by item heads). (b) Every text codec. **Recommendation: (a)**: TOML, YAML and INI's structure depends on line context and is not found without parsing (YAML's `yp_is_key` looks ahead, `yamlparse.c`). The others refuse: `CF_E_CAPABILITY`, "lazy reading is for JSON and CBOR".
  * **Names.** `typedef struct cf_lazy cf_lazy;` `CF_API cf_lazy* cf_lazy_open(const cf_codec* c, const cf_blob* in, const cf_allocator* alloc, cf_error* err)` (the blob must outlive the `cf_lazy`: it is borrowed, said in the header); `cf_lazy_free`; `cf_lazy_root(const cf_lazy*)` a `cf_lazy_ref` (a small value struct: index position); `cf_lazy_member(cf_lazy_ref table, const char* name, size_t n, cf_lazy_ref* out)` (false when absent); `cf_lazy_element(cf_lazy_ref array, size_t i, cf_lazy_ref* out)`; `cf_lazy_count(cf_lazy_ref)`; `cf_lazy_value(cf_lazy_ref, cf_value*, cf_error*)` (parses that one scalar: a bad number refuses here, with its place). The index is validated at open only as far as structure goes: unbalanced brackets, a string never closed, depth past `CF_MAX_DEPTH` refuse at `cf_lazy_open`; a malformed value inside refuses when asked for, never earlier. **Said in the header and the Guide**: a lazy read accepts some documents the event read refuses, until the value is touched; the event read is the validator.
  * **The index**: one array of 8-byte entries in an arena from `alloc` (byte offset of each structural token, 32-bit, and its kind); built by `lanes_best()`'s `find_byte`/`find_any` where P-3 has made them available (`docs/plan/P-word-at-a-time.md`), by a plain loop until then: the card writes the plain loop and does not depend on P-3.
  * Option `CF_WITH_LAZY`, default ON except in `minimal`, as M-14's.
  * Tests (`lib/serialize/tests/test_lazy.c`): three keys of a 5 MiB JSON read lazily equal the DOM's (M-14); the number of values parsed (a counter in the test's build via `CF_LAZY_COUNT`, a macro that is 0 in a normal build) is 3 plus the path to them; an unbalanced document refuses at open with the event read's `line:col`; a bad number in an untouched member does not refuse; JSONTestSuite's `y_` files all open and read equal to the event read's values.
* **files**: `lib/serialize/include/configforge/cf_lazy.h`, `lib/serialize/src/lazy.c`, `lib/serialize/CMakeLists.txt`, `cmake/ConfigForgeOptions.cmake`, `lib/serialize/tests/test_lazy.c`; last: `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/M-15`. Option, header, `test_lazy.c`.
  2. See it fail: it does not build.
  3. JSON index and `cf_lazy_value`; `ctest -R test_lazy`.
  4. CBOR index; the same tests over the CBOR fixtures.
  5. A fuzz target `lazy` in `tests/fuzz/CMakeLists.txt` (the `readers` list at line 25: its number is in `libfuzzer.c`'s `CF_FUZZ_TARGET` numbering; add it at the end and update both): a minute clean.
  6. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_lazy` exit 0; the fuzz minute clean.
* **stop if**: the index of a JSON string containing `\"` cannot be found without the escape rules (it can: count the backslashes before the quote; if a case defeats that, stop and note it).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "`cf_lazy`: a structural index and values parsed when asked for (`CF_WITH_LAZY`)". `docs/Roadmap.md`: nothing until M-17. `docs/Decisions.md`: D-180 — "**D-180 — Lazy reading indexes structure and parses on request.** JSON and CBOR only; the index is validated for structure at open and a value when touched, so a lazy read accepts what the event read refuses until it is asked for it; the blob is borrowed. Rejected: lazy TOML, YAML and INI (their structure is found by parsing)."

## M-16 - Ways to read: in-situ strings

* **id**: M-16 (version 2.0.0)
* **goal**: `cf_codec_decode_insitu` reads a JSON document in a buffer the caller gives up, unescaping strings in place and handing them to the sink as pointers into that buffer, with no copy and no `reserve_text` call, behind `CF_WITH_INSITU`.
* **why**: RapidJSON's in-situ mode (`docs/Roadmap.md:1247-1250`): no copy, no allocation for strings; a binary format is in-situ by nature (`:1255-1257`), so CBOR's text strings are passed by pointer into the blob in the same call.
* **needs**: the maintainer's approval; the TO DECIDE below
* **size**: medium
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the buffer.** (a) A `char*` and a length the caller owns and **gives up** (the call writes into it: unescaped strings shrink in place, each followed by a NUL written over the closing quote). (b) A copy made by the library first (no gain). **Recommendation: (a)**, the only form that is in-situ. Written for (a).
  * **Names.** `CF_API cf_bool cf_codec_decode_insitu(const cf_codec* c, char* buf, size_t n, const char* name, const cf_doc_sink* out, cf_error* err);` For JSON: the sink's `scalar` receives `v->u.t.p` pointing into `buf` (a text, NUL-terminated, valid until the caller reuses the buffer); `reserve_text` is not called. For CBOR (text strings are already unescaped): the pointer is into `buf` and the string is not NUL-terminated by the reader unless the byte after it is a head, so the call copies nothing and `textual` stays as the reader sets it; the header says a CBOR text is length-delimited and not terminated. For any other codec: `CF_E_CAPABILITY`, "in-situ reading is for JSON and CBOR". An unescape that would grow (never in JSON: `\uXXXX` shrinks) cannot happen; a string with an invalid escape refuses as the whole read does.
  * **After the call the buffer's bytes are not the document**; the header and the Guide say so.
  * Option `CF_WITH_INSITU`, default ON except `minimal`.
  * Tests (`lib/serialize/tests/test_insitu.c`): the events equal the whole read's for every JSON fixture with escapes; the sink's string pointers lie inside `[buf, buf+n)` (checked in the test's sink); no allocation (a counting allocator and a sink with no `reserve_text`); CBOR the same; a TOML buffer refused.
* **files**: `lib/serialize/include/configforge/cf_serialize.h`, `lib/serialize/src/json.c` (an `insitu` flag in `js_scan`, `json.c:41-51`, taken by `js_string`, `json.c:193-247`), `lib/serialize/src/cbor.c`, `lib/serialize/src/codec.c`, `cmake/ConfigForgeOptions.cmake`, `lib/serialize/tests/test_insitu.c`; last: `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/M-16`. Option; `test_insitu.c`.
  2. See it fail: it does not build.
  3. JSON in-situ in `js_string`: when `s->insitu`, the loop writes the unescaped byte at the read position's lag (`w <= p` always), no `memcpy` to a scratch; the run copy of P-3 (not yet merged) is not assumed.
  4. CBOR pointer delivery.
  5. `ctest -R "test_insitu|json"`; a fuzz minute on `json` with the in-situ flag (`CF_FUZZ_TARGET` for it, as M-15 step 5).
  6. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R test_insitu` exit 0.
* **stop if**: `js_string`'s capacity checks (`json.c:225-228`) are needed by the in-situ write (the write can never pass `p`: if a case shows otherwise it is a bug in the card: stop).
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "`cf_codec_decode_insitu` for JSON and CBOR (`CF_WITH_INSITU`)". `docs/Roadmap.md`: nothing until M-17. `docs/Decisions.md`: D-181 — "**D-181 — In-situ reading gives the buffer up.** JSON strings are unescaped over the caller's own bytes and passed by pointer; no copy, no allocation; the buffer is not the document afterwards. CBOR text strings are passed by pointer, length-delimited. Other codecs refuse."

## M-17 - Ways to read: the table, and each mode measured

* **id**: M-17 (version 2.0.0)
* **goal**: `docs/Formats.md` has the table of which format supports which way to read (events, DOM, stream, lazy, in-situ), made from the build, and the benchmark page has a cell for each way over JSON and CBOR.
* **why**: "Each mode is measured on the benchmark page ... Which formats support which mode is a table" (`docs/Roadmap.md:1251-1257`).
* **needs**: M-9, M-14, M-15, M-16, K-7b
* **size**: medium
* **version**: 2.4.0
* **status**: parked
* **decided**:
  * The table is generated, like `docs/Limits.md` and the feature matrix (`scripts/generate/matrix.cmake`, `limits.cmake`): `scripts/generate/readmodes.cmake` writes the part of `docs/Formats.md` between `<!-- readmodes:begin -->` and `<!-- readmodes:end -->`, from the table of formats in `lib/serialize/src/codec.c` through `cf_format_reads` (K-7b: the bits `CF_READS_EVENTS`, `CF_READS_STREAM`, `CF_READS_DOM`, `CF_READS_LAZY` and `CF_READS_IN_SITU`; no second array) printed by `configforge codec modes` (a one-line addition to `tools/codec`: `grep -n "\"list\"" tools/codec/*.c` for where the sub-commands are), so the document cannot drift from the code; a `gate.readmodes` test fails when the generated part is stale (as `gate.apiref` does).
  * Benchmark cells: in `bench/bench.c` (cells at `bench/bench.c:1448-1455` show the pattern) four new `mode` values for `json` and `cbor` at 4 MiB: `dom`, `stream`, `lazy3` (three keys), `insitu`, counted under callgrind like the others (`scripts/bench/run.py:157-174`); they are not in the quick scope's gated set until their first baseline is written from CI's artifact (never a laptop, `scripts/bench/gate.py`'s docstring).
* **files**: `scripts/generate/readmodes.cmake`, `lib/serialize/src/codec.c`, `tools/codec/` (the sub-command), `docs/Formats.md`, `tests/CMakeLists.txt` (`gate.readmodes`), `bench/bench.c`, `scripts/bench/run.py`; last: `docs/Performance.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Codemap.md`.
* **steps**:
  1. `git switch -c work/M-17`. Add `gate.readmodes` with no generated part: it fails (see it fail).
  2. The array, the sub-command, the generator; `ctest -R gate.readmodes` exit 0.
  3. The benchmark cells; run `python3 scripts/bench/run.py --scope quick` once for the new cells and write the numbers into `docs/Performance.md` as measured (a part "The ways to read"): a row per mode, instructions and the ratio to the event read.
  4. Format, generators, ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R gate.readmodes` exit 0; the bench smoke job (`ci.yml:590`) green.
* **stop if**: a mode is slower than the event read on its own document and the card cannot say why: write the number down anyway; it is a finding, not a stop. Stop only if a cell cannot be built.
* **docs**: `CHANGELOG.md` under "## 2.0.0 — unreleased", "### Added": "the table of ways to read each format, and a benchmark cell for each". `docs/Roadmap.md`: "**Ways to read: DOM, SAX, lazy, in-situ.**" (line 1235) to 🟩.

## M-18 - OSS-Fuzz: the project files in the repository

* **id**: M-18 (version 1.9.0)
* **goal**: `oss-fuzz/` holds the three files OSS-Fuzz asks of a project (`project.yaml`, `Dockerfile`, `build.sh`) so that its builder produces the same harnesses ClusterFuzzLite builds, and a `gate.ossfuzz` test checks them, so M-19-m is only an application.
* **why**: "an application to the project, with the existing fuzz targets and their build script" (`docs/Roadmap.md:1356-1360`); the harnesses and the build already exist (`.clusterfuzzlite/build.sh`, `tests/fuzz/CMakeLists.txt:25-40`).
* **needs**: the maintainer's approval (it names a contact and what Google may do with reports); nothing else
* **size**: small
* **version**: 1.9.0
* **status**: parked
* **decided**:
  * `oss-fuzz/project.yaml`: `homepage: "https://github.com/develooper1994/configforge"`, `language: c`, `primary_contact: "TO BE FILLED BY THE MAINTAINER"` (the one field an agent cannot know: **the card leaves it as that literal and `gate.ossfuzz` fails on it only when `OSSFUZZ_APPLY=1`**), `main_repo: "https://github.com/develooper1994/configforge"`, `sanitizers: [address, undefined]`, `architectures: [x86_64]`, `fuzzing_engines: [libfuzzer, afl, honggfuzz]`, `vendor_ccs: []`. A `primary_contact` is not an agent's to invent.
  * `oss-fuzz/Dockerfile`: `FROM gcr.io/oss-fuzz-base/base-builder`, `RUN apt-get update && apt-get install -y cmake`, `RUN git clone --depth 1 https://github.com/develooper1994/configforge $SRC/configforge`, `WORKDIR $SRC/configforge`, `COPY build.sh $SRC/`.
  * `oss-fuzz/build.sh`: the body of `.clusterfuzzlite/build.sh` (read it whole: it is 25 lines) with `$SRC/configforge` unchanged and the sanitizer flags taken from `$CFLAGS` as it does; the card does not duplicate it: `.clusterfuzzlite/build.sh` becomes `exec "$SRC/configforge/oss-fuzz/build.sh"`'s shared part, i.e. the common body moves to `scripts/fuzz/build_harnesses.sh`, and both files call it (one place, the project's rule against duplication).
  * `gate.ossfuzz` (`tests/CMakeLists.txt`, a CMake script test `tests/fuzz/check_ossfuzz.cmake`): `project.yaml` has each key above; the Dockerfile has the three lines; `build.sh` calls the shared script; with `OSSFUZZ_APPLY` unset the contact placeholder passes.
* **files**: `oss-fuzz/project.yaml`, `oss-fuzz/Dockerfile`, `oss-fuzz/build.sh`, `scripts/fuzz/build_harnesses.sh`, `.clusterfuzzlite/build.sh`, `tests/fuzz/check_ossfuzz.cmake`, `tests/CMakeLists.txt`; last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-18`. Write `check_ossfuzz.cmake` and the `gate.ossfuzz` entry; it fails (no files).
  2. Create the shared script from `.clusterfuzzlite/build.sh`; make that file call it; run the cflite harness build locally if Docker is present (`python3 infra/helper.py` is OSS-Fuzz's, not here: skip) and otherwise `bash -n` both scripts and run `scripts/fuzz/build_harnesses.sh` in a temp dir with `OUT` set and `CC=clang` to see `fuzz_json` appear in `$OUT`.
  3. Create the three `oss-fuzz/` files; `ctest -R gate.ossfuzz` exit 0.
  4. Format (shell and cmake files are not clang-formatted), ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R gate.ossfuzz` exit 0; the `cflite` workflow (`.github/workflows/cflite.yml`) still builds its harnesses on the pull request.
* **stop if**: the shared script cannot be used by both builders (a variable only one sets): keep two scripts and say so under notes.
* **docs**: `CHANGELOG.md` under "## 1.9.0 — unreleased", "### Added": "OSS-Fuzz project files". `docs/Roadmap.md`: the item "**OSS-Fuzz.**" (line 1356) stays ⬜ (the application is M-19-m); add nothing.

## M-19-m - OSS-Fuzz: the application (parked)

* **id**: M-19-m (version 1.9.0)
* **goal**: the project is accepted by OSS-Fuzz and the README may say "fuzzed by OSS-Fuzz".
* **why**: `docs/Roadmap.md:1356-1360`; a public, checkable claim.
* **needs**: M-18; the maintainer's own access
* **size**: small (the maintainer's)
* **version**: 1.9.0
* **status**: parked: not now
* **decided**: **Waits on**: the maintainer: a pull request to `google/oss-fuzz` adding `projects/configforge/` with the three files of `oss-fuzz/` (copied as they are, the contact filled), an email address Google accepts for reports (a Google account's address is required for the private bug view), and the application's review by Google. No agent step. When accepted: the README's badge and one line in `docs/Compliance.md` ("What may be said, and when", line 210) naming the date.
* **files**: none in this repository by an agent; after acceptance the maintainer (or a card) edits `README.md` and `docs/Compliance.md`.
* **steps**: the maintainer opens the pull request at `google/oss-fuzz`, answers its review, and tells the organiser when it is merged.
* **accept**: OSS-Fuzz's build status page shows the project building; the first fuzzing run has started.
* **stop if**: Google declines (too small a user base is the usual reason): drop the card and record it under "Done" in `docs/Plan.md` as dropped; ClusterFuzzLite stays.
* **docs**: after acceptance, `docs/Roadmap.md` "**OSS-Fuzz.**" (line 1356) to 🟩; `CHANGELOG.md` "### Added" under 1.9.0.

## M-20 - The evidence pack: the script that assembles it, with a manifest that never claims what is not there

* **id**: M-20 (version 1.9.0)
* **goal**: `python scripts/release/evidence.py --out <dir>` writes `configforge-<version>-evidence.tar.gz` and a `MANIFEST.json` listing every item of the pack with its path, its SHA-256 and the commit it is for, and listing as `"status": "absent"` with a reason each item whose producer is not in the tree yet.
* **why**: "everything an integrator's safety case asks the library for, from one place and for one commit", "built from what the gates already produce" (`docs/Roadmap.md:1361-1370`); `docs/Compliance.md:162-188` names the items and says which are planned.
* **needs**: the maintainer's approval (what the pack promises in public); nothing else; the items whose producers arrive later (the safety manual, the CWE table, mutation score, MC/DC) are picked up by name when they exist, by the manifest's rule below.
* **size**: medium
* **version**: 1.9.0
* **status**: parked
* **decided**:
  * **The items, each with its producer in the tree today** (the manifest's keys): `deviations` (`docs/compliance/deviations.txt`, copied); `misra_report` (the Markdown report of `gate.misra`'s run, `docs/Compliance.md:141-160`, "planned": absent until a producer exists); `cert_matrix`, `cwe_table`, `known_weaknesses` (absent until their cards land; each is looked for at `docs/compliance/<key>.md`); `coverage` (gcovr's HTML and Cobertura XML from `cmake -DEXTRA=coverage -DMODE=quick -P scripts/verify.cmake`, the `coverage` job `ci.yml:466-476`: the script runs it when `--coverage` is given, else takes `--coverage-dir`); `mutation_score` (absent until the mutation card: looked for at `build/mutation/score.json`); `fuzzing` (the corpus directories' file counts and sizes, the `corpus_<reader>` directories `tests/fuzz/CMakeLists.txt:1-9` names, as a JSON table; hours are the sum of `CF_FUZZ_SECONDS` of the runs given by `--fuzz-log`, else absent); `benchmark` (`docs/Performance.md` and the bench JSON, `bench/baseline/instructions.json`); `safety_manual` (`docs/compliance/safety-manual.md`, "planned": absent until it exists); `provenance` (the SPDX SBOM the release already makes, `release.yml:132-134`, and the attestation's bundle path given by `--attestation`); `reproducible_build` (absent until the "release built twice and compared" card writes `build/repro/result.json`).
  * **The rule of the manifest**: an item is `"status": "present"` with `path` and `sha256` only if its file exists; otherwise `"absent"` with `"reason"` naming the file it looked for. The script never writes a placeholder file and never fills a gap with a general statement; the pack is honest about what it holds. `docs/Compliance.md:210-` ("What may be said, and when") is the test of every sentence in the pack's README: the README is generated from the manifest, one line an item, no adjectives.
  * **Reproducible archive**: files sorted, mtime set to `SOURCE_DATE_EPOCH` (the commit's author time, `git log -1 --format=%ct`), owner 0, mode 0644, gzip with `mtime=0` (Python's `tarfile` with a filter and `gzip.GzipFile(mtime=0)`); two runs on the same inputs give the same SHA-256 (a test).
  * **Python 3.8+, standard library only** (the scripts' rule; `scripts/README.md`).
* **files**: `scripts/release/evidence.py`, `scripts/release/test_evidence.py` (unittest, run by `ci.yml`'s `gates` job the way `scripts/bench/test_gate.py` is, `ci.yml:567`), `scripts/README.md` (one line); last: `docs/Compliance.md` (the table row "The compliance report" and a paragraph "The evidence pack"), `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/M-20`. Write `test_evidence.py`: a temp tree with `deviations.txt` and nothing else gives a manifest with `deviations` present and every other item absent with a reason; two runs give equal SHA-256; an item present gets its file's SHA-256; a placeholder file is never created.
  2. See it fail: `python -m unittest scripts/release/test_evidence.py` fails (no module).
  3. Write `evidence.py`; the tests pass.
  4. Run it on this tree: `python scripts/release/evidence.py --out $TMP/pack`; read `MANIFEST.json` once: it has `deviations`, `benchmark` present and the others absent as they are.
  5. Add the unittest to the `gates` job's command (find it: `grep -n "test_gate" .github/workflows/ci.yml`).
  6. Docs, format (Python: `python scripts/work/work.py format` covers what it covers), ACCEPT.
* **accept**: the standard ACCEPT; `python -m unittest scripts/release/test_evidence.py` exit 0 on Linux and Windows.
* **stop if**: a producer's output has no stable path (coverage's directory differs by preset): take it from an argument and say so, do not guess a path.
* **docs**: `CHANGELOG.md` under "## 1.9.0 — unreleased", "### Added": "`scripts/release/evidence.py`: the evidence pack and its manifest". `docs/Roadmap.md`: the item "**An evidence pack with each release.**" (line 1361) stays ⬜ until M-21. `docs/Decisions.md`: D-182 — "**D-182 — The evidence pack lists what is absent.** A release's pack is assembled from what the gates produce at that commit; an item whose producer is not in the tree is listed as absent with the file it looked for, never replaced by a statement; the archive is reproducible (sorted, fixed times, SHA-256 in the manifest)."

## M-21 - The evidence pack in the release

* **id**: M-21 (version 1.9.0)
* **goal**: the `publish` job of `.github/workflows/release.yml` builds the pack with `evidence.py`, attaches it to the release beside the other files (so it is in `SHA256SUMS` and attested), and the release notes list its manifest's present and absent items.
* **why**: "one archive attached to a release" (`docs/Roadmap.md:1361-1370`).
* **needs**: M-20; the maintainer's approval
* **size**: small
* **version**: 1.9.0
* **status**: parked
* **decided**:
  * A job `evidence` in `release.yml` (`needs: [check]`, runs on `ubuntu-26.04`, same checkout and action pins as the file's other jobs, `release.yml:195-200`) runs `cmake -DEXTRA=coverage -DMODE=quick -P scripts/verify.cmake`-equivalent (`ci.yml:466-476`: `apt-get install gcovr` then the same command) and `python scripts/release/evidence.py --out evidence --coverage-dir <that run's coverage output>`; uploads `evidence/` as an artifact `evidence`.
  * `publish` (`needs` gains `evidence`) downloads it with the others (`actions/download-artifact`, `merge-multiple: true`, `release.yml:210-213`), so `dist/*` includes `configforge-<v>-evidence.tar.gz`, which the existing steps already attest (`attest-build-provenance`, `subject-path: dist/*`) and checksum (`sha256sum ./*`). No new permission.
  * The notes step appends a section built from `MANIFEST.json` (a `python - <<EOF` of eight lines in the step): "Evidence pack: present: a, b; absent: c (reason), ...". Nothing is claimed that the manifest does not say.
  * The workflow is tested without a tag: `release.yml`'s `on:` has `workflow_dispatch`? (read `release.yml:22-47`); if it has not, this card adds `workflow_dispatch:` so the maintainer can run it, and the `publish` job's own `if: startsWith(github.ref, 'refs/tags/v')` (line 197) stays, so only the `evidence` job runs without a tag.
* **files**: `.github/workflows/release.yml`; last: `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Compliance.md`.
* **steps**:
  1. `git switch -c work/M-21`. Read `release.yml:22-47` and `195-232`.
  2. Add the `evidence` job and the `needs`; `python scripts/check/check_docs.py` and `actionlint .github/workflows/release.yml` (if installed) exit 0.
  3. See it fail: push the branch and run the workflow by `gh workflow run release --ref work/M-21` (the `evidence` job; the `publish` job skips); a missing path in step 2's command shows here. Fix until the job is green and its artifact holds `MANIFEST.json`.
  4. Docs; ACCEPT (the tag-built `publish` is first exercised by the next real release: said in the commit message, and the notes' section was tested by running its script on the job's manifest).
* **accept**: the standard ACCEPT; the `evidence` job green from `workflow_dispatch`; its artifact's manifest valid JSON with `deviations` present.
* **stop if**: `gh workflow run` is not allowed on the branch by the repository's rules: note it; the check is then the next tag.
* **docs**: `CHANGELOG.md` under "## 1.9.0 — unreleased", "### Added": "each release carries its evidence pack". `docs/Roadmap.md`: "**An evidence pack with each release.**" (line 1361) to 🟩. `docs/Compliance.md`: the row "The compliance report ... attached to each release" status "done" only for what the manifest has present (leave "planned" for the rest).

## M-22 - Publishing by trusted publishing: the release workflow's registry jobs

* **id**: M-22 (version 1.10.0)
* **goal**: `release.yml` has jobs `publish-pypi` and `publish-crates` that publish the wheel and the crate after the GitHub release, by OIDC trusted publishing (no token in the repository), each skipped with a message when its registry is not set up, and the Homebrew formula and the vcpkg and Conan recipes are in the tree and tested.
* **why**: "the crate on crates.io, the wheel on PyPI ... each from the release workflow, after the GitHub release" (`docs/Roadmap.md:787-793`); PyPI and crates.io accept OIDC (`docs/Roadmap.md:895-900`). The registry-side set-up is M-23-m.
* **needs**: the maintainer's approval; the TO DECIDE below; nothing else (the jobs skip until M-23-m is done)
* **size**: large
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the crate's registry token.** crates.io's trusted publishing exists (via `rust-lang/crates-io-auth-action`); a token in a repository secret does not need it. (a) Trusted publishing only, no secret. (b) A `CARGO_REGISTRY_TOKEN` secret, `cargo publish --locked`. **Recommendation: (a)**, as the Roadmap's "so no token is kept in the repository" says; written for (a). Under (b) the job reads the secret and the registry set-up card changes.
  * **Skipping**: each job has `if: startsWith(github.ref, 'refs/tags/v') && vars.PUBLISH_PYPI == 'true'` (and `PUBLISH_CRATES`): repository variables the maintainer sets in M-23-m. With them unset the job is skipped and the release is as it was.
  * `publish-pypi`: `needs: [publish]`; `environment: pypi` (an environment the maintainer creates, M-23-m); `permissions: { id-token: write, contents: read }`; builds the sdist and a wheel per platform by M-26's job when it exists (until then: one `python -m build` sdist and a pure `py3-none-<platform>` wheel on the runner of the `publish` job's ubuntu), downloads them, `pypa/gh-action-pypi-publish` pinned by commit with no `password`. The wheel's contents are the one `tests/python/test_package.py` already checks (`ci.yml:96-105`).
  * `publish-crates`: `needs: [publish]`; `environment: crates-io`; `permissions: { id-token: write, contents: read }`; `cmake -DWORK=$RUNNER_TEMP/rust-package -P scripts/release/package_rust.cmake` (as `ci.yml:106`), then `rust-lang/crates-io-auth-action` (pinned), `cargo publish --locked --manifest-path <staged>/Cargo.toml`.
  * **Recipes in the tree**: `packaging/vcpkg/configforge` and `packaging/conan` exist (tested by the `vcpkg` and `conan` jobs, `ci.yml:226-261`); this card adds `packaging/homebrew/configforge.rb` (a formula: `url` the tag's source archive, `sha256` filled by the release job from `SHA256SUMS`, `depends_on "cmake" => :build`, `system "cmake", "-S", ".", "-B", "build", *std_cmake_args` and `install`, a `test do` that compiles `tests/consumer/consumer.c`) and a `ci.yml` job `homebrew` on `macos-latest` that runs `brew install --build-from-source ./packaging/homebrew/configforge.rb` and `brew test configforge`.
  * **The job's check without a registry**: `gate.publish` (a CMake script test, `tests/CMakeLists.txt`) greps `release.yml` for: no `secrets.` reference in `publish-pypi` or `publish-crates` (under (a)); `id-token: write` only in those two jobs and `publish` (`release.yml:204`); every `uses:` pinned by a 40-hex commit.
* **files**: `.github/workflows/release.yml`, `.github/workflows/ci.yml` (the `homebrew` job), `packaging/homebrew/configforge.rb`, `tests/publish/check_publish.cmake`, `tests/CMakeLists.txt`; last: `docs/Maintaining.md` (a section "Publishing to the registries": the jobs, the variables, what M-23-m sets), `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`.
* **steps**:
  1. `git switch -c work/M-22`. Write `check_publish.cmake` and `gate.publish`; it fails (no jobs).
  2. Add the two jobs; `ctest -R gate.publish` exit 0; `actionlint` if installed; `python scripts/check/check_docs.py`.
  3. Write the formula and the `homebrew` job; push; the `homebrew` job green (the formula's `url` for a branch build points at the branch's source archive through a `--HEAD` install: the job runs `brew install --build-from-source --HEAD ./packaging/homebrew/configforge.rb` with a `head "https://github.com/develooper1994/configforge.git", branch: "main"` stanza).
  4. Docs; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R gate.publish` exit 0; the `homebrew` job green.
* **stop if**: `crates-io-auth-action`'s inputs differ from what this card says (read its README at its pinned commit: fetch it with `gh api`): stop and note them.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Added": "release jobs that publish to PyPI and crates.io by trusted publishing; a Homebrew formula". `docs/Roadmap.md`: "**Published where programs get their libraries.**" (line 787) stays ⬜ until M-24. `docs/Decisions.md`: D-183 — "**D-183 — Registries by trusted publishing, skipped until set up.** The release publishes the wheel and the crate by OIDC from jobs that need a repository variable the maintainer sets; no registry token is kept; `id-token: write` is in those jobs only; with the variables unset the release is unchanged. Recipes (vcpkg, Conan, Homebrew) are in the tree and built by CI; their upstream submission is the maintainer's."

## M-23-m - Registry set-up (parked)

* **id**: M-23-m (version 1.10.0)
* **goal**: PyPI and crates.io trust this repository's `release.yml` jobs, and the first publication has been made.
* **why**: M-22's jobs publish nothing until the registries know them (`docs/Roadmap.md:787-793`: "needs the registries' ... set up once by the maintainer", `docs/Maintaining.md:109`).
* **needs**: M-22; the maintainer's own access
* **size**: small (the maintainer's)
* **version**: 1.10.0
* **status**: parked: not now
* **decided**: **Waits on**: the maintainer. (1) PyPI: create the project `configforge` (or a "pending publisher" for a new name) with publisher GitHub, owner `develooper1994`, repository `configforge`, workflow `release.yml`, environment `pypi`. (2) crates.io: the first release of a crate cannot be published by OIDC: the maintainer publishes `0.x` or the current version once by hand with `cargo publish` from the staged package (`scripts/release/package_rust.cmake`), then adds the trusted publisher (repository, workflow `release.yml`, environment `crates-io`). (3) GitHub: create environments `pypi` and `crates-io` (with required reviewers if wished), and repository variables `PUBLISH_PYPI=true`, `PUBLISH_CRATES=true`. (4) The name `configforge` is checked free on both registries first; if taken, a TO DECIDE for the name (the maintainer's) and M-22's package names change.
* **files**: none by an agent.
* **steps**: the maintainer does (1) to (3) and tells the organiser; the organiser opens M-24.
* **accept**: the next tagged release's `publish-pypi` and `publish-crates` jobs are green and the packages appear on the registries.
* **stop if**: a registry refuses the name or the publisher: the maintainer reports; M-22 is amended, not this card.
* **docs**: `docs/Maintaining.md` records the date and the environment names; nothing else.

## M-24 - The clean-machine install, in CI

* **id**: M-24 (version 1.10.0)
* **goal**: a workflow `registries.yml` installs `configforge` from PyPI, crates.io and vcpkg's registry on a clean runner after a release and runs each one's smallest program, and fails when one cannot.
* **why**: "done when `cargo add configforge`, `pip install configforge` and `vcpkg install configforge` work on a clean machine, and CI installs each in a job of its own" (`docs/Roadmap.md:793-797`).
* **needs**: M-22; M-23-m (nothing to install before it); M-25-m for the vcpkg job only (that job stays disabled until the port is merged upstream)
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * `.github/workflows/registries.yml`, `on: workflow_run` of `release` (types `completed`, conclusion success) and `schedule` (weekly) and `workflow_dispatch` with an input `version` (default: the latest GitHub release's tag: `gh release view --json tagName`).
  * Job `pip` (matrix `ubuntu-26.04`, `macos-latest`, `windows-latest`): `python -m venv v && v/bin/pip install configforge==$VERSION` (retrying up to 10 times a minute apart: the index lags a publish), then `python -c "import configforge as cf; print(cf.version())"` equals `$VERSION` (the binding's `version`, `__init__.py`'s `__all__`).
  * Job `cargo` (ubuntu): `cargo new t && cd t && cargo add configforge@=$VERSION` (retry as above), a `main.rs` that builds a configuration with one defined key from `examples/rust`'s smallest example (copy the first twenty lines of the file `ls examples/rust/*.rs | head -1` names) and prints the default; `cargo run --locked` prints it.
  * Job `vcpkg` (ubuntu): `if: vars.VCPKG_PORT_MERGED == 'true'`; `vcpkg install configforge` with the runner's vcpkg and `cmake` consuming it with `find_package(configforge CONFIG REQUIRED)` and `tests/consumer/consumer.c`.
  * A failed job opens (or comments on) one issue titled "registry install failed: <registry> <version>" with `gh issue create`/`gh issue comment`, once per version (`gh issue list --search`), `permissions: { issues: write }` on that job only.
* **files**: `.github/workflows/registries.yml`; last: `docs/Maintaining.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-24`. Write the workflow; `actionlint`, `python scripts/check/check_docs.py` exit 0.
  2. See it fail: `gh workflow run registries --ref work/M-24 -f version=0.0.0`: the `pip` and `cargo` jobs fail (no such version): that is the check that the jobs detect absence.
  3. After M-23-m and a real release: run it with the released version; green.
  4. Docs; ACCEPT-CI.
* **accept**: ACCEPT-CI (the workflow's static checks); the three jobs green for a released version (the card's own check, after M-23-m).
* **stop if**: PyPI's wheel for a platform is missing (M-26 not done): the `pip` job runs on the platforms that have one; note which.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Added": "a workflow that installs each release from the registries". `docs/Roadmap.md`: "**Published where programs get their libraries.**" (line 787) to 🟩 when the three jobs are green for one release.

## M-25-m - Upstream submissions: vcpkg and ConanCenter (parked)

* **id**: M-25-m (version 1.10.0)
* **goal**: the vcpkg port and the ConanCenter recipe are merged upstream.
* **why**: "the vcpkg and ConanCenter recipes submitted upstream" (`docs/Roadmap.md:788-789`).
* **needs**: M-22 (the recipes in `packaging/`, tested by CI: `ci.yml:226-261`); a tagged release to point at; the maintainer's own access
* **size**: small (the maintainer's)
* **version**: 1.10.0
* **status**: parked: not now
* **decided**: **Waits on**: the maintainer and the upstream reviewers. A pull request to `microsoft/vcpkg` adding `ports/configforge` from `packaging/vcpkg/configforge` (with the `versions/` database entry made by `vcpkg x-add-version configforge`), and one to `conan-io/conan-center-index` adding `recipes/configforge` from `packaging/conan` (its `config.yml`, `conandata.yml` with the release's URL and SHA-256 from `SHA256SUMS`). Upstream's reviewers' changes are folded back into `packaging/` by an agent card opened then.
* **files**: none by an agent now.
* **steps**: the maintainer opens both pull requests after a release; each upstream's CI must pass.
* **accept**: `vcpkg install configforge` and `conan install --requires=configforge/<v>` resolve on a clean machine.
* **stop if**: an upstream declines (usually for too few users): drop and record under "Done" in `docs/Plan.md` as dropped; the in-tree recipes stay.
* **docs**: set the repository variable `VCPKG_PORT_MERGED=true` (enables M-24's `vcpkg` job); `docs/Maintaining.md` records the date.

## M-26 - Wheels for each platform

* **id**: M-26 (version 2.3.0)
* **goal**: the release builds a wheel for Linux x86_64 and aarch64 (manylinux 2_28 and musllinux), macOS arm64 and x86_64, and Windows x64, each tested by `tests/python/test_package.py`, and `publish-pypi` (M-22) publishes all of them.
* **why**: "wheels for each platform" (`docs/Roadmap.md:895-897`); today `ci.yml:96-105` builds one.
* **needs**: M-22; the maintainer's approval (the wheel matrix is a promise)
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * `cibuildwheel` (pinned by commit, version 2.x: the `pypa/cibuildwheel` action) in a `wheels` job of `release.yml`, matrix `ubuntu-26.04`, `ubuntu-24.04-arm`, `macos-13`, `macos-latest`, `windows-latest`. Configured in `pyproject.toml` `[tool.cibuildwheel]`: `build = "cp38-*"` (one wheel per platform: `wheel.py-api = "py3"` is already set, `pyproject.toml:26`; the wheel is tagged `py3-none-<platform>`), `skip = "*-musllinux_aarch64"` only if the aarch64 musl image is unavailable, `test-command = "python -I {project}/tests/python/test_package.py"`, `test-requires = ""`. The wheel's shared library is built by scikit-build-core from the CMake project (as today).
  * The wheels are uploaded as artifacts into `dist/`, so they are checksummed and attested with the rest (`release.yml:210-224`), and `publish-pypi` takes them from `dist/`.
  * Not in this card: free-threaded builds, PyPy, a Python 3.7 wheel (`requires-python >= 3.8`, `pyproject.toml:11`).
* **files**: `.github/workflows/release.yml`, `pyproject.toml`; last: `CHANGELOG.md`, `docs/Maintaining.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-26`. Add `[tool.cibuildwheel]`; run locally on Linux if `cibuildwheel` and Docker are installed (`python -m cibuildwheel --platform linux --only cp38-manylinux_x86_64`), else skip.
  2. See it fail: push the branch and run `release.yml` by `gh workflow run release --ref work/M-26` with `wheels` only (the job has no tag condition); the first run shows the platform that fails.
  3. Fix per platform until the five wheels are green and each ran `test_package.py`.
  4. Docs; ACCEPT-CI.
* **accept**: the standard ACCEPT; the `wheels` job green on five platforms.
* **stop if**: a platform's wheel cannot load its shared library under the test (an rpath): stop, note the platform and the loader error.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "Python wheels for each platform". `docs/Roadmap.md`: "**Distribution, as infrastructure.**" (line 895) stays ⬜ until M-31.

## M-27 - Recipes in the tree: Meson WrapDB, the Bazel Central Registry, conda-forge

* **id**: M-27 (version 2.3.0)
* **goal**: `packaging/meson/`, `packaging/bazel/` and `packaging/conda/` hold what each registry asks of a project, each built and tested by a CI job that uses it the way a consumer would.
* **why**: "recipes submitted to ... Meson's WrapDB, the Bazel Central Registry and conda-forge" (`docs/Roadmap.md:897-899`); the in-tree recipes come first so a submission is a copy.
* **needs**: the maintainer's approval; M-22 (the pattern)
* **size**: large (split into M-27a Meson, M-27b Bazel, M-27c conda if the diff passes ~600 lines)
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **Meson**: `packaging/meson/meson.build` (a `project('configforge', 'c', version: <the version>, meson_version: '>=0.57')` that builds the library's sources as the CMake does: the amalgamation `scripts/generate/amalgamate.py` makes is the one source list, so the file lists `configforge.c` and installs `configforge.h`, with `declare_dependency` as `configforge_dep` and a `pkg-config` file) and `packaging/meson/configforge.wrap` as a WrapDB `[wrap-file]` (`source_url` the release's amalgamation archive `configforge-<v>-amalgamation.tar.gz`, `source_hash` its SHA-256, `patch_directory = configforge`, `[provide] configforge = configforge_dep`). CI job `meson`: `pip install meson ninja`, a scratch project that `subproject('configforge')`s from the wrap and compiles `tests/consumer/consumer.c`.
  * **Bazel**: `packaging/bazel/MODULE.bazel` (`module(name = "configforge", version = <the version>)`), `BUILD.bazel` (a `cc_library` from the amalgamation with `hdrs`, `includes`), `presubmit.yml`, `source.json` (URL and `integrity` the SRI of the release's archive) in the BCR's layout `modules/configforge/<version>/`. CI job `bazel`: `bazel build //... ` in a scratch module that `bazel_dep(name = "configforge")` with `local_path_override` to `packaging/bazel`.
  * **conda-forge**: `packaging/conda/meta.yaml` (`package.name: configforge`, `source.url` and `sha256` as above, `build.script` the CMake configure/build/install, `requirements.build: [{{ compiler('c') }}, cmake, ninja]`, `test.commands: [test -f $PREFIX/include/configforge/configforge.h]`, `about.license: Zlib`, `extra.recipe-maintainers: [develooper1994]`). CI job `conda`: `conda-build packaging/conda` on Linux with `conda-incubator/setup-miniconda` pinned.
  * Versions and hashes are written by `scripts/release/version.cmake`'s sibling `scripts/release/packaging.py --version V --archive A`, which fills the `<version>` and `sha256` fields in all four recipes (vcpkg's and Conan's too: it replaces the hand edits `docs/Maintaining.md` now describes); a unittest runs it on a temp copy.
* **files**: `packaging/meson/*`, `packaging/bazel/*`, `packaging/conda/*`, `scripts/release/packaging.py`, `scripts/release/test_packaging.py`, `.github/workflows/ci.yml` (three jobs); last: `docs/Maintaining.md`, `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-27`. Write `test_packaging.py`; it fails (no script).
  2. Write `packaging.py`; tests pass.
  3. Meson recipe and job; push; green. Then Bazel; then conda (one commit each; see each job fail first by pointing its `source_url` at a missing file).
  4. Docs; ACCEPT-CI.
* **accept**: the standard ACCEPT; the three jobs green.
* **stop if**: the amalgamation lacks a header a consumer needs (a `configforge/` subdirectory of installed headers the CMake install makes): stop and note the missing files.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "recipes for Meson, Bazel and conda-forge, built by CI". `docs/Roadmap.md`: nothing until M-31.

## M-28 - Embedded registries: ESP-IDF, PlatformIO, Zephyr

* **id**: M-28 (version 2.3.0)
* **goal**: the manifests the three embedded registries ask for are in the tree, each checked by CI: `idf_component.yml`, `library.json`, and `zephyr/module.yml` with its `CMakeLists.txt` and `Kconfig`.
* **why**: "for embedded targets the ESP-IDF Component Registry, PlatformIO's registry and a Zephyr module" (`docs/Roadmap.md:899-901`).
* **needs**: the maintainer's approval; M-27 (`scripts/release/packaging.py` fills their versions)
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * `idf_component.yml` at the root: `version` (filled), `description`, `url`, `license: Zlib`, `dependencies: { idf: ">=5.0" }`, `targets` omitted (all); `CMakeLists.txt`-side: `packaging/esp-idf/CMakeLists.txt` with `idf_component_register(SRCS "configforge.c" INCLUDE_DIRS ".")` over the amalgamation (the component's tree is a generated directory `packaging/esp-idf/` made by `scripts/release/packaging.py --esp-idf <dir>`: not committed, so the component has one source of truth).
  * `library.json`: `name`, `version`, `description`, `repository`, `license`, `frameworks: "*"`, `platforms: "*"`, `build: { srcDir: "native", includeDir: "native" }` over the same generated tree; a PlatformIO job runs `pio pkg pack` and `pio test` is not run (hardware).
  * Zephyr: `zephyr/module.yml` (`build: { cmake: zephyr, kconfig: zephyr/Kconfig }`), `zephyr/CMakeLists.txt` (`zephyr_library()` and `zephyr_library_sources(configforge.c)` under `if(CONFIG_CONFIGFORGE)`), `zephyr/Kconfig` (`config CONFIGFORGE bool "ConfigForge" default n`). Zephyr's own build is not run in CI (a toolchain of several gigabytes): the job `embedded-manifests` checks the three manifests parse (`python -c "import yaml,json"`), the `version` fields equal the project's version, and a host compile of the generated sources with `-ffreestanding -DCF_PORT_FREESTANDING` (the flag the `minimal` profile uses: `grep -rn FREESTANDING cmake/profiles`) succeeds. **What is not tested says so in `docs/Maintaining.md`**: "these three are not built on a target".
* **files**: `idf_component.yml`, `library.json`, `zephyr/module.yml`, `zephyr/CMakeLists.txt`, `zephyr/Kconfig`, `scripts/release/packaging.py`, `.github/workflows/ci.yml` (`embedded-manifests`); last: `docs/Maintaining.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/M-28`. Write the CI job's check as a script `scripts/check/manifests.py` first; run it: it fails (no manifests).
  2. Write the manifests and the `--esp-idf`/`--platformio` generation in `packaging.py`; the script exits 0.
  3. Add the job; ACCEPT-CI.
* **accept**: the standard ACCEPT; the `embedded-manifests` job green.
* **stop if**: the freestanding flag does not compile the amalgamation on a host (it needs the port layer's headers): note the error, stop.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "manifests for the ESP-IDF, PlatformIO and Zephyr registries". `docs/Roadmap.md`: nothing until M-31.

## M-29-m - Submissions and registry accounts (parked)

* **id**: M-29-m (version 2.3.0)
* **goal**: ConfigForge is listed on Meson's WrapDB, the Bazel Central Registry, conda-forge, the ESP-IDF Component Registry, PlatformIO's registry and as a Zephyr module.
* **why**: M-27 and M-28 prepare; the submissions are other projects' and need the maintainer's accounts (`docs/Roadmap.md:895-901`).
* **needs**: M-27, M-28, a tagged release; the maintainer's own access
* **size**: small (the maintainer's)
* **version**: 2.3.0
* **status**: parked: not now
* **decided**: **Waits on**: the maintainer and each registry's reviewers. Pull requests to `mesonbuild/wrapdb` (the wrap), `bazelbuild/bazel-central-registry` (`modules/configforge/<v>/`), and `conda-forge/staged-recipes` (`recipes/configforge/meta.yaml`); `compote publish` (ESP-IDF) and `pio pkg publish` (PlatformIO) with the maintainer's accounts and tokens, run by the maintainer from the generated trees; the Zephyr module is listed by a pull request to the Zephyr modules list or used by `west.yml` entry (no registry), documented in `docs/Guide.md`.
* **files**: none by an agent now.
* **steps**: the maintainer submits each; reviewers' changes are folded back by an agent card.
* **accept**: each channel installs the release on a clean machine (the monthly check of M-31 and `registries.yml` cover the ones CI can reach).
* **stop if**: a registry declines: drop that channel and record it under "Done" in `docs/Plan.md` as dropped.
* **docs**: `docs/Maintaining.md` records each channel's date.

## M-30-m - The GitHub Action in a repository of its own (parked)

* **id**: M-30-m (version 2.3.0)
* **goal**: `develooper1994/configforge-doctor` exists with `action.yml` at its root, tagged, and listed on the Marketplace.
* **why**: "the GitHub Action in a repository of its own with `action.yml` at its root, as the Marketplace requires" (`docs/Roadmap.md:901-903`); today it is `actions/doctor` inside this repository (`ls actions/doctor`).
* **needs**: a decision to rename (the repository's name is the maintainer's); the maintainer's own access (creating a repository and accepting the Marketplace's terms)
* **size**: small (the maintainer's)
* **version**: 2.3.0
* **status**: parked: not now
* **decided**: **Waits on**: the maintainer. Steps for the maintainer: create the repository; the organiser then opens an agent card M-30a that copies `actions/doctor/*` into a staging directory, rewrites its self-references (`uses: ./actions/doctor` in `ci.yml:478-505`'s job becomes the new repository at a tag), and adds a workflow to this repository that syncs the directory to the new repository's `main` on each release (deploy key or fine-grained token: a secret the maintainer creates). That card is not written until the repository exists.
* **files**: none now.
* **steps**: as above.
* **accept**: a workflow in another repository runs `uses: develooper1994/configforge-doctor@v1`.
* **stop if**: the maintainer prefers to keep the Action in this repository: drop and record as dropped.
* **docs**: after the card: `docs/Doctor.md`'s Action section, `docs/Roadmap.md` item.

## M-31 - The monthly count of downloads, traffic and issues

* **id**: M-31 (version 2.3.0)
* **goal**: `scripts/release/counts.py` prints, as Markdown, the month's downloads of each release asset, the repository's views and clones, and the issues opened and closed, and a scheduled workflow appends it to `docs/Maintaining.md`'s "Counts" section by a pull request once a month.
* **why**: "downloads, traffic and issues counted once a month into Maintaining.md" (`docs/Roadmap.md:903-905`); with the M-29-m channels, the end of "Done when each channel installs a release on a clean machine in CI" (line 904).
* **needs**: the maintainer's approval (it makes a bot's pull request)
* **size**: small
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * The script uses `gh api` only (the CLI's token), no dependency: `repos/{o}/{r}/releases` (`assets[].download_count`), `repos/{o}/{r}/traffic/views` and `/clones` (needs push access: the workflow's `GITHUB_TOKEN` has `contents: read` and traffic needs `administration`-level; so **traffic is written only when `--traffic` is given and the token allows it; otherwise the section says "traffic: not collected"**), `gh issue list --state all --search "created:>=YYYY-MM-01"`. Output: one table row `| 2026-12 | downloads | views | clones | opened | closed |`.
  * The workflow `counts.yml`: `schedule: cron "17 4 2 * *"`, `workflow_dispatch`; `permissions: { contents: write, pull-requests: write }`; it runs the script, appends the row under the heading "## Counts" of `docs/Maintaining.md` (created by the card), and opens a pull request with `gh pr create` (never pushes to `main`).
  * Discussions: "open" already (the release job creates one, `release.yml:229-231`); the card adds nothing for them.
* **files**: `scripts/release/counts.py`, `scripts/release/test_counts.py` (unittest with a fake `gh` through an environment variable `CF_GH`), `.github/workflows/counts.yml`; last: `docs/Maintaining.md` ("## Counts"), `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-31`. Write `test_counts.py` with a fake `gh` that prints canned JSON: the row is the sum of the asset counts; it fails (no script).
  2. Write `counts.py`; tests pass.
  3. Write `counts.yml`; `actionlint`, `python scripts/check/check_docs.py`.
  4. Docs; ACCEPT-CI.
* **accept**: the standard ACCEPT; `python -m unittest scripts/release/test_counts.py` exit 0; `gh workflow run counts --ref work/M-31` makes a pull request (closed by the maintainer: it is a test).
* **stop if**: the API's field names differ from the fake's: use the real response (`gh api repos/develooper1994/configforge/releases --jq '.[0].assets[0]'`) and rewrite the fake.
* **docs**: `CHANGELOG.md` under "## 2.3.0 — unreleased", "### Added": "a monthly count of downloads, traffic and issues". `docs/Roadmap.md`: "**Distribution, as infrastructure.**" (line 895) to 🟩 only when M-26 to M-31 are done and M-29-m and M-30-m are done or dropped.

## M-32 - One digest a purpose: SHA-256 as the fingerprint's option

* **id**: M-32 (version 1.10.0)
* **goal**: `cf_builder_fingerprint(b, CF_FP_SHA256)` makes `cf_fingerprint_text` write `sha256:<64 hex digits>` over the same bytes the FNV-1a fingerprint reads, and `cf_fingerprint` and the report's `"fingerprint"` stay `fnv1a:` unless asked.
* **why**: "a fingerprint used to approve a configuration or allow it through a gate, where FNV-1a's collisions can be made on purpose" (`docs/Roadmap.md:916-931`); `cf_fingerprint` says "Not cryptographic ... proves nothing" (`lib/config/include/configforge/configforge.h:643-649`).
* **needs**: the maintainer's approval; the TO DECIDE below
* **size**: large
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the interface.** `cf_fingerprint` returns a `cf_u64` and `cf_fingerprint_format` writes 22 characters (`configforge.h:649-653`), neither can hold 256 bits. (a) New functions beside them: `cf_fingerprint_text(const cf_config*, char* buf, size_t cap)` writing whichever kind the builder chose (`fnv1a:` 22 bytes, `sha256:` 71), the 1.x pair unchanged. (b) A `cf_fingerprint256(const cf_config*, unsigned char out[32])` only. **Recommendation: (a)**: a program that logs a fingerprint wants the text, and the prefix tells a reader the kind (`docs/Roadmap.md:928-929`: "the prefix every digest already carries"). Written for (a).
  * **Names.** `typedef enum cf_fp_kind { CF_FP_FNV1A = 0, CF_FP_SHA256 = 1 } cf_fp_kind;` `CF_API cf_bool cf_builder_fingerprint(cf_builder* b, cf_fp_kind kind);` `CF_API size_t cf_fingerprint_text(const cf_config* c, char* buf, size_t cap);` (the length it needs: 22 or 71, nothing written past `cap`, as `cf_fingerprint_format`'s contract). `cf_fingerprint` (the u64) stays FNV-1a whatever the builder chose and its comment says so. **A report carries one fingerprint kind** (`docs/Roadmap.md:928-929`: so a diff compares): `"fingerprint"` is the chosen kind's text; Doctor compares two fingerprints only when their prefixes agree, else CFD020 (as D-85's digests: `docs/Decisions.md:3560-3568`) naming the two kinds.
  * **The bytes hashed.** FNV-1a's fingerprint folds over the selected values with `\037` and `\036` separators (`lib/config/src/core/keyspace.c:1065-1078`, `cf_fingerprint_step` `define.c:990-998`, `cf_defset_fingerprint` `define.c:1000-1029`) and a secret enters as its digest text (D-85: `cf_ks_secret_digest_text`, `cf_keyspace.h:419`). The SHA-256 fingerprint hashes **the same byte stream** (one function `cf_fp_stream` feeds either a FNV-1a state or a SHA-256 state through a two-function interface `{update, finish}`), so the two never disagree about what the fingerprint covers; a test pins it.
  * **SHA-256**: FIPS 180-4, in `lib/base/src/sha256.c` (internal, `cfi_sha256_init/update/final`, C89, no allocation, 64-byte block, `cf_u32` arithmetic; a header `lib/base/src/sha256.h`; `CF_INTERNAL_API` export `cf_sha256(const void*, size_t, unsigned char out[32])` for the tests' vectors, as `cf_siphash24` is, `cf_keyspace.h:410-415`). Vectors: FIPS 180-4's "abc", the empty string, the 448-bit message, and the million `a`s; plus one for a message cut at every offset 0 to 130 by `update` calls.
  * **A secret in a SHA-256 fingerprint** still enters by its digest text, keyed when the program gave a key (D-85), so the fingerprint can be published wherever a blind report can (the contract of `cf_fingerprint`, `configforge.h:643-649`); the Guide says a SHA-256 fingerprint over an unkeyed FNV-1a secret digest is as guessable for that secret as before.
  * Tests (`lib/config/tests/test_fingerprint.c`): two configurations with the same values have the same text and one value changed another (FNV and SHA); the kind's prefix and length; `cf_fingerprint_text` with `cap` 0, 21, 22, 70, 71; derived configuration keeps the kind; the same byte stream: the SHA-256 of an independently written reference (the test builds the byte string by hand for a three-key configuration and hashes it with `cf_sha256`) equals the text.
* **files**: `lib/base/src/sha256.c`, `lib/base/src/sha256.h`, `lib/base/CMakeLists.txt`, `lib/config/include/configforge/configforge.h`, `lib/config/include/configforge/cf_keyspace.h`, `lib/config/src/core/keyspace.c`, `lib/config/src/core/define.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_fingerprint.c`, `lib/config/tests/CMakeLists.txt`, `tools/doctor/` (the comparison of kinds: find it with `grep -rn CFD020 tools/doctor | head`); last: `docs/Guide.md`, `CHANGELOG.md`, `docs/Roadmap.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/M-32`. Write `test_fingerprint.c` with the SHA-256 vectors first; it does not build.
  2. See it fail.
  3. `sha256.c`; the vectors pass.
  4. The two-function stream interface in `keyspace.c`/`define.c`; FNV-1a through it first: `ctest` whole unchanged (the fingerprint's value for every existing test is the same: `test_digestkey`, `test_fingerprint` of 1.1).
  5. `cf_builder_fingerprint`, the storage in `cf_defset`, `cf_fingerprint_text`; the report's `"fingerprint"`; Doctor's kind check with its `doctor.*` test (two reports of different kinds give CFD020).
  6. `docs/Guide.md` paragraph; format; generators; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R "test_fingerprint|test_digestkey|doctor"` exit 0.
* **stop if**: the report schema's `"fingerprint"` field is read by a consumer that expects 22 characters (`grep -rn "fnv1a:" tools lib tests bindings | head`): list them under notes and change each only if it is a test; stop for a consumer outside the tree.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Added": "SHA-256 as an option of the fingerprint (`cf_builder_fingerprint`, `cf_fingerprint_text`)". `docs/Roadmap.md`: "**One digest a purpose, and a door for the program's own.**" (line 916) stays ⬜ until M-34. `docs/Decisions.md`: D-184 — "**D-184 — One default digest for each job, named by its prefix.** A fingerprint that tells changes is FNV-1a (`fnv1a:`), one that decides is SHA-256 (`sha256:`) chosen by `cf_builder_fingerprint`; both hash the same byte stream; a report holds one kind and Doctor compares two only when their prefixes agree (CFD020 otherwise); a secret enters by its digest text, keyed when the program gave a key (D-85). The u64 `cf_fingerprint` stays FNV-1a. Rejected: a menu of algorithms."

## M-33 - A digest function the program gives

* **id**: M-33 (version 1.10.0)
* **goal**: `cf_builder_digest_fn(b, &fn)` lets a program give the fingerprint's digest function (an HMAC-SHA-256 a rule asks for, a microcontroller's hash engine), with the name the report writes before its digests, and a test gives one and reads its name back from the report.
* **why**: "a rule or a device that names its own ... a function the program gives, with the name the report writes before its digests" (`docs/Roadmap.md:920-924`, `929-931`).
* **needs**: the maintainer's approval; the TO DECIDE below; M-32 (the stream interface and the kinds)
* **size**: medium
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * **TO DECIDE: the function's shape.** (a) A streaming triple `init(ctx)`, `update(ctx, p, n)`, `final(ctx, out, cap) -> n` (what a hash engine's driver offers; any digest size up to 64 bytes). (b) One-shot `digest(ctx, p, n, out)`: the library would have to buffer the whole byte stream (a configuration's can be large; no allocation for it in a freestanding build). **Recommendation: (a)**; written for (a).
  * **Names.** `typedef struct cf_digest_fn { size_t size; const char* name; size_t out_len; void (*init)(void* ctx); void (*update)(void* ctx, const void* p, size_t n); void (*final)(void* ctx, unsigned char* out); void* ctx; } cf_digest_fn;` — `size` is `sizeof` at the call site (D-51); `name` is `[a-z0-9-]` 1 to 16 characters (`"hmac-sha256"`, `"stm32-hash"`; the Roman letters of `cf_key_valid`'s class so a prefix is never ambiguous: `fnv1a`, `sip24` and `sha256` are refused as names: "that name is the library's"); `out_len` 8 to 64. `CF_API cf_bool cf_builder_digest_fn(cf_builder* b, const cf_digest_fn* fn);` — the struct is copied (the pointers it holds are the program's: it keeps `ctx` alive for the configuration's life, said in the header); refused with `CF_E_ARG` for a bad name, `out_len`, NULL function or a `size` too small, and `CF_E_POLICY` when `cf_builder_fingerprint` was given `CF_FP_SHA256` too ("one digest a purpose: a fingerprint has one kind"). With it `cf_fingerprint_text` writes `<name>:<2*out_len hex digits>`, up to 16 + 1 + 128 characters, over M-32's byte stream; `cf_fingerprint` (u64) stays FNV-1a.
  * **Concurrency and state**: the library calls `init` before each fingerprint and `final` once; it never calls them from two threads on one `ctx` (a fingerprint is computed under the configuration's own lock where there is one; for the lock-free `cf_get_*` readers the fingerprint is made once at `create`, `live.c:335`'s `cf_fingerprint(next)` pattern: **the function is called at `create` and at each `cf_live` reload, nowhere else**, and the text is stored; a call later reads the stored text). This is the rule that keeps the program's function out of the read path.
  * Tests (`lib/config/tests/test_digestfn.c`): a function that XORs bytes into a 8-byte state, named `"xor8"`: `cf_fingerprint_text` begins `xor8:`, has 16 hex digits and equals the hand-computed XOR of the stream M-32's test builds; refusals for each bad input above; the report's `"fingerprint"` begins `xor8:`; Doctor compares two `xor8` reports (equal and different) and two reports of different kinds give CFD020.
* **files**: `lib/config/include/configforge/configforge.h`, `lib/config/src/core/define.c`, `lib/config/src/core/config.c`, `lib/config/tests/test_digestfn.c`, `lib/config/tests/CMakeLists.txt`; last: `docs/Guide.md`, `CHANGELOG.md`, `docs/Decisions.md`, `docs/API.md` and `docs/Codemap.md` (generated).
* **steps**:
  1. `git switch -c work/M-33`. Write `test_digestfn.c`; it does not build.
  2. See it fail.
  3. The struct, the builder function and its refusals, storage in `cf_defset`; `ctest -R test_digestfn`.
  4. Compute the text at `create` and at `cf_live` reloads only; the report and Doctor.
  5. Format; generators; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R "test_digestfn|test_fingerprint|doctor"` exit 0.
* **stop if**: a path computes a fingerprint outside `create` and a reload (`grep -rn "cf_fingerprint(" lib | head`): `live.c:335`, `565`, `714` are the known ones; any other: stop and note it.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Added": "`cf_builder_digest_fn`: a digest function of the program's own". `docs/Roadmap.md`: nothing until M-34. `docs/Decisions.md`: D-185 — "**D-185 — The program's own digest is a streaming triple with a name.** A program gives `init`, `update`, `final`, an output length and a name of its own (never `fnv1a`, `sip24` or `sha256`); the report writes the name as the prefix; the function runs at `create` and at each reload only, so it is never in the read path; a fingerprint has one kind, so it cannot be combined with SHA-256."

## M-34 - A gate built on a SHA-256 fingerprint, in the Guide, with its test

* **id**: M-34 (version 1.10.0)
* **goal**: `docs/Guide.md` has a worked recipe: approve a configuration by its SHA-256 fingerprint and refuse to start under any other, with the C code compiled and run by a test, and `docs/Roadmap.md`'s item is marked done.
* **why**: "Done when a gate built on a SHA-256 fingerprint is in the Guide, and a test gives a digest function of its own and reads its name back from the report" (`docs/Roadmap.md:928-931`); the second half is M-33's test.
* **needs**: M-32, M-33
* **size**: small
* **version**: 2.6.0
* **status**: parked
* **decided**:
  * The recipe is `examples/c/fingerprint_gate.c`: it builds a configuration (the example `examples/c/settings.c`'s first twenty lines for the pattern), calls `cf_builder_fingerprint(b, CF_FP_SHA256)`, reads `cf_fingerprint_text`, compares it to an approved string given as `argv[1]` with `strcmp` (not constant-time: the fingerprint is not a secret, said in the Guide) and exits 0 on equal and 3 otherwise, printing both. A second run with a changed value exits 3.
  * `examples/CMakeLists.txt`: two tests `example.fingerprint_gate.ok` and `.refused`, the first computing the approved string by running the example with no argument (it prints its own fingerprint when `argv[1]` is absent), the second passing a wrong one and expecting exit 3 (`WILL_FAIL`-style through `PASS_REGULAR_EXPRESSION` as `examples/expect.cmake` does for the others).
  * The Guide section, "Approving a configuration by its fingerprint", quotes the example by `include` of its lines (the Guide's mechanism for example code: `grep -n "examples/c" docs/Guide.md | head -3`, followed as it is) and states: FNV-1a tells changes, SHA-256 decides, a secret in the fingerprint enters by its digest text (keyed when `cf_builder_digest_key` was given, D-85), so give a key when a gate covers a secret with few possible values.
* **files**: `examples/c/fingerprint_gate.c`, `examples/CMakeLists.txt`, `docs/Guide.md`; last: `CHANGELOG.md`, `docs/Roadmap.md`.
* **steps**:
  1. `git switch -c work/M-34`. Add the two tests to `examples/CMakeLists.txt`: `ctest -R fingerprint_gate` fails (no file).
  2. Write the example; both tests exit 0.
  3. Write the Guide section; `python scripts/check/check_docs.py` exit 0.
  4. Docs; format; ACCEPT.
* **accept**: the standard ACCEPT; `ctest -R fingerprint_gate` exit 0.
* **stop if**: the example's output depends on the platform (the fingerprint is "identical on every platform", `configforge.h:630-632`): if the two ACCEPT machines differ, it is a bug in M-32: stop and name both strings.
* **docs**: `CHANGELOG.md` under "## 1.10.0 — unreleased", "### Added": "a worked gate on a SHA-256 fingerprint". `docs/Roadmap.md`: "**One digest a purpose, and a door for the program's own.**" (line 916) to 🟩.
