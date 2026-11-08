cmake_minimum_required(VERSION 3.20)

project(RSMA_MCU
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RSMA_MCU_BUILD_TOOLS "Build command-line tools" ON)
option(RSMA_MCU_BUILD_TESTS "Build tests" OFF)

add_library(rsma_mcu_core INTERFACE)
target_include_directories(rsma_mcu_core
    INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(RSMA_MCU_BUILD_TOOLS)
    add_executable(rsma-mcu-info tools/rsma_mcu_info.cpp)
    target_link_libraries(rsma-mcu-info PRIVATE rsma_mcu_core)

    if(MSVC)
        target_compile_options(rsma-mcu-info PRIVATE /W4 /permissive-)
    else()
        target_compile_options(rsma-mcu-info PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(RSMA_MCU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
