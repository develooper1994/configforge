# `configforge gen schema --from`: a contract drafted from a sample file
# (D-96). Run with -DGEN=<configforge-gen> -DWORK=<a directory of its own>,
# and -DTOML=ON|OFF -DINI=ON|OFF for the formats this build has: a case
# that needs a format the build left out is not run.

file(REMOVE_RECURSE ${WORK})
file(MAKE_DIRECTORY ${WORK})

# Runs configforge-gen in WORK and leaves its standard output in sc_out and
# its standard error in sc_err. The exit code is checked as well as the
# text: a tool that printed nothing would pass a check on the text alone.
function(sc_run name expect)
  execute_process(
    COMMAND ${GEN} ${ARGN}
    WORKING_DIRECTORY ${WORK}
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT rc EQUAL expect)
    message(FATAL_ERROR "${name}: expected exit ${expect}, got ${rc}\n${err}")
  endif()
  set(sc_out
      "${out}"
      PARENT_SCOPE)
  set(sc_err
      "${err}"
      PARENT_SCOPE)
endfunction()

function(sc_has name what text regex)
  if(NOT "${text}" MATCHES "${regex}")
    message(FATAL_ERROR "${name}: ${what} lacks '${regex}':\n${text}")
  endif()
endfunction()

function(sc_lacks name what text regex)
  if("${text}" MATCHES "${regex}")
    message(FATAL_ERROR "${name}: ${what} holds '${regex}':\n${text}")
  endif()
endfunction()

if(TOML)
  file(
    WRITE ${WORK}/sample.toml
    "debug = true\nratio = 0.5\ntags = [\"a\", \"b\"]\n[server]\nport = 8080\nhost = \"web\"\n"
  )

  # toml: every value a key, its type from the value, nothing else.
  sc_run(toml 0 schema --from sample.toml --out s.json)
  file(READ ${WORK}/s.json schema)
  sc_has(toml "the schema" "${schema}"
         "\"port\": \\{ \"type\": \"integer\", \"description\": \"\" \\}")
  sc_has(toml "the schema" "${schema}"
         "\"host\": \\{ \"type\": \"string\", \"description\": \"\" \\}")
  sc_has(toml "the schema" "${schema}" "\"boolean\"")
  sc_has(toml "the schema" "${schema}" "\"number\"")
  sc_has(toml "the schema" "${schema}" "\"array\"")
  sc_has(toml "the schema" "${schema}" "\"title\": \"sample\"")
  sc_lacks(toml "the schema" "${schema}" "\"default\"")
  sc_lacks(toml "the schema" "${schema}" "\"required\"")
  sc_has(toml "stderr" "${sc_err}" "a draft from sample.toml: 5 key\\(s\\)")

  # holds: the sample holds to its own draft, and a misspelt key does not.
  sc_run(holds 0 check --schema s.json sample.toml)
  file(WRITE ${WORK}/typo.toml "[server]\nprot = 1\n")
  sc_run(typo 1 check --schema s.json typo.toml)

  # gen: the draft is a contract the C writer can write.
  sc_run(gen 0 --schema s.json --out s.c)

  # stdout, a title of its own, and the format named for a document with no name.
  sc_run(stdout 0 schema --from sample.toml --title mine)
  sc_has(stdout "standard output" "${sc_out}" "\"title\": \"mine\"")
  execute_process(
    COMMAND ${GEN} schema --from - --as toml
    WORKING_DIRECTORY ${WORK}
    INPUT_FILE ${WORK}/sample.toml
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE err)
  if(NOT rc EQUAL 0)
    message(FATAL_ERROR "stdin: expected exit 0, got ${rc}\n${err}")
  endif()
  sc_has(stdin "standard output" "${out}" "\"title\": \"stdin\"")
  sc_has(stdin "stderr" "${err}" "a draft from -: 5 key\\(s\\)")
endif()

if(INI)
  # ini: text with no types, typed by what it says (D-24).
  file(WRITE ${WORK}/sample.ini "[server]\nport = 8080\nname = web\n")
  sc_run(ini 0 schema --from sample.ini)
  sc_has(ini "the schema" "${sc_out}" "\"port\": \\{ \"type\": \"integer\"")
  sc_has(ini "the schema" "${sc_out}" "\"name\": \\{ \"type\": \"string\"")
endif()

# left_out: what a contract has no type for is named, and the rest drafted.
file(
  WRITE ${WORK}/odd.json
  "{\"n\": null, \"rows\": [{\"a\": 1}], \"mix\": [1, \"a\"], \"none\": [], \"k\": 1}")
sc_run(left_out 0 schema --from odd.json --out odd.schema.json)
foreach(
  line
  "n: a null, whose type the sample does not show; left out"
  "rows: an array of tables, which a contract has no type for; left out"
  "mix: an array of mixed types, which a contract has no type for; left out"
  "none: an empty array, whose element type the sample does not show; left out"
  "a draft from odd.json: 1 key\\(s\\)")
  sc_has(left_out "stderr" "${sc_err}" "${line}")
endforeach()
file(READ ${WORK}/odd.schema.json odd)
sc_has(left_out "the schema" "${odd}" "\"k\"")
foreach(key n rows mix none)
  sc_lacks(left_out "the schema" "${odd}" "\"${key}\":")
endforeach()

# Unusable input, each exit 2.
sc_run(no_from 2 schema)
sc_run(missing 2 schema --from missing.toml)
sc_has(missing "stderr" "${sc_err}" "gen: cannot read missing.toml")
sc_run(no_format 2 schema --from odd.json --as nosuch)
sc_has(no_format "stderr" "${sc_err}" "no format called 'nosuch'")
