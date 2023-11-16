#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/kernel/hash.h"
#include <stdlib.h>

#define MAX_ARGS 1024
#define WORD_ALIGN_MASK 0xfffffffc

struct file_descriptor {
  int fd;                   
  struct file *file;         
  struct hash_elem elem;
  bool executing;  
};

tid_t process_execute (const char *file_name);
struct child *get_child_by_thread(struct thread *thread);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
