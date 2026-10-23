# Doctor reads reports (JSON) and its adapters write them.
if(CF_WITH_JSON AND CF_WITH_REPORT)
  add_subdirectory(doctor)
else()
  message(STATUS "doctor needs CF_WITH_JSON and CF_WITH_REPORT; not built")
endif()
