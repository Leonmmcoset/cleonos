cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED NM_TOOL OR "${NM_TOOL}" STREQUAL "")
    message(FATAL_ERROR "NM_TOOL is required")
endif()

if(NOT DEFINED KERNEL_ELF OR "${KERNEL_ELF}" STREQUAL "")
    message(FATAL_ERROR "KERNEL_ELF is required")
endif()

if(NOT DEFINED OUT_SYMBOL_FILE OR "${OUT_SYMBOL_FILE}" STREQUAL "")
    message(FATAL_ERROR "OUT_SYMBOL_FILE is required")
endif()

execute_process(
    COMMAND "${NM_TOOL}" -n "${KERNEL_ELF}"
    RESULT_VARIABLE _nm_result
    OUTPUT_VARIABLE _nm_output
    ERROR_VARIABLE _nm_error
)

if(NOT _nm_result EQUAL 0)
    message(FATAL_ERROR "failed to run nm (${_nm_result}): ${_nm_error}")
endif()

string(REPLACE "\r\n" "\n" _nm_output "${_nm_output}")
string(REPLACE "\r" "\n" _nm_output "${_nm_output}")
string(REPLACE "\n" ";" _nm_lines "${_nm_output}")

set(_out_text "CLEONOS_KERNEL_SYMBOLS_V1\n")

foreach(_line IN LISTS _nm_lines)
    string(STRIP "${_line}" _line)

    if(_line STREQUAL "")
        continue()
    endif()

    if(_line MATCHES "^([0-9A-Fa-f]+)[ \t]+([tTwW])[ \t]+(.+)$")
        set(_addr "${CMAKE_MATCH_1}")
        set(_name "${CMAKE_MATCH_3}")

        if(_name MATCHES "^\\.")
            continue()
        endif()

        string(TOUPPER "${_addr}" _addr_upper)
        set(_out_text "${_out_text}0X${_addr_upper} ${_name}\n")
    endif()
endforeach()

file(WRITE "${OUT_SYMBOL_FILE}" "${_out_text}")
