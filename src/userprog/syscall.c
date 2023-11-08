#include "userprog/syscall.h"
#include "userprog/process.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

void
syscall_init(void) {
    intr_register_int(0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler(struct intr_frame *f) {
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
            int status = *((int *) (f->esp + 4));
            exit(status);
            break;
        }
        case SYS_EXEC: { /**/
            const char *cmd_line = *((const char **) (f->esp + 4));
            f->eax = (uint32_t) exec(cmd_line);
            break;
        }
        case SYS_WAIT: {
            pid_t pid = *((pid_t * )(f->esp + 4));
            f->eax = (uint32_t) wait(pid);
            break;
        }
        case SYS_CREATE: { /* Create a file. */
            const char *file = *((const char **) (f->esp + 4));
            unsigned initial_size = *((unsigned *) (f->esp + 8));
            f->eax = (uint32_t) create(file, initial_size);
            break;
        }
        case SYS_REMOVE: { /* Delete a file. */
            const char *file = *((const char **) (f->esp + 4));
            f->eax = (uint32_t) remove(file);
            break;
        }
        case SYS_OPEN: {  /* Open a file. */
            const char *file = *((const char **) (f->esp + 4));
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
            void *buffer = *((void **) (f->esp + 8));
            unsigned size = *((unsigned *) (f->esp + 12));
            f->eax = (uint32_t) read(fd, buffer, size);
            break;
        }
        case SYS_WRITE: { /* Write to a file. */
            int fd = *((int *) (f->esp + 4));
            const void *buffer = *((const void **) (f->esp + 8));
            unsigned size = *((unsigned *) (f->esp + 12));
            f->eax = (uint32_t) write(fd, buffer, size);
            break;
        }
        case SYS_SEEK: { /* Change position in a file. */
            int fd = *((int *) (f->esp + 4));
            unsigned position = *((unsigned *) (f->esp + 8));
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
exec(const char *cmd_line) {
    //TODO
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
        return process_add_file(f);
    }
}

int
filesize(int fd) {
    //TODO
}

int
read(int fd, void *buffer, unsigned size) {
    //TODO
}

int
write(int fd, const void *buffer, unsigned size) {
    if (fd == 1) {  // writes to console
        for (int j; j < size; j += 200)  // max 200B at a time
            putbuf(buffer + j, min(200 + j, size);
        return size;
    }
    int i;
    for (i = 0; i < size; i++) {
        if (!put_user(fd + i, buffer + i))
            break;
    }
    return i;

}

void
seek(int fd, unsigned position) {
    //TODO
}

unsigned
tell(int fd) {
    //TODO
}

void
close(int fd) {
    //TODO
}