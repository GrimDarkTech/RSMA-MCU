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
foreach(required_text "Instructions: 3" "[0] HAL_CALL" "[1] WAIT" "[2] JUMP")
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
if(NOT "${condition_stderr}" MATCHES "compile-time integer constant")
    message(FATAL_ERROR "runtime condition did not receive a useful diagnostic:\n${condition_stderr}")
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
        "arbitrary_call.cpp|unsupported function call 'helper'"
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