#ifndef LIBFAT12_H
#define LIBFAT12_H

#include <stdio.h>
#include <inttypes.h>

enum f12_error {
        F12_SUCCESS = 0,
        F12_NOT_A_DIR,
        F12_DIR_FULL,
        F12_ALLOCATION_ERROR,
        F12_IO_ERROR,
        F12_LOGIC_ERROR,
        F12_IMAGE_FULL,
        F12_FILE_NOT_FOUND,
        F12_EMPTY_PATH,
        F12_DIR_NOT_EMPTY,
        F12_UNKNOWN_ERROR,
};

enum f12_path_relations {
        F12_PATHS_EQUAL,
        F12_PATHS_UNRELATED,
        F12_PATHS_FIRST,
        F12_PATHS_SECOND,
};

/* directory entry attributes */
#define F12_ATTR_SUBDIRECTORY 0x10

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
        uint16_t entry_count;
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

void f12_free_entry(struct f12_directory_entry *entry);

enum f12_error f12_move_entry(struct f12_directory_entry *src,
                   struct f12_directory_entry *dest);


/* error.c */
void f12_save_errno(void);

char *f12_strerror(enum f12_error err);

/* io.c */
enum f12_error f12_read_metadata(FILE *fp, struct f12_metadata **f12_meta);

enum f12_error f12_write_metadata(FILE *fp, struct f12_metadata *f12_meta);

enum f12_error f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, int hard_delete);

enum f12_error f12_dump_file(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, FILE *dest_fp);

enum f12_error f12_create_entry_from_path(struct f12_metadata *f12_meta,
                               struct f12_path *path,
                               struct f12_directory_entry **entry);

enum f12_error f12_create_file(FILE *fp, struct f12_metadata *f12_meta,
                    struct f12_path *path, FILE *source_fp);

enum f12_error f12_create_directory_table(struct f12_metadata *f12_meta,
                               struct f12_directory_entry *entry);

/* metadata.c */
size_t f12_get_partition_size(struct f12_metadata *f12_meta);

size_t f12_get_used_bytes(struct f12_metadata *f12_meta);

void f12_free_metadata(struct f12_metadata *f12_meta);

/* name.c */
char *f12_get_file_name(struct f12_directory_entry *entry);

char *f12_convert_name(char *name);

/* path.c */
enum f12_error f12_parse_path(const char *input, struct f12_path **path);

void f12_free_path(struct f12_path *path);

struct f12_directory_entry *f12_entry_from_path(struct f12_directory_entry *entry,
                    struct f12_path *path);

enum f12_path_relations f12_path_get_parent(struct f12_path *path_a, struct f12_path *path_b);

enum f12_error f12_path_create_directories(struct f12_metadata *f12_meta,
                                struct f12_directory_entry *entry,
                                struct f12_path *path);

#endif
