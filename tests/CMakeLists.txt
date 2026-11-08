add_executable(rsma-firmware-tests firmware_tests.cpp)
target_link_libraries(rsma-firmware-tests PRIVATE rsma_mcu_core)

if(MSVC)
    target_compile_options(rsma-firmware-tests PRIVATE /W4 /permissive-)
else()
    target_compile_options(rsma-firmware-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME rsma-firmware-tests COMMAND rsma-firmware-tests "${CMAKE_CURRENT_BINARY_DIR}/vectors")
