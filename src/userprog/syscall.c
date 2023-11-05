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
syscall_handler (struct intr_frame *f) 
{
  check_user_pointer(f, f->esp);  // check the frame's stack ptr is valid
  				  // whoever is dealing with arguments, call the above func on each one
  int val = get_user(f->esp);  // check and put user should be used to dereference
  if (val == -1)               // or write to location at any user pointer  
    page_fault(f) 

  if !(put_user(f->esp, 0))    // for any write
    page_fault(f);

  printf ("system call!\n");
  thread_exit ();
}


// credit to pintos manual: modified to include check_user
static void
check_user (struct intr_frame *f, uint32_t ptr)
{
// don't need to worry about code running after as it kills the process
  if (!is_user_vaddr(ptr))
    kill();  
}


/* Reads a byte at user virtual address UADDR.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
static int
get_user (const uint8_t *uaddr)
{
  check_user(uaddr);
  int result;
  asm ("movl $1f, %0; movzbl %1, %0; 1:"
	: "=&a" (result) : "m" (*uaddr));
  return result;
}

/* Writes BYTE to user address UDST.
 * Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
  check_user(udst);
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
	: "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}

