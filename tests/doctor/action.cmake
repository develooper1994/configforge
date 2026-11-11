# The Doctor action's scripts and the pre-commit hook, run as a project's
# CI would run them, on the fixture reports. comment.sh talks to a stub
# gh that records what it was asked, so no forge is needed.
#
#   cmake -DDOCTOR=<doctor> -DBASH=<bash> -DACTION=<actions/doctor>
#         -DWORK=<dir with the fixtures> -P action.cmake

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
    ${CMAKE_COMMAND} -E env CONFIGFORGE_DOCTOR=${DOCTOR} ${BASH}
    ${ACTION}/check.sh --schema ${contract} cf_fx_contract_ok.json
    cf_fx_contract_typo.json
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
  COMMAND ${CMAKE_COMMAND} -E env CONFIGFORGE_DOCTOR=${DOCTOR} ${BASH}
          ${ACTION}/check.sh cf_fx_contract_ok.json
  WORKING_DIRECTORY ${WORK}
  RESULT_VARIABLE rc
  OUTPUT_QUIET ERROR_QUIET)
if(NOT rc EQUAL 2)
  fail("the contract hook without --schema should be refused, got ${rc}")
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
message(
  STATUS "doctor action: run.sh, pre-commit.sh and comment.sh as CI runs them")
