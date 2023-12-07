#include "vm/swap.h"
#include "devices/swap.h"
#include <stdlib.h>
#include <stdio.h>
#include "threads/vaddr.h"
#include "devices/block.h"
#include "lib/debug.h"
#include "threads/synch.h"
#include <threads/malloc.h>
#include <threads/palloc.h>

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)

struct block *swap_block;  // Block device for the swap space

struct lock swap_lock;

struct list swap_list;

void swap_vm_init(void) {
  list_init(&swap_list);
  lock_init(&swap_lock);  
  swap_block = block_get_role(BLOCK_SWAP);

  if (swap_block == NULL) {
    return;
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

size_t swap_out_memory(void *user_vaddr) {
  if (swap_block == NULL || list_empty(&swap_list)) {
    return SWAP_ERROR;
  }


  if (list_size(&swap_list) < (block_size(swap_block) / SECTORS_PER_PAGE)) {
    struct swap_store *ss = malloc(sizeof(struct swap_store));
    if (ss == NULL) {
      return SWAP_ERROR;
    }
    ss->ssid = list_size(&swap_list);
    lock_acquire(&swap_lock);
    list_push_back(&swap_list, &ss->elem);
    lock_release(&swap_lock);
    for (int i = 0; i < SECTORS_PER_PAGE; i++) {
      block_write (swap_block, ss->ssid * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
    }
    lock_release(&swap_lock);
    return ss->ssid;
  } else {
    return SWAP_ERROR;
  } 
}

static struct swap_store *get_swap_store_by_ssid(size_t swap_slot) {
  struct swap_store *ss = NULL;
  for (struct list_elem *e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
    ss = list_entry(e, struct swap_store, elem);
    if (ss->ssid == (int) swap_slot) {
      break;
    }
  }
  return ss;
}

void swap_in_memory(size_t swap_slot, void *user_vaddr) {
  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_ssid(swap_slot);
  if (ss == NULL) {
    lock_release(&swap_lock);
    return;
  }

  list_remove(&ss->elem);

  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_read (swap_block, swap_slot * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
  }

  lock_release(&swap_lock);
}
