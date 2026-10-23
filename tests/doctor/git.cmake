# The git config adapter: git's own origins in, a report out, and every
# Doctor command working on it unchanged.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# What `git config --list --show-origin --show-scope` prints, written
# here rather than taken from the machine running the test, whose git
# configuration is nobody's business and changes under it.
file(
  WRITE "${work}/git_list.txt"
  "system\tfile:/etc/gitconfig\tcore.autocrlf=input\n"
  "global\tfile:/home/u/.gitconfig\tuser.name=Someone\n"
  "global\tfile:/home/u/.gitconfig\tcore.autocrlf=false\n"
  "global\tfile:/home/u/.gitconfig\turl.https://example.com/.insteadof=ex:\n"
  "local\tfile:.git/config\tcore.bare\n"
  "local\tfile:.git/config\tcore.autocrlf=true\n"
  "command\tcommand line:\tuser.name=Override\n")

execute_process(
  COMMAND ${DOCTOR} import-git
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/git_list.txt"
  OUTPUT_FILE "${work}/git_blind.json"
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "import-git failed: ${err}")
endif()
# A key with a URL in it cannot be a report key; it is counted aloud
# rather than mangled into one that might collide with another.
if(NOT err MATCHES "1 key\\(s\\) left out")
  message(
    FATAL_ERROR "the key that could not be carried was not reported: ${err}")
endif()

execute_process(
  COMMAND ${DOCTOR} dump git_blind.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the adapter's report does not load")
endif()
# Git's precedence: the command line over local over global over system.
if(NOT out MATCHES "user.name[^\n]*\n  command [^\n]*selected")
  message(FATAL_ERROR "the command line should win user.name")
endif()
if(NOT out MATCHES "core.autocrlf[^\n]*\n  local [^\n]*selected")
  message(FATAL_ERROR "the repository should win core.autocrlf")
endif()
if(out MATCHES "Someone|Override")
  message(FATAL_ERROR "a blind report carried a value")
endif()

# The rules run on it as on any report: here the system file and the
# user's both win nothing.
execute_process(
  COMMAND ${DOCTOR} lint git_blind.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT out MATCHES "CFD010[^\n]*system")
  message(FATAL_ERROR "a scope that wins nothing was not reported")
endif()

# With --full the values are there, and nothing claims to be secret.
execute_process(
  COMMAND ${DOCTOR} import-git --full
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/git_list.txt"
  OUTPUT_FILE "${work}/git_full.json"
  ERROR_QUIET)
execute_process(
  COMMAND ${DOCTOR} explain git_full.json user.name
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out)
message("${out}")
if(NOT out MATCHES "Override" OR out MATCHES "sensitive")
  message(FATAL_ERROR "a full import should carry values and no sensitivity")
endif()

# Output it was not written for is refused, not guessed at.
file(WRITE "${work}/git_bad.txt" "user.name=no scope or origin\n")
execute_process(
  COMMAND ${DOCTOR} import-git
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/git_bad.txt"
  OUTPUT_QUIET
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 2 OR NOT err MATCHES "stdin:1")
  message(FATAL_ERROR "unrecognised input should be refused with its line")
endif()

message("doctor git adapter: clean")
