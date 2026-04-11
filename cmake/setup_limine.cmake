cmake_minimum_required(VERSION 3.20)
include("${CMAKE_CURRENT_LIST_DIR}/log.cmake")

function(run_cmd)
    set(options)
    set(oneValueArgs WORKING_DIRECTORY)
    set(multiValueArgs COMMAND)
    cmake_parse_arguments(RUN "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if("${RUN_WORKING_DIRECTORY}" STREQUAL "")
        execute_process(COMMAND ${RUN_COMMAND} RESULT_VARIABLE _rv)
    else()
        execute_process(COMMAND ${RUN_COMMAND} WORKING_DIRECTORY "${RUN_WORKING_DIRECTORY}" RESULT_VARIABLE _rv)
    endif()

    if(NOT _rv EQUAL 0)
        string(REPLACE ";" " " _cmd "${RUN_COMMAND}")
        cl_log_error("command failed (${_rv}): ${_cmd}")
    endif()
endfunction()

cl_log_step("preparing limine")
set(_limine_makefile_missing FALSE)

if(NOT EXISTS "${LIMINE_DIR}")
    if("${LIMINE_REF}" STREQUAL "")
        cl_log_info("cloning limine (default branch) into ${LIMINE_DIR}")
        run_cmd(COMMAND "${GIT_TOOL}" clone --depth 1 "${LIMINE_REPO}" "${LIMINE_DIR}")
    else()
        cl_log_info("cloning limine (${LIMINE_REF}) into ${LIMINE_DIR}")
        run_cmd(COMMAND "${GIT_TOOL}" clone --branch "${LIMINE_REF}" --depth 1 "${LIMINE_REPO}" "${LIMINE_DIR}")
    endif()
endif()

if(LIMINE_SKIP_CONFIGURE)
    if(NOT EXISTS "${LIMINE_DIR}/Makefile")
        set(_limine_makefile_missing TRUE)
        cl_log_warn("LIMINE_SKIP_CONFIGURE=1 but ${LIMINE_DIR}/Makefile is missing; continue with existing limine binaries")
    endif()
    cl_log_warn("skipping limine Makefile generation (LIMINE_SKIP_CONFIGURE=1)")
else()
    set(cfg_fingerprint "FLAGS=${LIMINE_CONFIGURE_FLAGS};OBJCOPY=${OBJCOPY_FOR_TARGET};OBJDUMP=${OBJDUMP_FOR_TARGET};READELF=${READELF_FOR_TARGET}")

    set(need_configure FALSE)
    if(NOT EXISTS "${LIMINE_DIR}/Makefile")
        set(need_configure TRUE)
    endif()
    if(NOT EXISTS "${LIMINE_SETUP_STAMP}")
        set(need_configure TRUE)
    endif()

    if(EXISTS "${LIMINE_SETUP_STAMP}")
        file(READ "${LIMINE_SETUP_STAMP}" _stamp_content)
        string(STRIP "${_stamp_content}" _stamp_content)
        if(NOT _stamp_content STREQUAL cfg_fingerprint)
            set(need_configure TRUE)
        endif()
    endif()

    if(need_configure)
        cl_log_step("generating/reconfiguring limine Makefile")

        if(EXISTS "${LIMINE_DIR}/bootstrap")
            run_cmd(COMMAND "${SH_TOOL}" "./bootstrap" WORKING_DIRECTORY "${LIMINE_DIR}")
        endif()

        if(NOT EXISTS "${LIMINE_DIR}/configure")
            cl_log_error("limine configure script missing")
        endif()

        separate_arguments(_cfg_flags UNIX_COMMAND "${LIMINE_CONFIGURE_FLAGS}")
        run_cmd(
            COMMAND
                "${CMAKE_COMMAND}" -E env
                "OBJCOPY_FOR_TARGET=${OBJCOPY_FOR_TARGET}"
                "OBJDUMP_FOR_TARGET=${OBJDUMP_FOR_TARGET}"
                "READELF_FOR_TARGET=${READELF_FOR_TARGET}"
                "${SH_TOOL}" "./configure" ${_cfg_flags}
            WORKING_DIRECTORY "${LIMINE_DIR}"
        )

        file(WRITE "${LIMINE_SETUP_STAMP}" "${cfg_fingerprint}\n")
        file(REMOVE "${LIMINE_BUILD_STAMP}")
    else()
        cl_log_info("limine configure state unchanged")
    endif()
endif()

set(need_build FALSE)
if(NOT EXISTS "${LIMINE_BUILD_STAMP}")
    set(need_build TRUE)
endif()

foreach(_bin limine limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin)
    if(NOT EXISTS "${LIMINE_BIN_DIR}/${_bin}")
        set(need_build TRUE)
    endif()
endforeach()

if(need_build)
    if(_limine_makefile_missing)
        cl_log_warn("limine Makefile missing, skip limine build and use existing artifacts only")
    else()
        cl_log_info("building limine")
        run_cmd(COMMAND "${MAKE_TOOL}" -C "${LIMINE_DIR}")
        file(WRITE "${LIMINE_BUILD_STAMP}" "built\n")
    endif()
else()
    cl_log_info("limine already built, skipping compile")
endif()

foreach(_required limine limine-bios.sys limine-bios-cd.bin limine-uefi-cd.bin)
    if(NOT EXISTS "${LIMINE_BIN_DIR}/${_required}")
        cl_log_error("${_required} missing in ${LIMINE_BIN_DIR}")
    endif()
endforeach()

cl_log_info("limine artifacts ready")