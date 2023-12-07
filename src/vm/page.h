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
#include <vm/frame.h>
#include <userprog/pagedir.h>
#include <filesys/file.h>


typedef int mapid_t;
/*
struct page {
  void *frame;
  bool present;
  bool dirty;
};
*/

void* allocate_page(void);
//void* deallocate_page(struct page *pg);

enum spte_type {
    File, 
    Mmap
};

struct spt_entry {
    uint8_t * user_vaddr;  /* User virtual address.*/
    //uint32_t frame_addr; 
    bool in_memory;       /* Whether the page is currently in memory. */
    struct hash_elem elem;
    struct list_elem lelem;
    // other potential fields: fd, file_offset, is_read_only, is_dirty, timestamp, swap slot, is_swapped_out
    // lazy loading fields
    off_t ofs;            /* Offset within the file. */
    uint32_t read_bytes;  /* Number of bytes to read from the file. */
    uint32_t zero_bytes;  /* Number of zero bytes to add to the end of the page. */
    bool writable;        /* Whether the page is writable. */
    struct file * file;    /* File associated with the page. */
    bool is_dirty;
    bool is_valid;
    int count;
    // swap
    size_t swap_slot;     /* Swap slot index. */
    struct frame *frame;  /* Pointer to the frame in memory. */
    enum spte_type type;
    // shared
    bool is_shared;
};

// page table itself
struct sup_page_table {
  struct hash table;
  struct lock spt_lock;
  // other potential fields: owner_thread, spt_lock

};

/* Representation of memory-mapped file */
struct map_file {
  mapid_t mid;
  struct file *file;
  void *addr;  // should this be void* ?
  size_t length;
  struct list_elem elem;
  struct list pages;
};

struct shared_page {
    struct hash_elem elem;
    struct spt_entry *spte;
    int shared_count;
    uint8_t *kpage;           // Pointer to the kernel page.
    uint32_t *pagedir;        // Pointer to the page directory.
};

struct lock page_sharing_lock;
struct hash shared_pages;

void spt_init (struct sup_page_table *spt);

struct sup_page_table *spt_create(void);

unsigned spt_hash (const struct hash_elem *e, void *aux);

bool spt_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

bool
spt_insert_file (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool spt_insert_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes);

bool load_page(struct spt_entry *spte, void * kpage);
//bool load_page_mmap(struct spt_entry *spte, void * kpage);

struct spt_entry* spt_find_page(struct hash *spt, void *vaddr);

void free_spt(struct hash_elem *e, void *aux);

void init_page_sharing(void) ;

unsigned shared_page_hash(const struct hash_elem *e, void *aux);
bool shared_page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux ); 


#endif /* vm/page.h */


