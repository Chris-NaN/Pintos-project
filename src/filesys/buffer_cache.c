#include "filesys/buffer_cache.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "devices/timer.h"
#include "filesys/filesys.h"
#include <string.h>

void buffer_cache_init()
{
    list_init(&buffer_cache);
    lock_init(&cache_lock);
    cache_num = 0;   // initial number of cache entry = 0
    // write all cache entry which is dirty back to block
    thread_create("write back timer", PRI_MAX - 1, timer_func, NULL );
}

struct cache_entry* init_cache_entry(block_sector_t sector)
{
    struct cache_entry* entry = NULL;
    entry = malloc(sizeof(struct cache_entry));
    entry->sector = sector;
    entry->dirty = false;
    entry->is_accessed = true; // get_cache_entry call this func, so it's accessed
    entry->loaded = false;  
    // track the cache entry. When table is full, the clock pointer points to '12 clock'
    clock_pointer = &entry -> elem; 
    return entry; 
}

struct cache_entry* get_cache_entry(block_sector_t sector)
{
    
    struct cache_entry* entry = NULL;
    struct list_elem* e;
    // find cache entry in list
    for (e = list_begin(&buffer_cache); e != list_end(&buffer_cache);
       e = list_next(e)){
        entry = list_entry(e, struct cache_entry, elem);
        //entry->loaded = false;
        if (entry->sector == sector) return entry;
    }
    // if not find, go to next two situation: not full(push) or full(evict)
    // not full, malloc a cache entry
    
    if (cache_num < BUFFER_CACHE_SIZE){
        entry = init_cache_entry(sector);
        list_push_back(&buffer_cache, &entry->elem);

        lock_acquire(&cache_lock);
        cache_num++;
        lock_release(&cache_lock);
        return entry;
    }

    // cache table is full, evict an entry for the sector
    entry = evict_cache_entry();
    // change the entry info
    // entry -> sector = sector;
    // entry -> is_accessed = true;
    // block_read(fs_device, entry->sector, &entry->block);
    return entry;
}

struct cache_entry* evict_cache_entry()
{
    struct cache_entry* entry = NULL;
    // find an entry which is not accessed recently
    
    while (true){
        entry = list_entry(clock_pointer, struct cache_entry, elem);
        // no matter what happen, clock pointer will point to the next clock
        clock_pointer = list_next(clock_pointer) == list_end(&buffer_cache)? list_begin(&buffer_cache): list_next(clock_pointer);
        // entry is accessed recently
        if (entry->is_accessed == true){
            lock_acquire(&cache_lock);
            entry->is_accessed = false;
            lock_release(&cache_lock);
            // give the entry second chance
            continue;
        }
        if (entry -> open_cnt > 0){    
            continue;
        }
        // entry could be evict
        if (entry->dirty == true){
            // write back if dirty
            block_write(fs_device, entry->sector, entry->block);
            entry-> dirty = false;
        }
        
        break;
    }
    
    //memset (entry->block, 0, BLOCK_SECTOR_SIZE);
    entry -> loaded = false;  // have to reload data after origin is evicted
    return entry;   
}

struct cache_entry* read_cache_entry(block_sector_t sector)
{
    struct cache_entry* entry = get_cache_entry(sector);
    // get_cache_entry is just return entry, do not produce any data
    // so we have to read block sector data to entry->block 
    if (entry->loaded == false){
        // load data
        block_read (fs_device, sector, entry->block);
        entry->loaded = true;
    }
    
    //block_read (fs_device, sector, entry->block);
    lock_acquire(&cache_lock);
    entry -> open_cnt++;
    entry -> sector = sector;
    lock_release(&cache_lock);
    entry -> is_accessed = true;

    lock_acquire(&cache_lock);
    entry -> open_cnt--;
    lock_release(&cache_lock);
    return entry;
}

void write_cache_entry(block_sector_t sector, void* buffer, int write_bytes)
{
    struct cache_entry* entry = get_cache_entry(sector);
    
    if (entry->loaded == false){
        // load data
        block_read (fs_device, sector, entry->block);
        entry->loaded = true;
    }
    

    lock_acquire(&cache_lock);
    entry -> open_cnt++;
    entry -> sector = sector;
    lock_release(&cache_lock);
    entry -> is_accessed = true;

    memcpy (entry->block, buffer, write_bytes);
    // there might be some data of other block(evicted block)
    // set remain data(write_byte to BLOCK_SECTOR_SIZE) to 0
    //memset (entry->block + write_bytes, 0, BLOCK_SECTOR_SIZE - write_bytes);
    entry -> dirty = true;   // could be write back later

    lock_acquire(&cache_lock);
    entry -> open_cnt--;
    lock_release(&cache_lock);
}




void write_all_cache_back()
{
    struct list_elem* e;
    struct cache_entry* entry;
    lock_acquire (&cache_lock);
    for (e = list_begin (&buffer_cache); e != list_end (&buffer_cache);e = list_next (e))
    {
        entry = list_entry (e, struct cache_entry, elem);
        if (entry->dirty == true){
            block_write (fs_device, entry->sector, entry->block);  
            entry->dirty = false;
        }
    }
    lock_release (&cache_lock);
}

void timer_func(void* aux UNUSED)
{
    while(true){
        write_all_cache_back();
        timer_sleep(WRITE_BACK_INTERVAL);
    }
}


