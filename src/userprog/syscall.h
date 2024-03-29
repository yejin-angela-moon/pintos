#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <stdint.h>
#include <vm/page.h>


#define FAIL -1
#define MAX_CONSOLE_WRITE 200
#define FIRST_FD_NUMBER 2

typedef int pid_t;

void syscall_init (void);

void halt(void);

void exit(int status);

pid_t exec(const char *cmd_line);

int wait(pid_t pid);

bool create(const char *file, unsigned initial_size);

bool remove(const char *file);

int open(const char *file);

int filesize(int fd);

int read(int fd, void *buffer, unsigned size);

int write(int fd, const void *buffer, unsigned size);

void seek(int fd, unsigned position);

unsigned tell(int fd);

void close(int fd); 

mapid_t mmap(int fd, void *addr);

void munmap(mapid_t mapping);

void free_mmap (struct map_file * mf);

void free_fd(struct hash_elem *e, void *aux UNUSED);

bool hash_less(int a, int b);

struct file_descriptor *process_get_fd(int fd);


void process_remove_fd(int fd);

#endif /* userprog/syscall.h */
