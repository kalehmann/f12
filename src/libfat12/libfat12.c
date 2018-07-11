#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <inttypes.h>

#include "libfat12.h"

/*
 * Function: get_file_name
 * -----------------------
 * Generates a human readable filename in the format name.extension from a fat12
 * directory entry. 
 * 
 * entry: pointer to the f12_directory_entry structure to generate the filename from.
 *
 * returns: a pointer to the human readable filename which must be freed after use.
 */
char *get_file_name(struct f12_directory_entry *entry)
{
  char name[13], *result;
  int length = 0;
  int i = 0;
  
  while (i < 8 &&
	 entry->ShortFileName[i] != 0 &&
	 entry->ShortFileName[i] != ' ') {
    name[length++] = entry->ShortFileName[i++];
  }
  if (entry->ShortFileExtension[0] != ' ') {
    name[length++] = '.';
    i = 0;
    
    while (i<3 &&
	   entry->ShortFileExtension[i] != 0 &&
	   entry->ShortFileExtension[i] != ' ') {
      name[length++] = entry->ShortFileExtension[i++];
    }
  }
  name[length++] = 0;

  result = malloc(length);

  if (NULL == result) {
    return NULL;
  }

  memcpy(result, name, length);

  return result;
} 

/*
 * Function: convert_name
 * ----------------------
 * Converts a human readable filename to the 8.3 format
 *
 * name: a pointer to the human readable filename
 *
 * returns: a pointer to an arraz of 11 chars with the filename in the 8.3 format,
 *          that must be freed after use.
 */
char *convert_name(char *name)
{
  int i=0;
  char *converted_name = malloc(11), c;

  if (NULL == converted_name) {
    return NULL;
  }

  for (int i=0; i<11; i++) {
    converted_name[i] = ' ';
  }

  while ((c = *(name++)) != '\0') {
    if (c == '.' && *name != '\0' && *name != '.') {
      i = 8;
      continue;
    }
    converted_name[i++] = c; 
  }

  return converted_name;
}

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

uint16_t read_fat_entry(char *fat, int n)
{
  uint16_t fat_entry;
  memcpy(&fat_entry, fat + n * 3 / 2, 2);
  if (n % 2) {
    return fat_entry & 0xfff;
  } else {
    return fat_entry >> 4;
  }
}

static int cluster_offset(uint16_t cluster, struct f12_metadata *f12_meta)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;
  cluster -= 2;
  int root_start_sector = f12_meta->root_dir_offset;
  int root_size = bpb->RootDirEntries * 32 / bpb->SectorSize;
  int sector_offset = cluster * f12_meta->bpb->SectorsPerCluster + root_start_sector +
    root_size;

  return sector_offset * bpb->SectorSize;
}

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

static size_t get_cluster_size(struct f12_metadata *f12_meta)
{
  return f12_meta->bpb->SectorSize * f12_meta->bpb->SectorsPerCluster;
}

static size_t get_cluster_chain_size(uint16_t start_cluster,
				     struct f12_metadata *f12_meta)
{
  size_t cluster_size = get_cluster_size(f12_meta);
  
  return cluster_size * get_cluster_chain_length(start_cluster, f12_meta);
}

char *load_cluster_chain(FILE *fp, uint16_t start_cluster,
			 struct f12_metadata *f12_meta)
{
  uint16_t *fat_entries = f12_meta->fat_entries;
  uint16_t current_cluster = start_cluster;
  uint16_t cluster_size = get_cluster_size(f12_meta);
  size_t data_size = get_cluster_chain_size(start_cluster, f12_meta);
  char *data;
  
  data = malloc(data_size);

  if (NULL == data) {
    return NULL;
  }

  do {
    fseek(fp, cluster_offset(current_cluster, f12_meta), SEEK_SET);
    fread(data, 1, cluster_size, fp);
    data += cluster_size;
  } while ((current_cluster = fat_entries[current_cluster])
	   != f12_meta->end_of_chain_marker);

  data -= data_size;

  return data;
}

int f12_erase_cluster_chain(FILE *fp, struct f12_metadata *f12_meta,
			    uint16_t first_cluster)
{
  uint16_t *fat_entries = f12_meta->fat_entries;
  uint16_t current_cluster = first_cluster;
  size_t cluster_size = get_cluster_size(f12_meta);
  char *zeros = malloc(cluster_size);

  if (NULL == zeros) {
    return -1;
  }

  for (int i=0; i<cluster_size; i++) {
    zeros[i] = 0; 
  }

  do {
    fseek(fp, cluster_offset(current_cluster, f12_meta), SEEK_SET);
    fwrite(zeros, 1, cluster_size, fp);
  } while ((current_cluster = fat_entries[current_cluster])
	   != f12_meta->end_of_chain_marker);

  return 0;
}

static void f12_erase_entry(struct f12_directory_entry *entry)
{
  size_t entry_size = sizeof(struct f12_directory_entry);
  char *_entry = (char *)entry;

  for (int i =0; i<entry_size; i++) {
    _entry[i] = 0;
  }
}

int read_dir_entry(char *data, struct f12_directory_entry *entry)
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

  return 0;
}

int scan_subsequent_entries(FILE *fp, struct f12_metadata *f12_meta,
			      struct f12_directory_entry *dir_entry)
{
  if (f12_entry_is_empty(dir_entry) ||
      !f12_is_directory(dir_entry)) {
    return 0;
  }
  
  if (0 == memcmp(dir_entry->ShortFileName, ".       ", 8) &&
      0 == memcmp(dir_entry->ShortFileExtension, "   ", 3)) {
    dir_entry->children = dir_entry->parent->children;
    dir_entry->child_count = dir_entry->parent->child_count;
    return 0;
  }
  if (0 == memcmp(dir_entry->ShortFileName, "..      ", 8) &&
      0 == memcmp(dir_entry->ShortFileExtension, "   ", 3)) {
    if (dir_entry->parent == f12_meta->root_dir ) {
      return 0;
    }
    dir_entry->children = dir_entry->parent->parent->children;
    dir_entry->child_count = dir_entry->parent->parent->child_count;
    return 0;
  }
  
  size_t directory_size = get_cluster_chain_size(dir_entry->FirstCluster, f12_meta);
  int entry_count = directory_size / 32;
  char *directory_table = load_cluster_chain(fp, dir_entry->FirstCluster, f12_meta);
  struct f12_directory_entry *entries = malloc(entry_count *
					       sizeof(struct f12_directory_entry));
  
  if (NULL == directory_table) {
    return -1;
  }

  if (NULL == entries) {
    free(directory_table);
    return -1;
  }

  if (f12_is_directory(dir_entry)) {
    dir_entry->child_count = entry_count;
    dir_entry->children = entries;
    for (int i = 0; i < entry_count; i++) {
      read_dir_entry(directory_table + i * 32, &entries[i]);
      entries[i].parent = dir_entry;
      scan_subsequent_entries(fp, f12_meta, &entries[i]);
    }
  }
  
  free(directory_table);

  return 0;
}

int load_root_dir(FILE *fp, struct f12_metadata *f12_meta)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;

  int root_start = f12_meta->root_dir_offset;
  size_t root_size = bpb->RootDirEntries * 32;
  char *root_data = malloc(root_size);

  if (NULL == root_data) {
    return -1;
  }

  struct f12_directory_entry *root_dir = malloc(sizeof(struct f12_directory_entry));
  
  if (NULL == root_dir) {
    free(root_data);
    return -1;
  }

  fseek(fp, root_start, SEEK_SET);
  fread(root_data, 1, root_size, fp);

  struct f12_directory_entry *root_entries =
    malloc(sizeof(struct f12_directory_entry) * bpb->RootDirEntries);

  if (NULL == root_entries) {
    free(root_data);
    free(root_dir);
    return -1;
  }

  root_dir->parent = NULL;
  root_dir->FileAttributes |= F12_ATTR_SUBDIRECTORY;
  root_dir->children = root_entries;
  root_dir->child_count = f12_meta->bpb->RootDirEntries;
  
  f12_meta->root_dir = root_dir;

  for (int i = 0; i < f12_meta->bpb->RootDirEntries; i++) {
    read_dir_entry(root_data + i * 32, &root_entries[i]);
    root_entries[i].parent = root_dir;
    
    scan_subsequent_entries(fp, f12_meta, &root_entries[i]);
  }
  
  free(root_data);
}

int read_fat_entries(FILE *fp, struct f12_metadata *f12_meta)
{

  struct bios_parameter_block *bpb = f12_meta->bpb;
  int fat_start_addr = bpb->SectorSize * bpb->ReservedForBoot;
  size_t fat_size = bpb->SectorsPerFat * bpb->SectorSize;
  int cluster_count = fat_size / 3 * 2;
  
  char *fat = malloc(fat_size);

  if (NULL == fat) {
    return -1;
  }

  fseek(fp, fat_start_addr, SEEK_SET);
  fread(fat, 1, fat_size, fp);

  f12_meta->fat_entries = malloc(sizeof(uint16_t) * cluster_count);
  
  if (NULL == f12_meta->fat_entries) {
    free(fat);
    return -1;
  }

  for (int i = 0; i < cluster_count; i++) {
    f12_meta->fat_entries[i] = read_fat_entry(fat, i);
  }

  free(fat);
  return 0;
}

int read_bpb(FILE *fp, struct bios_parameter_block *bpb)
{
  fseek (fp, 3, SEEK_SET);

  fread (&(bpb->OEMLabel), 1, 8, fp);
  fread (&(bpb->SectorSize), 1, 2, fp);
  fread (&(bpb->SectorsPerCluster), 1, 1, fp);
  fread (&(bpb->ReservedForBoot), 1, 2, fp);
  fread (&(bpb->NumberOfFats), 1, 1, fp);
  fread (&(bpb->RootDirEntries), 1, 2, fp);
  fread (&(bpb->LogicalSectors), 1, 2, fp);
  fread (&(bpb->MediumByte), 1, 1, fp);
  fread (&(bpb->SectorsPerFat), 1, 2, fp);
  fread (&(bpb->SectorsPerTrack), 1, 2, fp);
  fread (&(bpb->NumberOfHeads), 1, 2, fp);
  fread (&(bpb->HiddenSectors), 1, 4, fp);
  fread (&(bpb->LargeSectors), 1, 4, fp);
  fread (&(bpb->DriveNumber), 1, 1, fp);
  fread (&(bpb->Flags), 1, 1, fp);
  fread (&(bpb->Signature), 1, 1, fp);
  fread (&(bpb->VolumeID), 1, 4, fp);
  fread (&(bpb->VolumeLabel), 1, 11, fp);
  bpb->VolumeLabel[11] = 0;
  fread (&(bpb->FileSystem), 1, 8, fp);
  bpb->FileSystem[8] = 0;

  return 0;
}

int read_f12_metadata(FILE *fp, struct f12_metadata **f12_meta)
{
  struct bios_parameter_block *bpb;
  
  *f12_meta = malloc(sizeof(struct f12_metadata));

  if (NULL == *f12_meta) {
    return -1;
  }

  (*f12_meta)->bpb = bpb = malloc(sizeof(struct bios_parameter_block));

  if (NULL == (*f12_meta)->bpb) {
    return -1;
  }

  if (0 != read_bpb(fp, (*f12_meta)->bpb)) {
    return -2;
  }

  (*f12_meta)->root_dir_offset = bpb->SectorSize *
    ((bpb->NumberOfFats * bpb->SectorsPerFat) + bpb->ReservedForBoot);
  
  read_fat_entries(fp, *f12_meta);
  (*f12_meta)->fat_id = (*f12_meta)->fat_entries[0];
  (*f12_meta)->end_of_chain_marker = (*f12_meta)->fat_entries[1];

  if (0 != load_root_dir(fp, *f12_meta)) {

    return -1;
  }

  return 0;
}

int write_to_clusterchain(FILE *fp, void *data, uint16_t first_cluster, size_t bytes,
			  struct f12_metadata *f12_meta)
{
  uint16_t chain_length = get_cluster_chain_length(first_cluster, f12_meta);
  uint16_t written_bytes = 0;
  uint16_t current_cluster = first_cluster;
  uint16_t cluster_size = get_cluster_size(f12_meta);
  uint16_t *fat_entries = f12_meta->fat_entries;
  uint16_t bytes_left;

  /* Check if the data is larger than the clusterchain */
  if (bytes > chain_length) {
    return -1;
  }

  /* Check if the data is more than on cluster smaller than the clusterchain */
  if (bytes <= chain_length - cluster_size) {
    return -2;
  }
  
  while (written_bytes < bytes) {
    fseek(fp, cluster_offset(current_cluster, f12_meta), SEEK_SET);
    bytes_left = bytes - written_bytes;
	  
    if (bytes_left < cluster_size) {
      fwrite(data, 1, bytes_left, fp);
      char zero = 0;
      for (int i=0; i<bytes_left; i++) {
	fwrite(&zero, 1, 1, fp);
      }
    } else {
      fwrite(data, 1, cluster_size, fp);
    }

    written_bytes += cluster_size;
    data += cluster_size;
    current_cluster = fat_entries[current_cluster];
  }

  return 0;
}

int write_f12_bpb(FILE *fp, struct f12_metadata *f12_meta)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;
  
  fseek (fp, 3, SEEK_SET);

  fwrite (&(bpb->OEMLabel), 1, 8, fp);
  fwrite (&(bpb->SectorSize), 1, 2, fp);
  fwrite (&(bpb->SectorsPerCluster), 1, 1, fp);
  fwrite (&(bpb->ReservedForBoot), 1, 2, fp);
  fwrite (&(bpb->NumberOfFats), 1, 1, fp);
  fwrite (&(bpb->RootDirEntries), 1, 2, fp);
  fwrite (&(bpb->LogicalSectors), 1, 2, fp);
  fwrite (&(bpb->MediumByte), 1, 1, fp);
  fwrite (&(bpb->SectorsPerFat), 1, 2, fp);
  fwrite (&(bpb->SectorsPerTrack), 1, 2, fp);
  fwrite (&(bpb->NumberOfHeads), 1, 2, fp);
  fwrite (&(bpb->HiddenSectors), 1, 4, fp);
  fwrite (&(bpb->LargeSectors), 1, 4, fp);
  fwrite (&(bpb->DriveNumber), 1, 1, fp);
  fwrite (&(bpb->Flags), 1, 1, fp);
  fwrite (&(bpb->Signature), 1, 1, fp);
  fwrite (&(bpb->VolumeID), 1, 4, fp);
  fwrite (&(bpb->VolumeLabel), 1, 11, fp);
  fwrite (&(bpb->FileSystem), 1, 8, fp);
  
  return 0;
}

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

  for (int i=0; i<cluster_count; i++) {
    if (i % 2) {
      // Odd cluster
      cluster = (f12_meta->fat_entries[i] >> 4) | (f12_meta->fat_entries[i-1] << 12);
      memmove(fat + i * 3 / 2, &cluster, 2);
      continue;
    }
    cluster = f12_meta->fat_entries[i];
    memmove(fat + i * 3 / 2, &cluster, 2);
  }

  return fat;
}

static char *create_directory(struct f12_directory_entry *dir_entry,
			      struct f12_metadata *f12_meta, size_t dir_size)
{
  size_t offset;
  char *dir = malloc(dir_size);
  struct f12_directory_entry *entry;

  if (NULL == dir) {
    return NULL;
  }

  if (!f12_is_directory(dir_entry)) {
    return NULL;
  }
  
  for (int i=0; i<dir_entry->child_count; i++) {
    entry = &dir_entry->children[i];
    offset = i * 32;
    memcpy(dir + offset, &entry->ShortFileName,  8);
    memcpy(dir + offset + 8, &entry->ShortFileExtension, 3);
    memcpy(dir + offset + 11, &entry->FileAttributes, 1);
    memcpy(dir + offset + 12, &entry->UserAttributes, 1);
    memcpy(dir + offset + 13, &entry->CreateTimeOrFirstCharacter, 1);
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

int write_f12_fats(FILE *fp, struct f12_metadata *f12_meta)
{
  struct bios_parameter_block *bpb = f12_meta->bpb;
  int fat_offset = bpb->SectorSize * bpb->ReservedForBoot;
  int fat_size = bpb->SectorsPerFat * bpb->SectorSize;
  int fat_count = bpb->NumberOfFats;
  char *fat = create_fat(f12_meta);

  if (NULL == fat) {
    return -1;
  }
  
  fseek(fp, fat_offset, SEEK_SET);
  for (int i=0; i<fat_count; i++) {
    fwrite(fat, 1, fat_size, fp);
  }
  
  free(fat);
  return 0;
}

int write_f12_directory(FILE *fp, struct f12_metadata *f12_meta,
			struct f12_directory_entry *entry)
{
  uint16_t first_cluster = entry->FirstCluster;
  size_t dir_size = get_cluster_chain_length(entry->FirstCluster, f12_meta);
  char *dir = create_directory(entry, f12_meta, dir_size);
  
  if (NULL == dir) {
    return -1;
  }

  for (int i=0; i<entry->child_count; i++) {
    write_f12_directory(fp, f12_meta, &entry->children[i]);
  }

  write_to_clusterchain(fp, dir, first_cluster, dir_size, f12_meta);
  
  free(dir);
  return 0;
}

int write_f12_root_dir(FILE *fp, struct f12_metadata *f12_meta)
{
  size_t dir_size = 32 * f12_meta->bpb->RootDirEntries;
  char *dir = create_directory(f12_meta->root_dir, f12_meta, dir_size);

  for (int i=0; i<f12_meta->root_dir->child_count; i++) {
    write_f12_directory(fp, f12_meta, &f12_meta->root_dir->children[i]);
  }
  
  fseek(fp, f12_meta->root_dir_offset, SEEK_SET);
  
  fwrite(dir, 1, dir_size, fp);
}

int write_f12_metadata(FILE *fp, struct f12_metadata *f12_meta)
{
  write_f12_bpb(fp, f12_meta);
  write_f12_fats(fp, f12_meta);
  write_f12_root_dir(fp, f12_meta);
  
  return 0;
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

int f12_get_file_count(struct f12_directory_entry *entry)
{
  int file_count = 0;

  for (int i = 0; i < entry->child_count; i++) {
    if (f12_is_directory(&entry->children[i]) && !f12_is_dot_dir(&entry->children[i])) {
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
    if (f12_is_directory(&entry->children[i]) && !f12_is_dot_dir(&entry->children[i])) {
      dir_count++;
      dir_count += f12_get_directory_count(&entry->children[i]);
    }
  }

  return dir_count;
}

int f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
		  struct f12_directory_entry *entry, int hard_delete)
{
  struct f12_directory_entry *parent = entry->parent;
  if (f12_is_directory(entry) && entry->child_count > 2) {
    return DIRECTORY_NOT_EMPTY;
  }

  if (1 == hard_delete) {
    f12_erase_cluster_chain(fp, f12_meta, entry->FirstCluster);
    f12_erase_entry(entry);
    write_f12_metadata(fp, f12_meta);

    return 0;
  }

  return 0;
}
