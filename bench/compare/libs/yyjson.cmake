# yyjson: src/yyjson.c, as its CMakeLists.txt lists it; no definition.
cfb_library(
  yyjson
  SOURCES
  "${LIB_DIR}/src/yyjson.c"
  INCLUDES
  "${LIB_DIR}/src")
cfb_adapter(
  yyjson
  adapters/yyjson.cpp
  VERSIONOF
  yyjson
  LIBS
  yyjson)
