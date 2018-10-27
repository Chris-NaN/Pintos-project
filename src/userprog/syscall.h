#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H
#include <stdbool.h>
#include <debug.h>

typedef int pid_t;

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


#endif /* userprog/syscall.h */
