# Warning configuration, per compiler rather than per platform.
#
# The list is chosen, not accumulated. Each entry has caught something
# in this codebase or guards a claim the design makes; anything that
# only produced noise was left out rather than suppressed with a
# pragma, because a warning that is routinely ignored is worse than no
# warning at all.

function(configforge_apply_warnings tgt)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    target_compile_options(${tgt} PRIVATE
      # Conformance. -pedantic is the C89 gate, and it caught a C99
      # integer literal on the first build.
      -pedantic -Werror -Wall -Wextra

      # Types and conversions.
      -Wconversion -Wsign-conversion -Wdouble-promotion
      -Wcast-qual -Wcast-align -Wbad-function-cast
      -Wshadow -Wwrite-strings -Wpointer-arith

      # Declarations. -Wundef caught a feature macro visible in one
      # translation unit and absent in another, which is how two
      # objects end up with different struct layouts.
      -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations
      -Wold-style-definition -Wnested-externs -Wredundant-decls -Wundef

      # Logic mistakes.
      -Wnull-dereference
      -Wswitch-enum -Wvla)

    # GCC-only. Handing these to clang is an error rather than a
    # warning, and clang-tidy's front end found them before a clang
    # build would have: the flags were being passed to every compiler
    # because the branch tested for GNU *or* Clang.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${tgt} PRIVATE
        -Wlogical-op -Wduplicated-cond -Wduplicated-branches
        -Wshift-overflow=2)
    endif()

    # Clang-only, and worth having: it is stricter than GCC about
    # implicit fallthrough and about comparing a value against a range
    # it cannot reach.
    if(CMAKE_C_COMPILER_ID MATCHES "Clang")
      target_compile_options(${tgt} PRIVATE
        -Wimplicit-fallthrough -Wtautological-compare
        -Wcomma -Wassign-enum)
    endif()

    # The design claims the library is usable on targets with small
    # stacks, and the exact-digit generator is the one place that could
    # quietly break that. A number in the build is a claim that stays
    # true; a sentence in a document is not.
    # AddressSanitizer inflates every frame with redzones, so the budget
    # only means something in a normal build. GCC-only: clang has no
    # equivalent, so the budget is enforced by the GCC job rather than
    # by every job.
    if(NOT CF_SANITIZE AND CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${tgt} PRIVATE -Wstack-usage=2048)
    endif()
  elseif(MSVC)
    target_compile_options(${tgt} PRIVATE /W4 /WX /wd4200)
    target_compile_definitions(${tgt} PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
endfunction()
