# The Doctor action's scripts and the pre-commit hook, run as a project's
# CI would run them, on the fixture reports. comment.sh talks to a stub
# gh that records what it was asked, so no forge is needed.
#
#   cmake -DDOCTOR=<doctor> -DGEN=<gen> -DBASH=<bash>
#         -DACTION=<actions/doctor> -DWORK=<dir with the fixtures>
#         -P action.cmake

# Stops the test with what went wrong, given in as many pieces as it
# takes to keep a line short.
function(fail)
  string(JOIN "" what ${ARGV})
  message(FATAL_ERROR "doctor action: ${what}")
endfunction()

# Runs `bash <script>` with the environment given as NAME=value pairs.
function(run_script out_rc out_text script)
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${ARGN} ${BASH} ${script}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE text
    ERROR_VARIABLE err)
  set(${out_rc}
      ${rc}
      PARENT_SCOPE)
  set(${out_text}
      "${text}${err}"
      PARENT_SCOPE)
endfunction()

set(md ${WORK}/action-test.md)
set(sarif ${WORK}/action-test.sarif)
file(REMOVE ${md} ${sarif})

# A change that moves a value from the environment to a file: a warning,
# which fails a run that fails on warnings, and every output written.
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_staging.json
  CF_BASE=cf_fx_prod.json
  CF_FAIL_ON=warning
  CF_SARIF=${sarif}
  CF_MARKDOWN=${md})
if(NOT rc EQUAL 1)
  fail("a warning in the diff should fail on warnings, got ${rc}:\n${text}")
endif()
if(NOT text MATCHES "CFD002")
  fail("the log does not show the diff's origin drift:\n${text}")
endif()
file(READ ${md} m)
set(diff_heading "### ConfigForge Doctor: diff cf\\\\_fx\\\\_prod.json, ")
string(APPEND diff_heading "cf\\\\_fx\\\\_staging.json")
foreach(
  want
  "^<!-- configforge-doctor -->"
  "### ConfigForge Doctor: lint cf\\\\_fx\\\\_staging.json"
  "${diff_heading}"
  "\\| CFD002 \\| warning \\| server.host \\|"
  "\\| CFD014 \\| info \\| db.password \\|"
  "this check fails")
  if(NOT m MATCHES "${want}")
    fail("the Markdown lacks '${want}':\n${m}")
  endif()
endforeach()
# A secret is compared by digest: neither side's value is in anything a
# pull request would show.
foreach(secret prodsecret devsecret)
  if(m MATCHES "${secret}" OR text MATCHES "${secret}")
    fail("a secret reached the output: ${secret}")
  endif()
endforeach()
file(READ ${sarif} s)
if(NOT s MATCHES "\"version\": \"2.1.0\"")
  fail("no SARIF was written")
endif()

# With show-values unset the action passes --redact-all: no plain value
# reaches the Markdown or the log (D-86). CF_SHOW_VALUES=true brings the
# values back.
foreach(needle 9090 127.0.0.1 "127\\.0\\.0\\.1")
  string(FIND "${m}${text}" "${needle}" at)
  if(NOT at EQUAL -1)
    fail("a value reached the output with show-values unset: ${needle}")
  endif()
endforeach()
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_staging.json
  CF_BASE=cf_fx_prod.json
  CF_FAIL_ON=never
  CF_SHOW_VALUES=true
  CF_MARKDOWN=${md})
file(READ ${md} m)
if(NOT m MATCHES "9090")
  fail("show-values true did not show the values:\n${m}")
endif()

# The same change passes a run that fails only on errors.
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_staging.json
  CF_BASE=cf_fx_prod.json
  CF_MARKDOWN=${md})
file(READ ${md} m)
if(NOT rc EQUAL 0 OR NOT m MATCHES "Nothing at `error` or above")
  fail("errors only: expected a pass, got ${rc}:\n${m}")
endif()

# Unusable input is exit 2, and the Markdown says so rather than nothing.
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_none.json
  CF_MARKDOWN=${md})
file(READ ${md} m)
if(NOT rc EQUAL 2 OR NOT m MATCHES "could not use its input")
  fail("an unreadable report: expected 2 and a note, got ${rc}:\n${m}")
endif()
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_prod.json
  CF_FAIL_ON=sometimes)
if(NOT rc EQUAL 2)
  fail("an unknown --fail-on should be refused, got ${rc}")
endif()

# Only what the change touched, passed through to the lint; and asked for
# without a base to measure from, refused.
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_staging.json
  CF_BASE=cf_fx_staging.json
  CF_ONLY_CHANGED=true
  CF_FAIL_ON=info)
if(NOT rc EQUAL 0 OR NOT text MATCHES "on what the change did not touch")
  fail("only-changed against itself: expected a pass, the rest counted, "
       "got ${rc}:\n${text}")
endif()
run_script(
  rc
  text
  ${ACTION}/run.sh
  CF_DOCTOR=${DOCTOR}
  CF_REPORT=cf_fx_staging.json
  CF_ONLY_CHANGED=true)
if(NOT rc EQUAL 2)
  fail("only-changed without a base should be refused, got ${rc}")
endif()

# The pre-commit hook: each report, its options before them, the worst
# exit code; nothing staged is nothing to say.
run_script(rc text ${ACTION}/pre-commit.sh CONFIGFORGE_DOCTOR=${DOCTOR})
if(NOT rc EQUAL 0)
  fail("no reports: expected 0, got ${rc}:\n${text}")
endif()
execute_process(
  COMMAND
    ${CMAKE_COMMAND} -E env CONFIGFORGE_DOCTOR=${DOCTOR} ${BASH}
    ${ACTION}/pre-commit.sh --fail-on warning cf_fx_staging.json cf_fx_dead.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE text
  ERROR_VARIABLE err)
if(NOT rc EQUAL 1 OR NOT text MATCHES "CFD010")
  fail("the hook should fail on the dead source's warning, "
       "got ${rc}:\n${text}${err}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CONFIGFORGE_DOCTOR=${DOCTOR} ${BASH}
          ${ACTION}/pre-commit.sh --base cf_fx_prod.json cf_fx_staging.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE text)
if(NOT rc EQUAL 0 OR NOT text MATCHES "CFD002")
  fail("the hook's --base should diff, and pass on errors only, "
       "got ${rc}:\n${text}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CONFIGFORGE_DOCTOR=${DOCTOR} ${BASH}
          ${ACTION}/pre-commit.sh --nonsense cf_fx_staging.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 2)
  fail("an unknown option should be refused, got ${rc}")
endif()

# The contract hook: each file on its own, the worst exit code, the
# refusal named; without --schema the hook is unusable.
set(contract ${CMAKE_CURRENT_LIST_DIR}/contract.schema.json)
execute_process(
  COMMAND
    ${CMAKE_COMMAND} -E env CONFIGFORGE_GEN=${GEN} ${BASH} ${ACTION}/check.sh
    --schema ${contract} cf_fx_contract_ok.json cf_fx_contract_typo.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE text)
if(NOT rc EQUAL 1
   OR NOT text MATCHES "cf_fx_contract_ok.json: ok"
   OR NOT text MATCHES "cf_fx_contract_typo.json: unknown key 'server.prot'")
  fail("the contract hook should pass one file and refuse the other, "
       "got ${rc}:\n${text}")
endif()
execute_process(
  COMMAND ${CMAKE_COMMAND} -E env CONFIGFORGE_GEN=${GEN} ${BASH}
          ${ACTION}/check.sh cf_fx_contract_ok.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 2)
  fail("the contract hook without --schema should be refused, got ${rc}")
endif()

# changes.sh in a repository of its own: the keys a change moved, per
# file, and never a value. SENTINEL is in the files so that a value that
# leaked into the Markdown would be seen.
if(GIT)
  set(repo ${WORK}/changes-repo)
  file(REMOVE_RECURSE ${repo})
  file(MAKE_DIRECTORY ${repo}/config)
  # Runs git in the repository; the result is in git_out.
  function(git_run)
    execute_process(
      COMMAND ${GIT} -c user.name=t -c user.email=t@example.com -c
              commit.gpgsign=false -c core.autocrlf=false ${ARGN}
      WORKING_DIRECTORY ${repo}
      RESULT_VARIABLE grc
      OUTPUT_VARIABLE gout
      ERROR_VARIABLE gerr)
    if(NOT grc EQUAL 0)
      fail("git ${ARGN}: ${grc}: ${gerr}")
    endif()
    string(STRIP "${gout}" gout)
    set(git_out
        "${gout}"
        PARENT_SCOPE)
  endfunction()
  git_run(init -q)
  file(WRITE ${repo}/config/app.json "{\"port\":1,\"s\":\"SENTINEL-A\"}")
  file(WRITE ${repo}/config/old.json "{\"k\":1}")
  file(WRITE ${repo}/config/bad.json "{\"k\":1}")
  file(WRITE ${repo}/README.md "one\n")
  git_run(add -A)
  git_run(commit -q -m first)
  git_run(rev-parse HEAD)
  set(first "${git_out}")
  file(WRITE ${repo}/config/app.json
       "{\"port\":2,\"s\":\"SENTINEL-A\",\"new\":true}")
  file(WRITE ${repo}/config/new.json "{\"k\":1}")
  file(REMOVE ${repo}/config/old.json)
  file(WRITE ${repo}/config/bad.json "{\"k\":SENTINEL}")
  file(WRITE ${repo}/README.md "two\n")
  git_run(add -A)
  git_run(commit -q -m second)
  git_run(rev-parse HEAD)
  set(second "${git_out}")

  set(chg ${WORK}/changes-test.md)
  file(REMOVE ${chg})
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env CF_DIFF=${DIFF} CF_BASE_SHA=${first}
            CF_FILES=config/** CF_MARKDOWN=${chg} ${BASH} ${ACTION}/changes.sh
    WORKING_DIRECTORY ${repo}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE text
    ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    fail("changes: expected 0, got ${rc}:\n${text}${err}")
  endif()
  file(READ ${chg} c)
  if(NOT c MATCHES "^<!-- configforge-changes -->")
    fail("changes: the marker should come first:\n${c}")
  endif()
  set(tab "\t")
  foreach(
    want
    "`config/app.json`"
    "`config/new.json`"
    "`config/old.json`"
    "`config/bad.json`"
    "changed${tab}$[\"port\"]"
    "added${tab}$[\"new\"]"
    "added${tab}$[\"k\"]"
    "removed${tab}$[\"k\"]"
    "could not be compared"
    "Keys only: no value is shown")
    string(FIND "${c}" "${want}" at)
    if(at EQUAL -1)
      fail("changes: the Markdown lacks '${want}':\n${c}")
    endif()
  endforeach()
  if(c MATCHES "README" OR c MATCHES "SENTINEL")
    fail(
      "changes: a file outside config:, or a value, is in the Markdown:\n${c}")
  endif()

  # Nothing that matches changed: said, and the file still written.
  git_run(
    commit
    -q
    --allow-empty
    -m
    third)
  git_run(rev-parse HEAD)
  set(third "${git_out}")
  foreach(case nomatch emptyfiles badbase)
    set(files README.md)
    set(base ${second})
    if(case STREQUAL "emptyfiles")
      set(files "")
    elseif(case STREQUAL "badbase")
      set(base 0123456789012345678901234567890123456789)
      set(files "config/**")
    endif()
    file(REMOVE ${chg})
    execute_process(
      COMMAND ${CMAKE_COMMAND} -E env CF_DIFF=${DIFF} CF_BASE_SHA=${base}
              CF_FILES=${files} CF_MARKDOWN=${chg} ${BASH} ${ACTION}/changes.sh
      WORKING_DIRECTORY ${repo}
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE text
      ERROR_VARIABLE err)
    if(case STREQUAL "nomatch")
      file(READ ${chg} c)
      if(NOT rc EQUAL 0 OR NOT c MATCHES "No file matching")
        fail("changes: no change should say so, got ${rc}:\n${c}")
      endif()
    elseif(NOT rc EQUAL 2)
      fail("changes, ${case}: expected 2, got ${rc}")
    elseif(case STREQUAL "badbase" AND NOT err MATCHES "fetch-depth: 0")
      fail("changes: an unknown base should give the fetch hint:\n${err}")
    endif()
  endforeach()

  # A name with a backtick or a newline cannot break the Markdown's code
  # span or heading: both are removed.
  file(WRITE "${repo}/config/a`b.json" "{\"k\":1}")
  git_run(add -A)
  git_run(commit -q -m fourth)
  file(REMOVE ${chg})
  execute_process(
    COMMAND ${CMAKE_COMMAND} -E env CF_DIFF=${DIFF} CF_BASE_SHA=${third}
            CF_FILES=config/** CF_MARKDOWN=${chg} ${BASH} ${ACTION}/changes.sh
    WORKING_DIRECTORY ${repo}
    RESULT_VARIABLE rc
    OUTPUT_QUIET ERROR_QUIET)
  file(READ ${chg} c)
  if(NOT rc EQUAL 0 OR NOT c MATCHES "`config/ab.json`")
    fail("changes: a backtick in a name should be removed:\n${c}")
  endif()
endif()

# comment.sh against a stub gh: the first push posts, a later one finds
# the marked comment and updates it, and a token that cannot write is a
# note, not a failed job.
set(stub ${WORK}/gh-stub)
file(MAKE_DIRECTORY ${stub})
file(
  WRITE ${stub}/gh
  "#!/usr/bin/env bash\n"
  "echo \"$*\" >>\"$GH_LOG\"\n"
  "if [ \"$3\" = repos/o/r/issues/7/comments ] && [ \"$1\" = api ] "
  "&& [ \"$2\" = --paginate ]; then\n"
  "  [ -n \"$GH_EXISTING\" ] && echo \"$GH_EXISTING\"\n"
  "  exit 0\n"
  "fi\n"
  "[ -z \"$GH_FAIL\" ]\n")
execute_process(COMMAND chmod +x ${stub}/gh)
set(log ${WORK}/gh.log)
foreach(case post update refused)
  file(REMOVE ${log})
  set(extra GH_EXISTING= GH_FAIL=)
  if(case STREQUAL "update")
    set(extra GH_EXISTING=4242 GH_FAIL=)
  elseif(case STREQUAL "refused")
    set(extra GH_EXISTING= GH_FAIL=1)
  endif()
  run_script(
    rc
    text
    ${ACTION}/comment.sh
    PATH=${stub}:$ENV{PATH}
    GH_LOG=${log}
    ${extra}
    CF_REPO=o/r
    CF_PR=7
    CF_MARKDOWN=${md})
  file(READ ${log} calls)
  if(NOT rc EQUAL 0)
    fail("comment, ${case}: expected 0, got ${rc}:\n${text}")
  endif()
  if(case STREQUAL "post" AND NOT calls MATCHES
                              "-X POST repos/o/r/issues/7/comments")
    fail("comment: the first push should post:\n${calls}")
  endif()
  if(case STREQUAL "update" AND NOT calls MATCHES
                                "-X PATCH repos/o/r/issues/comments/4242")
    fail("comment: a later push should update the marked comment:\n${calls}")
  endif()
  # Only a comment the action's own account wrote is its own: the marker
  # alone would let anyone's comment be taken for Doctor's.
  if(NOT calls MATCHES "user[.]login == \"github-actions[[]bot[]]\"")
    fail("comment, ${case}: the search should be for the action's own "
         "comments:\n${calls}")
  endif()
  if(case STREQUAL "refused" AND NOT text MATCHES "could not write the comment")
    fail("comment: a token that cannot write should say so:\n${text}")
  endif()
endforeach()
run_script(
  rc
  text
  ${ACTION}/comment.sh
  CF_REPO=o/r
  CF_PR=7x
  CF_MARKDOWN=${md})
if(NOT rc EQUAL 2)
  fail("a pull-request number that is not one should be refused, got ${rc}")
endif()
# Another marker finds another comment; one that is not the shape of a
# marker is refused, since it goes into a jq filter.
file(REMOVE ${log})
run_script(
  rc
  text
  ${ACTION}/comment.sh
  PATH=${stub}:$ENV{PATH}
  GH_LOG=${log}
  GH_EXISTING=
  GH_FAIL=
  "CF_MARKER=<!-- configforge-changes -->"
  CF_REPO=o/r
  CF_PR=7
  CF_MARKDOWN=${md})
file(READ ${log} calls)
if(NOT rc EQUAL 0 OR NOT calls MATCHES
                     "startswith[(]\"<!-- configforge-changes -->")
  fail(
    "comment: CF_MARKER should be the marker searched for, got ${rc}:\n${calls}"
  )
endif()
run_script(
  rc
  text
  ${ACTION}/comment.sh
  PATH=${stub}:$ENV{PATH}
  GH_LOG=${log}
  "CF_MARKER=x\; rm -rf"
  CF_REPO=o/r
  CF_PR=7
  CF_MARKDOWN=${md})
if(NOT rc EQUAL 2)
  fail("a marker that is not one should be refused, got ${rc}")
endif()
message(
  STATUS
    "doctor action: run.sh, pre-commit.sh, comment.sh and changes.sh as CI runs them"
)
