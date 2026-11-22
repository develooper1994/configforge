# The libraries the comparison reads the same documents through, each pinned
# by the commit that is fetched (D-111). Read by scripts/bench/fetch.cmake and
# by bench/compare/CMakeLists.txt, and by scripts/bench/page.py for the page.
#
# A release tag is named where there is one, and the commit is what is
# fetched; they were checked against each other with `git ls-remote --tags`
# (the peeled ^{} line of an annotated tag). A library with no usable release
# is pinned at a commit of its default branch at least 30 days old, with its
# date. Never a branch name, never "latest". A version changes only by a commit
# that also reruns the baseline (bench/baseline/instructions.json).
#
# Chosen on 2026-11-22: the newest release tag at least 30 days old.
#
# expat is not here: it is the system's (libexpat1-dev), the one ConfigForge's
# XML reader links too, so the two XML rows run one library; its version is
# recorded in the results.
#
# c4core is not here either: rapidyaml 0.16.0 carries it inside its own tree
# (ext/c4core, no submodule), so the one commit below is both.

set(CFB_LIB_NAMES
    yyjson
    simdjson
    rapidjson
    cjson
    libyaml
    rapidyaml
    tomlc17
    tomlpp
    tinycbor
    inih)

set(LIB_yyjson_URL "https://github.com/ibireme/yyjson")
set(LIB_yyjson_TAG "0.13.0")
set(LIB_yyjson_COMMIT "6447536015f3d600f3d65323b10976103b337ca7")
set(LIB_yyjson_DATE "2026-09-08")
set(LIB_yyjson_LICENSE "MIT")

set(LIB_simdjson_URL "https://github.com/simdjson/simdjson")
set(LIB_simdjson_TAG "v5.0.5")
set(LIB_simdjson_COMMIT "9c003534b1be3bc4f1322bfa7a770a7ffbf247d9")
set(LIB_simdjson_DATE "2026-10-14")
set(LIB_simdjson_LICENSE "Apache-2.0 OR MIT")

# RapidJSON's last release is a 2016 tag that does not build with today's
# compilers: the default branch, master, at its newest commit 30 days old.
set(LIB_rapidjson_URL "https://github.com/Tencent/rapidjson")
set(LIB_rapidjson_TAG "")
set(LIB_rapidjson_COMMIT "24b5e7a8b27f42fa16b96fc70aade9106cf7102f")
set(LIB_rapidjson_DATE "2025-02-05")
set(LIB_rapidjson_LICENSE "MIT")

set(LIB_cjson_URL "https://github.com/DaveGamble/cJSON")
set(LIB_cjson_TAG "v1.7.19")
set(LIB_cjson_COMMIT "c859b25da02955fef659d658b8f324b5cde87be3")
set(LIB_cjson_DATE "2025-09-09")
set(LIB_cjson_LICENSE "MIT")

# 0.2.6 (2026-11-20) is two days old, and 0.2.6-rc.1 a release candidate.
set(LIB_libyaml_URL "https://github.com/yaml/libyaml")
set(LIB_libyaml_TAG "0.2.5")
set(LIB_libyaml_COMMIT "2c891fc7a770e8ba2fec34fc6b545c672beb37e6")
set(LIB_libyaml_DATE "2020-06-01")
set(LIB_libyaml_LICENSE "MIT")

set(LIB_rapidyaml_URL "https://github.com/biojppm/rapidyaml")
set(LIB_rapidyaml_TAG "v0.16.0")
set(LIB_rapidyaml_COMMIT "f8ac8dd50f4f7916579d55a05ebf9c6488e52670")
set(LIB_rapidyaml_DATE "2026-07-21")
set(LIB_rapidyaml_LICENSE "MIT")

# tomlc17 has no release tag: master (main), 2026-10-22.
set(LIB_tomlc17_URL "https://github.com/cktan/tomlc17")
set(LIB_tomlc17_TAG "")
set(LIB_tomlc17_COMMIT "4987cf895df61f702d19e3481f34b032ab29ed3a")
set(LIB_tomlc17_DATE "2026-10-22")
set(LIB_tomlc17_LICENSE "MIT")

# v3.5.0 (2026-11-22) is a day old.
set(LIB_tomlpp_URL "https://github.com/marzer/tomlplusplus")
set(LIB_tomlpp_TAG "v3.4.0")
set(LIB_tomlpp_COMMIT "30172438cee64926dc41fdd9c11fb3ba5b2ba9de")
set(LIB_tomlpp_DATE "2023-10-13")
set(LIB_tomlpp_LICENSE "MIT")

set(LIB_tinycbor_URL "https://github.com/intel/tinycbor")
set(LIB_tinycbor_TAG "v7.1")
set(LIB_tinycbor_COMMIT "da6421f2b5b91e3468c0af7265117427ea73f19e")
set(LIB_tinycbor_DATE "2026-10-23")
set(LIB_tinycbor_LICENSE "MIT")

# r63 (2026-11-01) is 21 days old.
set(LIB_inih_URL "https://github.com/benhoyt/inih")
set(LIB_inih_TAG "r62")
set(LIB_inih_COMMIT "26254ee9de7681f8825433415443e7116ff24b98")
set(LIB_inih_DATE "2025-09-11")
set(LIB_inih_LICENSE "BSD-3-Clause")
