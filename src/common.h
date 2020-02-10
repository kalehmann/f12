#ifndef F12_COMMON_H
#define F12_COMMON_H

#include <stdio.h>
#include <sys/time.h>

#include "f12.h"
#include "libfat12/libfat12.h"

/**
 * Formats the bytes human readable.
 *
 * @param bytes 
 * @return a pointer to the string with the human readable bytes, that must be
 *         freed after use
 */
char *_f12_format_bytes(size_t bytes);

/**
 *
 */
int _f12_walk_dir(FILE * fp, struct f12_put_arguments *args,
		  struct lf12_metadata *f12_meta, suseconds_t created,
		  char **output);

/**
 * Enhanced sprintf. This is essentially the same as asprintf, except if strp
 * already points to an existing string pointer, it is freed.
 * 
 * Check the manual of asprintf for information about the parameters.
 */
int esprintf(char **strp, const char *fmt, ...);

int open_image(FILE * fp, struct lf12_metadata **f12_meta, char **output);

/**
 *
 */
int print_error(FILE * fp, struct lf12_metadata *f12_meta, char **strp,
		char *fmt, ...);

/**
 * Returns the current time in microseconds
 *
 * @return the current time in mircroseconds
 */
suseconds_t time_usec(void);

#endif
