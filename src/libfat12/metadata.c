#include <stdlib.h>
#include <string.h>

#include "libfat12.h"

enum f12_error f12_create_root_dir_meta(struct f12_metadata *f12_meta)
{
	enum f12_error err;
	struct bios_parameter_block *bpb = f12_meta->bpb;
	size_t fat_size = bpb->SectorsPerFat * bpb->SectorSize;
	uint16_t cluster_count = bpb->LogicalSectors / bpb->SectorsPerCluster;
	struct f12_directory_entry *root_dir = NULL;
	struct f12_directory_entry *root_entries = NULL;

	root_dir = calloc(1, sizeof(struct f12_directory_entry));
	if (NULL == root_dir) {
		return F12_ALLOCATION_ERROR;
	}

	root_entries = calloc(bpb->RootDirEntries, sizeof(struct f12_directory_entry));
	if (NULL == root_entries) {
		free(root_dir);
		
		return F12_ALLOCATION_ERROR;
	}

	root_dir->parent = NULL;
	root_dir->FileAttributes |= F12_ATTR_SUBDIRECTORY;
	root_dir->children = root_entries;
	root_dir->child_count = bpb->RootDirEntries;
	
	f12_meta->root_dir = root_dir;

	for (int i = 0; i < bpb->RootDirEntries; i++) {
		root_entries[i].parent = root_dir;
	}

	f12_meta->entry_count = cluster_count;
	f12_meta->fat_entries = calloc(cluster_count, sizeof(uint16_t));
	if (NULL == f12_meta->fat_entries) {
		free(root_entries);
		free(root_dir);

		return F12_ALLOCATION_ERROR;
	}
	f12_meta->fat_id = (uint16_t)bpb->MediumByte;
	f12_meta->fat_entries[0] = (uint16_t)bpb->MediumByte;
	f12_meta->end_of_chain_marker = 0xfff;
	f12_meta->fat_entries[1] = 0xfff;
	
	return F12_SUCCESS;
}


size_t f12_get_partition_size(struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;

        return bpb->SectorSize * bpb->LogicalSectors;
}

size_t f12_get_used_bytes(struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;
	uint16_t cluster_count = bpb->LogicalSectors / bpb->SectorsPerCluster;

        size_t used_bytes = bpb->SectorSize *
                            (bpb->ReservedForBoot + bpb->NumberOfFats * bpb->SectorsPerFat)
                            + bpb->RootDirEntries * 32;

        for (int i = 2; i < cluster_count; i++) {
                if (f12_meta->fat_entries[i])
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

enum f12_error f12_create_metadata(struct f12_metadata **f12_meta)
{
        *f12_meta = malloc(sizeof(struct f12_metadata));
        if (NULL == *f12_meta) {
                return F12_ALLOCATION_ERROR;
        }

        (*f12_meta)->bpb = malloc(sizeof(struct bios_parameter_block));
        if (NULL == (*f12_meta)->bpb) {
		free(*f12_meta);
		
                return F12_ALLOCATION_ERROR;
        }
	
	return F12_SUCCESS;
}

void f12_init_bpb(struct f12_metadata *f12_meta, size_t size)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;
	bpb->SectorSize = 512;
	
}
