#ifndef BUFFER_CACHE_H
#define BUFFER_CACHE_H

#include <list.h>
#include <devices/block.h>
#include <threads/synch.h>

// limit buffer cache size to 64 sectors
#define BUFFER_CACHE_SIZE 64
#define WRITE_BACK_INTERVAL 5*TIMER_FREQ

// buffer cache
struct list buffer_cache;
// syn lock, only one thread run in FS code at once
struct lock cache_lock;
// number of cache_entry
uint32_t cache_num;
// clock pointer, used for clock evict alg
struct list_elem* clock_pointer;


// cache sector list entry
struct cache_entry{

    uint8_t block[BLOCK_SECTOR_SIZE];  // block data in each entry
    block_sector_t sector;            // Index of a block device sector
    bool dirty;                       // dirty bit, 1: dirty entry, need to write back
    bool is_accessed;                 // access bit, 0: not access and 1 is oppo
    bool loaded;                      // if cache entry have loaded data from sector


    int open_cnt;                     // count how many processes open this entry, syn problem
    // some thing more
    struct list_elem elem;
};
// cache operation: init, allocate, get, evict, ...(more)
void buffer_cache_init(void);
// init cache entrym, used in 'not full' situation
struct cache_entry* init_cache_entry(block_sector_t sector);

struct cache_entry* get_cache_entry(block_sector_t sector);
struct cache_entry* evict_cache_entry(void);
// struct cache_entry* cache_read(block_sector_t sector, int )
struct cache_entry* read_cache_entry(block_sector_t sector);
void write_cache_entry(block_sector_t sector, void* buffer, int write_bytes);

void write_all_cache_back(void);
void timer_func(void*);




#endif
