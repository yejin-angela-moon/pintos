#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>

#define MAX_ARGS 40
#define WORD_ALIGN_MASK 0xfffffffc

struct file_descriptor {
  int fd;                   
  struct file *file;         
  struct hash_elem elem;
  bool executing; 
};


tid_t process_execute (const char *file_name);
struct child *get_child_by_thread(struct thread *thread);
struct child *find_child_in_cp_manager(tid_t tid, struct child_parent_manager *cp_manager);
struct child *find_or_create_child(tid_t pid, struct child_parent_manager *cp_manager);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
