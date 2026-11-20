# One directory per component (D-100). Each adds its sources to
# configforge, and the ones the serialization library is made of to
# configforge_serialize too, as the order here says: a component uses
# only the ones before it.
add_subdirectory(port)
add_subdirectory(numeric)
add_subdirectory(base)
add_subdirectory(platform)
add_subdirectory(serialize)
add_subdirectory(config)
add_subdirectory(cxx)

# The serialization library's sources beyond lib/serialize/: the leaves it
# uses (cmake/ConfigForgeSerialize.cmake), which configforge compiles in the
# directories above. The three platform file.c are chosen as
# platform/src/CMakeLists.txt chooses.
if(CF_PLATFORM STREQUAL "none")
  set(cf_serialize_platform none)
elseif(WIN32)
  set(cf_serialize_platform win32)
else()
  set(cf_serialize_platform posix)
endif()
foreach(leaf ${CF_SERIALIZE_LEAVES})
  if(NOT leaf MATCHES "^platform/src/"
     OR leaf STREQUAL "platform/src/${cf_serialize_platform}/file.c")
    target_sources(configforge_serialize
                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${leaf})
  endif()
endforeach()
if(CF_PLATFORM STREQUAL "none")
  target_compile_definitions(configforge_serialize PUBLIC CF_PLATFORM_NONE=1)
endif()
