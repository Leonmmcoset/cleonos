#ifndef CLKS_SERVICE_H
#define CLKS_SERVICE_H

#include <clks/types.h>

#define CLKS_SERVICE_NAME_MAX 24U

enum clks_service_id {
    CLKS_SERVICE_LOG = 1,
    CLKS_SERVICE_MEM = 2,
    CLKS_SERVICE_FS = 3,
    CLKS_SERVICE_DRIVER = 4,
    CLKS_SERVICE_SCHED = 5,
    CLKS_SERVICE_KELF = 6,
    CLKS_SERVICE_USER = 7,
};

enum clks_service_state {
    CLKS_SERVICE_STATE_OFFLINE = 0,
    CLKS_SERVICE_STATE_READY = 1,
    CLKS_SERVICE_STATE_DEGRADED = 2,
};

struct clks_service_info {
    u32 id;
    char name[CLKS_SERVICE_NAME_MAX];
    enum clks_service_state state;
    u64 heartbeat_count;
    u64 last_heartbeat_tick;
};

void clks_service_init(void);
clks_bool clks_service_heartbeat(u32 service_id, u64 tick);
u64 clks_service_count(void);
u64 clks_service_ready_count(void);
clks_bool clks_service_get(u32 service_id, struct clks_service_info *out_info);

#endif
