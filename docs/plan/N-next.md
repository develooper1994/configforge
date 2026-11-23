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
* **notes**: (empty)

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
* **notes**: (empty)
