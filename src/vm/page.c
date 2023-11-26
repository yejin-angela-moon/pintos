#include <stdio.h>
#include "threads/malloc.h"
// could move to a header file

// one entry in the supplemental page table (SPT)
struct spt_entry {
    uint32_t user_vaddr;
    uint32_t frame_addr;
    bool in_memory; 
    struct hash_elem elem;
    // other potential fields: fd, file_offset, is_read_only, is_dirty, timestamp, swap slot, is_swapped_out
}

// page table itself
struct sup_page_table {
    struct hash table;
    // other potential fields: owner_thread, spt_lock
}


// hash table helper functions
void
spt_init (struct sup_page_table *spt) {
  spt->table = malloc(sizeof(struct hash))
  hash_init(&sup_page_table->table, spt_hash, spt_less, NULL);
}


unsigned
spt_hash (const struct hash_elem *e, void *aux UNUSED) {
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
  return hash_int(spte->spte);
}

bool 
fd_less (const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct file_descriptor *fd_a = hash_entry(a, struct file_descriptor, elem);
  struct file_descriptor *fd_b = hash_entry(b, struct file_descriptor, elem);
  return fd_a->fd < fd_b->fd;
}  




