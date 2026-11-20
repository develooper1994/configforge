# configforge-gen: the contract's program, written as C calls and held
# against files. It reads the contract with the JSON reader and writes it
# through cf_schema_write, so it is built where Doctor is. Like Doctor's
# directory, it sees the library's internal tier (D-51, D-88): the
# contract code uses the definition set's functions.
add_definitions(-DCF_INTERNAL_API=1)

# A generator outside the tree is found and run by the file the platform
# has (D-110), chosen as the front door's is.
if(CF_PLATFORM STREQUAL "none" OR CMAKE_SYSTEM_NAME STREQUAL "WASI")
  set(gen_spawn spawn_none.c)
elseif(WIN32)
  set(gen_spawn spawn_win32.c)
else()
  set(gen_spawn spawn_posix.c)
endif()
add_executable(configforge_gen gen.c contract.c ${gen_spawn})
if(WIN32)
  target_link_libraries(configforge_gen PRIVATE shell32)
endif()
target_link_libraries(configforge_gen PRIVATE configforge)
set_target_properties(
  configforge_gen
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             OUTPUT_NAME configforge-gen
             RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR})
configforge_apply_warnings(configforge_gen)
configforge_apply_hardening(configforge_gen)
configforge_apply_analysis(configforge_gen)

# In a Windows shared build the tests' programs stay in bin/ beside the
# DLL; a tool that links the library gets it copied beside it.
if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET configforge_gen
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge>
            $<TARGET_FILE_DIR:configforge_gen>
    VERBATIM)
endif()
# An installed shared build finds the library by its run path.
if(BUILD_SHARED_LIBS AND NOT WIN32)
  if(APPLE)
    set(gen_rpath "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(gen_rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  endif()
  set_property(TARGET configforge_gen PROPERTY INSTALL_RPATH "${gen_rpath}")
endif()

# Everything clang-tidy checks, less cert-err33-c, as Doctor's.
if(CMAKE_C_CLANG_TIDY)
  set(gen_tidy ${CMAKE_C_CLANG_TIDY} --checks=-cert-err33-c)
  set_target_properties(configforge_gen PROPERTIES C_CLANG_TIDY "${gen_tidy}")
endif()
