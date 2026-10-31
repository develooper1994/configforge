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
#
# Warnings are errors, except where a compiler warns about the C
# library's own headers and offers no way to say they are system
# headers: the Portable C Compiler warns at glibc's __cold__ attribute,
# in stdio.h, on every file. That build turns CF_WERROR off and still
# shows every warning; every other build keeps it on.
option(CF_WERROR "Treat warnings as errors" ON)
if(CF_WERROR)
  set(CF_W_CONFORMANCE -pedantic -Werror -Wall -Wextra)
else()
  set(CF_W_CONFORMANCE -pedantic -Wall -Wextra)
endif()

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
    # /W4 alone is weaker than the set above, so the classes that have
    # an MSVC counterpart are turned on by number. It is still not
    # parity, and D-32 says which classes have no counterpart at all - a
    # project that claims one standard and enforces two has the weaker
    # one, and the honest thing is to name the gap rather than let /W4
    # imply there isn't one.
    #
    # Built and tested with this set on every verify of a Windows
    # checkout, and by the windows-latest CI job.
    target_compile_options(
      ${tgt}
      PRIVATE /W4
              $<$<BOOL:${CF_WERROR}>:/WX>
              /permissive- # conformance, the closest thing to -pedantic
              /wd4200 # zero-sized array, used deliberately
              # Conversions, the class C89 loses bits in.
              /w14242 # conversion, possible loss of data
              /w14254 # bit-field conversion, possible loss
              /w14287 # unsigned/negative constant mismatch
              /we4296 # expression is always true/false
              /w14311 # pointer truncation
              /w14826 # sign-extending conversion
              # Declarations and lifetime.
              /w14263 # member function does not override
              /w14265 # class has virtual functions, non-virtual dtor
              /w14296 # comparison always true
              /w14545 # expression before comma has no effect
              /w14546 # call before comma missing argument list
              /w14547 # operator before comma has no effect
              /w14549 # operator before comma has no effect
              /w14555 # expression has no effect
              /w14619 # unknown #pragma warning number
              /w14640 # thread-unsafe static initialisation
              /w14905 # wide string literal cast
              /w14906 # string literal cast
              /w14928 # illegal copy-initialisation
    )
    target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)

    # MSVC has no C89 mode: /std:c11 is the oldest it offers and its
    # default is C89 with extensions. CMake's C_STANDARD 90 is a no-op
    # here, so the C89 conformance claim rests on the GCC and Clang
    # jobs. Said out loud because a silent no-op is how a standard stops
    # being enforced without anyone deciding to stop enforcing it.
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

  # The library's frames are measured against the builder's budget
  # (configforge_stack_budget, D-37). The project's own code - tests,
  # Doctor, the examples - is held to the budget it was written for,
  # 2048 bytes, at the limits it was written for; with other limits its
  # frames grow with them, and it is not what ships.
  #
  # AddressSanitizer inflates every frame with redzones, so no budget
  # means anything under the sanitiser.
  if(tgt STREQUAL "configforge")
    configforge_stack_budget(${tgt})
  elseif(NOT CF_INSTRUMENTED AND CF_DEFAULT_LIMITS)
    check_c_compiler_flag("-Wstack-usage=2048" CF_HAS_STACK_USAGE)
    if(CF_HAS_STACK_USAGE)
      target_compile_options(${tgt} PRIVATE -Wstack-usage=2048)
    endif()
  endif()
endfunction()

# Measure every frame of the library and check it against
# CF_STACK_BUDGET once the library links, with scripts/stack.cmake.
#
# Measured after the build rather than refused during it (-Wstack-usage)
# because the answer a builder needs is a table, not the first function
# that failed: which frames are over, which feature each belongs to, and
# so what to turn off or lower. The table is written to
# configforge-stack.txt either way, so the cost of the chosen limits can
# be read without breaking anything.
#
# ~~~
# configforge_stack_budget(<target>)
# ~~~
function(configforge_stack_budget tgt)
  if(CF_INSTRUMENTED)
    message(STATUS "stack: not measured under the sanitiser")
    return()
  endif()
  # Asked of GCC and Clang only: the Tiny C Compiler accepts any flag it
  # does not know without a word, so the check passes there and nothing
  # is ever measured.
  check_c_compiler_flag("-fstack-usage" CF_HAS_FSTACK_USAGE)
  if(NOT CF_HAS_FSTACK_USAGE OR NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    message(STATUS "stack: this compiler reports no frame sizes; "
                   "CF_STACK_BUDGET is not checked here")
    return()
  endif()
  if(CF_STACK_BUDGET EQUAL 0)
    message(WARNING "CF_STACK_BUDGET=0: the library's frames are measured "
                    "and reported, not checked")
  endif()
  target_compile_options(${tgt} PRIVATE -fstack-usage)
  add_custom_command(
    TARGET ${tgt}
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND}
      -DOBJDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${tgt}.dir
      -DBUDGET=${CF_STACK_BUDGET}
      -DOUT=${CMAKE_BINARY_DIR}/configforge-stack.txt
      -DLIB=$<TARGET_FILE:${tgt}> -P ${CMAKE_SOURCE_DIR}/scripts/stack.cmake
    VERBATIM)
endfunction()

# Hardening for what ships: the library and Doctor (docs/Safety.md).
# What the compiler and the linker can add at no cost to the code - a
# canary on each frame that holds an array, probes so a large frame
# cannot step over the guard page, checked libc calls where the size is
# known, control-flow protection where the CPU has it, and a relocation
# table made read-only before main. Each flag is asked of the compiler
# first: TinyCC and pcc take none of them, and get none.
option(CF_HARDEN
       "Stack protector, fortified libc calls, RELRO on shipped targets" ON)

# Apply the hardening flags the compiler has to a shipped target.
#
# ~~~
# configforge_apply_hardening(<target>)
# ~~~
function(configforge_apply_hardening tgt)
  if(NOT CF_HARDEN)
    return()
  endif()
  if(MSVC)
    target_compile_options(${tgt} PRIVATE /guard:cf)
    get_target_property(type ${tgt} TYPE)
    if(type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "EXECUTABLE")
      target_link_options(${tgt} PRIVATE /guard:cf)
    endif()
    return()
  endif()
  if(NOT CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    return()
  endif()
  check_c_compiler_flag(-fstack-protector-strong CF_HAVE_STACK_PROTECTOR)
  check_c_compiler_flag(-fstack-clash-protection CF_HAVE_STACK_CLASH)
  check_c_compiler_flag(-fcf-protection=full CF_HAVE_CF_PROTECTION)
  if(CF_HAVE_STACK_PROTECTOR)
    target_compile_options(${tgt} PRIVATE -fstack-protector-strong)
  endif()
  if(CF_HAVE_STACK_CLASH)
    target_compile_options(${tgt} PRIVATE -fstack-clash-protection)
  endif()
  if(CF_HAVE_CF_PROTECTION)
    target_compile_options(${tgt} PRIVATE -fcf-protection=full)
  endif()
  # Fortified calls need the optimiser to know sizes, so only where it
  # runs; and a toolchain that already sets a level is overruled, not
  # warned at. Not under the sanitisers, whose own checks are stronger
  # and which replace the same calls.
  if(NOT CF_SANITIZE AND NOT CF_SANITIZE_THREAD)
    target_compile_options(
      ${tgt} PRIVATE $<$<NOT:$<CONFIG:Debug>>:-U_FORTIFY_SOURCE>
                     $<$<NOT:$<CONFIG:Debug>>:-D_FORTIFY_SOURCE=3>)
  endif()
  if(UNIX AND NOT APPLE)
    get_target_property(type ${tgt} TYPE)
    if(type STREQUAL "SHARED_LIBRARY" OR type STREQUAL "EXECUTABLE")
      target_link_options(${tgt} PRIVATE -Wl,-z,relro -Wl,-z,now)
    endif()
  endif()
endfunction()
