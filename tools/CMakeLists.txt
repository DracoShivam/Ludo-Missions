cmake_minimum_required(VERSION 3.20)
project(lm_tools CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(SRC_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../Source)
file(GLOB_RECURSE LM_LOGIC_SRC CONFIGURE_DEPENDS
     ${SRC_ROOT}/Models/*.cpp ${SRC_ROOT}/Controllers/Logic/*.cpp ${SRC_ROOT}/Utils/*.cpp)
add_executable(lm_trace ${CMAKE_CURRENT_SOURCE_DIR}/trace_match.cpp ${LM_LOGIC_SRC})
target_include_directories(lm_trace PRIVATE ${SRC_ROOT} ${CMAKE_CURRENT_SOURCE_DIR}/../axmol/3rdparty)
