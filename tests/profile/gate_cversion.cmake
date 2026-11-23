cmake_minimum_required(VERSION 3.10)

set(data "${CMAKE_CURRENT_LIST_DIR}/data")
function(
  check
  name
  db
  declarations
  c89_only
  expected
  pattern)
  # Real compile databases name files by absolute path.
  file(READ "${data}/${db}_db.json" contents)
  string(REPLACE "\"file\": \"lib/" "\"file\": \"${SRC}/lib/" contents
                 "${contents}")
  set(compdb "${WORK}/${name}_db.json")
  file(WRITE "${compdb}" "${contents}")
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" "-DSRC=${SRC}" "-DLIST=${declarations}"
      "-DCOMPDB=${compdb}" "-DC89_ONLY=${c89_only}" -P
      "${SRC}/cmake/gates/GateCVersion.cmake"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result STREQUAL "${expected}" OR NOT "${output}${error}" MATCHES
                                          "${pattern}")
    message(FATAL_ERROR "${name}: expected exit ${expected} and /${pattern}/; "
                        "got ${result}\nstdout:\n${output}\nstderr:\n${error}")
  endif()
endfunction()

set(ok "${data}/ok_list.txt")
set(empty "${data}/empty_list.txt")
check(
  ok
  ok
  "${ok}"
  OFF
  0
  "clean,[ \n]+1[ \n]+file")
check(
  undeclared
  ok
  "${empty}"
  OFF
  1
  "is[ \n]+not[ \n]+declared")
check(
  wrong_std
  ok
  "${data}/c99_list.txt"
  OFF
  1
  "declared[ \n]+as[ \n]+c99[ \n]+but[ \n]+compiled[ \n]+with[ \n]+-std=c11")
check(
  stale
  ok
  "${data}/stale_list.txt"
  OFF
  1
  "is[ \n]+declared[ \n]+in[ \n]+cf_cversion.txt[ \n]+and[ \n]+is[ \n]+never[ \n]+compiled[ \n]+above[ \n]+C89"
)
file(WRITE "${WORK}/short_list.txt" "lib/serialize/src/yaml.c|c11|short\n")
check(
  short
  ok
  "${WORK}/short_list.txt"
  OFF
  1
  "reason[ \n]+of[ \n]+5[ \n]+characters")
check(
  refused
  ok
  "${ok}"
  ON
  1
  "the[ \n]+profile[ \n]+allows[ \n]+C89[ \n]+only")
check(
  extensions
  gnu
  "${empty}"
  OFF
  1
  "compiled[ \n]+with[ \n]+-std=gnu90")
