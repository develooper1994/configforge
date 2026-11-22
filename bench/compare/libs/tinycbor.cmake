# tinycbor: the parser, the validator and the error strings of its
# CMakeLists.txt (the encoder, the pretty printer and the JSON conversion are
# not read paths). Its two generated headers are made from the .in files it
# ships: the export header (empty, for a static library) and the version.
set(cbor_gen "${CMAKE_BINARY_DIR}/gen/tinycbor")
file(MAKE_DIRECTORY "${cbor_gen}")
configure_file("${LIB_DIR}/src/tinycbor-export.h.in"
               "${cbor_gen}/tinycbor-export.h" COPYONLY)
set(PROJECT_VERSION_MAJOR 7)
set(PROJECT_VERSION_MINOR 1)
configure_file("${LIB_DIR}/src/tinycbor-version.h.in"
               "${cbor_gen}/tinycbor-version.h")
cfb_library(
  tinycbor
  SOURCES
  "${LIB_DIR}/src/cborparser.c"
  "${LIB_DIR}/src/cborvalidation.c"
  "${LIB_DIR}/src/cborerrorstrings.c"
  "${LIB_DIR}/src/cborparser_float.c"
  INCLUDES
  "${LIB_DIR}/src"
  "${cbor_gen}"
  DEFINITIONS
  CBOR_STATIC_DEFINE)
cfb_adapter(
  tinycbor
  adapters/tinycbor.cpp
  VERSIONOF
  tinycbor
  LIBS
  tinycbor)
