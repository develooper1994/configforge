add_executable(configforge_diff main.c)
target_link_libraries(configforge_diff PRIVATE configforge_serialize)
set_target_properties(
  configforge_diff
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             OUTPUT_NAME configforge-diff
             RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR})
configforge_apply_warnings(configforge_diff)
configforge_apply_hardening(configforge_diff)
configforge_apply_analysis(configforge_diff)

# In a Windows shared build the tests' programs stay in bin/ beside the
# DLL; a tool that links the library gets it copied beside it.
if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET configforge_diff
    POST_BUILD
    COMMAND
      ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge_serialize>
      $<TARGET_FILE_DIR:configforge_diff>
    VERBATIM)
endif()
# An installed shared build finds the library by its run path.
if(BUILD_SHARED_LIBS AND NOT WIN32)
  if(APPLE)
    set(diff_rpath "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(diff_rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  endif()
  set_property(TARGET configforge_diff PROPERTY INSTALL_RPATH "${diff_rpath}")
endif()
