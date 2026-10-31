# Doctor reads reports; the library produces them. Keeping them apart is
# what stops the tool from growing a resolver of its own, so the rules
# are a library of their own (cf_doctor.h), not a feature of the
# configuration library: a device that reads configuration does not
# carry the rules that audit it. Static, so there is one ABI to keep,
# the configuration library's. The program is the library with a
# command line around it.
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
  git.c
  spring.c
  dotnet.c
  systemd.c)
target_link_libraries(doctor PRIVATE configforge_doctor)
set_target_properties(
  doctor
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(doctor)
configforge_apply_hardening(doctor)
configforge_apply_analysis(doctor)

# Everything clang-tidy checks, less cert-err33-c: it asks that the
# result of every fprintf to stderr be checked, and a diagnostic that
# could not be printed has nowhere better to be reported.
if(CMAKE_C_CLANG_TIDY)
  set(doctor_tidy ${CMAKE_C_CLANG_TIDY} --checks=-cert-err33-c)
  foreach(t doctor configforge_doctor)
    set_target_properties(${t} PROPERTIES C_CLANG_TIDY "${doctor_tidy}")
  endforeach()
endif()
