cmake_minimum_required(VERSION 3.20)
project(CLeonOS NONE)

# User-configurable toolchain and tool variables (Makefile-compatible defaults).
set(NO_COLOR 0 CACHE STRING "Disable colored log output when set to 1")
include("${CMAKE_SOURCE_DIR}/cmake/log.cmake")
set(CL_LOG_EMIT_SCRIPT "${CMAKE_SOURCE_DIR}/cmake/log_emit.cmake")

set(CC "x86_64-elf-gcc" CACHE STRING "Kernel C compiler")
set(LD "x86_64-elf-ld" CACHE STRING "Kernel linker")
set(USER_CC "cc" CACHE STRING "User-space C compiler")
set(USER_LD "ld" CACHE STRING "User-space linker")
set(RUSTC "rustc" CACHE STRING "Rust compiler")
set(NM "nm" CACHE STRING "nm executable")
set(ADDR2LINE "addr2line" CACHE STRING "addr2line executable")

set(XORRISO "xorriso" CACHE STRING "xorriso executable")
set(TAR "tar" CACHE STRING "tar executable")
set(GIT_TOOL "git" CACHE STRING "git executable")
set(MAKE_TOOL "make" CACHE STRING "make executable")
set(SH_TOOL "sh" CACHE STRING "POSIX shell executable")

set(OBJCOPY_FOR_TARGET "llvm-objcopy" CACHE STRING "objcopy tool for limine configure")
set(OBJDUMP_FOR_TARGET "llvm-objdump" CACHE STRING "objdump tool for limine configure")
set(READELF_FOR_TARGET "llvm-readelf" CACHE STRING "readelf tool for limine configure")

set(QEMU_X86_64 "qemu-system-x86_64" CACHE STRING "QEMU executable")

set(LIMINE_DIR "limine" CACHE STRING "Limine source directory")
set(LIMINE_REPO "https://gh-proxy.com/https://github.com/limine-bootloader/limine.git" CACHE STRING "Limine git repository")
set(LIMINE_REF "" CACHE STRING "Optional Limine branch or tag")
set(LIMINE_BIN_DIR "${LIMINE_DIR}/bin" CACHE STRING "Limine bin directory")
set(LIMINE_CONFIGURE_FLAGS "--enable-bios-cd --enable-uefi-cd --enable-uefi-x86-64" CACHE STRING "Limine configure flags")
set(LIMINE_SKIP_CONFIGURE 0 CACHE BOOL "Skip Limine configure step")

function(resolve_path INPUT_PATH OUTPUT_VAR)
    if(IS_ABSOLUTE "${INPUT_PATH}")
        set(${OUTPUT_VAR} "${INPUT_PATH}" PARENT_SCOPE)
    else()
        set(${OUTPUT_VAR} "${CMAKE_SOURCE_DIR}/${INPUT_PATH}" PARENT_SCOPE)
    endif()
endfunction()

function(resolve_tool_with_fallback VAR_NAME)
    set(_fallbacks ${ARGN})
    set(_requested "${${VAR_NAME}}")

    if("${_requested}" STREQUAL "")
        cl_log_error("empty tool variable: ${VAR_NAME}")
    endif()

    if(IS_ABSOLUTE "${_requested}")
        if(NOT EXISTS "${_requested}")
            cl_log_error("${VAR_NAME} not found: ${_requested}")
        endif()
        set(_resolved "${_requested}")
    else()
        find_program(_requested_path NAMES "${_requested}")
        if(_requested_path)
            set(_resolved "${_requested}")
        else()
            set(_resolved "")
            foreach(_cand IN LISTS _fallbacks)
                find_program(_cand_path NAMES "${_cand}")
                if(_cand_path)
                    cl_log_warn("${VAR_NAME} '${_requested}' not found; fallback to '${_cand}'")
                    set(_resolved "${_cand}")
                    break()
                endif()
            endforeach()
            if("${_resolved}" STREQUAL "")
                cl_log_error("${VAR_NAME} tool not found: '${_requested}', fallbacks='${_fallbacks}'")
            endif()
        endif()
    endif()

    get_property(_help CACHE "${VAR_NAME}" PROPERTY HELPSTRING)
    if("${_help}" STREQUAL "")
        set(_help "tool path/name")
    endif()
    set(${VAR_NAME} "${_resolved}" CACHE STRING "${_help}" FORCE)
    set(${VAR_NAME} "${_resolved}" PARENT_SCOPE)
endfunction()

set(BUILD_ROOT "${CMAKE_SOURCE_DIR}/build/x86_64")
set(OBJ_ROOT "${BUILD_ROOT}/obj")
set(ISO_ROOT "${BUILD_ROOT}/iso_root")
set(RAMDISK_ROOT "${BUILD_ROOT}/ramdisk_root")
set(KERNEL_ELF "${BUILD_ROOT}/clks_kernel.elf")
set(RAMDISK_IMAGE "${BUILD_ROOT}/cleonos_ramdisk.tar")
set(ISO_IMAGE "${CMAKE_SOURCE_DIR}/build/CLeonOS-x86_64.iso")
set(KERNEL_SYMBOLS_FILE "${BUILD_ROOT}/kernel.sym")

set(USER_BUILD_ROOT "${BUILD_ROOT}/user")
set(USER_OBJ_ROOT "${USER_BUILD_ROOT}/obj")
set(USER_APP_DIR "${USER_BUILD_ROOT}/apps")
set(USER_LIB_DIR "${USER_BUILD_ROOT}/lib")
set(USER_RUST_LIB "${USER_LIB_DIR}/libcleonos_user_rust.a")
set(KERNEL_RUST_LIB "${BUILD_ROOT}/libclks_kernel_rust.a")

resolve_tool_with_fallback(CC gcc cc clang)
resolve_tool_with_fallback(LD ld.lld ld)
resolve_tool_with_fallback(USER_CC cc gcc clang)
resolve_tool_with_fallback(USER_LD ld.lld ld)
resolve_tool_with_fallback(NM llvm-nm x86_64-elf-nm nm)
resolve_tool_with_fallback(ADDR2LINE llvm-addr2line x86_64-elf-addr2line addr2line)
resolve_tool_with_fallback(OBJCOPY_FOR_TARGET llvm-objcopy x86_64-linux-gnu-objcopy objcopy)
resolve_tool_with_fallback(OBJDUMP_FOR_TARGET llvm-objdump x86_64-linux-gnu-objdump objdump)
resolve_tool_with_fallback(READELF_FOR_TARGET llvm-readelf x86_64-linux-gnu-readelf readelf)

resolve_path("${LIMINE_DIR}" LIMINE_DIR_ABS)
resolve_path("${LIMINE_BIN_DIR}" LIMINE_BIN_DIR_ABS)
set(LIMINE_SETUP_STAMP "${LIMINE_DIR_ABS}/.cleonos-limine-setup.stamp")
set(LIMINE_BUILD_STAMP "${LIMINE_DIR_ABS}/.cleonos-limine-build.stamp")

set(CLKS_ARCH "x86_64" CACHE STRING "Target CLKS arch")
set(LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/clks/arch/${CLKS_ARCH}/linker.ld")
set(USER_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/user.ld")
set(KELF_LINKER_SCRIPT "${CMAKE_SOURCE_DIR}/cleonos/c/kelf.ld")
set(CLEONOS_MENUCONFIG_CMAKE "${CMAKE_SOURCE_DIR}/configs/menuconfig/config.cmake" CACHE FILEPATH "menuconfig generated CMake config")

if(EXISTS "${CLEONOS_MENUCONFIG_CMAKE}")
    include("${CLEONOS_MENUCONFIG_CMAKE}")
    cl_log_info("menuconfig loaded from ${CLEONOS_MENUCONFIG_CMAKE}")
endif()

function(cl_set_bool_cache VAR_NAME DEFAULT_VALUE DOC_TEXT)
    if(NOT DEFINED ${VAR_NAME})
        set(${VAR_NAME} ${DEFAULT_VALUE} CACHE BOOL "${DOC_TEXT}")
    else()
        set(${VAR_NAME} ${${VAR_NAME}} CACHE BOOL "${DOC_TEXT}" FORCE)
    endif()
endfunction()

function(cl_bool_to_int BOOL_VAR OUT_VAR)
    if(${BOOL_VAR})
        set(${OUT_VAR} 1 PARENT_SCOPE)
    else()
        set(${OUT_VAR} 0 PARENT_SCOPE)
    endif()
endfunction()

cl_set_bool_cache(CLEONOS_CLKS_ENABLE_AUDIO ON "Initialize kernel audio subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_MOUSE ON "Initialize kernel mouse subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_DESKTOP ON "Initialize desktop compositor subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_DRIVER_MANAGER ON "Initialize kernel driver manager")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KELF ON "Enable KELF app dispatcher")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC ON "Auto exec /shell/shell.elf on boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_HEAP_SELFTEST ON "Run heap selftest at boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_EXTERNAL_PSF ON "Load /system/tty.psf external font")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KEYBOARD ON "Initialize keyboard input subsystem")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_ELFRUNNER_PROBE ON "Run ELFRUNNER kernel ELF probe")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KLOGD_TASK ON "Enable scheduler klogd task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KWORKER_TASK ON "Enable scheduler kworker task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USRD_TASK ON "Enable scheduler usrd task")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_BOOT_VIDEO_LOG ON "Print boot framebuffer geometry logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_PMM_STATS_LOG ON "Print PMM stats logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_HEAP_STATS_LOG ON "Print heap stats logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_FS_ROOT_LOG ON "Print root children FS log")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SYSTEM_DIR_CHECK ON "Require /system directory sanity check")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_ELFRUNNER_INIT ON "Initialize ELFRUNNER framework")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SYSCALL_TICK_QUERY ON "Query syscall timer ticks at boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_TTY_READY_LOG ON "Print TTY ready logs")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG ON "Print idle loop debug log")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_PROCFS ON "Enable virtual /proc procfs syscall layer")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG ON "Enable EXEC info logs on serial output")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_TTY_SWITCH_HOTKEY ON "Enable ALT+F1..F4 TTY switch hotkey")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_CTRL_SHORTCUTS ON "Enable Ctrl+A/C/V keyboard shortcuts")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_KBD_FORCE_STOP_HOTKEY ON "Enable Ctrl+Alt+C force-stop hotkey")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USER_INIT_SCRIPT_PROBE ON "Probe /shell/init.cmd during userland init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_USER_SYSTEM_APP_PROBE ON "Probe /system apps (elfrunner/memc) during userland init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SCHED_TASK_COUNT_LOG ON "Print scheduler task count during boot")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_INTERRUPT_READY_LOG ON "Print interrupt ready log after IDT/PIC init")
cl_set_bool_cache(CLEONOS_CLKS_ENABLE_SHELL_MODE_LOG ON "Print default shell mode log during boot")

cl_bool_to_int(CLEONOS_CLKS_ENABLE_AUDIO CLKS_CFG_AUDIO_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_MOUSE CLKS_CFG_MOUSE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_DESKTOP CLKS_CFG_DESKTOP_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_DRIVER_MANAGER CLKS_CFG_DRIVER_MANAGER_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KELF CLKS_CFG_KELF_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USERLAND_AUTO_EXEC CLKS_CFG_USERLAND_AUTO_EXEC_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_HEAP_SELFTEST CLKS_CFG_HEAP_SELFTEST_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_EXTERNAL_PSF CLKS_CFG_EXTERNAL_PSF_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KEYBOARD CLKS_CFG_KEYBOARD_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_ELFRUNNER_PROBE CLKS_CFG_ELFRUNNER_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KLOGD_TASK CLKS_CFG_KLOGD_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KWORKER_TASK CLKS_CFG_KWORKER_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USRD_TASK CLKS_CFG_USRD_TASK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_BOOT_VIDEO_LOG CLKS_CFG_BOOT_VIDEO_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_PMM_STATS_LOG CLKS_CFG_PMM_STATS_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_HEAP_STATS_LOG CLKS_CFG_HEAP_STATS_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_FS_ROOT_LOG CLKS_CFG_FS_ROOT_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SYSTEM_DIR_CHECK CLKS_CFG_SYSTEM_DIR_CHECK_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_ELFRUNNER_INIT CLKS_CFG_ELFRUNNER_INIT_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SYSCALL_TICK_QUERY CLKS_CFG_SYSCALL_TICK_QUERY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_TTY_READY_LOG CLKS_CFG_TTY_READY_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_IDLE_DEBUG_LOG CLKS_CFG_IDLE_DEBUG_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_PROCFS CLKS_CFG_PROCFS_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_EXEC_SERIAL_LOG CLKS_CFG_EXEC_SERIAL_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_TTY_SWITCH_HOTKEY CLKS_CFG_KBD_TTY_SWITCH_HOTKEY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_CTRL_SHORTCUTS CLKS_CFG_KBD_CTRL_SHORTCUTS_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_KBD_FORCE_STOP_HOTKEY CLKS_CFG_KBD_FORCE_STOP_HOTKEY_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USER_INIT_SCRIPT_PROBE CLKS_CFG_USER_INIT_SCRIPT_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_USER_SYSTEM_APP_PROBE CLKS_CFG_USER_SYSTEM_APP_PROBE_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SCHED_TASK_COUNT_LOG CLKS_CFG_SCHED_TASK_COUNT_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_INTERRUPT_READY_LOG CLKS_CFG_INTERRUPT_READY_LOG_INT)
cl_bool_to_int(CLEONOS_CLKS_ENABLE_SHELL_MODE_LOG CLKS_CFG_SHELL_MODE_LOG_INT)

set(CFLAGS_COMMON
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -g
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/clks/include"
)

set(ARCH_CFLAGS
    -DCLKS_ARCH_X86_64=1
    "-DCLKS_CFG_AUDIO=${CLKS_CFG_AUDIO_INT}"
    "-DCLKS_CFG_MOUSE=${CLKS_CFG_MOUSE_INT}"
    "-DCLKS_CFG_DESKTOP=${CLKS_CFG_DESKTOP_INT}"
    "-DCLKS_CFG_DRIVER_MANAGER=${CLKS_CFG_DRIVER_MANAGER_INT}"
    "-DCLKS_CFG_KELF=${CLKS_CFG_KELF_INT}"
    "-DCLKS_CFG_USERLAND_AUTO_EXEC=${CLKS_CFG_USERLAND_AUTO_EXEC_INT}"
    "-DCLKS_CFG_HEAP_SELFTEST=${CLKS_CFG_HEAP_SELFTEST_INT}"
    "-DCLKS_CFG_EXTERNAL_PSF=${CLKS_CFG_EXTERNAL_PSF_INT}"
    "-DCLKS_CFG_KEYBOARD=${CLKS_CFG_KEYBOARD_INT}"
    "-DCLKS_CFG_ELFRUNNER_PROBE=${CLKS_CFG_ELFRUNNER_PROBE_INT}"
    "-DCLKS_CFG_KLOGD_TASK=${CLKS_CFG_KLOGD_TASK_INT}"
    "-DCLKS_CFG_KWORKER_TASK=${CLKS_CFG_KWORKER_TASK_INT}"
    "-DCLKS_CFG_USRD_TASK=${CLKS_CFG_USRD_TASK_INT}"
    "-DCLKS_CFG_BOOT_VIDEO_LOG=${CLKS_CFG_BOOT_VIDEO_LOG_INT}"
    "-DCLKS_CFG_PMM_STATS_LOG=${CLKS_CFG_PMM_STATS_LOG_INT}"
    "-DCLKS_CFG_HEAP_STATS_LOG=${CLKS_CFG_HEAP_STATS_LOG_INT}"
    "-DCLKS_CFG_FS_ROOT_LOG=${CLKS_CFG_FS_ROOT_LOG_INT}"
    "-DCLKS_CFG_SYSTEM_DIR_CHECK=${CLKS_CFG_SYSTEM_DIR_CHECK_INT}"
    "-DCLKS_CFG_ELFRUNNER_INIT=${CLKS_CFG_ELFRUNNER_INIT_INT}"
    "-DCLKS_CFG_SYSCALL_TICK_QUERY=${CLKS_CFG_SYSCALL_TICK_QUERY_INT}"
    "-DCLKS_CFG_TTY_READY_LOG=${CLKS_CFG_TTY_READY_LOG_INT}"
    "-DCLKS_CFG_IDLE_DEBUG_LOG=${CLKS_CFG_IDLE_DEBUG_LOG_INT}"
    "-DCLKS_CFG_PROCFS=${CLKS_CFG_PROCFS_INT}"
    "-DCLKS_CFG_EXEC_SERIAL_LOG=${CLKS_CFG_EXEC_SERIAL_LOG_INT}"
    "-DCLKS_CFG_KBD_TTY_SWITCH_HOTKEY=${CLKS_CFG_KBD_TTY_SWITCH_HOTKEY_INT}"
    "-DCLKS_CFG_KBD_CTRL_SHORTCUTS=${CLKS_CFG_KBD_CTRL_SHORTCUTS_INT}"
    "-DCLKS_CFG_KBD_FORCE_STOP_HOTKEY=${CLKS_CFG_KBD_FORCE_STOP_HOTKEY_INT}"
    "-DCLKS_CFG_USER_INIT_SCRIPT_PROBE=${CLKS_CFG_USER_INIT_SCRIPT_PROBE_INT}"
    "-DCLKS_CFG_USER_SYSTEM_APP_PROBE=${CLKS_CFG_USER_SYSTEM_APP_PROBE_INT}"
    "-DCLKS_CFG_SCHED_TASK_COUNT_LOG=${CLKS_CFG_SCHED_TASK_COUNT_LOG_INT}"
    "-DCLKS_CFG_INTERRUPT_READY_LOG=${CLKS_CFG_INTERRUPT_READY_LOG_INT}"
    "-DCLKS_CFG_SHELL_MODE_LOG=${CLKS_CFG_SHELL_MODE_LOG_INT}"
    -m64
    -mno-red-zone
    -mcmodel=kernel
    -fno-pic
    -fno-pie
)

set(ASFLAGS_COMMON
    -ffreestanding
    "-I${CMAKE_SOURCE_DIR}/clks/include"
)

set(LDFLAGS_COMMON
    -nostdlib
    -z
    max-page-size=0x1000
)

set(USER_CFLAGS
    -std=c11
    -ffreestanding
    -fno-stack-protector
    -fno-builtin
    -Wall
    -Wextra
    -Werror
    "-I${CMAKE_SOURCE_DIR}/cleonos/c/include"
)

set(USER_LDFLAGS
    -nostdlib
    -z
    max-page-size=0x1000
    -T
    "${USER_LINKER_SCRIPT}"
)

set(KELF_LDFLAGS
    -nostdlib
    -z
    max-page-size=0x1000
    -T
    "${KELF_LINKER_SCRIPT}"
)

if(NOT CLKS_ARCH STREQUAL "x86_64")
    cl_log_error("CLKS_ARCH=${CLKS_ARCH} is not supported by this CMake build yet; use x86_64")
endif()

set(KERNEL_SOURCE_DIRS
    "${CMAKE_SOURCE_DIR}/clks/kernel"
    "${CMAKE_SOURCE_DIR}/clks/lib"
    "${CMAKE_SOURCE_DIR}/clks/drivers"
    "${CMAKE_SOURCE_DIR}/clks/arch/${CLKS_ARCH}"
)

set(C_SOURCES)
set(ASM_SOURCES)
foreach(_src_dir IN LISTS KERNEL_SOURCE_DIRS)
    if(EXISTS "${_src_dir}")
        file(GLOB_RECURSE _c_abs CONFIGURE_DEPENDS "${_src_dir}/*.c")
        file(GLOB_RECURSE _s_abs CONFIGURE_DEPENDS "${_src_dir}/*.S")
        foreach(_abs IN LISTS _c_abs)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
            list(APPEND C_SOURCES "${_rel}")
        endforeach()
        foreach(_abs IN LISTS _s_abs)
            file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
            list(APPEND ASM_SOURCES "${_rel}")
        endforeach()
    endif()
endforeach()
list(REMOVE_DUPLICATES C_SOURCES)
list(REMOVE_DUPLICATES ASM_SOURCES)
list(SORT C_SOURCES)
list(SORT ASM_SOURCES)

file(GLOB_RECURSE KERNEL_INC_SOURCES_ABS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/clks/**/*.inc"
)
list(SORT KERNEL_INC_SOURCES_ABS)

file(GLOB_RECURSE USER_INC_SOURCES_ABS CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/cleonos/**/*.inc"
)
list(SORT USER_INC_SOURCES_ABS)

if(NOT C_SOURCES)
    cl_log_error("no kernel C sources found in clks/")
endif()
if(NOT EXISTS "${LINKER_SCRIPT}")
    cl_log_error("missing linker script: ${LINKER_SCRIPT}")
endif()

file(GLOB_RECURSE USER_COMMON_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/src/*.c")
set(USER_COMMON_SOURCES)
foreach(_abs IN LISTS USER_COMMON_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_COMMON_SOURCES "${_rel}")
endforeach()
list(SORT USER_COMMON_SOURCES)

file(GLOB USER_APP_MAIN_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/apps/*_main.c")
file(GLOB USER_APP_KMAIN_SOURCES_ABS CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/c/apps/*_kmain.c")

set(USER_APP_MAIN_SOURCES)
set(USER_APP_KMAIN_SOURCES)
foreach(_abs IN LISTS USER_APP_MAIN_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_APP_MAIN_SOURCES "${_rel}")
endforeach()
foreach(_abs IN LISTS USER_APP_KMAIN_SOURCES_ABS)
    file(RELATIVE_PATH _rel "${CMAKE_SOURCE_DIR}" "${_abs}")
    list(APPEND USER_APP_KMAIN_SOURCES "${_rel}")
endforeach()
list(SORT USER_APP_MAIN_SOURCES)
list(SORT USER_APP_KMAIN_SOURCES)

if(NOT USER_COMMON_SOURCES)
    cl_log_error("no user common sources found in cleonos/c/src/")
endif()
if(NOT USER_APP_MAIN_SOURCES)
    cl_log_error("no user app *_main.c found in cleonos/c/apps/")
endif()
if(NOT USER_APP_KMAIN_SOURCES)
    cl_log_error("no kernel app *_kmain.c found in cleonos/c/apps/")
endif()

function(add_kernel_c_object SRC OUTPUT_LIST_VAR)
    string(REGEX REPLACE "\\.c$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${KERNEL_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${CC} ${CFLAGS_COMMON} ${ARCH_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_LIST_VAR} ${${OUTPUT_LIST_VAR}} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

function(add_kernel_asm_object SRC OUTPUT_LIST_VAR)
    string(REGEX REPLACE "\\.S$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${KERNEL_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${CC} ${ASFLAGS_COMMON} ${ARCH_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_LIST_VAR} ${${OUTPUT_LIST_VAR}} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

function(add_user_c_object SRC OUTPUT_VAR)
    string(REGEX REPLACE "\\.c$" ".o" OBJ_REL "${SRC}")
    set(OBJ_PATH "${USER_OBJ_ROOT}/${OBJ_REL}")
    get_filename_component(OBJ_DIR "${OBJ_PATH}" DIRECTORY)
    set(_deps "${CMAKE_SOURCE_DIR}/${SRC}" ${USER_INC_SOURCES_ABS})

    add_custom_command(
        OUTPUT "${OBJ_PATH}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${OBJ_DIR}"
        COMMAND ${USER_CC} ${USER_CFLAGS} -c "${CMAKE_SOURCE_DIR}/${SRC}" -o "${OBJ_PATH}"
        DEPENDS ${_deps}
        VERBATIM
    )

    set(${OUTPUT_VAR} "${OBJ_PATH}" PARENT_SCOPE)
endfunction()

set(KERNEL_OBJECTS)
foreach(SRC IN LISTS C_SOURCES)
    add_kernel_c_object("${SRC}" KERNEL_OBJECTS)
endforeach()
foreach(SRC IN LISTS ASM_SOURCES)
    add_kernel_asm_object("${SRC}" KERNEL_OBJECTS)
endforeach()

add_custom_command(
    OUTPUT "${KERNEL_RUST_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${RUSTC} --crate-type staticlib -C panic=abort -O "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs" -o "${KERNEL_RUST_LIB}"
    DEPENDS "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs"
    VERBATIM
)

add_custom_command(
    OUTPUT "${KERNEL_ELF}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${LD} ${LDFLAGS_COMMON} -T "${LINKER_SCRIPT}" -o "${KERNEL_ELF}" ${KERNEL_OBJECTS} "${KERNEL_RUST_LIB}"
    DEPENDS ${KERNEL_OBJECTS} "${LINKER_SCRIPT}" "${KERNEL_RUST_LIB}" "${CMAKE_SOURCE_DIR}/clks/rust/src/lib.rs"
    VERBATIM
)

add_custom_target(kernel DEPENDS "${KERNEL_ELF}")

add_custom_command(
    OUTPUT "${KERNEL_SYMBOLS_FILE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${CMAKE_COMMAND}
        "-DNM_TOOL=${NM}"
        "-DADDR2LINE_TOOL=${ADDR2LINE}"
        "-DKERNEL_ELF=${KERNEL_ELF}"
        "-DOUT_SYMBOL_FILE=${KERNEL_SYMBOLS_FILE}"
        -P "${CMAKE_SOURCE_DIR}/cmake/gen_kernel_symbols.cmake"
    DEPENDS "${KERNEL_ELF}" "${CMAKE_SOURCE_DIR}/cmake/gen_kernel_symbols.cmake"
    VERBATIM
)

add_custom_target(kernel-symbols DEPENDS "${KERNEL_SYMBOLS_FILE}")
add_dependencies(kernel-symbols kernel)

set(USER_COMMON_OBJECTS)
foreach(SRC IN LISTS USER_COMMON_SOURCES)
    add_user_c_object("${SRC}" OBJ_OUT)
    list(APPEND USER_COMMON_OBJECTS "${OBJ_OUT}")
endforeach()

add_custom_command(
    OUTPUT "${USER_RUST_LIB}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_LIB_DIR}"
    COMMAND ${RUSTC} --crate-type staticlib -C panic=abort -O "${CMAKE_SOURCE_DIR}/cleonos/rust/src/lib.rs" -o "${USER_RUST_LIB}"
    DEPENDS "${CMAKE_SOURCE_DIR}/cleonos/rust/src/lib.rs"
    VERBATIM
)

set(USER_KELF_APP_NAMES)
foreach(SRC IN LISTS USER_APP_KMAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_kmain$" "" _app_name "${_stem}")
    list(APPEND USER_KELF_APP_NAMES "${_app_name}")
endforeach()
list(REMOVE_DUPLICATES USER_KELF_APP_NAMES)

set(USER_APP_OUTPUTS)
set(USER_APP_NAMES)
set(RAMDISK_SHELL_APPS)
set(RAMDISK_DRIVER_APPS)
set(RAMDISK_SYSTEM_APPS)
set(RAMDISK_ROOT_APPS)

set(USER_SHELL_COMMAND_APPS
    help args ls cat grep head tail wc cut uniq sort pwd cd exec pid spawn wait sleep yield
    bg fg jobs kill ps top
    procstat sysstat
    shutdown restart exit clear ansi ansitest wavplay fastfetch memstat fsstat taskstat userstat
    shstat stats tty dmesg kbdstat mkdir touch write append cp mv rm kdbg
)

foreach(SRC IN LISTS USER_APP_MAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_main$" "" _app_name "${_stem}")
    string(TOUPPER "${_app_name}" _app_name_upper)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _app_name_key "${_app_name_upper}")
    set(_app_enabled_var "CLEONOS_USER_APP_${_app_name_key}")

    list(FIND USER_KELF_APP_NAMES "${_app_name}" _shadowed_by_kmain)
    if(NOT _shadowed_by_kmain EQUAL -1)
        cl_log_info("skip ${SRC} because ${_app_name}_kmain.c exists")
        continue()
    endif()

    if(NOT DEFINED ${_app_enabled_var})
        set(${_app_enabled_var} ON CACHE BOOL "Build user app ${_app_name}")
    else()
        set(${_app_enabled_var} ${${_app_enabled_var}} CACHE BOOL "Build user app ${_app_name}" FORCE)
    endif()

    if(NOT ${_app_enabled_var})
        cl_log_info("menuconfig: skip disabled user app ${_app_name}")
        continue()
    endif()

    list(FIND USER_APP_NAMES "${_app_name}" _dup_name_idx)
    if(NOT _dup_name_idx EQUAL -1)
        cl_log_error("duplicate user app name: ${_app_name}")
    endif()
    list(APPEND USER_APP_NAMES "${_app_name}")

    add_user_c_object("${SRC}" _user_obj)

    set(_app_specific_objs)
    file(GLOB _app_specific_abs CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/cleonos/c/apps/${_app_name}_*.c"
        "${CMAKE_SOURCE_DIR}/cleonos/c/apps/${_app_name}/*.c"
    )

    list(FIND USER_SHELL_COMMAND_APPS "${_app_name}" _shell_cmd_idx)
    if(NOT _shell_cmd_idx EQUAL -1)
        list(APPEND _app_specific_abs "${CMAKE_SOURCE_DIR}/cleonos/c/apps/cmd_runtime.c")
    endif()
    foreach(_extra_abs IN LISTS _app_specific_abs)
        file(RELATIVE_PATH _extra_rel "${CMAKE_SOURCE_DIR}" "${_extra_abs}")

        if(_extra_rel STREQUAL "${SRC}")
            continue()
        endif()

        if(_extra_rel MATCHES "_kmain\\.c$")
            continue()
        endif()

        add_user_c_object("${_extra_rel}" _extra_obj)
        list(APPEND _app_specific_objs "${_extra_obj}")
    endforeach()
    list(REMOVE_DUPLICATES _app_specific_objs)

    set(_app_out "${USER_APP_DIR}/${_app_name}.elf")
    add_custom_command(
        OUTPUT "${_app_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
        COMMAND ${USER_LD} ${USER_LDFLAGS} -o "${_app_out}" ${USER_COMMON_OBJECTS} "${_user_obj}" ${_app_specific_objs} "${USER_RUST_LIB}"
        DEPENDS ${USER_COMMON_OBJECTS} "${_user_obj}" ${_app_specific_objs} "${USER_RUST_LIB}" "${USER_LINKER_SCRIPT}"
        VERBATIM
    )

    list(APPEND USER_APP_OUTPUTS "${_app_out}")
    if(_app_name MATCHES ".*drv$")
        list(APPEND RAMDISK_DRIVER_APPS "${_app_out}")
    elseif(_app_name STREQUAL "hello")
        list(APPEND RAMDISK_ROOT_APPS "${_app_out}")
    else()
        list(APPEND RAMDISK_SHELL_APPS "${_app_out}")
    endif()
endforeach()

foreach(SRC IN LISTS USER_APP_KMAIN_SOURCES)
    get_filename_component(_stem "${SRC}" NAME_WE)
    string(REGEX REPLACE "_kmain$" "" _app_name "${_stem}")
    string(TOUPPER "${_app_name}" _app_name_upper)
    string(REGEX REPLACE "[^A-Z0-9]" "_" _app_name_key "${_app_name_upper}")
    set(_app_enabled_var "CLEONOS_USER_APP_${_app_name_key}")

    if(NOT DEFINED ${_app_enabled_var})
        set(${_app_enabled_var} ON CACHE BOOL "Build user app ${_app_name}")
    else()
        set(${_app_enabled_var} ${${_app_enabled_var}} CACHE BOOL "Build user app ${_app_name}" FORCE)
    endif()

    if(NOT ${_app_enabled_var})
        cl_log_info("menuconfig: skip disabled user app ${_app_name}")
        continue()
    endif()

    list(FIND USER_APP_NAMES "${_app_name}" _dup_name_idx)
    if(NOT _dup_name_idx EQUAL -1)
        cl_log_error("duplicate user app name: ${_app_name}")
    endif()
    list(APPEND USER_APP_NAMES "${_app_name}")

    add_user_c_object("${SRC}" _kelf_obj)
    set(_app_out "${USER_APP_DIR}/${_app_name}.elf")
    add_custom_command(
        OUTPUT "${_app_out}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${USER_APP_DIR}"
        COMMAND ${USER_LD} ${KELF_LDFLAGS} -o "${_app_out}" "${_kelf_obj}"
        DEPENDS "${_kelf_obj}" "${KELF_LINKER_SCRIPT}"
        VERBATIM
    )

    list(APPEND USER_APP_OUTPUTS "${_app_out}")
    list(APPEND RAMDISK_SYSTEM_APPS "${_app_out}")
endforeach()

if(NOT USER_APP_OUTPUTS)
    cl_log_error("no user ELF apps were discovered")
endif()

add_custom_target(userapps DEPENDS ${USER_APP_OUTPUTS})

add_custom_command(
    TARGET userapps
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=user elf apps ready"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

file(GLOB_RECURSE RAMDISK_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/ramdisk/*")
set(RAMDISK_ROOT_STAMP "${BUILD_ROOT}/.ramdisk_root.stamp")
set(RAMDISK_COPY_COMMANDS)
foreach(_app IN LISTS RAMDISK_SHELL_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/shell/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_ROOT_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_DRIVER_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/driver/${_app_file}")
endforeach()
foreach(_app IN LISTS RAMDISK_SYSTEM_APPS)
    get_filename_component(_app_file "${_app}" NAME)
    list(APPEND RAMDISK_COPY_COMMANDS COMMAND ${CMAKE_COMMAND} -E copy "${_app}" "${RAMDISK_ROOT}/system/${_app_file}")
endforeach()

add_custom_command(
    OUTPUT "${RAMDISK_ROOT_STAMP}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/ramdisk" "${RAMDISK_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${RAMDISK_ROOT}/system" "${RAMDISK_ROOT}/shell" "${RAMDISK_ROOT}/driver" "${RAMDISK_ROOT}/dev"
    COMMAND ${CMAKE_COMMAND} -E copy "${KERNEL_SYMBOLS_FILE}" "${RAMDISK_ROOT}/system/kernel.sym"
    ${RAMDISK_COPY_COMMANDS}
    COMMAND ${CMAKE_COMMAND} -E touch "${RAMDISK_ROOT_STAMP}"
    DEPENDS ${RAMDISK_FILES} ${USER_APP_OUTPUTS} "${KERNEL_SYMBOLS_FILE}"
    VERBATIM
)

add_custom_target(ramdisk-root DEPENDS "${RAMDISK_ROOT_STAMP}")
add_dependencies(ramdisk-root userapps)

add_custom_command(
    OUTPUT "${RAMDISK_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${BUILD_ROOT}"
    COMMAND ${TAR} -C "${RAMDISK_ROOT}" -cf "${RAMDISK_IMAGE}" .
    DEPENDS "${RAMDISK_ROOT_STAMP}"
    VERBATIM
)

add_custom_target(ramdisk DEPENDS "${RAMDISK_IMAGE}")

add_custom_target(setup-tools
    COMMAND ${CMAKE_COMMAND}
        "-DGIT_TOOL=${GIT_TOOL}"
        "-DTAR_TOOL=${TAR}"
        "-DXORRISO_TOOL=${XORRISO}"
        "-DNO_COLOR=${NO_COLOR}"
        "-DCC_TOOL=${CC}"
        "-DLD_TOOL=${LD}"
        "-DOBJCOPY_TOOL=${OBJCOPY_FOR_TARGET}"
        "-DOBJDUMP_TOOL=${OBJDUMP_FOR_TARGET}"
        "-DREADELF_TOOL=${READELF_FOR_TARGET}"
        "-DNM_TOOL=${NM}"
        "-DADDR2LINE_TOOL=${ADDR2LINE}"
        "-DUSER_CC_TOOL=${USER_CC}"
        "-DUSER_LD_TOOL=${USER_LD}"
        "-DRUSTC_TOOL=${RUSTC}"
        "-DMAKE_TOOL=${MAKE_TOOL}"
        "-DSH_TOOL=${SH_TOOL}"
        -P "${CMAKE_SOURCE_DIR}/cmake/check_tools.cmake"
)

add_custom_target(setup-limine
    COMMAND ${CMAKE_COMMAND}
        "-DGIT_TOOL=${GIT_TOOL}"
        "-DMAKE_TOOL=${MAKE_TOOL}"
        "-DSH_TOOL=${SH_TOOL}"
        "-DNO_COLOR=${NO_COLOR}"
        "-DLIMINE_DIR=${LIMINE_DIR_ABS}"
        "-DLIMINE_REPO=${LIMINE_REPO}"
        "-DLIMINE_REF=${LIMINE_REF}"
        "-DLIMINE_BIN_DIR=${LIMINE_BIN_DIR_ABS}"
        "-DLIMINE_SETUP_STAMP=${LIMINE_SETUP_STAMP}"
        "-DLIMINE_BUILD_STAMP=${LIMINE_BUILD_STAMP}"
        "-DLIMINE_CONFIGURE_FLAGS=${LIMINE_CONFIGURE_FLAGS}"
        "-DLIMINE_SKIP_CONFIGURE=${LIMINE_SKIP_CONFIGURE}"
        "-DOBJCOPY_FOR_TARGET=${OBJCOPY_FOR_TARGET}"
        "-DOBJDUMP_FOR_TARGET=${OBJDUMP_FOR_TARGET}"
        "-DREADELF_FOR_TARGET=${READELF_FOR_TARGET}"
        -P "${CMAKE_SOURCE_DIR}/cmake/setup_limine.cmake"
)
add_dependencies(setup-limine setup-tools)

add_custom_target(setup DEPENDS setup-tools setup-limine)
add_custom_command(
    TARGET setup
    POST_BUILD
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=environment ready"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

add_custom_command(
    OUTPUT "${ISO_IMAGE}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${ISO_ROOT}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${ISO_ROOT}/boot/limine" "${ISO_ROOT}/EFI/BOOT"
    COMMAND ${CMAKE_COMMAND} -E copy "${KERNEL_ELF}" "${ISO_ROOT}/boot/clks_kernel.elf"
    COMMAND ${CMAKE_COMMAND} -E copy "${RAMDISK_IMAGE}" "${ISO_ROOT}/boot/cleonos_ramdisk.tar"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/boot/limine/limine.conf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/EFI/BOOT/limine.conf"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_SOURCE_DIR}/configs/limine.conf" "${ISO_ROOT}/limine.conf"
    COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-bios.sys" "${ISO_ROOT}/boot/limine/limine-bios.sys"
    COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-bios-cd.bin" "${ISO_ROOT}/boot/limine/limine-bios-cd.bin"
    COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/limine-uefi-cd.bin" "${ISO_ROOT}/boot/limine/limine-uefi-cd.bin"
    COMMAND ${CMAKE_COMMAND} -E copy "${LIMINE_BIN_DIR_ABS}/BOOTX64.EFI" "${ISO_ROOT}/EFI/BOOT/BOOTX64.EFI"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_SOURCE_DIR}/build"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=building iso -> ${ISO_IMAGE}"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${XORRISO} -as mkisofs
        -b boot/limine/limine-bios-cd.bin
        -no-emul-boot
        -boot-load-size 4
        -boot-info-table
        --efi-boot boot/limine/limine-uefi-cd.bin
        -efi-boot-part
        --efi-boot-image
        --protective-msdos-label
        "${ISO_ROOT}"
        -o "${ISO_IMAGE}"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=installing limine boot sectors"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND "${LIMINE_BIN_DIR_ABS}/limine" bios-install "${ISO_IMAGE}"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=iso ready: ${ISO_IMAGE}"
        -P "${CL_LOG_EMIT_SCRIPT}"
    DEPENDS "${KERNEL_ELF}" "${RAMDISK_IMAGE}" "${CMAKE_SOURCE_DIR}/configs/limine.conf"
    VERBATIM
)

add_custom_target(iso ALL DEPENDS "${ISO_IMAGE}")
add_dependencies(iso setup-tools setup-limine kernel ramdisk)

add_custom_target(run
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=launching qemu run"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${QEMU_X86_64} -M q35 -m 1024M -cdrom "${ISO_IMAGE}" -serial stdio
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(debug
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=launching qemu debug (-s -S)"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${QEMU_X86_64} -M q35 -m 1024M -cdrom "${ISO_IMAGE}" -serial stdio -s -S
    DEPENDS iso
    USES_TERMINAL
)

add_custom_target(clean-x86
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=cleaning ${BUILD_ROOT}"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${BUILD_ROOT}"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=clean done"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

add_custom_target(clean-all
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=STEP"
        "-DLOG_TEXT=cleaning build"
        -P "${CL_LOG_EMIT_SCRIPT}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${CMAKE_SOURCE_DIR}/build"
    COMMAND ${CMAKE_COMMAND}
        "-DNO_COLOR=${NO_COLOR}"
        "-DLOG_LEVEL=INFO"
        "-DLOG_TEXT=clean-all done"
        -P "${CL_LOG_EMIT_SCRIPT}"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(menuconfig
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui
        COMMAND ${Python3_EXECUTABLE} "${CMAKE_SOURCE_DIR}/scripts/menuconfig.py" --gui
        USES_TERMINAL
    )
else()
    add_custom_target(menuconfig
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py manually"
        USES_TERMINAL
    )
    add_custom_target(menuconfig-gui
        COMMAND ${CMAKE_COMMAND} -E echo "python3 not found; run scripts/menuconfig.py --gui manually"
        USES_TERMINAL
    )
endif()

add_custom_target(cleonos-help
    COMMAND ${CMAKE_COMMAND} -E echo "CLeonOS CMake build system (x86_64 only)"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake -S . -B build-cmake"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target menuconfig-gui"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target setup"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target userapps"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target iso"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target run"
    COMMAND ${CMAKE_COMMAND} -E echo "  cmake --build build-cmake --target debug"
)

