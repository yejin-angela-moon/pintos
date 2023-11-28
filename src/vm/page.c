#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "vm/page.h"
// could move to a header file

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

struct sup_page_table*
spt_create (void) {
  struct sup_page_table *spt = malloc(sizeof(struct sup_page_table));
  hash_init (&spt->table, spt_hash, spt_less, NULL);
  return spt;
}

