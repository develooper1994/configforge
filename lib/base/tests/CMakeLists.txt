cf_component_tests(base)
cf_unit_tests(test_alloc test_value)

# The same test under a comma-decimal locale and a Turkish locale.
foreach(loc de_DE.UTF-8 tr_TR.UTF-8)
  add_test(NAME unit.test_value.${loc} COMMAND test_value ${loc})
endforeach()
