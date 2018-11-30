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
	swap_elem = NULL;
} 

void * frame_allocate(enum palloc_flags flags)
{
	/* what if the frame is not for user pool */

	/* Obtains a single free page and returns its kernel virtual address.
     If PAL_USER is set, the page is obtained from the user pool,*/

	if (flags == PAL_USER){
		void* kpage = palloc_get_page(PAL_USER);
		if (!kpage)
		{
			kpage = frame_swap(PAL_USER);
		}
		struct frame_node* fnode = malloc(sizeof(struct frame_node));
		fnode->kpage=kpage;
		fnode->thread=thread_current();
		list_push_back(&frame_list,&fnode->elem);

		return kpage;
	}
	return NULL;
	// if (!user_frame)
}

void frame_remove(void* kpage)
{	
	for (struct list_elem *e=list_begin(&frame_list);e!=list_end(&frame_list); 
	  e = list_next(e)){
		struct frame_node *fnode = list_entry(e, struct frame_node, elem);
    	if(fnode->kpage==kpage){
    		list_remove(e);
    		free(fnode);
    		palloc_free_page(kpage);
      	return;
    	}
	}
}

void * frame_swap(enum palloc_flags pallocflags)
{
	struct list_elem * e;
	if (swap_elem == NULL)
	{
		e = list_begin(&frame_list);
	}
	else
	{
		e = swap_elem;
	}
	while(true)
	{
		struct frame_node *fnode = list_entry(e, struct frame_node, elem);
		struct thread * t = fnode->thread;
	    struct spt_node * sptnode = fnode->sptnode;
	    if (pagedir_is_accessed(t->pagedir,sptnode->upage))
	    {
	      pagedir_set_accessed(t->pagedir,sptnode->upage, false);
	   	  e = list_next(e) == list_end(&frame_list)? list_begin(&frame_list): list_next(e);
	   	  continue;
	   	}
	   	if (sptnode->loaded){
	   		if (sptnode->is_mmap)
	   		{
	   			if (pagedir_is_dirty(t->pagedir,sptnode->upage))
	   			{
	   				file_write_at(sptnode->file, sptnode->upage, sptnode->read_bytes, sptnode->ofs);
	   			}
	   		}
	   		else
	   		{
	   			sptnode->disk_index = swap_into_disk(fnode->kpage);
	   		}
	   		sptnode->loaded = false;
	   		pagedir_clear_page(t->pagedir, sptnode->upage);
	   		palloc_free_page(fnode->kpage);
	   		
	   	}
		swap_elem = list_next(e) == list_end(&frame_list)? list_begin(&frame_list): list_next(e);
		list_remove(e);
    	free(fnode);
    	return palloc_get_page(pallocflags);
	}
}


