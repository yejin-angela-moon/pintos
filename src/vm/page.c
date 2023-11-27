#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "../lib/kernel/hash.h"
#include "../vm/page.h"
// could move to a header file

unsigned page_hash(const struct hash_elem *elem, void *aux) {
    struct page_entry *page = hash_entry(elem, struct page_entry, elem);
    return hash_int((int)page->user_vaddr);
}

// Comparison function for page entries
bool page_less(const struct hash_elem *a, const struct hash_elem *b, void *aux) {
    struct page_entry *page_a = hash_entry(a, struct page_entry, elem);
    struct page_entry *page_b = hash_entry(b, struct page_entry, elem);
    return page_a->user_vaddr < page_b->user_vaddr;  
}


