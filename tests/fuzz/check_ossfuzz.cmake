# gate.ossfuzz: the three files OSS-Fuzz asks of a project (oss-fuzz/) say
# what the card M-18 decided, and build the harnesses through the script
# ClusterFuzzLite shares. The contact is a placeholder until M-19-m: it
# fails this gate only when OSSFUZZ_APPLY=1 is in the environment.
set(dir "${SRC}/oss-fuzz")
foreach(f project.yaml Dockerfile build.sh)
  if(NOT EXISTS "${dir}/${f}")
    message(FATAL_ERROR "ossfuzz gate: oss-fuzz/${f} is missing")
  endif()
endforeach()
file(READ "${dir}/project.yaml" yaml)
file(READ "${dir}/Dockerfile" docker)
file(READ "${dir}/build.sh" build)
file(READ "${SRC}/.clusterfuzzlite/build.sh" cflite)

set(repo "https://github.com/develooper1994/configforge")
foreach(
  line
  "homepage: \"${repo}\""
  "language: c"
  "primary_contact:"
  "main_repo: \"${repo}\""
  "sanitizers: [address, undefined]"
  "architectures: [x86_64]"
  "fuzzing_engines: [libfuzzer, afl, honggfuzz]"
  "vendor_ccs: []")
  string(FIND "${yaml}" "${line}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "ossfuzz gate: project.yaml lacks `${line}`")
  endif()
endforeach()
if("${yaml}" MATCHES "TO BE FILLED BY THE MAINTAINER" AND "$ENV{OSSFUZZ_APPLY}"
                                                          STREQUAL "1")
  message(FATAL_ERROR "ossfuzz gate: primary_contact is still the placeholder")
endif()

foreach(
  line
  "FROM gcr.io/oss-fuzz-base/base-builder"
  "RUN apt-get update && apt-get install -y cmake"
  "RUN git clone --depth 1 ${repo} $SRC/configforge"
  "WORKDIR $SRC/configforge"
  "COPY build.sh $SRC/")
  string(FIND "${docker}" "${line}" at)
  if(at EQUAL -1)
    message(FATAL_ERROR "ossfuzz gate: Dockerfile lacks `${line}`")
  endif()
endforeach()

foreach(text "${build}" "${cflite}")
  string(FIND "${text}" "scripts/fuzz/build_harnesses.sh" at)
  if(at EQUAL -1)
    message(
      FATAL_ERROR
        "ossfuzz gate: a builder does not call scripts/fuzz/build_harnesses.sh")
  endif()
endforeach()
if(NOT EXISTS "${SRC}/scripts/fuzz/build_harnesses.sh")
  message(
    FATAL_ERROR "ossfuzz gate: scripts/fuzz/build_harnesses.sh is missing")
endif()
