cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED NM_TOOL OR "${NM_TOOL}" STREQUAL "")
    message(FATAL_ERROR "NM_TOOL is required")
endif()

if(NOT DEFINED ADDR2LINE_TOOL OR "${ADDR2LINE_TOOL}" STREQUAL "")
    message(FATAL_ERROR "ADDR2LINE_TOOL is required")
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

set(_symbol_addrs_upper)
set(_symbol_addrs_query)
set(_symbol_names)

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
        list(APPEND _symbol_addrs_upper "${_addr_upper}")
        list(APPEND _symbol_addrs_query "0x${_addr_upper}")
        list(APPEND _symbol_names "${_name}")
    endif()
endforeach()

set(_out_text "CLEONOS_KERNEL_SYMBOLS_V2\n")

list(LENGTH _symbol_addrs_query _symbol_count)

if(_symbol_count GREATER 0)
    execute_process(
        COMMAND "${ADDR2LINE_TOOL}" -f -C -e "${KERNEL_ELF}" ${_symbol_addrs_query}
        RESULT_VARIABLE _a2l_result
        OUTPUT_VARIABLE _a2l_output
        ERROR_VARIABLE _a2l_error
    )

    if(NOT _a2l_result EQUAL 0)
        message(FATAL_ERROR "failed to run addr2line (${_a2l_result}): ${_a2l_error}")
    endif()

    string(REPLACE "\r\n" "\n" _a2l_output "${_a2l_output}")
    string(REPLACE "\r" "\n" _a2l_output "${_a2l_output}")
    string(REPLACE "\n" ";" _a2l_lines "${_a2l_output}")
    list(LENGTH _a2l_lines _a2l_line_count)

    math(EXPR _last_index "${_symbol_count} - 1")
    foreach(_idx RANGE 0 ${_last_index})
        math(EXPR _src_idx "${_idx} * 2 + 1")

        list(GET _symbol_addrs_upper ${_idx} _addr_upper)
        list(GET _symbol_names ${_idx} _name)

        if(_src_idx LESS _a2l_line_count)
            list(GET _a2l_lines ${_src_idx} _src_line)
        else()
            set(_src_line "??:0")
        endif()

        string(STRIP "${_src_line}" _src_line)

        if("${_src_line}" STREQUAL "")
            set(_src_line "??:0")
        endif()

        string(REPLACE "\t" " " _src_line "${_src_line}")
        set(_out_text "${_out_text}0X${_addr_upper}\t${_name}\t${_src_line}\n")
    endforeach()
endif()

file(WRITE "${OUT_SYMBOL_FILE}" "${_out_text}")
