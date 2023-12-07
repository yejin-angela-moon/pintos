#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "devices/block.h"
#include <list.h>

#define INVALID_SWAP_SLOT SIZE_MAX
#define SWAP_ERROR -1

struct swap_store {
  struct list_elem elem;
  int ssid;
};

void swap_vm_init(void);
//size_t swap_out(void *frame);
size_t swap_out_memory (void *vaddr);
void swap_in_memory(size_t swap_slot, void *vaddr);
void swap_free(size_t swap_slot);

#endif /* vm/swap.h */

