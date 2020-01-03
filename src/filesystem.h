#ifndef F12_FILESYSTEM_H
#define F12_FILESYSTEM_H

#include <stdio.h>
#include <sys/time.h>

#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * Dumps a file or directory from a fat 12 image on to the filesystem.
 *
 * @param fp a file pointer to the fat 12 image
 * @param f12_meta a pointer to the metadata of the fat 12 image
 * @param entry a pointer to the lf12_directory_entry structure describing the
 *              the file or directory to dump
 * @param dest_path a pointer to the string with the output path on the
 *                  filesystem
 * @param args a pointer to the structure with the arguments for the dump
 * @param output a pointer to the string containing the output, e.g. all moved
 *               files
 * @return 0 on success or -1 on an error
 */
int _f12_dump_f12_structure(FILE * fp, struct lf12_metadata *f12_meta,
			    struct lf12_directory_entry *entry, char *dest_path,
			    struct f12_get_arguments *args, char **output);

/**
 * Puts a directory from the filesystem on to a fat 12 image.
 *
 * @param fp a file pointer to the fat 12 image
 * @param args a pointer to a structure containing the arguments for the
 *             operation 
 * @param f12_meta a pointer to the metadata of the fat 12 image
 * @param created the microseconds since the epoch since the file creation
 * @param output a pointer to the string pointer of the output, e.g. all moved
 *               files
 * @return 0 on success and -1 on failure. The string in output will be set 
 *         according to error
 */
int _f12_walk_dir(FILE * fp, struct f12_put_arguments *args,
		  struct lf12_metadata *f12_meta, suseconds_t created,
		  char **output);

#endif
