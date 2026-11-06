# Stage a standalone crate without keeping a second C source tree.
# cmake -DWORK=<new-directory> -P scripts/release/package_rust.cmake
cmake_minimum_required(VERSION 3.10)

get_filename_component(root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
if(NOT DEFINED WORK OR WORK STREQUAL "")
  message(FATAL_ERROR "package-rust: WORK must name a new directory")
endif()
get_filename_component(work "${WORK}" ABSOLUTE)
if(EXISTS "${work}")
  message(FATAL_ERROR "package-rust: WORK already exists: ${work}")
endif()
find_program(CARGO_EXE cargo)
if(NOT CARGO_EXE)
  message(FATAL_ERROR "package-rust: cargo is required")
endif()
include("${root}/cmake/ConfigForgeVersion.cmake")
configforge_read_version(version)

set(stage "${work}/package")
set(native "${stage}/native")
file(MAKE_DIRECTORY "${native}")
# What build.rs compiles (D-70): the headers and the C sources.
foreach(dir include src)
  file(COPY "${root}/${dir}" DESTINATION "${native}")
endforeach()
foreach(name LICENSE README.md CHANGELOG.md)
  file(COPY "${root}/${name}" DESTINATION "${native}")
endforeach()
foreach(
  name
  Cargo.toml
  Cargo.lock
  build.rs
  README.md
  src)
  file(COPY "${root}/bindings/rust/${name}" DESTINATION "${stage}")
endforeach()
file(COPY "${root}/LICENSE" DESTINATION "${stage}")
file(GLOB examples "${root}/examples/rust/*.rs")
file(COPY ${examples} DESTINATION "${stage}/examples")
file(READ "${stage}/Cargo.toml" manifest)
string(REPLACE "../../examples/rust/" "examples/" manifest "${manifest}")
configforge_write_lf("${stage}/Cargo.toml" "${manifest}")

execute_process(
  COMMAND "${CARGO_EXE}" package --locked --manifest-path "${stage}/Cargo.toml"
          --target-dir "${work}/target" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "package-rust: cargo package failed (${rc})")
endif()
set(unpacked "${work}/target/package/configforge-${version}")
execute_process(
  COMMAND "${CARGO_EXE}" test --locked --manifest-path "${unpacked}/Cargo.toml"
          --target-dir "${work}/target" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "package-rust: unpacked crate tests failed (${rc})")
endif()
message("package-rust: ${work}/target/package/configforge-${version}.crate")
