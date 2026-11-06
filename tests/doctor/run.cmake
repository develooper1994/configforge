# Runs Doctor and asserts both the exit code and that the findings it
# was supposed to produce are in the output. Checking only the exit
# code would pass on a tool that printed nothing.
string(REPLACE "|" ";" argv "${ARGS}")
execute_process(
  COMMAND ${DOCTOR} ${argv}
  RESULT_VARIABLE rc
  OUTPUT_VARIABLE out
  ERROR_VARIABLE errout)
message("${out}${errout}")
if(NOT rc EQUAL EXPECT_FAIL)
  message(FATAL_ERROR "expected exit ${EXPECT_FAIL}, got ${rc}")
endif()

list(GET argv 0 cmd)

# Each lint case asserts the rule it exists for. A case that only
# checked the exit code would pass on a tool that printed nothing, and a
# case that checked the wrong rule would pass on a tool that found
# something else entirely.
if(cmd STREQUAL "lint" AND argv MATCHES "dead")
  if(NOT out MATCHES "CFD010")
    message(FATAL_ERROR "a dead source was not reported")
  endif()
  if(NOT out MATCHES "wins none")
    message(FATAL_ERROR "the finding does not say what is wrong")
  endif()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "leak")
  # A secret that reached the command line is the finding that matters
  # for the case policy exists for: rejecting it did not un-leak it.
  if(NOT out MATCHES "CFD016")
    message(FATAL_ERROR "a policy violation was not reported")
  endif()
  if(NOT out MATCHES "db.password")
    message(FATAL_ERROR "the policy finding does not name the key")
  endif()
  # And only once. The same rejected candidate used to be CFD006 (a
  # contract violation it was not) and CFD010 (a source that lost, when
  # it was refused) as well: three errors for one problem, which is how
  # the one that matters stops being read.
  foreach(rule CFD006 CFD010)
    if(out MATCHES "${rule}")
      message(FATAL_ERROR "one leaked secret was also reported as ${rule}")
    endif()
  endforeach()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "twice")
  # The earlier of two lines in one file can never win.
  if(NOT out MATCHES "CFD001[^\n]*server.port[^\n]*cf_fx_twice.ini:2")
    message(FATAL_ERROR "the unreachable line was not reported, or not named")
  endif()
  if(out MATCHES "CFD001[^\n]*server.host")
    message(FATAL_ERROR "a key set once was reported as unreachable")
  endif()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "sources")
  if(NOT out MATCHES "CFD007[^\n]*api.token")
    message(FATAL_ERROR "a secret on the command line was not reported")
  endif()
  if(NOT out MATCHES "CFD013[^\n]*server.port[^\n]*env above cli")
    message(FATAL_ERROR "the policy that inverts precedence was not reported")
  endif()
endif()

if(cmd STREQUAL "lint" AND argv MATCHES "rules")
  # The three a single report can prove from its definitions alone.
  foreach(rule CFD005 CFD009 CFD011)
    if(NOT out MATCHES "${rule}")
      message(FATAL_ERROR "lint did not produce ${rule}")
    endif()
  endforeach()
  if(NOT out MATCHES "nonesuch.key")
    message(FATAL_ERROR "the undefined key was not named")
  endif()
  if(NOT out MATCHES "old.name")
    message(FATAL_ERROR "the deprecated key was not named")
  endif()
endif()
if(cmd STREQUAL "lint" AND argv MATCHES "files")
  # Only the secret every user can read is reported, and only where the
  # platform can tell: Windows cannot, and says nothing rather than
  # guessing in either direction.
  if(UNIX)
    if(NOT out MATCHES "CFD007[^\n]*api.key[^\n]*every user")
      message(FATAL_ERROR "a secret in a world-readable file was not reported")
    endif()
  endif()
  if(out MATCHES "CFD007[^\n]*db.key")
    message(FATAL_ERROR "a secret only its owner can read was reported")
  endif()
endif()

if(cmd STREQUAL "diff")
  foreach(rule CFD002 CFD003 CFD014)
    if(NOT out MATCHES "${rule}")
      message(FATAL_ERROR "diff did not produce ${rule}")
    endif()
  endforeach()
  # The host is the same value from an environment variable here and a
  # file there: drift, and not a difference. The port is another value
  # from another source: a difference, and not drift. A value diff says
  # so once, and calling it drift as well would put a warning on the
  # commonest difference there is.
  if(NOT out MATCHES
     "CFD002[^\n]*server.host[^\n]*'env' here and 'user-file' there")
    message(
      FATAL_ERROR "the same host from two kinds of source was not reported")
  endif()
  if(out MATCHES "VALUE[^\n]*server.host")
    message(FATAL_ERROR "a host that agrees was reported as a difference")
  endif()
  if(out MATCHES "CFD002[^\n]*server.port")
    message(FATAL_ERROR "a port that differs was reported as origin drift")
  endif()
  if(NOT out MATCHES "VALUE[^\n]*server.port[^\n]*9090 here, 5000 there")
    message(
      FATAL_ERROR "the port that differs was not reported as a difference")
  endif()
endif()
if(cmd STREQUAL "lint" AND argv MATCHES "unset")
  # A key nothing won - an optional secret with no default, offered only
  # where its policy refuses it - was left out of what Doctor read, so
  # the secret it refused went unreported.
  if(NOT out MATCHES "CFD016[^\n]*license.code")
    message(FATAL_ERROR "a secret refused by policy, with no value left, "
                        "was not reported")
  endif()
  if(out MATCHES "ab12cd")
    message(FATAL_ERROR "the finding carries the secret")
  endif()
endif()
if(cmd STREQUAL "explain" AND argv MATCHES "cf_fx_doc")
  if(NOT out MATCHES "remote[^\n]*\\[json: content\\]")
    message(FATAL_ERROR "explain did not say a document's format was the content's")
  endif()
  if(NOT out MATCHES "embedded[^\n]*\\[json: named\\]")
    message(FATAL_ERROR "explain did not say a document's format was named")
  endif()
  if(out MATCHES "env[^\n]*\\[")
    message(FATAL_ERROR "explain gave a format to a source of variables")
  endif()
endif()
if(cmd STREQUAL "explain" AND argv MATCHES "cf_fx_prod")
  if(out MATCHES "\\[[a-z0-9]+: (named|extension|content)\\]")
    message(FATAL_ERROR "explain named a format for a report without any")
  endif()
endif()
if(cmd STREQUAL "explain" AND argv MATCHES "unset")
  if(NOT out MATCHES "cli[^\n]*rejected[^\n]*policy")
    message(FATAL_ERROR "explain did not show the refused candidate and why")
  endif()
endif()
if(cmd STREQUAL "explain"
   AND EXPECT_FAIL EQUAL 0
   AND NOT argv MATCHES "unset")
  if(NOT out MATCHES "shadowed")
    message(FATAL_ERROR "explain did not show the losing candidate")
  endif()
endif()

# A sequence names what changed and where it comes from now, and never
# the value: production's port 9090, staging's 5000, and the secrets.
if(cmd MATCHES "^(history|blame)$" AND out MATCHES
                                       "9090|5000|prodsecret|devsecret")
  message(FATAL_ERROR "${cmd} printed a value")
endif()
if(cmd STREQUAL "history" AND EXPECT_FAIL LESS 2)
  # A step's line: HISTORY, the key, and what the step did to it.
  set(on "[^\n]*")
  set(port_did "value changed, now from 'cli' rather than 'env'")
  set(secret_did "value changed \\(by digest\\), still from 'env'")
  set(host_did "same value, now from 'user-file' rather than 'env'")
  if(NOT out MATCHES "HISTORY${on}server.port${on}${port_did}")
    message(FATAL_ERROR "the port's change was not reported")
  endif()
  if(NOT out MATCHES "HISTORY${on}db.password${on}${secret_did}")
    message(FATAL_ERROR "the secret's change was not told by its digest")
  endif()
  if(NOT out MATCHES "HISTORY${on}server.host${on}${host_did}")
    message(FATAL_ERROR "the host that moved with its value was not reported")
  endif()
  if(out MATCHES "HISTORY[^\n]*log.level")
    message(FATAL_ERROR "a key that did not change has a history")
  endif()
endif()
# --only-changed: what the change did not touch is counted, not shown,
# and a source the change went through keeps its findings.
if(cmd STREQUAL "lint"
   AND argv MATCHES "only-changed"
   AND EXPECT_FAIL LESS 2)
  if(NOT out MATCHES "on what the change did not touch")
    message(FATAL_ERROR "the findings left out were not counted")
  endif()
  if(argv MATCHES "cf_fx_dead" AND NOT out MATCHES "CFD010")
    message(
      FATAL_ERROR "the dead source, which the change touched, was left out")
  endif()
  if(argv MATCHES "cf_fx_staging.json;--base;cf_fx_staging" AND out MATCHES
                                                                "CFD008")
    message(FATAL_ERROR "a finding on a key the change left alone was shown")
  endif()
endif()
if(cmd STREQUAL "blame" AND EXPECT_FAIL EQUAL 0)
  set(since "from 'cli' since cf_fx_staging.json: value changed")
  if(NOT out MATCHES "BLAME[^\n]*server.port[^\n]*${since}")
    message(FATAL_ERROR "blame did not name the report the port dates from")
  endif()
endif()

# A contract check names what is wrong, not only that something is: the
# misspelt key with the one it was meant to be, the value with its range.
if(cmd STREQUAL "check")
  if(argv MATCHES "contract_typo" AND NOT out MATCHES
                                      "server.prot.*did you mean 'server.port'")
    message(FATAL_ERROR "the misspelt key and its suggestion were not named")
  endif()
  if(argv MATCHES "contract_range" AND NOT out MATCHES "server.port.*70000")
    message(FATAL_ERROR "the value out of range was not named")
  endif()
  if(EXPECT_FAIL EQUAL 0 AND NOT out MATCHES "^ok: ")
    message(FATAL_ERROR "a file that holds to its contract was not said to")
  endif()
endif()
