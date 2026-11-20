# Generators outside the tree (D-110): `configforge gen --lang X`, X no entry of
# the table, runs the program configforge-gen-X found beside configforge-gen or
# on the PATH, with the contract as JSON on its standard input. Run with
# -DGEN=<configforge-gen> -DEXT=<the directory of the fake generators>
# -DEXE=<the executable suffix> -DSCHEMA=<contract.schema.json>
# -DGOLDEN=<golden_contract.json> -DWORK=<a directory to write into> -P.
#
# Bytes are compared as files (OUTPUT_FILE and compare_files), never read back
# through a variable; each case says what it expects of the exit code and of
# standard error, which a test that only checked the exit code would not.

if(CMAKE_HOST_WIN32)
  set(sep ";")
else()
  set(sep ":")
endif()
set(old_path "$ENV{PATH}")
set(with_ext "${EXT}${sep}${old_path}")

file(REMOVE_RECURSE "${WORK}")
file(MAKE_DIRECTORY "${WORK}")

# Runs the program with the PATH given; sets rc and err in the caller.
# gen(<path> <stdout file or ""> <arg>...)
function(gen path outfile)
  set(ENV{PATH} "${path}")
  if(outfile STREQUAL "")
    execute_process(
      COMMAND "${GEN}" ${ARGN}
      WORKING_DIRECTORY "${WORK}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE err)
  else()
    execute_process(
      COMMAND "${GEN}" ${ARGN}
      WORKING_DIRECTORY "${WORK}"
      RESULT_VARIABLE rc
      OUTPUT_FILE "${outfile}"
      ERROR_VARIABLE err)
  endif()
  set(rc
      "${rc}"
      PARENT_SCOPE)
  set(err
      "${err}"
      PARENT_SCOPE)
endfunction()

function(expect_exit case want)
  if(NOT rc EQUAL want)
    message(FATAL_ERROR "${case}: expected exit ${want}, got ${rc}\n${err}")
  endif()
endfunction()

function(expect_err case regex)
  if(NOT err MATCHES "${regex}")
    message(FATAL_ERROR "${case}: stderr does not match '${regex}':\n${err}")
  endif()
endfunction()

function(expect_same case a b)
  execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${a}" "${b}"
                  RESULT_VARIABLE differs)
  if(NOT differs EQUAL 0)
    message(FATAL_ERROR "${case}: ${a} and ${b} differ")
  endif()
endfunction()

# 1. contract_json
gen("${old_path}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    contract-json
    --out
    m.json)
expect_exit(contract_json 0)
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${GOLDEN}"
                        "${WORK}/m.json" RESULT_VARIABLE differs)
if(NOT differs EQUAL 0)
  message(
    FATAL_ERROR
      "the contract JSON differs from tests/gen/golden_contract.json: "
      "regenerate it with configforge gen --schema tests/doctor/contract.schema.json "
      "--lang contract-json --out tests/gen/golden_contract.json, and read the diff"
  )
endif()

# 2. stdout: the generator was handed the bytes the built-in writes.
gen("${with_ext}"
    "${WORK}/stdout.json"
    --schema
    "${SCHEMA}"
    --lang
    fake)
expect_exit(stdout 0)
expect_same(stdout "${GOLDEN}" "${WORK}/stdout.json")
if(NOT err STREQUAL "fake: args=\n")
  message(FATAL_ERROR "stdout: stderr is '${err}', not 'fake: args='")
endif()

# 3. args: the others pass, in order, unchanged; "name" is the only line that
# differs from the golden file.
gen("${with_ext}"
    ""
    --lang
    fake
    --name
    Thing
    --schema
    "${SCHEMA}"
    --out
    o.txt)
expect_exit(args 0)
if(NOT err STREQUAL "fake: args=--name|Thing|--out|o.txt\n")
  message(FATAL_ERROR "args: stderr is '${err}'")
endif()
# What it was handed is what the built-in writes for that name, which case 1
# holds to the golden file but for the "name" line: no byte is read back
# through a variable, where a Windows host would change the line ends.
gen("${old_path}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    contract-json
    --name
    Thing
    --out
    named.json)
expect_exit(args 0)
expect_same(args "${WORK}/named.json" "${WORK}/o.txt")
execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files "${GOLDEN}"
                        "${WORK}/named.json" RESULT_VARIABLE same_as_golden)
if(same_as_golden EQUAL 0)
  message(FATAL_ERROR "args: the name did not reach the contract's \"name\"")
endif()

# 4. exit: the generator's code passes through.
gen("${with_ext}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    fake
    --name
    exit7)
expect_exit(exit 7)
expect_err(exit "fake: exit 7 asked for")

# 5. missing
gen("${with_ext}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    nosuch)
expect_exit(missing 2)
expect_err(
  missing
  "gen: no language 'nosuch'; there are: c, cpp, rust, python, contract-json; no configforge-gen-nosuch in .+ or on the PATH"
)

# 6. name_rule
foreach(bad "../x" "Fake" "-x" "a/b")
  gen("${with_ext}"
      ""
      --schema
      "${SCHEMA}"
      --lang
      "${bad}")
  expect_exit("name_rule ${bad}" 2)
  expect_err("name_rule ${bad}" "is not a language name")
endforeach()

# 7. builtin_wins: ext/ has a configforge-gen-c, which must never run.
gen("${with_ext}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    c
    --out
    w.c)
expect_exit(builtin_wins 0)
file(READ "${WORK}/w.c" wc)
if(NOT wc MATCHES "cf_builder_define")
  message(FATAL_ERROR "builtin_wins: w.c is not the C writer's file")
endif()

# 8. bad_schema: nothing was started.
file(MAKE_DIRECTORY "${WORK}/w")
gen("${with_ext}"
    ""
    --schema
    missing.json
    --lang
    fake
    --name
    ran
    --out
    w/o.txt)
expect_exit(bad_schema 2)
expect_err(bad_schema "gen: ")
if(EXISTS "${WORK}/w/ran.marker")
  message(FATAL_ERROR "bad_schema: the generator was started")
endif()

# 9. closed_input: far past a pipe's buffer, to a generator that left.
set(big "${WORK}/big.schema.json")
file(WRITE "${big}" "{\"type\": \"object\", \"properties\": {\n")
foreach(i RANGE 0 2999)
  if(i LESS 2999)
    set(comma ",")
  else()
    set(comma "")
  endif()
  file(APPEND "${big}" "\"k${i}\": {\"type\": \"integer\"}${comma}\n")
endforeach()
file(APPEND "${big}" "}}\n")
gen("${with_ext}"
    ""
    --schema
    "${big}"
    --lang
    fake
    --name
    noread)
expect_exit(closed_input 3)
if(err MATCHES "gen: ")
  message(FATAL_ERROR "closed_input: gen reported a failure:\n${err}")
endif()

# 10. beside: the copy of configforge-gen finds the generator beside itself.
set(GEN "${EXT}/configforge-gen${EXE}")
gen("${old_path}"
    ""
    --schema
    "${SCHEMA}"
    --lang
    fake
    --name
    ran
    --out
    w/o2.txt)
expect_exit(beside 0)
if(NOT EXISTS "${WORK}/w/ran.marker")
  message(FATAL_ERROR "beside: the generator beside the program was not run")
endif()

# 11. not_a_directory: a directory is not a program.
if(NOT CMAKE_HOST_WIN32)
  file(MAKE_DIRECTORY "${EXT}/configforge-gen-dirlang")
  gen("${with_ext}"
      ""
      --schema
      "${SCHEMA}"
      --lang
      dirlang)
  expect_exit(not_a_directory 2)
  expect_err(not_a_directory "gen: no language 'dirlang'")
endif()
