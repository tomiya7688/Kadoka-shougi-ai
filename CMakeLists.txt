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
    engine/src/impasse.cpp
    engine/src/legal.cpp
    engine/src/movegen.cpp
    engine/src/position.cpp
    engine/src/repetition.cpp
    engine/src/terminal.cpp
)

add_dependencies(kadoka_shogi_core kadoka_rule_check)

target_include_directories(kadoka_shogi_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

add_library(kadoka_shogi_runtime
    runtime/src/ai_backend.cpp
    runtime/src/headless_match.cpp
    runtime/src/match_clock.cpp
    runtime/src/process_ai_backend.cpp
    runtime/src/replay_series.cpp
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

add_executable(repetition_test tests/repetition_test.cpp)
target_link_libraries(repetition_test PRIVATE kadoka_shogi_core)
add_test(NAME repetition_test COMMAND repetition_test)

add_executable(terminal_test tests/terminal_test.cpp)
target_link_libraries(terminal_test PRIVATE kadoka_shogi_core)
add_test(NAME terminal_test COMMAND terminal_test)

add_executable(impasse_test tests/impasse_test.cpp)
target_link_libraries(impasse_test PRIVATE kadoka_shogi_core)
add_test(NAME impasse_test COMMAND impasse_test)

add_executable(runtime_turn_runner_test tests/runtime_turn_runner_test.cpp)
target_link_libraries(runtime_turn_runner_test PRIVATE kadoka_shogi_runtime)
add_test(NAME runtime_turn_runner_test COMMAND runtime_turn_runner_test)

add_executable(semantic_action_test tests/semantic_action_test.cpp)
target_link_libraries(semantic_action_test PRIVATE kadoka_shogi_runtime)
add_test(NAME semantic_action_test COMMAND semantic_action_test)

add_executable(headless_match_test tests/headless_match_test.cpp)
target_link_libraries(headless_match_test PRIVATE kadoka_shogi_runtime)
add_test(NAME headless_match_test COMMAND headless_match_test)

add_executable(replay_series_test tests/replay_series_test.cpp)
target_link_libraries(replay_series_test PRIVATE kadoka_shogi_runtime)
add_test(NAME replay_series_test COMMAND replay_series_test)

add_executable(match_clock_test tests/match_clock_test.cpp)
target_link_libraries(match_clock_test PRIVATE kadoka_shogi_runtime)
add_test(NAME match_clock_test COMMAND match_clock_test)

add_executable(time_forfeit_test tests/time_forfeit_test.cpp)
target_link_libraries(time_forfeit_test PRIVATE kadoka_shogi_runtime)
add_test(NAME time_forfeit_test COMMAND time_forfeit_test)

add_executable(impasse_outcome_test tests/impasse_outcome_test.cpp)
target_link_libraries(impasse_outcome_test PRIVATE kadoka_shogi_runtime)
add_test(NAME impasse_outcome_test COMMAND impasse_outcome_test)

add_executable(headless_impasse_test tests/headless_impasse_test.cpp)
target_link_libraries(headless_impasse_test PRIVATE kadoka_shogi_runtime)
add_test(NAME headless_impasse_test COMMAND headless_impasse_test)

add_executable(process_ai_backend_helper tests/process_ai_backend_helper.cpp)

add_executable(process_ai_backend_test tests/process_ai_backend_test.cpp)
target_link_libraries(process_ai_backend_test PRIVATE kadoka_shogi_runtime)
add_test(
    NAME process_ai_backend_test
    COMMAND process_ai_backend_test
        $<TARGET_FILE:process_ai_backend_helper>
        ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/process_ai_backend_helper.py
)
