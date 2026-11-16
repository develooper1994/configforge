# Two boundaries, both stated in the design and neither of them real
# until something checks them.
#
# 1. src/core must not reach into sources, serialize or platform.
# 2. The serialization library - every file of src/serialize, the leaf
#    files it calls and its headers - may include, of configforge/, only
#    the headers in cmake/ConfigForgeSerialize.cmake and the two
#    definition lists. A list of what is allowed rather than of what is
#    not: a header the layer reaches for is a line in that file, and one
#    that is not there is a violation. The build reads the same lists.

set(bad 0)

file(GLOB_RECURSE core_files "${SRC}/core/*.c" "${SRC}/core/*.h")
foreach(f ${core_files})
  file(READ "${f}" text)
  if(text MATCHES
     "#[ \t]*include[ \t]*[\"<](\\.\\./)?(sources|platform|serialize)/")
    message("VIOLATION: ${f} includes outside the core")
    set(bad 1)
  endif()
  if(text MATCHES "configforge/cf_(serialize|flatten|source)\\.h")
    message("VIOLATION: ${f} (core) depends on a layer above it")
    set(bad 1)
  endif()
endforeach()

include("${SRC}/../cmake/ConfigForgeSerialize.cmake")

file(GLOB_RECURSE ser_files "${SRC}/serialize/*.c" "${SRC}/serialize/*.h")
set(ser_leaf_files "")
foreach(leaf ${CF_SERIALIZE_LEAVES})
  if(EXISTS "${SRC}/${leaf}")
    list(APPEND ser_leaf_files "${SRC}/${leaf}")
  else()
    message("VIOLATION: ${SRC}/${leaf} (a leaf of the serialization library) "
            "does not exist")
    set(bad 1)
  endif()
endforeach()
set(ser_header_files "")
foreach(h ${CF_SERIALIZE_HEADERS})
  # cf_build_options.h is generated and is not in the source tree.
  if(EXISTS "${SRC}/../include/configforge/${h}")
    list(APPEND ser_header_files "${SRC}/../include/configforge/${h}")
  endif()
endforeach()
set(ser_inc "^[ \t]*#[ \t]*include[ \t]*[<\"]configforge/([A-Za-z0-9_.]+)[>\"]")
foreach(f ${ser_files} ${ser_leaf_files} ${ser_header_files})
  file(STRINGS "${f}" lines REGEX "${ser_inc}")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "${ser_inc}.*" "\\1" inc "${line}")
    if(NOT inc IN_LIST CF_SERIALIZE_HEADERS AND NOT inc MATCHES "\\.def$")
      message(
        "VIOLATION: ${f} (the serialization library) includes configforge/${inc}"
      )
      set(bad 1)
    endif()
  endforeach()
endforeach()

file(
  GLOB_RECURSE
  below
  "${SRC}/core/*.c"
  "${SRC}/serialize/*.c"
  "${SRC}/sources/*.c")
foreach(f ${below})
  file(READ "${f}" text)
  if(text MATCHES "configforge/(cf_flatten|cf_image)\\.h")
    message("VIOLATION: ${f} depends on the bridge above it")
    set(bad 1)
  endif()
endforeach()

if(bad)
  message(FATAL_ERROR "layering gate failed")
endif()
message("layering gate: clean")
