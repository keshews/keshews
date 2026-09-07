cmake_minimum_required(VERSION 3.20)
project(keshews)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_executable(keshews
    src/main.cpp
    src/lexer.cpp
    src/parser.cpp
    src/ast.cpp
)

target_include_directories( keshews PRIVATE include)
