#include "vm/swap.h"

struct bitmap* swap_table;
struct lock swap_lock;
struct block* swap_block;
#define FREE  0
#define NO_FREE  1

/* block size == number of sector */
/* block sector size == 512kb, we could write into block this size each time */
/* len(swap_table) == total size of block/ page size, which means how many number of swap entry we can get */
void swap_init(void)
{
    swap_block = block_get_role (BLOCK_SWAP);
    swap_table = bitmap_create(block_size(swap_block)*BLOCK_SECTOR_SIZE / PGSIZE);
    lock_init(&swap_lock);
    bitmap_set_all(swap_table, FREE);
}


/* this is page swap, which means we have to swap a page each call */
/* Every time, we could write 512kb into block, so we have to write 8 times to fullfill a page */
/* SECTORS_PER_PAGE = 8 */
size_t swap_into_disk(void* frame)
{
    // write to target section address in block
    size_t tar_addr = bitmap_scan_and_flip(swap_table, 0, 1, FREE);
    for (uint32_t i=0;i<SECTORS_PER_PAGE;i++){
        block_write(swap_block, tar_addr*SECTORS_PER_PAGE+i, (uint8_t*)frame+i*BLOCK_SECTOR_SIZE);
    }
    return tar_addr;
}

void get_from_disk(size_t idx, void* upage)
{
    bitmap_flip(swap_table, idx);

    for (size_t i = 0; i < SECTORS_PER_PAGE; i++)
    {
        block_read(swap_block, idx * SECTORS_PER_PAGE + i,
        (uint8_t *) upage + i * BLOCK_SECTOR_SIZE);
    }
}


