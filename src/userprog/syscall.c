#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");
  thread_exit ();
}

void
exit(int status) {
  struct thread *cur = thread_current();
  cur->exit_status = status;
  cur->call_exit = true;
  thread_exit();
}

int 
wait(pid_t pid) {
  /*since each process has one thread, pid == tid*/
  return process_wait(pid);
}


