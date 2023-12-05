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
  hash_init(&frame_table, frame_hash, frame_less, NULL);
}

/* Allocate a free frame and return its address */
void *allocate_frame(void) {
  struct frame frame;// = malloc(sizeof(struct frame));
//  if (frame != NULL) {
    frame.page = palloc_get_page(PAL_USER);
    if (frame.page != NULL) {
      frame.is_free = false;
      hash_insert(&frame_table, &frame.elem);
      return frame.page;
    } else {
    //  free(frame);
    }
  //}
  return NULL;
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

