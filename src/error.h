#ifndef F12_ERROR_H
#define F12_ERROR_H

#include <stdio.h>

#include "libfat12/libfat12.h"

/**
 * Enhanced sprintf. This is essentially the same as asprintf, except if strp
 * already points to an existing string pointer, it is freed.
 * 
 * Check the manual of asprintf for information about the parameters.
 */
int esprintf(char **strp, const char *fmt, ...);

/**
 *
 */
int print_error(FILE * fp, struct lf12_metadata *f12_meta, char **strp,
		char *fmt, ...);

#endif
