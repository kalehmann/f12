#ifndef LIBFAT12_H
#define LIBFAT12_H

#include <stdio.h>
#include <inttypes.h>

#define F12_ATTR_SUBDIRECTORY 0x10
#define EMPTY_PATH -2
#define FILE_NOT_FOUND NULL
#define DIRECTORY_NOT_EMPTY -2
#define PATHS_SECOND 1
#define PATHS_EQUAL 0
#define PATHS_FIRST -1
#define PATHS_UNRELATED -2

struct bios_parameter_block {
  char OEMLabel[8];
  // Bytes per logical sector
  uint16_t SectorSize;
  // Logical sectors per cluster
  uint8_t SectorsPerCluster;
  // Reserved logical sectors
  uint16_t ReservedForBoot;
  // Number of FATs
  uint8_t NumberOfFats;
  // Number of root directory entries
  uint16_t RootDirEntries;
  // Total logical sectors
  uint16_t LogicalSectors;
  // Media descriptor
  unsigned char MediumByte;
  // Logial sectors per track
  uint16_t SectorsPerFat;
  // Physical sectors per track
  uint16_t SectorsPerTrack;
  // Number of heads
  uint16_t NumberOfHeads;
  // Hidden sectors
  uint32_t HiddenSectors;
  // Large total logical sectors
  uint32_t LargeSectors;
  // Physical drive number
  uint8_t DriveNumber;
  // Flags etc.
  char Flags;
  // Extended boot signature
  char Signature;
  // Volume serial number
  uint32_t VolumeID;
  // Volume label
  char VolumeLabel[12];
  // File-system type
  char FileSystem[9];
};

struct f12_directory_entry {
  char ShortFileName[8];
  char ShortFileExtension[3];
  char FileAttributes;
  char UserAttributes;
  char CreateTimeOrFirstCharacter;
  uint16_t PasswordHashOrCreateTime;
  uint16_t CreateDate;
  uint16_t OwnerId;
  uint16_t AccessRights;
  uint16_t LastModifiedTime;
  uint16_t LastModifiedDate;
  uint16_t FirstCluster;
  uint32_t FileSize;
  struct f12_directory_entry *children;
  struct f12_directory_entry *parent;
  int child_count;
};

struct f12_metadata {
  uint16_t fat_id;
  uint16_t end_of_chain_marker;
  long root_dir_offset;
  struct bios_parameter_block *bpb;
  struct f12_directory_entry *root_dir;
  uint16_t *fat_entries;
};

struct f12_path {
  char *name;
  char *short_file_name;
  char *short_file_extension;
  struct f12_path *ancestor;
  struct f12_path *descendant;
};

int parse_f12_path(const char *input, struct f12_path **path);
int free_f12_path(struct f12_path *path);
struct f12_directory_entry *f12_entry_from_path(struct f12_directory_entry *entry,
						struct f12_path *path);
int f12_get_parent(struct f12_path *path_a, struct f12_path *path_b);
char *get_file_name(struct f12_directory_entry *entry);
char *convert_name(char *name);
int f12_is_directory(struct f12_directory_entry *entry);
int f12_is_dot_dir(struct f12_directory_entry *entry);
int f12_entry_is_empty(struct f12_directory_entry *entry);
int read_bpb(FILE *fp, struct bios_parameter_block *bpb);
int read_f12_metadata(FILE *fp, struct f12_metadata **f12_meta);
int write_f12_metadata(FILE *fp, struct f12_metadata *f12_meta);
int free_f12_metadata(struct f12_metadata *f12_meta);
size_t f12_get_partition_size(struct f12_metadata *f12_meta);
size_t f12_get_used_bytes(struct f12_metadata *f12_meta);
int f12_get_file_count(struct f12_directory_entry *entry);
int f12_get_directory_count(struct f12_directory_entry *entry);
int f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
		  struct f12_directory_entry *entry, int hard_delete);

#endif
