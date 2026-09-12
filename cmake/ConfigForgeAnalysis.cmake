# Static and dynamic analysis.
#
# Off by default because both are slow, on in CI because both find
# things that reading does not.

option(CF_ANALYZER "GCC -fanalyzer symbolic execution" OFF)
option(CF_SANITIZE "Address and undefined behaviour" OFF)
option(CF_CPPCHECK "cppcheck, when present" OFF)
option(CF_CLANG_TIDY "clang-tidy, when present" OFF)

# Apply the optional analysers to a target: -fanalyzer and sanitisers.
#
# ~~~
# configforge_apply_analysis(<target>)
# ~~~
function(configforge_apply_analysis tgt)
  if(CF_ANALYZER AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
    # Symbolic execution over the whole function: finds leaks, double
    # frees, NULL dereferences and use-after-free that no warning based
    # on syntax can see. It found a real double lookup in Doctor.
    #
    # C only. GCC documents the analyzer as supported for C; on C++ it
    # reports uninitialised values inside ordinary member functions
    # that are plainly initialised. Suppressing those findings would
    # teach us to ignore the tool, so the tool is simply not pointed at
    # a language it does not claim to handle.
    target_compile_options(${tgt} PRIVATE $<$<COMPILE_LANGUAGE:C>:-fanalyzer>)
  endif()

  if(CF_SANITIZE)
    target_compile_options(
      ${tgt} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer
                     -fno-sanitize-recover=all -g)
    target_link_options(${tgt} PRIVATE -fsanitize=address,undefined)
  endif()
endfunction()

# Route compilation through cppcheck when it is present and requested.
#
# ~~~
# configforge_setup_cppcheck()
# ~~~
function(configforge_setup_cppcheck)
  if(NOT CF_CPPCHECK)
    return()
  endif()
  find_program(CPPCHECK_EXE cppcheck)
  if(NOT CPPCHECK_EXE)
    message(STATUS "cppcheck requested but not found; skipping")
    return()
  endif()
  set(CMAKE_C_CPPCHECK
      "${CPPCHECK_EXE}"
      "--std=c89"
      "--enable=warning,style,performance,portability"
      "--inline-suppr"
      "--error-exitcode=1"
      "--suppress=missingIncludeSystem"
      "--suppress=unusedFunction"
      PARENT_SCOPE)
endfunction()
