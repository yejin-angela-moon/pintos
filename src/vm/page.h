#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "../lib/kernel/hash.h"
#include "../threads/synch.h"
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

// one entry in the supplemental page table (SPT)
struct spt_entry {
    uint32_t user_vaddr;
    uint32_t frame_addr;
    bool in_memory; 
    struct hash_elem elem;
    // other potential fields: fd, file_offset, is_read_only, is_dirty, timestamp, swap slot, is_swapped_out
    // lazy loading fields
    off_t file_offset;
    uint32_t read_bytes;
    uint32_t zero_bytes;
    bool writable;
    struct file *file;
};

// page table itself
struct sup_page_table {
  struct hash table;
  struct lock spt_lock;
  // other potential fields: owner_thread, spt_lock

};

void spt_init (struct sup_page_table *spt);

unsigned spt_hash (const struct hash_elem *e, void *aux UNUSED);

bool spt_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED);

#endif
