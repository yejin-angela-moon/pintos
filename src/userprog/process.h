#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"
#include "lib/kernel/hash.h"
#include "threads/synch.h"
#include <stdlib.h>

#define MAX_ARGS 1024
#define WORD_ALIGN_MASK 0xfffffffc
#define ESP_DECREMENT 4

struct child {
  tid_t tid;                     /* tid of the thread with the status. */
  struct list_elem child_elem;   /* list_elem to be inserted in children of thread. */
  int exit_status;               /* The status when exit. */
  bool waited;                   /* Whether wait is called for the thread. */
  bool call_exit;                /* Whether exit is called for the thread. */
};

struct file_descriptor {
  int fd;                     /* The file descriptor number. */  
  struct file *file;          /* The file. */
  struct hash_elem elem;      /* hash_elem to be inserted to fd_table of thread. */
  bool executing;             /* Whether the file is executing. */
};


tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */
