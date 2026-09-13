# Compiler and standard detection may appear only in cf_port.h and
# port.c. The value of a portability layer is that it is the only place
# with conditional compilation in it.
file(
  GLOB_RECURSE
  files
  "${SRC}/src/*.c"
  "${SRC}/src/*.h"
  "${SRC}/include/*.h")
set(bad 0)
foreach(f ${files})
  get_filename_component(basename "${f}" NAME)
  # cf_port.h decides which compiler; cf_platform.h decides which
  # platform. Both are the door, so both are allowed to look outside it.
  if(NOT basename STREQUAL "cf_port.h"
     AND NOT basename STREQUAL "port.c"
     AND NOT basename STREQUAL "cf_platform.h")
    file(READ "${f}" text)

    # Compiler and standard detection belongs to the portability layer.
    if(text MATCHES
       "(^|[^A-Za-z0-9_])(__STDC_VERSION__|_MSC_VER|__GNUC__|__clang__)")
      message("VIOLATION: ${f} performs its own feature detection")
      set(bad 1)
    endif()

    # Platform detection belongs to src/platform, where the build picks
    # the file and the call site never asks. A raw _WIN32 anywhere else
    # is how three layers ended up each knowing what an operating system
    # is. CF_ATOMIC_WIN32 and friends are derived names and are fine -
    # hence the boundary on the left of the match.
    if(NOT f MATCHES "/src/platform/")
      set(plat "_WIN32|_WIN64|__unix__|__unix|__linux__")
      set(plat "${plat}|__APPLE__|__FreeBSD__|__MINGW32__")
      set(edge "[^A-Za-z0-9_]")
      if(text MATCHES "(^|${edge})(${plat})(${edge}|$)")
        message("VIOLATION: ${f} detects the platform itself")
        set(bad 1)
      endif()
    endif()
  endif()
endforeach()

if(bad)
  message(FATAL_ERROR "detection gate failed")
endif()
message("detection gate: clean")
