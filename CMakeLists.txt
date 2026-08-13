cmake_minimum_required(VERSION 3.20)

project(blackhole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

function(add_blackhole_target name source)
    add_executable(${name} ${source})

    target_link_libraries(${name} PRIVATE
        SFML::Graphics
        SFML::Window
        SFML::System
    )

    add_custom_command(
        TARGET ${name} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/assets/liberation-mono.ttf"
            "$<TARGET_FILE_DIR:${name}>/assets/liberation-mono.ttf"
        VERBATIM
    )
endfunction()

add_blackhole_target(blackhole main.cpp)
add_blackhole_target(benchmark benchmark.cpp)
add_blackhole_target(nbody nbody.cpp)
add_blackhole_target(barnes_hut barnes_hut.cpp)
