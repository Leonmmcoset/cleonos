#ifndef CLEONOS_RUST_BRIDGE_H
#define CLEONOS_RUST_BRIDGE_H

typedef unsigned long long u64;
typedef unsigned long long usize;

u64 cleonos_rust_guarded_len(const unsigned char *ptr, usize max_len);

#endif
