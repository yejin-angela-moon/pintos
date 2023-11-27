#include "vm/page.h"
#include "threads/malloc.h"

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




