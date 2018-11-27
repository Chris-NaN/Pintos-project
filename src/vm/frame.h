#ifndef VM_FRAME_H
#define VM_FRAME_H


#include <list.h>
#include "threads/thread.h"
#include "vm/page.h"
#include "threads/palloc.h"


struct frame_node
{
	
  struct spt_node * sptnode;

  /* page pointer to the kernel virtual address for the user*/
  /* kpage ? */ 
  void * kpage;

  struct thread* thread;

  struct list_elem elem;
};



void frame_table_init(void);
void * frame_allocate(enum palloc_flags);
void frame_remove(void* kpage);



















#endif /* vm/frame.h */
