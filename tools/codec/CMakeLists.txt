# configforge-codec: the serialization library's own program. It reads
# documents and never a configuration, so it is linked to
# configforge_serialize alone, includes configforge/cf_serialize.h and
# nothing else of the library, and is compiled without the internal tier
# (D-51, D-88; Doctor's and gen's directories define it, this one does
# not): it is the proof that the serialization library is enough.
add_executable(configforge_codec codec.c detect.c dump.c formats.c)
target_link_libraries(configforge_codec PRIVATE configforge_serialize)
set_target_properties(
  configforge_codec
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             OUTPUT_NAME configforge-codec
             RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR})
configforge_apply_warnings(configforge_codec)
configforge_apply_hardening(configforge_codec)
configforge_apply_analysis(configforge_codec)

# In a Windows shared build the tests' programs stay in bin/ beside the
# DLL; a tool that links the library gets it copied beside it.
if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET configforge_codec
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge_serialize>
      $<TARGET_FILE_DIR:configforge_codec>
    VERBATIM)
endif()
# An installed shared build finds the library by its run path.
if(BUILD_SHARED_LIBS AND NOT WIN32)
  if(APPLE)
    set(codec_rpath "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(codec_rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  endif()
  set_property(TARGET configforge_codec PROPERTY INSTALL_RPATH "${codec_rpath}")
endif()

# Everything clang-tidy checks, less cert-err33-c, as Doctor's.
if(CMAKE_C_CLANG_TIDY)
  set(codec_tidy ${CMAKE_C_CLANG_TIDY} --checks=-cert-err33-c)
  set_property(TARGET configforge_codec PROPERTY C_CLANG_TIDY "${codec_tidy}")
endif()
