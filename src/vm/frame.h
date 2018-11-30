#ifndef VM_FRAME_H
#define VM_FRAME_H


#include <list.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/synch.h"
#include "vm/page.h"
/* the pointer for our swap algorithm */
struct frame_node *swap_pointer;
struct lock frame_list_lock;

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
void * frame_allocate(enum palloc_flags,struct spt_node * sptnode);
void frame_remove(void* kpage);

void * frame_swap(enum palloc_flags);

















#endif /* vm/frame.h */
