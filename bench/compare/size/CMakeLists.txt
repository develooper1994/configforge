# What a library adds to a program (D-111): per adapter, a program that parses
# one fixed 64-byte document and touches it, compiled with the same flags, in
# sections the linker drops (-ffunction-sections -fdata-sections
# -Wl,--gc-sections), stripped, linked dynamically to the system's C and C++
# libraries. A row is its file size less that of the same program with an
# empty adapter. Read by scripts/bench/run.py.
set(size_flags -Wl,--gc-sections -Wl,-s)

add_executable(size_empty main.cpp empty.cpp ../registry.cpp)
target_include_directories(size_empty PRIVATE ..)
cfb_flags(size_empty)
target_compile_options(size_empty PRIVATE -ffunction-sections -fdata-sections)
target_link_options(size_empty PRIVATE ${size_flags})

foreach(id ${CFB_ADAPTER_IDS})
  add_executable(size_${id} main.cpp ../registry.cpp
                            $<TARGET_OBJECTS:sizead_${id}>)
  target_include_directories(size_${id} PRIVATE ..)
  cfb_flags(size_${id})
  target_compile_options(size_${id} PRIVATE -ffunction-sections -fdata-sections)
  target_link_options(size_${id} PRIVATE ${size_flags})
  foreach(l ${CFB_ADAPTER_${id}_LIBS})
    target_link_libraries(size_${id} PRIVATE sizelib_${l})
  endforeach()
  target_link_libraries(size_${id} PRIVATE ${CFB_ADAPTER_${id}_SYSTEM})
endforeach()
