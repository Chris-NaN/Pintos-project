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
#include "threads/vaddr.h"
#include "userprog/pagedir.h"

#include "userprog/process.h"
#include "devices/shutdown.h"
#include "threads/synch.h"

#include "filesys/inode.h"
#include "filesys/directory.h"
#include <string.h>

static void syscall_handler (struct intr_frame *);
static struct file* getFile(struct thread* t, int fd);
void CloseFile(struct thread *t, int fd, bool All);
void is_mapped_vaddr(const void *addr);       /* check whether the user virtual address is valid mapped to the physical address*/
void is_vbuffer(const void *buffer, unsigned size);  /* check the buffer byte by byte from head to tail */
void check_vaddr (const void *ptr);         /* check whether the address is valid user virtual address */
void check_each_byte(const void* pointer);  /* check four bytes related to a pointer */
void check_string(const void* pointer);  /* check the string byte by byte from head to tail */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{ 
  // terminate process with exit code -1 if esp was setted to bad addr
  if(!f || !(f->esp)){  // check if ponter is NULL
    Err_exit(-1);
  }

  /* check validity of pointers and whether the pointer is valid mapped */
  check_each_byte((int *)f->esp);
  check_each_byte(((int *)f->esp)+1);
  check_each_byte(((int *)f->esp)+2);
  check_each_byte(((int *)f->esp)+3);

  int syscall_num = *(int *)f->esp;
  
  if(syscall_num <0 || syscall_num > 20 ){
    Err_exit(-1);  
  }
  switch(syscall_num)
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
      check_string(cmd_line);
      f->eax = Sys_exec(cmd_line);
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
      is_mapped_vaddr(file);
      unsigned initial_size =  *((int*)f->esp+2);
      f->eax = Sys_create(file,initial_size);
      break;
    }
    case SYS_REMOVE:
    {
      const char* file = (char *) *((int*)f->esp+1);
      is_mapped_vaddr(file);
      f->eax = Sys_remove(file);
      break;
    }
    case SYS_OPEN:
    {
      const char* file = (char *)*((int*)f->esp+1);
      is_mapped_vaddr(file);
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
      is_vbuffer(buffer,size);
      f->eax = Sys_read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd = *((int*)f->esp+1);
      const void* buffer = (void *)*((int *)f->esp+2);
      unsigned size = *((int *)f->esp+3);
      is_vbuffer(buffer,size);
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
    case SYS_CHDIR:
    {
      const char* dir = (char *)*((int*)f->esp+1);
      is_mapped_vaddr(dir);
      f->eax = Sys_chdir(dir);
      break;
    }
    case SYS_MKDIR:
    {
      const char* dir = (char *)*((int*)f->esp+1);
      is_mapped_vaddr(dir);
      f->eax = Sys_mkdir (dir);
      break;
    }
    case SYS_READDIR:
    {
      int fd = *((int*)f->esp+1);
      char* name = (char *)*((int*)f->esp+2);
      check_string(name);
      f->eax = Sys_readdir(fd, name);
      break;
    }
    case SYS_ISDIR:
    {
      int fd = *((int*)f->esp+1);
      f->eax = Sys_isdir(fd);
      break;
    }
    case SYS_INUMBER:
    {
      int fd = *((int*)f->esp+1);
      f->eax = Sys_inumber(fd);
      break;
    }
  }
}

void
Sys_halt()
{
  shutdown_power_off();
}

void
Sys_exit(int status)
{
  struct thread *t = thread_current();
  /* store process return status which will be printed when process return*/
  t->exit_code = status; 
  thread_exit();
}


/* the parent process cannot return from the exec until it knows whether the child
process successfully loaded its executable. */
pid_t 
Sys_exec(const char* cmd_line)
{
  if (!cmd_line)
  	return -1;
  pid_t pid = process_execute(cmd_line);
  return pid;
}

int
Sys_wait(pid_t pid)
{
  return process_wait(pid);
}

bool
Sys_create(const char* file, unsigned initial_size)
{
  if(!file)
    return false;
  return filesys_create(file,initial_size, false);
}

bool
Sys_remove(const char* file)
{
  return filesys_remove(file);
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
  
  if (inode_is_dir(file_get_inode(f))){
    node -> dir = (struct dir *)f;
    node -> isdir = true;
  }else{
    node -> file = f;
    node -> isdir = false;
  }

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
  if(fd==STDOUT_FILENO)
    Err_exit(-1);
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

unsigned 
Sys_tell (int fd)
{
  struct file *f = getFile(thread_current(),fd);
  if(!f)
    return -1;
  return file_tell(f);
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
    Err_exit(-1);   // return 0 if no bytes could be written at all
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

// have to call this func whit All == true when process exit
void CloseFile(struct thread *t, int fd, bool All)
{
  if(All){
    while(!list_empty(&t->file_list)){
      struct file_node *node = list_entry(list_pop_front(&t->file_list), struct file_node,
            elem);
      if (node -> isdir){
        dir_close(node->dir);
      }else{
        file_close(node->file);
      }
      list_remove(&node->elem);
      free(node);
    }
    return;
  }
  for (struct list_elem *e=list_begin(&t->file_list);e!=list_end(&t->file_list);
       e = list_next(e)){
    struct file_node *node = list_entry(e, struct file_node, elem);
    if(node->fd==fd){
      if (node -> isdir){
        dir_close(node->dir);
      }else{
        file_close(node->file);
      }
      list_remove(e);
      free(node);
      return;
    }

  }
}
void
Err_exit(int status)
{
  struct thread *t = thread_current();
  t->exit_code = status;
  thread_exit();
}

void check_vaddr (const void *ptr)
{
  if(!is_user_vaddr(ptr) || ptr < USER_VADDR_BASE)
    Err_exit(-1);
}

void is_mapped_vaddr(const void *addr)
{
  if (is_user_vaddr(addr))
  {
    void *ptr = pagedir_get_page(thread_current()->pagedir, addr);
    if(!ptr)
      Err_exit(-1);
  }
}

void is_vbuffer(const void *buffer, unsigned size)
{
  char *buf = (char *) buffer;
  for(unsigned i=0;i<size;i++)
  {
    check_vaddr((const void *) buf);
    is_mapped_vaddr(buf);
    buf++;
  }
}

void check_each_byte(const void* pointer)
{
  unsigned char* pt = (unsigned char*) pointer;
  for (int i=0;i<4;i++)
  {
    check_vaddr((const void*) pt);
    is_mapped_vaddr((const void*) pt);
    pt++;
  }
}

void check_string(const void* pointer)
{
  char* pt = (char*) pointer;
  while(pagedir_get_page(thread_current()->pagedir,(void *)pt))
  {
    check_vaddr((const void*) pt);
    if (*pt == '\0')
      return;
    pt++;
  }
  Err_exit(-1);
}

bool Sys_chdir (const char *dir)
{
  return dirsys_chdir(dir);
}

// filesys syscall
bool Sys_mkdir (const char *dir)
{
  return filesys_create(dir, 0, true);
}

// fd -> dir, store dir_name into name
bool Sys_readdir(int fd, char* name)
{
  // "." and ".." should not be return 
  if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)  return false;
  struct file_node *node = get_file_node(fd);
  if (!node || !node->isdir){
    return false;
  }
  if (!dir_readdir(node->dir, name))
  {
    return false;
  }
  return true;

}

bool Sys_isdir (int fd)
{
  struct file_node *node = get_file_node(fd);
  if (!node)
    {
      return -1;
    }
  return node->isdir;
}

int Sys_inumber (int fd)
{
  struct file_node *node = get_file_node(fd);
  if (!node)  return -1;

  block_sector_t inumber;
  if (node->isdir)
  {
    inumber = inode_get_inumber(dir_get_inode(node->dir));
  }
  else
  {
    inumber = inode_get_inumber(file_get_inode(node->file));
  }
  return inumber;
}

// given fd, return file_node of fd
// return NULL if not file_node is not found
struct file_node * get_file_node(int fd)
{
  struct thread *t = thread_current();
  for (struct list_elem *e=list_begin(&t->file_list);e!=list_end(&t->file_list);
       e = list_next(e)){
    struct file_node *node = list_entry(e, struct file_node, elem);
    if(node->fd==fd){
      return node;
    }
  }
  return NULL;
}


