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
#include <string.h>
#include "threads/pte.h"
#include "vm/swap.h"

struct hash frame_table;
struct lock frame_lock; 
struct list frame_list;
struct list_elem *hand;
//int counting;

//static struct frame *frame_to_evict_v (void);
static bool save_evicted_frame (struct frame *);
struct frame* clock_frame_next(struct list_elem **hand);
struct frame* frame_to_evict (uint32_t *pagedir, struct list_elem **hand);


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
  hand = NULL;
  //counting = 0;
}

/* Allocate a free frame and return its address */
void *allocate_frame(void) {
  //lock_acquire (&frame_lock);

  void *frame_page = palloc_get_page (PAL_USER);
 // lock_acquire (&frame_lock);
  remove_frame_for_thread();
  //lock_release (&frame_lock);
  if (frame_page == NULL) {
  //printf("no page at all time to evict\n");
    struct frame *evicted = frame_to_evict (thread_current()->pagedir, &hand);
    //printf("run frame to evict\n");
    frame_page = evicted->kpage;
    
    ASSERT (evicted != NULL && get_thread_by_tid(evicted->tid) != NULL);
   // if (evicted->t->pagedir != (void *)0xcccccccc) {
    //  printf("the tid with weird pd is %d\n", evicted->t->tid);
    //} 
    //ASSERT (evicted->t->pagedir != (void *)0xcccccccc);
  //  if (pagedir_is_dirty(evicted->t->pagedir, evicted->user_vaddr)) {
  //    printf("that evict frame is dirty\n");
      save_evicted_frame(evicted);
    //}
    struct thread *t = get_thread_by_tid(evicted->tid);
    struct spt_entry *evi_spte = spt_find_page(&t->spt, evicted->user_vaddr);
    evi_spte->in_memory = false;
    evi_spte->frame_page = NULL;
   
    evicted->kpage = NULL;
    evicted->user_vaddr = NULL;
    evicted->pte = NULL;
    evicted->tid = thread_current()->tid;
    lock_acquire (&frame_lock);
    list_remove(&evicted->lelem);
    hash_delete(&frame_table, &evicted->elem);
    lock_release (&frame_lock); 
 
    ASSERT (frame_page != NULL);
 }

  struct frame *frame = malloc(sizeof(struct frame));
  if (frame == NULL) {
    lock_release(&frame_lock);
    return NULL;
  }

  frame->tid = thread_current()->tid;
  frame->kpage = frame_page;
  frame->pinned = true; // can't be evicted yet

  lock_acquire (&frame_lock);
  hash_insert (&frame_table, &frame->elem);
  list_push_back (&frame_list, &frame->lelem);
  lock_release (&frame_lock);

  return frame->kpage;
}

/* Remove the frames that is set for a terminated thread */
void remove_frame_for_thread() {
  struct frame *frame;
  struct list_elem *ne;
  lock_acquire(&frame_lock);
  struct list_elem *e = list_begin(&frame_list);
  while (e != list_end(&frame_list)) {
    ne = list_next(e);
    frame = list_entry(e, struct frame, lelem);
    if (get_thread_by_tid(frame->tid) == NULL) {
      lock_release(&frame_lock);
      hash_delete(&frame_table, &frame->elem);
      list_remove(e);
      lock_acquire(&frame_lock);
    }
    e = ne;
  }
  lock_release(&frame_lock);
}


/* Set the kpage and pte of the frame when the pagedir is set */
void frame_set_status (void *kpage, uint32_t *pte, void *upage) {
  struct frame *frame = NULL; 
  lock_acquire(&frame_lock);
  for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
     frame = list_entry(e, struct frame, lelem);
     if (frame->kpage == kpage) {
       break;
     }
   }
  lock_release(&frame_lock); 
 
  if (frame != NULL) {
      frame->pte = pte;
      frame->user_vaddr = upage;
  }
}

/* Deallocate a frame by marking it as free */
void deallocate_frame(void *page) {  
  struct frame *frame = get_frame_by_kpage(page);
  lock_acquire(&frame_lock);
  hash_delete(&frame_table, &frame->elem);
  list_remove(&frame->lelem);
  lock_release(&frame_lock);
  palloc_free_page(frame->kpage);
  free(frame);
}

static bool
save_evicted_frame (struct frame *frame) {
  struct thread *t = get_thread_by_tid(frame->tid);
  lock_acquire(&t->spt_lock);
  struct spt_entry *spte = spt_find_page (&t->spt, frame->user_vaddr);
  lock_release(&t->spt_lock);

  if (spte == NULL) {
    spte = malloc (sizeof (struct spt_entry));
    spte->user_vaddr = frame->user_vaddr;
    spte->type = Swap;
    lock_acquire(&frame_lock);
    struct hash_elem *he = hash_insert(&t->spt, &spte->elem);
    list_push_back(&frame_list, &spte->lelem);
    
    if (he != NULL)
      return false;
    lock_release(&frame_lock);
  }

  size_t swap_slot = 0;

  if (pagedir_is_dirty (t->pagedir, spte->user_vaddr) && (spte->type == Mmap)) {
     file_seek (spte->file, spte->ofs);
     file_write (spte->file, spte->user_vaddr,spte->read_bytes);
  } else if ((pagedir_is_dirty (t->pagedir, spte->user_vaddr)) && (spte->writable)) {
    swap_slot = swap_out_memory (spte->user_vaddr);
    if (swap_slot == SWAP_ERROR)
      return false;
    spte->type = spte->type | Swap;
  }

  //delete_shared_page(search_shared_page(frame->kpage));
  struct shared_page *sp = search_shared_page(frame->kpage);
  memset (frame->kpage, 0, PGSIZE);

  spte->swap_slot = swap_slot;
  spte->writable = *(frame->pte) & PTE_W;
  spte->in_memory = false;
 /* struct shared_page *sp = search_shared_page(frame->kpage);
  struct thread *thr;
  struct list_elem *te = list_begin(&sp->pd_list); 
  struct list_elem *nte;
  printf("remove stuff when save\n");
  while (te != list_end(&sp->pd_list)) {
	//printf("remove from the pd list and clear page for tid %d\n", thr->tid);
    nte = list_next(te);
    thr = list_entry(te, struct thread, pd_elem);
     printf("remove from the pd list and clear page for tid %d\n", thr->tid);
    pagedir_clear_page (thr->pagedir, spte->user_vaddr);
    list_remove(te);
    te = nte;
  }*/
  if (sp != NULL) {
	  printf("remove a sp from its list\n");
  list_remove(&sp->elem);
  }
  pagedir_clear_page (t->pagedir, spte->user_vaddr);
  //delete_shared_page(sp);
  return true;
}

/* Get the frame with the page kpage */
struct frame *get_frame_by_kpage (void *kpage) {
  if (lock_held_by_current_thread(&frame_lock)) {
    lock_release(&frame_lock);
  } 
  lock_acquire(&frame_lock);
  struct frame *frame = NULL; 
  for (struct list_elem *e = list_begin(&frame_list); e != list_end(&frame_list); e = list_next(e)) {
    frame = list_entry(e, struct frame, lelem);
    if (frame->kpage == kpage) {
      lock_release(&frame_lock);
      return frame;
    }
  }
  lock_release(&frame_lock);
  return NULL;
}


/*static void
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
*/

/* Choose the frame by looping the list twice, decide the frame by its pinned and is the pagedie accessed */
struct frame* frame_to_evict (uint32_t *pagedir, struct list_elem **hand) {
  lock_acquire(&frame_lock);
  size_t n = list_size(&frame_list);
  size_t i;
  struct frame *e;
  
  for(i = 0; i <= n * 2; i++) {
    e = clock_frame_next(hand);
    if(e->pinned) {
      e->pinned = false;
      continue;
    } else if (pagedir_is_accessed(pagedir, e->user_vaddr)) {
      pagedir_set_accessed(pagedir, e->user_vaddr, false);
      continue;
    }
  
    lock_release(&frame_lock);
    return e;
  }

  PANIC ("Can't evict any frame, not enough memory");
}

/* Find the next elem from the head that evicted last time */
struct frame* clock_frame_next(struct list_elem **hand) {
  ASSERT (!list_empty(&frame_list));
 
  if (*hand == NULL || list_next(*hand) == list_end (&frame_list)) {
    *hand = list_begin (&frame_list);
  } else {
    *hand = list_next (*hand);
  }

  struct frame *e = list_entry(*hand, struct frame, lelem);

  return e;
}


