# A generated file against the one checked in (D-95): what `configforge gen`
# writes for a schema in a language must be the golden file byte for byte, so
# that a change to the writer shows in a diff of the file. Run with
# -DGEN=<configforge-gen> -DSCHEMA=<schema> -DLANG=<language>
# -DGOLDEN=<the checked-in file> -DOUT=<where to write the generated one>.

execute_process(
  COMMAND ${GEN} --schema ${SCHEMA} --lang ${LANG} --out ${OUT}
  RESULT_VARIABLE rc
  ERROR_VARIABLE err)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "configforge gen --lang ${LANG} exited ${rc}\n${err}")
endif()

execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${GOLDEN} ${OUT}
                RESULT_VARIABLE differs)
if(NOT differs EQUAL 0)
  file(RELATIVE_PATH golden ${CMAKE_CURRENT_LIST_DIR}/../.. ${GOLDEN})
  file(RELATIVE_PATH schema ${CMAKE_CURRENT_LIST_DIR}/../.. ${SCHEMA})
  message(
    FATAL_ERROR
      "the generated ${LANG} differs from ${golden}: regenerate it with "
      "configforge gen --schema ${schema} --lang ${LANG} --out ${golden}, "
      "and read the diff")
endif()
