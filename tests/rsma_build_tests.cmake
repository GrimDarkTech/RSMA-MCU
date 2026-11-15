file(REMOVE_RECURSE "${OUTPUT_DIR}")
file(MAKE_DIRECTORY "${OUTPUT_DIR}")

set(valid_output "${OUTPUT_DIR}/blink.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/blink_main.cpp" -o "${valid_output}"
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_stdout
    ERROR_VARIABLE build_stderr
)
if(NOT build_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected Blink fixture:\n${build_stdout}\n${build_stderr}")
endif()
if(NOT EXISTS "${valid_output}")
    message(FATAL_ERROR "rsma-build did not create the firmware package")
endif()

execute_process(
    COMMAND "${RSMA_INSPECT}" "${valid_output}"
    RESULT_VARIABLE inspect_result
    OUTPUT_VARIABLE inspect_stdout
    ERROR_VARIABLE inspect_stderr
)
if(NOT inspect_result EQUAL 0)
    message(FATAL_ERROR "generated firmware failed validation:\n${inspect_stdout}\n${inspect_stderr}")
endif()
foreach(required_text "Entry: main (instruction 0)" "Instructions: 5" "HAL_CALL" "WAIT" "HALT" "Peripherals: GPIO SYSTICK")
    string(FIND "${inspect_stdout}" "${required_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "generated firmware inspection is missing '${required_text}':\n${inspect_stdout}")
    endif()
endforeach()

set(eeprom_output "${OUTPUT_DIR}/eeprom-storage.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/eeprom_storage.cpp" -o "${eeprom_output}"
    RESULT_VARIABLE eeprom_result
    OUTPUT_VARIABLE eeprom_stdout
    ERROR_VARIABLE eeprom_stderr
)
if(NOT eeprom_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected EEPROM storage fixture:\n${eeprom_stdout}\n${eeprom_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${eeprom_output}"
    RESULT_VARIABLE eeprom_inspect_result
    OUTPUT_VARIABLE eeprom_inspect_stdout
    ERROR_VARIABLE eeprom_inspect_stderr
)
if(NOT eeprom_inspect_result EQUAL 0)
    message(FATAL_ERROR "EEPROM storage firmware is invalid:\n${eeprom_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "HALT" "Peripherals: EEPROM")
    string(FIND "${eeprom_inspect_stdout}" "${required_text}" eeprom_text_index)
    if(eeprom_text_index EQUAL -1)
        message(FATAL_ERROR "EEPROM storage firmware is missing '${required_text}':\n${eeprom_inspect_stdout}")
    endif()
endforeach()

set(yield_output "${OUTPUT_DIR}/yield.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/yield.cpp" -o "${yield_output}"
    RESULT_VARIABLE yield_result
    OUTPUT_VARIABLE yield_stdout
    ERROR_VARIABLE yield_stderr
)
if(NOT yield_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected yield():\n${yield_stdout}\n${yield_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${yield_output}"
    RESULT_VARIABLE yield_inspect_result
    OUTPUT_VARIABLE yield_inspect_stdout
    ERROR_VARIABLE yield_inspect_stderr
)
if(NOT yield_inspect_result EQUAL 0)
    message(FATAL_ERROR "yield firmware is invalid:\n${yield_inspect_stderr}")
endif()
string(FIND "${yield_inspect_stdout}" "YIELD (0 operand bytes)" yield_opcode_index)
if(yield_opcode_index EQUAL -1)
    message(FATAL_ERROR "yield firmware does not contain YIELD:\n${yield_inspect_stdout}")
endif()

set(analog_output "${OUTPUT_DIR}/analog-read.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/analog_read.cpp" -o "${analog_output}"
    RESULT_VARIABLE analog_result
    OUTPUT_VARIABLE analog_stdout
    ERROR_VARIABLE analog_stderr
)
if(NOT analog_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected analogRead():\n${analog_stdout}\n${analog_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${analog_output}"
    RESULT_VARIABLE analog_inspect_result
    OUTPUT_VARIABLE analog_inspect_stdout
    ERROR_VARIABLE analog_inspect_stderr
)
if(NOT analog_inspect_result EQUAL 0)
    message(FATAL_ERROR "analogRead firmware is invalid:\n${analog_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "HALT" "Peripherals: ADC")
    string(FIND "${analog_inspect_stdout}" "${required_text}" analog_text_index)
    if(analog_text_index EQUAL -1)
        message(FATAL_ERROR "analogRead firmware is missing '${required_text}':\n${analog_inspect_stdout}")
    endif()
endforeach()

set(millis_output "${OUTPUT_DIR}/millis-control.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/millis_control.cpp" -o "${millis_output}"
    RESULT_VARIABLE millis_result
    OUTPUT_VARIABLE millis_stdout
    ERROR_VARIABLE millis_stderr
)
if(NOT millis_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected millis() fixture:\n${millis_stdout}\n${millis_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${millis_output}"
    RESULT_VARIABLE millis_inspect_result
    OUTPUT_VARIABLE millis_inspect_stdout
    ERROR_VARIABLE millis_inspect_stderr
)
if(NOT millis_inspect_result EQUAL 0)
    message(FATAL_ERROR "millis firmware is invalid:\n${millis_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "WAIT" "HALT" "Peripherals: GPIO SYSTICK")
    string(FIND "${millis_inspect_stdout}" "${required_text}" millis_text_index)
    if(millis_text_index EQUAL -1)
        message(FATAL_ERROR "millis firmware is missing '${required_text}':\n${millis_inspect_stdout}")
    endif()
endforeach()

set(stage73_output "${OUTPUT_DIR}/source-api-stage73.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/source_api_stage73.cpp" -o "${stage73_output}"
    RESULT_VARIABLE stage73_result
    OUTPUT_VARIABLE stage73_stdout
    ERROR_VARIABLE stage73_stderr
)
if(NOT stage73_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected stage 7.3 source API fixture:\n${stage73_stdout}\n${stage73_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${stage73_output}"
    RESULT_VARIABLE stage73_inspect_result
    OUTPUT_VARIABLE stage73_inspect_stdout
    ERROR_VARIABLE stage73_inspect_stderr
)
if(NOT stage73_inspect_result EQUAL 0)
    message(FATAL_ERROR "stage 7.3 source API firmware is invalid:\n${stage73_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "ATTACH_IRQ" "DETACH_IRQ" "Peripherals: GPIO IRQ PWM SYSTICK UART")
    string(FIND "${stage73_inspect_stdout}" "${required_text}" stage73_text_index)
    if(stage73_text_index EQUAL -1)
        message(FATAL_ERROR "stage 7.3 source API firmware is missing '${required_text}':\n${stage73_inspect_stdout}")
    endif()
endforeach()

set(interrupt_output "${OUTPUT_DIR}/source-interrupt.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/source_interrupt.cpp" -o "${interrupt_output}"
    RESULT_VARIABLE interrupt_result
    OUTPUT_VARIABLE interrupt_stdout
    ERROR_VARIABLE interrupt_stderr
)
if(NOT interrupt_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected source interrupt fixture:\n${interrupt_stdout}\n${interrupt_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${interrupt_output}"
    RESULT_VARIABLE interrupt_inspect_result
    OUTPUT_VARIABLE interrupt_inspect_stdout
    ERROR_VARIABLE interrupt_inspect_stderr
)
if(NOT interrupt_inspect_result EQUAL 0)
    message(FATAL_ERROR "source interrupt firmware is invalid:\n${interrupt_inspect_stderr}")
endif()
foreach(required_text "ATTACH_IRQ" "DETACH_IRQ" "ENABLE_IRQ" "RETURN_IRQ" "Peripherals: GPIO IRQ")
    string(FIND "${interrupt_inspect_stdout}" "${required_text}" interrupt_text_index)
    if(interrupt_text_index EQUAL -1)
        message(FATAL_ERROR "source interrupt firmware is missing '${required_text}':\n${interrupt_inspect_stdout}")
    endif()
endforeach()

set(serial_output "${OUTPUT_DIR}/serial-echo.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_echo.cpp" -o "${serial_output}"
    RESULT_VARIABLE serial_result
    OUTPUT_VARIABLE serial_stdout
    ERROR_VARIABLE serial_stderr
)
if(NOT serial_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected Serial echo fixture:\n${serial_stdout}\n${serial_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${serial_output}"
    RESULT_VARIABLE serial_inspect_result
    OUTPUT_VARIABLE serial_inspect_stdout
    ERROR_VARIABLE serial_inspect_stderr
)
if(NOT serial_inspect_result EQUAL 0)
    message(FATAL_ERROR "Serial echo firmware is invalid:\n${serial_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "WAIT" "HALT" "Peripherals: UART SYSTICK")
    string(FIND "${serial_inspect_stdout}" "${required_text}" serial_text_index)
    if(serial_text_index EQUAL -1)
        message(FATAL_ERROR "Serial echo firmware is missing '${required_text}':\n${serial_inspect_stdout}")
    endif()
endforeach()

set(serial_ports_output "${OUTPUT_DIR}/serial-ports.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_ports.cpp" -o "${serial_ports_output}"
    RESULT_VARIABLE serial_ports_result
    OUTPUT_VARIABLE serial_ports_stdout
    ERROR_VARIABLE serial_ports_stderr
)
if(NOT serial_ports_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected multi-port Serial fixture:\n${serial_ports_stdout}\n${serial_ports_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${serial_ports_output}"
    RESULT_VARIABLE serial_ports_inspect_result
    OUTPUT_VARIABLE serial_ports_inspect_stdout
    ERROR_VARIABLE serial_ports_inspect_stderr
)
if(NOT serial_ports_inspect_result EQUAL 0)
    message(FATAL_ERROR "multi-port Serial firmware is invalid:\n${serial_ports_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "WAIT" "HALT" "Peripherals: UART SYSTICK")
    string(FIND "${serial_ports_inspect_stdout}" "${required_text}" serial_ports_text_index)
    if(serial_ports_text_index EQUAL -1)
        message(FATAL_ERROR "multi-port Serial firmware is missing '${required_text}':\n${serial_ports_inspect_stdout}")
    endif()
endforeach()

set(serial_callback_output "${OUTPUT_DIR}/serial-callback.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_callback.cpp" -o "${serial_callback_output}"
    RESULT_VARIABLE serial_callback_result
    OUTPUT_VARIABLE serial_callback_stdout
    ERROR_VARIABLE serial_callback_stderr
)
if(NOT serial_callback_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected UART receive callback fixture:\n${serial_callback_stdout}\n${serial_callback_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${serial_callback_output}"
    RESULT_VARIABLE serial_callback_inspect_result
    OUTPUT_VARIABLE serial_callback_inspect_stdout
    ERROR_VARIABLE serial_callback_inspect_stderr
)
if(NOT serial_callback_inspect_result EQUAL 0)
    message(FATAL_ERROR "UART receive callback firmware is invalid:\n${serial_callback_inspect_stderr}")
endif()
foreach(required_text "ATTACH_UART_RX" "RETURN_UART_RX" "HAL_CALL" "WAIT" "HALT" "Peripherals: UART SYSTICK")
    string(FIND "${serial_callback_inspect_stdout}" "${required_text}" serial_callback_text_index)
    if(serial_callback_text_index EQUAL -1)
        message(FATAL_ERROR "UART receive callback firmware is missing '${required_text}':\n${serial_callback_inspect_stdout}")
    endif()
endforeach()

set(serial_packet_callback_output "${OUTPUT_DIR}/serial-packet-callback.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_packet_callback.cpp" -o "${serial_packet_callback_output}"
    RESULT_VARIABLE serial_packet_callback_result
    OUTPUT_VARIABLE serial_packet_callback_stdout
    ERROR_VARIABLE serial_packet_callback_stderr
)
if(NOT serial_packet_callback_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected UART packet callback fixture:\n${serial_packet_callback_stdout}\n${serial_packet_callback_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${serial_packet_callback_output}"
    RESULT_VARIABLE serial_packet_callback_inspect_result
    OUTPUT_VARIABLE serial_packet_callback_inspect_stdout
    ERROR_VARIABLE serial_packet_callback_inspect_stderr
)
if(NOT serial_packet_callback_inspect_result EQUAL 0)
    message(FATAL_ERROR "UART packet callback firmware is invalid:\n${serial_packet_callback_inspect_stderr}")
endif()
foreach(required_text "ATTACH_UART_PACKET_RX" "RETURN_UART_PACKET_RX" "HAL_CALL" "WAIT" "HALT" "Peripherals: UART SYSTICK")
    string(FIND "${serial_packet_callback_inspect_stdout}" "${required_text}" serial_packet_callback_text_index)
    if(serial_packet_callback_text_index EQUAL -1)
        message(FATAL_ERROR "UART packet callback firmware is missing '${required_text}':\n${serial_packet_callback_inspect_stdout}")
    endif()
endforeach()

set(analog_threshold_output "${OUTPUT_DIR}/analog-threshold-pwm.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/analog_threshold_pwm.cpp" -o "${analog_threshold_output}"
    RESULT_VARIABLE analog_threshold_result
    OUTPUT_VARIABLE analog_threshold_stdout
    ERROR_VARIABLE analog_threshold_stderr
)
if(NOT analog_threshold_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected analog threshold/PWM fixture:\n${analog_threshold_stdout}\n${analog_threshold_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${analog_threshold_output}"
    RESULT_VARIABLE analog_threshold_inspect_result
    OUTPUT_VARIABLE analog_threshold_inspect_stdout
    ERROR_VARIABLE analog_threshold_inspect_stderr
)
if(NOT analog_threshold_inspect_result EQUAL 0)
    message(FATAL_ERROR "analog threshold/PWM firmware is invalid:\n${analog_threshold_inspect_stderr}")
endif()
foreach(required_text "Instructions:" "HAL_CALL" "CMP_GE_I32" "JUMP_IF" "WAIT" "Peripherals: GPIO ADC PWM SYSTICK")
    string(FIND "${analog_threshold_inspect_stdout}" "${required_text}" analog_threshold_text_index)
    if(analog_threshold_text_index EQUAL -1)
        message(FATAL_ERROR "analog threshold/PWM firmware is missing '${required_text}':\n${analog_threshold_inspect_stdout}")
    endif()
endforeach()

set(bus_output "${OUTPUT_DIR}/bus-transfers.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/bus_transfers.cpp" -o "${bus_output}"
    RESULT_VARIABLE bus_result
    OUTPUT_VARIABLE bus_stdout
    ERROR_VARIABLE bus_stderr
)
if(NOT bus_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected SPI/I2C/DMA transfers:\n${bus_stdout}\n${bus_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${bus_output}"
    RESULT_VARIABLE bus_inspect_result
    OUTPUT_VARIABLE bus_inspect_stdout
    ERROR_VARIABLE bus_inspect_stderr
)
if(NOT bus_inspect_result EQUAL 0)
    message(FATAL_ERROR "SPI/I2C/DMA firmware is invalid:\n${bus_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "HALT" "Peripherals: SPI I2C DMA")
    string(FIND "${bus_inspect_stdout}" "${required_text}" bus_text_index)
    if(bus_text_index EQUAL -1)
        message(FATAL_ERROR "SPI/I2C/DMA firmware is missing '${required_text}':\n${bus_inspect_stdout}")
    endif()
endforeach()

set(async_dma_output "${OUTPUT_DIR}/async-dma.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/async_dma.cpp" -o "${async_dma_output}"
    RESULT_VARIABLE async_dma_result
    OUTPUT_VARIABLE async_dma_stdout
    ERROR_VARIABLE async_dma_stderr
)
if(NOT async_dma_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected asynchronous DMA fixture:\n${async_dma_stdout}\n${async_dma_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${async_dma_output}"
    RESULT_VARIABLE async_dma_inspect_result
    OUTPUT_VARIABLE async_dma_inspect_stdout
    ERROR_VARIABLE async_dma_inspect_stderr
)
if(NOT async_dma_inspect_result EQUAL 0)
    message(FATAL_ERROR "asynchronous DMA firmware is invalid:\n${async_dma_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "HALT" "Peripherals: DMA")
    string(FIND "${async_dma_inspect_stdout}" "${required_text}" async_dma_text_index)
    if(async_dma_text_index EQUAL -1)
        message(FATAL_ERROR "asynchronous DMA firmware is missing '${required_text}':\n${async_dma_inspect_stdout}")
    endif()
endforeach()

set(control_output "${OUTPUT_DIR}/constant-control-flow.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/constant_control_flow.cpp" -o "${control_output}"
    RESULT_VARIABLE control_result
    OUTPUT_VARIABLE control_stdout
    ERROR_VARIABLE control_stderr
)
if(NOT control_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected constant control flow:\n${control_stdout}\n${control_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${control_output}"
    RESULT_VARIABLE control_inspect_result
    OUTPUT_VARIABLE control_inspect_stdout
    ERROR_VARIABLE control_inspect_stderr
)
if(NOT control_inspect_result EQUAL 0)
    message(FATAL_ERROR "constant control-flow package failed validation:\n${control_inspect_stderr}")
endif()
foreach(required_text "HAL_CALL" "WAIT" "JUMP" "CMP_EQ_I32")
    string(FIND "${control_inspect_stdout}" "${required_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "constant-control-flow package is missing '${required_text}':\n${control_inspect_stdout}")
    endif()
endforeach()
string(FIND "${control_inspect_stdout}" "Peripherals: GPIO SYSTICK" peripheral_index)
if(peripheral_index EQUAL -1)
    message(FATAL_ERROR "constant-control-flow package did not declare used peripherals:\n${control_inspect_stdout}")
endif()

set(runtime_condition_output "${OUTPUT_DIR}/runtime-condition.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/runtime_condition.cpp" -o "${runtime_condition_output}"
    RESULT_VARIABLE condition_result
    OUTPUT_VARIABLE condition_stdout
    ERROR_VARIABLE condition_stderr
)
if(condition_result EQUAL 0)
    message(FATAL_ERROR "rsma-build unexpectedly accepted a runtime condition")
endif()
if(EXISTS "${runtime_condition_output}")
    message(FATAL_ERROR "rsma-build left a package after rejecting a runtime condition")
endif()
string(FIND "${condition_stderr}" "call target 'read_switch' has no definition in this source" condition_message_index)
if(condition_message_index EQUAL -1)
    message(FATAL_ERROR "runtime condition did not receive a useful diagnostic:\n${condition_stderr}")
endif()

foreach(fixture IN ITEMS "runtime_gpio_condition.cpp" "runtime_while.cpp")
    set(runtime_output "${OUTPUT_DIR}/${fixture}.rsfw")
    execute_process(
        COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/${fixture}" -o "${runtime_output}"
        RESULT_VARIABLE runtime_result
        OUTPUT_VARIABLE runtime_stdout
        ERROR_VARIABLE runtime_stderr
    )
    if(NOT runtime_result EQUAL 0)
        message(FATAL_ERROR "rsma-build rejected ${fixture}:\n${runtime_stdout}\n${runtime_stderr}")
    endif()
    execute_process(
        COMMAND "${RSMA_INSPECT}" "${runtime_output}"
        RESULT_VARIABLE runtime_inspect_result
        OUTPUT_VARIABLE runtime_inspect_stdout
        ERROR_VARIABLE runtime_inspect_stderr
    )
    if(NOT runtime_inspect_result EQUAL 0)
        message(FATAL_ERROR "${fixture} produced invalid bytecode:\n${runtime_inspect_stderr}")
    endif()
    foreach(required_text "HAL_CALL" "CMP_EQ_I32" "JUMP_IF")
        string(FIND "${runtime_inspect_stdout}" "${required_text}" text_index)
        if(text_index EQUAL -1)
            message(FATAL_ERROR "${fixture} is missing '${required_text}':\n${runtime_inspect_stdout}")
        endif()
    endforeach()
endforeach()

set(for_output "${OUTPUT_DIR}/for-loop.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/for_loop.cpp" -o "${for_output}"
    RESULT_VARIABLE for_result
    OUTPUT_VARIABLE for_stdout
    ERROR_VARIABLE for_stderr
)
if(NOT for_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected for loop:\n${for_stdout}\n${for_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${for_output}"
    RESULT_VARIABLE for_inspect_result
    OUTPUT_VARIABLE for_inspect_stdout
    ERROR_VARIABLE for_inspect_stderr
)
if(NOT for_inspect_result EQUAL 0)
    message(FATAL_ERROR "for loop produced invalid bytecode:\n${for_inspect_stderr}")
endif()
foreach(required_text "ADD_I32" "CMP_LT_I32" "JUMP_IF" "JUMP" "HALT")
    string(FIND "${for_inspect_stdout}" "${required_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "for loop is missing '${required_text}':\n${for_inspect_stdout}")
    endif()
endforeach()

set(functions_output "${OUTPUT_DIR}/functions.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/functions.cpp" -o "${functions_output}"
    RESULT_VARIABLE functions_result
    OUTPUT_VARIABLE functions_stdout
    ERROR_VARIABLE functions_stderr
)
if(NOT functions_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected supported functions:\n${functions_stdout}\n${functions_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${functions_output}"
    RESULT_VARIABLE functions_inspect_result
    OUTPUT_VARIABLE functions_inspect_stdout
    ERROR_VARIABLE functions_inspect_stderr
)
if(NOT functions_inspect_result EQUAL 0)
    message(FATAL_ERROR "functions produced invalid bytecode:\n${functions_inspect_stderr}")
endif()
foreach(call_line "[2] CALL (4 operand bytes)" "[6] CALL (4 operand bytes)")
    string(FIND "${functions_inspect_stdout}" "${call_line}" call_index)
    if(call_index EQUAL -1)
        message(FATAL_ERROR "expected function call '${call_line}':\n${functions_inspect_stdout}")
    endif()
endforeach()
foreach(required_text "Entry: main (instruction 0)" "RETURN" "HALT")
    string(FIND "${functions_inspect_stdout}" "${required_text}" text_index)
    if(text_index EQUAL -1)
        message(FATAL_ERROR "compiled functions are missing '${required_text}':\n${functions_inspect_stdout}")
    endif()
endforeach()
foreach(required_text "Section: Symbols" "Section: Debug" "Symbols: 3" "main = instruction 0" "get_level = instruction 8" "set_led = instruction 15")
    string(FIND "${functions_inspect_stdout}" "${required_text}" symbol_index)
    if(symbol_index EQUAL -1)
        message(FATAL_ERROR "symbol/debug output is missing '${required_text}':\n${functions_inspect_stdout}")
    endif()
endforeach()

set(recursive_output "${OUTPUT_DIR}/recursive.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/recursive_function.cpp" -o "${recursive_output}"
    RESULT_VARIABLE recursive_result
    OUTPUT_VARIABLE recursive_stdout
    ERROR_VARIABLE recursive_stderr
)
if(NOT recursive_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected supported recursive function:\n${recursive_stdout}\n${recursive_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${recursive_output}"
    RESULT_VARIABLE recursive_inspect_result
    OUTPUT_VARIABLE recursive_inspect_stdout
    ERROR_VARIABLE recursive_inspect_stderr
)
if(NOT recursive_inspect_result EQUAL 0)
    message(FATAL_ERROR "recursive function package is invalid:\n${recursive_inspect_stderr}")
endif()

set(global_output "${OUTPUT_DIR}/global-memory.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/global_memory.cpp" -o "${global_output}"
    RESULT_VARIABLE global_result
    OUTPUT_VARIABLE global_stdout
    ERROR_VARIABLE global_stderr
)
if(NOT global_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected global memory fixture:\n${global_stdout}\n${global_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${global_output}"
    RESULT_VARIABLE global_inspect_result
    OUTPUT_VARIABLE global_inspect_stdout
    ERROR_VARIABLE global_inspect_stderr
)
if(NOT global_inspect_result EQUAL 0)
    message(FATAL_ERROR "global memory package is invalid:\n${global_inspect_stderr}")
endif()
foreach(required_text "GLOBAL_LOAD" "GLOBAL_STORE")
    string(FIND "${global_inspect_stdout}" "${required_text}" global_text_index)
    if(global_text_index EQUAL -1)
        message(FATAL_ERROR "global memory package is missing '${required_text}':\n${global_inspect_stdout}")
    endif()
endforeach()

foreach(fixture_and_message IN ITEMS
        "unsupported_global_type.cpp|global variables currently support only int and bool"
        "unsupported_global_initializer.cpp|expected a compile-time integer constant"
        "millis_argument.cpp|no matching function for call to 'millis'"
        "serial_custom_object.cpp|call target 'begin' has no definition in this source"
        "serial_packet_custom_object.cpp|call target 'packetType' has no definition in this source")
    string(REPLACE "|" ";" fixture_fields "${fixture_and_message}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 expected_message)
    set(rejected_output "${OUTPUT_DIR}/${fixture_name}.rsfw")
    execute_process(
        COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/${fixture_name}" -o "${rejected_output}"
        RESULT_VARIABLE rejected_result
        OUTPUT_VARIABLE rejected_stdout
        ERROR_VARIABLE rejected_stderr
    )
    if(rejected_result EQUAL 0 OR EXISTS "${rejected_output}")
        message(FATAL_ERROR "rsma-build accepted unsupported global fixture ${fixture_name}")
    endif()
    if(NOT "${rejected_stderr}" MATCHES "${expected_message}")
        message(FATAL_ERROR "${fixture_name} received the wrong diagnostic:\n${rejected_stderr}")
    endif()
endforeach()

foreach(fixture_and_message IN ITEMS
        "serial_packet_invalid_arguments.cpp|too many arguments to function call"
        "serial_packet_invalid_handler.cpp|different number of parameters")
    string(REPLACE "|" ";" fixture_fields "${fixture_and_message}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 expected_message)
    set(rejected_output "${OUTPUT_DIR}/${fixture_name}.rsfw")
    execute_process(
        COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/${fixture_name}" -o "${rejected_output}"
        RESULT_VARIABLE rejected_result
        OUTPUT_VARIABLE rejected_stdout
        ERROR_VARIABLE rejected_stderr
    )
    if(rejected_result EQUAL 0 OR EXISTS "${rejected_output}")
        message(FATAL_ERROR "rsma-build accepted invalid UART packet fixture ${fixture_name}")
    endif()
    if(NOT "${rejected_stderr}" MATCHES "${expected_message}")
        message(FATAL_ERROR "${fixture_name} received the wrong diagnostic:\n${rejected_stderr}")
    endif()
endforeach()

foreach(fixture_and_message IN ITEMS
        "source_interrupt_invalid_mode.cpp|interrupt mode must be CHANGE"
        "source_interrupt_invalid_handler.cpp|no matching function for call to 'attachInterrupt'")
    string(REPLACE "|" ";" fixture_fields "${fixture_and_message}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 expected_message)
    set(rejected_output "${OUTPUT_DIR}/${fixture_name}.rsfw")
    execute_process(
        COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/${fixture_name}" -o "${rejected_output}"
        RESULT_VARIABLE rejected_result
        OUTPUT_VARIABLE rejected_stdout
        ERROR_VARIABLE rejected_stderr
    )
    if(rejected_result EQUAL 0 OR EXISTS "${rejected_output}")
        message(FATAL_ERROR "rsma-build accepted invalid source interrupt fixture ${fixture_name}")
    endif()
    if(NOT "${rejected_stderr}" MATCHES "${expected_message}")
        message(FATAL_ERROR "${fixture_name} received the wrong diagnostic:\n${rejected_stderr}")
    endif()
endforeach()

set(serial_invalid_output "${OUTPUT_DIR}/serial-invalid-baud.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_invalid_baud.cpp" -o "${serial_invalid_output}"
    RESULT_VARIABLE serial_invalid_result
    OUTPUT_VARIABLE serial_invalid_stdout
    ERROR_VARIABLE serial_invalid_stderr
)
if(serial_invalid_result EQUAL 0 OR EXISTS "${serial_invalid_output}")
    message(FATAL_ERROR "rsma-build accepted invalid Serial baud preset")
endif()
if(NOT "${serial_invalid_stderr}" MATCHES "Serial.begin baud preset must be between 0 and 4")
    message(FATAL_ERROR "invalid Serial baud received the wrong diagnostic:\n${serial_invalid_stderr}")
endif()

set(serial_arguments_output "${OUTPUT_DIR}/serial-invalid-arguments.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_invalid_arguments.cpp" -o "${serial_arguments_output}"
    RESULT_VARIABLE serial_arguments_result
    OUTPUT_VARIABLE serial_arguments_stdout
    ERROR_VARIABLE serial_arguments_stderr
)
if(serial_arguments_result EQUAL 0 OR EXISTS "${serial_arguments_output}")
    message(FATAL_ERROR "rsma-build accepted invalid Serial argument count")
endif()
if(NOT "${serial_arguments_stderr}" MATCHES "too many arguments to function call")
    message(FATAL_ERROR "invalid Serial argument count received the wrong diagnostic:\n${serial_arguments_stderr}")
endif()

set(serial_write_output "${OUTPUT_DIR}/serial-invalid-write.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/serial_invalid_write.cpp" -o "${serial_write_output}"
    RESULT_VARIABLE serial_write_result
    OUTPUT_VARIABLE serial_write_stdout
    ERROR_VARIABLE serial_write_stderr
)
if(NOT serial_write_result EQUAL 0)
    message(FATAL_ERROR "rsma-build rejected runtime UART byte-range fixture:\n${serial_write_stdout}\n${serial_write_stderr}")
endif()
execute_process(
    COMMAND "${RSMA_INSPECT}" "${serial_write_output}"
    RESULT_VARIABLE serial_write_inspect_result
    OUTPUT_VARIABLE serial_write_inspect_stdout
    ERROR_VARIABLE serial_write_inspect_stderr
)
if(NOT serial_write_inspect_result EQUAL 0)
    message(FATAL_ERROR "runtime UART byte-range firmware is invalid:\n${serial_write_inspect_stderr}")
endif()

set(invalid_output "${OUTPUT_DIR}/unsupported.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/unsupported_loop.cpp" -o "${invalid_output}"
    RESULT_VARIABLE invalid_result
    OUTPUT_VARIABLE invalid_stdout
    ERROR_VARIABLE invalid_stderr
)
if(invalid_result EQUAL 0)
    message(FATAL_ERROR "rsma-build unexpectedly accepted an unsupported loop")
endif()
if(EXISTS "${invalid_output}")
    message(FATAL_ERROR "rsma-build left a firmware package after rejecting source")
endif()
if(NOT "${invalid_stderr}" MATCHES "unsupported statement")
    message(FATAL_ERROR "unsupported source did not receive a useful diagnostic:\n${invalid_stderr}")
endif()

set(dynamic_output "${OUTPUT_DIR}/dynamic-argument.rsfw")
execute_process(
    COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/dynamic_argument.cpp" -o "${dynamic_output}"
    RESULT_VARIABLE dynamic_result
    OUTPUT_VARIABLE dynamic_stdout
    ERROR_VARIABLE dynamic_stderr
)
if(dynamic_result EQUAL 0)
    message(FATAL_ERROR "rsma-build unexpectedly accepted a runtime HAL argument")
endif()
if(EXISTS "${dynamic_output}")
    message(FATAL_ERROR "rsma-build left a package after rejecting a runtime HAL argument")
endif()
if(NOT "${dynamic_stderr}" MATCHES "compile-time integer constant")
    message(FATAL_ERROR "dynamic argument did not receive a useful diagnostic:\n${dynamic_stderr}")
endif()

foreach(fixture_and_message IN ITEMS
        "source_api_stage73_invalid_frequency.cpp|PWM frequency must be between 1 and 1000000 Hz"
        "source_api_stage73_invalid_pwm_pin.cpp|board capability validation failed"
        "unsupported_function_parameter.cpp|function parameters currently support only int and bool"
        "fake_hal.cpp|declaration in <rsma_mcu/Arduino.h>"
        "nested_call_expression.cpp|unsupported statement")
    string(REPLACE "|" ";" fixture_fields "${fixture_and_message}")
    list(GET fixture_fields 0 fixture_name)
    list(GET fixture_fields 1 expected_message)
    set(rejected_output "${OUTPUT_DIR}/${fixture_name}.rsfw")
    execute_process(
        COMMAND "${RSMA_BUILD}" "${SOURCE_DIR}/${fixture_name}" -o "${rejected_output}"
        RESULT_VARIABLE rejected_result
        OUTPUT_VARIABLE rejected_stdout
        ERROR_VARIABLE rejected_stderr
    )
    if(rejected_result EQUAL 0)
        message(FATAL_ERROR "rsma-build unexpectedly accepted ${fixture_name}")
    endif()
    if(EXISTS "${rejected_output}")
        message(FATAL_ERROR "rsma-build left a package after rejecting ${fixture_name}")
    endif()
    if(NOT "${rejected_stderr}" MATCHES "${expected_message}")
        message(FATAL_ERROR "${fixture_name} received the wrong diagnostic:\n${rejected_stderr}")
    endif()
endforeach()