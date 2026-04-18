#include <clks/driver.h>
#include <clks/fs.h>
#include <clks/heap.h>
#include <clks/kelf.h>
#include <clks/log.h>
#include <clks/scheduler.h>
#include <clks/service.h>
#include <clks/string.h>
#include <clks/types.h>
#include <clks/userland.h>

#define CLKS_SERVICE_MAX 8U

static struct clks_service_info clks_services[CLKS_SERVICE_MAX];
static u64 clks_service_used = 0ULL;

static void clks_service_copy_name(char *dst, usize dst_size, const char *src) {
    usize i = 0U;

    if (dst == CLKS_NULL || dst_size == 0U) {
        return;
    }

    while (i + 1U < dst_size && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }

    dst[i] = '\0';
}

static i32 clks_service_find_index(u32 service_id) {
    u64 i;

    for (i = 0ULL; i < clks_service_used; i++) {
        if (clks_services[i].id == service_id) {
            return (i32)i;
        }
    }

    return -1;
}

static clks_bool clks_service_register(u32 id, const char *name, enum clks_service_state state) {
    struct clks_service_info *slot;

    if (clks_service_used >= CLKS_SERVICE_MAX) {
        return CLKS_FALSE;
    }

    slot = &clks_services[clks_service_used];
    clks_memset(slot, 0, sizeof(*slot));

    slot->id = id;
    clks_service_copy_name(slot->name, sizeof(slot->name), name);
    slot->state = state;
    slot->heartbeat_count = 0ULL;
    slot->last_heartbeat_tick = 0ULL;

    clks_service_used++;
    return CLKS_TRUE;
}

void clks_service_init(void) {
    struct clks_heap_stats heap_stats;

    clks_memset(clks_services, 0, sizeof(clks_services));
    clks_service_used = 0ULL;

    heap_stats = clks_heap_get_stats();

    clks_service_register(CLKS_SERVICE_LOG, "log", CLKS_SERVICE_STATE_READY);
    clks_service_register(CLKS_SERVICE_MEM, "memory",
                          (heap_stats.total_bytes > 0U) ? CLKS_SERVICE_STATE_READY : CLKS_SERVICE_STATE_DEGRADED);
    clks_service_register(CLKS_SERVICE_FS, "filesystem",
                          (clks_fs_is_ready() == CLKS_TRUE) ? CLKS_SERVICE_STATE_READY : CLKS_SERVICE_STATE_DEGRADED);
    clks_service_register(CLKS_SERVICE_DRIVER, "driver",
                          (clks_driver_count() > 0ULL) ? CLKS_SERVICE_STATE_READY : CLKS_SERVICE_STATE_DEGRADED);
    clks_service_register(CLKS_SERVICE_SCHED, "scheduler", CLKS_SERVICE_STATE_READY);
    clks_service_register(CLKS_SERVICE_KELF, "kelf",
                          (clks_kelf_count() > 0ULL) ? CLKS_SERVICE_STATE_READY : CLKS_SERVICE_STATE_DEGRADED);
    clks_service_register(CLKS_SERVICE_USER, "userland",
                          (clks_userland_shell_ready() == CLKS_TRUE) ? CLKS_SERVICE_STATE_READY
                                                                     : CLKS_SERVICE_STATE_DEGRADED);

    clks_log(CLKS_LOG_INFO, "SRV", "KERNEL SERVICES ONLINE");
    clks_log_hex(CLKS_LOG_INFO, "SRV", "COUNT", clks_service_count());
    clks_log_hex(CLKS_LOG_INFO, "SRV", "READY", clks_service_ready_count());
}

clks_bool clks_service_heartbeat(u32 service_id, u64 tick) {
    i32 idx = clks_service_find_index(service_id);

    if (idx < 0) {
        return CLKS_FALSE;
    }

    clks_services[(u32)idx].heartbeat_count++;
    clks_services[(u32)idx].last_heartbeat_tick = tick;

    if (clks_services[(u32)idx].state == CLKS_SERVICE_STATE_OFFLINE) {
        clks_services[(u32)idx].state = CLKS_SERVICE_STATE_READY;
    }

    return CLKS_TRUE;
}

u64 clks_service_count(void) {
    return clks_service_used;
}

u64 clks_service_ready_count(void) {
    u64 i;
    u64 ready = 0ULL;

    for (i = 0ULL; i < clks_service_used; i++) {
        if (clks_services[i].state == CLKS_SERVICE_STATE_READY) {
            ready++;
        }
    }

    return ready;
}

clks_bool clks_service_get(u32 service_id, struct clks_service_info *out_info) {
    i32 idx;

    if (out_info == CLKS_NULL) {
        return CLKS_FALSE;
    }

    idx = clks_service_find_index(service_id);

    if (idx < 0) {
        return CLKS_FALSE;
    }

    *out_info = clks_services[(u32)idx];
    return CLKS_TRUE;
}
