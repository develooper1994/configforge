# Two boundaries, both stated in the design and neither of them real
# until something checks them.
#
# 1. src/core must not reach into sources, serialize or platform.
# 2. src/serialize may use the leaf types - value, error, alloc, port -
#    and may not touch the key space, the definitions or the sources.
#    That is what keeps extracting the serialization layer later a
#    directory move and a build file rather than a rewrite.

set(bad 0)

file(GLOB_RECURSE core_files "${SRC}/core/*.c" "${SRC}/core/*.h")
foreach(f ${core_files})
  file(READ "${f}" text)
  if(text MATCHES "#[ \t]*include[ \t]*[\"<](\\.\\./)?(sources|platform|serialize)/")
    message("VIOLATION: ${f} includes outside the core")
    set(bad 1)
  endif()
  if(text MATCHES "configforge/cf_(serialize|flatten|source)\\.h")
    message("VIOLATION: ${f} (core) depends on a layer above it")
    set(bad 1)
  endif()
endforeach()

file(GLOB_RECURSE ser_files "${SRC}/serialize/*.c" "${SRC}/serialize/*.h")
foreach(f ${ser_files})
  file(READ "${f}" text)
  if(text MATCHES "configforge/(cf_keyspace|cf_define|cf_source|cf_flatten|cf_explain)\\.h")
    message("VIOLATION: ${f} (serialize) reaches into the configuration model")
    set(bad 1)
  endif()
  if(text MATCHES "configforge/configforge\\.h")
    message("VIOLATION: ${f} (serialize) includes the public umbrella header")
    set(bad 1)
  endif()
endforeach()

file(GLOB_RECURSE below "${SRC}/core/*.c" "${SRC}/serialize/*.c"
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
