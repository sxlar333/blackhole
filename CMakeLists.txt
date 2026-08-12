cmake_minimum_required(VERSION 3.20)

project(blackhole LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SFML 3 REQUIRED COMPONENTS Graphics Window System)

add_executable(blackhole
    main.cpp
)

target_link_libraries(blackhole PRIVATE
    SFML::Graphics
    SFML::Window
    SFML::System
)

add_custom_command(
    TARGET blackhole POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/liberation-mono.ttf"
        "$<TARGET_FILE_DIR:blackhole>/assets/liberation-mono.ttf"
    VERBATIM
)
