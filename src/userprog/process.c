#include "stdlib.h"
#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/syscall.h"
#include "vm/frame.h"


static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);
static void setup_stack_populate (char *argv[MAX_ARGS], int argc, void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name)
{
  char *fn_copy;
  tid_t tid;

  /* Make a copy of FILE_NAME to obtain just the process name.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);

  char *fn_copy2;
  /* Make a copy of FILE_NAME to pass for the args.
     Otherwise there's a race between the caller and load(). */
  fn_copy2 = palloc_get_page (0);
  if (fn_copy2 == NULL)
    return TID_ERROR;
  strlcpy (fn_copy2, file_name, PGSIZE);

  /* Parse the argement strings */
  char *save_ptr;
  char *process_name = strtok_r(fn_copy, " ", &save_ptr);

  /* Check for strtok_r failure (could be ASSUME)*/
  if (process_name == NULL) {
    palloc_free_page(fn_copy);
    return TID_ERROR;
  }

  /* Prevent stack overflow. */
  if (strlen(file_name) - strlen(process_name) > 512) {
   return TID_ERROR;
  }

  /* Create a new thread to execute FILE_NAME. */
  tid = thread_create (process_name, PRI_DEFAULT, start_process, fn_copy2);
 // hash_init (&thread_current()->spt, spt_hash, spt_less, NULL);
  //hash_init (&get_thread_by_tid(tid)->spt, spt_hash, spt_less, NULL);
  
  /* Freeing process_name fn_copy */
  palloc_free_page(fn_copy);

  if (tid == TID_ERROR) {
    palloc_free_page (fn_copy2);
  } else {
    lock_acquire(&thread_current()->cp_manager.children_lock);
    struct child *child = malloc (sizeof(*child));
    if (child != NULL) {
      child->tid = tid;  
      child->waited = false;
      child->call_exit = false;
      child->exit_status = 0;
      get_thread_by_tid(tid)->parent_tid = thread_current()->tid;
      list_push_back(&thread_current()->cp_manager.children_list, &child->child_elem);
    }
    lock_release(&thread_current()->cp_manager.children_lock);
  }
  
  return tid;
}

/* Set up the stack. Push arguments from right to left. */
void setup_stack_populate (char *argv[MAX_ARGS], int argc, void **esp) {
  uint32_t argv_addresses[argc];
  *esp = PHYS_BASE;
  int length = 0;
 
  for (int i = argc - 1; i >= 0; i--) {
    int strlength = 0;
    if (strlen(argv[i]) >= PGSIZE) {
      strlength = PGSIZE;
      argv[i][PGSIZE] = '\0';
    } else {
      strlength = strlen(argv[i]);
    }
    length += strlength + 1;
    *esp = *esp - strlength - 1;
    memcpy(*esp, argv[i], strlength + 1);   
    argv_addresses[i] = (uint32_t) *esp;
  }
  /* Word-align the stack pointer */
  *esp -= ESP_DECREMENT - length % ESP_DECREMENT;

  /* Push a null pointer sentinel */
  *esp -= ESP_DECREMENT;
  *(uint32_t *) *esp = 0x0;

  /* Push the addresses of arguments */
  for (int i = argc - 1; i >= 0; i--) {
    *esp -= ESP_DECREMENT;
    * (uint32_t *) *esp = argv_addresses[i];
  }

  /* Push address of argv */
  void *argv0_addr = *esp;
  *esp -= ESP_DECREMENT;
  *(void **) *esp = argv0_addr;

  /* Push argc */
  *esp -= sizeof(int);
  *(int *) *esp = argc;

  /* Push fake return address */
  *esp -= ESP_DECREMENT;
  * (uint32_t *) *esp = 0x0;
}


/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  struct intr_frame if_;
  bool success;
  struct thread *cur = thread_current ();
 struct hash spt = cur->spt;
  //hash_init (&spt, spt_hash, spt_less, NULL);

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;

  /* Parse file name into arguments */
  char *token, *save_ptr;
  int argc = 0;
  char **argv = palloc_get_page(0);
  if (argv == NULL){
    thread_exit ();
  }
  char *process_name = strtok_r(file_name, " ", &save_ptr);
  argv[argc++] = process_name;

  /* Parse file_name and save arguments in argv */
  for (token = strtok_r (NULL, " ", &save_ptr); token != NULL; token = strtok_r(NULL, " ", &save_ptr)) {
    argv[argc++] = token;
     if (argc == MAX_ARGS - 1) {
       break;
     }
  }

  /* Terminate argv */
  argv[argc] = NULL;

  /* Load the actual process in the the thread */
  success = load (file_name, &if_.eip, &if_.esp);

  /* Communicate whether the load was successful to its parent. */
  int status = success ? 1 : -1;
  struct thread *parent = get_thread_by_tid(thread_current()->parent_tid);
  if (parent != NULL) {
    lock_acquire(&parent->cp_manager.children_lock);
    parent->cp_manager.load_result = status;
    cond_signal(&parent->cp_manager.children_cond, &parent->cp_manager.children_lock);
    lock_release(&parent->cp_manager.children_lock);
  }

  if (!success) {
    thread_exit ();
  }

  /* Set up the stack. Push arguments from right to left. */
  setup_stack_populate(argv, argc, &if_.esp);
  palloc_free_page (argv);
  palloc_free_page (file_name);  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. 
     s*/
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.
 * If it was terminated by the kernel (i.e. killed due to an exception),
 * returns -1.
 * If TID is invalid or if it was not a child of the calling process, or if
 * process_wait() has already been successfully called for the given TID,
 * returns -1 immediately, without waiting.
 *
 * This function will be implemented in task 2.
 * For now, it does nothing. */
int
process_wait(tid_t child_tid)
{
  /* Return TID_ERROR if the child_tid is invalid. */
  if (child_tid == TID_ERROR) {
    return TID_ERROR;
  }

  bool isChild = false;
  struct thread *cur = thread_current();
  struct list_elem *e;

  /* Check whether the thread with child_tid is a child of the current thread. */
  lock_acquire(&cur->cp_manager.children_lock);
  for (e = list_begin(&cur->cp_manager.children_list); e != list_end(&cur->cp_manager.children_list); e = list_next(e)) {
    if (list_entry(e, struct child, child_elem)->tid == child_tid) {
      isChild = true;
      break;
    }
  }
  lock_release(&cur->cp_manager.children_lock);

  /* Return TID_ERROR if it is not a child of the current thread. */
  if (!isChild) {
    return TID_ERROR;
  } else {
    struct child *child = list_entry(e, struct child, child_elem);
    
    /* Return TID_ERROR if process_wait has been called on the thread. */
    if (child->waited) {
      return TID_ERROR;
    }
    child->waited = true;
   
    lock_acquire(&cur->cp_manager.children_lock);
    
    /* Wait until the thread exit. */
    while (get_thread_by_tid (child_tid) != NULL) {
       cond_wait (&cur->cp_manager.children_cond, &cur->cp_manager.children_lock);
    }

    /* Return the exit status if the thread was exited by exit(). Othereise, return TID_ERROR if the thread was terminated by the kernel.*/
    int status = 0;
    child = list_entry(e, struct child, child_elem);
    if (child->call_exit) {
      status = child->exit_status;
    } else {
      status =  TID_ERROR;
    }
    
    lock_release(&cur->cp_manager.children_lock);
    return status;  
  }
}

/* Free the mmap files. */
/*void free_mmap (struct map_file * mf) {
  if (mf == NULL)
    return;  // not found
  void *uaddr = mf->addr;
  // TODO remove page from list of virtual pages
  for (int count = 0 ; count < mf->page_no; count++) {

    struct spt_entry *page =  spt_find_page(&thread_current()->spt, uaddr);
    if (page->in_memory && pagedir_is_dirty (thread_current()->pagedir, page->user_vaddr)) {
      lock_acquire(&syscall_lock);
      file_seek(page->file, page->ofs);
      file_write(page->file, page->user_vaddr, page->read_bytes);
      lock_release(&syscall_lock);
    }
    uaddr += PGSIZE;

    free(page);
  }
  free(mf);

}*/

/* Close the file and free the file_descriptor. */
/*void free_fd(struct hash_elem *e, void *aux UNUSED) {
  struct file_descriptor *fd = hash_entry(e, struct file_descriptor, elem);
  file_close(fd->file);
  free(fd);
}
*/
static void free_spte (struct hash_elem *e, void *aux UNUSED)
{
  struct spt_entry *spte = hash_entry (e, struct spt_entry, elem);
  free (spte);
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the fd_table with free_fd. */
  hash_destroy(&cur->fd_table, free_fd);

  struct list_elem *me = list_begin(&cur->mmap_files);
  struct list_elem *nme;

  lock_acquire(&cur->mf_lock);
  while (me != list_end(&cur->mmap_files)){
    nme = list_next(me);
    struct map_file *mmap = list_entry (me, struct map_file, elem);
    list_remove(me);
    free_mmap (mmap);
    me = nme;
  }
  lock_release(&cur->mf_lock);
  

//  list_remove(&cur->pd_elem);
  hash_destroy(&cur->spt, free_spte);

  /* Destroy the fd_table with free_fd. */
  //hash_destroy(&cur->fd_table, free_fd);

  /* Free all the locks that current thread hold. */
  for (struct list_elem *e = list_begin(&cur->locks); e != list_end(&cur->locks);
          e = list_next(e)){
    lock_release(list_entry(e, struct lock, lock_elem));
  }

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL)
  {
    /* Correct ordering here is crucial.  We must set
       cur->pagedir to NULL before switching page directories,
       so that a timer interrupt can't switch back to the
       process page directory.  We must activate the base page
       directory before destroying the process's page
       directory, or our active page directory will be one
       that's been freed (and cleared). */
    cur->pagedir = NULL;
    pagedir_activate (NULL);
    pagedir_destroy (pd);
  } 
  
  /* Remove the child_elem from the children_list and free all the child.  */
  while (!list_empty(&cur->cp_manager.children_list)) {
    struct list_elem *e = list_pop_front(&cur->cp_manager.children_list);
    struct child *child = list_entry (e, struct child, child_elem);
    free(child);
  }

  /* Communicate the thread is about to exit to its parent. */
  struct thread *parent = get_thread_by_tid (cur->parent_tid);
  if (parent != NULL) {
    lock_acquire (&parent->cp_manager.children_lock);
    cond_broadcast (&parent->cp_manager.children_cond, &parent->cp_manager.children_lock);
    lock_release (&parent->cp_manager.children_lock);
  }  
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
{
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
{
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
static bool load_segment_lazily (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp)
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL)
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL)
  {
    printf ("load: %s: open failed\n", file_name);
    goto done;
  }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024)
  {
    printf ("load: %s: error loading executable\n", file_name);
    goto done;
  }
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++)
  {
    struct Elf32_Phdr phdr;

    if (file_ofs < 0 || file_ofs > file_length (file))
      goto done;
    file_seek (file, file_ofs);

    if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
      goto done;
    file_ofs += sizeof phdr;
    switch (phdr.p_type)
    {
      case PT_NULL:
      case PT_NOTE:
      case PT_PHDR:
      case PT_STACK:
      default:
        /* Ignore this segment. */
        break;
      case PT_DYNAMIC:
      case PT_INTERP:
      case PT_SHLIB:
        goto done;
      case PT_LOAD:
        if (validate_segment (&phdr, file))
        {
          bool writable = (phdr.p_flags & PF_W) != 0;
          uint32_t file_page = phdr.p_offset & ~PGMASK;
          uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
          uint32_t page_offset = phdr.p_vaddr & PGMASK;
          uint32_t read_bytes, zero_bytes;
          if (phdr.p_filesz > 0)
          {
            /* Normal segment.
               Read initial part from disk and zero the rest. */
            read_bytes = page_offset + phdr.p_filesz;
            zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                          - read_bytes);
          }
          else
          {
            /* Entirely zero.
               Don't read anything from disk. */
            read_bytes = 0;
            zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
          }
          if (!load_segment_lazily (file, file_page, (void *) mem_page,
                             read_bytes, zero_bytes, writable))
            goto done;
        }
        else
          goto done;
        break;
    }
  }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;

  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

  done:
  /* We arrive here whether the load is successful or not. */
//  file_close (file);  //TODO need to close the file somewhere
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file)
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK))
    return false;

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file))
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz)
    return false;

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;

  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

// static void *allocate_user_frame(void) {
//   void *frame = allocate_frame();
//   if (frame == NULL) {
//     process_exit();
//   }
//   return frame;
// }

static bool
load_segment_lazily (struct file *file, off_t ofs, uint8_t *upage,
    uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  while (read_bytes > 0 || zero_bytes > 0) {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
//printf("ready to insert one new page in lazy load for tid %d\n", thread_current()->tid);
    if (!thread_current()->init_spt) {
//	    printf("need to be init\n");
      hash_init (&thread_current()->spt, spt_hash, spt_less, NULL);
      thread_current()->init_spt = true;
    }
    if (!spt_insert_file (file, ofs, upage, page_read_bytes,
                          page_zero_bytes, writable)) {
    //  return false;
    }

  //  printf("insert spt file");
    /* Advance */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    ofs += page_read_bytes;
    upage += PGSIZE;
  }
  //printf("end lazy while loop and file length %d\n", file_length(file));
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - RE:q
AD_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

//printf("the addr load is %d\n", (uint32_t) upage);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0)
  {
    /* Calculate how to fill this page.
       We will read PAGE_READ_BYTES bytes from FILE
       and zero the final PAGE_ZERO_BYTES bytes. */
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;

    /* Check if virtual page already allocated */
    struct thread *t = thread_current ();

    uint8_t *kpage = pagedir_get_page (t->pagedir, upage);

    if (kpage == NULL){

      /* Get a new page of memory. */
      kpage = allocate_frame();
      if (kpage == NULL){
        return false;
      }

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable))
      {
        deallocate_frame (kpage);
        return false;
      }

    } else {

      /* Check if writable flag for the page should be updated */
      if(writable && !pagedir_is_writable(t->pagedir, upage)){
        pagedir_set_writable(t->pagedir, upage, writable);
      }

    }
//printf("kpage pointer: %p\n", (void *) kpage);
    /* Load data into the page. */
    if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes){
    //  deallocate_frame(kpage);
      return false;
    }
    memset (kpage + page_read_bytes, 0, page_zero_bytes);

    /* Advance. */
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
    upage += PGSIZE;
  }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp)
{
  uint8_t *kpage;
  bool success = false;

  kpage = allocate_frame();
  if (kpage != NULL)
  {
    success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
    if (success)
      *esp = PHYS_BASE;
    else
      deallocate_frame (kpage);
  }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable));
}



