# The boundaries, stated in the design and none of them real until
# something checks them.
#
# 1. A component includes the public headers of itself and of the
#    components it declares in cf_component(... DEPENDS ...), and of those
#    they declare, and of no other: the header's component is the one whose
#    include/ holds it, the file's is the one it is under (D-100). The
#    declarations are read from the file the build wrote at configure time
#    (-DLAYERS=, cf_write_layers in cmake/ConfigForgeComponent.cmake). A
#    dependency spelled <component>:<header> allows that header and no
#    other of the component.
# 2. lib/config/src/core must not reach into sources, serialize or
#    platform, nor include cf_serialize.h, cf_flatten.h or cf_source.h.
# 3. core, sources and serialize include neither cf_flatten.h nor
#    cf_image.h, which are the bridge's.
# 4. The serialization library - every file of lib/serialize/src, the
#    leaf files it calls (the SERIALIZE_SOURCES of the declarations) and its
#    headers - may include, of configforge/,
#    only the headers in cmake/ConfigForgeSerialize.cmake and the two
#    definition lists. A list of what is allowed rather than of what is
#    not: a header the layer reaches for is a line in that file, and one
#    that is not there is a violation. The build reads the same lists.

# A script run with -P starts with every policy unset; IN_LIST needs
# CMP0057, as GateAbi.cmake's own line gives it.
cmake_minimum_required(VERSION 3.10)

set(bad 0)

include("${LAYERS}")
include("${LIB}/../cmake/ConfigForgeSerialize.cmake")

# What each component may use: its declared dependencies, and theirs.
foreach(c ${CF_LAYER_COMPONENTS})
  set(allowed "")
  set(todo ${CF_DEPENDS_${c}})
  while(NOT "${todo}" STREQUAL "")
    list(GET todo 0 d)
    list(REMOVE_AT todo 0)
    if(d IN_LIST allowed)
      continue()
    endif()
    list(APPEND allowed ${d})
    # A header of a component is not the component: what it uses is not
    # allowed by it.
    if(NOT d MATCHES ":")
      if(NOT DEFINED CF_DEPENDS_${d})
        message(
          "VIOLATION: ${c} depends on ${d}, which no cf_component declares")
        set(bad 1)
      else()
        list(APPEND todo ${CF_DEPENDS_${d}})
      endif()
    endif()
  endwhile()
  set(CF_LAYER_${c} ${allowed})
endforeach()

# The leaf files of the serialization library: the sources the declarations
# say go into it. The platform's files are one per platform and the build
# compiles one of them; each is held to the same rule, whichever this
# build took.
set(CF_SERIALIZE_LEAVES "")
foreach(c ${CF_LAYER_COMPONENTS})
  foreach(leaf ${CF_LEAVES_${c}})
    list(APPEND CF_SERIALIZE_LEAVES ${leaf})
    if(leaf MATCHES "^(.*/src)/(posix|win32|none)/(.*)$")
      set(leaf_dir "${CMAKE_MATCH_1}")
      set(leaf_file "${CMAKE_MATCH_3}")
      foreach(variant posix win32 none)
        list(APPEND CF_SERIALIZE_LEAVES "${leaf_dir}/${variant}/${leaf_file}")
      endforeach()
    endif()
  endforeach()
endforeach()
if(CF_SERIALIZE_LEAVES)
  list(REMOVE_DUPLICATES CF_SERIALIZE_LEAVES)
endif()

# 1. The table. A header's component is the one whose include/ holds it.
foreach(c ${CF_LAYER_COMPONENTS})
  file(GLOB heads "${LIB}/${c}/include/configforge/*")
  foreach(h ${heads})
    get_filename_component(name "${h}" NAME)
    set(cf_home_${name} ${c})
  endforeach()
endforeach()

set(any_inc
    "^[ \t]*#[ \t]*include[ \t]*[<\"](configforge|lanes)/([A-Za-z0-9_.]+)[>\"]")
foreach(c ${CF_LAYER_COMPONENTS})
  file(
    GLOB_RECURSE
    files
    "${LIB}/${c}/src/*.c"
    "${LIB}/${c}/src/*.h"
    "${LIB}/${c}/include/*.h"
    "${LIB}/${c}/include/*.hpp")
  foreach(f ${files})
    file(STRINGS "${f}" lines REGEX "${any_inc}")
    foreach(line IN LISTS lines)
      string(REGEX REPLACE "${any_inc}.*" "\\1" dir "${line}")
      string(REGEX REPLACE "${any_inc}.*" "\\2" inc "${line}")
      if(dir STREQUAL "lanes")
        set(home lanes)
      elseif(DEFINED cf_home_${inc})
        set(home ${cf_home_${inc}})
      else()
        # Not a header of a component: the generated one, or a tool's.
        continue()
      endif()
      if(NOT home STREQUAL c
         AND NOT home IN_LIST CF_LAYER_${c}
         AND NOT "${home}:${inc}" IN_LIST CF_LAYER_${c})
        message("VIOLATION: ${f} (${c}) includes ${dir}/${inc} of ${home}")
        set(bad 1)
      endif()
    endforeach()
  endforeach()
endforeach()

# 2. The core.
file(GLOB_RECURSE core_files "${LIB}/config/src/core/*.c"
     "${LIB}/config/src/core/*.h")
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

# 4. The serialization library.
file(GLOB_RECURSE ser_files "${LIB}/serialize/src/*.c"
     "${LIB}/serialize/src/*.h")
set(ser_leaf_files "")
foreach(leaf ${CF_SERIALIZE_LEAVES})
  if(EXISTS "${LIB}/${leaf}")
    list(APPEND ser_leaf_files "${LIB}/${leaf}")
  else()
    message("VIOLATION: ${LIB}/${leaf} (a leaf of the serialization library) "
            "does not exist")
    set(bad 1)
  endif()
endforeach()
set(ser_header_files "")
foreach(h ${CF_SERIALIZE_HEADERS})
  # cf_build_options.h is generated; the tree holds its defaults.
  if(DEFINED cf_home_${h})
    list(APPEND ser_header_files
         "${LIB}/${cf_home_${h}}/include/configforge/${h}")
  endif()
endforeach()
set(ser_inc "^[ \t]*#[ \t]*include[ \t]*[<\"]configforge/([A-Za-z0-9_.]+)[>\"]")
foreach(f ${ser_files} ${ser_leaf_files} ${ser_header_files})
  file(STRINGS "${f}" lines REGEX "${ser_inc}")
  foreach(line IN LISTS lines)
    string(REGEX REPLACE "${ser_inc}.*" "\\1" inc "${line}")
    if(NOT inc IN_LIST CF_SERIALIZE_HEADERS AND NOT inc MATCHES "\\.def$")
      message("VIOLATION: ${f} (the serialization library) "
              "includes configforge/${inc}")
      set(bad 1)
    endif()
  endforeach()
endforeach()

# 3. The bridge is above all three.
file(
  GLOB_RECURSE
  below
  "${LIB}/config/src/core/*.c"
  "${LIB}/serialize/src/*.c"
  "${LIB}/config/src/sources/*.c")
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
