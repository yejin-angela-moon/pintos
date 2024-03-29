#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include <stdint.h>
#include "threads/malloc.h"

struct frame {
  void *kpage;       /* Pointer to the kernel virtual page */
  int tid;           /* tid of the thread that owns the pagedir */
  uint8_t *user_vaddr; /* User virtual address */
  bool pinned;             /* Decides whether this frame to be evicted */
  struct hash_elem elem;
  struct list_elem lelem;
  uint32_t *pte;           /* pte when lookup page for the user vaddr and pagedir */
};

unsigned frame_hash(const struct hash_elem *e, void *aux);

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void frame_table_init(void);

void* allocate_frame(void);

void remove_frame_for_thread(void);

void deallocate_frame(void* frame_addr);

struct frame * get_frame_by_kpage (void *kpage);

void frame_set_status (void *kpage, uint32_t *pte UNUSED, void *upage);

#endif /* vm/frame.h */
