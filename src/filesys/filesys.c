#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "filesys/buffer_cache.h"
#include "threads/thread.h"
#include "threads/malloc.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  // my code
  // init buffer cache
  buffer_cache_init();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  write_all_cache_back();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool isdir) 
{
  block_sector_t inode_sector = 0;
  // struct dir *dir = dir_open_root ();
  // dir code
  struct dir* dir = get_parent_dir(name);
  char* file_name = get_filename(name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, isdir)
                  && dir_add (dir, file_name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);
  free(file_name);
  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  // struct dir *dir = dir_open_root ();
  if (strcmp(name, "/") == 0) return (struct file*) dir_open_root();
  struct dir* dir = get_parent_dir(name);
  char* file_name = get_filename(name);
  struct inode *inode = NULL;

  if (dir != NULL)
    dir_lookup (dir, file_name, &inode);
  dir_close (dir);

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  // struct dir *dir = dir_open_root ();
  struct dir* dir = get_parent_dir(name);
  char* file_name = get_filename(name);
  
  bool success = dir != NULL && dir_remove (dir, file_name);
  dir_close (dir); 

  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}


// dir related func

/* 
  Like filesys_create because dir is special file which contain list of files' info.
  Create dir given  the dir's name which may be relative or absolute.
  we have to deal four situation of given name:
  relative:  mkdir dir_name;  mkdir ../dir_name;  mkdir ./dir_name;   (do not include '/' at header)
  absolute:  mkdir /home/dir_name          (must include '/' in the head of name)
*/

/*
bool dirsys_create (const char *name)
{
  // some code copied from filesys_create()
  block_sector_t inode_sector = 0;
  struct dir* dir = get_parent_dir(name);
  char* file_name = get_filename(name);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && dir_create (inode_sector, DEFAULT_DIR_SIZE, name)
                  && dir_add (dir, name, inode_sector));
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}
*/


/*
  get the parent dir of given path
  we have to deal four situation of given name:
  relative:  mkdir dir_name;  mkdir ../dir_name;  mkdir ./dir_name;   (do not include '/' at header)
  absolute:  mkdir /home/dir_name          (must include '/' in the head of name)
*/

struct dir *get_parent_dir (const char *path)
{
  struct thread *cur = thread_current ();
  char cp_path[strlen(path) + 1];
  memcpy(cp_path, path, strlen(path) + 1);
  char *save_ptr, *token = NULL, *next_token = NULL;
  struct inode *inode = NULL;
  struct dir *dir;
  
  //printf("cur->cwd: %p\n", cur->cwd);
  //printf("cwd: %p\n", dir_open(dir_get_inode(cur->cwd)));  
 
  token = strtok_r (cp_path, "/", &save_ptr);
  if (strlen(path)==0){
    dir = dir_open_root();
    return dir;
  }
  
  // deal root dir mkdir problem:
  // absolute path: 

  // absolute : mkdir /a/b/dir_name
  // token == NULL
  if (path[0] == '/' || !cur->cwd){
    // open root dir first
    dir = dir_open_root();
    if (strlen(path)==1) return dir;  // only "/"
    //  mkdir /a/b/c, should remove first "/"
    //token = strtok_r(NULL, "/", &save_ptr);
  }else{  // open cwd
    //dir = dir_open (dir_get_inode(cur->cwd));
    dir = dir_reopen (cur->cwd);
  }
  // relative: mkdir ../dir_name
  // return the parent's working dir
  // also some recursive problem: mkdir ../../dir_name

  // relative: mkdir dir_name;  mkdir ./dir_name;
  // return the current working dir
  if (strchr (path, '/') == NULL || strcmp (token, ".") == 0)
  {
    if (strlen (path) > NAME_MAX + 1)
      return NULL;
    if (cur->cwd){
      //dir = dir_open (dir_get_inode(cur->cwd));
      dir = dir_reopen (cur->cwd);
      return dir;
    }
  }


  
  // if path is relative path, token== ".."

  // find the parents recursively
  while ((next_token = strtok_r (NULL, "/", &save_ptr)) != NULL){
  //while(token != NULL ){
    if (strlen(token) > NAME_MAX+1) return NULL;
    // relative path: ../../
    if (strcmp(token, "..") == 0){  // get the cwd's parent
      if (!dir_get_parent(dir, &inode)) return NULL;

    }else{  // absolute path: /a/b/dir_name
      //printf("token: %s\n", token);
      if (!dir_lookup(dir, token, &inode)) return NULL;
    }
    dir_close(dir);
    dir = dir_open(inode);
    //token = strtok_r(NULL, "/", &save_ptr);
    token = next_token;
  }
  return dir; 
}


/*
  get filename of given path
  remove string before '/'

*/
char * get_filename (const char* path)
{
  // copy path because strtok_r could split char*
  char cp_path[strlen(path) + 1];
  memcpy(cp_path, path, strlen(path) + 1);

  char* token = NULL;
  char* save_ptr = NULL;
  char* prev_token = "";
  for (token = strtok_r(cp_path, "/", &save_ptr); token != NULL;
     token = strtok_r (NULL, "/", &save_ptr))
  {
    prev_token = token;
  }
  char *file_name = malloc(strlen(prev_token) + 1);
  // have to be free
  memcpy(file_name, prev_token, strlen(prev_token) + 1);
  return file_name;
}


bool dirsys_chdir (const char* name)
{
 
  struct dir* dir = get_parent_dir(name);
  char* file_name = get_filename(name);
  struct inode *inode = NULL;

  struct thread *t = thread_current ();
  if (!dir_lookup (dir, file_name, &inode))  return false;
  dir_close (dir);
  //printf("chidir: %s\n", file_name);
  dir_close(t->cwd);
  t->cwd = dir_open (inode);
  return true;
   
}
