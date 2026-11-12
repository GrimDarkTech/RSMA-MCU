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