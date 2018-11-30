#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <bitmap.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "threads/synch.h"

#define SECTORS_PER_PAGE (PGSIZE / BLOCK_SECTOR_SIZE)
// #define FREE  0
// #define NO_FREE  1

// struct bitmap* swap_table;
// struct lock swap_lock;
// struct block* swap_block;

void swap_init(void);
size_t swap_into_disk(void* frame);
void get_from_disk(size_t idx, void* upage);



#endif /* vm/swap.h */