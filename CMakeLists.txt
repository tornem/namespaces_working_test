cmake_minimum_required(VERSION 3.0.0)
project(netns_switcher VERSION 0.1.0)

include(CTest)
enable_testing()

add_executable(${PROJECT_NAME} main.c utilitis.c)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wpedantic -Werror)
