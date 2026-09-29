cmake_minimum_required(VERSION 2.8.3)
project(emoa)

# find_package(Boost REQUIRED)

set(CMAKE_CXX_FLAGS "-std=c++0x ${CMAKE_CXX_FLAGS}")

file(GLOB_RECURSE ALL_HDRS "include/*.hpp")
file(GLOB_RECURSE ALL_SRCS "source/*.cpp")

include_directories(DIRECTORY
  include/${PROJECT_NAME}
  include
  third_party
)

add_library(${PROJECT_NAME} SHARED
  ${ALL_SRCS}
)

# variant_factory.cpp instantiates the full backend x store x cmp x dim
# template matrix; under MinGW's unoptimized (no CMAKE_BUILD_TYPE) COFF
# objects this exceeds the 32-bit object format's section-count limit
# ("too many sections" / "file too big"). -mbig-obj switches the assembler
# to the bigobj COFF variant; it changes no semantics or optimization.
if (MINGW)
  set_source_files_properties(source/bench/variant_factory.cpp PROPERTIES
    COMPILE_OPTIONS "-Wa,-mbig-obj")
endif()

# nanoflann 1.10 requires C++17 (uses `if constexpr`); the project builds at
# C++11. Scope C++17 to just the cross-check TU (the last -std on the command
# line wins under g++). The vendored header is only included here.
set_source_files_properties(test/nanoflann_xcheck.cpp PROPERTIES
  COMPILE_OPTIONS "-std=c++17")

set(test_cpp_dir "test/")
set(test_cpp_files "")
file(GLOB_RECURSE test_cpp_files "${test_cpp_dir}/*.cpp")
foreach(test_cpp_file ${test_cpp_files})
  get_filename_component(test_cpp_name ${test_cpp_file} NAME_WE)
  #ADD_EXECUTABLE(${test_cpp_name} ${ALL_SRCS} ${test_cpp_dir}/${test_cpp_name}.cpp)
  ADD_EXECUTABLE(${test_cpp_name} ${test_cpp_dir}/${test_cpp_name}.cpp)
  TARGET_LINK_LIBRARIES(${test_cpp_name} 
    ${PROJECT_NAME} 
  )
endforeach(test_cpp_file ${test_cpp_files})
