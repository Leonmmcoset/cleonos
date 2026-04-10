typedef unsigned long long u64;

static u64 clks_local_counter = 0ULL;

u64 cleonos_kelf_entry(u64 tick, u64 run_count) {
    clks_local_counter += (run_count ^ tick) & 0xFFULL;
    return clks_local_counter;
}
