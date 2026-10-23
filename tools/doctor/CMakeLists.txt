# A separate binary, not a library feature. Doctor reads reports; the
# library produces them. Keeping them apart is what stops the tool from
# growing a resolver of its own.
add_executable(doctor doctor.c git.c)
target_link_libraries(doctor PRIVATE configforge)
set_target_properties(
  doctor
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(doctor)
configforge_apply_analysis(doctor)

# Everything clang-tidy checks, less cert-err33-c: it asks that the
# result of every fprintf to stderr be checked, and a diagnostic that
# could not be printed has nowhere better to be reported.
if(CMAKE_C_CLANG_TIDY)
  set(doctor_tidy ${CMAKE_C_CLANG_TIDY} --checks=-cert-err33-c)
  set_target_properties(doctor PROPERTIES C_CLANG_TIDY "${doctor_tidy}")
endif()
