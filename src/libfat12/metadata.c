#include <stdlib.h>
#include <string.h>

#include "libfat12.h"

enum f12_error f12_create_root_dir_meta(struct f12_metadata *f12_meta)
{
	enum f12_error err;
	struct bios_parameter_block *bpb = f12_meta->bpb;

	struct f12_directory_entry *root_dir =
		malloc(sizeof(struct f12_directory_entry));

	if (NULL == root_dir) {
		return F12_ALLOCATION_ERROR;
	}
	
	struct f12_directory_entry *root_entries =
		malloc(sizeof(struct f12_directory_entry) * bpb->RootDirEntries);

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
		memset(&root_entries[i], 0, sizeof(struct f12_directory_entry *));
		root_entries[i].parent = root_dir;
	}
	
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
