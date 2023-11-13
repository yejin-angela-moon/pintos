#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include "filesys/file.h"

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

void check_user (void *ptr);

int get_user (const uint8_t *uaddr);

struct file_descriptor* process_get_fd(int fd);

int process_add_fd(struct file *file);

void process_remove_fd(int fd);




#endif /* userprog/syscall.h */
