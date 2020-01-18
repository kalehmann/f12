#ifndef FAT12_H
#define FAT12_H

#include <inttypes.h>
#include <libintl.h>
#include <locale.h>

#define _(STRING) gettext(STRING)

struct f12_create_arguments {
	char *device_path;
	char *root_dir_path;
	char *volume_label;
	char *boot_file;
	int verbose;
	unsigned int volume_size;
	uint16_t sector_size;
	uint16_t sectors_per_cluster;
	uint16_t reserved_sectors;
	uint8_t number_of_fats;
	uint16_t root_dir_entries;
	uint8_t drive_number;
};

struct f12_del_arguments {
	char *device_path;
	char *path;
	int recursive;
	int soft_delete;
	int verbose;
};

struct f12_get_arguments {
	char *device_path;
	char *path;
	char *dest;
	int recursive;
	int verbose;
};

struct f12_info_arguments {
	char *device_path;
	int dump_bpb;
};

struct f12_list_arguments {
	char *device_path;
	char *path;
	int creation_date;
	int modification_date;
	int access_date;
	int with_size;
	int recursive;
};

struct f12_move_arguments {
	char *device_path;
	char *source;
	char *destination;
	int recursive;
	int verbose;
};

struct f12_put_arguments {
	char *device_path;
	char *source;
	char *destination;
	int recursive;
	int verbose;
};

/**
 * Create a new fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_create(struct f12_create_arguments *, char **output);

/**
 * Deletes a file or directory on a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_del(struct f12_del_arguments *args, char **output);

/**
 * Dump a file or directory from a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_get(struct f12_get_arguments *args, char **output);

/**
 * Get information about a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_info(struct f12_info_arguments *args, char **output);

/**
 * List the contents of a directory on a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_list(struct f12_list_arguments *args, char **output);

/**
 * Move a file on directory on a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_move(struct f12_move_arguments *args, char **output);

/**
 * Put a file or directory onto a fat12 image
 *
 * @param args the arguments for the function
 * @param output the output to show the user.
 * @return EXIT_SUCCESS on success and EXIT_FAILURE on failure
 */
int f12_put(struct f12_put_arguments *args, char **output);

#endif
