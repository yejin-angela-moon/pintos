#include "userprog/exception.h"
#include "userprog/pagedir.h"
#include <inttypes.h>
#include <stdio.h>
#include "threads/loader.h"
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "userprog/syscall.h"
#include "vm/page.h"
#include "threads/vaddr.h"
#include "vm/swap.h"
#include "vm/frame.h"
#include <string.h>
#include "filesys/file.h"
#include <stdlib.h>
#include <threads/malloc.h>
#include <threads/palloc.h>

/* Number of page faults processed. */
static long long page_fault_cnt;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

static void load_page_from_swap(struct spt_entry *spte, void *frame);
static void load_page_from_file(struct spt_entry *spte, void *frame);

bool install_page(void *upage, void *kpage, bool writable);
static bool stack_valid(void *vaddr, void *esp);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill, "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill, "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill, "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected. Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  
         Shouldn't happen.  Panic the kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      PANIC ("Kernel bug - this shouldn't be possible!");
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to task 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present; /* True: not-present page, False: writing to read-only page. */
  bool write;       /* True: access was write, False: access was read. */
  bool user;        /* True: access by user, False: access by kernel. */

  void *fault_addr;  /* Fault address. */
  struct spt_entry *spte;

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* To implement virtual memory, delete the rest of the function
     body, and replace it with code that brings in the page to
     which fault_addr refers. */

//printf("PAGE FAULT\n\n");

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;

  struct thread *cur = thread_current();
  void * fault_page = (void *) pg_round_down (fault_addr);

  if (!not_present) { 
    exit(-1);
  }
 
  if (fault_addr == NULL || !is_user_vaddr(fault_addr) || fault_addr < 0x08048000){ 
    exit(-1);
  }

  lock_acquire(&cur->spt_lock);
  spte = spt_find_page(&cur->spt, fault_page);
  lock_release(&cur->spt_lock);

  //stack growth code:
  void *esp = user ? f->esp : cur->esp; 
  if (spte == NULL && stack_valid(fault_addr, esp)) {
     if (!user && fault_addr < PHYS_BASE){
        exit(-1);
     }

    void *kpage = allocate_frame();
    if (kpage == NULL) {
      deallocate_frame (kpage);
      exit(-1);
    }

    if (kpage != NULL && !pagedir_set_page (cur->pagedir, fault_page, kpage, true)) {
      deallocate_frame (kpage); 
      exit(-1);
    }
    return;
  }

  if (spte == NULL) { 
    exit(-1);
  }

 if (!spte->in_memory) {
   if (spte->file != NULL) {

        struct thread *cur = thread_current();

        struct shared_page *found_shared_page = NULL;
        
        if (!spte->writable) {
//          found_shared_page = get_shared_page(spte);//hash_entry(found_elem, struct shared_page, elem);
        }
        

        uint8_t *kpage = NULL;
        if (found_shared_page != NULL) {
            // If page is shared and read-only, use existing kpage.
         
	    //printf("share pageee with detail file %p, uservaddr %p\n", spte->user_vaddr, spte->file);

            spte->frame_page = share_page(spte->user_vaddr, spte->file);
	    get_frame_by_kpage (spte->frame_page)->tid = thread_current()->tid;
                  
            spte->in_memory = true;

        } else {
            // Allocate a new frame if page not shared or writable.
            kpage = pagedir_get_page(cur->pagedir, spte->user_vaddr);
            if (kpage == NULL) {
               // deallocate_frame (kpage);
                kpage = allocate_frame();
                if (kpage == NULL) {
                    deallocate_frame (kpage);
                    exit(-1); 
                }
            }

	    switch (spte->type) {
               case File:
               case Mmap:
               case (Mmap | Swap):
                 load_page (spte, kpage);
                 break;
               case (File | Swap):
	       case Swap:
                 load_page_swap (spte, kpage);
                 break;
               default:
                 break;
            }
             spte->in_memory = true;

            // If page is read-only, consider sharing it.
            if (!spte->writable) {
                // Here you can either use the share_page function or write the logic directly.
                // Ensure to update the shared_page struct with kpage and pagedir.
             //  create_shared_page (spte, kpage);
            }
        }
    } else if (spte->swap_slot != INVALID_SWAP_SLOT) {
//	   printf("load page fromswap\n"); 
   //   load_page_from_swap(spte, frame);
    } else {
      /* Page is an all-zero page. */
     // memset(frame, 0, PGSIZE);
    }
      spte->in_memory = true;
  }

  //if (!install_page((void *) spte->user_vaddr, frame, spte->writable)) {
  // exit(-1);
 // }

  /* (3.1.5) a page fault in the kernel merely sets eax to 0xffffffff
  * and copies its former value into eip. see syscall.c:get_user() */
  else if(!user) { // kernel mode
    f->eip = (void *) f->eax;
    f->eax = 0xffffffff;
    return;
  } else {

  /* Page fault can't be handled - kill the process */
  printf ("Page fault at %p: %s error %s page in %s context.\n",
          fault_addr,
          not_present ? "not present" : "rights violation",
          write ? "writing" : "reading",
          user ? "user" : "kernel");
  kill (f);
  }
  return;

}

static void load_page_from_file (struct spt_entry *spte, void *frame) {
//  off_t bytes_read = file_read_at(spte->file, frame, spte->read_bytes, spte->ofs);
 // if (bytes_read != (off_t) spte->read_bytes) {
  //  exit(-1);
 // }

  if (spte->zero_bytes > 0) {
    memset(frame + spte->read_bytes, 0, spte->zero_bytes);
  }
}

static void load_page_from_swap(struct spt_entry *spte, void *frame) {
  //swap_read(spte->swap_slot, frame);
}

bool install_page(void *upage, void *kpage, bool writable) {
  struct thread *cur = thread_current();
  struct spt_entry *spte = spt_find_page(&cur->spt, upage);

  if (spte == NULL)
    return false;

  if (pagedir_get_page(cur->pagedir, upage) != NULL)
    return false;

  if (!pagedir_set_page(cur->pagedir, upage, kpage, writable))
    return false;

 // spte->frame = kpage;

  return true;
}

static bool stack_valid(void *vaddr, void *esp){
//  return  (PHYS_BASE - pg_round_down(vaddr) <= MAX_STACK_SIZE) && (vaddr >= esp - PUSHA_SIZE); 
  return  (PHYS_BASE - pg_round_down(vaddr) <= MAX_STACK_SIZE) && (vaddr == esp - 32 || vaddr == esp - 4 || vaddr >= esp); 

}

