#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "libfat12.h"

/**
 * Get the value for a position in the file allocation table.
 *
 * @param fat a pointer to the compressed file allocation table
 * @param n the number of the entry to readout
 * @return the value of the entry
 */
static uint16_t read_fat_entry(char *fat, int n)
{
        uint16_t fat_entry;
        memcpy(&fat_entry, fat + n * 3 / 2, 2);
        if (n % 2) {
                return fat_entry >> 4;
        } else {
                return fat_entry & 0xfff;
        }
}

/**
 * Get the position of a cluster on a fat12 partition.
 *
 * @param cluster the number of the cluster
 * @param f12_meta a pointer to the metadata of the partition
 * @return the position of the cluster on the partition
 */
static int cluster_offset(uint16_t cluster, struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;
        cluster -= 2;
        int root_dir_offset = f12_meta->root_dir_offset;
        int root_size = bpb->RootDirEntries * 32 / bpb->SectorSize;
        int sector_offset =
                cluster * f12_meta->bpb->SectorsPerCluster + root_size;

        return sector_offset * bpb->SectorSize + root_dir_offset;
}

/**
 * Get the number of clusters in a cluster chain.
 *
 * @param start_cluster number of the first cluster on the cluster chain
 * @param f12_meta a pointer to the metadata of the partition
 * @return the number of clusters in the cluster chain
 */
static uint16_t get_cluster_chain_length(uint16_t start_cluster,
                                         struct f12_metadata *f12_meta)
{
        uint16_t current_cluster = start_cluster;
        uint16_t chain_length = 1;
        uint16_t *fat_entries = f12_meta->fat_entries;

        while ((current_cluster = fat_entries[current_cluster])
               != f12_meta->end_of_chain_marker) {
                chain_length++;
        }

        return chain_length;
}

/**
 * Get the cluster size of the image
 *
 * @param f12_meta a pointer to the metadata of the partition
 * @return the size of a cluster on the partition in bytes
 */
static size_t get_cluster_size(struct f12_metadata *f12_meta)
{
        return f12_meta->bpb->SectorSize * f12_meta->bpb->SectorsPerCluster;
}

/**
 * Get the size of a cluster chain in bytes.
 *
 * @param start_cluster the number of the first cluster of the cluster chain
 * @param f12_meta a pointer to the metadata of the fat12 partition
 * @return the size of the cluster chain in bytes
 */
static size_t get_cluster_chain_size(uint16_t start_cluster,
                                     struct f12_metadata *f12_meta)
{
        size_t cluster_size = get_cluster_size(f12_meta);

        return cluster_size * get_cluster_chain_length(start_cluster, f12_meta);
}

/**
 * Load the contents of a cluster chain into memory.
 *
 * @param fp the file descriptor of the fat12 image
 * @param start_cluster the number of the first cluster in the cluster chain
 * @param f12_meta a pointer to the metadata of the partition
 * @return a pointer to the contents of the cluster chain in the memory. Must be
 *         freed.
 */
static char *load_cluster_chain(FILE *fp, uint16_t start_cluster,
                                struct f12_metadata *f12_meta)
{
        uint16_t *fat_entries = f12_meta->fat_entries;
        uint16_t current_cluster = start_cluster;
        uint16_t cluster_size = get_cluster_size(f12_meta);
        size_t data_size = get_cluster_chain_size(start_cluster, f12_meta);
        int offset;
        char *data;

        data = malloc(data_size);

        if (NULL == data) {
                return NULL;
        }

        do {
                offset = cluster_offset(current_cluster, f12_meta);
                if (0 != fseek(fp, offset, SEEK_SET)) {
                        f12_save_errno();

                        return NULL;
                }
                if (cluster_size != fread(data, 1, cluster_size, fp)) {
                        f12_save_errno();

                        return NULL;
                }
                data += cluster_size;
        } while ((current_cluster = fat_entries[current_cluster])
                 != f12_meta->end_of_chain_marker);

        data -= data_size;

        return data;
}

/**
 * Erase the contents of a cluster chain on the partition.
 *
 * @param fp file pointer of the partition
 * @param f12_meta a pointer to the metadata of the partition
 * @param first_cluster the number of the first cluster of the cluster chain
 * @return F12_SUCCESS or any error that occurred
 */
static enum f12_error
erase_cluster_chain(FILE *fp, struct f12_metadata *f12_meta,
                    uint16_t first_cluster)
{
        uint16_t *fat_entries = f12_meta->fat_entries;
        uint16_t current_cluster = first_cluster;
        size_t cluster_size = get_cluster_size(f12_meta);
        int offset;
        char *zeros = malloc(cluster_size);

        if (NULL == zeros) {
                return F12_ALLOCATION_ERROR;
        }

        memset(zeros, 0, cluster_size);

        do {
                offset = cluster_offset(current_cluster, f12_meta);
                if (0 != fseek(fp, offset, SEEK_SET)) {
                        f12_save_errno();
                        free(zeros);

                        return F12_IO_ERROR;
                }
                if (cluster_size != fwrite(zeros, 1, cluster_size, fp)) {
                        f12_save_errno();
                        free(zeros);

                        return F12_IO_ERROR;
                }
        } while ((current_cluster = fat_entries[current_cluster])
                 != f12_meta->end_of_chain_marker);

        free(zeros);

        return F12_SUCCESS;
}

/**
* Erase a f12_directory_entry structure
 *
* @param entry a pointer to the f12_directory_entry structure
*/
static void erase_entry(struct f12_directory_entry *entry)
{
        memset(entry, 0, sizeof(struct f12_directory_entry));
}

/**
 * Populate a f12_directory_entry structure from the raw entry from the disk.
 *
 * @param data a pointer to the raw data
 * @param entry a pointer to the f12_directory_entry structure to populate
 */
static void read_dir_entry(char *data, struct f12_directory_entry *entry)
{
        memcpy(&entry->ShortFileName, data, 8);
        memcpy(&entry->ShortFileExtension, data + 8, 3);
        entry->FileAttributes = *(data + 11);
        entry->UserAttributes = *(data + 12);
        entry->CreateTimeOrFirstCharacter = *(data + 13);
        memcpy(&entry->PasswordHashOrCreateTime, data + 14, 2);
        memcpy(&entry->CreateDate, data + 16, 2);
        memcpy(&entry->OwnerId, data + 18, 2);
        memcpy(&entry->AccessRights, data + 20, 2);
        memcpy(&entry->LastModifiedTime, data + 22, 2);
        memcpy(&entry->LastModifiedDate, data + 24, 2);
        memcpy(&entry->FirstCluster, data + 26, 2);
        memcpy(&entry->FileSize, data + 28, 4);
}

/**
 * Scan for subdirectories and files in a directory on the partition.
 *
 * @param fp file pointer of the fat12 partition
 * @param f12_meta a pointer to the metadata of the partition
 * @param dir_entry a pointer to a f12_directory_entry structure describing the
 * directory, that should be scanned for subdirectories and files
 * @return F12_SUCCESS or any error that occurred
 */
static enum f12_error
scan_subsequent_entries(FILE *fp, struct f12_metadata *f12_meta,
                        struct f12_directory_entry *dir_entry)
{
        enum f12_error err;

        if (f12_entry_is_empty(dir_entry) ||
            !f12_is_directory(dir_entry)) {
                return F12_SUCCESS;
        }

        if (0 == memcmp(dir_entry->ShortFileName, ".       ", 8) &&
            0 == memcmp(dir_entry->ShortFileExtension, "   ", 3)) {
                dir_entry->children = dir_entry->parent->children;
                dir_entry->child_count = dir_entry->parent->child_count;
                return F12_SUCCESS;
        }
        if (0 == memcmp(dir_entry->ShortFileName, "..      ", 8) &&
            0 == memcmp(dir_entry->ShortFileExtension, "   ", 3)) {
                if (dir_entry->parent == f12_meta->root_dir) {
                        return F12_SUCCESS;
                }
                dir_entry->children = dir_entry->parent->parent->children;
                dir_entry->child_count = dir_entry->parent->parent->child_count;
                return F12_SUCCESS;
        }

        size_t directory_size = get_cluster_chain_size(dir_entry->FirstCluster,
                                                       f12_meta);
        int entry_count = directory_size / 32;
        char *directory_table = load_cluster_chain(fp, dir_entry->FirstCluster,
                                                   f12_meta);
        if (NULL == directory_table) {
                return F12_UNKNOWN_ERROR;
        }

        struct f12_directory_entry *entries =
		calloc(entry_count, sizeof(struct f12_directory_entry));
        if (NULL == entries) {
                free(directory_table);
                return F12_ALLOCATION_ERROR;
        }

        dir_entry->child_count = entry_count;
        dir_entry->children = entries;
        for (int i = 0; i < entry_count; i++) {
                read_dir_entry(directory_table + i * 32, &entries[i]);
                entries[i].parent = dir_entry;
                err = scan_subsequent_entries(fp, f12_meta, &entries[i]);
                if (F12_SUCCESS != err) {
                        free(directory_table);
                        return err;
                }
        }
        free(directory_table);

        return F12_SUCCESS;
}

/**
 * Loads the root directory of a fat12 partition.
 *
 * @param fp file pointer of the partition
 * @param f12_meta a pointer to the metadata of the partition
 * @return F12_SUCCESS or any error that occurred
 */
static enum f12_error load_root_dir(FILE *fp, struct f12_metadata *f12_meta)
{
        enum f12_error err;
        struct bios_parameter_block *bpb = f12_meta->bpb;

        int root_start = f12_meta->root_dir_offset;
        size_t root_size = bpb->RootDirEntries * 32;
        char *root_data = malloc(root_size);

        if (NULL == root_data) {
                return F12_ALLOCATION_ERROR;
        }

        if (0 != fseek(fp, root_start, SEEK_SET)) {
                f12_save_errno();
                free(root_data);

                return F12_IO_ERROR;
        }
        if (root_size != fread(root_data, 1, root_size, fp)) {
                f12_save_errno();
                free(root_data);

                return F12_IO_ERROR;
        }

	struct f12_directory_entry *root_entries = f12_meta->root_dir->children;

        for (int i = 0; i < bpb->RootDirEntries; i++) {
                read_dir_entry(root_data + i * 32, &root_entries[i]);

                err = scan_subsequent_entries(fp, f12_meta, &root_entries[i]);
                if (F12_SUCCESS != err) {
                        free(root_data);

                        return err;
                }
        }

        free(root_data);

        return F12_SUCCESS;
}

/**
 * Reads all values out of the cluster table of the partition.
 *
 * @param fp file pointer of the partition
 * @param f12_meta a pointer to the metadata of the partition
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error read_fat_entries(FILE *fp, struct f12_metadata *f12_meta)
{

        struct bios_parameter_block *bpb = f12_meta->bpb;
        int fat_start_addr = bpb->SectorSize * bpb->ReservedForBoot;
        size_t fat_size = bpb->SectorsPerFat * bpb->SectorSize;
        uint16_t cluster_count = fat_size / 3 * 2;

        char *fat = malloc(fat_size);

        if (NULL == fat) {
                return F12_ALLOCATION_ERROR;
        }

        if (0 != fseek(fp, fat_start_addr, SEEK_SET)) {
                f12_save_errno();
                free(fat);

                return F12_IO_ERROR;
        }
        if (fat_size != fread(fat, 1, fat_size, fp)) {
                f12_save_errno();
                free(fat);

                return F12_IO_ERROR;
        }

        for (int i = 0; i < cluster_count; i++) {
                f12_meta->fat_entries[i] = read_fat_entry(fat, i);
        }

        free(fat);
        return F12_SUCCESS;
}

/**
 * Populates a bios_parameter_block structure with data from the image.
 *
 * @param fp file pointer of the image
 * @param bpb a pointer to the bios_parameter_block structure to populate
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error read_bpb(FILE *fp, struct bios_parameter_block *bpb)
{
        char buffer[59];

        if (0 != fseek(fp, 3L, SEEK_SET)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }
        if (59 != fread(buffer, 1, 59, fp)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }

        memcpy(&(bpb->OEMLabel), buffer, 8);
        memcpy(&(bpb->SectorSize), buffer + 8, 2);
        memcpy(&(bpb->SectorsPerCluster), buffer + 10, 1);
        memcpy(&(bpb->ReservedForBoot), buffer + 11, 2);
        memcpy(&(bpb->NumberOfFats), buffer + 13, 1);
        memcpy(&(bpb->RootDirEntries), buffer + 14, 2);
        memcpy(&(bpb->LogicalSectors), buffer + 16, 2);
        memcpy(&(bpb->MediumByte), buffer + 18, 1);
        memcpy(&(bpb->SectorsPerFat), buffer + 19, 2);
        memcpy(&(bpb->SectorsPerTrack), buffer + 21, 2);
        memcpy(&(bpb->NumberOfHeads), buffer + 23, 2);
        memcpy(&(bpb->HiddenSectors), buffer + 25, 4);
        memcpy(&(bpb->LargeSectors), buffer + 29, 4);
        memcpy(&(bpb->DriveNumber), buffer + 33, 1);
        memcpy(&(bpb->Flags), buffer + 34, 1);
        memcpy(&(bpb->Signature), buffer + 35, 1);
        memcpy(&(bpb->VolumeID), buffer + 36, 4);
        memcpy(&(bpb->VolumeLabel), buffer + 40, 11);
        bpb->VolumeLabel[11] = 0;
        memcpy(&(bpb->FileSystem), buffer + 51, 8);
        bpb->FileSystem[8] = 0;

        return F12_SUCCESS;
}

/**
 * Writes data to a cluster chain.
 *
 * @param fp the file pointer of the image
 * @param data a pointer to the data to write
 * @param first_cluster the number of the first cluster of the cluster chain
 * @param bytes the number of bytes to write
 * @param f12_meta a pointer to the metadata of the partition
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error
write_to_cluster_chain(FILE *fp, void *data, uint16_t first_cluster,
                      size_t bytes, struct f12_metadata *f12_meta)
{
        uint16_t chain_length = get_cluster_chain_size(first_cluster, f12_meta);
        uint16_t written_bytes = 0;
        uint16_t current_cluster = first_cluster;
        uint16_t cluster_size = get_cluster_size(f12_meta);
        uint16_t *fat_entries = f12_meta->fat_entries;
        uint16_t bytes_left;
        int offset;

        /* Check if the data is larger than the clusterchain */
        if (bytes > chain_length) {
                return F12_LOGIC_ERROR;
        }

        /* Check if the data is more than on cluster smaller than the clusterchain */
        if (bytes + cluster_size <= chain_length) {
                return F12_LOGIC_ERROR;
        }

        while (written_bytes < bytes) {
                offset = cluster_offset(current_cluster, f12_meta);
                if (0 != fseek(fp, offset, SEEK_SET)) {
                        f12_save_errno();

                        return F12_IO_ERROR;

                }
                bytes_left = bytes - written_bytes;

                if (bytes_left < cluster_size) {
                        if (bytes_left != fwrite(data, 1, bytes_left, fp)) {
                                f12_save_errno();

                                return F12_IO_ERROR;
                        }
                        char zero = 0;
                        for (uint16_t i = bytes_left; i < cluster_size; i++) {
                                if (1 != fwrite(&zero, 1, 1, fp)) {
                                        f12_save_errno();

                                        return F12_IO_ERROR;
                                }
                        }
                } else {
                        if (cluster_size != fwrite(data, 1, cluster_size, fp)) {
                                f12_save_errno();

                                return F12_IO_ERROR;
                        }
                }

                written_bytes += cluster_size;
                data += cluster_size;
                current_cluster = fat_entries[current_cluster];
        }

        return F12_SUCCESS;
}

/**
 * Write the bios_parameter_block structure to the image.
 *
 * @param fp file pointer of the partition
 * @param f12_meta a pointer to the metadata of the image
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error write_bpb(FILE *fp, struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;
        char buffer[59];

        memcpy(buffer, &(bpb->OEMLabel), 8);
        memcpy(buffer + 8, &(bpb->SectorSize), 2);
        memcpy(buffer + 10, &(bpb->SectorsPerCluster), 1);
        memcpy(buffer + 11, &(bpb->ReservedForBoot), 2);
        memcpy(buffer + 13, &(bpb->NumberOfFats), 1);
        memcpy(buffer + 14, &(bpb->RootDirEntries), 2);
        memcpy(buffer + 16, &(bpb->LogicalSectors), 2);
        memcpy(buffer + 18, &(bpb->MediumByte), 1);
        memcpy(buffer + 19, &(bpb->SectorsPerFat), 2);
        memcpy(buffer + 21, &(bpb->SectorsPerTrack), 2);
        memcpy(buffer + 23, &(bpb->NumberOfHeads), 2);
        memcpy(buffer + 25, &(bpb->HiddenSectors), 4);
        memcpy(buffer + 29, &(bpb->LargeSectors), 4);
        memcpy(buffer + 33, &(bpb->DriveNumber), 1);
        memcpy(buffer + 34, &(bpb->Flags), 1);
        memcpy(buffer + 35, &(bpb->Signature), 1);
        memcpy(buffer + 36, &(bpb->VolumeID), 4);
        memcpy(buffer + 40, &(bpb->VolumeLabel), 11);
        memcpy(buffer + 51, &(bpb->FileSystem), 8);

        if (0 != fseek(fp, 3L, SEEK_SET)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }
        if (59 != fwrite(buffer, 1, 59, fp)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }

        return F12_SUCCESS;
}

/**
 * Creates a compressed file allocation table from the metadata of a image.
 *
 * @param f12_meta a pointer to the metadata of the image
 * @return a pointer to the compressed fat that must be freed or NULL on failure
 */
static char *create_fat(struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;
        int fat_size = bpb->SectorsPerFat * bpb->SectorSize;
        int cluster_count = fat_size / 3 * 2;
        uint16_t cluster;

        char *fat = malloc(fat_size);

        if (NULL == fat) {
                return NULL;
        }

        for (int i = 0; i < cluster_count; i++) {
                if (i % 2) {
                        // Odd cluster
                        cluster = (f12_meta->fat_entries[i] << 4) |
                                  (f12_meta->fat_entries[i - 1] >> 8);
                        memmove(fat + i * 3 / 2, &cluster, 2);
                        continue;
                }
                cluster = f12_meta->fat_entries[i];
                memmove(fat + i * 3 / 2, &cluster, 2);
        }

        return fat;
}

/**
 * Compress all the childrens (but not their childrens) of a directory entry,
 * so that it can be written on the partition.
 *
 * @param dir_entry a pointer to the f12_directory_entry structure
 * @param dir_size the size of the directory on the image in bytes
 * @return a pointer to the compressed directory, that must be freed or NULL
 * on failure
 */
static char *create_directory(struct f12_directory_entry *dir_entry,
                              size_t dir_size)
{
        size_t offset;
        char *dir = calloc(1, dir_size);
        struct f12_directory_entry *entry;

        if (NULL == dir) {
                return NULL;
        }

        if (!f12_is_directory(dir_entry)) {
                return NULL;
        }

        for (int i = 0; i < dir_entry->child_count; i++) {
                entry = &dir_entry->children[i];
                offset = i * 32;
                memcpy(dir + offset, &entry->ShortFileName, 8);
                memcpy(dir + offset + 8, &entry->ShortFileExtension, 3);
                memcpy(dir + offset + 11, &entry->FileAttributes, 1);
                memcpy(dir + offset + 12, &entry->UserAttributes, 1);
                memcpy(dir + offset + 13, &entry->CreateTimeOrFirstCharacter,
                       1);
                memcpy(dir + offset + 14, &entry->PasswordHashOrCreateTime, 2);
                memcpy(dir + offset + 16, &entry->CreateDate, 2);
                memcpy(dir + offset + 18, &entry->OwnerId, 2);
                memcpy(dir + offset + 20, &entry->AccessRights, 2);
                memcpy(dir + offset + 22, &entry->LastModifiedTime, 2);
                memcpy(dir + offset + 24, &entry->LastModifiedDate, 2);
                memcpy(dir + offset + 26, &entry->FirstCluster, 2);
                memcpy(dir + offset + 28, &entry->FileSize, 4);
        }

        return dir;
}

/**
 * Writes the file allocation table from the metadata on the partition.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error write_fats(FILE *fp, struct f12_metadata *f12_meta)
{
        struct bios_parameter_block *bpb = f12_meta->bpb;
        int fat_offset = bpb->SectorSize * bpb->ReservedForBoot;
        size_t fat_size = bpb->SectorsPerFat * bpb->SectorSize;
        int fat_count = bpb->NumberOfFats;
        char *fat = create_fat(f12_meta);

        if (NULL == fat) {
                return F12_ALLOCATION_ERROR;
        }

        if (0 != fseek(fp, fat_offset, SEEK_SET)) {
                f12_save_errno();
                free(fat);

                return F12_IO_ERROR;
        }
        for (int i = 0; i < fat_count; i++) {
                if (fat_size != fwrite(fat, 1, fat_size, fp)) {
                        f12_save_errno();
                        free(fat);

                        return F12_IO_ERROR;
                }
        }

        free(fat);
        return F12_SUCCESS;
}

/**
 * Writes the directory described by a f12_directory_entry structure and all
 * its children on the image.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata of the image
 * @param entry a pointer to the f12_directory_entry structure
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error write_directory(FILE *fp, struct f12_metadata *f12_meta,
                                      struct f12_directory_entry *entry)
{
        enum f12_error err;

        if (f12_entry_is_empty(entry)
            || !f12_is_directory(entry)
            || f12_is_dot_dir(entry)
            || entry->FirstCluster == 0
                ) {
                return F12_SUCCESS;
        }

        uint16_t first_cluster = entry->FirstCluster;

        for (int i = 0; i < entry->child_count; i++) {
                err = write_directory(fp, f12_meta, &entry->children[i]);
                if (F12_SUCCESS != err) {
                        return err;
                }
        }

        size_t dir_size = get_cluster_chain_size(entry->FirstCluster, f12_meta);
        char *dir = create_directory(entry, dir_size);
        if (NULL == dir) {
                return F12_ALLOCATION_ERROR;
        }
        err = write_to_cluster_chain(fp, dir, first_cluster, dir_size, f12_meta);
        free(dir);
        if (F12_SUCCESS != err) {
                return err;
        }

        return F12_SUCCESS;
}

/**
 * Writes the root directory from the metadata of a fat12 image on the
 * image.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata of the image
 * @return F12_SUCCESS or any other error that occurred
 */
static enum f12_error write_root_dir(FILE *fp, struct f12_metadata *f12_meta)
{
        enum f12_error err;

        size_t dir_size = 32 * f12_meta->bpb->RootDirEntries;
        char *dir = create_directory(f12_meta->root_dir, dir_size);

        if (NULL == dir) {
                return F12_ALLOCATION_ERROR;
        }

        for (int i = 0; i < f12_meta->root_dir->child_count; i++) {
                err = write_directory(fp, f12_meta,
                                      &f12_meta->root_dir->children[i]);
                if (F12_SUCCESS != err) {
                        return err;
                }
        }

        if (0 != fseek(fp, f12_meta->root_dir_offset, SEEK_SET)) {
                f12_save_errno();
                free(dir);

                return F12_IO_ERROR;
        }

        if (dir_size != fwrite(dir, 1, dir_size, fp)) {
                f12_save_errno();
                free(dir);

                return F12_IO_ERROR;
        }
        free(dir);

        return F12_SUCCESS;
}

/**
 * Create a new cluster chain in the file allocation table of the metadata.
 *
 * @param f12_meta a pointer to the metadata of a fat12 image
 * @param cluster_count the number of clusters to allocate
 * @return the first cluster in the new cluster chain or zero if the file
 * allocation table is full
 */
static uint16_t get_cluster_chain(struct f12_metadata *f12_meta,
                                  int cluster_count)
{
        uint16_t i = 0, j = 2, first_cluster, last_cluster;

        while (j < f12_meta->entry_count) {
                if (f12_meta->fat_entries[j] == 0) {
                        if (i > 0) {
                                f12_meta->fat_entries[last_cluster] = j;
                        } else {
                                first_cluster = j;
                        }
                        last_cluster = j;
                        i++;
                        if (i == cluster_count) {
                                f12_meta->fat_entries[j] = f12_meta->end_of_chain_marker;
                                return first_cluster;
                        }
                }
                j++;
        }

        return 0;
}

enum f12_error f12_read_metadata(FILE *fp, struct f12_metadata **f12_meta)
{
        enum f12_error err;
        struct bios_parameter_block *bpb;

	err = f12_create_metadata(f12_meta);
	if (F12_SUCCESS != err) {
		return err;
	}
	bpb = (*f12_meta)->bpb;
	
        if (F12_SUCCESS != (err = read_bpb(fp, (*f12_meta)->bpb))) {
                free(f12_meta);
                return err;
        }

        (*f12_meta)->root_dir_offset = bpb->SectorSize *
                                       ((bpb->NumberOfFats *
                                         bpb->SectorsPerFat) +
                                        bpb->ReservedForBoot);

	err = f12_create_root_dir_meta(*f12_meta);
	if (F12_SUCCESS != err) {
		return err;
	}
	
        if (F12_SUCCESS != (err = read_fat_entries(fp, *f12_meta))) {
                free((*f12_meta)->bpb);
                free(*f12_meta);

                return err;
        }

        (*f12_meta)->fat_id = (*f12_meta)->fat_entries[0];
        (*f12_meta)->end_of_chain_marker = (*f12_meta)->fat_entries[1];

        if (F12_SUCCESS != (err = load_root_dir(fp, *f12_meta))) {
		free((*f12_meta)->bpb);
		free((*f12_meta)->fat_entries);
		free(*f12_meta);

                return err;
        }

        return F12_SUCCESS;
}

enum f12_error f12_write_metadata(FILE *fp, struct f12_metadata *f12_meta)
{
        enum f12_error err;

        err = write_bpb(fp, f12_meta);
        if (F12_SUCCESS != err) {
                return err;
        }

        err = write_fats(fp, f12_meta);
        if (F12_SUCCESS != err) {
                return err;
        }

        err = write_root_dir(fp, f12_meta);
        if (F12_SUCCESS != err) {
                return err;
        }

        return F12_SUCCESS;
}

enum f12_error f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
                             struct f12_directory_entry *entry, int soft_delete)
{
        enum f12_error err;

        if (f12_is_directory(entry) && f12_get_child_count(entry) > 2) {
                return F12_DIR_NOT_EMPTY;
        }

        if (1 == soft_delete) {
                return F12_SUCCESS;
        }

        if (entry->FirstCluster) {
                err = erase_cluster_chain(fp, f12_meta, entry->FirstCluster);
                if (err != F12_SUCCESS) {
                        return err;
                }
        }
	if (entry->children) {
		free(entry->children);
	}
        erase_entry(entry);
        err = f12_write_metadata(fp, f12_meta);
        if (F12_SUCCESS != err) {
                return err;
        }

        return F12_SUCCESS;
}

enum f12_error f12_dump_file(FILE *fp, struct f12_metadata *f12_meta,
                             struct f12_directory_entry *entry, FILE *dest_fp)
{
        uint32_t file_size = entry->FileSize;
        char *buffer = load_cluster_chain(fp, entry->FirstCluster, f12_meta);

        if (NULL == buffer) {
                return F12_UNKNOWN_ERROR;
        }

        if (file_size != fwrite(buffer, 1, file_size, dest_fp)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }

        free(buffer);

        return F12_SUCCESS;
}

enum f12_error f12_create_file(FILE *fp, struct f12_metadata *f12_meta,
                               struct f12_path *path, FILE *source_fp)
{
        enum f12_error err;

        struct f12_directory_entry *entry;
        size_t cluster_size, file_size;
        int cluster_count;
        uint16_t first_cluster;

        cluster_size = get_cluster_size(f12_meta);
        if (0 != fseek(source_fp, 0L, SEEK_END)) {
                f12_save_errno();

                return F12_IO_ERROR;
        }
        long int ftell_res = ftell(source_fp);
        if (-1L == ftell_res) {
                f12_save_errno();
                fclose(fp);

                return F12_IO_ERROR;
        }
        file_size = (size_t) ftell_res;
        rewind(source_fp);

        cluster_count = file_size / cluster_size;
        if (file_size % cluster_size) {
                cluster_count++;
        }

        first_cluster = get_cluster_chain(f12_meta, cluster_count);

        if (0 == first_cluster) {
                // Image full
                return F12_IMAGE_FULL;
        }

        char *data = malloc(file_size);
        if (NULL == data) {
                return F12_ALLOCATION_ERROR;
        }

        if (file_size != fread(data, 1, file_size, source_fp)) {
                f12_save_errno();
                free(data);

                return F12_IO_ERROR;
        }

        err = write_to_cluster_chain(fp, data, first_cluster, file_size,
                                    f12_meta);
        if (F12_SUCCESS != err) {
                free(data);
                return err;
        }

        err = f12_create_entry_from_path(f12_meta, path, &entry);
        if (err != F12_SUCCESS) {
                free(data);
                return err;
        }
        entry->FirstCluster = first_cluster;
        entry->FileSize = file_size;

        free(data);

        return F12_SUCCESS;
}

enum f12_error f12_create_directory_table(struct f12_metadata *f12_meta,
                                          struct f12_directory_entry *entry)
{
        size_t table_size = 244 * 32;

        struct f12_directory_entry *children =
		calloc(224, sizeof(struct f12_directory_entry));
        if (NULL == children) {
                return F12_ALLOCATION_ERROR;
        }

        uint16_t cluster_count = table_size / get_cluster_size(f12_meta);

        entry->children = children;
        entry->child_count = 224;
        entry->FileAttributes = F12_ATTR_SUBDIRECTORY;
        entry->FirstCluster = get_cluster_chain(f12_meta, cluster_count);
        if (0 == entry->FirstCluster) {
                return F12_IMAGE_FULL;
        }

        // Create first dot dir
        memcpy(&children[0], entry, sizeof(struct f12_directory_entry));
        memcpy(children[0].ShortFileName, ".       ", 8);
        memset(children[0].ShortFileExtension, 32, 3);
        children[0].parent = entry;
        children[0].FirstCluster = 0;

        // Create second dot dir
        memcpy(&children[1], entry->parent, sizeof(struct f12_directory_entry));
        memcpy(children[1].ShortFileName, "..      ", 8);
        memset(children[1].ShortFileExtension, 32, 3);
        children[1].parent = entry;
        children[1].FirstCluster = 0;

        return F12_SUCCESS;
}

enum f12_error f12_create_entry_from_path(struct f12_metadata *f12_meta,
                                          struct f12_path *path,
                                          struct f12_directory_entry **entry)
{
        enum f12_error err;

        struct f12_path *tmp_path = path, *last_element = path;
        struct f12_directory_entry *parent_entry;

        *entry = NULL;

        if (path->descendant == NULL) {
                // Destination is in the root directory
                parent_entry = f12_meta->root_dir;
        } else {
                do {
                        tmp_path = tmp_path->descendant;
                } while (tmp_path->descendant != NULL);
                tmp_path = tmp_path->ancestor;
                last_element = tmp_path->descendant;
                tmp_path->descendant = NULL;
                err = f12_path_create_directories(f12_meta, f12_meta->root_dir,
                                                  path);
                if (F12_SUCCESS != err) {
			tmp_path->descendant = last_element;
			
                        return err;
                }

                parent_entry = f12_entry_from_path(f12_meta->root_dir, path);

                if (NULL == parent_entry) {
			tmp_path->descendant = last_element;
			
                        return F12_FILE_NOT_FOUND;
                }
                tmp_path->descendant = last_element;
        }

        // Check if the file exists
        for (int i = 0; i < parent_entry->child_count; i++) {
                if (0 == memcmp(parent_entry->children[i].ShortFileName,
                                last_element->short_file_name, 8) &&
                    0 == memcmp(parent_entry->children[i].ShortFileExtension,
                                last_element->short_file_extension, 3)) {
                        *entry = &parent_entry->children[i];
                        break;
                }
        }

        if (NULL == *entry) {
                for (int i = 0; i < parent_entry->child_count; i++) {
                        if (parent_entry->children[i].ShortFileName[0] == 0) {
                                *entry = &parent_entry->children[i];
                                break;
                        }
                }
        }

        if (NULL == *entry) {
                return F12_DIR_FULL;
        }

	memcpy((*entry)->ShortFileName, last_element->short_file_name, 8);
        memcpy((*entry)->ShortFileExtension, last_element->short_file_extension, 3);
	
        return F12_SUCCESS;
}

enum f12_error f12_create_image(FILE *fp, struct f12_metadata *f12_meta)
{
	struct bios_parameter_block *bpb = f12_meta->bpb;
	size_t file_size = bpb->LargeSectors * bpb->SectorSize;

	void *sector = calloc(bpb->SectorSize, 1);
	if (NULL == sector) {
		return F12_ALLOCATION_ERROR;
	}

	for (int i = 0; i < bpb->LargeSectors; i++) {
		fwrite(sector, 1, bpb->SectorSize, fp);
	}
	free(sector);

	f12_write_metadata(fp, f12_meta);

	return F12_SUCCESS;
}
