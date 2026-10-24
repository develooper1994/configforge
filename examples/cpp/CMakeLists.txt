# The C++ examples, through the wrapper (configforge.hpp).
# Built as C++98 on purpose: the wrapper claims C++03 and a claim the
# build does not check is a claim that decays.
add_executable(tier1_cpp tier1.cpp)
target_link_libraries(tier1_cpp PRIVATE configforge)
set_target_properties(
  tier1_cpp
  PROPERTIES CXX_STANDARD 98
             CXX_STANDARD_REQUIRED ON
             CXX_EXTENSIONS OFF)
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
  target_compile_options(tier1_cpp PRIVATE -pedantic -Werror -Wall -Wextra)
endif()
configforge_apply_analysis(tier1_cpp)
# It writes the configuration back out, which needs the emitter. Built
# either way, so the example compiles against every build.
if(CF_WITH_EMIT)
  add_test(NAME example.tier1_cpp COMMAND tier1_cpp)
endif()
