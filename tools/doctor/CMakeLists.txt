# A separate binary, not a library feature. Doctor reads reports; the
# library produces them. Keeping them apart is what stops the tool from
# growing a resolver of its own.
add_executable(doctor doctor.c)
target_link_libraries(doctor PRIVATE configforge)
set_target_properties(
  doctor
  PROPERTIES C_STANDARD 90
             C_STANDARD_REQUIRED ON
             C_EXTENSIONS OFF)
configforge_apply_warnings(doctor)
configforge_apply_analysis(doctor)
