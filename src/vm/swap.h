#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stddef.h>

#define INVALID_SWAP_SLOT SIZE_MAX

void swap_init(void);
size_t swap_out(void *frame);
void swap_in(size_t swap_slot, void *frame);
void swap_free(size_t swap_slot);

#endif /* vm/swap.h */

