#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <debug.h>
#include "userprog/process.h"

#include "threads/synch.h"


typedef int pid_t;

struct file_node
{
  struct file *file;
  int fd;
  struct list_elem elem;
};

/* 12.1 */
struct lock filesys_lock;
/* 12.1 */

void syscall_init (void);

/* my code */
void Sys_halt(void);
void Sys_exit(int status);
pid_t Sys_exec(const char *file);
int Sys_wait(pid_t pid);
bool Sys_create(const char*file, unsigned initial_size);
bool Sys_remove(const char*file);
int Sys_open(const char *file);
int Sys_filesize(int fd);
int Sys_read(int fd, void *buffer, unsigned length);
int Sys_write(int fd, const void* buffer, unsigned length);
void Sys_seek(int fd, unsigned position);
unsigned Sys_tell(int fd);
void Sys_close(int fd);
int Sys_mmap(int fd, void *addr);
void Sys_munmap(int mapid);
void Err_exit(int status);



#endif /* userprog/syscall.h */
