# The bridge: key space <-> bytes.
#
# These are the only files that legitimately use both the configuration
# model and the serialization layer, because bridging them is what they
# are. Nothing below may depend on this directory, which is the rule the
# layering gate checks.
target_sources(
  configforge
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/flatten.c
          ${CMAKE_CURRENT_SOURCE_DIR}/image.c
          ${CMAKE_CURRENT_SOURCE_DIR}/file.c
          ${CMAKE_CURRENT_SOURCE_DIR}/dotenv.c
          ${CMAKE_CURRENT_SOURCE_DIR}/dir.c)
