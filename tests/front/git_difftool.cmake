file(MAKE_DIRECTORY "${WORK}")
file(READ "${TOOLS_PAGE}" tools_page)
string(REGEX MATCH "\\[difftool \"configforge-json\"\\]\n[ \t]+cmd = [^\n]+"
             recipe "${tools_page}")
if(recipe STREQUAL "")
  message(FATAL_ERROR "missing configforge-json difftool recipe")
endif()

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

# Set up the scratch repository without printing fixture data on failure.
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

setup_git(init --quiet)
setup_git(config --local core.autocrlf false)
setup_git(config --local core.excludesFile "${WORK}/empty.excludes")
file(APPEND "${repo}/.git/config" "\n${recipe}\n")
set(settings "app settings.json")
file(WRITE "${repo}/${settings}" [=[{"port":8123,"tag":"old-private"}]=])
setup_git(add -- "${settings}")

# Run the documented command and check its status, output and privacy.
function(check_difftool variant content expected)
  file(WRITE "${repo}/${settings}" "${content}")
  execute_process(
    COMMAND "${GIT_EXECUTABLE}" difftool --no-prompt --trust-exit-code
            --tool=configforge-json --diff-filter=M -- "${settings}"
    WORKING_DIRECTORY "${repo}"
    RESULT_VARIABLE status
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  string(REPLACE "\r\n" "\n" output "${output}")
  set(result "git difftool (${variant}) exited with ${status}")
  if(output MATCHES "8123|9123|old-private|new-private|invalid-private"
     OR error MATCHES "8123|9123|old-private|new-private|invalid-private")
    message(FATAL_ERROR "${result} and exposed a fixture value")
  endif()
  if(variant STREQUAL "invalid")
    if(NOT status MATCHES "^[1-9][0-9]*$")
      message(FATAL_ERROR "${result}, expected a nonzero exit code")
    endif()
  elseif(NOT status STREQUAL "0")
    message(FATAL_ERROR "${result}, expected success")
  endif()
  if(NOT output STREQUAL expected)
    message(FATAL_ERROR "${result} with unexpected stdout")
  endif()
endfunction()

check_difftool(order [=[{"tag":"old-private", "port":8123}]=] "")
check_difftool(changed [=[{"port":9123,"tag":"new-private"}]=]
               "changed\t$[\"port\"]\nchanged\t$[\"tag\"]\n")
check_difftool(invalid [=[{"tag":"invalid-private",]=] "")
