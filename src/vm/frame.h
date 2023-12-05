#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "vm/page.h"

struct frame {
  struct page *page;
  bool is_free;
  struct hash_elem elem;
  // other fields if needed
};



unsigned frame_hash(const struct hash_elem *e, void *aux);
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void *frame_get_page(struct spt_entry *spte);
void frame_table_init(void);
void* allocate_frame(void);
void deallocate_frame(void* frame_addr);

#endif /* vm/frame.h */
