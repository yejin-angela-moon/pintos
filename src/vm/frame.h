#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "vm/page.h"
//#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdint.h>
#include "threads/malloc.h"

#define SWAP_ERROR SIZE_MAX

struct frame {
  void *kpage;       /* Pointer to the kernel virtual page */
  struct thread *t; /* Pointer to the thread */
  struct spt_entry *spte;
  void *user_vaddr; /* User virtual address */
  bool pinned;
  struct hash_elem elem;
  struct list_elem lelem;
  // other fields if needed
};



unsigned frame_hash(const struct hash_elem *e, void *aux);
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void *frame_get_page(struct spt_entry *spte);
void *evict_frame (void);
void frame_table_init(void);
void* allocate_frame(void);
void free_frame (void *kpage);
void frame_remove_entry (void *kpage);
void deallocate_frame(void* frame_addr, bool free_page);
void frame_unpint (void *kpage);
void frame_pin (void *kpage);
struct frame *frame_to_evict (uint32_t *pagedir);

#endif /* vm/frame.h */
