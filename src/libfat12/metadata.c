#include <stdlib.h>

#include "libfat12.h"

/**
 * Get the size of a fat12 partition in bytes
 *
 * @param f12_meta a pointer to the metadata of the partition
 * @return the size of the partition in bytes
 */
size_t f12_get_partition_size(struct f12_metadata *f12_meta)
{

        struct bios_parameter_block *bpb = f12_meta->bpb;

        return bpb->SectorSize * bpb->LogicalSectors;
}

/**
 * Get the number of used bytes of a fat12 partition
 *
 * @param f12_meta a pointer to the metadata of the partition
 * @return the number of used bytes of the partition
 */
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

/**
 * Free all the metadata of a fat12 partition
 *
 * @param f12_meta a pointer to the metadata of the fat12 partition
 * @return
 */
int f12_free_metadata(struct f12_metadata *f12_meta)
{
        free(f12_meta->bpb);
        free(f12_meta->fat_entries);
        f12_free_entry(f12_meta->root_dir);
        free(f12_meta->root_dir);
        free(f12_meta);

        return 0;
}
