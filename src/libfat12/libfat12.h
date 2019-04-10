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


// directory_entry.c

/**
 * Check whether a f12_directory_entry structure describes a file or a
 * directory.
 *
 * @param entry a pointer to a f12_directory_entry structure
 * @return 1 if the structure describes a directory else 0
 */
int f12_is_directory(struct f12_directory_entry *entry);

/**
 * Check if a f12_directory_entry structure describes a dot dir.
 * The dot directories . and .. are links to the current and the parent
 * directory.
 *
 * @param entry a pointer to the f12_directory_entry structure.
 * @return 1 if the structure describes a dot directory else 0
 */
int f12_is_dot_dir(struct f12_directory_entry *entry);

/**
 * Check whether a f12_directory_entry structure describes a file/directory or
 * is empty.
 *
 * @param entry a pointer to the f12_directory_entry structure
 * @return 1 if the entry describes a directory else 0
 */
int f12_entry_is_empty(struct f12_directory_entry *entry);

/**
 * Get the number of used entries of a directory (not its subdirectories).
 *
 * @param entry a pointer to a f12_directory_entry structure of a directory
 * @return the number of used entries in the directory
 */
int f12_get_child_count(struct f12_directory_entry *entry);

/**
 * Get the number of files in a directory and all its subdirectories.
 *
 * @param entry a pointer to a f12_directory_entry structure of a directory
 * @return the number of files in the directory and all its subdirectories
 */
int f12_get_file_count(struct f12_directory_entry *entry);

/**
 * Get the number of subdirectories (and their subdirectories) in a directory.
 *
 * @param entry a pointer to a f12_directory_entry structure of a directory
 * @return the number of subdirectories and their subdirectories in the
 *         directory.
 */
int f12_get_directory_count(struct f12_directory_entry *entry);

/**
 * Frees a f12_directory_entry structure and all subsequent entries.
 *
 * @param entry a pointer to the f12_directory_entry structure to be freed
 */
void f12_free_entry(struct f12_directory_entry *entry);

/**
 * Remove the entry src from the parent and make it a child of the entry dest.
 * If src points to the "." entry, all siblings of the entry (except "..") will
 * be moved.
 *
 * @param src a pointer to the f12_directory_entry structure that should be
 * moved
 * @param dest a pointer to the f12_directory_entry structure that should be
 * the new parent of src
 * @return F12_SUCCESS or any error that occurred
 */
enum f12_error f12_move_entry(struct f12_directory_entry *src,
                   struct f12_directory_entry *dest);


// error.c
/**
 * Saves the current value of errno for later processing by f12_strerror.
 * This function works only one time. If it is called a second time, it does
 * nothing.
 */
void f12_save_errno(void);

/**
 * Returns a string description of the given error.
 *
 * @param err the error to get a string description for
 * @return the string description for the error, even for an unknown error
 */
char *f12_strerror(enum f12_error err);

// io.c
/**
 * Populates a f12_metadata structure with data from a fat12 image.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to a pointer to the f12_metadata structure to
 * populate.
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_read_metadata(FILE *fp, struct f12_metadata **f12_meta);

/**
 * Writes the data from a f12_metadata structure on a fat12 image.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_write_metadata(FILE *fp, struct f12_metadata *f12_meta);

/**
 * Deletes a file or directory from a fat12 image.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata of the image
 * @param entry a pointer to a f12_directory_entry structure, that describes the
 * file or directory that should be removed; Note that the entry will also be
 * removed from the metadata.
 * @param soft_delete if non zero the entry is not erased, but marked as deleted
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_del_entry(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, int hard_delete);

/**
 * Dump a file from the fat 12 image onto the host file system.
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata of the image
 * @param entry a pointer to the f12_directory_entry structure of the file that
 * should be dumped
 * @param dest_fp the file pointer of the destination file.
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_dump_file(FILE *fp, struct f12_metadata *f12_meta,
                  struct f12_directory_entry *entry, FILE *dest_fp);

/**
 * Write a file to the given path on an image
 *
 * @param fp the file pointer of the image
 * @param f12_meta a pointer to the metadata of the image
 * @param path the path for the newly created file
 * @param source_fp pointer to the file to write on the image
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_create_file(FILE *fp, struct f12_metadata *f12_meta,
                               struct f12_path *path, FILE *source_fp);

/**
 * Populate a new directory entry and add a directory table for it to the
 * metadata of an image.
 *
 * @param f12_meta a pointer to the metadata of an image
 * @param entry a pointer to the directory entry
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_create_directory_table(struct f12_metadata *f12_meta,
                                          struct f12_directory_entry *entry);

/**
 * Create a new directory entry for a given path on an image.
 *
 * @param f12_meta a pointer to the metadata of an image
 * @param path a pointer to the path of the entry to create on the image
 * @param entry a pointer to a pointer to the entry that will be created
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_create_entry_from_path(struct f12_metadata *f12_meta,
                               struct f12_path *path,
                               struct f12_directory_entry **entry);

/**
 * Create a new (empty) fat12 image
 *
 * @param fp a pointer to the file for the image to create
 * @param f12_meta a pointer to the metadata of the image
 * @return F12_SUCCESS or any error that occured
 */
enum f12_error f12_create_image(FILE *fp, struct f12_metadata *f12_meta);

// metadata.c
/**
 * Get the size of a fat12 image in bytes by its metadata
 *
 * @param f12_meta a pointer to the metadata of the image
 * @return the size of the partition in bytes
 */
size_t f12_get_partition_size(struct f12_metadata *f12_meta);

/**
 * Get the number of used bytes of a fat12 image
 *
 * @param f12_meta a pointer to the metadata of the image
 * @return the number of used bytes of the image
 */
size_t f12_get_used_bytes(struct f12_metadata *f12_meta);

/**
 * Creates new metadata 
 *
 * @param f12_meta a pointer to the memory the pointer to the 
 * newly created metadata gets written into
 * @return F12_SUCCESS or any error that occurred
 */
enum f12_error f12_create_metadata(struct f12_metadata **f12_meta);
/**
 * Initializes the empty root directory metadata
 * 
 * @param f12_meta a pointer to the metadata of the partition
 */
enum f12_error f12_create_root_dir_meta(struct f12_metadata *f12_meta);
/**
 * Free the metadata of a fat12 image
 *
 * @param f12_meta a pointer to the metadata of the fat12 image
 */
void f12_free_metadata(struct f12_metadata *f12_meta);
/**
 * Initialize the bios parameter block for an image with the given size.
 * 
 * @param f12_meta a pointer to the metadata of the image
 * @param size the size of the image
 */
void f12_init_bpb(struct f12_metadata *f12_meta, size_t size);

// name.c
/**
 * Generates a human readable filename in the format name.extension from a fat12
 * directory entry.
 *
 * @param entry the directory entry to generate the file name from
 * @return a pointer to the file name that must be freed or NULL on failure
 */
char *f12_get_file_name(struct f12_directory_entry *entry);

/**
 * Converts a human readable filename to the 8.3 format
 *
 * @param name a pointer to the human readable filename
 * @return a pointer to an array of 11 chars with the filename in the 8.3
 * format, that must be freed after use or NULL on failure
 */
char *f12_convert_name(char *name);

// path.c
/**
 * Find a file or subdirectory in a directory on the image from a path.
 *
 * @param entry a pointer to a f12_directory_entry structure, that describes the
 * directory that should be searched for files
 * @param path a pointer to a f12_path structure
 * @return a pointer to a f12_directory_entry structure describing the file
 * or subdirectory or NULL if the path matches no file in the given directory
 */
struct f12_directory_entry *f12_entry_from_path(struct f12_directory_entry *entry,
                                                struct f12_path *path);

/**
 * Creates a f12_path structure from a string with a filepath.
 *
 * @param input a pointer to a string with the filepath to parse
 * @param path a pointer to a pointer to a f12_path structure with the parsed
 * path, that must be freed.
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_parse_path(const char *input, struct f12_path **path);

/**
 * Frees a f12_path structure
 *
 * @param path a pointer to the f12_path structure
 */
void f12_free_path(struct f12_path *path);

/**
 * Determines if one of two given f12_path structure describes a parent
 * directory of the file or directory described by the other path.
 *
 * @param path_a a pointer to a f12_path structure
 * @param path_b a pointer to a f12_path structure
 * @return F12_PATHS_EQUAL if both structures describe the same file path
 *         F12_PATHS_SECOND if the second path describes a parent directory of the
 *         first path
 *         F12_PATHS_FIRST if the first path describes a parent directory of the
 *         second path
 *         F12_PATHS_UNRELATED if they have no common ancestors
 */
enum f12_path_relations f12_path_get_parent(struct f12_path *path_a, struct f12_path *path_b);

/**
 * Create the directory entries for the given path in a directory. Does nothing
 * if the directory structure for the given path already exists.
 *
 * @param f12_meta a pointer to the metadata of an image
 * @param entry a pointer to an entry under which the entries for the path
 * should be generated.
 * @param path the path to generate the entries for
 * @return F12_SUCCESS or any other error that occurred
 */
enum f12_error f12_path_create_directories(struct f12_metadata *f12_meta,
                                struct f12_directory_entry *entry,
                                struct f12_path *path);

#endif
