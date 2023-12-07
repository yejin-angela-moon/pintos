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
#include "threads/thread.h"
#include <string.h>
#include "threads/vaddr.h"
#include "userprog/exception.h"
#include "threads/synch.h"
// could move to a header file

// Define a hash table to store shared pages.
// Define a lock for page sharing.

bool share_page(struct spt_entry *spte);
void unshare_page(struct spt_entry *spte);

static int count = 0;
struct list shared_pages;
struct lock page_sharing_lock;

void spte_init(struct spt_entry *spte) {
  /*
  pg->frame = NULL;
  pg->present = false;
  pg->dirty = false;
  */ 
  //spte->frame = NULL;
  //spte->is_dirty = false;
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

/*
struct sup_page_table *spt_create (void) {
  struct sup_page_table *spt = malloc(sizeof(struct sup_page_table));
  hash_init (&spt->table, spt_hash, spt_less, NULL);
  return spt;
}
*/


void spt_init (struct sup_page_table *spt) {
  hash_init(&spt->table, spt_hash, spt_less, NULL);
}

bool
spt_insert_file (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{ //return true;
	//printf("indide the spt insrt function\n");
  struct spt_entry *spte = malloc (sizeof(struct spt_entry));
  //printf("calloc a new spte\n");
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
 // spte->count = count;
  //count++;
  //printf("set up the spte\n");
//  struct hash spt = cur->spt;
 // printf("get the spt of the cur thread\n");
//  hash_init (&spt, spt_hash, spt_less, NULL);
//file_seek (spte->file, spte->ofs);
//void * kp = palloc_get_page(PAL_USER);
//printf("file read when insert %d\n ", file_read (spte->file, kp, spte->read_bytes));
//  printf("when insertedcount of spte is %d, ofs %d, read %d, file %p, file length %d, writeable %d\n", spte->count, spte->ofs, spte->read_bytes, spte->file, file_length(spte->file), spte->writable);
  e = hash_insert (&cur->spt, &spte->elem);
  //struct spt_entry *result = hash_entry(e, struct spt_entry, elem);
  if (e != NULL) {
	  struct spt_entry *result = hash_entry(e, struct spt_entry, elem);
//	  printf("cannot insert\n");
	  //free (spte);
	  //result->user_vaddr = upage;
	 // result->file = file;
	  //result->ofs = ofs;
	  result->read_bytes = read_bytes;
	  result->zero_bytes = zero_bytes;
	  //result->in_memory = false;
	  result->writable = writable;  // return false;
  }
  //load_page_to_frame(spte);
//printf("inserted a new addr %d to the hash\n", (uint32_t) upage);
  return true;
}

bool spt_insert_mmap(struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes) {
   struct spt_entry *spte = malloc (sizeof(struct spt_entry));
  //printf("calloc a new spte\n");
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
  
//printf("file read when insert %d\n ", file_read (spte->file, kp, spte->read_bytes));
  //printf("when insertedcount of spte is %d, ofs %d, read %d, file %p, file length %d\n", spte->count, spte->ofs, spte->read_bytes, spte->file, file_length(spte->file));
  e = hash_insert (&cur->spt, &spte->elem);
  if (e != NULL) {     
          struct spt_entry *result = hash_entry(e, struct spt_entry, elem);
          result->read_bytes = read_bytes;
  }
//  void * kpage = allocate_frame();
  //printf("try load page rn %d\n", load_page(spte, kpage));
  return true;
}

bool load_page(struct spt_entry *spte, void * kpage) {
	struct thread *cur = thread_current ();

  file_seek(spte->file, spte->ofs);
  bool writable = spte->type == File ? spte->writable : true;
  if (file_read (spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes)
    {
//	        printf("kpage pointer: %p\n", (void *) kpage);
//	    printf("the read byte is not equal with %d and %d\n", file_read (spte->file, kpage, (off_t) (int) spte->read_bytes), (int) spte->read_bytes);
      frame_remove_entry (kpage);
      return false;
    }
  //printf("file read\n");
  uint32_t size = spte->type == File ? spte->zero_bytes : PGSIZE - spte->read_bytes;
  memset (kpage + spte->read_bytes, 0, size);  
  
  if (pagedir_get_page(cur->pagedir, spte->user_vaddr) == NULL) {
//	  printf("not mapped\n");
  if (!pagedir_set_page (cur->pagedir, spte->user_vaddr, kpage, writable)) {
      frame_remove_entry (kpage);
      return false;
    }
  } else {
  //  printf("already mapped\n");
  }
  //printf("end of the load page to frame function\n");
  spte->in_memory = true;
  spte->frame_page = kpage;
  return true;

}
/*
bool load_page_mmap(struct spt_entry *spte, void * kpage) {
        struct thread *cur = thread_current ();

  file_seek(spte->file, spte->ofs);

  if (file_read (spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes)
    {
//              printf("kpage pointer: %p\n", (void *) kpage);
//          printf("the read byte is not equal with %d and %d\n", file_read (spte->file, kpage, (off_t) (int) spte->read_bytes), (int) spte->read_bytes);
      deallocate_frame (kpage);
      return false;
    }
  //printf("file read\n");
  memset (kpage + spte->read_bytes, 0, spte->zero_bytes);

  if (pagedir_get_page(cur->pagedir, spte->user_vaddr) == NULL) {
//        printf("not mapped\n");
  if (!pagedir_set_page (cur->pagedir, spte->user_vaddr, kpage, true)) {
      deallocate_frame (kpage);
      return false;
    }
  } else {
  //  printf("already mapped\n");
  }
  //printf("end of the load page to frame function\n");
  spte->in_memory = true;
  return true;

}*/

//bool spt_insert_mmap(struct file *file, off_t ofs, void *addr, read_bytes)

bool is_equal_spt(struct spt_entry * this, struct spt_entry * other) {
  bool equal_mmap = (this->user_vaddr == other->user_vaddr) && (this->ofs == other->ofs) && (this->read_bytes == other->read_bytes) && (this->file == other->file);
  if (this->type == Mmap && this->type == other->type) {
    return equal_mmap;
  }
  return equal_mmap && (this->zero_bytes == other->zero_bytes) && (this->writable == other->writable) && (this->type == other->type);
}

struct shared_page *get_shared_page(struct spt_entry *spte) {
  struct shared_page *sp = NULL;
  struct spt_entry *tmp;
  lock_acquire(&page_sharing_lock);
  for (struct list_elem *e = list_begin(&shared_pages); e != list_end(&shared_pages); e = list_next(e)) {
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
  new_shared_page->pagedir = thread_current()->pagedir; 
  new_shared_page->shared_count = 1;

  lock_acquire(&page_sharing_lock);
  list_push_back(&shared_pages, &new_shared_page->elem);
  lock_release(&page_sharing_lock);

}

struct spt_entry* spt_find_page(struct hash *spt, void *vaddr) {
  struct spt_entry tmp;
  tmp.user_vaddr = vaddr;
  struct hash_elem *e = hash_find(spt, &tmp.elem);
  return e != NULL ? hash_entry(e, struct spt_entry, elem) : NULL;
}

void
free_spt(struct hash_elem *e, void *aux UNUSED) {
  struct spt_entry *spte = hash_entry(e, struct spt_entry, elem);
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
/*
// Function to share a page.
bool share_page(struct spt_entry *spte) {
    lock_acquire(&page_sharing_lock);

    struct shared_page spage;
    spage.spte = spte;
    spage.shared_count = 1;

    struct hash_elem *existing = hash_insert(&shared_pages, &spage.elem);
    spte->is_shared = true;

    if (existing != NULL) {
        struct shared_page *existing_spage = hash_entry(existing, struct shared_page, elem);
        existing_spage->shared_count++;
        lock_release(&page_sharing_lock);
        return false;
    }
    lock_release(&page_sharing_lock);
    return true;
}
*/
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

/* Allocate a new virtual page and return its address */
/*
void *allocate_page(void) {
 
}*/

/* Deallocate the virtual page and release associated resources */
/*
void deallocate_page(struct page *pg) {
}*/

/*
 * void handle_page_fault(struct page *pg) - although it would be in exception.c / 
 * or it can be called there?; bring the page into memory
 *
 * void mark_page_dirty(struct page *pg) - set dirty bit true
 *
 * void write_dirty_page(struct page *pg) - write the contents of the dirty page back to disk or swap
 *
 * void *get_frame(struct page *pg)
 *
 * bool is_page_present(struct page *pg)
 * */


