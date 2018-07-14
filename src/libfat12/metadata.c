#include <stdlib.h>

#include "libfat12.h"

/*
 * Function: free_entry
 * ------------------------
 * Free a f12_directory_entry structure and all subsequent entries.
 *
 * entry: a pointer to the f12_directory_entry structure to free
 *
 * returns: 0 on success
 */
static int free_entry(struct f12_directory_entry *entry)
{
  if (!f12_is_directory(entry) || f12_is_dot_dir(entry)) {
    return 0;
  }

  for (int i=0; i < entry->child_count; i++) {
    free_entry(&entry->children[i]);
  }
  free(entry->children);

  return 0;
}

/*
 * Function: f12_get_partition_size
 * --------------------------------
 * Get the size of a fat12 partition in bytes
 *
 * f12_meta: a pointer to the metadata of the partition
 *
 * returns: the size of the partition in bytes
 */
size_t f12_get_partition_size(struct f12_metadata *f12_meta)
{

  struct bios_parameter_block *bpb = f12_meta->bpb;

  return bpb->SectorSize * bpb->LogicalSectors;
}

/*
 * Function: f12_get_used_bytes
 * ----------------------------
 * Get the number of used bytes of a fat12 partition
 *
 * f12_meta: a pointer to the metadata of the partition
 *
 * returns: the number of used bytes of the partition
 */
size_t f12_get_used_bytes(struct f12_metadata *f12_meta)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;
  
  size_t used_bytes = bpb->SectorSize *
    (bpb->ReservedForBoot + bpb->NumberOfFats * bpb->SectorsPerFat)
    + bpb->RootDirEntries * 32;

  for (int i = 0 ; i < bpb->LogicalSectors; i++) {
    if (f12_meta->fat_entries[i+2])
      used_bytes++;
  }

  return used_bytes;
}

/*
 * Function: f12_free_metadata
 * ---------------------------
 * Free all the metadata of a fat12 partition
 *
 * f12_meta: a pointer to the metadata of the fat12 partition
 *
 * returns: 0 on success
 */
int f12_free_metadata(struct f12_metadata *f12_meta)
{
  free(f12_meta->bpb);
  free(f12_meta->fat_entries);
  free_entry(f12_meta->root_dir);
  free(f12_meta->root_dir);
  free(f12_meta);

  return 0;
}
