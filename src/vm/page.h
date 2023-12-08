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

void* allocate_page(void);
//void* deallocate_page(struct page *pg);

enum spte_type {
  Swap = 1,
  File = 2,
  Mmap = 4
};

struct spt_entry {
    uint8_t *user_vaddr;  /* User virtual address.*/
    bool in_memory;       /* Whether the page is currently in memory. */
    void *frame_page;    /* Pointer to the frame in memory. */
    struct hash_elem elem;
    struct list_elem lelem;
    
    // lazy loading fields
    struct file * file;    /* File associated with the page. */
    off_t ofs;            /* Offset within the file. */
    uint32_t read_bytes;  /* Number of bytes to read from the file. */
    uint32_t zero_bytes;  /* Number of zero bytes to add to the end of the page. */
    bool writable;        /* Whether the page is writable. */
    enum spte_type type;  /* Type of the spte. */

    // swap
    size_t swap_slot;     /* Swap slot index. */
  
    // shared
    bool is_shared;      /* Whether the spte share page with other */
};

/* Representation of memory-mapped file */
struct map_file {
  mapid_t mid;               // id of the file
  struct file *file;         // File associated
  void *addr;                // User virtual address 
  struct list_elem elem;
  int page_no;              // Total page count
};

struct shared_page {
//    struct hash_elem elem;
    struct list_elem elem;
    struct spt_entry *spte;
    int shared_count;
    struct list pd_list;
    uint8_t *kpage;           // Pointer to the kernel page.
//    uint32_t *pagedir;        // Pointer to the page directory.
};

void page_init(void);

struct sup_page_table *spt_create(void);

unsigned spt_hash (const struct hash_elem *e, void *aux);

bool spt_less (const struct hash_elem *a, const struct hash_elem *b, void *aux);

bool spt_insert_file (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool spt_insert_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes);

bool load_page(struct spt_entry *spte, void * kpage);

bool load_page_swap (struct spt_entry *spte, void *kpage);

struct shared_page *get_shared_page(struct spt_entry *spte);

void *share_page(void *upage, struct file *file);

void create_shared_page (struct spt_entry *spte, void *kpage);

struct spt_entry* spt_find_page(struct hash *spt, void *vaddr);

void free_spt(struct hash_elem *e, void *aux);

void init_page_sharing(void) ;

unsigned shared_page_hash(const struct hash_elem *e, void *aux);

bool shared_page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux ); 

struct shared_page *search_shared_page(void *kpage);

void delete_shared_page(struct shared_page *sp, void * user_vaddr);

#endif /* vm/page.h */



