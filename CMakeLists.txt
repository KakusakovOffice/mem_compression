cmake_minimum_required(VERSION 3.21)

set(CMAKE_CXX_STANDARD 11)

project(
    mem_compresiion
    LANGUAGES CXX
)

add_executable(mem_compression src/main.cpp)

add_compile_options(-fsanitize=address)
add_link_options(-fsanitize=address)
