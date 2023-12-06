#include "userprog/syscall.h"
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
#include "userprog/pagedir.h"

struct lock syscall_lock;
static void syscall_handler(struct intr_frame *);
int get_user(const uint8_t *uaddr);
static bool put_user(uint8_t *udst, uint8_t byte);
void check_user(void * ptr);
int process_add_fd(struct file *file, bool executing);
unsigned fd_hash(const struct hash_elem *e, void *aux);

bool fd_less(const struct hash_elem *a, const struct hash_elem *b, void *aux);

//static int next_mmap_id = 1;

/* Hash function to generate a hash value from a file descriptor. */
unsigned
fd_hash(const struct hash_elem *e, void *aux UNUSED) {
  struct file_descriptor *fd = hash_entry(e, struct file_descriptor, elem);
  return hash_int(fd->fd);
}

/* Hash less function to compare two file descriptors for ordering in
   the hash table. */
bool 
fd_less(const struct hash_elem *a, const struct hash_elem *b, void *aux UNUSED) {
  struct file_descriptor *fd_a = hash_entry(a, struct file_descriptor, elem);
  struct file_descriptor *fd_b = hash_entry(b, struct file_descriptor, elem);
  return fd_a->fd < fd_b->fd;
}  

void
syscall_init(void) {
  lock_init(&syscall_lock);
  intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f) {
  /* Initialise fd_table if it is not initialised. */
  if (!thread_current()->init_fd) {
    hash_init(&thread_current()->fd_table, fd_hash, fd_less, NULL);
    thread_current()->init_fd = true;	  
  }

  /* Call exit(-1) for invalid esp. */
  if (!is_user_vaddr(f->esp)) {
    exit(-1);
  }
  //This is to obtain the esp for stack growth
  thread_current()->esp = f->esp;

  /* Casting syscall_num into an int */
  int syscall_num = *(int *) (f->esp);
//  printf("the syscall is %d\n", syscall_num);
  switch (syscall_num) {
    case SYS_HALT: {      /* Halts Pintos */
      halt();
      break;
    }   
    case SYS_EXIT: {      /* Terminate user process*/
      check_user(f->esp + 4);
      int status = *((unsigned *) (f->esp + 4)); 
      exit(status);
      break;
    }
    case SYS_EXEC: {      /* Execute the command line. */  
      check_user(f->esp + 4);
      const char *cmd_line = *((const char **) (f->esp + 4));
      f->eax = (uint32_t) exec(cmd_line);
      break;
    }
    case SYS_WAIT: {      /* Wait for a child process. */
      check_user(f->esp + 4);
      pid_t pid = *((pid_t * )(f->esp + 4));
      f->eax = (uint32_t) wait(pid);
      break;
    }
    case SYS_CREATE: {    /* Create a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      const char *file = *((const char **) (f->esp + 4));
      unsigned initial_size = *((unsigned *) (f->esp + 8));
      f->eax = (uint32_t) create(file, initial_size);
      break;
    }
    case SYS_REMOVE: {    /* Delete a file. */
      check_user(f->esp + 4);
      const char *file = *((const char **) (f->esp + 4));
      f->eax = (uint32_t) remove(file);
      break;
    }
    case SYS_OPEN: {      /* Open a file. */
      check_user(f->esp + 4);
      const char *file = *((const char **) (f->esp + 4));
      f->eax = (uint32_t) open(file);
      break;
    }
    case SYS_FILESIZE: {  /* Obtain a file's size. */
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) filesize(fd);
      break;
    }
    case SYS_READ: {      /* Read from a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      check_user(f->esp + 12);
      int fd = *((int *) (f->esp + 4));
      void *buffer = *((void **) (f->esp + 8));
      unsigned size = *((unsigned *) (f->esp + 12));
      f->eax = (uint32_t) read(fd, buffer, size);
      break;
    }
    case SYS_WRITE: {     /* Write to a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      check_user(f->esp + 12);
      int fd = *((int *) (f->esp + 4));
      const void *buffer = *((const void **)(f->esp + 8));
      unsigned size = *((unsigned *) (f->esp + 12));
      f->eax = (uint32_t) write(fd, buffer, size);
      break;
    }
    case SYS_SEEK: {      /* Change position in a file. */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      int fd = *((int *) (f->esp + 4));
      unsigned position = *((unsigned *) (f->esp + 8));
      seek(fd, position);
      break;
    }
    case SYS_TELL: {      /* Report current position in a file. */
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      f->eax = (uint32_t) tell(fd);
      break;
    }
    case SYS_CLOSE: {     /* Close the file. */
      check_user(f->esp + 4);
      int fd = *((int *) (f->esp + 4));
      close(fd);
      break;
    }
    case SYS_MMAP: {  /* memory map a file */
      check_user(f->esp + 4);
      check_user(f->esp + 8);
      int fd = *((int *) (f->esp + 4));
      void *addr = *((void **) (f->esp + 8));
      f->eax = (uint32_t) mmap(fd, addr);
      break;
    }
    case SYS_MUNMAP: {  /* unmaps a file */
      check_user(f->esp + 4);
      mapid_t mid = *((mapid_t *) (f->esp + 4));
      munmap(mid);
      break;
    }
    default: {
      exit(-1);
      break;
    }
  }
}

void
halt(void) {
  shutdown_power_off();
}

/* Find the child by the tid and the cp_manager. */
static struct child *find_child_in_cp_manager(tid_t tid, struct child_parent_manager *cp_manager) {
    if (cp_manager == NULL) {
        return NULL;
    }
    struct list_elem *e;
    for (e = list_begin(&cp_manager->children_list); e != list_end(&cp_manager->children_list); e = list_next(e)) {
        struct child *child = list_entry(e, struct child, child_elem);
        if (child->tid == tid) {
            return child;          
	}
    }
    return NULL;  
}

void
exit(int status) {
  struct thread *cur = thread_current();
  printf ("%s: exit(%d)\n", cur->name, status);

  /* Change the status of the child. */
  struct thread *parent = get_thread_by_tid (cur->parent_tid);
  if (parent != NULL) { //  && par) { 
    struct child *child = find_child_in_cp_manager(cur->tid, &parent->cp_manager);
    lock_acquire(&parent->cp_manager.children_lock);
    child->exit_status = status;
    child->call_exit = true;    
    lock_release(&parent->cp_manager.children_lock);
  }
 
  thread_exit();
}

pid_t
exec(const char *cmd_line) {
  /* Check if the command line pointer is valid */
  if (cmd_line == NULL || !is_user_vaddr(cmd_line)) {
    return FAIL;
  }
  
  /* Execute the command line and get the pid. */ 
  pid_t pid = process_execute(cmd_line);
  if (pid == TID_ERROR) {
    return FAIL;
  }

  /* Wait until the load_result is not 0. If load_result is -1, return load_result. */
  struct thread *cur = thread_current();
  lock_acquire(&cur->cp_manager.children_lock);
  cur->cp_manager.load_result = 0;
  while(cur->cp_manager.load_result == 0) {
     cond_wait(&cur->cp_manager.children_cond, &cur->cp_manager.children_lock);
  }
  if (cur->cp_manager.load_result == FAIL) {
    pid = FAIL;
  }
  lock_release(&cur->cp_manager.children_lock);
  
  return pid;
}

int
wait(pid_t pid) {
  /* Since each process has one thread, pid == tid. Pass the pid to process_wait(). */
  return process_wait(pid);
}

bool
create(const char *file, unsigned initial_size) {
  /* Check if file is NULL. */
  if (file == NULL) {
    exit(-1);
    return false;
  }

  /* Create the file by filesys_create. */
  lock_acquire(&syscall_lock);
  bool success = filesys_create(file, initial_size);
  lock_release(&syscall_lock);
  return success;
}

bool
remove(const char *file) {
  /* Check if file is NULL. */
  if (file == NULL) {
    exit(-1);
    return false;
  }

  /* Remove the file by filesys_remove. */
  lock_acquire(&syscall_lock);
  bool success = filesys_remove(file);
  lock_release(&syscall_lock);
  return success;
}

int
open(const char *file) {
  /* Check if file is NULL */
  if (file == NULL) { 
    exit(-1);
    return FAIL;
  }

  /* Open the file by filesys_open. */
  lock_acquire(&syscall_lock);
  int status;
  struct file *f = filesys_open(file);
  if (f == NULL) { /* Case when file not found. */
    status = FAIL;
  } else {
    /* Add the file to the process's open file list and return the fd of the file descriptor */
    status = process_add_fd(f, !strcmp(file, thread_current()->name));
  }
  lock_release(&syscall_lock);
  
  return status;
}
 
int 
filesize(int fd) {
  lock_acquire(&syscall_lock);
  /* Check whether the file_descriptor exist. */
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) { /* Case when file not found. */
    return FAIL; 
  }

  /* Get the size of the file by file_length. */
  int size;
  size = file_length(filed->file);
  lock_release(&syscall_lock);
  return size;
}

int
read(int fd, void *buffer, unsigned size) {
  /* Check whether the buffer is valid. */
  check_user(buffer);
  int read_size;
  lock_acquire(&syscall_lock);
  if (fd == 0) {     /* Reading from the keyboard */
    //unsigned i;
    for (unsigned i = 0; i < size; i++) {
      ((uint8_t *) buffer)[i] = input_getc();
    }
    read_size = size;
  } else if (size == 0) { /* Case when input size is 0. */
    read_size = size;
  } else {
    struct file_descriptor *filed = process_get_fd(fd);
    if (filed == NULL){ /* Case when file not found. */
      read_size = FAIL; 
    } else {      /* Get the size bytes read by file_read. */
      read_size = file_read(filed->file, buffer, size);
    }
  }
  lock_release(&syscall_lock);
  return read_size;
}

int
write(int fd, const void *buffer, unsigned size) {
  int write_size;
  lock_acquire(&syscall_lock);
  if (fd == 1) {    /* Writes to console */
    int linesToPut;
    for (uint32_t j = 0; j < size; j += MAX_CONSOLE_WRITE) {  /* max 200B (MAX_CONSOLE_WRITE) at a time */
      linesToPut = (size < j + MAX_CONSOLE_WRITE) ? (size % MAX_CONSOLE_WRITE) : (j + MAX_CONSOLE_WRITE);
      putbuf(buffer + j, linesToPut);
    }
    write_size = size;
  } else if (size == 0) { /* Case when input size is 0. */
    write_size = size;
  } else {
    struct file_descriptor *filed = process_get_fd(fd);
    if (filed == NULL){  /* Case when file not found. */
      write_size = FAIL;     
    } else if (filed->executing) { /* If the file is executing, it will not be written. */
      write_size = 0;
    } else {         /* Get the size bytes written by file_write. */
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
  if (filed != NULL) {  /* Changes the next byte to be read or written in open file fd to position by file_seek. */
    file_seek(filed->file, position);
  } 
  lock_release(&syscall_lock);
}

unsigned
tell(int fd) {
  unsigned position;
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) {  /* Case when file not found. */
    position = FAIL; 
  } else { /* Returns the position of the next byte to be read or written by file_tell. */
    position = file_tell(filed->file);
  }
  lock_release(&syscall_lock);
  return position;
}

void
close(int fd) {
  lock_acquire(&syscall_lock);
  struct file_descriptor *filed = process_get_fd(fd);
  if (filed == NULL) { /* Case when file not found. */
    exit(-1);
  } 
  /* Remove the fd form fd_table, close the file and free the file_descriptor. */
  process_remove_fd(fd);
  lock_release(&syscall_lock);
}


mapid_t
add_mmap(struct map_file *mmap) {
  mmap->mid = thread_current()->mmap_id++;  // alternative: could use  hash
  list_push_back(&thread_current()->mmap_files, &mmap->elem);
  return mmap->mid;
}

bool
validate_mapping(void *addr, int length) {
  if (length == 0 || (uint32_t) addr % PGSIZE != 0)  // checks if file is empty or address is unaligned
    return false;
  
 void *end_addr = addr + (length - 1);
  for (void *i = addr; i < end_addr; i += PGSIZE) {
    if (pagedir_get_page(thread_current()->pagedir, addr) != NULL)  // page already in use
      return false;
  }
  
  // this is meant to return false if overlaps with stack growth region, probably needs to be tweaked: TODO
  return !((PHYS_BASE - (uint32_t) addr) <= PGSIZE || (PHYS_BASE - (uint32_t) end_addr) <= PGSIZE);

}

mapid_t
mmap(int fd, void *addr) {
  if (fd == 0 || fd == 1) {
    return -1;
  }
  struct file_descriptor *file = process_get_fd(fd);
  if (file == NULL || addr == 0)  // check for invalid file or addr
    return -1; 
 
  int length = file_length(file->file);
  if (!validate_mapping(addr, length))
    return -1;

  struct map_file *mmap = malloc(sizeof(struct map_file));
  if (mmap == NULL)
    return -1;

  mmap->file = file->file; 
  mmap->addr = addr;
  mmap->length = length;
  
  list_init(&mmap->pages);

  mmap->mid = thread_current()->mmap_id++;  // alternative: could use  hash
  list_push_back(&thread_current()->mmap_files, &mmap->elem);
  
  //add_mmap(mmap);
  // TODO do lazy load pages
  // then add the spt_entries of those pages to mmap->pages
  return mmap->mid;
}

void
munmap(mapid_t mapping) {
  struct list map_list = thread_current()->mmap_files;
  struct list_elem *e;
  struct map_file *mf;
  for (e = list_begin (&map_list); e != list_end (&map_list);
		  e = list_next (e)) {
    struct map_file *mmap = list_entry (e, struct map_file, elem);
    if (mmap->mid == mapping) {
      mf = mmap;
      list_remove(e);
      break;
  }
  if (mf == NULL)
    return;  // not found
  
  // TODO remove page from list of virtual pages
  for (e = list_begin (&mf->pages); e != list_end (&mf->pages);
                  e = list_next (e)) {
    struct spt_entry *page = list_entry (e, struct spt_entry, lelem);
    if (page->is_dirty)
      // TODO write back to file
      page->is_dirty = false;  
  }
  free(mf);

  }}

void
check_user (void *ptr) {
/* Don't need to worry about code running after as it kills the process */
    struct thread *t = thread_current();
    uint8_t *uaddr = ptr;
    if (!is_user_vaddr((void *) ptr) || (pagedir_get_page(t->pagedir, uaddr) == NULL)) {
        exit(-1);
    }

}

/* Credit to pintos manual: */
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

/* Get the file_descriptor by its fd. */
struct file_descriptor *
process_get_fd(int fd) {
  struct thread *t = thread_current();
  struct file_descriptor fd_temp;
  struct hash_elem *e;

  fd_temp.fd = fd;
  e = hash_find(&t->fd_table, &fd_temp.elem);

  return e != NULL ? hash_entry(e, struct file_descriptor, elem) : NULL;
}

/* Add a new fd to fd_table. */
int
process_add_fd(struct file *file, bool executing) {
  static int next_fd = FIRST_FD_NUMBER; 
  struct file_descriptor *fd = malloc(sizeof(struct file_descriptor));
 
  if (fd == NULL) {
   return FAIL;
  }
  fd->file = file;
  fd->fd = next_fd++;
  fd->executing = executing;
  hash_insert(&thread_current()->fd_table, &fd->elem);

  return fd->fd;
}

/* Remove the fd form fd_table, close the file and free the file_descriptor. */
void
process_remove_fd(int fd) {
  struct file_descriptor *fd_struct = process_get_fd(fd);
  if (fd != FAIL) {
    hash_delete(&thread_current()->fd_table, &fd_struct->elem);
    file_close(fd_struct->file);
    free(fd_struct);  
  }
} 


