# The warning standard, decision D-32.
#
# Flags are grouped by what they protect rather than by which compiler
# has them, and every flag is put to the compiler at configure time
# instead of being looked up in a table this file would have to keep
# true. A table is wrong the moment a compiler adds or drops a flag and
# nothing says so; the compiler is never wrong about itself.

include(CheckCCompilerFlag)

# Conformance. -pedantic is the C89 gate and it caught a C99 integer
# literal on the very first build.
set(CF_W_CONFORMANCE -pedantic -Werror -Wall -Wextra)

# Conversions, where a C89 codebase quietly loses bits.
set(CF_W_CONVERSION
    -Wconversion
    -Wsign-conversion
    -Wdouble-promotion
    -Wcast-qual
    -Wcast-align
    -Wbad-function-cast)

# Shadowing and aliasing.
set(CF_W_ALIASING -Wshadow -Wwrite-strings -Wpointer-arith)

# Declarations. -Wundef caught a feature macro visible in one
# translation unit and absent in another, which is how two objects end
# up with different struct layouts.
set(CF_W_DECLARATIONS
    -Wstrict-prototypes
    -Wmissing-prototypes
    -Wmissing-declarations
    -Wold-style-definition
    -Wnested-externs
    -Wredundant-decls
    -Wundef)

# Logic mistakes.
set(CF_W_LOGIC -Wnull-dereference -Wswitch-enum -Wvla -Wformat=2)

# Offered by one compiler only. Listed here rather than guarded by a
# compiler test: the flag check below settles it, so a compiler that
# gains one of these later picks it up with no edit here.
set(CF_W_EXTRA
    -Wlogical-op
    -Wduplicated-cond
    -Wduplicated-branches
    -Wshift-overflow=2
    -Wcomma
    -Wassign-enum
    -Wconditional-uninitialized
    -Wimplicit-fallthrough
    -Wtautological-compare)

# Filter warning flags down to the ones this compiler accepts.
#
# ~~~
# configforge_supported_flags(<out-var> <flag>...)
# ~~~
function(configforge_supported_flags out)
  set(accepted "")
  set(skipped "")
  foreach(flag ${ARGN})
    string(MAKE_C_IDENTIFIER "CF_HAS${flag}" var)
    # -Werror during the check, because clang only *warns* about an
    # unknown -W flag: without this the check reports success exactly
    # where the answer matters.
    set(CMAKE_REQUIRED_FLAGS "-Werror")
    check_c_compiler_flag("${flag}" ${var})
    if(${var})
      list(APPEND accepted "${flag}")
    else()
      list(APPEND skipped "${flag}")
    endif()
  endforeach()
  set(${out}
      "${accepted}"
      PARENT_SCOPE)
  set(CF_SKIPPED_FLAGS
      "${skipped}"
      PARENT_SCOPE)
endfunction()

# Apply the project warning standard to a target. See D-32.
#
# ~~~
# configforge_apply_warnings(<target>)
# ~~~
function(configforge_apply_warnings tgt)
  if(MSVC)
    # Not equivalent to the set above, and saying so is better than a
    # comment implying parity. A project that claims one standard and
    # enforces two has the weaker one.
    target_compile_options(${tgt} PRIVATE /W4 /WX /wd4200)
    target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
    return()
  endif()

  if(NOT DEFINED _CF_WARNING_FLAGS)
    configforge_supported_flags(
      flags
      ${CF_W_CONFORMANCE}
      ${CF_W_CONVERSION}
      ${CF_W_ALIASING}
      ${CF_W_DECLARATIONS}
      ${CF_W_LOGIC}
      ${CF_W_EXTRA})
    set(_CF_WARNING_FLAGS
        "${flags}"
        CACHE INTERNAL "the warning set this compiler accepted")
    message(STATUS "warnings: ${_CF_WARNING_FLAGS}")
    if(CF_SKIPPED_FLAGS)
      message(STATUS "warnings unsupported here: ${CF_SKIPPED_FLAGS}")
    endif()
  endif()

  target_compile_options(${tgt} PRIVATE ${_CF_WARNING_FLAGS})

  # The design claims the library is usable where a few kilobytes of
  # stack is the whole budget, and the exact-digit generator is the one
  # place that could quietly break that. A number in the build is a
  # claim that stays true; a sentence in a document is not.
  #
  # AddressSanitizer inflates every frame with redzones, so the budget
  # means nothing under the sanitiser.
  if(NOT CF_SANITIZE)
    check_c_compiler_flag("-Wstack-usage=2048" CF_HAS_STACK_USAGE)
    if(CF_HAS_STACK_USAGE)
      target_compile_options(${tgt} PRIVATE -Wstack-usage=2048)
    endif()
  endif()
endfunction()
