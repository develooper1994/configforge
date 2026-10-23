# The software bill of materials, as SPDX 2.3.
#
# Generated at configure time from the options this build was given,
# rather than written by hand, so it cannot describe a build that was not
# made: a build with CF_WAKE_KQUEUE on Linux links libkqueue and says so,
# and the default build says it depends on nothing. docs/Dependencies.md
# is the same information for a person, and gate.dependencies keeps that
# page honest about what the build looks for.

# Write <build>/configforge.spdx.json and install it beside the licence.
#
# ~~~
# configforge_generate_sbom()
# ~~~
function(configforge_generate_sbom)
  # SOURCE_DATE_EPOCH is honoured by string(TIMESTAMP), which keeps the
  # file reproducible where a build asks for that.
  string(TIMESTAMP CF_SBOM_CREATED "%Y-%m-%dT%H:%M:%SZ" UTC)

  set(opts "")
  set(sbom_opts "")
  foreach(f ${CF_FEATURES})
    list(APPEND sbom_opts CF_WITH_${f})
  endforeach()
  foreach(opt ${sbom_opts} CF_WAKE_KQUEUE)
    if(${opt})
      string(APPEND opts "${opt}=ON ")
    else()
      string(APPEND opts "${opt}=OFF ")
    endif()
  endforeach()
  string(STRIP "${opts}" CF_SBOM_OPTIONS)
  string(MD5 CF_SBOM_ID "${PROJECT_VERSION} ${CF_SBOM_OPTIONS}")

  set(CF_SBOM_PACKAGES "")
  set(CF_SBOM_RELATIONSHIPS "")
  if(CF_WAKE_KQUEUE
     AND NOT APPLE
     AND NOT CMAKE_SYSTEM_NAME MATCHES "BSD|DragonFly")
    set(CF_SBOM_PACKAGES
        ",
    {
      \"name\": \"libkqueue\",
      \"SPDXID\": \"SPDXRef-Package-libkqueue\",
      \"versionInfo\": \"${_CF_KQUEUE_VERSION}\",
      \"downloadLocation\": \"NOASSERTION\",
      \"filesAnalyzed\": false,
      \"licenseConcluded\": \"BSD-2-Clause\",
      \"licenseDeclared\": \"BSD-2-Clause\",
      \"copyrightText\": \"NOASSERTION\",
      \"comment\": \"The kqueue test build on Linux only.\"
    }")
    set(CF_SBOM_RELATIONSHIPS
        ",
    {
      \"spdxElementId\": \"SPDXRef-Package-configforge\",
      \"relationshipType\": \"DEPENDS_ON\",
      \"relatedSpdxElement\": \"SPDXRef-Package-libkqueue\"
    }")
  endif()

  configure_file("${CMAKE_SOURCE_DIR}/cmake/configforge.spdx.json.in"
                 "${CMAKE_BINARY_DIR}/configforge.spdx.json" @ONLY)
  install(FILES "${CMAKE_BINARY_DIR}/configforge.spdx.json"
          DESTINATION ${CMAKE_INSTALL_DATADIR}/doc/configforge)
endfunction()
