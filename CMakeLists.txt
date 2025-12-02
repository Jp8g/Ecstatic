cmake_minimum_required(VERSION 3.10)

project(ECSTATIC C)
set(CMAKE_C_STANDARD 99)

add_library(ecstatic STATIC src/ecstatic.c)

add_compile_options(-mbmi2)

target_include_directories(ecstatic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)