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
static struct bitmap *swap_map; // Bitmap of swap slot availabilities

struct lock swap_lock;

struct list swap_list;
static size_t count;

void swap_vm_init(void) {
  list_init(&swap_list);
  lock_init(&swap_lock);  
  swap_block = block_get_role(BLOCK_SWAP);
  count = 1;
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

static struct swap_store *get_swap_store_by_uv(uint8_t *user_vaddr) {
  struct swap_store *ss = NULL;
  struct swap_store *get;
  for (struct list_elem *e = list_begin(&swap_list); e != list_end(&swap_list); e = list_next(e)) {
    get = list_entry(e, struct swap_store, elem);
    if (get->user_vaddr == user_vaddr) {
//	    printf("find!!\n");
      ss = get;
      break;
    }
  }
  
  return ss;
}


size_t swap_out_memory(void *user_vaddr) {
  if (swap_block == NULL) {
    return SWAP_ERROR;
  }
//return -1;
lock_acquire(&swap_lock);
struct swap_store *ss = get_swap_store_by_uv(user_vaddr);
//if (ss != NULL) {
  //lock_release(&swap_lock);
//  return ss->ssid;
//} else
if (ss == NULL) {
if (list_size(&swap_list) < (block_size(swap_block) / SECTORS_PER_PAGE)) {
    ss = malloc(sizeof(struct swap_store));
    if (ss == NULL) {
      return SWAP_ERROR;
    }
    ss->ssid = count;//list_size(&swap_list);
    ss->user_vaddr = user_vaddr;
    count++;
    //lock_acquire(&swap_lock);
    list_push_back(&swap_list, &ss->elem);
} else {
  lock_release(&swap_lock);
    return SWAP_ERROR;
}
}
    //lock_release(&swap_lock);
   for (int i = 0; i < SECTORS_PER_PAGE; i++) {
      block_write (swap_block, ss->ssid * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
   }
    //if (ss->ssid == 0) { 
//	   printf("\nthe user vaddr for %d is %p\n", ss->ssid, user_vaddr);
  //  printf("the size of swap list is %d\n", ss->ssid);
    lock_release(&swap_lock);
    return ss->ssid;
  //} else {
   //lock_release(&swap_lock); 
 //   return SWAP_ERROR;
   
}

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

void swap_in_memory(size_t swap_slot, void *user_vaddr) {
//	printf("there are %d elem in swap list\n", list_size(&swap_list));
  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_ssid(swap_slot);
  if (ss == NULL) {
    lock_release(&swap_lock);
    return;
  }
//printf("remove the ele fom swap list\n");
  //list_remove(&ss->elem);

  for (int i = 0; i < SECTORS_PER_PAGE; i++) {
    block_read (swap_block, swap_slot * SECTORS_PER_PAGE + i, (uint8_t *) user_vaddr + i * BLOCK_SECTOR_SIZE);
  }

  lock_release(&swap_lock);
}


void remove_swap_store (size_t swap_slot) {
  lock_acquire(&swap_lock);
  struct swap_store *ss = get_swap_store_by_ssid(swap_slot);
  if (ss == NULL) {
    lock_release(&swap_lock);
    return;
  }
//  printf("remove the ele fom swap list\n");
  list_remove(&ss->elem);
  lock_release(&swap_lock);
}

