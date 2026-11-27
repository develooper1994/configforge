# The file layer's tests: the unit tests of this component that need nothing
# above it. A test belongs to the lowest component that holds every
# configforge/ header it includes (D-100).
cf_component_tests(platform)

cf_unit_tests(test_oom_file)
