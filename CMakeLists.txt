cmake_minimum_required(VERSION 3.20)
project(kadoka_shougi_ai LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(kadoka_shogi_core
    engine/src/movegen.cpp
    engine/src/position.cpp
)

target_include_directories(kadoka_shogi_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

add_executable(kadoka_shogi
    protocol/cli/main.cpp
)

target_link_libraries(kadoka_shogi PRIVATE kadoka_shogi_core)

enable_testing()

add_executable(core_smoke_test
    tests/core_smoke_test.cpp
)
target_link_libraries(core_smoke_test PRIVATE kadoka_shogi_core)
add_test(NAME core_smoke_test COMMAND core_smoke_test)

add_executable(movegen_test
    tests/movegen_test.cpp
)
target_link_libraries(movegen_test PRIVATE kadoka_shogi_core)
add_test(NAME movegen_test COMMAND movegen_test)
