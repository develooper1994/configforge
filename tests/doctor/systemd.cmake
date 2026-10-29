# The systemd adapter: cat-config in, a report out, and every Doctor
# command working on it unchanged.

set(work "${CMAKE_CURRENT_BINARY_DIR}")

# What `systemd-analyze cat-config systemd/journald.conf` prints, written
# here rather than read from the machine: the main file with a key set
# twice, a commented default, and a comment that looks like a header but
# is not after a blank line; a drop-in that is overridden in everything
# it sets; and the last drop-in, which wins, with a continued line.
file(
  WRITE "${work}/systemd_cat.txt"
  "# /etc/systemd/journald.conf\n"
  "[Journal]\n"
  "#Storage=auto\n"
  "Storage=volatile\n"
  "# /usr/share/doc says persistent is better\n"
  "Storage=persistent\n"
  "SystemMaxUse=1G\n"
  "\n"
  "# /usr/lib/systemd/journald.conf.d/10-vendor.conf\n"
  "[Journal]\n"
  "SystemMaxUse=500M\n"
  "\n"
  "# /etc/systemd/journald.conf.d/90-local.conf\n"
  "[Journal]\n"
  "SystemMaxUse=2G\n"
  "ForwardToSyslog=\\\n"
  "  yes\n")

execute_process(
  COMMAND ${DOCTOR} import-systemd --full
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/systemd_cat.txt"
  OUTPUT_FILE "${work}/systemd.json"
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "import-systemd failed: ${err}")
endif()

execute_process(
  COMMAND ${DOCTOR} dump systemd.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out
  RESULT_VARIABLE rc)
message("${out}")
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "the adapter's report does not load")
endif()
# systemd's order: the last drop-in over the earlier one over the file.
set(local "/etc/systemd/journald.conf.d/90-local.conf")
if(NOT out MATCHES
   "journal.systemmaxuse[^\n]*\n  ${local} +2G +selected +${local}:2")
  message(FATAL_ERROR "the last drop-in should win, at its own line")
endif()
# Within the file the later line wins, and the place is the file's line.
if(NOT out MATCHES "persistent +selected +/etc/systemd/journald.conf:5")
  message(FATAL_ERROR "the later line in a file should win, at its line")
endif()
if(NOT out MATCHES "journal.forwardtosyslog[^\n]*\n[^\n]*yes +selected")
  message(FATAL_ERROR "a continued line was not joined")
endif()
if(out MATCHES "share/doc")
  message(FATAL_ERROR "a comment not after a blank line was read as a header")
endif()

# The rules that need a chain run on it: the key set twice in one file,
# and the drop-in that wins nothing.
execute_process(
  COMMAND ${DOCTOR} lint systemd.json --fail-on never
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out)
message("${out}")
if(NOT out MATCHES "CFD001[^\n]*journal.storage")
  message(FATAL_ERROR "a key set twice in one file was not reported")
endif()
if(NOT out MATCHES "CFD010[^\n]*10-vendor.conf")
  message(FATAL_ERROR "a drop-in that wins nothing was not reported")
endif()

# Blind by default.
execute_process(
  COMMAND ${DOCTOR} import-systemd
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/systemd_cat.txt"
  OUTPUT_FILE "${work}/systemd_blind.json"
  ERROR_QUIET)
execute_process(
  COMMAND ${DOCTOR} dump systemd_blind.json
  WORKING_DIRECTORY "${work}"
  OUTPUT_VARIABLE out)
if(out MATCHES "persistent|2G")
  message(FATAL_ERROR "a blind report carried a value")
endif()

# Output it was not written for is refused, with its line.
file(WRITE "${work}/systemd_bad.txt" "[Journal]\nStorage=auto\n")
execute_process(
  COMMAND ${DOCTOR} import-systemd
  WORKING_DIRECTORY "${work}"
  INPUT_FILE "${work}/systemd_bad.txt"
  OUTPUT_QUIET
  ERROR_VARIABLE err
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 2 OR NOT err MATCHES "stdin:1")
  message(FATAL_ERROR "a file with no header should be refused: ${err}")
endif()

message("doctor systemd adapter: clean")
