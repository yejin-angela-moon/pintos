#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "lib/kernel/hash.h"
#include "vm/page.h"
//#include "threads/thread.h"
#include "threads/vaddr.h"
#include <stdint.h>
#include "threads/malloc.h"


struct frame {
  void *kpage;       /* Pointer to the kernel virtual page */
  struct thread *t; /* Pointer to the thread */
  struct spt_entry *spte;
  uint8_t *user_vaddr; /* User virtual address */
  bool pinned;
  struct hash_elem elem;
  struct list_elem lelem;
  int no;
  // other fields if needed
};



unsigned frame_hash(const struct hash_elem *e, void *aux);
bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

//void *frame_get_page(struct spt_entry *spte);
void frame_table_init(void);
void* allocate_frame(void);
void deallocate_frame(void* frame_addr);
void frame_set_status (void *kpage, uint32_t *pte UNUSED, void *upage);
#endif /* vm/frame.h */
