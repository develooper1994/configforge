# simdjson: the amalgamated singleheader/simdjson.cpp and .h, which its own
# singleheader/CMakeLists.txt builds; no definition.
cfb_library(
  simdjson
  SOURCES
  "${LIB_DIR}/singleheader/simdjson.cpp"
  INCLUDES
  "${LIB_DIR}/singleheader")
cfb_adapter(
  simdjson
  adapters/simdjson.cpp
  VERSIONOF
  simdjson
  LIBS
  simdjson)
