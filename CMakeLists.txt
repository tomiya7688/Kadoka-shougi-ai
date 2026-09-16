cmake_minimum_required(VERSION 3.20)
project(kadoka_shougi_ai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Python3 COMPONENTS Interpreter REQUIRED)

add_custom_target(kadoka_rule_check
    COMMAND
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/kadoka_rule_checker/script/kadoka_rule_checker.py
        ${CMAKE_CURRENT_SOURCE_DIR}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking Kadoka Shougi dependency rules"
    VERBATIM
)

add_library(kadoka_shogi_core
    engine/src/attack.cpp
    engine/src/legal.cpp
    engine/src/movegen.cpp
    engine/src/position.cpp
)

add_dependencies(kadoka_shogi_core kadoka_rule_check)

target_include_directories(kadoka_shogi_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

add_library(kadoka_shogi_runtime
    runtime/src/turn_runner.cpp
)

add_dependencies(kadoka_shogi_runtime kadoka_rule_check)

target_include_directories(kadoka_shogi_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/runtime/include
)

target_link_libraries(kadoka_shogi_runtime
    PUBLIC
        kadoka_shogi_core
)

add_executable(kadoka_shogi
    protocol/cli/main.cpp
)

target_link_libraries(kadoka_shogi PRIVATE kadoka_shogi_core)

enable_testing()

add_executable(core_smoke_test tests/core_smoke_test.cpp)
target_link_libraries(core_smoke_test PRIVATE kadoka_shogi_core)
add_test(NAME core_smoke_test COMMAND core_smoke_test)

add_executable(movegen_test tests/movegen_test.cpp)
target_link_libraries(movegen_test PRIVATE kadoka_shogi_core)
add_test(NAME movegen_test COMMAND movegen_test)

add_executable(legal_move_test tests/legal_move_test.cpp)
target_link_libraries(legal_move_test PRIVATE kadoka_shogi_core)
add_test(NAME legal_move_test COMMAND legal_move_test)

add_executable(runtime_turn_runner_test tests/runtime_turn_runner_test.cpp)
target_link_libraries(runtime_turn_runner_test PRIVATE kadoka_shogi_runtime)
add_test(NAME runtime_turn_runner_test COMMAND runtime_turn_runner_test)
