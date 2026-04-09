#ifndef CLKS_LOG_H
#define CLKS_LOG_H

#include <clks/types.h>

enum clks_log_level {
    CLKS_LOG_DEBUG = 0,
    CLKS_LOG_INFO = 1,
    CLKS_LOG_WARN = 2,
    CLKS_LOG_ERROR = 3,
};

void clks_log(enum clks_log_level level, const char *tag, const char *message);
void clks_log_hex(enum clks_log_level level, const char *tag, const char *label, u64 value);

#endif