#include "userprog/syscall.h"
#include "userprog/pagedir.h"
#include "userprog/process.h"
#include "userprog/exception.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "devices/shutdown.h"
#include "devices/input.h"
#include <string.h>
#include <stdlib.h>
#include "threads/malloc.h"

struct lock syscall_lock;

static void syscall_handler(struct intr_frame *);

int get_user(const uint8_t *uaddr);

static bool put_user(uint8_t *udst, uint8_t byte);

void check_user(void * ptr);

struct child* get_child_by_thread(struct thread *thread);

int process_add_fd(struct file *file, bool executing);

unsigned fd_hash(const struct hash_elem *e, void *aux);
bool fd_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

/* Hash function to generate a hash value from a file descriptor. */
unsigned
fd_hash(const struct hash_elem *e, void *aux UNUSED) {
  struct file_descriptor *fd = hash_entry(e, struct file_descriptor, elem);
  return hash_int(fd->fd);
}

/* Hash less function to compare two file descriptors for ordering in
   the hash table. */
bool fd_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct file_descriptor *fd_a = hash_entry(a, struct file_descriptor, elem);
  struct file_descriptor *fd_b = hash_entry(b, struct file_descriptor, elem);
  return fd_a->fd < fd_b->fd;
}  

void
syscall_init(void) {
  lock_init(&syscall_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//bool first_time = true;

static void
syscall_handler(struct intr_frame *f) {
  if (!thread_current()->init_fd) {
    hash_init(&thread_current()->fd_table, fd_hash, fd_less, NULL);
    thread_current()->init_fd = true;
  }

  if (!is_user_vaddr(f->esp) || f->esp < (void *) 0x08048000) {
    exit(-1);
  }


  int syscall_num = *(int *) (f->esp);

  switch (syscall_num) {
    case SYS_HALT: { /* Halts Pintos */
      halt();
      break;
    }
    case SYS_EXIT: { /* Terminate user process*/
      check_user(f->esp + 4);
      int status = *((unsigned *) (f->esp + 4)); // if status = -1 page_fault
      exit(status);
      break;
    }
    case SYS_EXEC: { /**/  // added cast to line below: fixes warning but is it safe?
      check_user(f->esp + 4);
      const char *cmd_line = *((const char **) (f->esp + 4)); // if status...
      f->eax = (uint32_t) exec(cmd_line);
      break;
    }
    case SYS_WAIT: {
      check_user(f->esp + 4);
      pid_t pid = *((pid_t *) (f->esp + 4));
      f->eax = (uint32_t) wait(pid);
      break;
    }
    case SYS_CREATE: { /* Create a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      const char *file = *((const char **) (f->esp + 4));
      unsigned initial_size = *((unsigned *) (f->esp + 8));
      f->eax = (uint32_t) create(file, initial_size);
      break;
    }
    case SYS_REMOVE: { /* Delete a file. */
      check_user(f->esp + 4);
      const char *file = *((const char **) (f->esp + 4));
      f->eax = (uint32_t) remove(file);
      break;
    }
    case SYS_OPEN: {  /* Open a file. */
      check_user(f->esp + 4);
      const char *file = *((const char **) (f->esp + 4));
      f->eax = (uint32_t) open(file);
      break;
    }
    case SYS_FILESIZE: { /* Obtain a file's size. */
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) filesize(fd);
      break;
    }
    case SYS_READ: {  /* Read from a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      check_user(f->esp + 12);
      int fd = *((int *) (f->esp + 4));
      void *buffer = *((void **) (f->esp + 8));
      unsigned size = *((unsigned *) (f->esp + 12));
      f->eax = (uint32_t) read(fd, buffer, size);
      break;
    }
    case SYS_WRITE: { /* Write to a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      int fd = *((int *) (f->esp + 4));
      // maybe should be: int fd = get_user(f->esp + 4);
      const void *buffer = *((const void **)(f->esp + 8));
      unsigned size = *((unsigned *)(f->esp + 12));
    //  printf("fd %d and size %d\n", fd, size);
      f->eax = (uint32_t) write(fd, buffer, size);
      break;
    }
    case SYS_SEEK: { /* Change position in a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      int fd = *((int *) (f->esp + 4));
      unsigned position = *((unsigned *) (f->esp + 8));
      seek(fd, position);
      break;
    }
    case SYS_TELL: { /* Report current position in a file. */
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) tell(fd);
      break;
    }
    case SYS_CLOSE: {
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      close(fd);
      break;
    }
    default: {
      exit(-1);
      break;
    }
  }
  //thread_exit();
}

void
halt(void) {
  shutdown_power_off();
}

static void free_fd(struct hash_elem *e, void *aux UNUSED) {
  struct file_descriptor *fd = hash_entry(e, struct file_descriptor, elem);
  file_close(fd->file);
  free(fd);
}

void
exit(int status) {

  struct thread *cur = thread_current();
  printf ("%s: exit(%d)\n", cur->name, status);

  struct thread *parent = get_thread_by_tid(cur->parent_tid);
  if (parent != NULL && parent->tid != 1) {
    struct child *child = find_child_in_cp_manager(cur->tid, &parent->cp_manager);
    lock_acquire(&parent->cp_manager.children_lock);
    child->exit_status = status;
    child->call_exit = true;
    lock_release(&parent->cp_manager.children_lock);
  }
  //struct hash_iterator i;
  hash_destroy(&cur->fd_table, free_fd);
  thread_exit();
  //printf("after thread exi\nt");
}

pid_t
exec(const char *cmd_line) {
  // Check if the command line pointer is valid
  check_user(cmd_line);
  if (cmd_line == NULL || !is_user_vaddr(cmd_line)) {
    return -1;
  }

  /* Create a new process and check if failed */
  pid_t pid = process_execute(cmd_line);
  if (pid == TID_ERROR) {
    return pid;
  }

  struct thread *cur = thread_current();
  lock_acquire(&cur->cp_manager.children_lock);
  cur->cp_manager.load_result = 0;

  while (cur->cp_manager.load_result == 0) {
    cond_wait(&cur->cp_manager.children_cond, &cur->cp_manager.children_lock);
  }

  if (&cur->cp_manager.load_result == -1) {
    get_thread_by_tid(cur->parent_tid)->cp_manager.load_result = -1;
    pid = -1;
  }

  lock_release(&cur->cp_manager.children_lock);
  return pid;

}


int
wait(pid_t pid) {
  /*since each process has one thread, pid == tid*/
  /*int status = process_wait(pid);
  if (status == -1) {
    exit(-1);
  }*/
  return process_wait(pid);
}


bool
create(const char *file, unsigned initial_size) {
  check_user(file);
  if (file == NULL) {
    exit(-1);
    return false;
  }
  lock_acquire(&syscall_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&syscall_lock);
  return success;
}

bool
remove(const char *file) {
  lock_acquire(&syscall_lock);
  bool success = filesys_remove(file);
  lock_release(&syscall_lock);
  return success;
  //return filesys_remove(file);
}

int
open(const char *file) {
//	printf("open a file fo tid %d\n", thread_current()->tid);
  check_user(file);
  if (file == NULL) {
    exit(-1);
    return -1;
  }
  lock_acquire(&syscall_lock);
  int status;
  struct file *f = filesys_open(file);
  if (f == NULL) {
    // File could not be opened
    status = -1;
  } else {
    // Add the file to the process's open file list and return the file descriptor
    status = process_add_fd(f, !strcmp(file, thread_current()->name));
  }
  lock_release(&syscall_lock);
  return status;
}

 
int 
filesize(int fd) {
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) {
    return -1; // File not found
  }
  int size;
  //lock_acquire(&syscall_lock);
  size = file_length(filed->file);
  lock_release(&syscall_lock);
  return size;
}


int
read(int fd, void *buffer, unsigned size) {
  check_user(buffer);
  int read_size;
  lock_acquire(&syscall_lock);
  if (fd == 0) {
    unsigned i;
    for (i = 0; i < size; i++) {
      ((uint8_t *) buffer)[i] = input_getc();
    }
    read_size = size;
  } else if (size == 0) {
    read_size = size;
  } else {
    struct file_descriptor *filed = process_get_fd(fd);
    if (filed == NULL){
      read_size = -1; // File not found
    } else {
      read_size = file_read(filed->file, buffer, size);
    }
  }
  lock_release(&syscall_lock);
  return read_size;
}



int
write(int fd, const void *buffer, unsigned size) {
 // printf("write \n");
  check_user(buffer);
  int write_size;
  lock_acquire(&syscall_lock);
  if (fd == 1) {  // writes to console
    int linesToPut;
    for (uint32_t j = 0; j < size; j += MAX_CONSOLE_WRITE) {  // max 200B at a time, j US so can compare with size
      linesToPut = (size < j + MAX_CONSOLE_WRITE) ? (size % MAX_CONSOLE_WRITE) : (j + MAX_CONSOLE_WRITE);
      putbuf(buffer + j, linesToPut);
    }
    write_size = size;
  } else if (size == 0) {
    write_size = size;
  } else {
    struct file_descriptor *filed = process_get_fd(fd);
    if (filed == NULL){
      write_size = -1;     
    } else if (filed->executing) {
      write_size = 0;
    } else { 
      write_size = file_write(filed->file, buffer, size);
    }
  }
  lock_release(&syscall_lock);
  return write_size;
}


void
seek(int fd , unsigned position) {
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed != NULL) {
    //lock_acquire(&syscall_lock);
    file_seek(filed->file, position);
  }
  lock_release(&syscall_lock);
  //}
}


unsigned
tell(int fd) {
  unsigned position;
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) {
    position = -1; // File not found
  } else {
    //lock_acquire(&syscall_lock);
    position = file_tell(filed->file);
    //lock_release(&syscall_lock);
  }
  lock_release(&syscall_lock);
  return position;
}


void
close(int fd) {
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) {
    exit(-1);
  }
  //filed->opened = false;
  //struct file *f = filed->file;
  //if (f != NULL) {
//  lock_acquire(&syscall_lock);
  process_remove_fd(fd);
  lock_release(&syscall_lock);
  //} else {
  //  exit(-1);
//  }
}

struct child *
get_child_by_thread(struct thread *thread) {
  struct thread *parent = get_thread_by_tid (thread->parent_tid);
  struct child *child = NULL;
  lock_acquire(&parent->children_lock);
  if (parent != NULL  && parent->tid != 1) {
    for (struct list_elem *e = list_begin(&parent->children); e != list_end(&parent->children); e = list_next(e)) {
      child = list_entry(e, struct child, child_elem);
      if (child->tid == thread->tid){
        break;
      }
    }
  }
  lock_release(&parent->children_lock);
  return child;
}

void
check_user (void *ptr)
{
// don't need to worry about code running after as it kills the process
  struct thread *t = thread_current();
  uint8_t *uaddr = ptr;
  if (!is_user_vaddr((void *) ptr) || (pagedir_get_page(t->pagedir, uaddr) == NULL)) {
    exit(-1);
  }
  
}

// credit to pintos manual:
/* Reads a byte at user virtual address UADDR.
 * Returns the byte value if successful, -1 if a segfault
 * occurred. */
int
get_user (const uint8_t *uaddr)
{
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
  int error_code;
  asm ("movl $1f, %0; movb %b2, %1; 1:"
          : "=&a" (error_code), "=m" (*udst) : "q" (byte));
  return error_code != -1;
}


struct file_descriptor *
process_get_fd(int fd) {
  struct thread *t = thread_current();
  struct file_descriptor fd_temp;
  struct hash_elem *e;

  fd_temp.fd = fd;
  e = hash_find(&t->fd_table, &fd_temp.elem);

  return e != NULL ? hash_entry(e, struct file_descriptor, elem) : NULL;
}

int
process_add_fd(struct file *file, bool executing) {
  static int next_fd = FIRST_FD_NUMBER; /* Magic number? after 0 and 1 */
  struct file_descriptor *fd = malloc(sizeof(struct file_descriptor));
 
  if (fd == NULL) return -1;

  fd->file = file;
  fd->fd = next_fd++;
  fd->executing = executing;

  struct thread *t = thread_current();
  hash_insert(&t->fd_table, &fd->elem);

  return fd->fd;
}

void
process_remove_fd(int fd) {
  struct file_descriptor *fd_struct = process_get_fd(fd);
  //printf("removing fd from tid %d\n", thread_current()->tid);
  if (fd != -1) {
    hash_delete(&thread_current()->fd_table, &fd_struct->elem);
    file_close(fd_struct->file);
    free(fd_struct);  //TODO must free this
    //printf("after free\n");
  }
} 

