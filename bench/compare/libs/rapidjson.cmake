# RapidJSON: header-only; include/. No SSE definition in `shipped` (a generic
# target); RAPIDJSON_SSE42 is added for `native` alone.
set(rj_defs "")
if(CF_BENCH_FLAGSET STREQUAL "native")
  set(rj_defs RAPIDJSON_SSE42)
endif()
add_library(lib_rapidjson INTERFACE)
target_include_directories(lib_rapidjson INTERFACE "${LIB_DIR}/include")
target_compile_definitions(lib_rapidjson INTERFACE ${rj_defs})
add_library(sizelib_rapidjson INTERFACE)
target_link_libraries(sizelib_rapidjson INTERFACE lib_rapidjson)
cfb_adapter(
  rapidjson
  adapters/rapidjson.cpp
  VERSIONOF
  rapidjson
  LIBS
  rapidjson)
