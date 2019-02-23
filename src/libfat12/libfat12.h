#ifndef LIBFAT12_H
#define LIBFAT12_H

#include <stdio.h>
#include <inttypes.h>

/* directory entry attributes */
#define F12_ATTR_SUBDIRECTORY 0x10
/* constants for describing errors */
#define F12_EMPTY_PATH -2
#define F12_FILE_NOT_FOUND NULL
#define F12_DIRECTORY_NOT_EMPTY -3
#define F12_NOT_A_FILE -4
#define F12_NOT_A_DIR -5
#define F12_RELATION_PROBLEM -6
#define F12_DIR_FULL -6
/* constants for comparing paths */
#define F12_PATHS_SECOND 1
#define F12_PATHS_EQUAL 0
#define F12_PATHS_FIRST -1
#define F12_PATHS_UNRELATED -2

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


/* directory_entry.c */
int f12_is_directory(struct f12_directory_entry *entry);

int f12_is_dot_dir(struct f12_directory_entry *entry);

int f12_entry_is_empty(struct f12_directory_entry *entry);

int f12_get_child_count(struct f12_directory_entry *entry);

int f12_get_file_count(struct f12_directory_entry *entry);

int f12_get_directory_count(struct f12_directory_entry *entry);

int f12_free_entry(struct f12_directory_entry *entry);

int f12_move_entry(struct f12_directory_entry *src,
                   struct f12_directory_entry *dest);

/* io.c */
int f12_read_metadata(FILE *fp, struct f12_metadata **f12_meta);

int f12_write_metadata(FILE *fp, struct f12_metadata *f12_meta);

int f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, int hard_delete);

int f12_dump_file(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, FILE *dest_fp);

/* metadata.c */
size_t f12_get_partition_size(struct f12_metadata *f12_meta);

size_t f12_get_used_bytes(struct f12_metadata *f12_meta);

int f12_free_metadata(struct f12_metadata *f12_meta);

/* name.c */
char *f12_get_file_name(struct f12_directory_entry *entry);

char *f12_convert_name(char *name);

/* path.c */
int f12_parse_path(const char *input, struct f12_path **path);

int f12_free_path(struct f12_path *path);

struct f12_directory_entry *f12_entry_from_path(struct f12_directory_entry *entry,
                                                struct f12_path *path);

int f12_path_get_parent(struct f12_path *path_a, struct f12_path *path_b);

#endif
