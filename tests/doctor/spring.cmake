# The Spring Boot adapter: /actuator/env in, a report out, and every
# Doctor command working on it unchanged.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# What /actuator/env answers, trimmed: the sources highest first, a
# masked password on the command line, a variable before relaxed binding
# has made it a property, and a property file with Spring's origin.
set(props "class path resource [application.properties]")
file(
  WRITE "${work}/spring_env.json"
  "{\"activeProfiles\": [\"prod\", \"eu\"],\n"
  " \"propertySources\": [\n"
  "  {\"name\": \"commandLineArgs\", \"properties\": {\n"
  "    \"server.port\": {\"value\": \"9090\"},\n"
  "    \"spring.datasource.password\": {\"value\": \"******\"}}},\n"
  "  {\"name\": \"systemEnvironment\", \"properties\": {\n"
  "    \"SERVER_PORT\": {\"value\": \"******\",\n"
  "      \"origin\": \"System Environment Property \\\"SERVER_PORT\\\"\"}}},\n"
  "  {\"name\": \"Config resource 'class path resource "
  "[application.properties]' via location 'optional:classpath:/'\",\n"
  "   \"properties\": {\n"
  "    \"server.port\": {\"value\": \"8080\",\n"
  "      \"origin\": \"${props} - 1:13\"},\n"
  "    \"spring.application.name\": {\"value\": \"orders\",\n"
  "      \"origin\": \"${props} - 2:25\"}}}\n"
  " ]}\n")

execute_process(
  COMMAND ${DOCTOR} import-spring
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/spring_env.json"
  OUTPUT_FILE "${work}/spring.json"
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "import-spring failed: ${err}")
endif()
# SERVER_PORT is server.port only after Spring's relaxed binding, which
# is Spring's to do; it is counted aloud, not re-spelled.
if(NOT err MATCHES "1 key\\(s\\) left out[^\n]*SERVER_PORT")
  message(
    FATAL_ERROR
      "the variable that could not be carried was not reported: ${err}")
endif()

execute_process(
  COMMAND ${DOCTOR} dump spring.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the adapter's report does not load")
endif()
# Spring's order: the command line over the property file.
if(NOT out MATCHES "server.port[^\n]*\n  commandLineArgs [^\n]*selected")
  message(FATAL_ERROR "the command line should win server.port")
endif()
if(NOT out MATCHES "application.properties:1:13")
  message(FATAL_ERROR "Spring's origin was not kept as a place")
endif()
if(NOT out MATCHES "profile=prod ")
  message(FATAL_ERROR "the first active profile is not the report's")
endif()
# What Spring masked stays masked; what it exposed, it exposed.
set(masked "spring.datasource.password[^\n]*sensitive[^\n]*\n")
if(NOT out MATCHES "${masked}  commandLineArgs <redacted>")
  message(FATAL_ERROR "a value Spring masked is not sensitive")
endif()

# What Spring masked is sensitive, and on the command line that is
# CFD007's; what it did not mask is not called a secret.
execute_process(
  COMMAND ${DOCTOR} lint spring.json --fail-on never
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT out MATCHES "CFD007[^\n]*spring.datasource.password")
  message(FATAL_ERROR "a masked value on the command line was not reported")
endif()
if(out MATCHES "CFD007[^\n]*server.port")
  message(FATAL_ERROR "a value Spring did not mask was called a secret")
endif()

# Output it was not written for is refused, not guessed at.
file(WRITE "${work}/spring_bad.json" "{\"status\": \"UP\"}\n")
execute_process(
  COMMAND ${DOCTOR} import-spring
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/spring_bad.json"
  OUTPUT_QUIET
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 2 OR NOT err MATCHES "propertySources")
  message(
    FATAL_ERROR "JSON that is not /actuator/env should be refused: ${err}")
endif()

message("doctor spring adapter: clean")
