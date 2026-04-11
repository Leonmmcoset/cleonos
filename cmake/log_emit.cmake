cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED NO_COLOR)
    set(NO_COLOR 0)
endif()

include("${CMAKE_CURRENT_LIST_DIR}/log.cmake")

if(NOT DEFINED LOG_LEVEL)
    set(LOG_LEVEL "INFO")
endif()

if(NOT DEFINED LOG_TEXT)
    set(LOG_TEXT "")
endif()

string(TOUPPER "${LOG_LEVEL}" _log_level)

if(_log_level STREQUAL "STEP")
    cl_log_step("${LOG_TEXT}")
elseif(_log_level STREQUAL "INFO")
    cl_log_info("${LOG_TEXT}")
elseif(_log_level STREQUAL "WARN")
    cl_log_warn("${LOG_TEXT}")
elseif(_log_level STREQUAL "ERROR")
    cl_log_error("${LOG_TEXT}")
else()
    cl_log_info("${LOG_TEXT}")
endif()