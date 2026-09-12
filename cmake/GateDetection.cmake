# Compiler and standard detection may appear only in cf_port.h and
# port.c. The value of a portability layer is that it is the only place
# with conditional compilation in it.
file(GLOB_RECURSE files "${SRC}/src/*.c" "${SRC}/src/*.h" "${SRC}/include/*.h")
set(bad 0)
foreach(f ${files})
  get_filename_component(name "${f}" NAME)
  if(NOT name STREQUAL "cf_port.h" AND NOT name STREQUAL "port.c")
    file(READ "${f}" text)
    if(text MATCHES "__STDC_VERSION__|_MSC_VER|__GNUC__|__clang__")
      message("VIOLATION: ${f} performs its own feature detection")
      set(bad 1)
    endif()
  endif()
endforeach()
if(bad)
  message(FATAL_ERROR "detection gate failed")
endif()
message("detection gate: clean")
