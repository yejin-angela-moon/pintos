#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../threads/vaddr.h"
#include "../userprog/pagedir.h"
#include <stdio.h>
#include "vm/page.h"
#include "vm/frame.h"
#include "userprog/syscall.h"
#include "../threads/synch.h"
#include "lib/kernel/hash.h"
#include "../threads/palloc.h"
#include "../threads/malloc.h"

static struct lock frame_lock;

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED) {
    struct frame_entry *frame = hash_entry(e, struct frame_entry, elem);
    return hash_bytes(&frame->page, sizeof frame->page); 
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
    struct frame_entry *frame_a = hash_entry(a, struct frame_entry, elem);
    struct frame_entry *frame_b = hash_entry(b, struct frame_entry, elem);
    return frame_a->page < frame_b->page; 
}

void 
frame_table_init(void) {
  lock_init(&frame_lock);
  hash_init(&frame_map, frame_hash, frame_less, NULL);

}

void
*allocate_frame(enum palloc_flags flags) {
  //lock_acquire(&frame_lock);
  
  void *frame_page = palloc_get_page(PAL_USER | flags);
  if (frame_page == NULL) {
    exit(-1);
    //TODO: also check if evicting possible
  }
  
    struct frame_entry *frame = malloc(sizeof(struct frame_entry));
    if (frame == NULL) {
      //lock_release(&frame_lock);
      //exit(-1);
      return NULL;
    }

    frame->page = frame_page;
    frame->in_use = true;
    frame->tid = thread_current()->tid;

    lock_acquire(&frame_lock);
    //lock_acquire(&ft->table_lock);
    hash_insert(&ft->table, &frame->elem);
    //lock_release(&ft->table_lock);
    lock_release(&frame_lock);

    return frame;
}



/* Free frame */
void 
frame_free_or_remove(void *page, bool free_page) {
  struct frame_entry frame_tmp;
  frame_tmp.page = page;

  struct hash_elem *h = hash_find(&frame_map, &(frame_tmp.hash_elem));
  if (h == NULL) {
    exit(-1);
  }

  struct frame_entry *f = hash_entry(h, struct frame_entry, elem);

  hash_delete(&frame_map, &f->elem);
  if (free_page) palloc_free_page(page);
  free(f);
}

/* Remove frame entry */
void
remove_frame_entry (void *page) {
  lock_acquire(&frame_lock);
  frame_free_or_remove(page, false);
  lock_release(&frame_lock);
}

/* Free or remove frame entry */
void 
free_frame(void *page) {
  lock_acquire(&frame_lock);
  frame_free_or_remove(page, true);
  lock_release(&frame_lock);
}

