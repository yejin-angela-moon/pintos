#include "vm/frame.h"
#include "lib/kernel/hash.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "threads/palloc.h"
#include "userprog/pagedir.h"
#include "userprog/syscall.h"
#include <stdbool.h>
#include <stddef.h>

struct hash frame_table;
struct lock frame_lock;

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED) {
  struct frame *frame = hash_entry(e, struct frame, elem);
  return hash_bytes(&frame->page, sizeof frame->page);
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct frame *frame_a = hash_entry(a, struct frame, elem);
  struct frame *frame_b = hash_entry(b, struct frame, elem);
  return frame_a->page < frame_b->page;
}

/* Initialise the frame table, set all frames as free. */
void frame_table_init(void) {
	printf("init frame table\n");
  hash_init(&frame_table, frame_hash, frame_less, NULL);
  lock_init(&frame_lock);
}


static bool set_frame(void *page) {
  struct frame *frame;
  frame = malloc(sizeof(struct frame));

  printf("malloc the frame\n");
  //struct frame *frame;
  
  if (frame == NULL) {
	  return false;
  }
        //page = palloc_get_page(PAL_USER| PAL_ZERO);
        //if (frame->page != NULL) {
            frame->page = page;
            frame->is_free = false;
	    lock_acquire(&frame_lock);
            hash_insert(&frame_table, &frame->elem);
	    lock_release(&frame_lock);
            return true;
        //}
    
}


/* Allocate a free frame and return its address */
void *allocate_frame(enum palloc_flags pal) {
  /*truct frame *frame = malloc(sizeof(struct frame));
  if (frame != NULL) {
    frame->page = palloc_get_page(PAL_USER);
    if (frame->page != NULL) {
      frame->is_free = false;
      hash_insert(&frame_table, &frame->elem);
      return frame->page;
    } else {
      free(frame);
    }
  }*/

	printf("start of allocate frame\n");
//struct spt_entry *spte = malloc (sizeof(struct spt_entry));
  struct frame *frame = malloc(sizeof(struct frame));
 
  printf("malloc the frame\n");
  //struct frame *frame;
  
    if (frame != NULL) {
      void *page = NULL;
        if (pal & PAL_USER) {
      if (pal & PAL_ZERO) {
        page = palloc_get_page (PAL_USER | PAL_ZERO);
      }else {
              printf("page ffrom user pool\n");
        page = palloc_get_page (PAL_USER);
      }
    }
       if (page != NULL) {
        if (frame->page != NULL) {
	    frame->page = page;
            frame->is_free = false;
            hash_insert(&frame_table, &frame->elem);
            return page;
        }
       }
    }
    free(frame);
  return NULL;
	/*void *page = NULL;
	if (pal & PAL_USER)
    {
      if (pal & PAL_ZERO) {
        page = palloc_get_page (PAL_USER | PAL_ZERO);
      }else {
	      printf("page ffrom user pool\n");
        page = palloc_get_page (PAL_USER);
      }
    }
       if (page != NULL) {
	       set_frame(page);
       }
       
 printf("the page is set to %hhn\n", (uint8_t *) page);       
       return page;*/
}

void *frame_get_page(struct spt_entry *spte) {
void *frame = palloc_get_page(PAL_USER);
  return frame;
} 


/* Deallocate a frame by marking it as free */
void deallocate_frame(void *page_addr) {  
  struct page *page = (struct page *)page_addr;
  struct hash_iterator i;
  struct frame *frame;
  hash_first(&i, &frame_table);
  while (hash_next(&i)) {
    struct hash_elem *e = hash_cur(&i);
    if (hash_entry(e, struct frame, elem)->page == page) {  //not sure if == is suitable
        frame = hash_entry(e, struct frame, elem);
	break;
    }
  }
  hash_delete(&frame_table, &frame->elem);
  palloc_free_page(frame->page);
  free(frame);
}

