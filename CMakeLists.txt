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
    runtime/src/game_history.cpp
    runtime/src/headless_match.cpp
    runtime/src/match_clock.cpp
    runtime/src/player_api.cpp
    runtime/src/process_ai_backend.cpp
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

add_library(kadoka_shogi_ai_package
    ai_package/src/board_recognition.cpp
    ai_package/src/json_player_observation_parser.cpp
    ai_package/src/observation.cpp
)

add_dependencies(kadoka_shogi_ai_package kadoka_rule_check)

target_include_directories(kadoka_shogi_ai_package
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/ai_package/include
)

target_link_libraries(kadoka_shogi_ai_package
    PUBLIC
        kadoka_shogi_runtime
)

add_library(kadoka_shogi_training_parser
    tools/training_parser/src/core_history_parser.cpp
    tools/training_parser/src/parser.cpp
    tools/training_parser/src/recursive_importer.cpp
    tools/training_parser/src/sfen_lines_parser.cpp
)

add_dependencies(kadoka_shogi_training_parser kadoka_rule_check)

target_include_directories(kadoka_shogi_training_parser
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/tools/training_parser/include
)

target_link_libraries(kadoka_shogi_training_parser
    PUBLIC
        kadoka_shogi_runtime
)

add_executable(kadoka_shogi
    protocol/cli/main.cpp
)

target_link_libraries(kadoka_shogi PRIVATE kadoka_shogi_core)

add_executable(kadoka_wire_compat
    tools/wire_compat/main.cpp
)

target_link_libraries(
    kadoka_wire_compat
    PRIVATE
        kadoka_shogi_ai_package
)

install(
    TARGETS
        kadoka_shogi
        kadoka_wire_compat
    RUNTIME DESTINATION bin
)

install(
    DIRECTORY tests/fixtures/wire_v1/
    DESTINATION share/kadoka/wire_v1
)

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

add_executable(player_api_test tests/player_api_test.cpp)
target_link_libraries(player_api_test PRIVATE kadoka_shogi_runtime)
add_test(NAME player_api_test COMMAND player_api_test)

add_executable(
    ai_package_json_observation_parser_test
    tests/ai_package_json_observation_parser_test.cpp
)
target_link_libraries(
    ai_package_json_observation_parser_test
    PRIVATE kadoka_shogi_ai_package
)
add_test(
    NAME ai_package_json_observation_parser_test
    COMMAND ai_package_json_observation_parser_test
)

add_executable(
    board_recognition_adapter_test
    tests/board_recognition_adapter_test.cpp
)
target_link_libraries(
    board_recognition_adapter_test
    PRIVATE kadoka_shogi_ai_package
)
add_test(
    NAME board_recognition_adapter_test
    COMMAND board_recognition_adapter_test
)

add_executable(game_history_test tests/game_history_test.cpp)
target_link_libraries(game_history_test PRIVATE kadoka_shogi_runtime)
add_test(NAME game_history_test COMMAND game_history_test)

add_executable(training_parser_test tests/training_parser_test.cpp)
target_link_libraries(
    training_parser_test
    PRIVATE kadoka_shogi_training_parser
)
add_test(NAME training_parser_test COMMAND training_parser_test)

add_executable(
    recursive_training_importer_test
    tests/recursive_training_importer_test.cpp
)
target_link_libraries(
    recursive_training_importer_test
    PRIVATE kadoka_shogi_training_parser
)
add_test(
    NAME recursive_training_importer_test
    COMMAND recursive_training_importer_test
)

add_executable(semantic_action_test tests/semantic_action_test.cpp)
target_link_libraries(semantic_action_test PRIVATE kadoka_shogi_runtime)
add_test(NAME semantic_action_test COMMAND semantic_action_test)

add_executable(headless_match_test tests/headless_match_test.cpp)
target_link_libraries(headless_match_test PRIVATE kadoka_shogi_runtime)
add_test(NAME headless_match_test COMMAND headless_match_test)

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
