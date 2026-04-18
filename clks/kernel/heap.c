#include <clks/heap.h>
#include <clks/string.h>
#include <clks/types.h>

#define CLKS_HEAP_ARENA_SIZE (1024ULL * 1024ULL)
#define CLKS_HEAP_ALIGN 16ULL
#define CLKS_HEAP_MAGIC 0x434C454F4E4F534FULL

struct clks_heap_block {
    usize size;
    clks_bool is_free;
    struct clks_heap_block *next;
    struct clks_heap_block *prev;
    u64 magic;
};

static u8 clks_heap_arena[CLKS_HEAP_ARENA_SIZE] __attribute__((aligned(16)));
static struct clks_heap_block *clks_heap_head = CLKS_NULL;
static clks_bool clks_heap_ready = CLKS_FALSE;

static usize clks_heap_used_bytes = 0;
static u64 clks_heap_alloc_count = 0;
static u64 clks_heap_free_count = 0;

static usize clks_heap_align_up(usize value) {
    return (value + (CLKS_HEAP_ALIGN - 1ULL)) & ~(CLKS_HEAP_ALIGN - 1ULL);
}

static void clks_heap_split_block(struct clks_heap_block *block, usize need_size) {
    usize min_tail_size = sizeof(struct clks_heap_block) + CLKS_HEAP_ALIGN;

    if (block->size < (need_size + min_tail_size)) {
        return;
    }

    {
        u8 *new_block_addr = (u8 *)block + sizeof(struct clks_heap_block) + need_size;
        struct clks_heap_block *new_block = (struct clks_heap_block *)new_block_addr;

        new_block->size = block->size - need_size - sizeof(struct clks_heap_block);
        new_block->is_free = CLKS_TRUE;
        new_block->next = block->next;
        new_block->prev = block;
        new_block->magic = CLKS_HEAP_MAGIC;

        if (block->next != CLKS_NULL) {
            block->next->prev = new_block;
        }

        block->next = new_block;
        block->size = need_size;
    }
}

static void clks_heap_merge_next(struct clks_heap_block *block) {
    struct clks_heap_block *next = block->next;

    if (next == CLKS_NULL) {
        return;
    }

    if (next->is_free == CLKS_FALSE) {
        return;
    }

    if (next->magic != CLKS_HEAP_MAGIC) {
        return;
    }

    block->size += sizeof(struct clks_heap_block) + next->size;
    block->next = next->next;

    if (next->next != CLKS_NULL) {
        next->next->prev = block;
    }
}

void clks_heap_init(void) {
    clks_memset(clks_heap_arena, 0, sizeof(clks_heap_arena));

    clks_heap_head = (struct clks_heap_block *)clks_heap_arena;
    clks_heap_head->size = CLKS_HEAP_ARENA_SIZE - sizeof(struct clks_heap_block);
    clks_heap_head->is_free = CLKS_TRUE;
    clks_heap_head->next = CLKS_NULL;
    clks_heap_head->prev = CLKS_NULL;
    clks_heap_head->magic = CLKS_HEAP_MAGIC;

    clks_heap_used_bytes = 0;
    clks_heap_alloc_count = 0;
    clks_heap_free_count = 0;
    clks_heap_ready = CLKS_TRUE;
}

void *clks_kmalloc(usize size) {
    struct clks_heap_block *current;
    usize aligned_size;

    if (clks_heap_ready == CLKS_FALSE) {
        return CLKS_NULL;
    }

    if (size == 0) {
        return CLKS_NULL;
    }

    aligned_size = clks_heap_align_up(size);
    current = clks_heap_head;

    while (current != CLKS_NULL) {
        if (current->magic != CLKS_HEAP_MAGIC) {
            return CLKS_NULL;
        }

        if (current->is_free == CLKS_TRUE && current->size >= aligned_size) {
            clks_heap_split_block(current, aligned_size);
            current->is_free = CLKS_FALSE;
            clks_heap_used_bytes += current->size;
            clks_heap_alloc_count++;
            return (void *)((u8 *)current + sizeof(struct clks_heap_block));
        }

        current = current->next;
    }

    return CLKS_NULL;
}

void clks_kfree(void *ptr) {
    struct clks_heap_block *block;

    if (clks_heap_ready == CLKS_FALSE) {
        return;
    }

    if (ptr == CLKS_NULL) {
        return;
    }

    block = (struct clks_heap_block *)((u8 *)ptr - sizeof(struct clks_heap_block));

    if (block->magic != CLKS_HEAP_MAGIC) {
        return;
    }

    if (block->is_free == CLKS_TRUE) {
        return;
    }

    block->is_free = CLKS_TRUE;

    if (clks_heap_used_bytes >= block->size) {
        clks_heap_used_bytes -= block->size;
    } else {
        clks_heap_used_bytes = 0;
    }

    clks_heap_free_count++;

    clks_heap_merge_next(block);

    if (block->prev != CLKS_NULL && block->prev->is_free == CLKS_TRUE) {
        clks_heap_merge_next(block->prev);
    }
}

struct clks_heap_stats clks_heap_get_stats(void) {
    struct clks_heap_stats stats;

    stats.total_bytes = CLKS_HEAP_ARENA_SIZE - sizeof(struct clks_heap_block);
    stats.used_bytes = clks_heap_used_bytes;
    stats.free_bytes = stats.total_bytes - stats.used_bytes;
    stats.alloc_count = clks_heap_alloc_count;
    stats.free_count = clks_heap_free_count;

    return stats;
}