# Bare-metal Arm Cortex-M4, soft float: the library built, not run (D-157).
#
#   cmake -S . -B build-cortexm \
#     --toolchain cmake/toolchains/arm-none-eabi.cmake \
#     -DCF_PROFILE=minimal -DCMAKE_BUILD_TYPE=MinSizeRel \
#     -DCF_BUILD_TESTING=OFF -DCF_BUILD_TOOLS=OFF -DCF_BUILD_EXAMPLES=OFF
#
# In Ubuntu, gcc-arm-none-eabi and libnewlib-arm-none-eabi. The static
# library alone is built, so no program is linked and no board support
# package is needed. Soft float: a Cortex-M0 or M3 has no FPU, and the
# numbers are then the pessimistic ones.
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -mfloat-abi=soft"
                       "-ffunction-sections -fdata-sections")
string(REPLACE ";" " " CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# No operating system: the "none" platform (no files, no threads, no
# clock beyond what the caller supplies), not the POSIX one "auto" picks
# for a Generic system. The profile's own set() does not override a cache
# entry that is already there.
set(CF_PLATFORM
    none
    CACHE STRING "" FORCE)
set(CF_SINGLE_THREADED
    ON
    CACHE BOOL "" FORCE)
