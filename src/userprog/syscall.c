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

#include "vm/page.h"
#include "vm/frame.h"



static void syscall_handler (struct intr_frame *);
static int get_user (const uint8_t *uaddr);
static bool put_user (uint8_t *udst, uint8_t byte);
static struct file* getFile(struct thread* t, int fd);
void CloseFile(struct thread *t, int fd, bool All);
void is_writable_vaddr(const void *addr);
void is_mapped_vaddr(const void *addr);       /* check whether the user virtual address is valid mapped to the physical address*/
void is_vbuffer(const void *buffer, unsigned size, int syscall_num);  /* check the buffer byte by byte from head to tail */
void check_vaddr (const void *ptr);         /* check whether the address is valid user virtual address */
void check_each_byte(const void* pointer);  /* check four bytes related to a pointer */
void check_string(const void* pointer);  /* check the string byte by byte from head to tail */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
  lock_init(&filesys_lock);
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


  // printf("----------------------------------------------------- begin syscall with num [ %d ]\n",syscall_num);
  
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
      check_string(file);
      unsigned initial_size =  *((int*)f->esp+2);
      lock_acquire(&filesys_lock);
      f->eax = Sys_create(file,initial_size);
      lock_release(&filesys_lock);
      break;
    }
    case SYS_REMOVE:
    {
      const char* file = (char *) *((int*)f->esp+1);
      is_mapped_vaddr(file);
      lock_acquire(&filesys_lock);
      f->eax = Sys_remove(file);
      lock_release(&filesys_lock);
      
      break;
    }
    case SYS_OPEN:
    {
      const char* file = (char *)*((int*)f->esp+1);
      check_string(file);
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
      is_vbuffer(buffer,size,syscall_num);
      f->eax = Sys_read(fd, buffer, size);
      break;
    }
    case SYS_WRITE:
    {
      int fd = *((int*)f->esp+1);
      const void* buffer = (void *)*((int *)f->esp+2);
      unsigned size = *((int *)f->esp+3);
      is_vbuffer(buffer,size,syscall_num);
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
    case SYS_MMAP:
    {
      int fd = *((int*)f->esp+1);
      void *addr = (void *)*((int *)f->esp+2);
      /* can't mmap over the stack segment  */
      if (addr+PGSIZE>f->esp)
      {
        f->eax=-1;
        break;
      }
      f->eax = Sys_mmap(fd,addr);
    }
    case SYS_MUNMAP:
    {
      int mapid = *((int*)f->esp+1);
      Sys_munmap(mapid);
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
  return filesys_create(file,initial_size);
}

bool
Sys_remove(const char* file)
{
  return filesys_remove(file);
}

int
Sys_open(const char*file)
{
  
  // printf("-----------------------into sys_open ------------------\n");
 
  lock_acquire(&filesys_lock);
  struct file *f = filesys_open(file);
  lock_release(&filesys_lock);
  struct thread *t = thread_current();

  // printf("-----------------------into 2 ------------------\n");
  // printf("file name : %s\n",file);
  // printf("where is file : %u\n",(unsigned)f);
  

  // return -1 if open file fail
  if(!f)
  {
    return -1;
  }

  // printf("-----------------------into 3 ------------------\n");
  // add file to process file_list and change file discriptor
  struct file_node *node = malloc(sizeof(struct file_node));
  // have to do free operation later, or it will occur mem leak
  node->file = f;
  node->fd = t->fd;
  t->fd++;
  // push node into thread's file_list
  list_push_back(&t->file_list, &node->elem);


  // printf("-----------------------node-> fd***------------------%d\n",node->fd);


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

    lock_acquire(&filesys_lock);
    int bytes = file_read(f,buffer,length);
    lock_release(&filesys_lock);
    return bytes;
  }

}
void
Sys_seek(int fd, unsigned position)
{
  struct file *f = getFile(thread_current(),fd);
  if(!f)
    return;
  lock_acquire(&filesys_lock);
  file_seek(f,position);
  lock_release(&filesys_lock);
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
  lock_acquire(&filesys_lock);
  CloseFile(thread_current(), fd, false);
  lock_release(&filesys_lock);
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
  lock_acquire(&filesys_lock);
  bytes = file_write(f,buffer,size);
  lock_release(&filesys_lock);
  return bytes;

}

/* int here is equal to mapid_t */
int
Sys_mmap(int fd, void *addr)
{
  if (fd==0 || fd==1)
    return -1;
  if (!is_user_vaddr(addr) || addr < USER_VADDR_BASE || (unsigned int)addr % PGSIZE !=0 )
    return -1;

  struct file* tmpfile = getFile(thread_current(),fd);
  /* validity check ? */
  if (!tmpfile)
    return -1;
 
  lock_acquire(&filesys_lock);
  int filesize = file_length(tmpfile);
  struct file * file = file_reopen(tmpfile); 
  lock_release(&filesys_lock);

  if (filesize == 0)
  {
    return -1;
  }
  off_t ofs = 0;  
  size_t read_bytes = filesize;
  int mapid = ++thread_current()->mmap_counter;
  while (read_bytes > 0 ) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* check overlap mapping */
      if (get_spt_node(addr))
        return -1;



      struct spt_node* sptnode = malloc(sizeof(struct spt_node));
      if (!sptnode)
        return -1;

      sptnode->file = file;
      sptnode->upage = addr;
      sptnode->read_bytes = page_read_bytes;
      sptnode->zero_bytes = page_zero_bytes;
      sptnode->ofs = ofs;
      sptnode->writable=true;
      sptnode->mapid = mapid;
      sptnode->is_mmap = true;
      sptnode->disk_index =0;
      sptnode->locking = false;
      list_push_back(&thread_current()->spt,&sptnode->elem);


      /* Advance. */
      read_bytes -= page_read_bytes;
      addr += PGSIZE;
      ofs  += page_read_bytes;
    }


  // printf("-----finish---- mmap---- mapid %d\n" , mapid);


  return mapid;
}

void 
Sys_munmap(int mapid)
{
  struct thread * t = thread_current();
  struct list_elem *e = list_begin(&t->spt);
  while (e!=list_end(&t->spt))
  {
      struct list_elem *tmp = e;
      e=list_next(e);
      struct spt_node * sptnode = list_entry(tmp,struct spt_node, elem);
    
      if((sptnode->is_mmap)&&(sptnode->mapid==mapid)){
          sptnode->locking = true;
          if (sptnode->loaded)
          {
            if (pagedir_is_dirty(t->pagedir,sptnode->upage)&&(sptnode->file))
            {
              file_write_at(sptnode->file, sptnode->upage, sptnode->read_bytes, sptnode->ofs);
            }
            frame_remove(pagedir_get_page(t->pagedir, sptnode->upage));
            pagedir_clear_page(t->pagedir, sptnode->upage);
          }
          file_close(sptnode->file);
          list_remove(tmp);
          free(sptnode);
      }
    }
}


/* Reads a byte at user virtual address UADDR.
UADDR must be below PHYS_BASE.
Returns the byte value if successful, -1 if a segfault
occurred. */
static int
get_user (const uint8_t *uaddr)
{
int result;
asm ("movl $1f, %0; movzbl %1, %0; 1:"
: "=&a" (result) : "m" (*uaddr));
return result;
}


/* Writes BYTE to user address UDST.
UDST must be below PHYS_BASE.
Returns true if successful, false if a segfault occurred. */
static bool
put_user (uint8_t *udst, uint8_t byte)
{
int error_code;
asm ("movl $1f, %0; movb %b2, %1; 1:"
: "=&a" (error_code), "=m" (*udst) : "q" (byte));
return error_code != -1;
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
    if(get_user(addr)==-1)
      Err_exit(-1);
  }
}


void is_writable_vaddr(const void *addr)
{
 if(get_user(addr)==-1)
      Err_exit(-1);
if (!put_user ((uint8_t *)addr, get_user(addr)))
      Err_exit(-1);
}

void is_vbuffer(const void *buffer, unsigned size , int syscall_num)
{
  switch(syscall_num)
  {
    case SYS_READ:
    {
     char *buf = (char *) buffer;
     // is_writable_vaddr(buf);
     // printf("size is : %u\n",size);

      for(unsigned i=0;i<size;i++)
      {
        check_vaddr((const void *) buf);
        
        // printf("%s\n","--1");
        // printf("%u\n",(unsigned)buf);
        is_writable_vaddr(buf);
        // is_mapped_vaddr(buf);
        // printf("%s\n","--2");
        buf++;
      }
    }
    case SYS_WRITE:
    {
      char *buf = (char *) buffer;
      for(unsigned i=0;i<size;i++)
      {
        check_vaddr((const void *) buf);
        is_mapped_vaddr(buf);
        // is_writable_vaddr(buf,esp);
        buf++;
      }
    }
  }


}

void check_each_byte(const void* pointer)
{
  unsigned char* pt = (unsigned char*) pointer;
  for (int i=0;i<4;i++)
  {
    check_vaddr((const void*) pt);
    // is_mapped_vaddr((const void*) pt);
    // let the pointer go into page fault heandler
    pt++;
  }
}

void check_string(const void* pointer)
{
    char* pt = (char*) pointer;
  while(get_user(pointer)!=-1)
  {
    check_vaddr((const void*) pt);
    if (*pt == '\0')
      return;
    pt++;
  }
  Err_exit(-1);
  // char* pt = (char*) pointer;
  // while(pagedir_get_page(thread_current()->pagedir,(void *)pt))
  // {
  //   check_vaddr((const void*) pt);
  //   if (*pt == '\0')
  //     return;
  //   pt++;
  // }
  // Err_exit(-1);
}



