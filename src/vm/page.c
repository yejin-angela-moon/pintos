#include "vm/page.h"
#include <stddef.h>
#include <stdbool.h>

void page_init(struct page *pg) {
  pg->frame = NULL;
  pg->present = false;
  pg->dirty = false;
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

