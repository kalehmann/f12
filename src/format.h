#ifndef F12_FORMAT_H
#define F12_FORMAT_H

#include <stddef.h>
#include "libfat12/libfat12.h"

/**
 * Formats the bytes human readable.
 *
 * @param bytes 
 * @return a pointer to the string with the human readable bytes, that must be
 *         freed after use
 */
char *format_bytes(size_t bytes);

/**
 * Counts the number of digits in the given number. 
 *
 * @param number the number to count the digits in
 * @return the count of digits in the number
 */
size_t digit_count(long number);

/**
 * Returns the length of the result of the format_bytes function for the given
 * bytes.
 *
 * @param bytes
 * @return the length of the string that format_bytes will return for the given
 *         bytes 
 */
size_t format_bytes_len(size_t bytes);

/**
 * Dumps all affected files of a move operation on a fat 12 image in to a
 * string.
 * 
 * @param src a pointer to the lf12_directory_entry structure describing the
 *            source of the movement
 * @param dest a pointer to the lf12_directory_entry structure describing the
 *             destination of the movement
 * @param output a pointer the the string pointer of the output.
 *               The contents of the pointer must be freed,
 * @return any error that occurred or F12_SUCCESS
 */
enum lf12_error dump_move(struct lf12_directory_entry *src,
			  struct lf12_directory_entry *dest, char **output);

#endif
