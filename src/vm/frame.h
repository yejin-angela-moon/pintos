#ifndef VM_FRAME_H
#define VM_FRAME_H

#include "../lib/kernel/hash.h"
#include "../threads/synch.h"
#include "../threads/palloc.h"

struct frame_entry {
    struct page_entry *page;     
    bool in_use;            
    struct hash_elem hash_elem; 
};



unsigned frame_hash(const struct hash_elem *elem, void *aux);

bool frame_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

void frame_table_init(struct frame_table *ft, size_t num_frames);

void *allocate_frame(enum palloc_flags flags, void *aux);

void free_frame(struct frame_table *ft, struct frame_entry *frame);



#endif
