#include "vm/swap.h"
#include "devices/swap.h"
#include <stdlib.h>
#include <stdio.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "lib/debug.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap_block;  // Block device for the swap space

void swap_init(void) {
    swap_block = block_get_role(BLOCK_SWAP);
    if (swap_block == NULL) {
        return false;
    }
}

void swap_read(size_t swap_slot, void *frame) {
    ASSERT(swap_block != NULL);
    ASSERT(swap_slot != INVALID_SWAP_SLOT);

    block_sector_t start_sector = swap_slot * SECTORS_PER_PAGE;

    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
        block_read(swap_block, start_sector + i, frame + i * BLOCK_SECTOR_SIZE);
    }
}

