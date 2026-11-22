# The serialization library alone is the one unit test of this component
# that is not config's: it includes cf_serialize.h and nothing above it.
cf_component_tests(serialize configforge_serialize)

# D-109: what each format can hold, held to what its codec says.
cf_unit_tests(test_holds test_text_escape)
# A-7: its YAML cases are the built-in parser's: the test must know when
# libyaml or libfyaml reads instead.
if(_CF_YAML_LIBYAML)
  target_compile_definitions(test_text_escape PRIVATE CF_YAML_LIBYAML=1)
endif()
if(_CF_YAML_LIBFYAML)
  target_compile_definitions(test_text_escape PRIVATE CF_YAML_LIBFYAML=1)
endif()

# D-90: a program linked to
# the serialization library and nothing else, which includes one header. An
# unresolved symbol fails its link, naming what the library lacks.
if(CF_WITH_JSON)
  add_executable(test_serialize_alone test_serialize_alone.c)
  target_link_libraries(test_serialize_alone PRIVATE ${CF_TEST_LIB})
  set_target_properties(
    test_serialize_alone
    PROPERTIES C_STANDARD 90
               C_STANDARD_REQUIRED ON
               C_EXTENSIONS OFF)
  configforge_apply_warnings(test_serialize_alone)
  configforge_apply_analysis(test_serialize_alone)
  if(CF_CPPCHECK_TESTS)
    set_target_properties(test_serialize_alone
                          PROPERTIES C_CPPCHECK "${CF_CPPCHECK_TESTS}")
  endif()
  add_test(NAME unit.serialize_alone COMMAND test_serialize_alone)
endif()
