cmake_minimum_required(VERSION 3.28)
project(apex_builder)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS program_options log thread filesystem system)

include_directories(${Boost_INCLUDE_DIRS})
include_directories("../baselines/bridging-mvh-dr/include")
include_directories("../baselines/bridging-mvh-dr/src")

add_executable(apex_builder 
    apex_builder_main.cpp
    ../baselines/bridging-mvh-dr/src/parser.cpp
    ../baselines/bridging-mvh-dr/src/data_structures/adjacency_matrix.cpp
    ../baselines/bridging-mvh-dr/src/data_structures/node.cpp
    ../baselines/bridging-mvh-dr/src/multivalued_heuristic/apex_mvh.cpp
    ../baselines/bridging-mvh-dr/src/solvers/apex.cpp
    ../baselines/bridging-mvh-dr/src/data_structures/apex_path_pair.cpp
)
target_link_libraries(apex_builder ${Boost_LIBRARIES})
