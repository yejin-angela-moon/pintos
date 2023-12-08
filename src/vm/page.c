#include "vm/page.h"
#include <stddef.h>
#include <stdbool.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "threads/thread.h"
#include <string.h>
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "threads/synch.h"

// Define a hash table to store shared pages.
// Define a lock for page sharing.

//bool share_page(struct spt_entry *spte);
void unshare_page(struct spt_entry *spte);


struct lock file_lock;
struct list shared_pages;
struct lock page_sharing_lock;

void page_init() {
  lock_init(&file_lock);
}

unsigned spt_hash(const struct hash_elem *elem, void *aux UNUSED) {
    struct spt_entry *page = hash_entry(elem, struct spt_entry, elem);
    return hash_int((int)page->user_vaddr);
}

// Comparison function for page entries
bool spt_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct spt_entry *spte_a = hash_entry(a, struct spt_entry, elem);
    struct spt_entry *spte_b = hash_entry(b, struct spt_entry, elem);
    return spte_a->user_vaddr < spte_b->user_vaddr;  
}

// Create a new spte with type File and the given infomation
bool spt_insert_file (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  struct spt_entry *spte = malloc (sizeof(struct spt_entry));
  struct hash_elem *e;
  struct thread *cur = thread_current ();
  if (spte == NULL) {
    return false;
  }
  spte->user_vaddr = upage;
  spte->file = file;
  spte->ofs = ofs;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;
  spte->in_memory = false;
  spte->type = File;
 
  lock_acquire(&cur->spt_lock); 
  e = hash_insert (&cur->spt, &spte->elem);
  lock_release(&cur->spt_lock);
  
  if (e != NULL) {
	  struct spt_entry *result = hash_entry(e, struct spt_entry, elem);
	  result->file = file;
	  result->read_bytes = read_bytes;
	  result->zero_bytes = zero_bytes;
	  result->writable = writable; 
  }
  
  return true;
}

// Create a new spte with type Mmap and the given infomation
bool spt_insert_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes) {
  struct spt_entry *spte = malloc (sizeof(struct spt_entry));
  struct hash_elem *e;
  struct thread *cur = thread_current ();
  if (spte == NULL) {
    return false;
  }
  spte->user_vaddr = upage;
  spte->file = file;
  spte->ofs = ofs;
  spte->read_bytes = read_bytes;
  spte->in_memory = false;
  spte->type = Mmap;
  
  lock_acquire(&cur->spt_lock);
  e = hash_insert (&cur->spt, &spte->elem);
  if (e != NULL) {     
          struct spt_entry *result = hash_entry(e, struct spt_entry, elem);
          result->read_bytes = read_bytes;
  }
  lock_release(&cur->spt_lock);

  return true;
}

// Load the page with the allocate frame when needed
bool load_page(struct spt_entry *spte, void * kpage) {  
  struct thread *cur = thread_current ();
  lock_acquire(&file_lock);
  file_seek(spte->file, spte->ofs);
  bool writable = spte->type == File ? spte->writable : true;
  if (file_read (spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes) {
    deallocate_frame (kpage);
    return false;
  }
  lock_release(&file_lock);

  uint32_t size = spte->type == File ? spte->zero_bytes : PGSIZE - spte->read_bytes;
  memset (kpage + spte->read_bytes, 0, size);  
 
  if (pagedir_get_page(cur->pagedir, spte->user_vaddr) == NULL) {
    if (!pagedir_set_page (cur->pagedir, spte->user_vaddr, kpage, writable)) {
      deallocate_frame (kpage);
      return false;
    }
  } 

  spte->in_memory = true;
  spte->frame_page = kpage;
  if (spte->type & Swap) {
    spte->type = Mmap;
  }

  return true;
}

// Load the page from swap
bool load_page_swap (struct spt_entry *spte, void *kpage) {
  if (pagedir_get_page(thread_current ()->pagedir, spte->user_vaddr) == NULL) {
    if (!pagedir_set_page (thread_current ()->pagedir, spte->user_vaddr, kpage, spte->writable)){
      deallocate_frame (kpage);
      return false;
    }
  }

  swap_in_memory (spte->swap_slot, spte->user_vaddr);
 
  if (spte->type == (File | Swap))  {
    spte->type = File; 
    spte->in_memory = true;
  }

  return true;
}


static bool is_equal_spt(struct spt_entry * this, struct spt_entry * other) {
  //bool equal_mmap = (this->user_vaddr == other->user_vaddr) && (this->ofs == other->ofs) && (this->read_bytes == other->read_bytes) && (this->file == other->file);
//  if (this->type == Mmap && this->type == other->type) {
//	  printf("mmap equal = %d\n", equal_mmap);
  return this->user_vaddr == other->user_vaddr; 
//  return equal_mmap;
  //}
//printf("mmap equal = %d\n", equal_mmap);
  //return equal_mmap && (this->zero_bytes == other->zero_bytes) && (this->writable == other->writable) && (this->type == other->type);
}

struct shared_page *get_shared_page(struct spt_entry *spte) {
  struct shared_page *sp = NULL;
  struct spt_entry *tmp;
  lock_acquire(&page_sharing_lock);
  for (struct list_elem *e = list_begin(&shared_pages); e != list_end(&shared_pages); e = list_next(e)) {
    printf("searching the elem in shared pages with size %d\n", list_size(&shared_pages));
    tmp = list_entry(e, struct shared_page, elem)->spte;
    if (is_equal_spt(spte, tmp)) {
      sp = list_entry(e, struct shared_page, elem);
      break;
    }
  }
  //sp->shared_count++;//lock_release(&page_sharing_lock);
  lock_release(&page_sharing_lock);
  return sp;
}

void create_shared_page (struct spt_entry *spte, void *kpage) {
  struct shared_page *new_shared_page = malloc(sizeof (struct shared_page));
  if (new_shared_page == NULL) {
    return;
  }
  new_shared_page->spte = spte;
  new_shared_page->kpage = kpage;

//  new_shared_page->pagedir = thread_current()->pagedir; 
  new_shared_page->shared_count = 1;
  list_init(&new_shared_page->pd_list);
  list_push_back(&new_shared_page->pd_list, &thread_current()->pd_elem);

  lock_acquire(&page_sharing_lock);
printf("add omre pages\n");
  list_push_back(&shared_pages, &new_shared_page->elem);
  
  lock_release(&page_sharing_lock);
 
}

// Find the spte by its user vaddr
struct spt_entry* spt_find_page(struct hash *spt, void *vaddr) {
  struct spt_entry tmp;
  tmp.user_vaddr = vaddr;
  struct hash_elem *e = hash_find(spt, &tmp.elem);
  return e != NULL ? hash_entry(e, struct spt_entry, elem) : NULL;
}

// Remove from the swap list and free the spte
void free_spt(struct hash_elem *e, void *aux UNUSED) {
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
  if (spte->type & Swap) {
    remove_swap_store (spte->swap_slot);
  }
  free(spte);
}

// Initialize the lock and hash table in your system initialization code.
void init_page_sharing(void) {
    lock_init(&page_sharing_lock);
    list_init(&shared_pages);
   // hash_init(&shared_pages, shared_page_hash, shared_page_less, NULL);
}

// Hash function for the shared pages hash table.
/*unsigned shared_page_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct shared_page *spage = hash_entry(e, struct shared_page, elem);
    return hash_int((int)spage->spte->user_vaddr);
}

// Comparison function for shared pages.
bool shared_page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct shared_page *sa = hash_entry(a, struct shared_page, elem);
    struct shared_page *sb = hash_entry(b, struct shared_page, elem);
    return sa->spte->user_vaddr < sb->spte->user_vaddr;
}*/

struct shared_page *search_shared_page(void *kpage) {
  struct shared_page *sp;
  for (struct list_elem *e = list_begin(&shared_pages); e != list_end(&shared_pages); e = list_next(e)) {
    sp = list_entry(e, struct shared_page, elem);
    if (sp->kpage == kpage) {
      return sp;
    }
  } 
  return NULL;
} 
 
static struct shared_page *search_shared_page_by_up(void *upage) {
  struct shared_page *sp;
  for (struct list_elem *e = list_begin(&shared_pages); e != list_end(&shared_pages); e = list_next(e)) {
    sp = list_entry(e, struct shared_page, elem);
    if (sp->spte->user_vaddr == upage) {
      return sp;
    }
  }
  return NULL;
}
  
// Function to share a page.
void * share_page(void *upage, struct file *file) {
    lock_acquire(&page_sharing_lock);
    struct thread *cur = thread_current();
    //struct shared_page spage;
    //spage.spte = spte;
    //spage.shared_count = 1;

    //struct hash_elem *existing = hash_insert(&shared_pages, &spage.elem);
    //spte->is_shared = true;
    struct shared_page *sp = search_shared_page_by_up(upage);
//Assert(sp->spte->file == spte->file);
    if (sp == NULL) {
	    printf("no file find\n");
        //struct shared_page *existing_spage = hash_entry(existing, struct shared_page, elem);
        //existing_spage->shared_count++;
        lock_release(&page_sharing_lock);
        return 0;
    }   
  //  spte->frame_page = sp->kpage;
    //spte->in_memory = true;
    list_push_back(&sp->pd_list, &cur->pd_elem);
    printf("set page with user vaddr %p and file %p and page %p\n", upage, file, sp->kpage);
    pagedir_set_page(cur->pagedir, upage, sp->kpage, false);
    lock_release(&page_sharing_lock);
    return sp->kpage;
}
/*
void delete_shared_page(struct shared_page *sp, void * user_vaddr) {
  struct thread *thr;
  struct list_elem *te = list_begin(&sp->pd_list);
  struct list_elem *nte;
  printf("remove stuff when save\n");
  while (te != list_end(&sp->pd_list)) {
        //printf("remove from the pd list and clear page for tid %d\n", thr->tid);
    nte = list_next(te);
    thr = list_entry(te, struct thread, pd_elem);
     printf("remove from the pd list and clear page for tid %d\n", thr->tid);
    pagedir_clear_page (thr->pagedir, user_vaddr);
    list_remove(te);
    te = nte;
  } 
}*/
// Function to unshare a page.
/*void unshare_page(struct spt_entry *spte) {
    lock_acquire(&page_sharing_lock);

    struct shared_page spage;
    spage.spte = spte;

    struct hash_elem *existing = hash_find(&shared_pages, &spage.elem);

    if (existing != NULL) {
        struct shared_page *existing_spage = hash_entry(existing, struct shared_page, elem);

        if (existing_spage->shared_count > 1) {
            existing_spage->shared_count--;
        } else {
            // Remove the shared page entry when shared_count becomes 0.
            hash_delete(&shared_pages, existing);
        }
    }

    lock_release(&page_sharing_lock);
}
*/

