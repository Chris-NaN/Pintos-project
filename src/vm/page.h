#ifndef VM_PAGE_H
#define VM_PAGE_H

#include "filesys/file.h"
#include "filesys/off_t.h"
#include <stdbool.h>
#include <stddef.h>
#include <list.h>

// limit max stack size to 512kb, 128page/stack
#define MAX_STACK_SIZE (1 << 24)

struct spt_node
{

	uint8_t * upage; 

	struct file * file;

	off_t ofs;  /* offset */

	size_t read_bytes;
	size_t zero_bytes;
	bool writable;
	struct list_elem elem;
};


struct spt_node* get_spt_node(void* user_vaddr);

bool spt_add_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable);

bool load_page_from_file(struct spt_node * sptnode);

bool grow_stack (void *user_vaddr);

#endif /* vm/page.h */
