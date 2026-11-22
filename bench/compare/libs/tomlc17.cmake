# tomlc17: src/tomlc17.c.
cfb_library(
  tomlc17
  SOURCES
  "${LIB_DIR}/src/tomlc17.c"
  INCLUDES
  "${LIB_DIR}/src")
cfb_adapter(
  tomlc17
  adapters/tomlc17.cpp
  VERSIONOF
  tomlc17
  LIBS
  tomlc17)
