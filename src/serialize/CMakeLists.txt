# The serialization layer. It may use the leaf types - values, errors,
# arenas - and may not touch the key space, the definitions or the
# sources. The boundary is checked in CI so that extracting this layer
# later stays a directory move and a build file.
target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/blob.c
                                   ${CMAKE_CURRENT_SOURCE_DIR}/codec.c)

if(CF_WITH_INI)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ini.c)
endif()
if(CF_WITH_JSON)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/json.c)
endif()
if(CF_WITH_TOML)
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/toml.c)
endif()

# The one codec with a dependency, so the one that has to be asked for.
# libyaml is found as a header and a library rather than through a
# package, because Debian, Homebrew and vcpkg agree on those two names
# and on nothing else.
if(CF_WITH_YAML)
  find_path(CF_YAML_INCLUDE_DIR yaml.h)
  find_library(CF_YAML_LIBRARY NAMES yaml libyaml yaml-0)
  if(NOT CF_YAML_INCLUDE_DIR OR NOT CF_YAML_LIBRARY)
    message(
      FATAL_ERROR
        "CF_WITH_YAML needs libyaml: install it (libyaml-dev, brew install "
        "libyaml, vcpkg install libyaml) or point CMAKE_PREFIX_PATH at it")
  endif()
  target_sources(configforge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/yaml.c)
  target_include_directories(configforge PRIVATE ${CF_YAML_INCLUDE_DIR})
  # Linked in the top-level CMakeLists.txt, where the target is made:
  # under the policies of CMake 3.10 a subdirectory may not link it.
endif()
