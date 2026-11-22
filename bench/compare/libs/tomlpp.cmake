# toml++: header-only, as TOML_HEADER_ONLY=1 and TOML_EXCEPTIONS=0.
add_library(lib_tomlpp INTERFACE)
target_include_directories(lib_tomlpp INTERFACE "${LIB_DIR}/include")
target_compile_definitions(lib_tomlpp INTERFACE TOML_HEADER_ONLY=1
                                                TOML_EXCEPTIONS=0)
add_library(sizelib_tomlpp INTERFACE)
target_link_libraries(sizelib_tomlpp INTERFACE lib_tomlpp)
cfb_adapter(
  tomlpp
  adapters/tomlpp.cpp
  VERSIONOF
  tomlpp
  LIBS
  tomlpp)
