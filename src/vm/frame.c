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
#include "vm/swap.h"
#include "devices/swap.h"
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

struct hash frame_table;
static struct lock frame_lock; 
static struct list frame_list;
static struct list_elem *hand;

static struct frame *frame_to_evict_v (void);
static bool save_evicted_frame (struct frame *);
static struct frame *clock_frame_next (void);

unsigned frame_hash(const struct hash_elem *e, void *aux UNUSED) {
  struct frame *frame = hash_entry(e, struct frame, elem);
  return hash_bytes(&frame->kpage, sizeof frame->kpage);
}

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct frame *frame_a = hash_entry(a, struct frame, elem);
  struct frame *frame_b = hash_entry(b, struct frame, elem);
  return frame_a->kpage < frame_b->kpage;
}

/* Initialise the frame table, set all frames as free. */
void frame_table_init(void) {
  hash_init(&frame_table, frame_hash, frame_less, NULL);
  lock_init(&frame_lock);
  list_init(&frame_list);
}

/* Allocate a free frame and return its address */
void *allocate_frame(void) {
  lock_acquire (&frame_lock);

  void *frame_page = palloc_get_page (PAL_USER);
  if (frame_page == NULL) {
    struct frame *evicted = frame_to_evict_v ();

    ASSERT (evicted != NULL && evicted->t != NULL);
    ASSERT (evicted->t->pagedir != (void *)0xcccccccc);
    pagedir_clear_page (evicted->t->pagedir, evicted->user_vaddr);

    bool dirty = false
                 || pagedir_is_dirty(evicted->t->pagedir, evicted->user_vaddr)
                 || pagedir_is_dirty(evicted->t->pagedir, evicted->kpage);

    //TODO: get swap slot index, set swap and dirty for spt, free frame

    size_t swap_slot = swap_out (evicted->kpage);
    spt_set_swap (evicted->t->spt, evicted->user_vaddr, swap_slot);
    spt_set_dirty (evicted->t->spt, evicted->user_vaddr, dirty);
    free_frame(evicted->kpage);
   
    frame_page = palloc_get_page (PAL_USER);
    ASSERT (frame_page != NULL);
  }


  struct frame *frame = malloc(sizeof(struct frame));

  if (frame == NULL) {
    lock_release(&frame_lock);
    return NULL;
  }

  frame->t = thread_current();
 // frame->user_vaddr = is_user_vaddr;
  frame->kpage = frame_page;
  frame->pinned = true; // can't be evicted yet
  
  hash_insert (&frame_table, &frame->elem);
  list_push_back (&frame_list, &frame->lelem);

  lock_release (&frame_lock);
  return frame->kpage;
}

void *frame_get_page(struct spt_entry *spte) {
void *frame = palloc_get_page(PAL_USER);
  return frame;
}

void 
free_frame (void *kpage) {
  lock_acquire (&frame_lock);
  deallocate_frame (kpage, true);
  lock_release (&frame_lock);
}

void 
frame_remove_entry (void *kpage) {
  lock_acquire (&frame_lock);
  deallocate_frame (kpage, false);
  lock_release (&frame_lock);
}


/* Deallocate a frame by marking it as free */
void deallocate_frame(void *page_addr, bool free_page) {  
  //struct page *page = (struct page *)page_addr;
  /*struct hash_iterator i;
  struct frame *frame;
  hash_first(&i, &frame_table);
  while (hash_next(&i)) {
    struct hash_elem *e = hash_cur(&i);
    if (hash_entry(e, struct frame, elem)->page == page_addr) {  //not sure if == is suitable
        frame = hash_entry(e, struct frame, elem);
	break;
    }
  }*/
  //ASSERT (pg_ofs(kpage) == 0);
  struct frame tmp;
  tmp.kpage = page_addr;
  struct hash_elem *e = hash_find(&frame_table, &tmp.elem);
  ASSERT (e != NULL);

  struct frame *frame = hash_entry(e, struct frame, elem);
  hash_delete(&frame_table, &frame->elem);

  if (free_page) 
    palloc_free_page(frame->kpage);
  free(frame);
}

void *
evict_frame() {
  bool result;
  //TODO: The struct should be a wrapper for void* frame
  struct frame *frame; 
  struct thread *cur = thread_current();

  /* To ensure eviction is atomic */
  lock_acquire (&frame_lock);

  frame = frame_to_evict_v();

  if (frame == NULL)
    PANIC ("No frame to evict");

  result = save_evicted_frame(frame);
  if (!result) 
    PANIC ("Cannot save evicted frame");

  lock_release (&frame_lock);

  /* Should return v*/
  return frame->kpage;
}

/* Use hash iterator to choose the frame to evict. */
// maybe list instead?
static struct frame *
frame_to_evict_v (void) {
  struct hash_iterator i;
  hash_first (&i, &frame_table);
  struct frame *victim_frame = hash_entry (hash_next(&i), struct frame, elem);
  return victim_frame;
}

static bool
save_evicted_frame (struct frame *frame) {
  struct thread *t = frame->t;
  struct spt_entry *spte = spt_find_page (&t->spt, frame->user_vaddr);

  if (spte == NULL) {
    spte = calloc (1, sizeof (struct spt_entry));
    spte->user_vaddr = frame->user_vaddr;
    spte->type = Swap;
    //if (!insert_spt_entry (&t->spt, spte))
      //return false;
  }

  size_t swap_slot;

  if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) && (spte->type == Mmap)) {
   // write_page_back_to_file_without_lock (spte);
  } else if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) || (spte->type != File)) {
    swap_slot = swap_out (spte->user_vaddr);
    if (swap_slot == SWAP_ERROR)
      return false;

    spte->type = spte->type | Swap;
  }

  memset (frame->kpage, 0, PGSIZE);

  spte->swap_slot = swap_slot;
//  spte->writable = *(frame->user_vaddr) & PTE_W;
  spte->in_memory = false;

  pagedir_clear_page (t->pagedir, spte->user_vaddr);

  return true;
}

static void
frame_set_pinned (void *kpage, bool new_pinned) {
  lock_acquire (&frame_lock);

  struct frame tmp;
  tmp.kpage = kpage;
  struct hash_elem *e = hash_find (&frame_table, &(tmp.elem));
  if (e == NULL) 
    PANIC ("The frame does not exist");

  struct frame *frame = hash_entry (e, struct frame, elem);
  frame->pinned = new_pinned;

  lock_release (&frame_lock);
}

void
frame_unpin (void *kpage) {
  frame_set_pinned (kpage, false);
}

void
frame_pin (void *kpage) {
  frame_set_pinned (kpage, true);
}

//static struct frame* clock_frame_next (void);

struct frame*
frame_to_evict (uint32_t *pagedir) {
  size_t n = hash_size(&frame_table);
  ASSERT (n > 0);

  size_t i;
  for(i = 0; i <= n * 2; ++i)  {
    struct frame *e = clock_frame_next();
    if(e->pinned) {
      continue;
    } else if (pagedir_is_accessed(pagedir, e->user_vaddr)) {
      pagedir_set_accessed(pagedir, e->user_vaddr, false);
      continue;
    }

    return e;
  }

  PANIC ("Can't evict any frame, not enough memory");
}

static struct frame* 
clock_frame_next(void) {
  ASSERT (!list_empty(&frame_list));

  if (hand == NULL || hand == list_end(&frame_list))
    hand = list_begin (&frame_list);
  else
    hand = list_next (hand);

  struct frame *e = list_entry(hand, struct frame, lelem);
  return e;
}



