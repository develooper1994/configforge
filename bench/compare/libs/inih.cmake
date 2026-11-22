# inih: ini.c with the settings the card names (a line of up to 8192 bytes, no
# stack buffer, stop at the first error) and INI_ALLOW_REALLOC, without which
# a heap buffer stays at INI_INITIAL_ALLOC (200 bytes) and INI_MAX_LINE is not
# the limit.
cfb_library(
  inih
  SOURCES
  "${LIB_DIR}/ini.c"
  INCLUDES
  "${LIB_DIR}"
  DEFINITIONS
  INI_STOP_ON_FIRST_ERROR=1
  INI_USE_STACK=0
  INI_ALLOW_REALLOC=1
  INI_MAX_LINE=8192)
cfb_adapter(
  inih
  adapters/inih.cpp
  VERSIONOF
  inih
  LIBS
  inih)
