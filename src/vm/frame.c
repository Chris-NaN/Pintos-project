#include "vm/frame.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/palloc.h"
#include "filesys/file.h"
#include "threads/thread.h"
// #include "vm/page.h"

static struct list frame_list;

void frame_table_init(void)
{
	list_init(&frame_list);
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
			/* do something */

			PANIC(" Run out of frame ");
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




