#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "../lib/kernel/hash.h"
#include "../threads/synch.h"
#include "../threads/palloc.h"

struct frame_entry {
  struct spt_entry *page;     
  bool in_use;            
  struct hash_elem hash_elem;
};

struct frame_table {
  struct hash table;
  struct lock table_lock;
};



unsigned frame_hash(const struct hash_elem *elem, void *aux);

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void *allocate_frame(enum palloc_flags flags, void *aux);

void free_frame(struct spt_entry *page);



#endif
