#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include <stdio.h>
#include "threads/malloc.h"
#include "devices/input.h"

static void syscall_handler (struct intr_frame *);
static struct file* getFile(struct thread* t, int fd);
void CloseFile(struct thread *t, int fd, bool All);

struct file_node
{
  struct file *file;
  int fd;
  struct list_elem elem;
};

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  switch(*(int *)f->esp)
  {
    case SYS_HALT:
    {
      Sys_halt();
      break;
    }
    case SYS_EXIT:
    {
      
      int status = *((int*)f->esp+1);
      Sys_exit(status);
      break;
    }
    case SYS_EXEC:
    {
      const char* cmd_line = (char *)*((int*)f->esp+1);
      Sys_exec(cmd_line);
      break;
    }
    case SYS_WAIT:
    {
      pid_t pid =  *((int*)f->esp+1);
      f->eax = Sys_wait(pid);
      break;
    }
    case SYS_CREATE:
    {
      const char*file =(char *) *((int*)f->esp+1);
      unsigned initial_size =  *((int*)f->esp+2);
      f->eax = Sys_create(file,initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      const char* file = (char *) *((int*)f->esp+1);
      f->eax = Sys_remove(file);
      break;
    }
    case SYS_OPEN:
    {
      const char* file = (char *)*((int*)f->esp+1);
      f->eax = Sys_open(file);
      break;
    }
    case SYS_FILESIZE:
    {
      int fd =  *((int*)f->esp+1);
      f->eax = Sys_filesize(fd);
      break;
    }
    case SYS_READ:
    {
      int fd = *((int*)f->esp+1);
      void* buffer = (void *)*((int*)f->esp+2);
      unsigned size= *((int*)f->esp+3);
      f->eax = Sys_read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd = *((int*)f->esp+1);
      const void* buffer = (void *)*((int *)f->esp+2);
      unsigned size = *((int *)f->esp+3);
      f->eax = Sys_write(fd,buffer,size);
      break;
    }
    case SYS_SEEK:
    {
      int fd = *((int*)f->esp+1);
      unsigned position = *((int*)f->esp+2);
      Sys_seek(fd, position);
      break;
    }
    case SYS_TELL:
    {  
      int fd = *((int*)f->esp+1);
      f->eax = Sys_tell(fd);
      break;
    }
    case SYS_CLOSE:
    { 
      int fd = *((int*)f->esp+1);
      Sys_close(fd);
      break;
    }
  }
}

void
Sys_halt()
{
}
void
Sys_exit(int status)
{
  // If the process's parent waits for it,this is the status that will be returned
  // something to implement later

  struct thread *t = thread_current();
  /* store process return status which will be printed when process return*/
  t->exit_code = status; 
  thread_exit();
}
pid_t Sys_exec(const char* cmd_line)
{
  return 0;
}
int
Sys_wait(pid_t pid)
{
  return 0;
}
bool
Sys_create(const char* file, unsigned initial_size)
{
  return true;
}
bool
Sys_remove(const char* file)
{
  return true;
}
int
Sys_open(const char*file)
{
  struct file *f = filesys_open(file);
  struct thread *t = thread_current();
  // return -1 if open file fail
  if(!f)
    return -1;
  // add file to process file_list and change file discriptor
  struct file_node *node = malloc(sizeof(struct file_node));
  // have to do free operation later, or it will occur mem leak
  node->file = f;
  node->fd = t->fd;
  t->fd++;
  // push node into thread's file_list
  list_push_back(&t->file_list, &node->elem);
  return node->fd;  // return a nonnegative int called "file discriptor"
}
int
Sys_filesize(int fd)
{
  struct file *f = getFile(thread_current(),fd);
  if(!f)
    return -1;
  return file_length(f); 
}
int
Sys_read(int fd, void *buffer, unsigned length)
{
  ASSERT(fd!=1);  // fd!=1  !!
  uint8_t *buf = (uint8_t *)buffer;
  if(fd==STDIN_FILENO){  // read from STDIN
    for(unsigned i=0;i<length;i++){
      buf[i] = input_getc();
    }
    return length;
  }else{
    struct file *f = getFile(thread_current(),fd);
    if(!f)
      return -1;
    int bytes = file_read(f,buffer,length);
    return bytes;
  }

}
void
Sys_seek(int fd, unsigned position)
{
  struct file *f = getFile(thread_current(),fd);
  if(!f)
    return;
  file_seek(f,position);
}
unsigned Sys_tell(int fd)
{
  return 0;
}
void
Sys_close(int fd)
{
  CloseFile(thread_current(), fd, false);
}

int
Sys_write(int fd, const void *buffer, unsigned size)
{
  if(fd==STDOUT_FILENO){
    putbuf(buffer, size);
    return size;
  }
  struct thread *t = thread_current();
  struct file *f = getFile(t,fd);   // get the file of current process
  int bytes = 0;
  if(f==NULL){
    return bytes;   // return 0 if no bytes could be written at all
  }
  bytes = file_write(f,buffer,size);
  return bytes;

}
struct file* getFile(struct thread* t, int fd)
{
  struct list_elem *e;
  for (e = list_begin(&t->file_list);e!=list_end(&t->file_list);e=list_next(e)){
    struct file_node *node = list_entry(e,struct file_node, elem);
    if(node->fd==fd){
      return node->file;
    }
  }
  return NULL;
}

void CloseFile(struct thread *t, int fd, bool All)
{
  if(All){
    while(!list_empty(&t->file_list)){
      struct file_node *node = list_entry(list_pop_front(&t->file_list), struct file_node,
            elem);
      file_close(node->file);
      free(node);
    }
    return;
  }
  for (struct list_elem *e=list_begin(&t->file_list);e!=list_end(&t->file_list);
       e = list_next(e)){
    struct file_node *node = list_entry(e, struct file_node, elem);
    if(node->fd==fd){
      list_remove(e);
      file_close(node->file);
      free(node);
      return;
    }

  }
}
