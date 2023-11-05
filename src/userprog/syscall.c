#include "userprog/syscall.h"
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
  check_user_pointer(f, f->esp);  // check the frame's stack ptr is valid
  // whoever is dealing with arguments, call the above func on each one
  printf ("system call!\n");
  thread_exit ();
}

static void
check_user_pointer (struct intr_frame *f, uint32_t ptr)
{
// don't need to worry about code running after as it should kill the process
  if (!is_user_vaddr(ptr))
    page_fault(f);  
}


