#include "vm/frame.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "filesys/file.h"
#include "userprog/pagedir.h"
#include "vm/swap.h"

static struct list frame_list;

void frame_table_init(void)
{
	list_init(&frame_list);
	lock_init(&frame_list_lock);
	swap_pointer = NULL;
} 

void * frame_allocate(enum palloc_flags flags,struct spt_node * sptnode)
{
	/* what if the frame is not for user pool */

	/* Obtains a single free page and returns its kernel virtual address.
     If PAL_USER is set, the page is obtained from the user pool,*/

	if (flags == PAL_USER){
		void* kpage = palloc_get_page(PAL_USER);

		if (!kpage)
			return frame_swap(PAL_USER,sptnode);
		// printf("%s\n","----------init---1--");
		struct frame_node* fnode = malloc(sizeof(struct frame_node));
		// printf("%s\n","----------init---1.5--");
		fnode->kpage=kpage;
		fnode->thread=thread_current();
		fnode->sptnode = sptnode;
		// printf("%s\n","----------init--2--");
		lock_acquire(&frame_list_lock);
		list_push_back(&frame_list,&fnode->elem);
		lock_release(&frame_list_lock);

		// printf("%s\n","----------init---3--");
		return kpage;
	}
	return NULL;
	// if (!user_frame)
}

void frame_remove(void* kpage)
{	
	lock_acquire(&frame_list_lock);
	for (struct list_elem *e=list_begin(&frame_list);e!=list_end(&frame_list); 
	  e = list_next(e)){
		struct frame_node *fnode = list_entry(e, struct frame_node, elem);
    	if(fnode->kpage==kpage){
    		list_remove(e);
    		free(fnode);
    		palloc_free_page(kpage);
    		lock_release(&frame_list_lock);
      		return;
    	}
	}
	lock_release(&frame_list_lock);
}

void * frame_swap(enum palloc_flags pallocflags,struct spt_node* newsptnode)
{
	lock_acquire(&frame_list_lock);
	struct list_elem * e;
	if (swap_pointer == NULL)
	{
		e = list_begin(&frame_list);
	}
	else
	{
		e = &swap_pointer->elem;
	}

	while(true)
	{

		// printf("%s\n","------------------1-----------------------");
		struct frame_node *fnode = list_entry(e, struct frame_node, elem);
		struct thread * t = fnode->thread;
	    struct spt_node * sptnode = fnode->sptnode;


	    if (sptnode->locking)
	    {

	    	// e = e == list_end(&frame_list)? list_begin(&frame_list): list_next(e);
	    
	    
	    e = list_next(e) == list_end(&frame_list)? list_begin(&frame_list): list_next(e);

	   	  continue;
	    }

	    if (pagedir_is_accessed(t->pagedir,sptnode->upage))
	    {
	    	// printf("%s\n","------------------2-----------------------");
	      pagedir_set_accessed(t->pagedir,sptnode->upage, false);
	      // printf("%s\n","------------------3-----------------------");
	   	  e = list_next(e) == list_end(&frame_list)? list_begin(&frame_list): list_next(e);
	   	
	   	  // printf("%s\n","------------------4-----------------------");
	   	  continue;
	   	}
	   	if (sptnode->loaded)
	   	{
	   		if (sptnode->is_mmap)
	   		{
	   			// printf("%s\n","------------------5-----------------------");
	   			if (pagedir_is_dirty(t->pagedir,sptnode->upage))
	   			{	
	   				file_write_at(sptnode->file, sptnode->upage, sptnode->read_bytes, sptnode->ofs);
	   			}
	   			// printf("%s\n","------------------6-----------------------");
	   		}
	   		else
	   		{
	   			// printf("%s\n","------------------7-----------------------");
	   			sptnode->disk_index = swap_into_disk(fnode->kpage);
	   			// printf("%s\n","------------------8-----------------------");
	   		}
	   		// printf("%s\n","------------------9-----------------------");
	   		sptnode->loaded = false;
	   		pagedir_clear_page(t->pagedir, sptnode->upage);
	   		palloc_free_page(fnode->kpage);
	   		
	   	}
	   
		struct list_elem* next_e = list_next(e) == list_end(&frame_list)? list_begin(&frame_list): list_next(e);
		swap_pointer = list_entry(next_e, struct frame_node, elem);
		
		fnode->kpage = palloc_get_page(pallocflags);
    	fnode->sptnode = newsptnode;
    	fnode->thread = thread_current();
    	lock_release(&frame_list_lock);
    	return fnode->kpage;
	}
}


