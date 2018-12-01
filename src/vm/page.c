#include "vm/page.h"
#include <debug.h>
#include <round.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "vm/frame.h"
#include "vm/swap.h"
#include "filesys/file.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "userprog/pagedir.h"

struct spt_node* get_spt_node(void* upage)
{
	struct list_elem *e;
	struct thread * t = thread_current();
	uint8_t * upagehead = pg_round_down(upage);


	for (e = list_begin(&t->spt);e!=list_end(&t->spt);e=list_next(e)){
    	struct spt_node * sptnode = list_entry(e,struct spt_node, elem);
    	if(sptnode->upage==upagehead){
      		return sptnode;
    	}
  	}
  	return NULL;
}

bool load_page(struct spt_node * sptnode)
{
	/* Get a page of memory. */
    uint8_t *kpage = frame_allocate(PAL_USER,sptnode);
    
          /* swap */
    if (sptnode->disk_index)
    {
      get_from_disk(sptnode->disk_index,kpage);
    }
    else 
    {      
      /* Load this page, starting at offset FILE_OFS in the file.*/
      if (file_read_at(sptnode->file, kpage, sptnode->read_bytes, sptnode->ofs) != (int)sptnode->read_bytes)
      {
      // printf("-----------3.2-----");
        frame_remove(kpage);
        return false; 
      }
      memset (kpage + sptnode->read_bytes, 0, sptnode->zero_bytes);
    }
      /* Add the page to the process's address space. */
    if (!install_page (sptnode->upage, kpage, sptnode->writable)) 
    {
     // printf("-----------3.4-----");
        frame_remove(kpage);
        return false; 
    }
    sptnode->loaded = true;

    return true;
}

void spt_destory(struct thread *t)
{

  while(!list_empty(&t->spt))
      {
        struct spt_node * sptnode = list_entry(list_pop_front(&t->spt), struct spt_node, elem);
      
      if(sptnode->is_mmap)
      {
          if (pagedir_is_dirty(t->pagedir,sptnode->upage))
          {
            file_write_at(sptnode->file, sptnode->upage, sptnode->read_bytes, sptnode->ofs);
          }
      }
      if (sptnode->loaded)
      {
        frame_remove(pagedir_get_page(t->pagedir, sptnode->upage));
        pagedir_clear_page(t->pagedir, sptnode->upage);
      }
      free(sptnode);
      }

}


bool grow_stack (void *user_vaddr)
{
    // check if exceed limit of stack
    if (PHYS_BASE - pg_round_down(user_vaddr) > MAX_STACK_SIZE){
        return false;
    }
    struct spt_node *sptnode = malloc(sizeof(struct spt_node));
    // store some info into new page node
    if (!sptnode){
        return false;
    }
    // no complete

    sptnode->disk_index = 0;
    sptnode->upage = pg_round_down(user_vaddr);
    sptnode->writable = true;
    sptnode->locking = true;
    sptnode->is_mmap = false;
    uint8_t* new_frame = frame_allocate(PAL_USER,sptnode);
    if(new_frame==NULL){
        free(sptnode);
        return false;
    }
    if (!install_page (sptnode->upage, new_frame, sptnode->writable)) 
    {
        free(sptnode);
        frame_remove(new_frame);
        return false; 
    }

    list_push_back(&thread_current()->spt,&sptnode->elem);
    sptnode->locking = false;
    return true;
    
    // have to deal with syscall and page fault pointer event
}
// page.h  exception.c  syscall.c