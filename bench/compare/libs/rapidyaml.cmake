# rapidyaml 0.16.0 carries c4core inside its tree (ext/c4core.src): the sources
# are ryml's own list (its CMakeLists.txt) and c4core's (ext/c4core.src.txt).
file(GLOB ryml_yml "${LIB_DIR}/src/c4/yml/*.cpp")
file(GLOB c4_core "${LIB_DIR}/ext/c4core.src/c4/*.cpp")
cfb_library(
  rapidyaml
  CXX
  SOURCES
  ${ryml_yml}
  ${c4_core}
  INCLUDES
  "${LIB_DIR}/src"
  "${LIB_DIR}/ext/c4core.src")
cfb_adapter(
  rapidyaml
  adapters/rapidyaml.cpp
  VERSIONOF
  rapidyaml
  LIBS
  rapidyaml)
