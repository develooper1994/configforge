# The .NET adapter: GetDebugView() in, a report out, and every Doctor
# command working on it unchanged.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# What IConfigurationRoot.GetDebugView() prints: sections, values with
# the provider that won each, a provider whose own name holds
# parentheses, and a key no report can carry.
set(json "JsonConfigurationProvider for 'appsettings.json' (Optional)")
file(
  WRITE "${work}/dotnet_view.txt"
  "AllowedHosts=* (${json})\n"
  "ConnectionStrings:\n"
  "  Main=Server=db (x) "
  "(EnvironmentVariablesConfigurationProvider Prefix: '')\n"
  "Logging:\n"
  "  LogLevel:\n"
  "    Default=Warning (CommandLineConfigurationProvider)\n"
  "    Microsoft.AspNetCore=Information (${json})\n"
  "Odd Key=1 (MemoryConfigurationProvider)\n")

execute_process(
  COMMAND ${DOCTOR} import-dotnet
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/dotnet_view.txt"
  OUTPUT_FILE "${work}/dotnet_blind.json"
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "import-dotnet failed: ${err}")
endif()
if(NOT err MATCHES "1 key\\(s\\) left out[^\n]*odd key")
  message(
    FATAL_ERROR "the key that could not be carried was not reported: ${err}")
endif()

execute_process(
  COMMAND ${DOCTOR} dump dotnet_blind.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the adapter's report does not load")
endif()
# Keys lowered and joined with '.', each with the provider .NET named,
# the parentheses in the provider's name and in the value kept apart.
if(NOT out MATCHES
   "logging.loglevel.default[^\n]*\n  CommandLineConfigurationProvider ")
  message(FATAL_ERROR "the section path or its provider is wrong")
endif()
set(provider "JsonConfigurationProvider for 'appsettings.json' \\(Optional\\)")
if(NOT out MATCHES "allowedhosts[^\n]*\n  ${provider} ")
  message(FATAL_ERROR "a provider with parentheses in its name was cut")
endif()
if(out MATCHES "Server=db|Warning")
  message(FATAL_ERROR "a blind report carried a value")
endif()

# Blind, every key is sensitive, and the command line's kind is .NET's
# own, so CFD007 does not call every command-line key a secret.
execute_process(
  COMMAND ${DOCTOR} lint dotnet_blind.json --fail-on never
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out)
message("${out}")
if(out MATCHES "CFD007")
  message(FATAL_ERROR "a blind import made every command-line key a secret")
endif()

# With --full the value is there, parentheses and all.
execute_process(
  COMMAND ${DOCTOR} import-dotnet --full
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/dotnet_view.txt"
  OUTPUT_FILE "${work}/dotnet_full.json"
  ERROR_QUIET)
execute_process(
  COMMAND ${DOCTOR} explain dotnet_full.json connectionstrings.main
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out)
message("${out}")
if(NOT out MATCHES "Server=db \\(x\\)" OR out MATCHES "sensitive")
  message(FATAL_ERROR "a full import should carry the whole value")
endif()

# Output it was not written for is refused, with its line.
file(WRITE "${work}/dotnet_bad.txt" "Logging:\n  Default=Warning\n")
execute_process(
  COMMAND ${DOCTOR} import-dotnet
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/dotnet_bad.txt"
  OUTPUT_QUIET
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 2 OR NOT err MATCHES "stdin:2")
  message(
    FATAL_ERROR
      "a value with no provider should be refused with its line: ${err}")
endif()

message("doctor dotnet adapter: clean")
