#include <string.h>

#include "libfat12.h"



int f12_is_directory(struct f12_directory_entry *entry)
{
  if (entry->FileAttributes & F12_ATTR_SUBDIRECTORY)
    return 1;
  return 0;
}

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


int f12_entry_is_empty(struct f12_directory_entry *entry)
{
  if (entry->ShortFileName[0] == 0)
    return 1;
  return 0;
}


int f12_get_file_count(struct f12_directory_entry *entry)
{
  int file_count = 0;

  for (int i = 0; i < entry->child_count; i++) {
    if (f12_is_directory(&entry->children[i]) &&
	!f12_is_dot_dir(&entry->children[i]))
    {
      file_count += f12_get_file_count(&entry->children[i]);
    } else if (!f12_entry_is_empty(&entry->children[i])) {
      file_count++;
    }
  }

  return file_count;
}

int f12_get_directory_count(struct f12_directory_entry *entry)
{
  int dir_count = 0;

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

