# Each layer owns its own list. The core knows nothing about formats;
# the serialization layer knows nothing about configuration semantics.
add_subdirectory(platform)
add_subdirectory(core)
add_subdirectory(sources)
add_subdirectory(diag)
add_subdirectory(serialize)
add_subdirectory(bridge)

# The serialization library's sources beyond src/serialize/: the leaves it
# uses (cmake/ConfigForgeSerialize.cmake), which configforge compiles in the
# directories above. The three platform file.c are chosen as
# platform/CMakeLists.txt chooses.
if(CF_PLATFORM STREQUAL "none")
  set(cf_serialize_platform none)
elseif(WIN32)
  set(cf_serialize_platform win32)
else()
  set(cf_serialize_platform posix)
endif()
foreach(leaf ${CF_SERIALIZE_LEAVES})
  if(NOT leaf MATCHES "^platform/" OR leaf STREQUAL
                                     "platform/${cf_serialize_platform}/file.c")
    target_sources(configforge_serialize
                   PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/${leaf})
  endif()
endforeach()
if(CF_PLATFORM STREQUAL "none")
  target_compile_definitions(configforge_serialize PUBLIC CF_PLATFORM_NONE=1)
endif()
