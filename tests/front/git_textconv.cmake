# Git's textconv driver for configuration (N-16): `git diff` and `git log -p`
# print a JSON or YAML file one line a key, so a re-indent or a comment shows
# nothing and a changed value shows one line; a file the readers refuse is
# shown as it is and `git diff` does not fail. The stanza and the
# .gitattributes lines are read from docs/Tools.md, so the page is held to
# what works. Skipped (the test is not added) where git is absent.
#
#   -DGIT_EXECUTABLE -DFRONT_DIR -DTOOLS_PAGE -DWORK -DHAVE_YAML
file(MAKE_DIRECTORY "${WORK}")
file(READ "${TOOLS_PAGE}" page)
string(REGEX MATCH "\\[diff \"configforge\"\\]\n[ \t]+textconv = ([^\n]+)"
             stanza "${page}")
if(stanza STREQUAL "")
  message(
    FATAL_ERROR
      "missing the [diff \"configforge\"] textconv stanza in docs/Tools.md")
endif()
set(tc "${CMAKE_MATCH_1}")
string(REGEX MATCH "```gitattributes\n([^`]*)```" fence "${page}")
if(fence STREQUAL "")
  message(FATAL_ERROR "missing the gitattributes block in docs/Tools.md")
endif()
set(attributes "${CMAKE_MATCH_1}")

if(CMAKE_HOST_WIN32)
  set(path_separator ";")
else()
  set(path_separator ":")
endif()
set(ENV{PATH} "${FRONT_DIR}${path_separator}$ENV{PATH}")
set(ENV{GIT_CONFIG_NOSYSTEM} 1)
set(ENV{GIT_CONFIG_GLOBAL} "${WORK}/empty.gitconfig")
set(ENV{GIT_TERMINAL_PROMPT} 0)
file(WRITE "${WORK}/empty.gitconfig" "")
file(WRITE "${WORK}/empty.excludes" "")

set(repo "${WORK}/scratch repo")
file(REMOVE_RECURSE "${repo}")
file(MAKE_DIRECTORY "${repo}")

# Set up the scratch repository, failing without printing fixtures.
function(setup_git)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" ${ARGN}
    WORKING_DIRECTORY "${repo}"
    RESULT_VARIABLE status
    OUTPUT_QUIET ERROR_QUIET)
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "git ${ARGN} exited with ${status}")
  endif()
endfunction()

function(commit message)
  setup_git(add -A)
  setup_git(
    -c
    user.name=t
    -c
    user.email=t@example.invalid
    -c
    commit.gpgsign=false
    commit
    --quiet
    -m
    "${message}")
endfunction()

# git with the driver given on the command line; any failure fails the test.
# The result is in git_out and git_err.
function(run_git)
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" -c "diff.configforge.textconv=${tc}" ${ARGN}
    WORKING_DIRECTORY "${repo}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  string(REPLACE "\r\n" "\n" out "${out}")
  if(NOT status STREQUAL "0")
    message(FATAL_ERROR "git ${ARGN} exited with ${status}\n${out}\n${err}")
  endif()
  set(git_out
      "${out}"
      PARENT_SCOPE)
  set(git_err
      "${err}"
      PARENT_SCOPE)
endfunction()

function(expect name text re)
  if(NOT text MATCHES "${re}")
    message(FATAL_ERROR "${name}: no line matches '${re}'\n${text}")
  endif()
endfunction()

function(refuse name text re)
  if(text MATCHES "${re}")
    message(FATAL_ERROR "${name}: unexpected '${re}'\n${text}")
  endif()
endfunction()

setup_git(init --quiet)
setup_git(config --local core.autocrlf false)
setup_git(config --local core.excludesFile "${WORK}/empty.excludes")

# Version 1.
file(WRITE "${repo}/.gitattributes" "${attributes}")
file(WRITE "${repo}/app.json"
     "{\"port\":8123,\"s\":\"SENTINEL-A\",\"tags\":[\"x\",\"y\"]}\n")
if(HAVE_YAML)
  file(WRITE "${repo}/app.yaml" "port: 8123\ns: SENTINEL-A\n")
endif()
file(WRITE "${repo}/bad.json" "{\"k\":1}\n")
file(WRITE "${repo}/plain.txt" "one\n")
commit("one")

# Version 2: app.json re-indented over several lines with two values
# changed, app.yaml with a comment and one value changed (a plain YAML scalar
# is untyped text and a quoted one is not, so quoting would be a change),
# bad.json refused.
file(
  WRITE "${repo}/app.json"
  "{\n  \"port\": 9123,\n  \"s\": \"SENTINEL-B\",\n  \"tags\": [\"x\", \"y\"]\n}\n"
)
if(HAVE_YAML)
  file(WRITE "${repo}/app.yaml" "# a comment\nport: 9123\ns: SENTINEL-A\n")
endif()
file(WRITE "${repo}/bad.json" "{\"k\":\n")
file(WRITE "${repo}/plain.txt" "two\n")
commit("two")

# a. The changed values are two lines each; a re-indent is no change. The
# values appear as the file holds them (decided: textconv shows no more than
# a plain git diff does, and nothing is redacted).
run_git(
  diff
  HEAD~1
  HEAD
  --
  app.json)
set(d "${git_out}")
expect(app.json "${d}" "(^|\n)-port\tint\t8123\n")
expect(app.json "${d}" "(^|\n)\\+port\tint\t9123\n")
expect(app.json "${d}" "(^|\n)-s\ttext\t\"SENTINEL-A\"\n")
expect(app.json "${d}" "(^|\n)\\+s\ttext\t\"SENTINEL-B\"\n")
refuse(app.json "${d}" "(^|\n)[-+]tags")
refuse(app.json "${d}" "\"port\"")
refuse(app.json "${d}" "{")

# b. YAML: a comment is no change.
if(HAVE_YAML)
  run_git(
    diff
    HEAD~1
    HEAD
    --
    app.yaml)
  set(d "${git_out}")
  expect(app.yaml "${d}" "(^|\n)-port\t[a-z]+\t\"?8123")
  expect(app.yaml "${d}" "(^|\n)\\+port\t[a-z]+\t\"?9123")
  refuse(app.yaml "${d}" "(^|\n)[-+]s\t")
endif()

# c. A file the readers refuse is shown as it is, and git does not fail
# (run_git fails the test on any exit but 0).
run_git(
  diff
  HEAD~1
  HEAD
  --
  bad.json)
set(d "${git_out}")
expect(bad.json "${d}" "(^|\n)-k\tint\t1\n")
expect(bad.json "${d}" "(^|\n)\\+\\{\"k\":\n")
expect(bad.json "${git_err}" "codec:")

# d. No driver on a path without the attribute.
run_git(
  diff
  HEAD~1
  HEAD
  --
  plain.txt)
expect(plain.txt "${git_out}" "(^|\n)-one\n")
expect(plain.txt "${git_out}" "(^|\n)\\+two\n")

# e. git log -p: the first commit from nothing, and the second.
run_git(
  log
  -p
  --format=%s
  --
  app.json)
expect(log "${git_out}" "(^|\n)\\+port\tint\t8123\n")
expect(log "${git_out}" "(^|\n)\\+port\tint\t9123\n")

# f. A change in the working tree, which the driver reads by its real path.
file(WRITE "${repo}/app.json"
     "{\"port\":9124,\"s\":\"SENTINEL-B\",\"tags\":[\"x\",\"y\"]}\n")
run_git(diff -- app.json)
expect(worktree "${git_out}" "(^|\n)-port\tint\t9123\n")
expect(worktree "${git_out}" "(^|\n)\\+port\tint\t9124\n")

# g. The plain diff is one flag away.
run_git(
  diff
  --no-textconv
  HEAD~1
  HEAD
  --
  app.json)
expect(no-textconv "${git_out}" "\"port\"")
