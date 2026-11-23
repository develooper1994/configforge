cmake_minimum_required(VERSION 3.10)

set(data "${CMAKE_CURRENT_LIST_DIR}/data")
function(check_sample sample expected_result expected_text)
  execute_process(
    COMMAND
      "${CMAKE_COMMAND}" -DCHECK=ON "-DFILE=${data}/sizes_base.txt"
      "-DMEASURED=${data}/sizes_${sample}.txt" -P
      "${SRC}/scripts/check/sizes.cmake"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE error)
  if(NOT result EQUAL expected_result OR NOT "${output}${error}" MATCHES
                                         "${expected_text}")
    message(FATAL_ERROR "${sample}: exit ${result}: ${output}${error}")
  endif()
endfunction()

check_sample(two 0 "")
check_sample(ten 1 "CF_WITH_INI[^\n]*100[^\n]*110")
check_sample(compiler 0 "not compared")
