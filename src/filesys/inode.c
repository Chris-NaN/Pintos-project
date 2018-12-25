#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"

#include <stdlib.h>
#include <stdio.h>  // debug

#include "threads/malloc.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"
#include "threads/synch.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44

#define MAX_DIRECT_OFFSET 12
#define MAX_INDIRECT_OFFSET 128
#define MAX_DOUBLY_INDIRECT_OFFSET 128*128

#define INDIRECT_INDEX 12*512
#define DOUBLY_INDIRECT_INDEX (12*512+128*512)

static char zeros[BLOCK_SECTOR_SIZE];
/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    /* my code */
    /* need to change the unused , the origin one is 125   !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!*/
    off_t indirect_offset;
    off_t doubly_offset_1;
    off_t doubly_offset_2;

    block_sector_t direct_blocks[12]; 
    block_sector_t indirect;
    block_sector_t doubly_indirect;

    // dir code
    bool isdir;
    block_sector_t parent;

    unsigned magic;                     /* Magic number. */
    uint32_t unused[107];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */

    off_t length; 

    off_t indirect_offset;
    off_t doubly_offset_1;
    off_t doubly_offset_2;

    block_sector_t direct_blocks[12]; 
    block_sector_t indirect;
    block_sector_t doubly_indirect;

    // dir code
    bool isdir;
    block_sector_t parent;

    struct lock extend_lock;
  
    // struct inode_disk data;             /* Inode content. */
  };

static block_sector_t sector_to_sector(const block_sector_t sector, off_t offset);
int indirect_block_allocate(struct inode * inode, size_t num_sectors);
int doubly_indirect_block_allocate(struct inode* inode, size_t num_sectors);
void inode_destroy(struct inode* inode);
bool inode_extend(struct inode* inode, off_t new_length);



/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->length)
  {
    off_t sector_offset;
    //   return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    if (pos < INDIRECT_INDEX)
    {
      sector_offset = pos / BLOCK_SECTOR_SIZE;
      return inode->direct_blocks[sector_offset];
    }
    else if (pos < DOUBLY_INDIRECT_INDEX)
    {
      sector_offset = (pos - INDIRECT_INDEX)/BLOCK_SECTOR_SIZE;
      block_sector_t pos_level1 = sector_to_sector(inode->indirect, sector_offset);
      return pos_level1;
    }
    else
    {


      sector_offset = (pos - DOUBLY_INDIRECT_INDEX)/BLOCK_SECTOR_SIZE;
      off_t sector_offset1 = sector_offset / MAX_INDIRECT_OFFSET;
      off_t sector_offset2 = sector_offset % MAX_INDIRECT_OFFSET;

      block_sector_t pos_level1 = sector_to_sector(inode->doubly_indirect, sector_offset1);
      
      block_sector_t pos_level2 = sector_to_sector(pos_level1,sector_offset2);
      

      return pos_level2;
    }
  }
  else
    return -1;
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool isdir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */

  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);

  if (disk_inode != NULL)
  {

    disk_inode->length = length;
    disk_inode->magic = INODE_MAGIC;

    // dir code
    disk_inode->isdir = isdir;
    disk_inode->parent = ROOT_DIR_SECTOR;

    struct inode inode;// = malloc(sizeof(inode));
    inode.length = 0;
    inode.indirect_offset = 0;
    inode.doubly_offset_1 = 0;
    inode.doubly_offset_2 = 0;
   
    memset(inode.direct_blocks,0,sizeof(inode.direct_blocks));
    inode.indirect = 0;
    inode.doubly_indirect = 0;
    success = inode_extend(&inode,length);


    // printf(" inode_extend    ture or false? %d\n",success);
    // printf("now the disk_inode->length is [%u]\n",disk_inode->length);
    if (success)
    {
      disk_inode->indirect_offset = inode.indirect_offset;
      disk_inode->doubly_offset_1 = inode.doubly_offset_1;
      disk_inode->doubly_offset_2 = inode.doubly_offset_2;

      memcpy(disk_inode->direct_blocks,inode.direct_blocks,sizeof(inode.direct_blocks));
      disk_inode->indirect = inode.indirect;
      disk_inode->doubly_indirect = inode.doubly_indirect;
   
      block_write(fs_device,sector,disk_inode);
    }
    
  }
  free(disk_inode);
  // printf("\n%s\n","+++++++++++++++++++++++++++++++++++++++++++++");
  // printf("%s\n","+++++++++++++++++++++++++++++++++++++++++++++");
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  // printf("\n+++++++++++ inode open with sector [%u] \n",sector);


  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  lock_init(&inode->extend_lock);

  struct inode_disk* disk_inode = malloc(sizeof(struct inode_disk));
  // block_read (fs_device, inode->sector, &inode->data);
  
  block_read (fs_device, inode->sector, disk_inode);
  
  inode->length = disk_inode->length;

  inode->indirect_offset = disk_inode->indirect_offset;
  inode->doubly_offset_1 = disk_inode->doubly_offset_1;
  inode->doubly_offset_2 = disk_inode->doubly_offset_2;

  /* dir */
  inode->isdir = disk_inode->isdir;
  inode->parent = disk_inode->parent;

  memcpy(inode->direct_blocks,disk_inode->direct_blocks,sizeof(disk_inode->direct_blocks));
  
  inode->indirect = disk_inode->indirect;
  inode->doubly_indirect = disk_inode->doubly_indirect;

  free(disk_inode);

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          inode_destroy(inode);
          // free_map_release (inode->data.start, bytes_to_sectors (inode->data.length)); 
        }
      else
        {
          struct inode_disk* disk_inode = malloc(sizeof(struct inode_disk));
          disk_inode->length = inode->length;
          disk_inode->indirect_offset = inode->indirect_offset;
          disk_inode->doubly_offset_1 = inode->doubly_offset_1;
          disk_inode->doubly_offset_2 = inode->doubly_offset_2;
          memcpy(disk_inode->direct_blocks,inode->direct_blocks,sizeof(disk_inode->direct_blocks));
          disk_inode->indirect = inode->indirect;

          /* dir */
          disk_inode-> isdir = inode->isdir;
          disk_inode-> parent = inode->parent;
          
          disk_inode->doubly_indirect = inode->doubly_indirect;
          block_write(fs_device,inode->sector,disk_inode);
          free(disk_inode);
        }
      free (inode); 
    }

}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  /* my code */
  // struct cache_entry *entry = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* my code, new cache buffer implement */
      // printf("wrong ahead ------------------\n");
      // entry = read_cache_entry (sector_idx);
      // memcpy (buffer + bytes_read, entry->block + sector_ofs, chunk_size);
      //printf("wrong behine ------------------\n");
      

      // origin bounce buffer implement
       
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          //Read full sector directly into caller's buffer.
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
           //Read sector into bounce buffer, then partially copy
          // into caller's buffer.
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);
    // printf("----dadssssssssssssssssssssssss------------------------%d\n",bytes_read);
  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;
  /* my code */
  // struct cache_entry *entry = NULL;

  if (inode->deny_write_cnt)
    return 0;

  // printf("%s\n","8888888888888888888888888888888888888888888888888888888888888888888888888");

  // printf("+++++++++++++++++++++++++++++++++++  inode_write_at with size [%d], offset [%d]\n",size,offset);
  // printf(" the inode sector [%u]\n",inode->sector);
  // printf("+++++ the current inode length [%d]\n\n",inode_length(inode));
  if (offset + size > inode_length(inode))
  {
    // printf("%s\n","need to extend");

    lock_acquire(&inode->extend_lock);
    inode_extend(inode,offset + size);
    lock_release(&inode->extend_lock);

    // printf("%s\n","extend over");
  }


  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* my code, new cache buffer implement */
      
     /// entry = read_cache_entry (sector_idx);
      ///memcpy (entry->block + sector_ofs, buffer + bytes_written, chunk_size);
      //entry->loaded = true;
      ///write_cache_entry (sector_idx, entry->block, chunk_size);
      //write_cache_entry (sector_idx,(void*) buffer, chunk_size);
      

      // origin bounce buffer implement
      
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          // Write full sector directly to disk.
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          // We need a bounce buffer.
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

           //If the sector contains data before or after the chunk
           //  we're writing, then we need to read in the sector
           //  first.  Otherwise we start with a sector of all zeros. 
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
        
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  // printf(" --------------------------------------------------------the success bytes_written [%d] \n\n",bytes_written);
  // printf("%s\n","8888888888888888888888888888888888888888888888888888888888888888888888888");
  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}

/* my code */
static block_sector_t
sector_to_sector(const block_sector_t sector, off_t offset)
{
  block_sector_t buf[128];
  // block_sector_t* buf = calloc (128, sizeof *block_sector_t);
  block_read(fs_device,sector,&buf);
  // block_sector_t ret = buf[sector_idx];
  // free(buf);
  // return ret;
  return buf[offset];
}

int
indirect_block_allocate(struct inode * inode, size_t num_sectors)
{
  int num_allcate = 0;
  if (inode->indirect == 0)
  {
    if (!free_map_allocate(1,&inode->indirect))
      return -1;
    block_write(fs_device, inode->indirect, zeros);
  }

  block_sector_t indirect_blocks[MAX_INDIRECT_OFFSET];
  block_read(fs_device, inode->indirect, indirect_blocks);


  // printf(" inode->indirect_offset : %d num_sectors : %d \n",inode->indirect_offset,num_sectors);

  for (size_t i= (size_t) inode->indirect_offset; i<num_sectors && i< MAX_INDIRECT_OFFSET ;i++)
  {
    if (!free_map_allocate(1,&indirect_blocks[i]))
    {
      return -1;
    }
    block_write(fs_device, indirect_blocks[i], zeros);
    inode->indirect_offset++;
    num_allcate ++;
  }

  block_write(fs_device, inode->indirect, indirect_blocks);
  return num_allcate;
}

int 
doubly_indirect_block_allocate(struct inode * inode, size_t num_sectors)
{
  int num_allcate = 0;
  if (inode->doubly_indirect == 0)
  {
    if (!free_map_allocate(1,&inode->doubly_indirect))
      return -1;
    block_write(fs_device, inode->doubly_indirect, zeros);
  }


  block_sector_t doubly_indirect_blocks[128];

  block_read(fs_device, inode->doubly_indirect, doubly_indirect_blocks);

  off_t sector_off1 = DIV_ROUND_UP(num_sectors,MAX_INDIRECT_OFFSET);
  off_t sector_off2 = num_sectors % MAX_INDIRECT_OFFSET;

  

  // printf(" inode->doubly_offset_1 : %d sector_off1 : %d \n",inode->doubly_offset_1,sector_off1);

  for (size_t i=(size_t) inode->doubly_offset_1;
        i < (size_t)sector_off1 && i<MAX_INDIRECT_OFFSET; i++)
  {
    size_t level2_start;
    size_t level2_end;

    if (doubly_indirect_blocks[i] == 0)
    {
      if (!free_map_allocate(1,&doubly_indirect_blocks[i]))
        return -1;
      block_write(fs_device, doubly_indirect_blocks[i], zeros);
      level2_start = 0;
    }
    else
      level2_start = inode->doubly_offset_2;

    block_sector_t indirect_blocks[128];
    block_read(fs_device, doubly_indirect_blocks[i], indirect_blocks);

    if (i == (size_t)sector_off1)
      level2_end = sector_off2;
    else
      level2_end = MAX_INDIRECT_OFFSET;

    // printf(" level2_start : %d level2_end : %d \n",level2_start,level2_end);
    for (size_t j= level2_start; j< level2_end; j++)
    {
      if(!free_map_allocate(1,&indirect_blocks[j]))
      {
        return -1;
      }
      block_write(fs_device,indirect_blocks[j], zeros);
      num_allcate ++;
    }

    block_write(fs_device,doubly_indirect_blocks[i],indirect_blocks);
    
    if (level2_end == MAX_INDIRECT_OFFSET) 
      inode->doubly_offset_1++;
    
    if ((i+1 ==(size_t)sector_off1)&&(sector_off2 == 0))
      break;
  }

  block_write(fs_device, inode->doubly_indirect , doubly_indirect_blocks);
  inode->doubly_offset_2 = sector_off2;

  return num_allcate;
}

void 
inode_destroy(struct inode* inode)
{
  size_t i;
  size_t num_sectors = bytes_to_sectors (inode->length);
  // struct inode_disk * disk_inode = & inode ->data;

  free_map_release (inode->sector, 1);
  
  for (i = 0;i< num_sectors && i< MAX_DIRECT_OFFSET;i++)
  {
    free_map_release (inode->direct_blocks[i],1);
    num_sectors --;
  }
  if (num_sectors == 0 ) return;

  block_sector_t buf[128];
  block_read(fs_device,inode->indirect,buf);

  for (i = 0;i< (size_t)inode->indirect_offset;i++)
  {
    free_map_release(buf[i],1);
    num_sectors --;
  }
  if (num_sectors == 0 ) return;

  block_read(fs_device,inode->doubly_indirect,buf);

  for (i = 0;i <=(size_t)inode->doubly_offset_1;i++)
  {
    if (buf[i]==0) return;
    block_sector_t buf2[128];
    block_read(fs_device,buf[i],buf2);
    for (size_t j =0;j<MAX_INDIRECT_OFFSET;j++)
    {
      if (buf2[j] == 0) return;
      free_map_release(buf2[j],1);
    }
  }

}

bool
inode_extend(struct inode* inode, off_t new_length)
{
  // printf("\n//////////////////////////////////////////////////////////////////////\n" );

  // printf("++++++ inode extend with newlength [%d]++++++++\n",new_length);


  size_t new_sectors = bytes_to_sectors (new_length);
  size_t cur_sectors = bytes_to_sectors (inode->length);
  size_t extend_sectors = new_sectors - cur_sectors;


  // printf("new_sectors [%u] , cur_sectors [%u] , extend_sectors [%u] \n",new_sectors,cur_sectors,extend_sectors);



  if (extend_sectors == 0){
    inode->length = new_length;
    // printf("%s\n","----1-----");
    return true;
  }

  while (cur_sectors < MAX_DIRECT_OFFSET)
  {
    if(free_map_allocate(1,&inode->direct_blocks[cur_sectors]))
      block_write(fs_device, inode->direct_blocks[cur_sectors], zeros);
    else{
      // printf("%s\n","----2-----");
      return false;
    }
    if (--extend_sectors == 0)
    {
      inode->length = new_length;
      // printf("%s\n","----3-----");
      return true;
    }
    cur_sectors ++;
  }



  int num_allcate = indirect_block_allocate(inode,new_sectors-MAX_DIRECT_OFFSET);
  if (num_allcate == -1){
     // printf("%s\n","----4-----");
    return false;
  }
  extend_sectors -= (size_t) num_allcate;

  if (extend_sectors == 0)
  {
    // printf("%s\n","----5-----");
    inode->length = new_length;
    return true;
  }
  
  num_allcate = doubly_indirect_block_allocate(inode,new_sectors-MAX_DIRECT_OFFSET- MAX_INDIRECT_OFFSET );
  if (num_allcate == -1){
    // printf("%s\n","----6-----");
    return false;
  }

  inode->length = new_length;
  // printf("%s\n","----7-----");
  return true;
}

/* my code */

block_sector_t inode_get_parent (const struct inode *inode)
{
  return inode->parent;
}


// isdir is not init now
bool inode_is_dir (const struct inode *inode)
{
  return inode->isdir;
}

bool inode_add_parent (block_sector_t parent_sector, block_sector_t child_sector)
{
  struct inode* inode = inode_open(child_sector);
  if (!inode)  return false;
  inode->parent = parent_sector;
  inode_close(inode);
  return true;
}

int inode_open_cnt(struct inode* inode)
{
  return inode->open_cnt;
}
