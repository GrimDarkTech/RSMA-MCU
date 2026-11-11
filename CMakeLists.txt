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
option(RSMA_MCU_BUILD_BUILDER "Build the Clang-based rsma-build frontend" OFF)

add_library(rsma_mcu_core STATIC
    src/firmware_format.cpp
    src/firmware_validator.cpp
)
target_include_directories(rsma_mcu_core
    PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(rsma_mcu_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(rsma_mcu_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(RSMA_MCU_BUILD_TOOLS)
    add_executable(rsma-mcu-info tools/rsma_mcu_info.cpp)
    target_link_libraries(rsma-mcu-info PRIVATE rsma_mcu_core)

    add_executable(rsma-firmware-inspect tools/rsma_firmware_inspect.cpp)
    target_link_libraries(rsma-firmware-inspect PRIVATE rsma_mcu_core)

    add_executable(rsma-pwm-motor-test examples/pwm_motor_test.cpp)
    target_link_libraries(rsma-pwm-motor-test PRIVATE rsma_mcu_core)

    add_executable(rsma-stepper-motor-test examples/stepper_motor_test.cpp)
    target_link_libraries(rsma-stepper-motor-test PRIVATE rsma_mcu_core)

    add_executable(rsma-uart-echo examples/uart_echo.cpp)
    target_link_libraries(rsma-uart-echo PRIVATE rsma_mcu_core)

    if(MSVC)
        target_compile_options(rsma-mcu-info PRIVATE /W4 /permissive-)
        target_compile_options(rsma-firmware-inspect PRIVATE /W4 /permissive-)
        target_compile_options(rsma-pwm-motor-test PRIVATE /W4 /permissive-)
        target_compile_options(rsma-stepper-motor-test PRIVATE /W4 /permissive-)
        target_compile_options(rsma-uart-echo PRIVATE /W4 /permissive-)
    else()
        target_compile_options(rsma-mcu-info PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(rsma-firmware-inspect PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(rsma-pwm-motor-test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(rsma-stepper-motor-test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(rsma-uart-echo PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(RSMA_MCU_BUILD_BUILDER)
    find_path(RSMA_MCU_LIBCLANG_INCLUDE_DIR
        NAMES clang-c/Index.h
        HINTS "${LLVM_INSTALL_PREFIX}" "$ENV{LLVM_INSTALL_PREFIX}" "C:/Program Files/LLVM"
        PATH_SUFFIXES include
    )
    find_library(RSMA_MCU_LIBCLANG_LIBRARY
        NAMES libclang clang
        HINTS "${LLVM_INSTALL_PREFIX}" "$ENV{LLVM_INSTALL_PREFIX}" "C:/Program Files/LLVM"
        PATH_SUFFIXES lib
    )
    if(NOT RSMA_MCU_LIBCLANG_INCLUDE_DIR OR NOT RSMA_MCU_LIBCLANG_LIBRARY)
        message(FATAL_ERROR
            "rsma-build requires libclang development files. Install the bundled LLVM/Clang toolchain "
            "or set LLVM_INSTALL_PREFIX to its installation directory.")
    endif()

    add_executable(rsma-build tools/rsma_build.cpp)
    target_include_directories(rsma-build PRIVATE "${RSMA_MCU_LIBCLANG_INCLUDE_DIR}")
    target_compile_definitions(rsma-build PRIVATE
        RSMA_MCU_INCLUDE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/include"
        RSMA_MCU_ARDUINO_HEADER="${CMAKE_CURRENT_SOURCE_DIR}/include/rsma_mcu/Arduino.h"
    )
    target_link_libraries(rsma-build PRIVATE rsma_mcu_core "${RSMA_MCU_LIBCLANG_LIBRARY}")
    if(MSVC)
        target_compile_options(rsma-build PRIVATE /W4 /permissive-)
    else()
        target_compile_options(rsma-build PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    if(RSMA_MCU_BUILD_TESTS)
        if(RSMA_MCU_BUILD_TOOLS)
            add_test(NAME rsma-build-cli-tests
                COMMAND "${CMAKE_COMMAND}"
                    "-DRSMA_BUILD=$<TARGET_FILE:rsma-build>"
                    "-DRSMA_INSPECT=$<TARGET_FILE:rsma-firmware-inspect>"
                    "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
                    "-DOUTPUT_DIR=${CMAKE_CURRENT_BINARY_DIR}/builder-vectors"
                    -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/rsma_build_tests.cmake"
            )
        endif()
    endif()

    if(WIN32)
        get_filename_component(RSMA_MCU_LIBCLANG_RUNTIME_DIR "${RSMA_MCU_LIBCLANG_LIBRARY}" DIRECTORY)
        add_custom_command(TARGET rsma-build POST_BUILD
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${RSMA_MCU_LIBCLANG_RUNTIME_DIR}/../bin/libclang.dll"
                "$<TARGET_FILE_DIR:rsma-build>/libclang.dll"
            VERBATIM
        )
    endif()
endif()

if(RSMA_MCU_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
