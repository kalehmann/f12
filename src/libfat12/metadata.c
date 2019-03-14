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

        for (int i = 0; i < bpb->LogicalSectors; i++) {
                if (f12_meta->fat_entries[i + 2])
                        used_bytes++;
        }

        return used_bytes;
}

void f12_free_metadata(struct f12_metadata *f12_meta)
{
        free(f12_meta->bpb);
        free(f12_meta->fat_entries);
        f12_free_entry(f12_meta->root_dir);
        free(f12_meta->root_dir);
        free(f12_meta);
}
