add_test(NAME matrix.sizes COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}"
                                   -P "${CMAKE_CURRENT_SOURCE_DIR}/sizes.cmake")

add_test(NAME matrix.script
         COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" -DMATRIX_LIB=ON
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/matrix.cmake")

add_test(NAME matrix.markers
         COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" -DMATRIX_LIB=ON
                 -DMARKERS=ON -P "${CMAKE_CURRENT_SOURCE_DIR}/matrix.cmake")

add_test(NAME matrix.tables
         COMMAND ${CMAKE_COMMAND} "-DSRC=${CMAKE_SOURCE_DIR}" -DMATRIX_LIB=ON
                 -P "${CMAKE_CURRENT_SOURCE_DIR}/tables.cmake")
