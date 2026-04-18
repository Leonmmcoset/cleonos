#include <clks/pmm.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_PMM_MAX_TRACKED_PAGES 262144ULL
#define CLKS_PMM_MIN_USABLE_ADDR 0x100000ULL

static u64 clks_pmm_free_stack[CLKS_PMM_MAX_TRACKED_PAGES];
static u64 clks_pmm_free_top = 0;
static u64 clks_pmm_managed_pages = 0;
static u64 clks_pmm_dropped_pages = 0;

static u64 clks_align_up_u64(u64 value, u64 alignment) {
    return (value + alignment - 1ULL) & ~(alignment - 1ULL);
}

static u64 clks_align_down_u64(u64 value, u64 alignment) {
    return value & ~(alignment - 1ULL);
}

void clks_pmm_init(const struct limine_memmap_response *memmap) {
    u64 i;

    clks_pmm_free_top = 0;
    clks_pmm_managed_pages = 0;
    clks_pmm_dropped_pages = 0;
    clks_memset(clks_pmm_free_stack, 0, sizeof(clks_pmm_free_stack));

    if (memmap == CLKS_NULL) {
        return;
    }

    for (i = 0; i < memmap->entry_count; i++) {
        const struct limine_memmap_entry *entry = memmap->entries[i];
        u64 start;
        u64 end;
        u64 addr;

        if (entry == CLKS_NULL) {
            continue;
        }

        if (entry->type != LIMINE_MEMMAP_USABLE) {
            continue;
        }

        start = clks_align_up_u64(entry->base, CLKS_PAGE_SIZE);
        end = clks_align_down_u64(entry->base + entry->length, CLKS_PAGE_SIZE);

        if (end <= start) {
            continue;
        }

        for (addr = start; addr < end; addr += CLKS_PAGE_SIZE) {
            if (addr < CLKS_PMM_MIN_USABLE_ADDR) {
                continue;
            }

            if (clks_pmm_free_top < CLKS_PMM_MAX_TRACKED_PAGES) {
                clks_pmm_free_stack[clks_pmm_free_top] = addr;
                clks_pmm_free_top++;
                clks_pmm_managed_pages++;
            } else {
                clks_pmm_dropped_pages++;
            }
        }
    }
}

u64 clks_pmm_alloc_page(void) {
    if (clks_pmm_free_top == 0) {
        return 0ULL;
    }

    clks_pmm_free_top--;
    return clks_pmm_free_stack[clks_pmm_free_top];
}

void clks_pmm_free_page(u64 phys_addr) {
    if (phys_addr == 0ULL) {
        return;
    }

    if ((phys_addr & (CLKS_PAGE_SIZE - 1ULL)) != 0ULL) {
        return;
    }

    if (clks_pmm_free_top >= CLKS_PMM_MAX_TRACKED_PAGES) {
        return;
    }

    clks_pmm_free_stack[clks_pmm_free_top] = phys_addr;
    clks_pmm_free_top++;
}

struct clks_pmm_stats clks_pmm_get_stats(void) {
    struct clks_pmm_stats stats;

    stats.managed_pages = clks_pmm_managed_pages;
    stats.free_pages = clks_pmm_free_top;
    stats.used_pages = clks_pmm_managed_pages - clks_pmm_free_top;
    stats.dropped_pages = clks_pmm_dropped_pages;

    return stats;
}