#ifndef VM_PAGE_H
#define VM_PAGE_H


#include <stdbool.h>
#include <stddef.h>
#include <list.h>
#include "threads/thread.h"
#include "filesys/file.h"
#include "filesys/off_t.h"

#define MAX_STACK_SIZE (1<<24)

struct spt_node
{

	uint8_t * upage; 
	struct file * file;
	off_t ofs;  /* offset */
	size_t read_bytes;
	size_t zero_bytes;
	bool writable;

	bool is_mmap;
	int mapid;


	bool locking;

	bool loaded; 

	size_t disk_index;
	

	struct list_elem elem;
};


struct spt_node* get_spt_node(void* user_vaddr);

bool load_page(struct spt_node * sptnode);

void spt_destory(struct thread *t);

bool grow_stack (void *user_vaddr);

#endif /* vm/page.h */
