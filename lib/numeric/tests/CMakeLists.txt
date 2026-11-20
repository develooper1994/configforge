cf_component_tests(numeric)
cf_unit_tests(test_numeric)

# The same test under a comma-decimal locale and a Turkish locale.
# The Turkish case is the standard test for the tolower hazard.
foreach(loc de_DE.UTF-8 tr_TR.UTF-8)
  add_test(NAME unit.test_numeric.${loc} COMMAND test_numeric ${loc})
endforeach()
