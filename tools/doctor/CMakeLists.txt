# Doctor reads reports; the library produces them. Keeping them apart is
# what stops the tool from growing a resolver of its own, so the rules
# are a library of their own (cf_doctor.h), not a feature of the
# configuration library: a device that reads configuration does not
# carry the rules that audit it. Static, so there is one ABI to keep,
# the configuration library's. The program is the library with a
# command line around it.
# Doctor sees the library's internal tier (D-51, D-88), here and in the
# directories below; the front door and the other tools do not.
add_definitions(-DCF_INTERNAL_API=1)
add_library(configforge_doctor STATIC engine.c gate.c)
add_library(ConfigForge::configforge_doctor ALIAS configforge_doctor)
target_link_libraries(configforge_doctor PUBLIC configforge)
set_target_properties(
  configforge_doctor
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             POSITION_INDEPENDENT_CODE ON)
configforge_apply_warnings(configforge_doctor)
configforge_apply_hardening(configforge_doctor)
configforge_apply_analysis(configforge_doctor)

add_executable(
  doctor
  doctor.c
  ../gen/contract.c
  formats.c
  git.c
  spring.c
  dotnet.c
  systemd.c)
target_link_libraries(doctor PRIVATE configforge_doctor)
set_target_properties(
  doctor
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF
             OUTPUT_NAME configforge-doctor
             RUNTIME_OUTPUT_DIRECTORY ${CF_TOOLS_DIR})
configforge_apply_warnings(doctor)
configforge_apply_hardening(doctor)
configforge_apply_analysis(doctor)

# The program is configforge-doctor (D-99). A copy named doctor stays
# beside it for 1.7.x, and says its new name when it is run.
add_custom_command(
  TARGET doctor
  POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:doctor>
          $<TARGET_FILE_DIR:doctor>/doctor${CMAKE_EXECUTABLE_SUFFIX}
  VERBATIM)
# In a Windows shared build the tests' programs stay in bin/ beside the
# DLL; a tool that links the library gets it copied beside it.
if(WIN32 AND BUILD_SHARED_LIBS)
  add_custom_command(
    TARGET doctor
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:configforge>
            $<TARGET_FILE_DIR:doctor>
    VERBATIM)
endif()
# An installed shared build finds the library by its run path.
if(BUILD_SHARED_LIBS AND NOT WIN32)
  if(APPLE)
    set(doctor_rpath "@loader_path/../${CMAKE_INSTALL_LIBDIR}")
  else()
    set(doctor_rpath "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
  endif()
  set_target_properties(doctor PROPERTIES INSTALL_RPATH "${doctor_rpath}")
endif()

# Everything clang-tidy checks, less cert-err33-c: it asks that the
# result of every fprintf to stderr be checked, and a diagnostic that
# could not be printed has nowhere better to be reported.
if(CMAKE_C_CLANG_TIDY)
  set(doctor_tidy ${CMAKE_C_CLANG_TIDY} --checks=-cert-err33-c)
  foreach(t doctor configforge_doctor)
    set_target_properties(${t} PROPERTIES C_CLANG_TIDY "${doctor_tidy}")
  endforeach()
endif()
