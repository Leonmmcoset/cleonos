#include <clks/boot.h>
#include <clks/compiler.h>

CLKS_USED static volatile u64 limine_requests_start[]
    __attribute__((section(".limine_requests_start"))) = LIMINE_REQUESTS_START_MARKER;

CLKS_USED static volatile u64 limine_base_revision[]
    __attribute__((section(".limine_requests"))) = LIMINE_BASE_REVISION(3);

CLKS_USED static volatile struct limine_framebuffer_request limine_framebuffer_request
    __attribute__((section(".limine_requests"))) = {
        .id = LIMINE_FRAMEBUFFER_REQUEST,
        .revision = 0,
        .response = CLKS_NULL,
    };

CLKS_USED static volatile struct limine_memmap_request limine_memmap_request
    __attribute__((section(".limine_requests"))) = {
        .id = LIMINE_MEMMAP_REQUEST,
        .revision = 0,
        .response = CLKS_NULL,
    };

CLKS_USED static volatile struct limine_executable_file_request limine_executable_file_request
    __attribute__((section(".limine_requests"))) = {
        .id = LIMINE_EXECUTABLE_FILE_REQUEST,
        .revision = 0,
        .response = CLKS_NULL,
    };

CLKS_USED static volatile struct limine_module_request limine_module_request
    __attribute__((section(".limine_requests"))) = {
        .id = LIMINE_MODULE_REQUEST,
        .revision = 0,
        .response = CLKS_NULL,
    };

CLKS_USED static volatile u64 limine_requests_end[]
    __attribute__((section(".limine_requests_end"))) = LIMINE_REQUESTS_END_MARKER;

clks_bool clks_boot_base_revision_supported(void) {
    return (limine_base_revision[2] == 0) ? CLKS_TRUE : CLKS_FALSE;
}

const struct limine_framebuffer *clks_boot_get_framebuffer(void) {
    volatile struct limine_framebuffer_request *request = &limine_framebuffer_request;

    if (request->response == CLKS_NULL) {
        return CLKS_NULL;
    }

    if (request->response->framebuffer_count < 1) {
        return CLKS_NULL;
    }

    return request->response->framebuffers[0];
}

const struct limine_memmap_response *clks_boot_get_memmap(void) {
    volatile struct limine_memmap_request *request = &limine_memmap_request;

    if (request->response == CLKS_NULL) {
        return CLKS_NULL;
    }

    return request->response;
}

const struct limine_file *clks_boot_get_executable_file(void) {
    volatile struct limine_executable_file_request *request = &limine_executable_file_request;

    if (request->response == CLKS_NULL) {
        return CLKS_NULL;
    }

    return request->response->executable_file;
}

u64 clks_boot_get_module_count(void) {
    volatile struct limine_module_request *request = &limine_module_request;

    if (request->response == CLKS_NULL) {
        return 0ULL;
    }

    return request->response->module_count;
}

const struct limine_file *clks_boot_get_module(u64 index) {
    volatile struct limine_module_request *request = &limine_module_request;

    if (request->response == CLKS_NULL) {
        return CLKS_NULL;
    }

    if (index >= request->response->module_count) {
        return CLKS_NULL;
    }

    return request->response->modules[index];
}
