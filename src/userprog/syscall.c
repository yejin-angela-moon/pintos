#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "devices/shutdown.h"

static void syscall_handler(struct intr_frame *);

int get_user(const uint8_t *uaddr);

static bool put_user(uint8_t *udst, uint8_t byte);

void check_user(struct intr_frame *f, uint32_t ptr);


void
syscall_init(void) {
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f) {

//   check_user_pointer(f, f->esp);  // check the frame's stack ptr is valid
//   				  // whoever is dealing with arguments, call the above func on each one
//   int val = get_user(f->esp);  // check and put user should be used to dereference
//   if (val == -1)               // or write to location at any user pointer
//     page_fault(f)

//   if !(put_user(f->esp, 0))    // for any write
//     page_fault(f);

//   // these checks should ensure that page_fault is only called when a user program
//   // makes an invalid memory access: if on happens in the kernel during a system call,
//   // we detect it and kill the program to stop the kernel exploding


  printf("system call!\n");
  int syscall_num = *(int *) (f->esp);

  if (!is_user_vaddr(f->esp) || f->esp < (void *) 0x08048000) {
    exit(-1);
    return;
  }

  switch (syscall_num) {
    case SYS_HALT: { /* Halts Pintos */
      halt();
      break;
    }
    case SYS_EXIT: { /* Terminate user process */
      int status = get_user(f->esp + 4); // if status = -1 page_fault
      exit(status);
      break;
    }
    case SYS_EXEC: { /**/  // added cast to line below: fixes warning but is it safe?
      const char *cmd_line = (char *) get_user(f->esp + 4); // if status...
      f->eax = (uint32_t) exec(cmd_line);
      break;
    }
    case SYS_WAIT: {
      pid_t pid = *((pid_t *)(f->esp +4));
      f->eax = (uint32_t) wait(pid);
      break;
    }
    case SYS_CREATE: { /* Create a file. */
      const char *file = *((const char **)(f->esp + 4));
      unsigned initial_size = *((unsigned *)(f->esp + 8));
      f->eax = (uint32_t) create(file, initial_size);
      break;
    }
    case SYS_REMOVE: { /* Delete a file. */
      const char *file = *((const char **)(f->esp + 4));
      f->eax = (uint32_t) remove(file);
      break;
    }
    case SYS_OPEN: {  /* Open a file. */
      const char *file = *((const char **)(f->esp + 4));
      f->eax = (uint32_t) open(file);
      break;
    }
    case SYS_FILESIZE: { /* Obtain a file's size. */
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) filesize(fd);
      break;
    }
    case SYS_READ: {  /* Read from a file. */
      int fd = *((int *) (f->esp + 4));
      void *buffer = *((void **)(f->esp + 8));
      unsigned size = *((unsigned *)(f->esp + 12));
      f->eax = (uint32_t) read(fd, buffer, size);
      break;
    }
    case SYS_WRITE: { /* Write to a file. */
      int fd = *((int *) (f->esp + 4));
      // maybe should be: int fd = get_user(f->esp + 4);
      const void *buffer = *((const void **)(f->esp + 8));
      unsigned size = *((unsigned *)(f->esp + 12));
      f->eax = (uint32_t) write(fd, buffer, size);
      break;
    }
    case SYS_SEEK: { /* Change position in a file. */
      int fd = *((int *) (f->esp + 4));
      unsigned position = *((unsigned *)(f->esp + 8));
      seek(fd, position);
      break;
    }
    case SYS_TELL: { /* Report current position in a file. */
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) tell(fd);
      break;
    }
    case SYS_CLOSE: {
      int fd = *((int *) (f->esp + 4));
      close(fd);
      break;
    }
    default: {
      exit(-1);
      break;
    }
  }
  thread_exit();
}

void
halt(void) {
  shutdown_power_off();
}

void
exit(int status) {
  struct thread *cur = thread_current();
  cur->exit_status = status;
  cur->call_exit = true;
  thread_exit();
}

pid_t
exec(const char *cmd_line UNUSED){
  //TODO
  return 0;
}

int
wait(pid_t pid) {
  /*since each process has one thread, pid == tid*/
  return process_wait(pid);
}

bool
create(const char *file, unsigned initial_size) {
  return filesys_create(file, initial_size);
}

bool
remove(const char *file) {
  return filesys_remove(file);
}

int
open(const char *file) {
  struct file *f = filesys_open(file);
  if (f == NULL) {
    // File could not be opened
    return -1;
  } else {
    // Add the file to the process's open file list and return the file descriptor
    return 0; //TODO implement process_add_file(f);
  }
}

int
filesize(int fd UNUSED) {
  //TODO
  return 0;
}

int
read(int fd UNUSED, void *buffer UNUSED, unsigned size UNUSED) {
  //TODO
  return 0;
}

int
write(int fd, const void *buffer, unsigned size) {
  if (fd == 1) {  // writes to conole
    int linesToPut;
    for (unsigned j; j < size; j += 200) {  // max 200B at a time, j US so can compare with size
      linesToPut = (size < j + 200) ? size : j + 200;
      putbuf(buffer + j, linesToPut);
    }
    return size;
  }
  unsigned i;
  for (i = 0; i < size; i++) {
    if (!put_user((uint8_t) fd+i, (uint8_t) buffer+i)) // added cast not sure if thats cool
      break;
  }
  return i;
}

void
seek(int fd UNUSED, unsigned position) {
  //TODO
}

unsigned
tell(int fd UNUSED) {
  //TODO
  return 0;
}

void
close(int fd UNUSED) {
  //TODO
}


// credit to pintos manual: modified to include check_user
void
check_user (struct intr_frame *f, uint32_t ptr)
{
// don't need to worry about code running after as it kills the process
  if (!is_user_vaddr(ptr))
    kill(f);
}


/* Reads a byte at user virtual address UADDR.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
int
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

