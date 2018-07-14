#include <stdlib.h>

#include "libfat12.h"


size_t f12_get_partition_size(struct f12_metadata *f12_meta)
{

  struct bios_parameter_block *bpb = f12_meta->bpb;

  return bpb->SectorSize * bpb->LogicalSectors;
}

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

static int free_f12_entry(struct f12_directory_entry *entry)
{
  if (!f12_is_directory(entry) || f12_is_dot_dir(entry)) {
    return 0;
  }

  for (int i=0; i < entry->child_count; i++) {
    free_f12_entry(&entry->children[i]);
  }
  free(entry->children);

  return 0;
}

int free_f12_metadata(struct f12_metadata *f12_meta)
{
  free(f12_meta->bpb);
  free(f12_meta->fat_entries);
  free_f12_entry(f12_meta->root_dir);
  free(f12_meta->root_dir);
  free(f12_meta);
}
