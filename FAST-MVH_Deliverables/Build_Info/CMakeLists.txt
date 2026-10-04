cmake_minimum_required(VERSION 3.20)
project(FastMVH_KDT LANGUAGES CXX)

# Enforce C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Optimization flags: -O2 / /O2
if(MSVC)
    add_compile_options(/O2 /W4 /permissive-)
else()
    add_compile_options(-O2 -Wall -Wextra -Wpedantic)
endif()

# Global include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/baselines/bridging-mvh-dr/include
)

# Core library for Fast MVH with K-d Tree Dominance Checking
add_library(fast_mvh_lib
    src/src/mvh_kdtree.cpp
    src/src/solvers/l_namoa_kdt_chooseh.cpp
    src/src/solvers/l_namoa_dr_mvh_kdt.cpp
    baselines/bridging-mvh-dr/src/parser.cpp
    baselines/bridging-mvh-dr/src/parsers/multi_valued_heuristic_parser.cpp
    baselines/bridging-mvh-dr/src/data_structures/adjacency_matrix.cpp
    baselines/bridging-mvh-dr/src/data_structures/apex_path_pair.cpp
    baselines/bridging-mvh-dr/src/data_structures/map_queue.cpp
    baselines/bridging-mvh-dr/src/data_structures/node.cpp
    baselines/bridging-mvh-dr/src/multivalued_heuristic/apex_mvh.cpp
    baselines/bridging-mvh-dr/src/solvers/apex.cpp
)

target_include_directories(fast_mvh_lib PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/include
    ${CMAKE_CURRENT_SOURCE_DIR}/baselines/bridging-mvh-dr/include
)

# Main executable skeleton
add_executable(fast_mvh
    src/src/main.cpp
)

target_link_libraries(fast_mvh PRIVATE fast_mvh_lib)

find_package(Boost COMPONENTS program_options log thread filesystem system QUIET)
if(Boost_FOUND)
    target_include_directories(fast_mvh_lib PUBLIC ${Boost_INCLUDE_DIRS})
    target_link_libraries(fast_mvh_lib PUBLIC ${Boost_LIBRARIES})
    target_link_libraries(fast_mvh PRIVATE ${Boost_LIBRARIES})
endif()

# Baseline inclusion (guarded by Boost availability)
option(BUILD_BASELINES "Build baseline solvers from baselines/bridging-mvh-dr" ON)

if(BUILD_BASELINES AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/baselines/bridging-mvh-dr/CMakeLists.txt")
    if(Boost_FOUND)
        message(STATUS "Boost found. Adding baselines/bridging-mvh-dr subdirectory.")
        if(MSVC)
            add_compile_options(/FIstring)
        endif()
        add_subdirectory(baselines/bridging-mvh-dr)
    else()
        message(STATUS "Boost not found; baseline subdirectory is present as a read-only oracle.")
    endif()
endif()
