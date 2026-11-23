# N - the next roadmap cards

## N-1 - compare two flat configuration files by value

* **id**: N-1
* **goal**: `configforge diff a.yaml b.json` reads two flat scalar mappings
  and lists the keys whose presence or value differs.
* **why**: `docs/Roadmap.md:1368-1377`. `cf_doctor_diff_files` is a
  report-to-report call (`tools/doctor/engine.c:2305-2320`). Doctor reads
  digests supplied by the reports (`tools/doctor/engine.c:1516-1536`). A
  plain document supplies typed events and has no sensitivity metadata
  (`lib/serialize/include/configforge/cf_serialize.h:118-150`).
* **needs**: W5-V1 (done)
* **size**: medium
* **decided**:
  * **Command**: `configforge diff [--as-left FORMAT] [--as-right FORMAT]
    [--] LEFT RIGHT`. The two options may appear once each, in either
    order, before the paths. A format argument cannot start with `-`.
    `--` ends option parsing. A path beginning
    with `-` needs `--`. Both operands name files, including a literal
    file named `-` after `--`. A sole `--help` prints the usage line to
    stdout and exits 0. Wrong argument count, unknown or repeated
    option, or missing option operand prints usage to stderr and exits
    2. Use the existing front door's normal sibling/PATH dispatch
    (`docs/Decisions.md:4369-4376`) with a C89 `configforge_diff` target
    named `configforge-diff` beside the other tools and installed by
    their CMake loop. Add its help line to the front door in a separate
    `fputs` call if the existing literal reaches the C89 length limit.
    Under D-100 keep the recorder in the tool and link only
    `configforge_serialize`, following the shared-build DLL
    copy, install run path and build checks in
    `tools/codec/CMakeLists.txt:17-48`. Build whenever tools are enabled,
    like codec, including builds without Doctor or encoders.
  * **Read**: per side use `cf_blob_file`, an explicit built-in reader
    from `cf_codec_by_name` plus `cf_codec_can_read` when named, or
    `cf_codec_detect` with the file path and NULL trial arena otherwise.
    Use any readable built-in codec available to the CLI for the
    accepted shape. Detection follows extension then content
    (`lib/serialize/include/configforge/cf_serialize.h:476-504`). An
    explicit unknown or disabled codec, a writer-only codec, an
    ambiguous or unidentified file, and a reader refusal all exit 2.
    Read and decode left before right. Finish both inputs and order their
    keys before writing any change. No report, conversion or text dump
    runs along this path.
  * **Shape and comparison**: record with a `cf_doc_sink`. Accept one
    unnamed top-level table of named scalar values, including an empty
    table. If a reader closes and reopens the unnamed root, continue the
    same mapping, as the converter does (`tools/codec/convert_core.c:223-252`).
    A non-NULL name with length zero is the empty key. A nested table,
    any array, unnamed scalar, or missing root is an
    unsupported shape and exits 2. Where the reader accepts repeated
    names, the last event wins. Compare names by unsigned bytes with a
    shorter proper prefix first. Compare corresponding `cf_value`s
    with `cf_value_equal` (`lib/base/src/value.c:198-233`). Keep the
    codec's scalar type. Ignore spelling, locations and the event's
    `textual` flag. A text value `"1"` differs from integer `1`.
  * **Recorder**: one local arena per side holds the blob, scalar
    entries, copied key and text spans, and reader workspace. Copy both
    spans by length before the callback returns. Record scalars in
    arrival order with an ordinal. Sort a pointer array by name then
    descending ordinal and merge the sorted sides, advancing past each
    duplicate run after its first element. O(n log n) ordering and a
    linear merge suffice. Supply `reserve_text` and `reserve_work`.
    Charge entries, spans, pointer array and workspace requests to an
    overflow-checked budget of 1 MiB plus 64 times input bytes,
    saturating the budget at `SIZE_MAX` as in
    `tools/codec/convert_core.c:862-878` (use `(size_t)-1` in C89).
    Overflow, exhaustion or allocation failure exits 2. Free both arenas
    on every exit (`lib/base/src/alloc.c:198-222` wipes the used blocks).
  * **Output**: stdout has one line for each changed key, in the byte
    order above. Write `added`, `removed` or `changed`, one tab,
    `$["`, the escaped key, `"]`, and a newline. Emit printable ASCII
    bytes directly except backslash and double quote, which are
    backslash-escaped. Emit all other bytes as `\xhh` with lowercase
    hex. A successful equal comparison exits 0 with empty stdout. Any
    change exits 1. stderr is empty in both cases. Check output writes
    and the final flush. A write or flush failure reported by stdio exits 2 and prints
    `diff: cannot write output` to stderr.
  * **Privacy**: paths in change lines are document key names and are
    visible. Scalar values and digests never appear in either stream.
    On a file failure emit only `diff: left input refused` or
    `diff: right input refused` on stderr and leave stdout empty. Never
    forward parser text, input paths, format arguments or source
    snippets. A plain document has no secret classification, so apply
    the value-blind rule to every scalar, consistent with the output
    boundary in `docs/Decisions.md:3595-3605`. Contract-aware secret
    digests, unit conversion and comparison of nested shapes are later
    work.
* **files**: `tools/diff/main.c`, `tools/diff/CMakeLists.txt`,
  `tools/CMakeLists.txt:7-34`, `tools/configforge/main.c:29-42`,
  `tests/front/CMakeLists.txt:17-28`, `tests/front/diff.cmake`,
  `docs/Tools.md`, `docs/Roadmap.md`, `CHANGELOG.md`.
* **steps**:
  1. Create `work/N-1` as in `docs/Plan.md:15-35`.
  2. Register `front.diff` beside `front.run` while the front target and
     the JSON reader are enabled. In a fresh default build with no
     installed `configforge-diff` on PATH, build the front door and run
     `ctest --preset default -R '^front\.diff$' --no-tests=error --output-on-failure`.
     See the first reordered JSON pair fail: expected equal (0), got
     the front door's missing-command status (2). Use a dedicated
     test directory. When YAML is enabled, also compare the same typed
     mapping as YAML and JSON. Check both automatic selection and
     explicit per-side names using files with unknown extensions.
     Assert exact streams and statuses for additions, removals, a
     changed number, string versus number, later duplicate wins,
     mismatched names including literal dots and escaped quotes, and
     byte ordering with a proper prefix. Include text beyond 255 bytes.
     Use distinct sentinel values on both sides, including one under
     an ordinary key and one under `password`. Check that neither
     stream contains any value sentinel. Cover bad arguments, a
     missing file, unknown explicit format, a malformed second file
     after a valid first one, and a second file with a nested table or
     an array. Include a sentinel in a malformed token or its source
     path and assert the fixed error with empty stdout. Normalize only
     CRLF to LF when checking streams on Windows.
  3. Build and install the tool through the normal CMake targets. Add
     the front-door help line and implement the CLI and recorder above.
     Run `front.diff`, then `front.run` and the codec tests. Smoke-test
     the installed front door with a flat JSON pair in a temporary
     prefix using the `tools` component of the default static build.
  4. Document syntax, shape, value-blind output and exit statuses in
     a new `## configforge diff` section of `docs/Tools.md`.
     Mark the flat slice built under the Roadmap item and
     keep the parent item open for nested shapes and the contract work.
     Add the first slice under Added in `CHANGELOG.md`.
  5. Run ACCEPT.
* **accept**: `docs/Plan.md:69-100` (ACCEPT-LINUX, ACCEPT-WINDOWS and
  ACCEPT-CI). `front.diff`, `front.run`, codec tests and the installed
  front-door smoke check pass. The YAML/JSON case runs in a build with
  both readers enabled. Record exact failing and passing exit codes.
* **stop if**: a flat mapping the codec already accepts cannot be compared
  through its public events, or the work requires a public
  serialization API or a choice about showing scalar values. Report
  the codec and a minimal input. After three failures on the same
  thing use the Plan's stop rule.
* **notes**: (empty)

### Organiser's answers

The YAML reader emits the plain scalars `1` and `true` as text. The
equal cross-format fixture therefore pairs them with JSON strings.

## N-2 - rebuild the Linux musl release archive and compare the bytes

* **id**: N-2
* **goal**: The Linux musl release job uploads its archive only after builds
  of the same commit in two clean trees produce byte-identical compressed files.
* **why**: `docs/Roadmap.md:988-991`; the 1.9.0 milestone in
  `docs/Maintaining.md:99-105`. The current job packages fresh filesystem
  times and the SBOM uses the configure time unless given an epoch.
* **needs**: nothing
* **size**: medium
* **decided**:
  * Work entirely in packaging. Build and test the existing C89 targets
    with their D-100 dependencies. Keep the Alpine packages in
    `.github/workflows/release.yml:75-106`. Run its pinned checkout
    action twice, with `path: source-a` and
    `path: source-second`, `ref: ${{ github.sha }}` and
    `persist-credentials: false` for both. Verify each HEAD with
    `git rev-parse HEAD` against `GITHUB_SHA`. Build and install into
    fresh sibling directories in a temporary directory from `mktemp -d`.
    The two source, build and staging paths must differ. Use one trap
    to remove only the temporary directory created by the job.
  * Read the version with `cmake -P scripts/release/version.cmake` in
    the first checkout, including on `workflow_dispatch`. The tag
    check still runs as it does today. Set `SOURCE_DATE_EPOCH` from
    `git show -s --format=%ct HEAD` in the first checkout and export
    it for both builds, including SBOM generation
    (`cmake/ConfigForgeSbom.cmake:14-18`). Use UTC, `LC_ALL=C` and umask 022 for both.
    Fetch and check the installed musl version's copyright once,
    using the current job's rule. Both builds install these same
    notice bytes.
  * Put the single-build recipe in `scripts/release/linux_musl.sh`.
    Run it as `sh <script> SOURCE WORK VERSION NOTICE` with an
    existing clean checkout, a new absolute work directory, the
    version read above, and the absolute path to the checked notice.
    Require a numeric epoch in the environment. Use `set -eu`, retain
    the current Release configuration, static linker flags and
    `CF_WITH_CXX=OFF`, and use a shared logical prefix
    `/opt/configforge` for configure (`cmake/ConfigForgeInstall.cmake:92-110`). Stage using
    `cmake --install "$work/build" --prefix "$work/stage/configforge-$v"`.
    Keep the normal CMake install and the current two stripped tool
    copies (`configforge`, `configforge-doctor`) and their 0755 modes.
    Run the existing full CTest command for each build and let its
    exit code reach the caller. Install the notice with mode 0644 at
    the existing path. Preserve every archive member and its
    executable or non-executable mode from the current Linux job.
  * Write `$work/configforge-$v-linux-x86_64-musl.tar.gz` with GNU tar
    using `--sort=name --mtime="@$SOURCE_DATE_EPOCH" --owner=0 --group=0
    --numeric-owner --format=gnu -czf` and the existing
    `configforge-$v` root under `$work/stage`. Compression reads tar
    bytes on stdin so the gzip header carries no input filename or
    mtime. Normalize archive metadata at packaging time. Leave
    library build IDs and SBOM contents in the artifact. If an actual
    compiled path differs, use GCC `-ffile-prefix-map` for the two
    source and work roots in this Linux recipe, mapping respectively
    to `/usr/src/configforge` and `/usr/src/configforge-build`.
  * Compare the two final files with `cmake -E compare_files`. On
    disagreement, print both SHA-256 hashes and compare the staged
    trees with `diff -qr` for diagnosis before returning nonzero.
    Copy the first archive into `dist/` only after comparison
    succeeds and use the existing upload step and artifact name.
    Add `scripts/release/linux_musl.sh` to the workflow's existing
    main dry-run path filter. This gate covers the Linux musl archive
    within the job's one toolchain and dependency set. Record that
    scope in the release-script README and the Roadmap.
* **files**: `.github/workflows/release.yml`,
  `scripts/release/linux_musl.sh`, `scripts/README.md`,
  `docs/Roadmap.md`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/N-2`.
  2. See it fail. On Alpine 3.20 with the job's existing packages, run the current
     archive recipe twice from the same revision in scratch trees and
     run `cmake -E compare_files` on the two archives. Ensure a
     deterministic failing control by setting the two installed
     root directories' mtimes to different fixed times before the
     original tar commands. Record the comparison's nonzero exit
     code and both hashes before changing the recipe.
  3. Write the single-build recipe. Run it on both clean checkouts
     with the same epoch. Compare the final files. List one archive
     and compare member paths and modes to the baseline. Extract it
     under a third absolute directory. Configure `tests/consumer/`
     with `-DCMAKE_PREFIX_PATH=<extracted configforge-$v root>` and
     build `consumer` and `consumer_serialize`. Check that both `.pc`
     files use `prefix=${pcfiledir}/../..` and that this relative path
     from their directory resolves to the extracted tree. Check the
     archive includes the SBOM, copyright notice and tools.
  4. Wire both builds and the comparison into the existing release
     job. In a scratch run, append a byte to the second archive
     immediately before comparison and check for the nonzero exit
     and absence of a copy in a fresh `dist/`. Remove the scratch
     mutation. Document the successful hashes and the mismatch
     control's exit code under Notes.
  5. Update the Roadmap, the release-script README and the changelog.
     Run ACCEPT. Dispatch the release workflow from `work/N-2`,
     verify the Linux job result independently of the other dry-run jobs and check its
     `linux-musl` upload contains exactly the compared archive.
* **accept**: `docs/Plan.md:69-100`, including ACCEPT-CI for the card
  branch, plus a successful Linux musl job in its release dry run.
  Both complete builds and test runs pass, both archives compare
  equal, and the mismatch control stops before the release copy.
* **stop if**: A byte difference survives the normalization and the
  scoped path mapping, or the normal CMake or pkg-config consumer
  needs a change in package layout. Report the smallest affected
  member and both hashes. Follow the three-attempt rule in
  `docs/Plan.md:27-36`.
* **Organiser's answers**: 2026-11-23: there is no Alpine (Docker) on the
  deputy's machine; the "see it fail" and the two-build comparison run
  in CI on the branch instead: the workflow change is written first with
  a deliberate difference between the two builds (an environment value
  baked into one archive) so that the branch's first run must fail at the
  comparison, then the difference is removed and the second run passes.
  Both run ids go under "notes". Locally: actionlint's rules by reading,
  the MSVC suites as usual.
* **notes**: The release dry run on `work/N-2` (commit `7d26929`) failed
  at the Linux musl archive comparison as intended. The job printed both
  archives' SHA-256 hashes and reported that `configforge.spdx.json` differed
  after the deliberate one-second increase to `SOURCE_DATE_EPOCH` in the
  second build.

  Release dry runs on work/N-2: 45265306838 all green (the two archives
  equal, publish skipped), after the first, with the deliberate
  difference, failed at the comparison as it should.
## N-3 - show a tracked JSON file's value changes in Git difftool

* **id**: N-3
* **goal**: A repository-local Git difftool recipe lists changed keys in a
  tracked flat JSON mapping without displaying their values.
* **why**: `docs/Roadmap.md:1368-1383` asks for both a Git difftool and
  `textconv`. `codec dump` prints source places, reader order and values
  (`docs/Tools.md:116-132`), so this slice calls the two-file comparison from
  N-1. The Roadmap's `textconv` work stays open.
* **needs**: N-1 (landed with its CLI and exit status contract).
* **size**: small
* **decided**:
  * D-100 (`docs/plan/A-architecture.md:40-73`,
    `docs/plan/A-architecture.md:95-99`): use the existing front door and a
    cross-component test. This card changes documentation and a CMake test,
    within the project's C89 baseline.
  * Add a `### Git difftool for JSON` subsection to N-1's `configforge diff`
    documentation in `docs/Tools.md`. With `configforge` on `PATH`, add this
    stanza to the repository's local Git config (`git config --local --edit`):

    ```gitconfig
    [difftool "configforge-json"]
      cmd = "configforge diff --as-left json --as-right json -- \"$LOCAL\" \"$REMOTE\"; rc=$?; test \"$rc\" -eq 1 || exit \"$rc\""
    ```

    Invoke it explicitly for the file being reviewed:

    ```sh
    git difftool --no-prompt --trust-exit-code --tool=configforge-json --diff-filter=M -- "app settings.json"
    ```

    Both Git paths are quoted. The explicit input format avoids relying on
    the names of Git's temporary files. N-1's status `1` means a value change
    was shown, so the shell returns success for it and passes other statuses
    through. Describe Git's failure as a nonzero exit since Git versions can
    report a different number than the tool returned. Show one changed key
    and an order-only edit with empty output. Explain that the recipe is for
    modified regular files with flat JSON mappings of scalars. `--diff-filter=M`
    selects modifications. The opt-in command takes an explicit path and
    selects the tool for this invocation.
    Link to the [Git difftool manual](https://git-scm.com/docs/git-difftool).
  * `front.git_difftool` is a `cmake -P` test registered next to `front.run`
    (`tests/front/CMakeLists.txt:17-28`) when the N-1 target, JSON reader and
    Git are available. Use `find_package(Git QUIET)` in the test build only.
    Pass `GIT_EXECUTABLE`, `$<TARGET_FILE_DIR:configforge_front>`, the Tools
    page and a test-specific binary directory to `tests/front/git_difftool.cmake`.
    Prepend the front door directory to `PATH` with the host separator. Set
    `GIT_CONFIG_NOSYSTEM=1`, `GIT_CONFIG_GLOBAL` to an empty scratch file,
    and `GIT_TERMINAL_PROMPT=0` inside the test. Use `git init --quiet` and
    `git add --` in a scratch repository whose path contains a space. Set
    local `core.autocrlf=false` and local `core.excludesFile` to an empty
    scratch file. Use `file(READ)` and a regex to extract the stanza and
    its `cmd =` line from the docs, then append both to the scratch repo's
    `.git/config`. If absent, fail with `missing configforge-json difftool recipe`.
  * Stage `app settings.json` with `{"port":8123,"tag":"old-private"}`.
    Leave the index at this baseline. Write each of the following working
    file variants and run the documented Git command, using
    `execute_process` with separate arguments and no shell in the test
    harness. Check the status and stdout (normalize CRLF to LF):
    1. `{"tag":"old-private", "port":8123}` gives exit 0 and empty output.
    2. `{"port":9123,"tag":"new-private"}` gives exit 0 and exactly
       `changed\t$["port"]\nchanged\t$["tag"]\n` (`\t` and `\n` mean literal
       tab and newline). Check neither stream contains either value from
       the old or new file.
    3. `{"tag":"invalid-private",` gives a nonzero exit, empty stdout, and
       no `invalid-private` in stderr. This holds the propagation of tool
       errors without binding the test to Git's own diagnostic or status.
* **files**: `tests/front/CMakeLists.txt`, `tests/front/git_difftool.cmake`,
  `docs/Tools.md`; last, `docs/Roadmap.md` under the Git item records the
  completed difftool recipe and leaves the `textconv` part open.
* **steps**:
  1. `git switch -c work/N-3` after N-1 lands.
  2. Add the test and configure and build the `default` preset. Run
     `ctest --preset default -R "^front\.git_difftool$" --no-tests=error --output-on-failure`
     before writing the recipe. See it fail with the missing-recipe message.
  3. Add the recipe and the two example results. Rerun the test; all three
     Git invocations must meet their assertions. Record the result and update
     the Roadmap item.
  4. Run ACCEPT (`docs/Plan.md:69-100`).
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI; `front.git_difftool`
  must run and pass in the default Linux and Windows builds with Git and JSON.
* **stop if**: N-1's installed invocation, output or statuses differ from the
  contract above; the documented Git command needs a different wrapper on
  Linux and Windows; a value appears in either stream. Report the command and
  exit code, with fixture values omitted from failure evidence.
* **Organiser's answers**:
  * Use the default preset with the Release override in the temporary Ninja
    build for the red and green checks, then verify Release before Debug.
    The root handoff concerns other worktrees and stays with the manager.
* **notes**: On Windows MSVC/Ninja Release, `front.git_difftool` failed
  before the recipe with the expected missing-recipe message (CTest exit 8).
  After the recipe, all three invocations met their assertions (CTest exit 0).

## N-4 - deliver one root table to a YAML document sink

* **id**: N-4
* **goal**: A consumer of the YAML codec receives exactly one root table for a
  mapping, an empty mapping, or an empty input. A second YAML document is refused.
* **why**: A consumer that accepts one document currently has to special-case
  YAML. The converter and the pair tests both hide the extra event. The
  diagnosis and the two workarounds are recorded in `docs/Decisions.md:4908-4913`.
* **needs**: W5-V2 (done)
* **size**: small
* **decided**:
  * Use the shared event adapter for the built-in, libyaml and libfyaml
    backends. Add one private zero-initialised flag to `yaml_scan`. Set it
    when the sink accepts the top-level mapping in
    `lib/serialize/src/yaml.c:398-433`. At stream end synthesise the
    begin/end pair only if no mapping was delivered. Depth returns to its
    initial value after a valid mapping. The document counter has a separate
    purpose, rejecting a second document (`lib/serialize/src/yaml.c:689-719`).
  * In `tools/codec/convert_core.c:228-249` and `tests/codec/chain.h:118-136`,
    a parentless member must be the first root and a table. Return NULL for
    a second root. Preserve named table reopening below the root (INI).
  * Test the public event stream with `codec dump`. Count whole root lines
    (the three tab-separated fields are `-`, `.`, `table`). Check a scalar
    event as well, so losing the real document cannot make the count pass.
    The empty file and comment-only file each yield just the synthetic root.
    An explicit empty mapping yields just its own root. Keep today's refusal
    for multiple documents and the existing sink-failure propagation.
  * C89 and D-100 apply. The fix lives in serialize, consumers use the codec
    interface, and the CLI regression belongs in the cross-component tests.
    Reader depth limits and scalar typing stay outside this slice.
* **files**: `lib/serialize/src/yaml.c`, `tools/codec/convert_core.c`,
  `tests/codec/chain.h`, `tests/codec/dump.cmake`, `CHANGELOG.md`.
* **steps**:
  1. `git switch -c work/N-4`.
  2. Add the four fixtures above to `codec.dump`, guarded by its existing
     YAML feature flag. Start with `a: one\n`. Run
     `ctest --preset default -R "^codec[.]dump$" --output-on-failure`;
     see it fail with two root lines where one was expected.
  3. Fix the shared adapter, run the dump cases, then remove both root
     workarounds. Run the converter and ordered-pair tests, including INI
     repeated sections. Record the observable event correction in the changelog.
  4. Run ACCEPT and record which YAML backends ran.
* **accept**: ACCEPT-LINUX with `-DEXTRA=yaml`, ACCEPT-WINDOWS, ACCEPT-CI.
  On the built-in backend and each available external backend, `codec.dump`
  and `unit.test_yaml` pass. `codec.convert` and `codec.pairs` pass in the
  full build with the existing pair manifest.
* **stop if**: another reader emits two roots for a valid fixture once the
  workarounds are removed, or the backends disagree about these four inputs.
  Record the input, backend and event lines.
* **notes**: (empty)

## N-5 - compare nested tables by value in `configforge diff`

* **id**: N-5
* **goal**: `configforge diff` compares two documents whose mappings nest
  (`{"db":{"port":1}}`, YAML, TOML, INI sections) and names each changed
  scalar by its whole path, `$["db"]["port"]`. Arrays stay refused.
* **why**: N-1 refuses a nested table, so the command cannot read most real
  configuration files (`docs/Roadmap.md:1368-1377`, "What is left: nested
  shapes"). The recorder already takes one scalar per event and the output
  already quotes keys in a path form, so the slice is small
  (`tools/diff/main.c:54-115`, `192-218`).
* **needs**: N-1 (done)
* **size**: small
* **decided**:
  * **Shape**: one unnamed root table, as today, then named tables to any
    depth up to 64 segments in a path (a named table may be reopened; the
    path is the same). `begin_table` with a NULL name is accepted only
    while no table is open (root, or the root reopened after it closed, as
    in N-1). A named `begin_table` is accepted only while the root is open
    and the path has room; it pushes the name. `end` pops one table.
    Replace `side.open` by a depth counter. At the end of the decode the
    counter must be zero and a root must have been seen. `begin_array`
    still returns false, so an array anywhere is refused (exit 2).
  * **Path**: an entry stores the path as an array of `(pointer, length)`
    segments: the names of the open named tables, then the scalar's own
    name. Copy each table name into the arena once, when it opens
    (the reader's span ends with the callback); an entry copies the
    pointer array, not the bytes. Charge every allocation through
    `reserve_work` as today. An empty name is a valid segment.
  * **Order**: compare paths segment by segment; a segment compares by
    unsigned bytes, a shorter proper prefix first; when every segment of
    the shorter path matches, the shorter path comes first. Equal paths
    are one key: the entry with the highest ordinal (the last one read)
    wins, as today. So `["a","z"]` sorts before `["ab"]`, and `["a"]`
    before `["a","b"]`.
  * **Compare**: unchanged: `cf_value_equal`, the reader's scalar type is
    kept. A path that is a scalar on one side and a table on the other
    gives two lines, `removed`/`added` for the scalar's path and for each
    scalar below the table, in the order above. `{"a.b":1}` and
    `{"a":{"b":1}}` differ the same way.
  * **Empty tables**: a table with no scalar below it records nothing, so
    `{"a":{}}` equals `{}` and equals a missing `a`. Say so in Tools.md.
  * **Output**: `added`, `removed` or `changed`, a tab, `$`, then for each
    segment `["`, the escaped segment, `"]`, then a newline. Escaping is
    N-1's (backslash and double quote escaped, bytes outside printable
    ASCII as `\xhh` lowercase). A flat document prints exactly what N-1
    printed. Values and digests never appear.
  * **Limits**: the budget, the exit statuses and the fixed refusal lines
    are N-1's. A path deeper than 64 segments is a refusal like any other
    (the reader's own depth limit, `CF_MAX_DEPTH`, stops a document first
    in the default build, so a fixture cannot reach it).
  * C89. No public API, no new target, no new file in `tools/`.
* **files**: `tools/diff/main.c`, `tools/configforge/main.c:40` (help
  text: drop "flat"), `tests/front/diff.cmake`, `docs/Tools.md`,
  `docs/Roadmap.md`, `CHANGELOG.md`.
* **steps**:
  1. Work in branch `work/N-5` (already cut from `main`).
  2. In `tests/front/diff.cmake` first: remove `[[{"a":{}}]]` from the
     refused list (it is now accepted) and add `[[{"a":{"b":[]}}]]` to it.
     Add the cases below, each with exact stdout, stderr and status:
     (a) reordered nested pair with `SENTINEL-A` under a nested
     `password` key: 0, empty; (b) `{"db":{"host":"h","port":1,"old":true},"top":1}`
     against `{"db":{"host":"h","port":2,"new":true},"top":1}`: 1, lines
     `added\t$["db"]["new"]`, `removed\t$["db"]["old"]`,
     `changed\t$["db"]["port"]`; (c) `{"a":1}` against `{"a":{"b":1}}`: 1,
     `removed\t$["a"]` then `added\t$["a"]["b"]`; (d) `{"ab":1,"a":{"z":1}}`
     against `{}`: 1, `removed\t$["a"]["z"]` then `removed\t$["ab"]`;
     (e) `{"a.b":1}` against `{"a":{"b":1}}`: 1, `added\t$["a"]["b"]`
     then `removed\t$["a.b"]`; (f) `{"a":{}}` against `{}`: 0; (g) a
     table `{"":{"":1}}` against `{"":{"":2}}`: 1, `changed\t$[""][""]`;
     (h) a key with a quote inside a nested table, escaped as in N-1;
     (i) 20 nested tables with a changed leaf: 1, one line of 21
     segments; 40 nested tables: 2 with the right-side refusal line;
     (j) with `HAVE_YAML`, a nested YAML mapping against the equal JSON
     with string leaves: 0. Run `ctest --preset default -R "^front[.]diff$"
     --output-on-failure`; see it fail on the first nested case (status 2
     where 0 was expected).
  3. Change `tools/diff/main.c` as decided. Run the test until it passes;
     then `ctest --preset default -R "^(front|codec)[.]" `.
  4. Update Tools.md (the table row, the section text, the recipe's
     "flat"), the help line, the Roadmap item (nested tables built
     (card N-5); arrays and contract still left) and the Changelog.
  5. Run ACCEPT.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `front.diff`,
  `front.git_difftool`, `front.run` and the codec tests pass; the YAML case
  runs in a build with the YAML reader.
* **stop if**: a reader emits a named `begin_table` while no root is open,
  or reopens a named table in a way that makes one path two tables with
  different scalars lost. Record the format and the input.
* **notes**: (empty)
## N-6 - `cf_usage`: the `--help` text from the contract

* **id**: N-6
* **goal**: `cf_usage(cfg, program, env_prefix, buf, cap)` writes a `--help`
  text from the definitions of a built configuration: each key's flags,
  type, description, default, range, permitted spellings, environment
  name and flags. `examples/c/tier1.c` prints it for `--help`, and a test
  holds both.
* **why**: A definition already holds the description, aliases, permitted
  spellings, range, default and environment name
  (`lib/config/include/configforge/cf_define.h:68-134`), yet every program
  writes its `--help` by hand, a second list that drifts from the
  contract. This is the first part of the Roadmap item "Help, completions
  and a sample file from the contract" ("From reading the project as a
  newcomer, 2026-11-14"). Not in this card: completions for bash, zsh,
  fish and PowerShell, a man page, a sample file, and `doctor gen`. They
  are later cards that read the same definitions.
* **needs**: nothing
  * Declaration in `lib/config/include/configforge/configforge.h`, after
    `cf_schema` (`configforge.h:670-671`), as
    `CF_EXPERIMENTAL(1, 8) size_t cf_usage(const cf_config* c, const char* program, const char* env_prefix, char* buf, size_t cap);`
    The release is 1.8.0 (open in CHANGELOG.md). gate.abi's list is
    `cmake/configforge.experimental.symbols`: add `cf_usage 1.8` in the
    file's order. No other header and no `CF_API` item changes.
  * Measure-or-write as `cf_schema` and `cf_explain` are, through
    `cf_sink` (`lib/serialize/include/configforge/cf_serialize.h:79-92`):
    the return value is the length the whole text has, without the NUL;
    `buf == NULL` measures; a `buf` too small gets the prefix that fits,
    NUL-terminated (`cf_sink_terminate`), and the return value is still
    the full length. `c == NULL`: return 0 and set `buf[0] = '\0'` when
    `buf != NULL && cap > 0` (as `cf_schema`, `config.c:2381-2393`).
    `program == NULL` prints `program`. `env_prefix == NULL` prints no
    `env` item; otherwise it is the string given to `cf_builder_env`
    (`cf_app` uses the upper-cased name and an underscore, `"MYAPP_"`),
    and the name is `cf_keymap_from_key(&m, key, len, CF_FALSE, out, cap)`
    with `m` from `cf_keymap_init(&m, CF_KEYMAP_ENV, env_prefix)`
    (`lib/config/src/sources/entry.c:229-271`), the natural spelling, into
    a 256-byte local buffer; a key for which it returns 0 gets no `env`
    item.
  * New file `lib/config/src/diag/usage.c`, listed in
    `lib/config/CMakeLists.txt` after `src/diag/diff.c`. It needs no
    `CF_WITH_REPORT`. It walks `cf_defs(c)` with `cf_defset_count` and
    `cf_defset_at` in the order they come (sorted by key once the set is
    sealed, which `cf_builder_create` does). Only authored definitions
    are listed, never a list's element projections.
  * The text, exactly. It begins
    `Usage: <program> [options]`, a newline, an empty line, `Options:`,
    a newline; then one block for each definition. With no definitions
    only the `Usage:` line is written (no empty line, no `Options:`). A
    definition's block:
    * Line 1: two spaces, then the flags, then the value. The flags are
      each alias of `d->aliases` in order (`-x` when the alias is one
      byte, `--word` otherwise, `cf_define.h:95-99`), each followed by
      `, `, then `--` and the key. The value is absent for `CF_TYPE_BOOL`;
      otherwise a space, `<`, the permitted spellings of `d->allowed`
      joined by `|` when there are any, else `cf_type_name(d->type)`
      (`int`, `real`, `text`), and `>`; then `...` when
      `d->list_mode != CF_LIST_NONE`.
    * Then the description, when not NULL and not empty: eight spaces and
      the text, a newline after it; inside the text each newline is
      followed by eight spaces.
    * Then one line of details, eight spaces and the items joined by `; `,
      only if there is at least one item; the items in this order:
      `required` (CF_REQUIRED); `default X` (`has_default`, a scalar,
      not CF_SENSITIVE; X is `cf_value_to_text` of `default_value`, in
      double quotes for `CF_TYPE_TEXT`, with no escaping); `range A..B`
      (`has_range`, for `CF_TYPE_INT` from `imin`, `imax`, for
      `CF_TYPE_REAL` from `rmin`, `rmax` as `cf_value_to_text` writes
      them, the real part only `#if CF_WITH_REAL`); `length A..B`
      (`CF_TYPE_TEXT` and `max_len != 0`, from `min_len`, `max_len`);
      `repeatable` (`list_mode != CF_LIST_NONE`);
      `deprecated, use --NEW` when CF_DEPRECATED and `replaced_by` is set,
      else `deprecated`; `env NAME`; `sensitive` (CF_SENSITIVE).
    * Every line ends with `\n`. No wrapping, no colour, no terminal width.
  * The library does not intercept `--help` itself. A program declares a
    `help` bool (as `tier1.c` declares `cf.explain`) and calls `cf_usage`.
  * The golden text of the unit test, program `myapp`, prefix `MYAPP_`,
    for the contract below (in the file the lines are flush left; each
    continuation line has eight spaces, shown here as written):
    ```
    Usage: myapp [options]

    Options:
      --db.password <text>
            Database password.
            Set it in the environment.
            env MYAPP_DB_PASSWORD; sensitive
      --log.level <debug|info|warn>
            Log level.
            default "info"; env MYAPP_LOG_LEVEL
      --old.port <int>
            deprecated, use --server.port; env MYAPP_OLD_PORT
      -p, --server.port <int>
            Port to listen on.
            default 8080; range 1..65535; env MYAPP_SERVER_PORT
      --tags <text>...
            Labels.
            repeatable; env MYAPP_TAGS
      --tls.enabled
            default false; env MYAPP_TLS_ENABLED
    ```
    Here the whole block is indented four spaces by the card; remove
    those four, so `Usage:` is at column 0, `--db.password` at column 2
    and `Database password.` at column 8. The contract: `server.port`
    int, default 8080, range 1..65535, alias `p`, description "Port to
    listen on."; `log.level` text, permitted debug/info/warn, default
    "info", description "Log level."; `tls.enabled` bool default false
    (`cf_builder_bool`); `db.password` text, default "x", CF_SENSITIVE,
    description "Database password.\nSet it in the environment.";
    `old.port` int, CF_DEPRECATED, `replaced_by` "server.port"; `tags`
    text, `list_mode` CF_LIST_APPEND, `list_separator` ',', description
    "Labels.". Each is a `cf_def` (`cf_def_init`, fields, then
    `cf_builder_define`) except `tls.enabled`. The default `x` of
    `db.password` must not appear in the text.
  * `tier1.c`: declare `help` as a bool `cf_def` with `aliases = {"h",
    NULL}` and description "Print this help and exit."; give
    `server.port`, `server.host`, `tls.enabled` and `log.level`
    descriptions by `cf_def` where the shorthand has none. After
    `cf_builder_create`, if `cf_get_bool(cfg, "help", CF_FALSE)`, print
    `cf_usage(cfg, "tier1", "MYAPP_", buf, sizeof buf)` to stdout and
    return 0. The `cf.explain` path and the three WILL_FAIL tests are
    unchanged.
  * C89 (no `//`, declarations at block start). Layer D-100: `usage.c`
    includes config's, base's and `cf_serialize.h` as `report.c` does,
    nothing from `tools/`.
* **files**: `lib/config/include/configforge/configforge.h`,
  `lib/config/src/diag/usage.c` (new), `lib/config/CMakeLists.txt`,
  `lib/config/tests/test_help.c` (new), `lib/config/tests/CMakeLists.txt`
  (add `test_help` to `CF_UNIT_TESTS`),
  `cmake/configforge.experimental.symbols`, `examples/c/tier1.c`,
  `examples/c/CMakeLists.txt`, `docs/Guide.md` (tier 1: `$ ./tier1 --help`
  and its first lines, pasted from the run), `CHANGELOG.md` (1.8.0,
  Added), `docs/Roadmap.md`, `docs/API.md` and `docs/Codemap.md`
  (generated).
  1. The branch is `work/N-6` (already cut).
  2. Add the declaration, the symbols-file line, and `usage.c` as a stub
     that returns 0 after the `c == NULL` rule; list it in CMake. Write
     `test_help.c` (C89, `cf_test.h`, `check()`, ending `return
     cf_test_done();`): the golden compare; the measured length equals
     the written length and `strlen` of the golden; a 20-byte buffer
     holds the first 19 bytes NUL-terminated and the return value is the
     full length; `c == NULL`; `env_prefix == NULL` leaves no ` env ` and
     no `MYAPP`; `program == NULL` starts `Usage: program [options]`; a
     contract with no definitions gives exactly `Usage: myapp
     [options]\n`; the string `default "x"` is absent. Add `test_help` to the list. Build in a
     copy, run it (`ctest -N | grep test_help` for the name): **see it
     fail** on the golden compare.
  3. Write the real `usage.c`. The test passes.
  4. `tier1.c` and, in `examples/c/CMakeLists.txt`, the test
     `example.tier1.help`: `add_test(NAME example.tier1.help COMMAND
     ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:tier1>
     -DDIR=${CMAKE_CURRENT_BINARY_DIR}/tier1-help -DARGS=--help
     "-DEXPECT=Usage: tier1 \\[options\\]|||--server.port <int>|||range
     1..65535" -P ${CF_EXAMPLE_EXPECT})` (`examples/expect.cmake`; check
     that the variable is set where `examples/c` is added, else use the
     path `${CMAKE_SOURCE_DIR}/examples/expect.cmake`; adjust the
     escapes until it passes). See it fail first by running it before
     `tier1.c` is changed. Paste the first lines of the real `./tier1
     --help` into `docs/Guide.md`.
  5. On Linux regenerate `docs/API.md`, `docs/Codemap.md` and
     `docs/Limits.md` (the three cmake commands of the brief's step 6 and
     `-DWRITE=docs/Limits.md -P scripts/generate/limits.cmake`); the
     CHANGELOG entry; the Roadmap item marked "(card N-6, `cf_usage`
     only)"; `python scripts/work/work.py format`.
  6. ACCEPT, and `ctest -R "^gate[.]"` in the Linux copy.
* **accept**: ACCEPT-LINUX, ACCEPT-WINDOWS, ACCEPT-CI. `test_help`,
  `example.tier1.help` and every `gate.*` (`gate.abi` among them) pass;
  in the `minimal` preset `test_help` still passes.
* **stop if**: `gate.abi` or `gate.apiref` wants a declaration that
  differs from the other `CF_EXPERIMENTAL(1, 8)` ones; the order of
  `cf_defset_at` is not bytewise by key (say so and change only the
  golden's order, not the rule); `cf_builder_create` refuses the
  contract (say which definition and why).
* **notes**: Organiser's answer: the item order in "decided" put `env` before `deprecated` while the golden text puts `deprecated` first; the golden is what reads right, so the rule was changed to match it (`deprecated`, then `env`, then `sensitive`).

## N-7 - a reload names the keys it changed

* **id**: N-7
* **goal**: When a `cf_live` swap makes a new configuration the one readers
  get, the log carries one `"key-changed"` event per changed key, at debug
  level, naming the key and the source its new value comes from, never a
  value.
* **why**: A swap's `"reload"` event says the generation and the
  fingerprints, not which keys moved. A program gets that today only by
  keeping the old snapshot and calling `cf_diff` itself. A log pipeline that
  must tell what a reload did at 03:00 has nothing to read. The swap already
  holds both snapshots, so the diff costs one pass, and only when a sink is
  set at debug. Roadmap, "From a review of the whole, 2026-11-14".
* **size**: small
* **decided**:
  * The event is made in `live_swap` (`lib/config/src/core/live.c:311-346`),
    after the fingerprints move (`previous`, `fingerprint`) and before the old
    snapshot is released, because that is the only place both snapshots are
    alive. It follows that the `"key-changed"` events come before the swap's
    `"reload"` event (`live_record`, `lib/config/src/core/live.c:440-476`),
    all carrying the new fingerprints. Document the order.
  * Only a swap that is not `quiet` logs them. A quiet swap (same values)
    and a rollback log none; a rollback keeps its `"reload-rolled-back"`.
    A swap made by `cf_live_approve` goes through `live_swap` and logs them.
  * Skip the diff altogether unless `l->log != NULL` and
    `l->log_min <= CF_LOG_DEBUG`. Otherwise no cost is added.
  * One event per `cf_diff` change (`lib/config/src/diag/diff.c:35-103`),
    level `CF_LOG_DEBUG`, `event` `"key-changed"`, `code` `CF_OK`, `key` the
    key copied NUL-terminated into a `CF_KEY_MAX` buffer (truncated as
    `live_log_restarts` does, `live.c:416-436`), `source` the name of the
    winning source in the new configuration, `message` one of the fixed
    sentences `"added"`, `"changed"`, `"removed"`, `"same value, from
    another source"` for `CF_CHANGE_ADDED`, `CF_CHANGE_VALUE`,
    `CF_CHANGE_REMOVED`, `CF_CHANGE_ORIGIN`. For a removed key `source` is
    NULL. No value is ever formatted: a secret needs no special case.
  * A `CF_RESTART` key that keeps its running value is reported by `cf_diff`
    as `CF_CHANGE_ORIGIN`, source `"running"`; it is logged as such, beside its
    `"restart-pending"` event. The log says what the diff says, with no case
    of its own.
  * `live_log` (`live.c:395-412`) keeps its signature (ten callers); a new
    `live_event` takes the `source` too and `live_log` passes NULL to it.
  * The source name comes from `diff_source_name` (`diff.c:124-133`), made
    non-static as `cfi_source_name` and declared beside `cfi_entry_secret` in
    `lib/config/include/configforge/cf_define.h:301`, so that the lookup is
    written once. `diff.c` calls the new name.
  * `"key-changed"` is added to the stable names in `cf_log.h:46-52` and in
    `cf_live.h:140-142`. Nothing else in the public interface changes: no
    new function, no new member.
  * C89; the callback is a `static` function in `live.c` taking a small
    struct `{const cf_live* l; const cf_config* next;}` as `ctx`.
  * The marker test (`lib/config/tests/test_marker.c:744-790`) already
    reloads with `log_min = CF_LOG_DEBUG` and checks every event's key,
    source and message for the secret; it covers the new events unchanged.
* **files**: `lib/config/src/core/live.c`, `lib/config/src/diag/diff.c`,
  `lib/config/include/configforge/cf_define.h`,
  `lib/config/include/configforge/cf_log.h`,
  `lib/config/include/configforge/cf_live.h`, `lib/config/tests/test_log.c`,
  `CHANGELOG.md`, and the generated `docs/API.md`, `docs/Codemap.md`,
  `docs/Limits.md` if they change.
* **steps**:
  1. `git switch -c work/N-7` (already cut by the organiser's worktree).
  2. In `lib/config/tests/test_log.c` add a `source[32]` member to `seen`
     and its copy in `sink`. In `test_live`, after the first reload to `g_v2`
     assert: `find("key-changed","server.port")` and
     `find("key-changed","log.level")` exist, level `CF_LOG_DEBUG`, source
     `"mem"`, message `"changed"`, and no message or key of any event holds
     `"9443"` or `"debug"`. After the following reload that changes nothing,
     assert no `"key-changed"` was seen. Run
     `ctest --preset default -R "^unit[.]test_log$" --output-on-failure`
     (find the exact name with `ctest -N | grep log`); see it fail at the
     first new assertion.
  3. Make the change as decided. Run the same test and `test_live`,
     `test_marker`, `test_diff`; all pass.
  4. Add one test with `log_min = CF_LOG_INFO`: a swap logs `"reload"` and
     no `"key-changed"`.
  5. CHANGELOG, 1.8.0 "Added": one sentence. Format, regenerate the three
     generated documents on Linux, run `ctest -R "^gate\."`.
* **accept**: ACCEPT-LINUX (default preset, ctest with `unit.test_log`,
  `unit.test_live`, `unit.test_marker`, `unit.test_diff` and every `gate.`
  test passing), ACCEPT-CI.
* **stop if**: the marker test finds a value in an event, or a swap with a
  sink at debug becomes measurably slower on `bench` (it must not: one pass
  over the keys). Record which.
* **notes**: (empty)
