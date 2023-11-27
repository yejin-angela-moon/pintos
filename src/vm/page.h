#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "../lib/kernel/hash.h"
#include "../threads/synch.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// one entry in the supplemental page table (SPT)
struct page_entry {
    uint32_t user_vaddr;
    uint32_t frame_addr;
    bool in_memory; 
    struct hash_elem elem;
    // other potential fields: fd, file_offset, is_read_only, is_dirty, timestamp, swap slot, is_swapped_out
};

unsigned page_hash (const struct hash_elem *e, void *aux);

bool page_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

#endif
