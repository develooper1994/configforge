# cJSON: cJSON.c; the symbols its CMakeLists.txt hides or exports are not
# needed for a static library.
cfb_library(
  cjson
  SOURCES
  "${LIB_DIR}/cJSON.c"
  INCLUDES
  "${LIB_DIR}")
cfb_adapter(
  cjson
  adapters/cjson.cpp
  VERSIONOF
  cjson
  LIBS
  cjson)
