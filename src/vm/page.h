#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <stdbool.h>

struct page {
  void *frame;
  bool present;
  bool dirty;
};

void* allocate_page(void);
void deallocate_page(struct page *pg);

#endif /* vm/page.h */


