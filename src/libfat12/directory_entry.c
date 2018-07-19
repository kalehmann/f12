#include <string.h>
#include <stdlib.h>

#include "libfat12.h"

/*
 * Function: get_free_entry
 * ------------------------------
 * Get a free entry of a directory.
 *
 * dir: a pointer to the f12_directory_entry structure describing the directory.
 *
 * returns: a pointer to the first free entry or 
 *          NULL if there are no free entries or dir is a file         
 */
static struct f12_directory_entry * get_free_entry(struct f12_directory_entry *dir)
{
  if (!f12_is_directory(dir)) {
    return NULL;
  }
  
  for (int i = 0; i < dir->child_count; i++) {
    if (f12_entry_is_empty(&dir->children[i])) {
      return &dir->children[i];
    }
  }

  return NULL;
}

/*
 * Function: f12_is_directory
 * --------------------------
 * Check whether a f12_directory_entry structure describes a file or a directory
 *
 * entry: a pointer to the f12_directory_entry structure
 *
 * returns: 1 if the structure describes a directory else 0
 */
int f12_is_directory(struct f12_directory_entry *entry)
{
  if (entry->FileAttributes & F12_ATTR_SUBDIRECTORY)
    return 1;
  return 0;
}

/*
 * Function: f12_is_dot_dir
 * ------------------------
 * Check if a f12_directory_entry structure describes a dot dir.
 * The dot directories . and .. are links to the current and the parent directory.
 *
 * entry: a pointer to the f12_directory_entry structure.
 *
 * returns: 1 if the structure describes a dot directory else 0
 */
int f12_is_dot_dir(struct f12_directory_entry *entry)
{
  if (!f12_is_directory(entry)) {
    return 0;
  }
  
  if (0 == memcmp(entry->ShortFileName, ".       ", 8) &&
      0 == memcmp(entry->ShortFileExtension, "   ", 3)) {

    return 1;
  }
  if (0 == memcmp(entry->ShortFileName, "..      ", 8) &&
      0 == memcmp(entry->ShortFileExtension, "   ", 3)) {

    return 1;
  }
  
  return 0;
}

/*
 * Function: f12_entry_is_empty
 * ----------------------------
 * Check whether a f12_directory_entry structure describes a file/directory or is
 * empty.
 *
 * entry: a pointer to the f12_direcotry_entry structure
 */
int f12_entry_is_empty(struct f12_directory_entry *entry)
{
  if (entry->ShortFileName[0] == 0)
    return 1;
  return 0;
}

/*
 * Function: f12_get_child_count
 * -----------------------------
 * Get the number of used entries of a directory (not its subdirectories).
 * 
 * entry: a pointer to the f12_directory_entry structure of the directory
 *
 * returns: the number of used entries of the directory or
 *          F12_NOT_A_DIR if entry is not a directory
 */
int f12_get_child_count(struct f12_directory_entry *entry)
{
  int child_count = 0;

  if (!f12_is_directory(entry)) {
    return F12_NOT_A_DIR;
  }
  
  for (int i = 0; i < entry->child_count; i++) {
    if (!f12_entry_is_empty(&entry->children[i])) {
      child_count++;
    }
  }

  return child_count;
}

/*
 * Function: f12_get_file_count
 * ----------------------------
 * Get the number of files in a directory and all its subdirectories.
 *
 * entry: a pointer to the f12_directory_entry structure of the directory
 *
 * returns: the number of files in the directory and all its subdirectories or
 *          F12_NOT_A_DIR if entry is not a directory
 */
int f12_get_file_count(struct f12_directory_entry *entry)
{
  struct f12_directory_entry *child;
  int file_count = 0;

  if (!f12_is_directory(entry)) {
    return F12_NOT_A_DIR;
  }
  
  for (int i = 0; i < entry->child_count; i++) {
    child = &entry->children[i];
    if (f12_is_directory(child)) {
      if (!f12_is_dot_dir(child)) {
	file_count += f12_get_file_count(child);
      }
    } else if (!f12_entry_is_empty(child)) {
      file_count++;
    }
  }

  return file_count;
}

/*
 * Function: f12_get_directory_count
 * ---------------------------------
 * Get the number of subdirectories (and their subdirectories) in a directory.
 *
 * entry: a pointer to the f12_directory_entry structure of the directory
 *
 * returns: the number of subdirectories and their subdirectories in the
 *          directory or F12_NOT_A_DIR if entry is not a directory
 */
int f12_get_directory_count(struct f12_directory_entry *entry)
{
  int dir_count = 0;

  if (!f12_is_directory(entry)) {
    return F12_NOT_A_DIR;
  }  

  for (int i = 0; i < entry->child_count; i++) {
    if (f12_is_directory(&entry->children[i]) &&
	!f12_is_dot_dir(&entry->children[i]))
    {
      dir_count++;
      dir_count += f12_get_directory_count(&entry->children[i]);
    }
  }

  return dir_count;
}

/*
 * Function: f12_free_entry
 * ------------------------
 * Free a f12_directory_entry structure and all subsequent entries.
 *
 * entry: a pointer to the f12_directory_entry structure to free
 *
 * returns: 0 on success
 */
int f12_free_entry(struct f12_directory_entry *entry)
{
  if (!f12_is_directory(entry) || f12_is_dot_dir(entry)) {
    return 0;
  }

  for (int i=0; i < entry->child_count; i++) {
    f12_free_entry(&entry->children[i]);
  }
  free(entry->children);

  return 0;
}

/*
 * Function: f12_move_entry
 * ------------------------
 * Remove the entry src from the parent and make it a child of the entry dest.
 *
 * src: a pointer to the f12_directory_entry structure that should be moved
 * dest: a pointer to the f12_directory_entry structure that should be the new
 *       parent of src
 *
 * returns: 0 on success
 *          F12_NOT_A_DIR if src or dest are files and not directories
 *          F12_RELATION_PROBLEM if the relationship of src and dest does not
 *                               allow src to  become a child of dest
 *
 */
int f12_move_entry(struct f12_directory_entry *src,
		   struct f12_directory_entry *dest)
{
  
}
