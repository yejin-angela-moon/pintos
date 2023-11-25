#include <stdio.h>
// could move to a header file

// one entry in the supplemental page table (SPT)
struct spt_entry {
    uint32_t user_vaddr;
    uint32_t frame_addr;
    bool swapped;  // true -> user_addr is in the frame frame_addr
    struct hash_elem elem;
}

struct s_page_table {
    struct hash table;
}

/*
to initialise table:

hash_init(&s_page_table, spt_hash, spt_less, NULL);

where:

unsigned
spt_hash(const struct hash_elem *e, void *aux UNUSED) {
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
  return hash_int(spte->spte);
}

bool 
fd_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct file_descriptor *fd_a = hash_entry(a, struct file_descriptor, elem);
  struct file_descriptor *fd_b = hash_entry(b, struct file_descriptor, elem);
  return fd_a->fd < fd_b->fd;
}  
*/