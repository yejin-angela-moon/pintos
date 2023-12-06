#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include <stdint.h>


struct frame_entry {
  struct spt_entry *page;     
  bool in_use;            
  struct hash_elem elem;
  void *physical_addr;
  tid_t tid;
  void *frame; 
};

struct frame_table {
  struct hash table;
  struct lock table_lock;
};



unsigned frame_hash(const struct hash_elem *elem, void *aux);

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void frame_init(void);

void *allocate_frame(enum palloc_flags flags);

void frame_free_or_remove(void *page, bool free_page);

void remove_frame_entry (void *page);

void free_frame(void *page);



#endif
