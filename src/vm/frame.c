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
  struct frame *frame = (struct frame *)malloc(sizeof(frame));
  if (frame != NULL) {
    frame->page = palloc_get_page(PAL_USER);
    if (frame->page != NULL) {
      frame->is_free = false;
      hash_insert(&frame_table, &frame->elem);
      return frame;
    } else {
      free(frame);
    }
  }
  return NULL;
}

/* Deallocate a frame by marking it as free */
void deallocate_frame(void *frame_addr) {
  struct frame *frame = (struct frame *)frame_addr;
  hash_delete(&frame_table, &frame->elem);
  palloc_free_page(frame->page);
  free(frame);
}

