#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include "filesys/off_t.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

/*
struct page {
  void *frame;
  bool present;
  bool dirty;
};
*/

void* allocate_page(void);
//void* deallocate_page(struct page *pg);

struct spt_entry {
    uint32_t user_vaddr;  /* User virtual address.*/
    //uint32_t frame_addr; 
    bool in_memory;       /* Whether the page is currently in memory. */
    struct hash_elem elem;
    // other potential fields: fd, file_offset, is_read_only, is_dirty, timestamp, swap slot, is_swapped_out
    // lazy loading fields
    off_t ofs;            /* Offset within the file. */
    uint32_t read_bytes;  /* Number of bytes to read from the file. */
    uint32_t zero_bytes;  /* Number of zero bytes to add to the end of the page. */
    bool writable;        /* Whether the page is writable. */
    struct file *file;    /* File associated with the page. */
    bool is_dirty;
    bool is_valid;
    // swap
    size_t swap_slot;     /* Swap slot index. */
    struct frame *frame;  /* Pointer to the frame in memory. */

};

// page table itself
struct sup_page_table {
  struct hash table;
  struct lock spt_lock;
  // other potential fields: owner_thread, spt_lock

};

void spt_init (struct sup_page_table *spt);

struct sup_page_table *spt_create(void);

unsigned spt_hash (const struct hash_elem *e, void *aux);

bool spt_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

bool
spt_insert_file (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable);

struct spt_entry* spt_find_page(struct sup_page_table *spt, void *vaddr);

void free_spt(struct hash_elem *e, void *aux);


#endif /* vm/page.h */


