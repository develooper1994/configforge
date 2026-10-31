# Every public header must compile standalone, as C89, with nothing
# included before it. A header that only works because some other
# header happened to come first is a header that breaks for the first
# user who includes it alone.
file(GLOB headers "${SRC}/include/configforge/*.h")
set(bad 0)

# This build's cf_build_options.h ahead of the source tree's defaults, as
# an installed copy would be: a single-threaded build's headers say so
# there, and the source tree's defaults would say otherwise.
set(gen_inc "")
if(EXISTS "${BIN}/generated/configforge/cf_build_options.h")
  set(gen_inc "-I${BIN}/generated")
endif()
# As C89, and as each later standard the compiler knows: what a header
# adds for C11 and C23 (D-53) is code too, and compiles nowhere unless
# something compiles it.
set(c_stds c89)
foreach(std c11 c2x)
  file(WRITE "${BIN}/probe_std_${std}.c" "int main(void){return 0;}\n")
  execute_process(
    COMMAND ${CC} -std=${std} -c -o "${BIN}/probe_std_${std}.o"
            "${BIN}/probe_std_${std}.c"
    RESULT_VARIABLE rc
    OUTPUT_QUIET ERROR_QUIET)
  if(rc EQUAL 0)
    list(APPEND c_stds ${std})
  else()
    message("header gate: C headers as ${std} skipped, not supported here")
  endif()
endforeach()
foreach(std ${c_stds})
  foreach(h ${headers})
    get_filename_component(basename "${h}" NAME)
    set(probe "${BIN}/probe_${std}_${basename}.c")
    file(WRITE "${probe}"
         "#include <configforge/${basename}>\nint main(void){return 0;}\n")
    execute_process(
      COMMAND ${CC} -std=${std} -pedantic -Werror ${gen_inc} -I${SRC}/include -c
              -o "${BIN}/probe_${std}_${basename}.o" "${probe}"
      RESULT_VARIABLE rc
      OUTPUT_VARIABLE out
      ERROR_VARIABLE out)
    if(NOT rc EQUAL 0)
      message("VIOLATION: ${basename} does not compile standalone as ${std}")
      message("${out}")
      set(bad 1)
    endif()
  endforeach()
endforeach()
if(bad)
  message(FATAL_ERROR "header gate failed")
endif()
message("header gate: clean")

# The C++ wrapper, standalone under every standard from C++98 to C++23
# that the compiler knows: one header, whose C++11, C++17 and C++20
# parts turn on with the standard (D-47). A claim the build does not
# check is a claim that decays - the wrapper was checked only as C++98
# for years and did not compile as C++11 on LP64. A standard the
# compiler does not know is skipped and said.
function(probe_cxx header std prereq)
  set(base "${BIN}/probe_${std}_${header}")
  file(WRITE "${base}.pre.cpp" "${prereq}\nint main(){return 0;}\n")
  execute_process(
    COMMAND ${CXX} -std=c++${std} -c -o "${base}.pre.o" "${base}.pre.cpp"
    RESULT_VARIABLE rc
    OUTPUT_QUIET ERROR_QUIET)
  if(NOT rc EQUAL 0)
    message("header gate: ${header} as C++${std} skipped, not supported here")
    return()
  endif()
  file(WRITE "${base}.cpp"
       "#include <configforge/${header}>\nint main(){return 0;}\n")
  execute_process(
    COMMAND ${CXX} -std=c++${std} -pedantic -Werror -Wall -Wextra ${gen_inc}
            -I${SRC}/include -c -o "${base}.o" "${base}.cpp"
    RESULT_VARIABLE rc
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
  if(NOT rc EQUAL 0)
    message("VIOLATION: ${header} does not compile standalone as C++${std}")
    message("${out}")
    set(cxx_bad
        1
        PARENT_SCOPE)
  endif()
endfunction()

if(CXX AND EXISTS "${SRC}/include/configforge/configforge.hpp")
  set(cxx_bad 0)
  foreach(
    std
    98
    11
    14
    17
    20
    23)
    probe_cxx(configforge.hpp ${std} "")
  endforeach()
  if(cxx_bad)
    message(FATAL_ERROR "header gate failed")
  endif()
  message("header gate: C++ clean")
endif()
