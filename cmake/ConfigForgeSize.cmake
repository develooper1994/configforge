# What the compiler and the linker can take away, for targets where the
# size of a program is the budget (the minimal preset), and what they can
# do across files for one that wants speed (CF_LTO).
#
# CF_GC_SECTIONS puts each function and each object in a section of its
# own, so the linker drops the ones no program reaches: a program that
# reads one INI file does not carry the TOML reader. Measured on the
# minimal preset (2026-11-05, GCC 13, x86-64), see docs/Maintaining.md.
#
# CF_LTO is link-time optimisation (CMake's INTERPROCEDURAL_OPTIMIZATION)
# of the library and what links it, where the toolchain has it. Off by
# default: it inlines across files, so the stack budget, which reads the
# compiler's per-file frame sizes, no longer measures the frames that
# run - the build says so rather than claim a figure it did not take.

option(CF_GC_SECTIONS "Drop the functions and data no program reaches" OFF)
option(CF_LTO "Link-time optimisation where the toolchain has it" OFF)

if(CF_GC_SECTIONS)
  if(MSVC)
    string(APPEND CMAKE_C_FLAGS " /Gy /Gw")
    string(APPEND CMAKE_CXX_FLAGS " /Gy /Gw")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " /OPT:REF /OPT:ICF")
    string(APPEND CMAKE_SHARED_LINKER_FLAGS " /OPT:REF /OPT:ICF")
  elseif(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    string(APPEND CMAKE_C_FLAGS " -ffunction-sections -fdata-sections")
    string(APPEND CMAKE_CXX_FLAGS " -ffunction-sections -fdata-sections")
    if(APPLE)
      string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,-dead_strip")
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,-dead_strip")
    else()
      string(APPEND CMAKE_EXE_LINKER_FLAGS " -Wl,--gc-sections")
      string(APPEND CMAKE_SHARED_LINKER_FLAGS " -Wl,--gc-sections")
    endif()
  else()
    message(STATUS "CF_GC_SECTIONS: ${CMAKE_C_COMPILER_ID} has no flag "
                   "for it here; nothing is dropped")
  endif()
endif()

if(CF_LTO)
  include(CheckIPOSupported)
  check_ipo_supported(
    RESULT CF_HAS_IPO
    OUTPUT CF_IPO_WHY
    LANGUAGES C)
  if(CF_HAS_IPO)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
    message(STATUS "CF_LTO: on; the stack budget is not checked under it")
  else()
    message(STATUS "CF_LTO: this toolchain cannot (${CF_IPO_WHY})")
  endif()
endif()
