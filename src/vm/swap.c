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
static size_t count;

/* Initialise the swap.c for vm, setting the block */
void swap_vm_init(void) {
  list_init(&swap_list);
  lock_init(&swap_lock);  
  swap_block = block_get_role(BLOCK_SWAP);
  count = 1;
  if (swap_block == NULL) {
    return;
  }
}

/* Find the swap_store by the user vaddr */
static struct swap_store *get_swap_store_by_uv(uint8_t *user_vaddr) {
  struct swap_store *ss = NULL;
  struct swap_store *get;
  for (struct list_elem *e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
    get = list_entry(e, struct swap_store, elem);
    if (get->user_vaddr == user_vaddr) {
      ss = get;
      break;
    }
  }
  return ss;
}

/* Swaps page at VADDR out of memory, returns the swap-slot used */
size_t swap_out_memory(void *user_vaddr) {
  if (swap_block == NULL) {
    return SWAP_ERROR;  
  }

  if (lock_held_by_current_thread(&swap_lock)) {
    lock_release(&swap_lock);
  }

  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_uv(user_vaddr);
  if (ss == NULL) {
    if (list_size(&swap_list) < (block_size(swap_block) / SECTORS_PER_PAGE)) {
      ss = malloc(sizeof(struct swap_store));
      if (ss == NULL) {
        return SWAP_ERROR;
      }
      ss->ssid = count;
      ss->user_vaddr = user_vaddr;
      count++;
      list_push_back(&swap_list, &ss->elem);
    } else {
      lock_release(&swap_lock);
      return SWAP_ERROR;
    }
  }
  lock_release(&swap_lock);
 
  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_write (swap_block, ss->ssid * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
  }
    
  return ss->ssid;
}

/* Get the swap store by its swap_slot */
static struct swap_store *get_swap_store_by_ssid(size_t swap_slot) {
  struct swap_store *ss = NULL;
  struct swap_store *get; 
  for (struct list_elem *e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
    get = list_entry(e, struct swap_store, elem);
    if (get->ssid == (int) swap_slot) {
      ss = get;
      break;
    }
  }
  return ss;
}

/* Swaps page on disk in swap-slot SLOT into memory at VADDR */
void swap_in_memory(size_t swap_slot, void *user_vaddr) {
  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_ssid(swap_slot);
  if (ss == NULL) {
    lock_release(&swap_lock);
    return;
  }
  lock_release(&swap_lock);

  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_read (swap_block, swap_slot * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
  }
}

/* Remove a swap store from the swap_list */
void remove_swap_store (size_t swap_slot) {
  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_ssid(swap_slot);
  if (ss == NULL) {
    lock_release(&swap_lock);
    return;
  }
  list_remove(&ss->elem);
  lock_release(&swap_lock);
}
