typedef unsigned long long u64;

static u64 clks_memc_state = 0ULL;

u64 cleonos_kelf_entry(u64 tick, u64 run_count) {
    clks_memc_state += (tick & 0x1FULL) + run_count;
    return clks_memc_state;
}
