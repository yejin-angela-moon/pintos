#include "vm/page.h"
#include <stddef.h>
#include <stdbool.h>

#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "vm/page.h"
#include "threads/thread.h"
#include <string.h>
#include "threads/vaddr.h"
#include "userprog/exception.h"
// could move to a header file


void spte_init(struct spt_entry *spte) {
  /*
  pg->frame = NULL;
  pg->present = false;
  pg->dirty = false;
  */
  spte->frame = NULL;
  spte->is_dirty = false;
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
  struct hash_elem *result;
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
  //printf("set up the spte\n");
//  struct hash spt = cur->spt;
 // printf("get the spt of the cur thread\n");
//  hash_init (&spt, spt_hash, spt_less, NULL);
  result = hash_insert (&cur->spt, &spte->elem);
  if (result != NULL) {
	  printf("cannot insert\n");
	  free (spte);
  // return false;
  }
printf("inserted a new addr %d to the hash\n", (uint32_t) upage);
  return true;
}


bool load_page_to_frame(struct spt_entry *spte, void * kpage) {
	struct thread *cur = thread_current ();

 file_seek (spte->file, spte->ofs);
printf("file seek\n");
//  uint8_t *kpage = allocate_frame ();

  printf("allocated frame\n");
  if (kpage == NULL) {
    return false;
  }
  printf("allocated frame not null\n");
  
  if (file_read (spte->file, kpage, spte->read_bytes) != (int) spte->read_bytes)
    {
	    printf("the read byte is not equal\n");
      deallocate_frame (kpage);
      return false;
    }
  printf("file read\n");
  memset (kpage + spte->read_bytes, 0, spte->zero_bytes);

  if (!pagedir_set_page (cur->pagedir, (void *) spte->user_vaddr, kpage, spte->writable))
    {
      deallocate_frame (kpage);
      return false;
    }
printf("end of the load page to frame function\n");
  //spte->in_memory = true;
  return true;

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

